/* store_serial.c
 * Everything pertaining to encoded saved games goes here.
 * Live access to the global store lives in store.c
 */
 
#include "game/bellacopia.h"

/* Decode Base64 digit.
 */
 
static int store_decode_base64_digit(char src) {
  if ((src>='A')&&(src<='Z')) return src-'A';
  if ((src>='a')&&(src<='z')) return src-'a'+26;
  if ((src>='0')&&(src<='9')) return src-'0'+52;
  if (src=='+') return 62;
  if (src=='/') return 63;
  return -1;
}

static const char store_base64_alphabet[64]=
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789+/"
;

/* Decode two Base64 digits as a 12-bit integer, or three as 18-bit.
 */
 
static int store_decode_12bit(const char *src,int srcc) {
  if (srcc<2) return -1;
  int hi=store_decode_base64_digit(src[0]);
  int lo=store_decode_base64_digit(src[1]);
  if ((hi<0)||(lo<0)) return -1;
  return (hi<<6)|lo;
}
 
static int store_decode_18bit(const char *src,int srcc) {
  if (srcc<3) return -1;
  int a=store_decode_base64_digit(src[0]);
  int b=store_decode_base64_digit(src[1]);
  int c=store_decode_base64_digit(src[2]);
  if ((a<0)||(b<0)||(c<0)) return -1;
  return (a<<12)|(b<<6)|c;
}
 
static int store_decode_30bit(const char *src,int srcc) {
  if (srcc<5) return -1;
  int a=store_decode_base64_digit(src[0]);
  int b=store_decode_base64_digit(src[1]);
  int c=store_decode_base64_digit(src[2]);
  int d=store_decode_base64_digit(src[3]);
  int e=store_decode_base64_digit(src[4]);
  if ((a<0)||(b<0)||(c<0)||(d<0)||(e<0)) return -1;
  return (a<<24)|(b<<18)|(c<<12)|(d<<6)|e;
}

static int store_encode_12bit(char *dst,int dsta,int src) {
  if (dsta>=2) {
    dst[0]=store_base64_alphabet[(src>>6)&0x3f];
    dst[1]=store_base64_alphabet[src&0x3f];
  }
  return 2;
}

static int store_encode_18bit(char *dst,int dsta,int src) {
  if (dsta>=3) {
    dst[0]=store_base64_alphabet[(src>>12)&0x3f];
    dst[1]=store_base64_alphabet[(src>>6)&0x3f];
    dst[2]=store_base64_alphabet[src&0x3f];
  }
  return 3;
}

static int store_encode_30bit(char *dst,int dsta,int src) {
  if (dsta>=5) {
    dst[0]=store_base64_alphabet[(src>>24)&0x3f];
    dst[1]=store_base64_alphabet[(src>>18)&0x3f];
    dst[2]=store_base64_alphabet[(src>>12)&0x3f];
    dst[3]=store_base64_alphabet[(src>>6)&0x3f];
    dst[4]=store_base64_alphabet[src&0x3f];
  }
  return 5;
}

/* Plain old base64 decode. Except it must end on a complete unit, ie a multiple of 4.
 */
 
static int store_decode_base64(uint8_t *dst,int dsta,const char *src,int srcc) {
  if (srcc&3) return -1;
  int dstc=0,srcp=0;
  int fullc=srcc>>2;
  while (fullc-->0) {
    int a=store_decode_base64_digit(src[srcp]);
    int b=store_decode_base64_digit(src[srcp+1]);
    int c=store_decode_base64_digit(src[srcp+2]);
    int d=store_decode_base64_digit(src[srcp+3]);
    srcp+=4;
    if ((a<0)||(b<0)||(c<0)||(d<0)) return -1;
    if (dstc>dsta-3) {
      dstc+=3;
    } else {
      dst[dstc++]=(a<<2)|(b>>4);
      dst[dstc++]=(b<<4)|(c>>2);
      dst[dstc++]=(c<<6)|d;
    }
  }
  return dstc;
}

static int store_encode_base64(char *dst,int dsta,const uint8_t *src,int srcc) {
  if (srcc%3) return -1;
  int dstc=0;
  for (;srcc>=3;src+=3,srcc-=3) {
    if (dstc>dsta-4) {
      dstc+=4;
    } else {
      dst[dstc++]=store_base64_alphabet[(src[0]>>2)&0x3f];
      dst[dstc++]=store_base64_alphabet[((src[0]<<4)|(src[1]>>4))&0x3f];
      dst[dstc++]=store_base64_alphabet[((src[1]<<2)|(src[2]>>6))&0x3f];
      dst[dstc++]=store_base64_alphabet[(src[2])&0x3f];
    }
  }
  return dstc;
}

/* Compute 30-bit checksum against a stream of base64.
 * Not that it actually needs to be base64 or anything, any data is fine.
 */
 
static int store_checksum(const char *src,int srcc) {
  uint32_t sum=0;
  for (;srcc-->0;src++) {
    sum=(sum>>31)|(sum<<1);
    sum^=*(uint8_t*)src;
  }
  return sum&0x3fffffff;
}

/* Private helper to unpack serial.
 * We don't validate content of the heaps, except confirming that they are base64 and a sensible length.
 */
 
struct store_toc {
  const char *fldv,*fld16v,*clockv,*jigstorev,*invstorev;
  int efldc,efld16c,eclockc,ejigstorec,einvstorec; // Length of encoded heaps in bytes.
  int lfldc,lfld16c,lclockc,ljigstorec,linvstorec; // Logical length, ie count of fields.
  int checksum_expect; // What's encoded in the serial for validation.
  int checksum_actual; // We compute during decode but we don't fail on mismatches.
};

static int store_toc_decode(struct store_toc *toc,const char *src,int srcc) {

  /* Serial must be at least 15 bytes: 10 for the lengths table and 5 for the checksum.
   * Every byte of it must be base64 legal. And no '='.
   */
  if (!src||(srcc<15)) return -1;
  const char *ck=src;
  int i=srcc;
  for (;i-->0;ck++) {
    if ((*ck>='A')&&(*ck<='Z')) continue;
    if ((*ck>='a')&&(*ck<='z')) continue;
    if ((*ck>='0')&&(*ck<='9')) continue;
    if (*ck=='+') continue;
    if (*ck=='/') continue;
    return -1;
  }
  
  /* The first 10 encoded bytes are lengths of the subsequent heaps.
   * Note that (fldc) is the encoded length and the others are the logical length.
   */
  int srcp=0;
  if ((toc->efldc=store_decode_12bit(src+srcp,srcc-srcp))<0) return -1; srcp+=2;
  if ((toc->lfld16c=store_decode_12bit(src+srcp,srcc-srcp))<0) return -1; srcp+=2;
  if ((toc->lclockc=store_decode_12bit(src+srcp,srcc-srcp))<0) return -1; srcp+=2;
  if ((toc->ljigstorec=store_decode_12bit(src+srcp,srcc-srcp))<0) return -1; srcp+=2;
  if ((toc->linvstorec=store_decode_12bit(src+srcp,srcc-srcp))<0) return -1; srcp+=2;
  
  /* Followed by the 5 heaps.
   */
  if (srcp>srcc-toc->efldc) return -1;
  toc->fldv=src+srcp;
  toc->lfldc=toc->efldc*6;
  srcp+=toc->efldc;
  
  toc->efld16c=toc->lfld16c*3;
  if (srcp>srcc-toc->efld16c) return -1;
  toc->fld16v=src+srcp;
  srcp+=toc->efld16c;
  
  toc->eclockc=toc->lclockc*5;
  if (srcp>srcc-toc->eclockc) return -1;
  toc->clockv=src+srcp;
  srcp+=toc->eclockc;
  
  toc->ejigstorec=toc->ljigstorec*5;
  if (srcp>srcc-toc->ejigstorec) return -1;
  toc->jigstorev=src+srcp;
  srcp+=toc->ejigstorec;
  
  toc->einvstorec=toc->linvstorec*4;
  if (srcp>srcc-toc->einvstorec) return -1;
  toc->invstorev=src+srcp;
  srcp+=toc->einvstorec;
  
  /* Finally the checksum, and assert that we hit the end exactly.
   * Don't fail if checksums mismatch; that's the caller's concern.
   */
  if (srcp!=srcc-5) return -1;
  toc->checksum_expect=store_decode_30bit(src+srcp,srcc-srcp);
  toc->checksum_actual=store_checksum(src,srcp);
  
  return 0;
}

/* Stateless serial validation.
 */
 
int store_validate_serial(const char *src,int srcc) {

  /* Confirm the TOC decodes and checksums match.
   */
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct store_toc toc={0};
  if (store_toc_decode(&toc,src,srcc)<0) return -1;
  if (toc.checksum_expect!=toc.checksum_actual) return -1;
  
  /* There's not much to easily validate beyond that.
   * Could check things like jigstore must name a real map, invstore a real item?
   * That would get expensive.
   * And anyway, random data would surely fail the length and base64 checks,
   * and adulterated data would probably fail the checksum.
   * Not sure we need any more than that.
   */
   
  return 0;
}

/* Decode individual heaps of a store, from TOC.
 */
 
static int store_decode_fldv(struct store *store,const struct store_toc *toc) {
  int fldc_decoded=(toc->lfldc+7)>>3;
  if (store_require_fldv(store,fldc_decoded)<0) return -1;
  store->fldc=0;
  uint8_t *dst=0;
  uint8_t dstmask=0x01;
  const char *src=toc->fldv;
  int i=toc->efldc;
  for (;i-->0;src++) {
    int digit=store_decode_base64_digit(*src);
    if (digit<0) return -1;
    uint8_t srcmask=0x01;
    for (;srcmask<0x40;srcmask<<=1) {
      if (!dst) {
        if (store->fldc>=store->flda) return -1;
        dst=store->fldv+store->fldc++;
        dstmask=0x01;
        *dst=0;
      }
      if (digit&srcmask) (*dst)|=dstmask;
      if (dstmask==0x80) dst=0; else dstmask<<=1;
    }
  }
  return 0;
}
 
static int store_decode_fld16v(struct store *store,const struct store_toc *toc) {
  if (store_require_fld16v(store,toc->lfld16c)<0) return -1;
  uint16_t *dst16=store->fld16v;
  const char *src=toc->fld16v;
  int i=toc->lfld16c;
  for (;i-->0;dst16++,src+=3) {
    int v=store_decode_18bit(src,3);
    if ((v<0)||(v&~0xffff)) return -1;
    *dst16=v;
  }
  store->fld16c=toc->lfld16c;
  return 0;
}
 
static int store_decode_clockv(struct store *store,const struct store_toc *toc) {
  if (store_require_clockv(store,toc->lclockc)<0) return -1;
  double *dstd=store->clockv;
  const char *src=toc->clockv;
  int i=toc->lclockc;
  for (;i-->0;dstd++,src+=5) {
    int v=store_decode_30bit(src,5);
    if (v<0) return -1;
    *dstd=(double)v/1000.0;
  }
  store->clockc=toc->lclockc;
  return 0;
}
 
static int store_decode_jigstorev(struct store *store,const struct store_toc *toc) {
  if (store_require_jigstorev(store,toc->ljigstorec)<0) return -1;
  struct jigstore *jigstore=store->jigstorev;
  const char *src=toc->jigstorev;
  int i=toc->ljigstorec;
  for (;i-->0;jigstore++,src+=5) {
    int v=store_decode_30bit(src,5);
    if (v<0) return -1;
    jigstore->mapid=v>>19;
    jigstore->x=v>>11;
    jigstore->y=v>>3;
    jigstore->xform=v&7;
  }
  store->jigstorec=toc->ljigstorec;
  return 0;
}
 
static int store_decode_invstorev(struct store *store,const struct store_toc *toc) {
  if (toc->linvstorec>INVSTORE_SIZE) return -1;
  if (store_decode_base64((uint8_t*)store->invstorev,sizeof(store->invstorev),toc->invstorev,toc->einvstorec)!=toc->linvstorec*3) return -1;
  memset(store->invstorev+toc->linvstorec,0,sizeof(struct invstore)*(INVSTORE_SIZE-toc->linvstorec));
  return 0;
}

/* Decode serial to a store, no globals.
 */
 
int store_decode(struct store *store,const char *src,int srcc) {

  /* Start by decoding TOC and validating checksum.
   */
  if (!store||!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct store_toc toc={0};
  if (store_toc_decode(&toc,src,srcc)<0) return -1;
  if (toc.checksum_actual!=toc.checksum_expect) return -1;

  /* Decode the five bespoke heaps, from TOC.
   * Splitting these out just for cleanliness's sake.
   */
  if (store_decode_fldv(store,&toc)<0) return -1;
  if (store_decode_fld16v(store,&toc)<0) return -1;
  if (store_decode_clockv(store,&toc)<0) return -1;
  if (store_decode_jigstorev(store,&toc)<0) return -1;
  if (store_decode_invstorev(store,&toc)<0) return -1;
  
  return 0;
}

/* Encode, no globals.
 */
 
int store_encode(char *dst,int dsta,const struct store *store) {
  int dstc=0,err;
  
  int fldc_encoded=((store->fldc<<3)+5)/6;
  dstc+=store_encode_12bit(dst+dstc,dsta-dstc,fldc_encoded);
  
  int fld16c=store->fld16c;
  while (fld16c&&!store->fld16v[fld16c-1]) fld16c--;
  dstc+=store_encode_12bit(dst+dstc,dsta-dstc,fld16c);
  
  int clockc=store->clockc;
  dstc+=store_encode_12bit(dst+dstc,dsta-dstc,clockc);
  
  int jigstorec=store->jigstorec;
  dstc+=store_encode_12bit(dst+dstc,dsta-dstc,jigstorec);
  
  int invstorec=INVSTORE_SIZE;
  while (invstorec&&!store->invstorev[invstorec-1].itemid) invstorec--;
  dstc+=store_encode_12bit(dst+dstc,dsta-dstc,invstorec);
  
  // fldv
  const uint8_t *fldsrc=store->fldv;
  int fldsrcc=store->fldc;
  uint8_t fldsrcmask=0x01;
  int i=fldc_encoded;
  for (;i-->0;) {
    int v=0,j=6,vmask=0x01;
    for (;j-->0;vmask<<=1) {
      if (fldsrcc<=0) break;
      if ((*fldsrc)&fldsrcmask) v|=vmask;
      if (fldsrcmask==0x80) {
        fldsrc++;
        fldsrcc--;
        fldsrcmask=0x01;
      } else {
        fldsrcmask<<=1;
      }
    }
    if (dstc<dsta) dst[dstc]=store_base64_alphabet[v];
    dstc++;
  }
  
  // fld16v
  const uint16_t *src16=store->fld16v;
  for (i=fld16c;i-->0;src16++) {
    dstc+=store_encode_18bit(dst+dstc,dsta-dstc,*src16);
  }
  
  // clockv
  const double *srcd=store->clockv;
  for (i=clockc;i-->0;srcd++) {
    int v=(int)((*srcd)*1000.0);
    if (v&~0x3fffffff) v=0x3fffffff; // Going over in a single session would take 37 days. If it happens, they get what they deserve.
    dstc+=store_encode_30bit(dst+dstc,dsta-dstc,v);
  }
  
  // jigstorev
  const struct jigstore *jigstore=store->jigstorev;
  for (i=jigstorec;i-->0;jigstore++) {
    int v=(jigstore->mapid<<19)|(jigstore->x<<11)|(jigstore->y<<3)|jigstore->xform;
    dstc+=store_encode_30bit(dst+dstc,dsta-dstc,v);
  }
  
  // invstorev
  dstc+=store_encode_base64(dst+dstc,dsta-dstc,(uint8_t*)store->invstorev,invstorec*3);
  
  // checksum
  int sum=0;
  if (dstc<=dsta) sum=store_checksum(dst,dstc);
  dstc+=store_encode_30bit(dst+dstc,dsta-dstc,sum);
  
  return dstc;
}

/* store_serial.c
 * Everything pertaining to encoded saved games goes here.
 * Live access to the global store lives in store.c
 */
 
#include "game/bellacopia.h"

/* Base64 primitives.
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

/* Decode composite Base64 integers.
 * Return value or <0. Caller has to know the encoded length.
 */
 
static int store_decode_12bit(const char *src,int srcc) { // 2
  if (srcc<2) return -1;
  int hi=store_decode_base64_digit(src[0]);
  int lo=store_decode_base64_digit(src[1]);
  if ((hi<0)||(lo<0)) return -1;
  return (hi<<6)|lo;
}
 
static int store_decode_18bit(const char *src,int srcc) { // 3
  if (srcc<3) return -1;
  int a=store_decode_base64_digit(src[0]);
  int b=store_decode_base64_digit(src[1]);
  int c=store_decode_base64_digit(src[2]);
  if ((a<0)||(b<0)||(c<0)) return -1;
  return (a<<12)|(b<<6)|c;
}
 
static int store_decode_24bit(const char *src,int srcc) { // 4
  if (srcc<4) return -1;
  int a=store_decode_base64_digit(src[0]);
  int b=store_decode_base64_digit(src[1]);
  int c=store_decode_base64_digit(src[2]);
  int d=store_decode_base64_digit(src[3]);
  if ((a<0)||(b<0)||(c<0)||(d<0)) return -1;
  return (a<<18)|(b<<12)|(c<<6)|d;
}
 
static int store_decode_30bit(const char *src,int srcc) { // 5
  if (srcc<5) return -1;
  int a=store_decode_base64_digit(src[0]);
  int b=store_decode_base64_digit(src[1]);
  int c=store_decode_base64_digit(src[2]);
  int d=store_decode_base64_digit(src[3]);
  int e=store_decode_base64_digit(src[4]);
  if ((a<0)||(b<0)||(c<0)||(d<0)||(e<0)) return -1;
  return (a<<24)|(b<<18)|(c<<12)|(d<<6)|e;
}

/* Encode composite Base64 integers.
 * Returns encoded length, tho you should know them anyway.
 */
 
static int store_encode_12bit(char *dst,int dsta,int v) {
  if (dsta>=2) {
    dst[0]=store_base64_alphabet[(v>>6)&0x3f];
    dst[1]=store_base64_alphabet[v&0x3f];
  }
  return 2;
}
 
static int store_encode_18bit(char *dst,int dsta,int v) {
  if (dsta>=3) {
    dst[0]=store_base64_alphabet[(v>>12)&0x3f];
    dst[1]=store_base64_alphabet[(v>>6)&0x3f];
    dst[2]=store_base64_alphabet[v&0x3f];
  }
  return 3;
}
 
static int store_encode_24bit(char *dst,int dsta,int v) {
  if (dsta>=4) {
    dst[0]=store_base64_alphabet[(v>>18)&0x3f];
    dst[1]=store_base64_alphabet[(v>>12)&0x3f];
    dst[2]=store_base64_alphabet[(v>>6)&0x3f];
    dst[3]=store_base64_alphabet[v&0x3f];
  }
  return 4;
}
 
static int store_encode_30bit(char *dst,int dsta,int v) {
  if (dsta>=5) {
    dst[0]=store_base64_alphabet[(v>>24)&0x3f];
    dst[1]=store_base64_alphabet[(v>>18)&0x3f];
    dst[2]=store_base64_alphabet[(v>>12)&0x3f];
    dst[3]=store_base64_alphabet[(v>>6)&0x3f];
    dst[4]=store_base64_alphabet[v&0x3f];
  }
  return 5;
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

/* Lil helpers for encoding.
 */
 
#define APPEND(_v) { \
  int __v=(_v); \
  if ((__v<0)||(__v>63)) return -1; \
  if (dstc<dsta) dst[dstc]=store_base64_alphabet[__v]; \
  dstc++; \
}

/* Decode fldv.
 */
 
static int store_decode_fldv_run(struct store *store,int fldid,int v,int c) {

  // Grow buffer and count, and zero when we extend the count.
  int reqc=(fldid+c+7)>>3;
  if (reqc>store->flda) {
    int na=(reqc+64)&~63;
    void *nv=realloc(store->fldv,na);
    if (!nv) return -1;
    store->fldv=nv;
    store->flda=na;
  }
  if (reqc>store->fldc) {
    memset(store->fldv+store->fldc,0,reqc-store->fldc);
    store->fldc=reqc;
  }
  
  // No need to touch zeroes, they're already zero.
  if (!v) return 0;
  
  // Touch every bit individually. This could be more efficient but meh.
  for (;c-->0;fldid++) {
    store->fldv[fldid>>3]|=1<<(fldid&7);
  }
  
  return 0;
}
 
static int store_decode_fldv(struct store *store,const char *src,int srcc) {
  int fldid=0,srcp=0,v=0;
  for (;srcp<srcc;srcp++) {
    int intake=store_decode_base64_digit(src[srcp]);
    if (intake<0) return -1;
    if (store_decode_fldv_run(store,fldid,v,intake&7)<0) return -1;
    fldid+=intake&7;
    if ((intake&7)!=7) v^=1;
    if (store_decode_fldv_run(store,fldid,v,intake>>3)<0) return -1;
    fldid+=intake>>3;
    if ((intake>>3)!=7) v^=1;
  }
  return 0;
}

/* Encode fldv.
 */
 
static int store_encode_fldv(char *dst,int dsta,const struct store *store) {
  int dstc=0;
  int dstbuf=0,dstshift=0;
  int srcp=0,v=0,runlen=1;
  uint8_t srcmask=0x02; // Start at the second bit and assume the first is zero -- the format requires it.
  for (;;) {
    // Measure the next run of bits.
    while (srcp<store->fldc) {
      int bit=(store->fldv[srcp]&srcmask)?1:0;
      if (bit!=v) break;
      runlen++;
      if (srcmask==0x80) {
        srcp++;
        srcmask=0x01;
      } else {
        srcmask<<=1;
      }
    }
    if (!runlen) break;
    v^=1;
    // Emit as many 7s as we need, then whatever's left (even if zero).
    int sevenc=runlen/7;
    runlen%=7;
    while (sevenc-->0) {
      dstbuf|=dstshift?0x38:0x07;
      if (dstshift) {
        APPEND(dstbuf)
        dstbuf=0;
        dstshift=0;
      } else {
        dstshift=3;
      }
    }
    dstbuf|=runlen<<dstshift;
    if (dstshift) {
      APPEND(dstbuf)
      dstbuf=0;
      dstshift=0;
    } else {
      dstshift=3;
    }
    runlen=0;
  }
  if (dstbuf) APPEND(dstbuf);
  return dstc;
}

/* Decode fld16v.
 */
 
static int store_decode_fld16v(struct store *store,const char *src,int srcc) {
  store->fld16c=0;
  int srcp=0;
  while (srcp<srcc) {
    int v=store_decode_base64_digit(src[srcp++]);
    if (v<0) return -1;
    if (v&0x20) {
      v=(v&0x1f)<<5;
      if (srcp>=srcc) return -1;
      int next=store_decode_base64_digit(src[srcp++]);
      if (next<0) return -1;
      v|=next&0x1f;
      if (next&0x20) {
        v<<=6;
        if (srcp>=srcc) return -1;
        next=store_decode_base64_digit(src[srcp++]);
        if (next<0) return -1;
        v|=next;
      }
    }
    if (store->fld16c>=store->fld16a) {
      int na=store->fld16a+32;
      if (na>INT_MAX/sizeof(uint16_t)) return -1;
      void *nv=realloc(store->fld16v,sizeof(uint16_t)*na);
      if (!nv) return -1;
      store->fld16v=nv;
      store->fld16a=na;
    }
    store->fld16v[store->fld16c++]=v;
  }
  return 0;
}

/* Encode fld16v.
 */
 
static int store_encode_fld16v(char *dst,int dsta,const struct store *store) {
  int dstc=0;
  const uint16_t *src=store->fld16v;
  int i=store->fld16c;
  for (;i-->0;src++) {
    int v=*src;
    if (v<0x0020) {
      APPEND(v)
    } else if (v<0x0400) {
      APPEND(0x20|(v>>5))
      APPEND(v&0x1f)
    } else {
      APPEND(0x20|(v>>11))
      APPEND(0x20|((v>>6)&0x1f))
      APPEND(v&0x3f)
    }
  }
  return dstc;
}

/* Decode clockv.
 */
 
static int store_decode_clockv(struct store *store,const char *src,int srcc) {
  store->clockc=0;
  int srcp=0;
  while (srcp<srcc) {
    int v=store_decode_30bit(src+srcp,srcc-srcp);
    if (v<0) return -1;
    srcp+=5;
    if (store->clockc>=store->clocka) {
      int na=store->clocka+32;
      void *nv=realloc(store->clockv,sizeof(double)*na);
      if (!nv) return -1;
      store->clockv=nv;
      store->clocka=na;
    }
    store->clockv[store->clockc++]=(double)v/1000.0;
  }
  return 0;
}

/* Encode clockv.
 */
 
static int store_encode_clockv(char *dst,int dsta,const struct store *store) {
  int dstc=0;
  const double *src=store->clockv;
  int i=store->clockc;
  for (;i-->0;src++) {
    int v=(int)((*src)*1000.0);
    if (v&~0x3fffffff) v=0x3fffffff;
    dstc+=store_encode_30bit(dst+dstc,dsta-dstc,v);
  }
  return dstc;
}

/* Decode jigstorev.
 */
 
static int store_add_jigstorev(struct store *store,int mapid,int x,int y,int xform) {
  if ((mapid<1)||(mapid>0x0fff)) return -1;
  if ((x<0)||(x>0xff)) return -1;
  if ((y<0)||(y>0xff)) return -1;
  if (store->jigstorec>=store->jigstorea) {
    int na=store->jigstorea+128;
    if (na>INT_MAX/sizeof(struct jigstore)) return -1;
    void *nv=realloc(store->jigstorev,sizeof(struct jigstore)*na);
    if (!nv) return -1;
    store->jigstorev=nv;
    store->jigstorea=na;
  }
  struct jigstore *j=store->jigstorev+store->jigstorec++;
  j->mapid=mapid;
  j->x=x;
  j->y=y;
  j->xform=xform;
  return 0;
}
 
static int store_decode_jigstorev(struct store *store,const char *src,int srcc) {
  store->jigstorec=0;
  int srcp=0;
  while (srcp<srcc) {
  
    // Principal record.
    int v=store_decode_30bit(src+srcp,srcc-srcp);
    if (v<0) return -1;
    srcp+=5;
    int mapid=v>>19;
    int x=(v>>11)&0xff;
    int y=(v>>3)&0xff;
    int xform=v&7;
    if (store_add_jigstorev(store,mapid,x,y,xform)<0) return -1;
    
    // Peek for a secondary record.
    v=store_decode_30bit(src+srcp,srcc-srcp);
    if (v&0xffffc000) continue; // <0 or something in the high 16 set. No secondary record.
    srcp+=5;
    int sequentialc=(v>>7)&0x7f;
    int namedc=v&0x7f;
    if (!sequentialc&&!namedc) continue; // Why did they bother writing it?
    
    // Find the reference map.
    const struct map *map=map_by_id(mapid);
    if (!map) return -1;
    
    // Add sequentially-ID'd maps.
    int i=sequentialc;
    int nmapid=mapid+1;
    for (;i-->0;nmapid++) {
      const struct map *nmap=map_by_id(nmapid);
      if (!nmap) return -1;
      if (map->z!=nmap->z) return -1; // Must be on the same plane.
      int dlng=nmap->lng-map->lng;
      int dlat=nmap->lat-map->lat;
      int nx,ny;
      switch (xform) {
        case                             0: nx=x+dlng*NS_sys_mapw; ny=y+dlat*NS_sys_maph; break;
        case EGG_XFORM_SWAP|EGG_XFORM_YREV: nx=x-dlat*NS_sys_maph; ny=y+dlng*NS_sys_mapw; break;
        case EGG_XFORM_XREV|EGG_XFORM_YREV: nx=x-dlng*NS_sys_mapw; ny=y-dlat*NS_sys_maph; break;
        case EGG_XFORM_SWAP|EGG_XFORM_XREV: nx=x+dlat*NS_sys_maph; ny=y-dlng*NS_sys_mapw; break;
        default: return -1;
      }
      if (store_add_jigstorev(store,nmapid,nx,ny,xform)<0) return -1;
    }
    
    // Read and add named maps. We're not currently producing these, but that could change at any time. No changes will be needed here.
    for (i=namedc;i-->0;) {
      int nmapid=store_decode_12bit(src+srcp,srcc-srcp);
      if (nmapid<0) return -1;
      const struct map *nmap=map_by_id(nmapid);
      if (!nmap) return -1;
      if (map->z!=nmap->z) return -1; // Must be on the same plane.
      int dlng=nmap->lng-map->lng;
      int dlat=nmap->lat-map->lat;
      int nx,ny;
      switch (xform) {
        case                             0: nx=x+dlng*NS_sys_mapw; ny=y+dlat*NS_sys_maph; break;
        case EGG_XFORM_SWAP|EGG_XFORM_YREV: nx=x-dlat*NS_sys_maph; ny=y+dlng*NS_sys_mapw; break;
        case EGG_XFORM_XREV|EGG_XFORM_YREV: nx=x-dlng*NS_sys_mapw; ny=y-dlat*NS_sys_maph; break;
        case EGG_XFORM_SWAP|EGG_XFORM_XREV: nx=x+dlat*NS_sys_maph; ny=y-dlng*NS_sys_mapw; break;
        default: return -1;
      }
      if (store_add_jigstorev(store,nmapid,nx,ny,xform)<0) return -1;
    }
  }
  return 0;
}

/* Encode jigstorev.
 */
 
static const struct jigstore *jigstore_find_mapid(const struct jigstore **v,int c,int mapid) {
  for (;c-->0;v++) if ((*v)->mapid==mapid) return *v;
  return 0;
}
 
static int store_encode_jigstorev(char *dst,int dsta,const struct store *store) {
  uint8_t visitv[128]={0}; // Little-endian bits corresponding to index in (store->jigstorev).
  if (store->jigstorec>=sizeof(visitv)*8) return -1; // Update me if we exceed 1024 jiggable maps, which we won't.
  int dstc=0;
  int i=0;
  for (;i<store->jigstorec;i++) {
    if (visitv[i>>3]&(1<<(i&7))) continue; // Already got it from some earlier repeat command.
    visitv[i>>3]|=1<<(i&7); // Probably not necessary, but mark this one visited.
    
    // Emit the 30-bit principal command.
    const struct jigstore *j=store->jigstorev+i;
    int v=(j->mapid<<19)|(j->x<<11)|(j->y<<3)|j->xform;
    dstc+=store_encode_30bit(dst+dstc,dsta-dstc,v);
    
    // Find the map and plane for this principal piece.
    const struct map *map=map_by_id(j->mapid);
    if (!map) return -1;
    const struct plane *plane=plane_by_position(map->z);
    if (!plane) return -1;
    int p_in_storage=map-plane->v;
    if ((p_in_storage<0)||(p_in_storage>=plane->w*plane->h)) return -1;
    int lng=p_in_storage%plane->w;
    int lat=p_in_storage/plane->w;
    
    // Search all remaining pieces and collect those on the same plane, with the same xform, whose positions are in the expected place.
    const struct jigstore *alsov[256];
    int alsoc=0;
    int otheri=i+1;
    for (;otheri<store->jigstorec;otheri++) {
      if (visitv[otheri>>3]&(1<<(otheri&7))) continue; // Skip, we already emitted this one.
      const struct jigstore *other=store->jigstorev+otheri;
      if (other->xform!=j->xform) continue; // Skip, xform must match.
      const struct map *omap=map_by_id(other->mapid);
      if (!omap||(omap->z!=plane->z)) continue; // Skip, invalid or different plane.
      int opsto=omap-plane->v;
      if ((opsto<0)||(opsto>=plane->w*plane->h)) continue; // Skip, invalid position in plane.
      int olng=opsto%plane->w;
      int olat=opsto/plane->w;
      int dlng=olng-lng;
      int dlat=olat-lat;
      int expectx,expecty;
      switch (j->xform) {
        case                             0: expectx=j->x+dlng*NS_sys_mapw; expecty=j->y+dlat*NS_sys_maph; break;
        case EGG_XFORM_YREV|EGG_XFORM_SWAP: expectx=j->x-dlat*NS_sys_maph; expecty=j->y+dlng*NS_sys_mapw; break;
        case EGG_XFORM_XREV|EGG_XFORM_YREV: expectx=j->x-dlng*NS_sys_mapw; expecty=j->y-dlat*NS_sys_maph; break;
        case EGG_XFORM_XREV|EGG_XFORM_SWAP: expectx=j->x+dlat*NS_sys_maph; expecty=j->y-dlng*NS_sys_mapw; break;
        default: continue;
      }
      if ((other->x!=expectx)||(other->y!=expecty)) continue; // Skip, not aligned.
      alsov[alsoc++]=other;
      if (alsoc>=256) break;
    }
    
    // Count sequential mapid from (j->mapid+1), how many do we have in (alsov)?
    int nmapid=j->mapid+1;
    int sequentialc=0;
    while (sequentialc<128) {
      const struct jigstore *q=jigstore_find_mapid(alsov,alsoc,nmapid);
      if (!q) break;
      nmapid++;
      sequentialc++;
      int qi=q-store->jigstorev;
      if ((qi<0)||(qi>=store->jigstorec)) return -1;
      visitv[qi>>3]|=1<<(qi&7);
    }
    
    // However many remain in (alsov) should be emitted by name.
    //XXX But I've found this tends to produce worse compression, so we're skipping it.
    // I believe if we could force mapid contiguous within each plane, this would be beneficial.
    // Needs further testing with intermediate states. I've only tried with best and worst case scenarios.
    int namedc=0; // alsoc-sequentialc;
    if (namedc>=128) namedc=127;
    
    // Emit a fake command with (sequentialc,namedc), and then the named mapids.
    dstc+=store_encode_30bit(dst+dstc,dsta-dstc,(sequentialc<<7)|namedc);
    int alsop=0;
    while (namedc-->0) {
      while (alsop<alsoc) {
        const struct jigstore *named=alsov[alsop];
        if ((named->mapid>=j->mapid)&&(named->mapid<nmapid)) alsop++;
        else break;
      }
      if (alsop>=alsoc) return -1;
      const struct jigstore *named=alsov[alsop];
      dstc+=store_encode_12bit(dst+dstc,dsta-dstc,named->mapid);
      alsop++;
      int qi=named-store->jigstorev;
      if ((qi<0)||(qi>=store->jigstorec)) return -1;
      visitv[qi>>3]|=1<<(qi&7);
    }
  }
  return dstc;
}

/* Decode invstorev.
 */
 
static int store_decode_invstorev(struct store *store,const char *src,int srcc) {
  memset(store->invstorev,0,sizeof(store->invstorev));
  int srcp=0,dstp=0;
  while (srcp<srcc) {
    int v=store_decode_base64_digit(src[srcp++]);
    if (v<0) return -1;
    if (v<63) {
      store->invstorev[dstp].itemid=v;
    } else {
      v=store_decode_24bit(src+srcp,srcc-srcp);
      if (v<0) return -1;
      srcp+=4;
      store->invstorev[dstp].itemid=v>>16;
      store->invstorev[dstp].limit=v>>8;
      store->invstorev[dstp].quantity=v;
    }
    if (++dstp>=INVSTORE_SIZE) break;
  }
  return 0;
}

/* Encode invstorev.
 */
 
static int store_encode_invstorev(char *dst,int dsta,const struct store *store) {
  int dstc=0;
  const struct invstore *invstore=store->invstorev;
  int i=INVSTORE_SIZE;
  for (;i-->0;invstore++) {
    if ((invstore->itemid<63)&&!invstore->limit&&!invstore->quantity) {
      APPEND(invstore->itemid);
    } else {
      APPEND(63)
      dstc+=store_encode_24bit(dst+dstc,dsta-dstc,(invstore->itemid<<16)|(invstore->limit<<8)|invstore->quantity);
    }
  }
  return dstc;
}

/* Decode a heap with leading length.
 * Caller supplies a callback that receives only the payload.
 */
 
static int store_decode_heap(
  struct store *store,
  const char *src,int srcc,
  int (*cb)(struct store *store,const char *src,int srcc)
) {
  if (srcc<2) return -1;
  int len=store_decode_12bit(src,2);
  if (len<0) return -1;
  if (2+len>srcc) return -1;
  if (cb(store,src+2,len)<0) return -1;
  return 2+len;
}

/* Encode a heap with leading length.
 * Caller supplies a callback to generate the actual content.
 */
 
static int store_encode_heap(
  char *dst,int dsta,
  const struct store *store,
  int (*cb)(char *dst,int dsta,const struct store *store)
) {
  // Skip the two length bytes.
  int len=cb(dst+2,dsta-2,store);
  if (len<0) return len;
  if (len>0xfff) return -1; // Limit 4095 per heap; they all need considerably less than that.
  if (dsta>=2) {
    dst[0]=store_base64_alphabet[len>>6];
    dst[1]=store_base64_alphabet[len&0x3f];
  }
  return 2+len;
}

/* Public: Decode to store.
 */
 
int store_decode(struct store *store,const char *src,int srcc) {
  if (!store) return -1;
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc<1)||(src[0]!='/')) return -1; // Signature.
  int srcp=1,err;
  
  if ((err=store_decode_heap(store,src+srcp,srcc-srcp,store_decode_fldv))<0) return -1; srcp+=err;
  if ((err=store_decode_heap(store,src+srcp,srcc-srcp,store_decode_fld16v))<0) return -1; srcp+=err;
  if ((err=store_decode_heap(store,src+srcp,srcc-srcp,store_decode_clockv))<0) return -1; srcp+=err;
  if ((err=store_decode_heap(store,src+srcp,srcc-srcp,store_decode_jigstorev))<0) return -1; srcp+=err;
  if ((err=store_decode_heap(store,src+srcp,srcc-srcp,store_decode_invstorev))<0) return -1; srcp+=err;
  
  if (srcp!=srcc-5) return -1;
  int expect=store_decode_30bit(src+srcp,srcc-srcp);
  int actual=store_checksum(src,srcp);
  if (expect!=actual) return -1;
  
  return 0;
}

/* Public: Encode store.
 */

int store_encode(char *dst,int dsta,const struct store *store) {
  if (!dst||(dsta<0)) dsta=0;
  if (!store) return -1;
  int dstc=0,err;
  
  if (dstc<dsta) dst[dstc]='/';
  dstc++;
  
  if ((err=store_encode_heap(dst+dstc,dsta-dstc,store,store_encode_fldv))<0) return -1; dstc+=err;
  if ((err=store_encode_heap(dst+dstc,dsta-dstc,store,store_encode_fld16v))<0) return -1; dstc+=err;
  if ((err=store_encode_heap(dst+dstc,dsta-dstc,store,store_encode_clockv))<0) return -1; dstc+=err;
  if ((err=store_encode_heap(dst+dstc,dsta-dstc,store,store_encode_jigstorev))<0) return -1; dstc+=err;
  if ((err=store_encode_heap(dst+dstc,dsta-dstc,store,store_encode_invstorev))<0) return -1; dstc+=err;
  
  int checksum=0;
  if (dstc<=dsta-5) {
    checksum=store_checksum(dst,dstc);
    dstc+=store_encode_30bit(dst+dstc,dsta-dstc,checksum);
  } else {
    dstc+=5;
  }
  
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

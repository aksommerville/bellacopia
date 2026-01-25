#include "game/bellacopia.h"

#define STORE_SAVE_DEBOUNCE 2.000
#define STORE_MAX_ENCODED_SIZE 2048

/* Sanitize a fresh store, after clear or load.
 * Sets a few defaults, forces valid HP, any other non-negotiable state.
 */
 
static int store_sanitize() {

  // (hp,hpmax) can't be less than 3, and (hp) can't exceed (hpmax).
  int hp=store_get_fld16(NS_fld16_hp);
  int hpmax=store_get_fld16(NS_fld16_hpmax);
  if (hpmax<3) store_set_fld16(NS_fld16_hpmax,hpmax=3);
  if (hp<3) store_set_fld16(NS_fld16_hp,hp=3);
  else if (hp>hpmax) store_set_fld16(NS_fld16_hp,hp=hpmax);
  
  // (goldmax) can't be less than 99, and (gold) can't exceed (goldmax).
  int gold=store_get_fld16(NS_fld16_gold);
  int goldmax=store_get_fld16(NS_fld16_goldmax);
  if (goldmax<99) store_set_fld16(NS_fld16_goldmax,goldmax=99);
  if (gold>goldmax) store_set_fld16(NS_fld16_gold,gold=goldmax);

  g.store.dirty=0;
  return 0;
}

/* Clear.
 */
 
int store_clear() {
  g.store.fldc=0;
  g.store.fld16c=0;
  g.store.clockc=0;
  g.store.jigstorec=0;
  memset(g.store.invstorev,0,sizeof(g.store.invstorev));
  
  // Those fld16 that hold initial limits must get populated.
  store_set_fld16(NS_fld16_hpmax,3);
  store_set_fld16(NS_fld16_hp,3);
  store_set_fld16(NS_fld16_goldmax,99);
  
  return store_sanitize();
}

/* Grow sections.
 * Caller provides the desired count. (c) is ignored.
 */
 
static int store_require_fldv(int totalc) {
  if (totalc<=g.store.flda) return 0;
  void *nv=realloc(g.store.fldv,totalc);
  if (!nv) return -1;
  g.store.fldv=nv;
  g.store.flda=totalc;
  return 0;
}

static int store_require_fld16v(int totalc) {
  if (totalc<=g.store.fld16a) return 0;
  if (totalc>INT_MAX/2) return -1;
  void *nv=realloc(g.store.fld16v,totalc<<1);
  if (!nv) return -1;
  g.store.fld16v=nv;
  g.store.fld16a=totalc;
  return 0;
}

static int store_require_clockv(int totalc) {
  if (totalc<=g.store.clocka) return 0;
  if (totalc>INT_MAX/sizeof(double)) return -1;
  void *nv=realloc(g.store.clockv,sizeof(double)*totalc);
  if (!nv) return -1;
  g.store.clockv=nv;
  g.store.clocka=totalc;
  return 0;
}

static int store_require_jigstorev(int totalc) {
  if (totalc<=g.store.jigstorea) return 0;
  if (totalc>INT_MAX/sizeof(struct jigstore)) return -1;
  void *nv=realloc(g.store.jigstorev,sizeof(struct jigstore)*totalc);
  if (!nv) return -1;
  g.store.jigstorev=nv;
  g.store.jigstorea=totalc;
  return 0;
}

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
 
static int store_decode_12bit(const uint8_t *src,int srcc) {
  if (srcc<2) return -1;
  int hi=store_decode_base64_digit(src[0]);
  int lo=store_decode_base64_digit(src[1]);
  if ((hi<0)||(lo<0)) return -1;
  return (hi<<6)|lo;
}
 
static int store_decode_18bit(const uint8_t *src,int srcc) {
  if (srcc<3) return -1;
  int a=store_decode_base64_digit(src[0]);
  int b=store_decode_base64_digit(src[1]);
  int c=store_decode_base64_digit(src[2]);
  if ((a<0)||(b<0)||(c<0)) return -1;
  return (a<<12)|(b<<6)|c;
}
 
static int store_decode_30bit(const uint8_t *src,int srcc) {
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
 
static int store_decode_base64(uint8_t *dst,int dsta,const uint8_t *src,int srcc) {
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

/* Load.
 */
 
static int store_load_fail() {
  fprintf(stderr,"Failed to decode saved game.\n");
  return store_clear();
}

int store_load(const char *k,int kc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  
  // Acquire the Base64-ish encoded game.
  uint8_t src[STORE_MAX_ENCODED_SIZE];
  int srcc=egg_store_get((char*)src,sizeof(src),k,kc);
  if ((srcc<10)||(srcc>sizeof(src))) {
    fprintf(stderr,"Reject saved game '%.*s' due to length %d.\n",kc,k,srcc);
    return store_clear();
  }
  
  // The first 10 encoded bytes are lengths of the subsequent heaps.
  int srcp=0,fldc,fld16c,clockc,jigstorec,invstorec;
  if ((fldc=store_decode_12bit(src+srcp,srcc-srcp))<0) return store_load_fail(); srcp+=2;
  if ((fld16c=store_decode_12bit(src+srcp,srcc-srcp))<0) return store_load_fail(); srcp+=2;
  if ((clockc=store_decode_12bit(src+srcp,srcc-srcp))<0) return store_load_fail(); srcp+=2;
  if ((jigstorec=store_decode_12bit(src+srcp,srcc-srcp))<0) return store_load_fail(); srcp+=2;
  if ((invstorec=store_decode_12bit(src+srcp,srcc-srcp))<0) return store_load_fail(); srcp+=2;
  
  // 1-bit fields. The length we've read is the decoded length in bytes. Must be a multiple of 3.
  if (fldc%3) return store_load_fail();
  int srcexpect=(fldc*4)/3;
  if (srcp>srcc-srcexpect) return store_load_fail();
  if (store_require_fldv(fldc)<0) return store_load_fail();
  if ((g.store.fldc=store_decode_base64(g.store.fldv,g.store.flda,src+srcp,srcexpect))!=fldc) return store_load_fail();
  srcp+=srcexpect;
  
  // 16-bit fields. Each encodes as three bytes of base64.
  srcexpect=fld16c*3;
  if (srcp>srcc-srcexpect) return store_load_fail();
  if (store_require_fld16v(fld16c)<0) return store_load_fail();
  uint16_t *dst16=g.store.fld16v;
  int i=fld16c;
  for (;i-->0;dst16++,srcp+=3) {
    int v=store_decode_18bit(src+srcp,srcc-srcp);
    if ((v<0)||(v&~0xffff)) return store_load_fail();
    *dst16=v;
  }
  g.store.fld16c=fld16c;
  
  // Clocks. Each is 5 bytes encoded, ie 30 bits.
  srcexpect=clockc*5;
  if (srcp>srcc-srcexpect) return store_load_fail();
  if (store_require_clockv(clockc)<0) return store_load_fail();
  double *dstd=g.store.clockv;
  for (i=clockc;i-->0;dstd++,srcp+=5) {
    int v=store_decode_30bit(src+srcp,srcc-srcp);
    if (v<0) return store_load_fail();
    *dstd=(double)v/1000.0;
  }
  g.store.clockc=clockc;
  
  // Jigstore. Five bytes each, packed bitwise.
  srcexpect=jigstorec*5;
  if (srcp>srcc-srcexpect) return store_load_fail();
  if (store_require_jigstorev(jigstorec)<0) return store_load_fail();
  struct jigstore *jigstore=g.store.jigstorev;
  for (i=jigstorec;i-->0;jigstore++,srcp+=5) {
    int v=store_decode_30bit(src+srcp,srcc-srcp);
    if (v<0) return store_load_fail();
    jigstore->mapid=v>>19;
    jigstore->x=v>>11;
    jigstore->y=v>>3;
    jigstore->xform=v&7;
  }
  g.store.jigstorec=jigstorec;
  
  // Invstore. Straight base64.
  if (invstorec>INVSTORE_SIZE) return store_load_fail();
  srcexpect=invstorec*4;
  if (srcp>srcc-srcexpect) return store_load_fail();
  if (store_decode_base64((uint8_t*)g.store.invstorev,sizeof(g.store.invstorev),src+srcp,srcexpect)!=invstorec*3) return store_load_fail();
  memset(g.store.invstorev+invstorec,0,sizeof(struct invstore)*(INVSTORE_SIZE-invstorec));
  srcp+=srcexpect;
  
  // Checksum.
  if (srcp!=srcc-5) return store_load_fail();
  int expect=store_decode_30bit(src+srcp,srcc-srcp);
  int actual=store_checksum((char*)src,srcp);
  srcp+=5;
  if (expect!=actual) return store_load_fail();
  
  //TODO Maybe some more invasive consistency checks, like (hp<=maxhp)?
  
  fprintf(stderr,"%s: Decoded saved game, %d bytes.\n",__func__,srcc);
  return store_sanitize();
}

/* Encode store.
 */
 
static int store_encode(char *dst,int dsta) {
  int dstc=0,err;
  
  // When encoded, (fldv) must have a multiple-of-three length. Grow it now if needed.
  int fldc=g.store.fldc;
  int m3=fldc%3;
  if (m3) {
    int nc=fldc+3-m3;
    if (store_require_fldv(nc)<0) return -1;
    while (g.store.fldc<nc) g.store.fldv[g.store.fldc++]=0;
    fldc=nc;
  }
  while ((fldc>=3)&&!g.store.fldv[fldc-1]&&!g.store.fldv[fldc-2]&&!g.store.fldv[fldc-3]) fldc-=3;
  dstc+=store_encode_12bit(dst+dstc,dsta-dstc,fldc);
  
  int fld16c=g.store.fld16c;
  while (fld16c&&!g.store.fld16v[fld16c-1]) fld16c--;
  dstc+=store_encode_12bit(dst+dstc,dsta-dstc,fld16c);
  
  int clockc=g.store.clockc;
  dstc+=store_encode_12bit(dst+dstc,dsta-dstc,clockc);
  
  int jigstorec=g.store.jigstorec;
  dstc+=store_encode_12bit(dst+dstc,dsta-dstc,jigstorec);
  
  int invstorec=INVSTORE_SIZE;
  while (invstorec&&!g.store.invstorev[invstorec-1].itemid) invstorec--;
  dstc+=store_encode_12bit(dst+dstc,dsta-dstc,invstorec);
  
  // fldv
  dstc+=store_encode_base64(dst+dstc,dsta-dstc,g.store.fldv,fldc);
  
  // fld16v
  const uint16_t *src16=g.store.fld16v;
  int i=fld16c;
  for (;i-->0;src16++) {
    dstc+=store_encode_18bit(dst+dstc,dsta-dstc,*src16);
  }
  
  // clockv
  const double *srcd=g.store.clockv;
  for (i=clockc;i-->0;srcd++) {
    int v=(int)((*srcd)*1000.0);
    if (v&~0x3fffffff) v=0x3fffffff; // Going over in a single session would take 37 days. If it happens, they get what they deserve.
    dstc+=store_encode_30bit(dst+dstc,dsta-dstc,v);
  }
  
  // jigstorev
  const struct jigstore *jigstore=g.store.jigstorev;
  for (i=jigstorec;i-->0;jigstore++) {
    int v=(jigstore->mapid<<19)|(jigstore->x<<11)|(jigstore->y<<3)|jigstore->xform;
    dstc+=store_encode_30bit(dst+dstc,dsta-dstc,v);
  }
  
  // invstorev
  dstc+=store_encode_base64(dst+dstc,dsta-dstc,(uint8_t*)g.store.invstorev,invstorec*3);
  
  // checksum
  int sum=0;
  if (dstc<=dsta) sum=store_checksum(dst,dstc);
  dstc+=store_encode_30bit(dst+dstc,dsta-dstc,sum);
  
  return dstc;
}

/* Save.
 */

static int store_save_now(const char *k,int kc) {
  g.store.dirty=0;
  g.store.savedebounce=0.0;
  char serial[STORE_MAX_ENCODED_SIZE];
  int serialc=store_encode(serial,sizeof(serial));
  if ((serialc<0)||(serialc>sizeof(serial))) {
    fprintf(stderr,"%s: Failed to encode store! c=%d\n",__func__,serialc);
    return -1;
  }
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (egg_store_set(k,kc,serial,serialc)<0) {
    fprintf(stderr,"%s: Failed to save store, encoded to %d bytes\n",__func__,serialc);
    return -1;
  } else {
    //fprintf(stderr,"%s: Saved store as '%.*s', %d bytes\n",__func__,kc,k,serialc);
  }
  return 0;
}

/* Save if dirty.
 */

void store_save_if_dirty(const char *k,int kc,int immediate) {
  if (!g.store.dirty) return;
  if (immediate) {
    store_save_now(k,kc);
  } else {
    double now=egg_time_real();
    if (g.store.savedebounce<=0.0) {
      g.store.savedebounce=now;
    } else if (now-g.store.savedebounce>=STORE_SAVE_DEBOUNCE) {
      store_save_now(k,kc);
    }
  }
}

/* Read.
 */

int store_get_fld(int fld) {
  // A few special fields are immutable, whether we have them for real or not:
  switch (fld) {
    case NS_fld_zero: return 0;
    case NS_fld_one: return 1;
    case NS_fld_alsozero: return 0;
  }
  if (fld<0) return 0;
  int p=fld>>3;
  if (p>=g.store.fldc) return 0;
  uint8_t mask=1<<(fld&7);
  return (g.store.fldv[p]&mask)?1:0;
}

int store_get_fld16(int fld16) {
  if ((fld16<0)||(fld16>=g.store.fld16c)) return 0;
  return g.store.fld16v[fld16];
}

double store_get_clock(int clock) {
  if ((clock<0)||(clock>=g.store.clockc)) return 0.0;
  return g.store.clockv[clock];
}

struct jigstore *store_get_jigstore(int mapid) {
  if (mapid&~0x07ff) return 0;
  struct jigstore *jigstore=g.store.jigstorev;
  int i=g.store.jigstorec;
  for (;i-->0;jigstore++) {
    if (jigstore->mapid==mapid) return jigstore;
  }
  return 0;
}

struct invstore *store_get_invstore(int invslot) {
  if ((invslot<0)||(invslot>=INVSTORE_SIZE)) return 0;
  return g.store.invstorev+invslot;
}

struct invstore *store_get_itemid(int itemid) {
  if (itemid&~0xff) return 0;
  struct invstore *invstore=g.store.invstorev;
  int i=INVSTORE_SIZE;
  for (;i-->0;invstore++) {
    if (invstore->itemid==itemid) return invstore;
  }
  return 0;
}

/* Set fields.
 */

int store_set_fld(int fld,int value) {
  if (fld<=NS_fld_alsozero) return 0;
  int p=fld>>3;
  if (p>=g.store.fldc) {
    if (!value) return 0;
    if (store_require_fldv(p+1)<0) return -1;
    while (g.store.fldc<=p) g.store.fldv[g.store.fldc++]=0;
  }
  uint8_t mask=1<<(fld&7);
  if (value) {
    if (g.store.fldv[p]&mask) return 0;
    g.store.fldv[p]|=mask;
  } else {
    if (!(g.store.fldv[p]&mask)) return 0;
    g.store.fldv[p]&=~mask;
  }
  g.store.dirty=1;
  return 1;
}

int store_set_fld16(int fld,int value) {
  if (fld<0) return -1;
  if (fld>=g.store.fld16c) {
    if (!value) return 0;
    if (store_require_fld16v(fld+1)<0) return -1;
    while (g.store.fld16c<=fld) g.store.fld16v[g.store.fld16c++]=0;
  }
  if (g.store.fld16v[fld]==value) return 0;
  g.store.fld16v[fld]=value;
  g.store.dirty=1;
  return 1;
}

/* Choose position and xform for new jigstore.
 * It's random but controlled in complicated ways.
 */

static void store_choose_jigstore_position(struct jigstore *jigstore) {
  
  /* (xform) may only be those of even chirality, zero or two bits set.
   * ie those you can rotate to from natural.
   */
  switch (rand()&3) {
    case 0: jigstore->xform=0; break;
    case 1: jigstore->xform=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
    case 2: jigstore->xform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
    case 3: jigstore->xform=EGG_XFORM_XREV|EGG_XFORM_SWAP; break;
  }
  
  /* If we have to bail out, eg for the first piece of a puzzle, it goes in the middle.
   */
  jigstore->x=JIGSAW_FLDW>>1;
  jigstore->y=JIGSAW_FLDH>>1;
  
  /* Get the map in question, so we can compare (z).
   */
  const struct map *map=map_by_id(jigstore->mapid);
  if (!map) return;
  
  /* Collect all jigstores on this plane.
   * We only store mapid per jigpiece, so there's a lot of looking-up.
   * Best to front load that effort.
   * No others is perfectly possible, eg the first jigpiece you pick up. In that case, we're already ready.
   */
  const struct jigstore *otherv[100]; // Largest plane is 10x10, we'll keep it that way.
  int otherc=0;
  const struct jigstore *q=g.store.jigstorev;
  int i=g.store.jigstorec;
  for (;i-->0;q++) {
    const struct map *qmap=map_by_id(q->mapid);
    if (!qmap) continue;
    if (qmap->z!=map->z) continue;
    otherv[otherc++]=q;
    if (otherc>=100) break;
  }
  if (!otherc) return;
  
  /* Take a few random guesses at position, and keep whichever yields the most breathing room.
   * I'm not crazy about this algorithm. It tends to favor the edges.
   */
  int repc=20;
  double bestscore=999999.999;
  while (repc-->0) {
    int x=rand()%JIGSAW_FLDW;
    int y=rand()%JIGSAW_FLDH;
    double score=0.0;
    const struct jigstore **otherp=otherv;
    int i=otherc;
    for (;i-->0;otherp++) {
      const struct jigstore *other=*otherp;
      int dx=other->x-x;
      int dy=other->y-y;
      if (!dx&&!dy) score+=1.0;
      else score+=1.0/(double)(dx*dx+dy*dy);
    }
    if (score<bestscore) {
      bestscore=score;
      jigstore->x=x;
      jigstore->y=y;
    }
  }
}

/* Add jigstore.
 */

struct jigstore *store_add_jigstore(int mapid) {
  if (mapid&~0x07ff) return 0;
  struct jigstore *jigstore=g.store.jigstorev;
  int i=g.store.jigstorec;
  for (;i-->0;jigstore++) {
    if (jigstore->mapid==mapid) return jigstore;
  }
  if (store_require_jigstorev(g.store.jigstorec+1)<0) return 0;
  jigstore=g.store.jigstorev+g.store.jigstorec;
  jigstore->mapid=mapid;
  store_choose_jigstore_position(jigstore);
  g.store.jigstorec++;
  g.store.dirty=1;
  return jigstore;
}

/* Add item.
 */

struct invstore *store_add_itemid(int itemid,int quantity) {
  if (itemid&~0xff) return 0;
  struct invstore *blank=0;
  struct invstore *invstore=g.store.invstorev;
  int i=INVSTORE_SIZE;
  for (;i-->0;invstore++) {
    if (invstore->itemid==itemid) {
      if (invstore->limit) {
        int nq=invstore->quantity+quantity;
        if (nq>invstore->limit) invstore->quantity=invstore->limit;
        else invstore->quantity=nq;
        g.store.dirty=1;
      }
      return invstore;
    } else if (!invstore->itemid&&!blank) {
      blank=invstore;
    }
  }
  if (!blank) return 0;
  blank->itemid=itemid;
  if (quantity) {
    blank->quantity=quantity;
    const struct item_detail *detail=item_detail_for_itemid(itemid);
    if (detail&&(detail->initial_limit>0)) blank->limit=detail->initial_limit;
    else blank->limit=quantity;
  }
  g.store.dirty=1;
  return blank;
}

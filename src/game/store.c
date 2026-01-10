#include "game.h"
#include "world/map.h"

#define STORE_SIZE_BYTES 1024
#define FIELD_SIZE_LIMIT 16

static struct {
  
  uint8_t v[STORE_SIZE_BYTES];
  
  struct listener {
    int listenerid;
    int fld,size;
    void (*cb)(int fld,int size,int v,void *userdata);
    void *userdata;
  } *listenerv;
  int listenerc,listenera;
  int listenerid_next;
  int dirty;
  
  /* Jigsaw content.
   * (jc) is the size of both (jraw) and (jenc) in maps, not bytes.
   * Each map is 3 bytes, and the stores are indexed by (mapid-1)*3.
   * (jraw) is [x,y,xform] with (xform==0xff) if piece not found yet.
   * (jenc) is 3 bytes of base64, ie 18 bits. Pack those big-endianly, then:
   *    xxxxxx xxyyyy yyyyrr
   *    (r) is the rotation: (0,1,2,3) = (natural,clockwise,180,deasil)
   *    The entire construction is ones if not found yet. (y==255 is definitely not valid).
   * Render order does not persist.
   */
  uint8_t *jraw;
  char *jenc;
  int jc;
  int jdirty;
  int jdebounce;
  
} store={0};

static int store_jigsaw_init();

/* Reset.
 */
 
void store_reset() {
  if (store.listenerv) free(store.listenerv);
  if (store.jraw) free(store.jraw);
  if (store.jenc) free(store.jenc);
  memset(&store,0,sizeof(store));
  store.v[0]=0x02; // NS_fld_one = 1
  store_set(NS_fld_hp,4,3);
  store_set(NS_fld_hpmax,4,3);
  store_jigsaw_init();
}

/* Immediate field access.
 */
 
int store_get(int fld,int size) {
  if ((size<1)||(size>FIELD_SIZE_LIMIT)) return 0;
  if (fld<0) return 0;
  if (fld+size>STORE_SIZE_BYTES<<3) return 0;
  int p=fld>>3;
  int shift=fld&7;
  if (size==1) { // Very common, and it's guaranteed to all be in one byte.
    return (store.v[p]&(1<<shift))?1:0;
  }
  int v=store.v[p]>>shift;
  int have=8-shift;
  shift=have;
  while (have<size) {
    p++;
    v|=store.v[p]<<shift;
    shift+=8;
    have+=8;
  }
  return v&((1<<size)-1);
}

int store_set(int fld,int size,int v) {
  if ((size<1)||(size>FIELD_SIZE_LIMIT)) return 0;
  if (fld<2) return 0; // Fields 0 and 1 exist, but are read-only.
  if (fld+size>STORE_SIZE_BYTES<<3) return 0;
  int p=fld>>3;
  int shift=fld&7;
  if (size==1) { // Optimize for this common and easy size.
    uint8_t mask=1<<shift;
    if (v&1) {
      if (store.v[p]&mask) return 0;
      store.v[p]|=mask;
    } else {
      if (!(store.v[p]&mask)) return 0;
      store.v[p]&=~mask;
    }
    return 1;
  }
  int result=0;
  int limit=(1<<size)-1;
  if (v>limit) v=limit;
  int v0=v;
  int rsize=size;
  if (shift) {
    uint8_t mask=((1<<size)-1)<<shift;
    int subv=(v<<shift)&mask;
    if ((store.v[p]&mask)!=subv) {
      result=1;
      store.v[p]=(store.v[p]&~mask)|subv;
    }
    p++;
    int cpc=8-shift;
    v>>=cpc;
    rsize-=cpc;
  }
  while (rsize>0) {
    uint8_t mask=(1<<size)-1;
    int subv=v&mask;
    if ((store.v[p]&mask)!=subv) {
      result=1;
      store.v[p]=(store.v[p]&~mask)|subv;
    }
    rsize-=8;
    v>>=8;
    p++;
  }
  if (!result) return 0;
  store.dirty=1;
  // Iterate listeners backward for broadcast in case they remove themselves.
  int i=store.listenerc;
  struct listener *listener=store.listenerv+i-1;
  for (;i-->0;listener--) {
    if ((listener->fld==fld)&&(listener->size==size)) { // Exact match is typical and easy.
      listener->cb(fld,size,v0,listener->userdata);
    } else if (!listener->fld&&!listener->size) { // Match-all listeners are easy too.
      listener->cb(fld,size,v0,listener->userdata);
    } else if ((listener->fld<fld+size)&&(listener->fld+listener->size>fld)) {
      // Listener covers a range that overlaps what was affected. Unusual but legal. Need to read the listener-sized field.
      listener->cb(listener->fld,listener->size,store_get(listener->fld,listener->size),listener->userdata);
    }
  }
  return result;
}

/* Listeners.
 */
 
int store_listen(int fld,int size,void (*cb)(int fld,int size,int v,void *userdata),void *userdata) {
  if (!fld&&!size) {
    // (0,0) is a legal special case.
  } else {
    if ((size<1)||(size>FIELD_SIZE_LIMIT)) return -1;
    if ((fld<0)||(fld+size<STORE_SIZE_BYTES<<3)) return -1;
  }
  if (store.listenerc>=store.listenera) {
    int na=store.listenera+16;
    if (na>INT_MAX/sizeof(struct listener)) return -1;
    void *nv=realloc(store.listenerv,sizeof(struct listener)*na);
    if (!nv) return -1;
    store.listenerv=nv;
    store.listenera=na;
  }
  struct listener *listener=store.listenerv+store.listenerc++;
  memset(listener,0,sizeof(struct listener));
  if (store.listenerid_next<1) store.listenerid_next=1;
  listener->listenerid=store.listenerid_next++;
  listener->fld=fld;
  listener->size=size;
  listener->cb=cb;
  listener->userdata=userdata;
  return listener->listenerid;
}

void store_unlisten(int listenerid) {
  if (listenerid<1) return;
  int i=store.listenerc;
  struct listener *listener=store.listenerv+i-1;
  for (;i-->0;listener--) {
    if (listener->listenerid!=listenerid) continue;
    store.listenerc--;
    memmove(listener,listener+1,sizeof(struct listener)*(store.listenerc-i));
    return;
  }
}

void store_unlisten_all() {
  store.listenerc=0;
}

/* Encode store for save.
 */
 
static const char alphabet[]=
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789+/"
;
 
static int store_encode(char *dst,int dsta,const uint8_t *src,int srcc) {
  if (srcc<0) srcc=0;
  while (srcc%3) srcc--; // Assume the tail is zeroes, and get back to a multiple of 3.
  while ((srcc>=3)&&!src[srcc-1]&&!src[srcc-2]&&!src[srcc-3]) srcc-=3; // Drop zeroes, three bytes at a time.
  int chunkc=srcc/3;
  int dstc=chunkc*4;
  if (dstc<=dsta) {
    for (;chunkc-->0;dst+=4,src+=3) {
      dst[0]=alphabet[src[0]>>2];
      dst[1]=alphabet[((src[0]<<4)|(src[1]>>4))&0x3f];
      dst[2]=alphabet[((src[1]<<2)|(src[3]>>6))&0x3f];
      dst[3]=alphabet[src[3]&0x3f];
    }
  }
  return dstc;
}

/* Load.
 */
 
static inline int store_decode_digit(char src) {
  if ((src>='A')&&(src<='Z')) return src-'A';
  if ((src>='a')&&(src<='z')) return src-'a'+26;
  if ((src>='0')&&(src<='9')) return src-'0'+52;
  if (src=='+') return 62;
  if (src=='/') return 63;
  return -1;
}
 
int store_load() {
  store_reset();
  char tmp[1024];
  int srcc=egg_store_get(tmp,sizeof(tmp),"save",4);
  if ((srcc<0)||(srcc>sizeof(tmp))||(srcc&3)) return -1;
  fprintf(stderr,"%s:%d: TODO: Load store from persistence: '%.*s'\n",__FILE__,__LINE__,srcc,tmp);
  if (!srcc) return 0;
  int chunkc=srcc>>2;
  int dstc=chunkc*3;
  if (dstc>STORE_SIZE_BYTES) return -1;
  char *src=tmp;
  uint8_t *dst=store.v;
  for (;chunkc-->0;src+=4,dst+=3) {
    int a=store_decode_digit(src[0]);
    int b=store_decode_digit(src[1]);
    int c=store_decode_digit(src[2]);
    int d=store_decode_digit(src[3]);
    if ((a<0)||(b<0)||(c<0)||(d<0)) {
      store_reset();
      return -1;
    }
    dst[0]=(a<<2)|(b>>4);
    dst[1]=(b<<4)|(c>>2);
    dst[2]=(c<<6)|d;
  }
  if ((store.v[0]&3)!=2) {
    store_reset();
    return -1;
  }
  //TODO Further validation. Maybe a broad checksum?
  return 0;
}

/* Save.
 */
 
int store_save() {
  char tmp[1024];
  int tmpc=store_encode(tmp,sizeof(tmp),store.v,sizeof(store.v));
  if ((tmpc<0)||(tmpc>sizeof(tmp))) {
    fprintf(stderr,"Failed to encode saved game!\n");
    return -1;
  }
  //fprintf(stderr,"Encoded saved game: '%.*s'\n",tmpc,tmp);
  store.dirty=0;
  return egg_store_set("save",4,tmp,tmpc);
}

int store_save_if_dirty() {
  if (!store.dirty) return 0;
  return store_save();
}

/* Jigsaw store.
 *******************************************************************************/

static int store_jigsaw_init() {

  // Get the highest mapid.
  int resp=res_search(EGG_TID_map+1,0);
  if (resp<0) resp=-resp-1;
  resp--;
  if (resp<0) return -1; // No maps, and nothing TOC'd with (tid) less than map? Highly fishy!
  const struct rom_entry *res=g.resv+resp;
  if (res->tid!=EGG_TID_map) return -1; // No maps. Also fishy.
  int mapc=res->rid; // We're going to store them sparsely, indexing on (mapid-1).
  
  // Allocate both buffers.
  int bufsize=mapc*3;
  if (!(store.jraw=malloc(bufsize))) return -1;
  if (!(store.jenc=malloc(bufsize))) return -1;
  
  // They both start straight ones, ie no piece is found yet.
  // But the encoded dump is pre-encoded, so "straight ones" means "slashes".
  memset(store.jraw,0xff,bufsize);
  memset(store.jenc,'/',bufsize);
  
  store.jc=mapc;
  return 0;
}

/* Load jigsaw store.
 */
 
void store_jigsaw_load() {
  if (store.jc<1) return;

  /* Optimistically read directly from Egg's store into our encoded dump.
   * If length doesn't match exactly, blank the whole thing.
   */
  int bufsize=store.jc*3;
  int encc=egg_store_get(store.jenc,bufsize,"jigsaw",6);
  if (encc!=bufsize) {
    fprintf(stderr,"Blanking jigsaw due to incorrect encoded length. Expected %d, found %d.\n",bufsize,encc);
    memset(store.jraw,0xff,bufsize);
    memset(store.jenc,'/',bufsize);
    return;
  }
  
  /* Decode each unit in turn.
   * Anything fishy at all, blank it all.
   */
  uint8_t *dst=store.jraw;
  const char *src=store.jenc;
  int i=store.jc;
  for (;i-->0;dst+=3,src+=3) {
    int a=store_decode_digit(src[0]);
    int b=store_decode_digit(src[1]);
    int c=store_decode_digit(src[2]);
    if ((a<0)||(b<0)||(c<0)) {
      fprintf(stderr,"Blanking jigsaw due to illegal character. One of these: 0x%02x 0x%02x 0x%02x.\n",src[0],src[1],src[2]);
      memset(store.jraw,0xff,bufsize);
      memset(store.jenc,'/',bufsize);
      return;
    }
    // Unit is kosher. Swizzle into the expanded 3-byte construction.
    if ((a==0x3f)&&(b==0x3f)&&(c==0x3f)) {
      dst[0]=dst[1]=dst[2]=0xff;
    } else {
      dst[0]=(a<<2)|(b>>4);
      dst[1]=(b<<4)|(c>>2);
      switch (c&3) {
        case 0: dst[2]=0; break;
        case 1: dst[2]=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
        case 2: dst[2]=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
        case 3: dst[2]=EGG_XFORM_XREV|EGG_XFORM_SWAP; break;
      }
    }
  }
  
  store.jdirty=0;
}

/* Save jigsaw store.
 */

void store_jigsaw_save_if_dirty(int immediate) {

  // Get out if no action needed. We expect to be called at high frequency, redundantly.
  if (!store.jdirty) return;
  if (immediate) ;// Proceed regardless of debounce.
  else if (store.jdebounce-->0) return; // High-frequency poll: Proceed only when the debounce hits zero.
  store.jdirty=0;

  // Rewrite (jenc) entirely, from (jraw).
  char *dst=store.jenc;
  const uint8_t *src=store.jraw;
  int i=store.jc;
  for (;i-->0;dst+=3,src+=3) {
    // If xform is not one of the 4 natural-chirality rotations, the piece is undiscovered. That encodes differently.
    int rot=-1;
    switch (src[2]) {
      case 0: rot=0; break;
      case EGG_XFORM_SWAP|EGG_XFORM_YREV: rot=1; break;
      case EGG_XFORM_XREV|EGG_XFORM_YREV: rot=2; break;
      case EGG_XFORM_XREV|EGG_XFORM_SWAP: rot=3; break;
    }
    if (rot<0) {
      dst[0]=dst[1]=dst[2]='/'; // Straight ones.
    } else { // Valid.
      int x=src[0];
      int y=src[1];
      if (y==0xff) y=0xfe; // Don't let it be straight ones. (y) should never go this high to begin with.
      dst[0]=alphabet[x>>2];
      dst[1]=alphabet[((x<<4)|(y>>4))&0x3f];
      dst[2]=alphabet[((y<<2)|rot)&0x3f];
    }
  }
  
  // And hey presto, it's ready to deliver to Egg Store.
  if (egg_store_set("jigsaw",6,store.jenc,store.jc*3)<0) {
    fprintf(stderr,"!!! Failed to save jigsaw state.\n");
  }
}

/* Get stored jigsaw piece.
 */
 
int store_jigsaw_get(int *x,int *y,uint8_t *xform,int mapid) {
  if (mapid<1) return -1;
  mapid--;
  if (mapid>=store.jc) return -1;
  const uint8_t *src=store.jraw+mapid*3;
  *x=src[0];
  *y=src[1];
  *xform=src[2];
  if (*xform==0xff) return 0;
  return 1;
}

/* Save jigsaw piece.
 */
 
int store_jigsaw_set(int mapid,int x,int y,uint8_t xform) {
  if (mapid<1) return -1;
  mapid--;
  if (mapid>=store.jc) return -1;
  uint8_t *dst=store.jraw+mapid*3;
  // Tolerate OOB (x,y). jigsaw.c lets them run wild.
  if (x<0) x=0; else if (x>0xff) x=0xff;
  if (y<0) y=0; else if (y>0xfe) y=0xfe;
  // But if (xform) is not one of the legal ones, force it all to ones.
  // (this isn't really our problem, it will get fixed up at encode either way, but let's try to stay straight throughout).
  switch (xform) {
    case 0:
    case EGG_XFORM_SWAP|EGG_XFORM_YREV:
    case EGG_XFORM_XREV|EGG_XFORM_YREV:
    case EGG_XFORM_XREV|EGG_XFORM_SWAP:
      break;
    default: x=y=xform=0xff;
  }
  // Get out without dirtying if it's redundant.
  if ((dst[0]==x)&&(dst[1]==y)&&(dst[2]==xform)) return 0;
  // OK, write to our store, mark us dirty, and restart the debounce.
  dst[0]=x;
  dst[1]=y;
  dst[2]=xform;
  store.jdirty=1;
  store.jdebounce=50;
  return 0;
}

/* Random position for a new jigpiece.
 * We'll make an effort to put it somewhere vacant.
 */
 
void store_jigpiece_new_position(int *x,int *y,int z) {
  const int fldw=256;
  const int fldh=144;
  
  // Acquire the plane.
  int px,py,pw=0,ph=0,oobx,ooby; // We only care about (pw,ph).
  struct map *mapv=maps_get_plane(&px,&py,&pw,&ph,&oobx,&ooby,z);
  int mapc=pw*ph;
  if (!mapv||(mapc<1)) {
    *x=rand()%fldw;
    *y=rand()%fldh;
    return;
  }
  
  /* Record the jigpiece position for each map in the plane.
   * Fair to assume that there will never be a jigsaw with more than 128 pieces.
   * (I think our largest is 90).
   */
  #define posa 128
  struct pos { int x,y; } posv[posa];
  int posc=0;
  struct map *map=mapv;
  int i=mapc;
  for (;i-->0;map++) {
    int mx,my;
    uint8_t mxform;
    if (store_jigsaw_get(&mx,&my,&mxform,map->rid)<=0) continue;
    struct pos *pos=posv+posc++;
    pos->x=mx;
    pos->y=my;
    if (posc>=posa) break;
  }
  #undef posa
  if (posc<1) {
    *x=rand()%fldw;
    *y=rand()%fldh;
    return;
  }
  
  /* Pick something randomly in (0..fldw,fldh), maximizing distance to everything in (posv).
   * This is an interesting geometry problem that I don't know the solution to it.
   * But of course it's supposed to be imperfect. So do it dumbly:
   * Take a few random samples, and pick whichever yields the highest sum of manhattan distances.
   */
  int hiscore=0;
  int samplec=10; // Arbitrary.
  while (samplec-->0) {
    int score=0;
    int qx=rand()%fldw;
    int qy=rand()%fldh;
    const struct pos *pos=posv;
    int i=posc;
    for (;i-->0;pos++) {
      int dx=qx-pos->x; if (dx<0) score-=dx; else score+=dx;
      int dy=qy-pos->y; if (dy<0) score-=dy; else score+=dy;
    }
    if (score>hiscore) {
      hiscore=score;
      *x=qx;
      *y=qy;
    }
  }
  if (!hiscore) { // eg just one piece and we guessed its exact location every time. Narrowly possible.
    *x=rand()%fldw;
    *y=rand()%fldh;
  }
}

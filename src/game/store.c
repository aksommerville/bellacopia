#include "game.h"

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
} store={0};

/* Reset.
 */
 
void store_reset() {
  if (store.listenerv) free(store.listenerv);
  memset(&store,0,sizeof(store));
  store.v[0]=0x02; // NS_fld_one = 1
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
 
static int store_encode(char *dst,int dsta,const uint8_t *src,int srcc) {
  const char alphabet[]=
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/"
  ;
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
  fprintf(stderr,"Encoded saved game: '%.*s'\n",tmpc,tmp);
  store.dirty=0;
  return egg_store_set("save",4,tmp,tmpc);
}

int store_save_if_dirty() {
  if (!store.dirty) return 0;
  return store_save();
}

/* res.c
 * We manage a generic resource TOC (g.rom,g.resv) and tilesheets (g.tilesheetv).
 * We coordinate with mapstore to load it.
 */

#include "bellacopia.h"

static const uint8_t tilesheet_zeroes[256]={0};

/* Receive tilesheet, prelink.
 * No need to TOC these. We're going to inflate immediately at load.
 */
 
static int res_welcome_tilesheet(const struct rom_entry *res) {

  if (g.tilesheetc>=g.tilesheeta) {
    int na=g.tilesheeta+16;
    if (na>INT_MAX/sizeof(struct tilesheet)) return -1;
    void *nv=realloc(g.tilesheetv,sizeof(struct tilesheet)*na);
    if (!nv) return -1;
    g.tilesheetv=nv;
    g.tilesheeta=na;
  }
  
  struct tilesheet *tilesheet=g.tilesheetv+g.tilesheetc++;
  memset(tilesheet,0,sizeof(struct tilesheet));
  tilesheet->rid=res->rid;
  if (!(tilesheet->physics=calloc(1,256))) return -1;
  if (!(tilesheet->jigctab=calloc(1,256))) return -1;
  
  struct tilesheet_reader reader;
  if (tilesheet_reader_init(&reader,res->v,res->c)<0) return -1;
  struct tilesheet_entry entry;
  while (tilesheet_reader_next(&entry,&reader)>0) {
    uint8_t *dst=0;
    switch (entry.tableid) {
      case NS_tilesheet_physics: dst=tilesheet->physics; break;
      case NS_tilesheet_jigctab: dst=tilesheet->jigctab; break;
    }
    if (!dst) continue;
    memcpy(dst+entry.tileid,entry.v,entry.c);
  }

  return 0;
}

/* Initial processing for one resource.
 * Return >0 to add to the TOC, <0 for any fatal error.
 */
 
static int res_welcome(const struct rom_entry *res) {
  switch (res->tid) {
    // Call out every type. If I forget to add a new type here, it must fail loud and clear.
    case EGG_TID_metadata:
    case EGG_TID_code:
    case EGG_TID_image:
    case EGG_TID_song:
    case EGG_TID_sound:
    case EGG_TID_strings:
      return 0;
    case EGG_TID_sprite:
      return 1;
    case EGG_TID_map: if (mapstore_welcome(res)<0) return -1; return 1;
    case EGG_TID_tilesheet: return res_welcome_tilesheet(res);
    default: {
        fprintf(stderr,"%s:%d: Unexpected resource type %d. Please update %s.\n",__FILE__,__LINE__,res->tid,__func__);
        return -1;
      }
  }
  return 0;
}

/* With the TOC fully populated, now examine resources and perform any extra init.
 * At this point, resources are free to refer to each other.
 */
 
static int res_link() {
  /* Iterate and link per tid, if we need it.
   *
  struct rom_entry *res=g.resv;
  int i=g.resc;
  for (;i-->0;res++) {
    switch (res->tid) {
    }
  }
  /**/
  if (mapstore_link()<0) return -1;
  return 0;
}

/* Init.
 */
 
int res_init() {
  if (g.rom||g.resv||g.tilesheetv) return -1;
  if (mapstore_reset()<0) return -1;
  
  g.romc=egg_rom_get(0,0);
  if (!(g.rom=malloc(g.romc))) return -1;
  if (egg_rom_get(g.rom,g.romc)!=g.romc) return -1;
  text_set_rom(g.rom,g.romc);
  
  g.resc=0;
  int resa=256;
  if (!(g.resv=malloc(sizeof(struct rom_entry)*resa))) return -1;
  struct rom_reader reader;
  if (rom_reader_init(&reader,g.rom,g.romc)<0) return -1;
  struct rom_entry res;
  while (rom_reader_next(&res,&reader)>0) {
    int err=res_welcome(&res);
    if (err<0) return -1;
    if (!err) continue;
    if (g.resc>=resa) {
      resa<<=1;
      if (resa>0x01000000) return -1;
      void *nv=realloc(g.resv,sizeof(struct rom_entry)*resa);
      if (!nv) return -1;
      g.resv=nv;
    }
    g.resv[g.resc++]=res;
  }
  
  return res_link();
}

/* Search TOC.
 */

int res_search(int tid,int rid) {
  int lo=0,hi=g.resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rom_entry *q=g.resv+ck;
         if (tid<q->tid) hi=ck;
    else if (tid>q->tid) lo=ck+1;
    else if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int res_get(void *dstpp,int tid,int rid) {
  int p=res_search(tid,rid);
  if (p<0) return 0;
  const struct rom_entry *res=g.resv+p;
  if (dstpp) *(const void**)dstpp=res->v;
  return res->c;
}

int tilesheet_search(int rid) {
  int lo=0,hi=g.tilesheetc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct tilesheet *q=g.tilesheetv+ck;
         if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

const uint8_t *tilesheet_get_physics(int rid) {
  int p=tilesheet_search(rid);
  if (p<0) return tilesheet_zeroes;
  return g.tilesheetv[p].physics;
}

const uint8_t *tilesheet_get_jigctab(int rid) {
  int p=tilesheet_search(rid);
  if (p<0) return tilesheet_zeroes;
  return g.tilesheetv[p].jigctab;
}

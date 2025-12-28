#include "game.h"
#include "world/map.h"

/* Private globals.
 */
 
static const uint8_t physics_default[256]={0};
 
static struct {

  // Physics table from each tilesheet, expanded.
  uint8_t *physicsv;
  int *physicsidv;
  int physicsidc,physicsida;
  
} res={0};

/* Receive tilesheet.
 */
 
static int res_welcome_tilesheet(int rid,const void *v,int c) {
  uint8_t *dst;
  {
    void *nv=realloc(res.physicsv,(res.physicsidc+1)*256);
    if (!nv) return -1;
    res.physicsv=nv;
    dst=res.physicsv+res.physicsidc*256;
    memset(dst,0,256);
  }
  if (res.physicsidc>=res.physicsida) {
    int na=res.physicsidc+8;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(res.physicsidv,sizeof(int)*na);
    if (!nv) return -1;
    res.physicsidv=nv;
    res.physicsida=na;
  }
  res.physicsidv[res.physicsidc++]=rid;
  struct tilesheet_reader reader;
  if (tilesheet_reader_init(&reader,v,c)<0) return -1;
  struct tilesheet_entry entry;
  while (tilesheet_reader_next(&entry,&reader)>0) {
    if (entry.tableid==NS_tilesheet_physics) {
      memcpy(dst+entry.tileid,entry.v,entry.c);
    }
  }
  return 0; // >0 to TOC it, but there's no need.
}

/* Welcome resource.
 * Return >0 to include in the TOC.
 */
 
static int res_welcome(int tid,int rid,const void *v,int c) {
  switch (tid) {
    case EGG_TID_map:
    case EGG_TID_sprite:
    case EGG_TID_decalsheet:
      return 1;
    case EGG_TID_tilesheet: return res_welcome_tilesheet(rid,v,c);
    // Anything we want to ignore, call it out here. Types not listed will be ignored but trigger a warning.
    case EGG_TID_metadata:
    case EGG_TID_code:
    case EGG_TID_strings:
    case EGG_TID_image:
    case EGG_TID_sound:
    case EGG_TID_song:
      return 0;
  }
  fprintf(stderr,"WARNING: Unexpected resource type %d (rid %d, c=%d)\n",tid,rid,c);
  return 0;
}

/* Init.
 */
 
int res_init() {
  if ((g.romc=egg_rom_get(0,0))<1) return -1;
  if (!(g.rom=malloc(g.romc))) return -1;
  if (egg_rom_get(g.rom,g.romc)!=g.romc) return -1;
  text_set_rom(g.rom,g.romc);
  g.resc=0;
  struct rom_reader reader;
  if (rom_reader_init(&reader,g.rom,g.romc)<0) return -1;
  struct rom_entry res;
  while (rom_reader_next(&res,&reader)>0) {
    int err=res_welcome(res.tid,res.rid,res.v,res.c);
    if (err<0) return err;
    if (!err) continue;
    if (g.resc>=g.resa) {
      int na=g.resa+128;
      if (na>INT_MAX/sizeof(struct rom_entry)) return -1;
      void *nv=realloc(g.resv,sizeof(struct rom_entry)*na);
      if (!nv) return -1;
      g.resv=nv;
      g.resa=na;
    }
    g.resv[g.resc++]=res;
  }
  //TODO postprocess resources
  if (maps_init()<0) return -1;
  return 0;
}

/* Search TOC.
 */
 
int res_search(int tid,int rid) {
  int lo=0,hi=g.resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rom_entry *res=g.resv+ck;
         if (tid<res->tid) hi=ck;
    else if (tid>res->tid) lo=ck+1;
    else if (rid<res->rid) hi=ck;
    else if (rid>res->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Get resource.
 */
 
int res_get(void *dstpp,int tid,int rid) {
  int p=res_search(tid,rid);
  if (p<0) return 0;
  const struct rom_entry *res=g.resv+p;
  *(const void**)dstpp=res->v;
  return res->c;
}

/* Get physics table from a tilesheet.
 */
 
const uint8_t *res_get_physics_table(int tilesheetid) {
  uint8_t *table=res.physicsv;
  const int *id=res.physicsidv;
  int i=res.physicsidc;
  for (;i-->0;id++,table+=256) {
    if (*id==tilesheetid) return table;
    if (*id>tilesheetid) break;
  }
  return physics_default;
}

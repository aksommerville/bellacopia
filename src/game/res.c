#include "game.h"

/* Welcome resource.
 * Return >0 to include in the TOC.
 */
 
static int res_welcome(int tid,int rid,const void *v,int c) {
  switch (tid) {
    case EGG_TID_map:
    case EGG_TID_sprite:
    case EGG_TID_tilesheet:
    case EGG_TID_decalsheet:
      return 1;
  }
  return 0;
}

/* Init.
 */
 
int res_init() {
  if ((g.romc=egg_rom_get(0,0))<1) return -1;
  if (!(g.rom=malloc(g.romc))) return -1;
  if (egg_rom_get(g.rom,g.romc)!=g.romc) return -1;
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

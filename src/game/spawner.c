#include "bellacopia.h"

/* If we have so many sprites spawned already, we won't spawn any more.
 * Everything we spawn gets added to (g.spawner.rsprites).
 * We don't do the removal from that group. Let camera kill them when far offscreen.
 */
#define RSPRITE_LIMIT 8

/* When so many rsprite are present at the start of an exposure cycle,
 * we'll try to cull the herd first.
 * Our culling is more aggressive than camera's.
 */
#define RSPRITE_CULL_START 4

/* Recalculate pending cell count randomly.
 */
 
static void spawnmap_recalculate_pending(struct spawnmap *spawnmap) {
  if (spawnmap->wsum<1) return;
  spawnmap->pending=(int)(255.0/(SPAWN_WEIGHT_MAX*spawnmap->wsum));
  if (spawnmap->pending<1) spawnmap->pending=1;
  spawnmap->pending+=rand()%spawnmap->pending; // Up to double the nominal wait.
}

/* Reset one map.
 */
 
static void spawnmap_reset(struct spawnmap *spawnmap,const struct map *map) {
  spawnmap->wsum=0;
  spawnmap->pending=0;
  if (!map) return;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_rsprite) {
      spawnmap->wsum+=cmd.arg[2];
    }
  }
  if (!spawnmap->wsum) return;
  spawnmap_recalculate_pending(spawnmap);
}

/* Reset.
 */
 
void spawner_reset() {
  sprite_group_clear(&g.spawner.rsprites); // Should happen by other means but we'll be sure of it.
  
  /* Rebuild (spawnmapv) if it doesn't match (g.mapstore.byidv).
   */
  if (g.spawner.spawnmapc!=g.mapstore.byidc) {
    g.spawner.spawnmapc=0;
    void *nv=realloc(g.spawner.spawnmapv,sizeof(struct spawnmap)*g.mapstore.byidc);
    if (!nv) return;
    g.spawner.spawnmapv=nv;
    g.spawner.spawnmapc=g.mapstore.byidc;
    struct spawnmap *spawnmap=g.spawner.spawnmapv;
    struct map **mapp=g.mapstore.byidv;
    int i=g.mapstore.byidc;
    for (;i-->0;spawnmap++,mapp++) {
      spawnmap_reset(spawnmap,*mapp);
    }
  }
}

/* Nonzero if there's a solid sprite overlapping (x,y).
 */
 
static int sprites_present_at_cell(int x,int y) {
  // Rather than accounting for each solid's hitbox, just assume we need a half meter on each side.
  double l=x-0.5;
  double r=x+1.5;
  double t=y-0.5;
  double b=y+1.5;
  struct sprite **p=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue;
    if (sprite->x<l) continue;
    if (sprite->x>r) continue;
    if (sprite->y<t) continue;
    if (sprite->y>b) continue;
    return 1;
  }
  return 0;
}

/* How many current rsprite have this arg?
 */
 
static int spawner_count_instances(const void *arg) {
  struct sprite **p=g.spawner.rsprites.sprv;
  int i=g.spawner.rsprites.sprc,c=0;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue;
    if (sprite->arg==arg) c++;
  }
  return c;
}

/* Having decided to spawn a sprite at a given point,
 * now we decide which sprite, and spawn it.
 * We didn't index rsprite commands. I feel it makes more sense to read them from scratch each spawn.
 * Returns the new effective (wsum) for (spawnmap), accounting for rsprites afield.
 */
 
static int spawner_spawn_1(struct spawnmap *spawnmap,const struct map *map,double x,double y) {
  if (spawnmap->wsum<1) return 0;
  
  /* Collect a list of available rsprite commands, respecting each's limit.
   */
  #define CANDIDATE_LIMIT 32
  uint8_t lastspawn=0; // bits (1<<p). Set if spawning this candidate would put it at the limit.
  struct cmdlist_entry candidatev[CANDIDATE_LIMIT];
  int candidatec=0;
  int wsum=0;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode!=CMD_map_rsprite) continue;
    if (!cmd.arg[2]) continue; // zero weight; ignore it.
    if (cmd.arg[3]) { // nonzero limit; check instances.
      int existc=spawner_count_instances(cmd.arg+4);
      if (existc>=cmd.arg[3]) continue;
      if (existc==cmd.arg[3]-1) lastspawn|=(1<<candidatec);
    }
    wsum+=cmd.arg[2];
    candidatev[candidatec++]=cmd;
    if (candidatec>=CANDIDATE_LIMIT) break;
  }
  #undef CANDIDATE_LIMIT
  if (wsum<1) return 0; // All rsprite commands are over their limit. Don't spawn anything.
  
  /* Pick one randomly according to weight.
   */
  int choice=rand()%wsum;
  struct cmdlist_entry *rcmd=candidatev;
  int i=0;
  for (;i<candidatec;i++,rcmd++) {
    choice-=rcmd->arg[2];
    if (choice>=0) continue;
    int rid=(rcmd->arg[0]<<8)|rcmd->arg[1];
    struct sprite *sprite=sprite_spawn(x,y,rid,rcmd->arg+4,rcmd->argc-4,0,0,0);
    if (!sprite) return wsum;
    sprite_group_add(&g.spawner.rsprites,sprite);
    //fprintf(stderr,"spawner spawn %p(%s) @%f,%f\n",sprite,sprite->type->name,sprite->x,sprite->y);
    if (lastspawn&(1<<i)) wsum-=rcmd->arg[2];
    return wsum;
  }
  return wsum;
}

/* Cell exposure within one map.
 * Caller confirms (wsum>0).
 * (x,y) are relative to the map, not the plane. OOB is allowed, we clip here.
 */
 
static void spawner_expose_1(struct spawnmap *spawnmap,const struct map *map,int x,int y,int w,int h) {
  if (spawnmap->wsum<1) return; // Shouldn't have called. But consequences would be disastrous if they do, so check.

  // Clip.
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>NS_sys_mapw-w) w=NS_sys_mapw-x;
  if (y>NS_sys_maph-h) h=NS_sys_maph-y;
  if ((w<1)||(h<1)) return;
  
  // List candidate cells. Vacant physics and no nearby solids.
  #define CANDIDATE_LIMIT 25
  struct candidate {
    int x,y; // plane meters
  } candidatev[CANDIDATE_LIMIT];
  int candidatec=0;
  int cky=map->lat*NS_sys_maph+y;
  const uint8_t *rowp=map->v+y*NS_sys_mapw+x;
  int yi=h;
  for (;yi-->0;rowp+=NS_sys_mapw,cky++) {
    int ckx=map->lng*NS_sys_mapw+x;
    const uint8_t *cellp=rowp;
    int xi=w;
    for (;xi-->0;cellp++,ckx++) {
      uint8_t physics=map->physics[*cellp];
      if (physics!=NS_physics_vacant) continue;
      if (sprites_present_at_cell(ckx,cky)) continue;
      struct candidate *candidate=candidatev+candidatec++;
      candidate->x=ckx;
      candidate->y=cky;
      if (candidatec>=CANDIDATE_LIMIT) goto _done_search_;
    }
  }
 _done_search_:;
  if (candidatec<1) return;
  #undef CANDIDATE_LIMIT
  
  /* Drop pending by the candidate count.
   * If it crosses zero, we spawn something.
   * Capture the remainders, and we might spawn more than once per call.
   */
  spawnmap->pending-=candidatec;
  while ((spawnmap->pending<=0)&&(candidatec>0)) {
    int candidatep=rand()%candidatec;
    int wsum=spawner_spawn_1(spawnmap,map,candidatev[candidatep].x+0.5,candidatev[candidatep].y+0.5);
    candidatec--;
    memmove(candidatev+candidatep,candidatev+candidatep+1,sizeof(struct candidate)*(candidatec-candidatep));
    // Advance spawnmap->pending according to the effective (wsum) returned by spawner_spawn_1.
    if (wsum>0) {
      int addc=(int)(255.0/(SPAWN_WEIGHT_MAX*wsum));
      if (addc<1) addc=1;
      spawnmap->pending+=addc+rand()%addc; // Up to double the nominal wait.
    } else {
      spawnmap->pending+=spawnmap->wsum<<1;
    }
  }
}

/* Kill any rsprite well outside of the camera's view.
 * Camera does this on its own, but it is not as aggressive as we'd like.
 * Beware that all rsprite are oob initially, just a little.
 * So we'll cull at a short but nonzero distance. Say 3 meters.
 * Cell exposures will definitely not happen during sprite updates, so it's ok to kill them directly instead of deathrow.
 */
 
static void spawner_drop_oobs() {
  const double margin=3.0;
  double l=(double)g.camera.rx/(double)NS_sys_tilesize-margin;
  double r=(double)(g.camera.rx+FBW)/(double)NS_sys_tilesize+margin;
  double t=(double)g.camera.ry/(double)NS_sys_tilesize-margin;
  double b=(double)(g.camera.ry+FBH)/(double)NS_sys_tilesize+margin;
  int i=g.spawner.rsprites.sprc;
  while (i-->0) {
    struct sprite *sprite=g.spawner.rsprites.sprv[i];
    // Include the sprite's hitbox, in case it's big.
    if (
      (sprite->x+sprite->hbr<l)||
      (sprite->x+sprite->hbl>r)||
      (sprite->y+sprite->hbb<t)||
      (sprite->y+sprite->hbt>b)
    ) {
      //fprintf(stderr,"spawner cull %p(%s) @%f,%f\n",sprite,sprite->type->name,sprite->x,sprite->y);
      sprite_kill(sprite);
    }
  }
}

/* Cell exposure: Possible spawn.
 */
 
void spawner_expose(int x,int y,int w,int h) {

  // Too many afield, no worries, we're not making more.
  if (g.spawner.rsprites.sprc>RSPRITE_CULL_START) {
    if (!g.telescoping) spawner_drop_oobs();
    if (g.spawner.rsprites.sprc>=RSPRITE_LIMIT) return;
  }
  
  // Acquire the plane and clip to it. Fully OOB is entirely possible.
  const struct plane *plane=plane_by_position(g.camera.z);
  if (!plane) return;
  int planew=plane->w*NS_sys_mapw;
  int planeh=plane->h*NS_sys_maph;
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>planew-w) w=planew-x;
  if (y>planeh-h) h=planeh-y;
  if ((w<1)||(h<1)) return;
  
  // Operate piecemeal on the covered maps. There's usually at least two at a time.
  int cola=x/NS_sys_mapw;
  int colz=(x+w-1)/NS_sys_mapw;
  int rowa=y/NS_sys_maph;
  int rowz=(y+h-1)/NS_sys_maph;
  int row=rowa; for (;row<=rowz;row++) {
    const struct map *map=plane->v+row*plane->w+cola;
    int col=cola; for (;col<=colz;col++,map++) {
      if ((map->rid<1)||(map->rid>=g.spawner.spawnmapc)) continue;
      struct spawnmap *spawnmap=g.spawner.spawnmapv+map->rid;
      if (!spawnmap->wsum) continue;
      int subx=x-map->lng*NS_sys_mapw;
      int suby=y-map->lat*NS_sys_maph;
      spawner_expose_1(spawnmap,map,subx,suby,w,h);
    }
  }
}

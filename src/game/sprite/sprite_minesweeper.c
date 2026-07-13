/* sprite_minesweeper.c
 * Not a real sprite.
 * Our purpose is to mark and control the diegetic minesweeper minigame.
 *
 * How the game works:
 *  - Field must exist on one map and must be a rectangle of initially all the same tile.
 *  - That tile must have 10 related tiles immediately after it. (concealed,zero,one,...,eight,concealed_marker)
 *  - Place the sprite at the top-left corner of the field.
 *  - We'll initially expose the entire column or row for the side the hero is on.
 *  - - This is mostly a hint to new players, give them some idea what they're getting into.
 *  - Player exposes a cell by stepping on it.
 *  - If she steps on a mine, place a fire there and let it hurt her. The game is then poisoned, you're not allowed to win.
 *  - If a flag was provided, you only need to solve the puzzle fully once.
 *
 * If your goal is just to get across this room, that's one thing.
 * But if you expose every safe cell, an extra bonus might happen.
 */

#include "game/bellacopia.h"

struct sprite_minesweeper {
  struct sprite hdr;
  int fldid;
  int started; // Flips nonzero permanently when the hero first crosses our fence. When we expose the near edge.
  int invalid; // If the player exposes a mine, she's not allowed to win. Mines exposed by the initial exposure don't count.
  int solved; // Stop doing things once she solves us.
  int qx,qy; // Quantized position of hero in map meters.
  int store_listener;
  
  /* Bounds of the play field in map meters.
   * NB not plane meters.
   */
  struct { int x,y,w,h; } fld;
  struct map *map;
  uint8_t tileid; // The base tile. Concealed. +1 for revealed with 0 neighbors, +2 for 1 neighbor, up to 8 neighbors. Then the mine marker.
  
  /* Bounds of my screen, in plane meters.
   * For checking whether we're actually in play yet.
   * Hero must enter this box before we do anything real.
   */
  struct { double l,r,t,b; } fence;
  
  /* If we create any bonfires, we must destroy them when we go out of scope.
   * Otherwise, we might reset but the last instance's fires linger.
   */
  struct sprite_group dependent_fires;
};

#define SPRITE ((struct sprite_minesweeper*)sprite)

static int _minesweeper_init(struct sprite *sprite);

/* Cleanup.
 */
 
static void _minesweeper_del(struct sprite *sprite) {
  store_unlisten(SPRITE->store_listener);
  sprite_group_cleanup(&SPRITE->dependent_fires,1);
}

/* Signal via store.
 */
 
static void minesweeper_cb_signal(char s,int id,int value,void *userdata) {
  struct sprite *sprite=userdata;
  if (id==NS_signal_reset_puzzle) {
    sprite_group_cleanup(&SPRITE->dependent_fires,1);
    sprite_group_add(GRP(update),sprite); // We remove ourselves at init if the puzzle is complete -- must restore here.
    _minesweeper_init(sprite);
  }
}

/* Init.
 * We can get retriggered, on manual resets.
 */
 
static int _minesweeper_init(struct sprite *sprite) {
  SPRITE->fldid=(sprite->arg[0]<<8)|sprite->arg[1];
  SPRITE->qx=-1;
  SPRITE->qy=-1;
  SPRITE->started=0;
  SPRITE->invalid=0;
  SPRITE->solved=0;

  /* First find my bearings.
   */
  struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  int x0=(int)sprite->x-map->lng*NS_sys_mapw;
  int y0=(int)sprite->y-map->lat*NS_sys_maph;
  if ((x0<0)||(y0<0)||(x0>=NS_sys_mapw)||(y0>=NS_sys_maph)) return -1;
  
  /* With the map known, we can define our fence.
   * It's just the bounds of the map.
   */
  SPRITE->fence.l=NS_sys_mapw*map->lng;
  SPRITE->fence.r=NS_sys_mapw*(map->lng+1);
  SPRITE->fence.t=NS_sys_maph*map->lat;
  SPRITE->fence.b=NS_sys_maph*(map->lat+1);
  
  /* Locate my bounds in the map.
   * Use (rov), not (v). A previous instance of me might have adulterated (v).
   */
  SPRITE->map=map;
  SPRITE->fld.x=x0;
  SPRITE->fld.y=y0;
  SPRITE->fld.w=1;
  SPRITE->fld.h=1;
  const uint8_t *rowp=map->rov+y0*NS_sys_mapw;
  sprite->tileid=rowp[x0];
  while ((SPRITE->fld.x+SPRITE->fld.w<NS_sys_mapw)&&(rowp[x0+SPRITE->fld.w]==sprite->tileid)) SPRITE->fld.w++;
  rowp+=NS_sys_mapw;
  while (SPRITE->fld.y+SPRITE->fld.h<NS_sys_maph) {
    int match=1,i=SPRITE->fld.w;
    while (i-->0) {
      if (rowp[x0+i]!=sprite->tileid) {
        match=0;
        break;
      }
    }
    if (!match) break;
    SPRITE->fld.h++;
    rowp+=NS_sys_mapw;
  }
  
  // On the real init, start listening for the reset signal.
  if (!SPRITE->store_listener) {
    SPRITE->store_listener=store_listen('s',minesweeper_cb_signal,sprite);
  }
  
  /* Being a static sprite, we are instantiated the moment our map comes into view.
   * So it's ok to overwrite the whole thing.
   * Start by concealing everything.
   * Or if we have a flag and it's set, reveal everything and leave no mines.
   */
  g.camera.mapsdirty=1; // Probly redundant, if we're spawning just now it hasn't rendered yet. But be correct about it.
  uint8_t *dstp=map->v+y0*NS_sys_mapw+x0;
  int i=SPRITE->fld.h;
  if (store_get_fld(SPRITE->fldid)) {
    for (;i-->0;dstp+=NS_sys_mapw) memset(dstp,sprite->tileid+1,SPRITE->fld.w);
    sprite_group_remove(GRP(update),sprite); // neutralize self
    return 0;
  } else {
    for (;i-->0;dstp+=NS_sys_mapw) memset(dstp,sprite->tileid,SPRITE->fld.w);
  }
  
  /* Place mines randomly.
   * It's OK to use the column or row nearest the hero, the one we'll expose.
   * As a rough guide, let's use the shorter of my dimensions for mine count.
   */
  int minec=(SPRITE->fld.w>SPRITE->fld.h)?SPRITE->fld.h:SPRITE->fld.w;
  while (minec-->0) {
    int panic=100;
    while (panic-->0) {
      int subx=rand()%SPRITE->fld.w;
      int suby=rand()%SPRITE->fld.h;
      uint8_t *p=map->v+(SPRITE->fld.y+suby)*NS_sys_mapw+SPRITE->fld.x+subx;
      if (*p==sprite->tileid) {
        *p=sprite->tileid+10;
        break;
      }
    }
  }
  
  /* Don't worry about exposing the near edge yet.
   * First off, it's probably not near enough yet.
   * And second, it's better to do that all during update, it's more generic that way.
   */

  return 0;
}

/* A mine cell was exposed! Blow it up!
 */
 
static void minesweeper_spawn_fire(struct sprite *sprite,int x,int y) {
  double sx=SPRITE->map->lng*NS_sys_mapw+x+0.5;
  double sy=SPRITE->map->lat*NS_sys_maph+y+0.5;
  struct sprite *bonfire=sprite_spawn(sx,sy,RID_sprite_bonfire,0,0,0,0,0);
  if (!bonfire) return;
  sprite_group_add(&SPRITE->dependent_fires,bonfire);
}

/* Count neighbor mines. (x,y) in map meters.
 * Only returns in 0..8.
 */
 
static int minesweeper_count_neighbors(struct sprite *sprite,int x,int y) {
  int c=0;
  int dx=-1; for (;dx<=1;dx++) {
    int subx=x+dx;
    if ((subx<SPRITE->fld.x)||(subx>=SPRITE->fld.x+SPRITE->fld.w)) continue;
    int dy=-1; for (;dy<=1;dy++) {
      if (!dx&&!dy) continue;
      int suby=y+dy;
      if ((suby<0)||(suby>=SPRITE->fld.y+SPRITE->fld.h)) continue;
      uint8_t tileid=SPRITE->map->v[suby*NS_sys_mapw+subx];
      if (tileid==sprite->tileid+10) c++;
    }
  }
  return c;
}

/* Expose one cell, expressed in map meters.
 * Returns the count of exposed neighbors, -1 if exposed a mine, or -2 if OOB or already exposed.
 * If a mine is freshly exposed, we create the fire sprite for it.
 * No sound effects.
 */
 
static int minesweeper_expose(struct sprite *sprite,int x,int y) {
  if ((x<SPRITE->fld.x)||(y<SPRITE->fld.y)||(x>=SPRITE->fld.x+SPRITE->fld.w)||(y>=SPRITE->fld.y+SPRITE->fld.h)) return -2;
  uint8_t *v=SPRITE->map->v+y*NS_sys_mapw+x;
  if (*v==sprite->tileid+10) { // Mine.
    minesweeper_spawn_fire(sprite,x,y);
    SPRITE->invalid=1;
    return -1;
  } else if (*v==sprite->tileid) { // Vacant.
    int neic=minesweeper_count_neighbors(sprite,x,y);
    *v=sprite->tileid+1+neic;
    g.camera.mapsdirty=1;
    return neic;
  } else {
    return -2;
  }
  return 0;
}

/* For a hero at (x,y) in plane meters, decide which of my edges is nearest and expose that whole edge.
 */
 
static void minesweeper_expose_near_edge(struct sprite *sprite,double herox,double heroy) {

  /* Use the field rather than the fence.
   * Fence would suffice if we only ran on the hero entering our room, but we do also trigger on manual resets.
   */
  int x=(int)herox-SPRITE->map->lng*NS_sys_mapw;
  int y=(int)heroy-SPRITE->map->lat*NS_sys_maph;
  int dl=x-SPRITE->fld.x;
  int dr=x-(SPRITE->fld.x+SPRITE->fld.w-1);
  int dt=y-SPRITE->fld.y;
  int db=y-(SPRITE->fld.y+SPRITE->fld.h-1);
  
  #define EXPOSE(_x,_y,dx,dy) { \
    int x=_x,y=_y; \
    if (x) x=SPRITE->fld.w-1; \
    if (y) y=SPRITE->fld.h-1; \
    for (;;) { \
      minesweeper_expose(sprite,SPRITE->fld.x+x,SPRITE->fld.y+y); \
      x+=dx; \
      y+=dy; \
      if ((x<0)||(y<0)||(x>=SPRITE->fld.w)||(y>=SPRITE->fld.h)) break; \
    } \
    SPRITE->invalid=0; /* We might have exposed a mine. It doesn't count; you can still solve the puzzle and win. */ \
    return; \
  }
  
  /* First, if we're on the far side of just one edge, that's the answer.
   * This is by far the likeliest case.
   */
  if ((dt>=0)&&(db<0)) {
    if (dl<0) EXPOSE(0,0,0,1)
    if (dr>=0) EXPOSE(1,0,0,1)
  }
  if ((dl>=0)&&(dr<0)) {
    if (dt<0) EXPOSE(0,0,1,0)
    if (db>=0) EXPOSE(0,1,1,0)
  }
  
  /* We're inside or diagonal.
   * Use whichever edge is closest.
   */
  int adl=(dl<0)?-dl:dl;
  int adr=(dr<0)?-dr:dr;
  int adt=(dt<0)?-dt:dt;
  int adb=(db<0)?-db:db;
  if ((adl<=adr)&&(adl<=adt)&&(adl<=adb)) EXPOSE(0,0,0,1)
  if ((adr<=adt)&&(adr<=adb)) EXPOSE(1,0,0,1)
  if (adt<=adb) EXPOSE(0,0,1,0)
  EXPOSE(0,1,1,0)
  #undef EXPOSE
}

/* Generate a little toast sprite above the hero that shows this number.
 * Super important! When you expose a cell, you're standing on top of its neighbor count.
 */
 
static void minesweeper_toast_exposure(struct sprite *sprite,struct sprite *hero,int c) {
  struct sprite *toast=sprite_spawn(hero->x,hero->y-0.5,0,0,0,&sprite_type_toast,0,0);
  if (!toast) return;
  if (c<0) c=0; else if (c>9) c=9;
  char ch='0'+c;
  sprite_toast_set_text(toast,&ch,1);
}

/* Nonzero if every non-mine cell is exposed, and we aren't explicitly invalid.
 */
 
static int minesweeper_check_completion(struct sprite *sprite) {
  if (SPRITE->invalid) return 0;
  const uint8_t *rowp=SPRITE->map->v+SPRITE->fld.y*NS_sys_mapw+SPRITE->fld.x;
  int yi=SPRITE->fld.h;
  for (;yi-->0;rowp+=NS_sys_mapw) {
    const uint8_t *p=rowp;
    int xi=SPRITE->fld.w;
    for (;xi-->0;p++) {
      if (*p==sprite->tileid) return 0;
    }
  }
  return 1;
}

/* Update.
 */
 
static void _minesweeper_update(struct sprite *sprite,double elapsed) {
  if (SPRITE->solved) return;

  /* Find the hero.
   * No hero, no worries, just get out.
   * We don't care whether it's an actual hero or a racer (or anything else we might add in the future).
   */
  if (GRP(hero)->sprc<1) { SPRITE->qx=SPRITE->qy=-1; return; }
  struct sprite *hero=GRP(hero)->sprv[0];
  
  /* If she's flying, she just doesn't exist for us.
   */
  if (!sprite_hero_is_grounded(hero)) { SPRITE->qx=SPRITE->qy=-1; return; }
  
  /* If we haven't started yet, check her against the fence.
   */
  if (!SPRITE->started) {
    if (hero->x<SPRITE->fence.l) { SPRITE->qx=SPRITE->qy=-1; return; }
    if (hero->x>SPRITE->fence.r) { SPRITE->qx=SPRITE->qy=-1; return; }
    if (hero->y<SPRITE->fence.t) { SPRITE->qx=SPRITE->qy=-1; return; }
    if (hero->y>SPRITE->fence.b) { SPRITE->qx=SPRITE->qy=-1; return; }
    minesweeper_expose_near_edge(sprite,hero->x,hero->y);
    SPRITE->started=1;
  }
  
  /* Take the hero's quantized position and stop doing things if it didn't change.
   * Likewise, if the new quantized position is outside our field, we're done.
   */
  int nqx=(int)hero->x-SPRITE->map->lng*NS_sys_mapw;
  int nqy=(int)hero->y-SPRITE->map->lat*NS_sys_maph;
  if ((nqx==SPRITE->qx)&&(nqy==SPRITE->qy)) return;
  SPRITE->qx=nqx;
  SPRITE->qy=nqy;
  if ((nqx<SPRITE->fld.x)||(nqy<SPRITE->fld.y)||(nqx>=SPRITE->fld.x+SPRITE->fld.w)||(nqy>=SPRITE->fld.y+SPRITE->fld.h)) return;
  
  /* Expose that cell.
   */
  int result=minesweeper_expose(sprite,nqx,nqy);
  if (result==-1) { // Mine!
    bm_sound(RID_sound_bombblow);
  } else if ((result>=0)&&(result<=8)) {
    // Handle zero separate if we want cascading, but our fields being so small, I think it's better without cascade.
    if (minesweeper_check_completion(sprite)) {
      SPRITE->solved=1;
      bm_sound(RID_sound_secret);
      if (SPRITE->fldid) {
        store_set_fld(SPRITE->fldid,1);
        g.camera.mapsdirty=1;
      }
    } else {
      bm_sound(RID_sound_treadle);
      minesweeper_toast_exposure(sprite,hero,result);
    }
  } else {
    // Already exposed or whatever. No worries.
  }
}

/* Dummy render, just to ensure nobody tries to render us.
 */
 
static void _minesweeper_render(struct sprite *sprite,int x,int y) {
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_minesweeper={
  .name="minesweeper",
  .objlen=sizeof(struct sprite_minesweeper),
  .del=_minesweeper_del,
  .init=_minesweeper_init,
  .update=_minesweeper_update,
  .render=_minesweeper_render,
};

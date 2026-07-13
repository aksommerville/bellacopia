/* sprite_trickfloor.c
 * A grid of uniform tiles with a randomly-generated path across.
 * You must employ the compass, divining rod, or magnifier (ie hints_override) to get across.
 * Or fly over it on the broom. :)
 *
 * We aim to be fully self-configuring.
 * Place sprite at the top-left corner of the grid. No args.
 * The puzzle can't be completed; you have to solve it any time you want to pass.
 */
 
#include "game/bellacopia.h"

#define TRICKFLOOR_SIZE_LIMIT 12 /* Per axis. */

struct sprite_trickfloor {
  struct sprite hdr;
  struct sprite_group flames;
  int x,y,w,h; // My bounds in plane meters. Verified at init to sit within one map.
  int present; // Nonzero if the hero is in our bounds and we have a path calculated.
  int qx,qy; // Hero's most recent quantized position in plane meters.
  int horizontal; // Nonzero if our paths run left<~>right, otherwise top<~>bottom.
  struct step { uint8_t x,y; } stepv[TRICKFLOOR_SIZE_LIMIT*TRICKFLOOR_SIZE_LIMIT]; // Allocates too much, could be smaller but harder to define. Relative to my bounds.
  int stepc;
  int stepp; // Volatile and may be OOB. Hero's position in (stepv).
};

#define SPRITE  ((struct sprite_trickfloor*)sprite)

/* Cleanup.
 */
 
static void _trickfloor_del(struct sprite *sprite) {
  sprite_group_cleanup(&SPRITE->flames,1);
}

/* Is this physics value kosher for my doormat?
 */
 
static int trickfloor_valid_doormat(uint8_t physics) {
  switch (physics) {
    case NS_physics_vacant:
    case NS_physics_safe:
    case NS_physics_ice:
      return 1;
  }
  return 0;
}

/* Init.
 */
 
static int _trickfloor_init(struct sprite *sprite) {
  
  /* Determine my bounds in the map.
   */
  struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  SPRITE->x=(int)sprite->x;
  SPRITE->y=(int)sprite->y;
  int subx=SPRITE->x-map->lng*NS_sys_mapw;
  int suby=SPRITE->y-map->lat*NS_sys_maph;
  if ((subx<0)||(suby<0)||(subx>=NS_sys_mapw)||(suby>=NS_sys_maph)) return -1;
  const uint8_t *row=map->v+suby*NS_sys_mapw+subx;
  uint8_t tileid=*row;
  SPRITE->w=1;
  while ((subx+SPRITE->w<NS_sys_mapw)&&(row[SPRITE->w]==tileid)) SPRITE->w++;
  SPRITE->h=1;
  row+=NS_sys_mapw;
  while (suby+SPRITE->h<NS_sys_maph) {
    if (*row!=tileid) break; // Don't bother checking the whole row. Just the left column, and take the rest on faith.
    row+=NS_sys_mapw;
    SPRITE->h++;
  }
  if ((SPRITE->w>TRICKFLOOR_SIZE_LIMIT)||(SPRITE->h>TRICKFLOOR_SIZE_LIMIT)) {
    fprintf(stderr,"trickfloor too big (%dx%d), limit %dx%d\n",SPRITE->w,SPRITE->h,TRICKFLOOR_SIZE_LIMIT,TRICKFLOOR_SIZE_LIMIT);
    return -1;
  }
  
  /* Determine orientation.
   * We must have two opposite edges entirely clear (vacant, safe, ice) on their outside.
   * When making maps, ensure there are solid walls on the other sides. We won't bother validating that.
   */
  int lopen=1,ropen=1,topen=1,bopen=1,i;
  if (subx<=0) lopen=0; else {
    for (i=SPRITE->h;i-->0;) {
      if (!trickfloor_valid_doormat(map->physics[map->v[(suby+i)*NS_sys_mapw+subx-1]])) {
        lopen=0;
        break;
      }
    }
  }
  if (suby<=0) topen=0; else {
    for (i=SPRITE->w;i-->0;) {
      if (!trickfloor_valid_doormat(map->physics[map->v[(suby-1)*NS_sys_mapw+subx+i]])) {
        topen=0;
        break;
      }
    }
  }
  if (subx+SPRITE->w>=NS_sys_mapw) ropen=0; else {
    for (i=SPRITE->h;i-->0;) {
      if (!trickfloor_valid_doormat(map->physics[map->v[(suby+i)*NS_sys_mapw+subx+SPRITE->w]])) {
        ropen=0;
        break;
      }
    }
  }
  if (suby+SPRITE->h>=NS_sys_maph) bopen=0; else {
    for (i=SPRITE->w;i-->0;) {
      if (!trickfloor_valid_doormat(map->physics[map->v[(suby+SPRITE->h)*NS_sys_mapw+subx+i]])) {
        bopen=0;
        break;
      }
    }
  }
  if (lopen&&ropen) {
    SPRITE->horizontal=1;
  } else if (topen&&bopen) {
    SPRITE->horizontal=0;
  } else {
    fprintf(stderr,"trickfloor requires two opposite edges with full doormats\n");
    return -1;
  }
  
  // (present) is initially zero, and (q) 0,0. Should work. Generating the path happens on demand when we find the hero.
  SPRITE->stepp=-1;
  
  return 0;
}

/* Generate a fresh path.
 * (qx,qy) must be a valid position on one of my edges.
 */
 
static void trickfloor_generate_path(struct sprite *sprite) {

  /* Take the relative position and confirm it's on one of our doormat sides.
   */
  int rx=SPRITE->qx-SPRITE->x;
  int ry=SPRITE->qy-SPRITE->y;
  int forelen,sidelen; // My size on the axis of travel and the other one, respectively.
  int dx=0,dy=0; // Forward normal.
  if (SPRITE->horizontal) {
    if ((rx!=0)&&(rx!=SPRITE->w-1)) return;
    if ((ry<0)||(ry>=SPRITE->h)) return;
    forelen=SPRITE->w;
    sidelen=SPRITE->h;
    dx=rx?-1:1;
  } else {
    if ((ry!=0)&&(ry!=SPRITE->h-1)) return;
    if ((rx<0)||(rx>=SPRITE->w)) return;
    forelen=SPRITE->h;
    sidelen=SPRITE->w;
    dy=ry?-1:1;
  }
  
  /* Turn the plan into steps.
   * First step is where she's at right now, and the second step is always foreward of that.
   */
  SPRITE->stepp=0;
  SPRITE->stepc=0;
  SPRITE->stepv[SPRITE->stepc++]=(struct step){rx,ry};
  for (;;) {
    // Move forward and plant a step.
    rx+=dx;
    ry+=dy;
    if ((rx<0)||(ry<0)||(rx>=SPRITE->w)||(ry>=SPRITE->h)) break;
    SPRITE->stepv[SPRITE->stepc++]=(struct step){rx,ry};
    // Pick a random lateral position and emit steps until we reach it.
    int dstx=rx,dsty=ry;
    if (SPRITE->horizontal) dsty=rand()%SPRITE->h; else dstx=rand()%SPRITE->w;
    for (;;) {
           if (dstx<rx) rx--;
      else if (dstx>rx) rx++;
      else if (dsty<ry) ry--;
      else if (dsty>ry) ry++;
      else break;
      SPRITE->stepv[SPRITE->stepc++]=(struct step){rx,ry};
    }
    // One more step forward.
    rx+=dx;
    ry+=dy;
    if ((rx<0)||(ry<0)||(rx>=SPRITE->w)||(ry>=SPRITE->h)) break;
    SPRITE->stepv[SPRITE->stepc++]=(struct step){rx,ry};
  }
}

/* Spawn a bonfire.
 */
 
static void trickfloor_burn(struct sprite *sprite,int rx,int ry) {
  double x=SPRITE->x+rx+0.5;
  double y=SPRITE->y+ry+0.5;
  struct sprite *bonfire=sprite_spawn(x,y,RID_sprite_bonfire,0,0,0,0,0);
  if (!bonfire) return;
  sprite_group_add(&SPRITE->flames,bonfire);
}

/* Hero moved.
 * If (qx,qy) is off-path, generate a bonfire.
 */
 
static void trickfloor_check_step(struct sprite *sprite) {
  int rx=SPRITE->qx-SPRITE->x;
  int ry=SPRITE->qy-SPRITE->y;
  if ((rx<0)||(ry<0)||(rx>=SPRITE->w)||(ry>=SPRITE->h)) return; // Shouldn't have called us!
  SPRITE->stepp=-1;
  int i=0;
  const struct step *step=SPRITE->stepv;
  for (;i<SPRITE->stepc;i++,step++) {
    if ((rx==step->x)&&(ry==step->y)) {
      SPRITE->stepp=i;
      break;
    }
  }
  if (SPRITE->stepp<0) {
    trickfloor_burn(sprite,rx,ry);
  }
}

/* Update.
 */
 
static void _trickfloor_update(struct sprite *sprite,double elapsed) {

  /* Find the hero, or if there isn't one, use 0,0.
   * Do find her even when flying -- flying nullifies our activity but it doesn't reset the path.
   * TODO How to manage flying? Punting it for now, the puzzle will affect flying witches same as walking ones.
   */
  struct sprite *hero=0;
  int nqx=0,nqy=0;
  if (GRP(hero)->sprc>=1) {
    hero=GRP(hero)->sprv[0];
    nqx=(int)hero->x;
    nqy=(int)hero->y;
  }
  
  /* No change to quantized hero position? Great, we're done.
   */
  if ((nqx==SPRITE->qx)&&(nqy==SPRITE->qy)) return;
  SPRITE->qx=nqx;
  SPRITE->qy=nqy;
  
  /* Check against our bounds.
   *   Out=>Out: Noop.
   *   Out=>In: Generate the path, with (qx,qy) safe.
   *   In=>In: Check new position for hazards.
   *   In=>Out: Drop flames and path.
   */
  int npresent=((SPRITE->qx>=SPRITE->x)&&(SPRITE->qy>=SPRITE->y)&&(SPRITE->qx<SPRITE->x+SPRITE->w)&&(SPRITE->qy<SPRITE->y+SPRITE->h));
  if (!npresent) {
    if (!SPRITE->present) return; // Out=>Out
    // In=>Out:
    if (hero) sprite_hero_drop_compass(hero);
    SPRITE->present=0;
    sprite_group_cleanup(&SPRITE->flames,1);
    SPRITE->stepc=0;
    SPRITE->stepp=-1;
    return;
  }
  if (hero) sprite_hero_drop_compass(hero); // Compass is usually refreshed at long intervals like changing planes. We need to do it often.
  if (SPRITE->present) { // In=>In
    trickfloor_check_step(sprite);
  } else { // Out=>In
    SPRITE->present=1;
    trickfloor_generate_path(sprite);
  }
}

/* Render.
 * We do render! When the hero is in bounds, we draw a highlight box around its quantized position.
 * The (x,y) provided here are the center of our top-left cell, which is pretty convenient.
 */
 
static void _trickfloor_render(struct sprite *sprite,int x,int y) {
  if (!SPRITE->present) return; // Only draw when on the floor.
  x+=(SPRITE->qx-SPRITE->x)*NS_sys_tilesize;
  y+=(SPRITE->qy-SPRITE->y)*NS_sys_tilesize;
  uint8_t xform=0;
  switch ((g.framec/10)&3) {
    case 1: xform=EGG_XFORM_SWAP|EGG_XFORM_XREV; break;
    case 2: xform=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
    case 3: xform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
  }
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,xform);
}

/* Type definition.
 */

const struct sprite_type sprite_type_trickfloor={
  .name="trickfloor",
  .objlen=sizeof(struct sprite_trickfloor),
  .del=_trickfloor_del,
  .init=_trickfloor_init,
  .update=_trickfloor_update,
  .render=_trickfloor_render,
};

/* Hints override query.
 */

int sprite_trickfloor_hint(int *x,int *y,const struct sprite *sprite,int fld) {
  if (!sprite||(sprite->type!=&sprite_type_trickfloor)) return -1;
  if (fld&&(fld!=NS_fld_alsozero)) return -1; // We can only be field zero.
  if ((SPRITE->stepp<0)||(SPRITE->stepp>=SPRITE->stepc)) return -1; // Not currently on the path.
  const struct step *step=SPRITE->stepv+SPRITE->stepp;
  
  // The very last step is a special case. Repeat the last delta.
  if (SPRITE->stepp>=SPRITE->stepc-1) {
    if (!SPRITE->stepp) return -1; // wait, what? Single-point path shouldn't be possible. Don't touch it.
    int dx=step->x-step[-1].x;
    int dy=step->y-step[-1].y;
    *x=SPRITE->x+step->x+dx;
    *y=SPRITE->y+step->y+dy;
    return 0;
  }
  
  // Everything else, report the position of the next step.
  step++;
  *x=SPRITE->x+step->x;
  *y=SPRITE->y+step->y;
  return 0;
}

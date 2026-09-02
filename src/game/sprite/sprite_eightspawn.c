/* sprite_eightspawn.c
 * A short-lived decoration spawned by figureeight, which locates an ok place, 
 * does some sparkle effect toward it, and spawns an eight there.
 * The decision of whether to start the process belongs to figureeight. We have a veto, and also we decide the location.
 */
 
#include "game/bellacopia.h"

struct sprite_eightspawn {
  struct sprite hdr;
  double dstx,dsty;
  double dx,dy; // m/s
};

#define SPRITE ((struct sprite_eightspawn*)sprite)

/* Confirm one cell is valid for spawning at, by physics and position only.
 */
 
static int eightspawn_valid_destination(const struct plane *plane,int x,int y) {
  if ((x<0)||(y<0)||(x>=plane->w*NS_sys_mapw)||(y>=plane->h*NS_sys_maph)) return 0;
  int mx=x/NS_sys_mapw;
  int my=y/NS_sys_maph;
  const struct map *map=plane->v+my*plane->w+mx;
  int subx=x%NS_sys_mapw;
  int suby=y%NS_sys_maph;
  uint8_t tileid=map->v[suby*NS_sys_mapw+subx];
  switch (map->physics[tileid]) {
    case NS_physics_vacant:
      return 1;
  }
  return 0;
}

/* Confirm a candidate position is valid for spawning, by checking solid sprites.
 * This doesn't ensure that it will be valid when we get there, but it's a good hint.
 * Because of that uncertainty, we require a wider than expected radius.
 */
 
static int eightspawn_valid_sprite_position(double x,double y) {
  struct sprite **p=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    double dx=sprite->x-x;
    double dy=sprite->y-y;
    double d2=dx*dx+dy*dy;
    if (d2<2.0) return 0;
  }
  return 1;
}

/* Init.
 */
 
static int _eightspawn_init(struct sprite *sprite) {
  
  /* Radiate squares outward to build up a set of candidate cells.
   * Only NS_physics_vacant is allowed.
   * Stop when we reach some sensible radius, or the set fills up.
   * We're not excluding cells due to solid sprites at this point -- that will come later because it's expensive.
   */
  #define CANDIDATE_LIMIT 20
  #define RADIUS_LIMIT 7
  const struct plane *plane=plane_by_position(sprite->z);
  if (!plane) return -1;
  int x0=(int)sprite->x;
  int y0=(int)sprite->y;
  struct candidate { int x,y; } candidatev[CANDIDATE_LIMIT];
  int candidatec=0;
  int radius=1;
  for (;radius<=RADIUS_LIMIT;radius++) {
    int xa=x0-radius;
    int xz=x0+radius;
    int ya=y0-radius;
    int yz=y0+radius;
    int x=xa; for (;x<=xz;x++) {
      if (eightspawn_valid_destination(plane,x,ya)) {
        if (candidatec>=CANDIDATE_LIMIT) goto _done_finding_candidates_;
        candidatev[candidatec++]=(struct candidate){x,ya};
      }
      if (eightspawn_valid_destination(plane,x,yz)) {
        if (candidatec>=CANDIDATE_LIMIT) goto _done_finding_candidates_;
        candidatev[candidatec++]=(struct candidate){x,yz};
      }
    }
    int y=ya; for (;y<=yz;y++) {
      if (eightspawn_valid_destination(plane,xa,y)) {
        if (candidatec>=CANDIDATE_LIMIT) goto _done_finding_candidates_;
        candidatev[candidatec++]=(struct candidate){xa,y};
      }
      if (eightspawn_valid_destination(plane,xz,y)) {
        if (candidatec>=CANDIDATE_LIMIT) goto _done_finding_candidates_;
        candidatev[candidatec++]=(struct candidate){xz,y};
      }
    }
  }
  #undef CANDIDATE_LIMIT
  #undef RADIUS_LIMIT
 _done_finding_candidates_:;
 
  /* Now pick a candidate at random, confirm that there's no nearby solid sprites, and that's where we're going.
   * Keep at this until the set is depleted. That's possible, and if it happens, abort.
   */
  for (;;) {
    if (candidatec<1) return -1;
    int p=rand()%candidatec;
    double x=candidatev[p].x+0.5;
    double y=candidatev[p].y+0.5;
    candidatec--;
    memmove(candidatev+p,candidatev+p+1,sizeof(struct candidate)*(candidatec-p));
    if (eightspawn_valid_sprite_position(x,y)) {
      SPRITE->dstx=x;
      SPRITE->dsty=y;
      break;
    }
  }
  
  /* And select a travel vector.
   * Constant speed.
   */
  SPRITE->dx=SPRITE->dstx-sprite->x;
  SPRITE->dy=SPRITE->dsty-sprite->y;
  double len=sqrt(SPRITE->dx*SPRITE->dx+SPRITE->dy*SPRITE->dy);
  if (len<0.5) return -1; // Can't be possible; we started the radius at 1.
  const double speed=8.0;
  SPRITE->dx=(SPRITE->dx*speed)/len;
  SPRITE->dy=(SPRITE->dy*speed)/len;

  return 0;
}

/* Update.
 */
 
static void _eightspawn_update(struct sprite *sprite,double elapsed) {
  sprite->x+=SPRITE->dx*elapsed;
  sprite->y+=SPRITE->dy*elapsed;
  if (
    ((SPRITE->dx<0.0)&&(sprite->x<=SPRITE->dstx))||
    ((SPRITE->dx>0.0)&&(sprite->x>=SPRITE->dstx))
  ) {
    if (
      ((SPRITE->dy<0.0)&&(sprite->y<=SPRITE->dsty))||
      ((SPRITE->dy>0.0)&&(sprite->y>=SPRITE->dsty))
    ) {
      sprite_kill_soon(sprite);
      struct sprite *monster=sprite_spawn(SPRITE->dstx,SPRITE->dsty,RID_sprite_eight,0,0,0,0,0);
    }
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_eightspawn={
  .name="eightspawn",
  .objlen=sizeof(struct sprite_eightspawn),
  .init=_eightspawn_init,
  .update=_eightspawn_update,
};

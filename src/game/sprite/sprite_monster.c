/* sprite_monster.c
 * Walks about randomly, and on contact launches a minigame.
 */

#include "game/game.h"
#include "sprite.h"

#define STAGE_IDLE   0
#define STAGE_WALK   1
#define STAGE_ATTACK 2

#define INITIAL_IDLE_TIME 1.000
#define ROAM_RANGE_2 25.0 /* m**2 from initial position */
#define ATTACK_RANGE_2 9.0 /* m**2, when the hero is this close, we approach. */
#define ATTACK_SPEED 5.0 /* m/s */
#define WALK_SPEED 3.0 /* m/s */

static void monster_idle_begin(struct sprite *sprite);
static void monster_walk_begin(struct sprite *sprite);

struct sprite_monster {
  struct sprite hdr;
  double x0,y0; // Initial position. We'll never stray far from it.
  uint8_t tileid0;
  int stage;
  double stageclock;
  double dx,dy; // Speed, in STAGE_WALK. (m/s)
  int battleid;
};

#define SPRITE ((struct sprite_monster*)sprite)

/* Init.
 */
 
static int _monster_init(struct sprite *sprite) {
  SPRITE->battleid=(sprite->arg[0]<<8)|sprite->arg[1];
  SPRITE->x0=sprite->x;
  SPRITE->y0=sprite->y;
  SPRITE->tileid0=sprite->tileid;
  SPRITE->stage=STAGE_IDLE;
  SPRITE->stageclock=INITIAL_IDLE_TIME;
  return 0;
}

/* Toggle "safe" from our physics mask.
 * In hot pursuit, we will cross spaces where normally we'd fear to tread.
 */
 
static void monster_permit_safe(struct sprite *sprite) {
  sprite->physics&=~(1<<NS_physics_safe);
}

static void monster_forbid_safe(struct sprite *sprite) {
  sprite->physics|=(1<<NS_physics_safe);
  if (sprite_test_position(sprite)) {
    // Cool, we're back to normal.
  } else {
    // Leave safe cells passable until we escape it incidentally. (see monster_walk_update)
    sprite->physics&=~(1<<NS_physics_safe);
  }
}

/* STAGE_IDLE
 */
 
static void monster_idle_update(struct sprite *sprite,double elapsed) {
}

static void monster_idle_begin(struct sprite *sprite) {
  SPRITE->stage=STAGE_IDLE;
  SPRITE->stageclock=0.250+((rand()&0xffff)*0.500)/32768.0;
}

static void monster_idle_end(struct sprite *sprite) {
  monster_walk_begin(sprite);
}

/* STAGE_WALK
 */
 
static void monster_walk_update(struct sprite *sprite,double elapsed) {
  if (!sprite_move(sprite,SPRITE->dx*elapsed,SPRITE->dy*elapsed)) {
    // Fully blocked.
    monster_idle_begin(sprite);
  } else {
    if (!(sprite->physics&(1<<NS_physics_safe))) { // Prior attack ended in a path collision. Can we start apply path collisions yet?
      sprite->physics|=(1<<NS_physics_safe);
      if (sprite_test_position(sprite)) {
        // Resuming deferred path collisions.
      } else {
        sprite->physics&=~(1<<NS_physics_safe);
      }
    }
  }
}

static void monster_walk_end(struct sprite *sprite) {
  monster_idle_begin(sprite);
}

static void monster_walk_begin(struct sprite *sprite) {

  /* Walk in a cardinal direction at a fixed speed for a random interval.
   * Any direction with at least one meter of freedom is a candidate.
   */
  struct dcan { double dx,dy; } dcanv[4];
  int dcanc=0,canxc=0,canyc=0;
  if (sprite_measure_freedom(sprite,-2.0,0.0,0)>=1.0) { canxc++; dcanv[dcanc++]=(struct dcan){-1.0,0.0}; }
  if (sprite_measure_freedom(sprite,2.0,0.0,0)>=1.0) { canxc++; dcanv[dcanc++]=(struct dcan){1.0,0.0}; }
  if (sprite_measure_freedom(sprite,0.0,-2.0,0)>=1.0) { canyc++; dcanv[dcanc++]=(struct dcan){0.0,-1.0}; }
  if (sprite_measure_freedom(sprite,0.0,2.0,0)>=1.0) { canyc++; dcanv[dcanc++]=(struct dcan){0.0,1.0}; }
  if (dcanc<1) {
    monster_idle_begin(sprite);
    return;
  }
  
  /* If we're outside ROAM_RANGE, prefer returning to the origin.
   * Only eliminate a direction if we have both options on that axis.
   */
  double dx=sprite->x-SPRITE->x0;
  double dy=sprite->y-SPRITE->y0;
  double d2=dx*dx+dy*dy;
  if (d2>ROAM_RANGE_2) {
    #define DROPDCAN(expr) { \
      int i=dcanc; while (i-->0) { \
        if (dcanv[i].expr) { \
          dcanc--; \
          memmove(dcanv+i,dcanv+i+1,sizeof(struct dcan)*(dcanc-i)); \
          break; \
        } \
      } \
    }
         if ((dx>0.0)&&(canxc>=2)) DROPDCAN(dx>0.0)
    else if ((dx<0.0)&&(canxc>=2)) DROPDCAN(dx<0.0)
         if ((dy>0.0)&&(canyc>=2)) DROPDCAN(dy>0.0)
    else if ((dy<0.0)&&(canyc>=2)) DROPDCAN(dy<0.0)
    #undef DROPDCAN
  }
  
  int dcanp=rand()%dcanc;
  SPRITE->dx=dcanv[dcanp].dx*WALK_SPEED;
  SPRITE->dy=dcanv[dcanp].dy*WALK_SPEED;
  SPRITE->stageclock=0.500+((rand()&0x7fff)*1.000)/32768.0;
  SPRITE->stage=STAGE_WALK;
}

/* STAGE_ATTACK
 */
 
static void monster_attack_update(struct sprite *sprite,double elapsed,struct sprite *hero) {
  double dx=hero->x-sprite->x;
  double dy=hero->y-sprite->y;
  double d2=dx*dx+dy*dy;
  if (d2<0.250) { // Impossibly close. Maybe we're not solid?
    return;
  }
  double d=sqrt(d2);
  if (!sprite_move(sprite,(dx*ATTACK_SPEED*elapsed)/d,(dy*ATTACK_SPEED*elapsed)/d)) {
    // Blocked, should we do something? No. Usually it's the hero that blocked us, and the battle has already triggered.
  }
}

static void monster_attack_begin(struct sprite *sprite,struct sprite *hero) {
  SPRITE->stage=STAGE_ATTACK;
  SPRITE->stageclock=30.0; // STAGE_ATTACK always ends explicitly; use an unreasonably long time.
}

static int monster_hero_in_range(struct sprite *sprite,struct sprite *hero) {
  if (!hero||hero->defunct) return 0;
  double dx=hero->x-sprite->x;
  double dy=hero->y-sprite->y;
  double d2=dx*dx+dy*dy;
  if (d2<ATTACK_RANGE_2) return 1;
  return 0;
}

/* Update.
 */
 
static void _monster_update(struct sprite *sprite,double elapsed) {

  /* If the hero is within ATTACK_RANGE and we're not already attacking, start attack, regardless of current stage.
   * Likewise, if we are attacking but the hero goes out of range, enter STAGE_IDLE.
   */
  struct sprite *hero=sprites_get_hero();
  if (SPRITE->stage==STAGE_ATTACK) {
    if (!monster_hero_in_range(sprite,hero)) {
      monster_forbid_safe(sprite);
      monster_idle_begin(sprite);
    }
  } else {
    if (monster_hero_in_range(sprite,hero)) {
      monster_permit_safe(sprite);
      monster_attack_begin(sprite,hero);
    }
  }

  if ((SPRITE->stageclock-=elapsed)<=0.0) {
    switch (SPRITE->stage) {
      case STAGE_IDLE: monster_idle_end(sprite); break;
      case STAGE_WALK: monster_walk_end(sprite); break;
      case STAGE_ATTACK: SPRITE->stageclock+=30.0; break; // STAGE_ATTACK doesn't time out.
      default: monster_idle_end(sprite);
    }
  } else {
    switch (SPRITE->stage) {
      case STAGE_IDLE: monster_idle_update(sprite,elapsed); break;
      case STAGE_WALK: monster_walk_update(sprite,elapsed); break;
      case STAGE_ATTACK: monster_attack_update(sprite,elapsed,hero); break;
    }
  }
}

/* Collide.
 */
 
static void _monster_collide(struct sprite *sprite,struct sprite *other) {
  if (other->type==&sprite_type_hero) {
    bm_begin_battle(SPRITE->battleid);
    sprite->defunct=1;
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_monster={
  .name="monster",
  .objlen=sizeof(struct sprite_monster),
  .init=_monster_init,
  .update=_monster_update,
  .collide=_monster_collide,
};

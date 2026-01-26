#include "game/bellacopia.h"

#define STAGE_IDLE   0
#define STAGE_WALK   1
#define STAGE_ATTACK 2
#define STAGE_TEMPT  3

#define INITIAL_IDLE_TIME 1.000
#define ROAM_RANGE_2 25.0 /* m**2 from initial position */
#define WALK_SPEED 3.0 /* m/s */
#define TEMPT_SPEED 4.0 /* m/s */
#define DAZE_TIME 2.0 /* Plus FLASH_TIME. */

static void monster_idle_begin(struct sprite *sprite);
static void monster_walk_begin(struct sprite *sprite);

struct sprite_monster {
  struct sprite hdr;
  double x0,y0; // Initial position. We'll never stray far from it.
  uint8_t tileid0;
  int stage;
  double stageclock;
  double dx,dy; // Speed, in STAGE_WALK. (m/s)
  int battle;
  double radius,radius2;
  double speed;
  double animclock;
  double dazeclock;
  int spent;
  int name_strix; // RID_strings_battle
};

#define SPRITE ((struct sprite_monster*)sprite)

/* Init.
 */
 
static int _monster_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->x0=sprite->x;
  SPRITE->y0=sprite->y;
  SPRITE->stage=STAGE_IDLE;
  SPRITE->stageclock=INITIAL_IDLE_TIME;
  SPRITE->radius=2.0;
  SPRITE->speed=TEMPT_SPEED;
  
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,sprite->cmd,sprite->cmdc)>=0) {
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_sprite_monster: {
            SPRITE->battle=(cmd.arg[0]<<8)|cmd.arg[1];
            SPRITE->radius=cmd.arg[2]/16.0;
            SPRITE->speed=cmd.arg[3]/16.0;
            SPRITE->name_strix=(cmd.arg[4]<<8)|cmd.arg[5];
          } break;
      }
    }
  }
  SPRITE->radius2=SPRITE->radius*SPRITE->radius;
  
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
  if (!sprite_move(sprite,(dx*SPRITE->speed*elapsed)/d,(dy*SPRITE->speed*elapsed)/d)) {
    // Blocked, should we do something? No. Usually it's the hero that blocked us, and the battle has already triggered.
  }
}

static void monster_attack_begin(struct sprite *sprite,struct sprite *hero) {
  SPRITE->stage=STAGE_ATTACK;
  SPRITE->stageclock=30.0; // STAGE_ATTACK always ends explicitly; use an unreasonably long time.
}

/* STAGE_TEMPT, basically the same thing as STAGE_ATTACK
 */
 
static void monster_tempt_update(struct sprite *sprite,double elapsed,struct sprite *hero) {
  double dx=hero->x-sprite->x;
  double dy=hero->y-sprite->y;
  double d2=dx*dx+dy*dy;
  if (d2<0.020) { // Impossibly close. Maybe we're not solid?
    return;
  }
  double d=sqrt(d2);
  sprite_move(sprite,(dx*TEMPT_SPEED*elapsed)/d,(dy*TEMPT_SPEED*elapsed)/d);
}

static void monster_tempt_begin(struct sprite *sprite,struct sprite *hero) {
  SPRITE->stage=STAGE_TEMPT;
  SPRITE->stageclock=30.0; // STAGE_TEMPT always ends explicitly; use an unreasonably long time.
}

/* Find our current target. Hero, princess, candy, maybe other things.
 * This is called every frame.
 * Return a sprite only if it is in range, etc.
 */
 
static struct sprite *monster_find_target(struct sprite *sprite) {
  struct sprite *best=0;
  double bestd2=999.999;
  struct sprite **otherp=GRP(monsterlike)->sprv;
  int otherc=GRP(monsterlike)->sprc;
  for (;otherc-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    
    /* Hero or Princess are ignored if outside the attack range.
     * Also, Candy overrides the living no matter what.
     * I'm not sure that makes sense from the standpoint of the monsters' motivation,
     * but as a game mechanic, Candy is expensive so it should have a simple and pronounced effect.
     */
    if (other->type==&sprite_type_hero) { //TODO "or princess"
      if (best&&(best->type==&sprite_type_candy)) continue; // Prefer Candy.
      if (other->type==&sprite_type_hero) {
        if (g.bugspray>0.0) continue;
        if (g.vanishing>0.0) continue;
      }
      double dx=other->x-sprite->x;
      double dy=other->y-sprite->y;
      double d2=dx*dx+dy*dy;
      if (d2>SPRITE->radius2) continue; // Too far away.
      if (!best) {
        best=other;
        bestd2=d2;
      } else { // Take the nearer of (other,best)
        if (d2<bestd2) {
          best=other;
          bestd2=d2;
        }
      }
      continue;
    }
    
    /* Candy anywhere is tempting.
     * Track all candy and retain the closest.
     */
    if (other->type==&sprite_type_candy) {
      double dx=other->x-sprite->x;
      double dy=other->y-sprite->y;
      double d2=dx*dx+dy*dy;
      if (!best||(best->type!=&sprite_type_candy)) {
        best=other;
        bestd2=d2;
      } else if (d2<bestd2) {
        best=other;
        bestd2=d2;
      }
      continue;
    }
  }
  return best;
}

/* Update.
 */
 
static void _monster_update(struct sprite *sprite,double elapsed) {

  // If dazed, nothing else happens, even animation.
  if (g.flash>0.0) { SPRITE->dazeclock=DAZE_TIME; return; }
  else if (SPRITE->dazeclock>0.0) {
    SPRITE->dazeclock-=elapsed;
    return;
  }

  // Animate always, even when standing still.
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.200;
    if (sprite->tileid==SPRITE->tileid0) sprite->tileid++;
    else sprite->tileid=SPRITE->tileid0;
  }
  
  // Track horizontal motion.
  double x0=sprite->x;
  
  /* Decide what we're targetting.
   */
  struct sprite *target=monster_find_target(sprite);
  if (!target) { // "nothing" is easy, just make sure we exit ATTACK or TEMPT, if we're there.
    if ((SPRITE->stage==STAGE_ATTACK)||(SPRITE->stage==STAGE_TEMPT)) {
      monster_forbid_safe(sprite);
      monster_idle_begin(sprite);
    }
  } else if (target->type==&sprite_type_hero) { // Hero gets a full vigorous attack. TODO Princess will get this treatment too.
    if (SPRITE->stage!=STAGE_ATTACK) {
      monster_permit_safe(sprite);
      monster_attack_begin(sprite,target);
    }
  } else { // All other targets, eg candy, cause a more passive draw.
    if (SPRITE->stage!=STAGE_TEMPT) {
      monster_forbid_safe(sprite);
      monster_tempt_begin(sprite,target);
    }
  }

  /* Generic update and stage transition per clock.
   */
  if ((SPRITE->stageclock-=elapsed)<=0.0) {
    switch (SPRITE->stage) {
      case STAGE_IDLE: monster_idle_end(sprite); break;
      case STAGE_WALK: monster_walk_end(sprite); break;
      case STAGE_ATTACK: SPRITE->stageclock+=30.0; break; // STAGE_ATTACK doesn't time out.
      case STAGE_TEMPT: SPRITE->stageclock+=30.0; break; // '' STAGE_TEMPT
      default: monster_idle_end(sprite);
    }
  } else {
    switch (SPRITE->stage) {
      case STAGE_IDLE: monster_idle_update(sprite,elapsed); break;
      case STAGE_WALK: monster_walk_update(sprite,elapsed); break;
      case STAGE_ATTACK: monster_attack_update(sprite,elapsed,target); break;
      case STAGE_TEMPT: monster_tempt_update(sprite,elapsed,target); break;
    }
  }
  
  // If we moved horizontally, update xform accordingly. Right is natural.
  if (sprite->x<x0) sprite->xform=EGG_XFORM_XREV;
  else if (sprite->x>x0) sprite->xform=0;
}

/* Battle callback.
 */
 
static void monster_cb_battle(struct modal *modal,int outcome,void *userdata) {
  struct sprite *sprite=userdata;
  sprite_kill_soon(sprite);
  if (outcome>0) {
    //TODO Other prizes.
    game_get_item(NS_itemid_gold,1);
    modal_battle_add_consequence(modal,NS_itemid_gold,1);
  } else if (outcome<0) {
    game_hurt_hero();
    modal_battle_add_consequence(modal,NS_itemid_heart,-1);
  }
}

/* Collide.
 */
 
static void _monster_collide(struct sprite *sprite,struct sprite *other) {
  if (SPRITE->spent) return;
  if (other->type==&sprite_type_hero) {
    // With vanishing cream, we will refuse to enter battle. Note that this is a further level of anti-battle than bugspray, which only prevents chasing.
    if (g.vanishing>0.0) {
      return;
    }
    int battle=SPRITE->battle;
    struct modal_args_battle args={
      .battle=battle,
      .players=NS_players_man_cpu,
      .handicap=0x80,//TODO how to decide?
      .cb=monster_cb_battle,
      .userdata=sprite,
      .left_name=4, // "Dot"
      .right_name=SPRITE->name_strix,
    };
    struct modal *modal=modal_spawn(&modal_type_battle,&args,sizeof(args));
    if (!modal) return;
    SPRITE->spent=1;
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

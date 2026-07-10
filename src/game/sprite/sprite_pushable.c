/* sprite_pushable.c
 * Moves by discrete intervals when bumped, optionally only when wearing the Power Glove.
 * We do not touch our tileid -- sprite_sokoban depends on that, and modifies tileid behind our back.
 */
 
#include "game/bellacopia.h"

#define SLIDE_SPEED 10.0
#define SLIDE_GIVEUP_TIME 0.250 /* > 1/SLIDE_SPEED */
#define NUDGE_SPEED 0.500

struct sprite_pushable {
  struct sprite hdr;
  int weight; // (0,1,2)=(ounce,pound,ton)
  int pressure; // Nonzero if we got a collision last time.
  double resistance; // Volatile. Rests at 1, and when it crosses 0 we move.
  double resrate; // unit/sec for resistance to drop during a push.
  int movedx,movedy;
  double targetx,targety;
  double moveclock; // Terminate move if it expires, and stay wherever we are.
  double pressdx,pressdy; // Must be a cardinal normal, if (pressure) nonzero.
};

#define SPRITE ((struct sprite_pushable*)sprite)

/* If this hero is on top of us, warp to one side to let her thru, and return nonzero.
 */
 
static int pushable_avoid_hero(struct sprite *sprite,double x,double y,int z) {
  if (sprite->z!=z) return 0;
  const double thresh=0.750;
  double dx=x-sprite->x;
  if ((dx<-thresh)||(dx>thresh)) return 0;
  double dy=y-sprite->y;
  if ((dy<-thresh)||(dy>thresh)) return 0;
  
  /* Move to any vacant or safe cell cardinally adjacent to me.
   * Select among them at random.
   * Designing maps, be careful not to put any solid sprites next to a door-cap. (or account for them here if needed)
   */
  const struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return 0;
  int col=(int)sprite->x-map->lng*NS_sys_mapw;
  int row=(int)sprite->y-map->lat*NS_sys_maph;
  if ((col<0)||(row<0)||(col>=NS_sys_mapw)||(row>=NS_sys_maph)) return 0;
  struct candidate { int x,y; } candidatev[4];
  int candidatec=0;
  #define CHECKCELL(_x,_y) { \
    int cx=(_x),cy=(_y); \
    if ((cx>=0)&&(cy>=0)&&(cx<NS_sys_mapw)&&(cy<NS_sys_maph)) { \
      uint8_t physics=map->physics[map->v[cy*NS_sys_mapw+cx]]; \
      if ((physics==NS_physics_vacant)||(physics==NS_physics_safe)||(physics==NS_physics_ice)) { \
        candidatev[candidatec++]=(struct candidate){cx,cy}; \
      } \
    } \
  }
  CHECKCELL(col-1,row)
  CHECKCELL(col+1,row)
  CHECKCELL(col,row-1)
  CHECKCELL(col,row+1)
  #undef CHECKCELL
  if (!candidatec) return 0; // Oh this is going to suck.
  int candidatep=rand()%candidatec;
  sprite->x=map->lng*NS_sys_mapw+candidatev[candidatep].x+0.5;
  sprite->y=map->lat*NS_sys_maph+candidatev[candidatep].y+0.5;
  return 1;
}

/* Init.
 */

static int _pushable_init(struct sprite *sprite) {

  // Optional "don't spawn" flag.
  int fldid=(sprite->arg[0]<<8)|sprite->arg[1];
  if (store_get_fld(fldid)) return -1;

  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,sprite->cmd,sprite->cmdc)>=0) {
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_sprite_weight: SPRITE->weight=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      }
    }
  }
  switch (SPRITE->weight) {
    case 0: SPRITE->resrate=5.000; break;
    case 1: SPRITE->resrate=3.000; break;
    case 2: SPRITE->resrate=2.000; break;
    default: fprintf(stderr,"%s: Invalid weight %d\n",__func__,SPRITE->weight); return -1;
  }
  
  /* We need special handling for the blocks that cover a ladder.
   * They need to move aside when you've entered this map from below.
   * And it's tricky because we get instantiated before hero assumes the new position.
   */
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    double dstx,dsty;
    if (pushable_avoid_hero(sprite,hero->x,hero->y,hero->z)) {
      // This will never be the case, because hero doesn't take her new position until after the map is loaded. But we check to be on the safe side.
    } else if (sprite_hero_is_using_door(&dstx,&dsty,hero)) {
      pushable_avoid_hero(sprite,dstx,dsty,sprite->z); // Assume that hero's new (z) is wherever we are. It's not recorded in hero.
    }
  }
  
  return 0;
}

/* Motion.
 */

static void pushable_begin_move(struct sprite *sprite) {

  // If it's obvious that we're being pushed into a wall, don't make the swish sound.
  // (we'll behave correctly without this check, just we make that inappropriate sound effect).
  int cx=(int)sprite->x,cy=(int)sprite->y,ignorewall=0;
  double subx=sprite->x-cx,suby=sprite->y-cy;
       if ((SPRITE->pressdx<0.0)&&(subx>0.490)&&(subx<0.510)) cx--;
  else if ((SPRITE->pressdx>0.0)&&(subx>0.490)&&(subx<0.510)) cx++;
  else if ((SPRITE->pressdy<0.0)&&(suby>0.490)&&(suby<0.510)) cy--;
  else if ((SPRITE->pressdy>0.0)&&(suby>0.490)&&(suby<0.510)) cy++;
  else ignorewall=1;
  if (!ignorewall) {
    struct map *map=map_by_sprite_position(cx,cy,sprite->z);
    if (map) {
      cx-=map->lng*NS_sys_mapw;
      cy-=map->lat*NS_sys_maph;
      if ((cx>=0)&&(cy>=0)&&(cx<NS_sys_mapw)&&(cy<NS_sys_maph)) {
        uint8_t physics=map->physics[map->v[cy*NS_sys_mapw+cx]];
        switch (physics) {
          case NS_physics_solid:
          case NS_physics_grabbable:
          case NS_physics_vanishable:
          case NS_physics_hole:
          case NS_physics_water:
            return;
        }
      }
    }
  }

  SPRITE->movedx=SPRITE->pressdx;
  SPRITE->movedy=SPRITE->pressdy;
  int qx=(int)sprite->x;
  int qy=(int)sprite->y;
  int tx=qx+SPRITE->movedx;
  int ty=qy+SPRITE->movedy;
  SPRITE->targetx=tx+0.5;
  SPRITE->targety=ty+0.5;
  SPRITE->moveclock=SLIDE_GIVEUP_TIME; // All motion is much faster than 1 m/s. If we're still trying after so long, give up.
  bm_sound(RID_sound_push);
}

static void pushable_end_move(struct sprite *sprite) {
  SPRITE->movedx=SPRITE->movedy=0;
  /* If we're very close to the target but not on it exactly, try nudging us on to it.
   */
  const double tolerance=1.0/NS_sys_tilesize;
  double offx=sprite->x-SPRITE->targetx;
  double offy=sprite->y-SPRITE->targety;
  if (
    ((offx<-0.0)||(offx>0.0)||(offy<-0.0)||(offy>0.0))&&
    (offx>-tolerance)&&(offx<tolerance)&&
    (offy>-tolerance)&&(offy<tolerance)
  ) {
    double x0=sprite->x;
    double y0=sprite->y;
    sprite->x=SPRITE->targetx;
    sprite->y=SPRITE->targety;
    if (!sprite_test_position(sprite)) {
      sprite->x=x0;
      sprite->y=y0;
    }
  }
}

/* Update.
 */

static void _pushable_update(struct sprite *sprite,double elapsed) {

  if (SPRITE->movedx||SPRITE->movedy) {
    if ((SPRITE->moveclock-=elapsed)<=0.0) {
      pushable_end_move(sprite);
    } else {
      sprite_move(sprite,SPRITE->movedx*SLIDE_SPEED*elapsed,SPRITE->movedy*SLIDE_SPEED*elapsed);
      if (SPRITE->movedx<0) {
        if (sprite->x<=SPRITE->targetx) {
          sprite->x=SPRITE->targetx;
          pushable_end_move(sprite);
        }
      } else if (SPRITE->movedx>0) {
        if (sprite->x>=SPRITE->targetx) {
          sprite->x=SPRITE->targetx;
          pushable_end_move(sprite);
        }
      } else if (SPRITE->movedy<0) {
        if (sprite->y<=SPRITE->targety) {
          sprite->y=SPRITE->targety;
          pushable_end_move(sprite);
        }
      } else if (SPRITE->movedy>0) {
        if (sprite->y>=SPRITE->targety) {
          sprite->y=SPRITE->targety;
          pushable_end_move(sprite);
        }
      }
    }
    return;
  }

  if (SPRITE->pressure) {
    SPRITE->pressure=0;
    SPRITE->resistance-=SPRITE->resrate*elapsed;
    if (SPRITE->resistance<=0.0) {
      SPRITE->resistance=1.0;
      pushable_begin_move(sprite);
    }
  } else {
    SPRITE->resistance=1.0;
    // Since nothing else is happening, approach a quantized position.
    int qx=(int)sprite->x;
    int qy=(int)sprite->y;
    double tx=qx+0.5;
    double ty=qy+0.5;
    if (sprite->x<tx) {
      sprite_move(sprite,NUDGE_SPEED*elapsed,0.0);
      if (sprite->x>=tx) sprite->x=tx;
    } else if (sprite->x>tx) {
      sprite_move(sprite,-NUDGE_SPEED*elapsed,0.0);
      if (sprite->x<=tx) sprite->x=tx;
    }
    if (sprite->y<ty) {
      sprite_move(sprite,0.0,NUDGE_SPEED*elapsed);
      if (sprite->y>=ty) sprite->y=ty;
    } else if (sprite->y>ty) {
      sprite_move(sprite,0.0,-NUDGE_SPEED*elapsed);
      if (sprite->y<=ty) sprite->y=ty;
    }
  }
}

/* Collide.
 */

static int is_bumper(struct sprite *sprite,struct sprite *bumper) {
  if (!bumper) return 0;
  
  // Very heavy blocks can only be pushed by a hero wearing the power glove.
  if (SPRITE->weight>=2) {
    if (bumper->type!=&sprite_type_hero) return 0;
    if (g.store.invstorev[0].itemid!=NS_itemid_glove) return 0;
    return 1;
  }
  
  // Other blocks (pound and ounce) can always be pushed by the hero.
  if (bumper->type==&sprite_type_hero) return 1;
  
  // The ounce block can also be pushed by marionettes.
  if (SPRITE->weight<=0) {
    if (bumper->type==&sprite_type_marionette) return 1;
  }
  
  return 0;
}

static void _pushable_collide(struct sprite *sprite,struct sprite *bumper) {
  if (!is_bumper(sprite,bumper)) return;
  SPRITE->pressure=1;
  double dx=sprite->x-bumper->x;
  double dy=sprite->y-bumper->y;
  double adx=dx; if (adx<0.0) adx=-adx;
  double ady=dy; if (ady<0.0) ady=-ady;
  if (adx>ady) {
    SPRITE->pressdx=(dx<0.0)?-1.0:1.0;
    SPRITE->pressdy=0.0;
  } else {
    SPRITE->pressdx=0.0;
    SPRITE->pressdy=(dy<0.0)?-1.0:1.0;
  }
}

/* Type definition.
 */

const struct sprite_type sprite_type_pushable={
  .name="pushable",
  .objlen=sizeof(struct sprite_pushable),
  .init=_pushable_init,
  .update=_pushable_update,
  .collide=_pushable_collide,
};

/* sprite_pushable.c
 * Moves by discrete intervals when bumped, optionally only when wearing the Power Glove.
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

static int _pushable_init(struct sprite *sprite) {
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
  return 0;
}

static void pushable_begin_move(struct sprite *sprite) {
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

const struct sprite_type sprite_type_pushable={
  .name="pushable",
  .objlen=sizeof(struct sprite_pushable),
  .init=_pushable_init,
  .update=_pushable_update,
  .collide=_pushable_collide,
};

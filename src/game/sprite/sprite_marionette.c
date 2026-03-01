/* sprite_marionette.c
 * A little puppet that Dot controls remotely.
 */
 
#include "game/bellacopia.h"

#define WALK_SPEED 4.0 /* Slower than Dot. */

struct sprite_marionette {
  struct sprite hdr;
  uint8_t tileid0;
  int indx,indy;
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_marionette*)sprite)

/* Init.
 */
 
static int _marionette_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  return 0;
}

/* Update.
 */
 
static void _marionette_update(struct sprite *sprite,double elapsed) {
  
  // Animate.
  if (SPRITE->indx||SPRITE->indy) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.200;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    }
  } else {
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
  }
  sprite->tileid=SPRITE->tileid0;
  switch (SPRITE->animframe) {
    case 1: sprite->tileid+=1; break;
    case 3: sprite->tileid+=2; break;
  }

  // Apply motion.
  if (SPRITE->indx) {
    sprite_move(sprite,SPRITE->indx*WALK_SPEED*elapsed,0.0);
  }
  if (SPRITE->indy) {
    sprite_move(sprite,0.0,SPRITE->indy*WALK_SPEED*elapsed);
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_marionette={
  .name="marionette",
  .objlen=sizeof(struct sprite_marionette),
  .init=_marionette_init,
  .update=_marionette_update,
};

/* Set input.
 */

void sprite_marionette_set_input(struct sprite *sprite,int dx,int dy) {
  if (!sprite||(sprite->type!=&sprite_type_marionette)) return;
  SPRITE->indx=dx;
  SPRITE->indy=dy;
}

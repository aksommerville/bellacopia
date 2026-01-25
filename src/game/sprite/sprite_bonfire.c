#include "game/bellacopia.h"

struct sprite_bonfire {
  struct sprite hdr;
  uint8_t tileid0;
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_bonfire*)sprite)

static int _bonfire_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  return 0;
}

static void _bonfire_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.150;
    if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    sprite->tileid=SPRITE->tileid0+(SPRITE->animframe&1);
    sprite->xform=(SPRITE->animframe&2)?0:EGG_XFORM_XREV;
  }
}

const struct sprite_type sprite_type_bonfire={
  .name="bonfire",
  .objlen=sizeof(struct sprite_bonfire),
  .init=_bonfire_init,
  .update=_bonfire_update,
};

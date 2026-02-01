#include "game/bellacopia.h"

struct sprite_setfld {
  struct sprite hdr;
  int fld;
};

#define SPRITE ((struct sprite_setfld*)sprite)

static int _setfld_init(struct sprite *sprite) {
  SPRITE->fld=(sprite->arg[0]<<8)|sprite->arg[1];
  if (store_get_fld(SPRITE->fld)) return -1;
  return 0;
}

static void _setfld_collide(struct sprite *sprite,struct sprite *other) {
  if (other->type==&sprite_type_hero) {
    bm_sound(RID_sound_collect);
    store_set_fld(SPRITE->fld,1);
    sprite_kill_soon(sprite);
  }
}

const struct sprite_type sprite_type_setfld={
  .name="setfld",
  .objlen=sizeof(struct sprite_setfld),
  .init=_setfld_init,
  .collide=_setfld_collide,
};

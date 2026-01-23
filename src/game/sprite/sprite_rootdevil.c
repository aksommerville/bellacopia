#include "game/bellacopia.h"

struct sprite_rootdevil {
  struct sprite hdr;
  int fld;
};

#define SPRITE ((struct sprite_rootdevil*)sprite)

static int _rootdevil_init(struct sprite *sprite) {
  SPRITE->fld=(sprite->arg[0]<<8)|sprite->arg[1];
  if (store_get_fld(SPRITE->fld)) return -1;
  return 0;
}

static void _rootdevil_collide(struct sprite *sprite,struct sprite *other) {
  if (other->type==&sprite_type_hero) {
    fprintf(stderr,"TODO: root devil battle %d!\n",SPRITE->fld);
    store_set_fld(SPRITE->fld,1);//XXX for now, you win the strangling contest by touching him at all
    sprite_kill_soon(sprite);
  }
}

const struct sprite_type sprite_type_rootdevil={
  .name="rootdevil",
  .objlen=sizeof(struct sprite_rootdevil),
  .init=_rootdevil_init,
  .collide=_rootdevil_collide,
};

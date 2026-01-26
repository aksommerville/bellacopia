#include "game/bellacopia.h"

struct sprite_tolltroll {
  struct sprite hdr;
  int itemid_wanted;
};

#define SPRITE ((struct sprite_tolltroll*)sprite)

static int _tolltroll_init(struct sprite *sprite) {
  SPRITE->itemid_wanted=tolltroll_get_appearance();
  switch (SPRITE->itemid_wanted) {
    case NS_itemid_stick: break;
    case NS_itemid_compass: sprite->tileid+=1; break;
    case NS_itemid_candy: sprite->tileid+=2; break;
    default: return -1;
  }
  return 0;
}

static void _tolltroll_update(struct sprite *sprite,double elapsed) {
  if (store_get_fld(NS_fld_toll_paid)) {
    sprite_kill_soon(sprite);
  }
}

static void _tolltroll_collide(struct sprite *sprite,struct sprite *other) {
  if (other->type==&sprite_type_hero) {
    game_begin_activity(NS_activity_tolltroll,SPRITE->itemid_wanted,sprite);
  }
}

const struct sprite_type sprite_type_tolltroll={
  .name="tolltroll",
  .objlen=sizeof(struct sprite_tolltroll),
  .init=_tolltroll_init,
  .update=_tolltroll_update,
  .collide=_tolltroll_collide,
};

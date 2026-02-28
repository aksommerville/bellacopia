#include "game/bellacopia.h"

struct sprite_tolltroll {
  struct sprite hdr;
  int itemid_wanted;
  int price,fld; // If both zero, we're the three-part fetch quest. Otherwise a generic pay-to-pass toll troll.
};

#define SPRITE ((struct sprite_tolltroll*)sprite)

static int _tolltroll_init(struct sprite *sprite) {
  SPRITE->price=(sprite->arg[0]<<8)|sprite->arg[1];
  SPRITE->fld=(sprite->arg[2]<<8)|sprite->arg[3];
  if (SPRITE->fld) { // Generic.
    if (store_get_fld(SPRITE->fld)) return -1;
  } else { // Main Quest.
    SPRITE->itemid_wanted=tolltroll_get_appearance();
    switch (SPRITE->itemid_wanted) {
      case NS_itemid_stick: break;
      case NS_itemid_compass: sprite->tileid+=1; break;
      case NS_itemid_candy: sprite->tileid+=2; break;
      default: return -1;
    }
  }
  return 0;
}

static void _tolltroll_update(struct sprite *sprite,double elapsed) {
  if (SPRITE->fld) {
    if (store_get_fld(SPRITE->fld)) {
      sprite_kill_soon(sprite);
    }
  } else {
    if (store_get_fld(NS_fld_toll_paid)) {
      sprite_kill_soon(sprite);
    }
  }
  if (GRP(hero)->sprc>0) {
    struct sprite *hero=GRP(hero)->sprv[0];
    double dx=hero->x-sprite->x;
    if (dx<-0.5) sprite->xform=EGG_XFORM_XREV;
    else if (dx>0.5) sprite->xform=0;
  }
}

static void _tolltroll_collide(struct sprite *sprite,struct sprite *other) {
  if (other->type==&sprite_type_hero) {
    if (SPRITE->fld) {
      game_begin_activity(NS_activity_generic_tolltroll,(SPRITE->price<<12)|SPRITE->fld,sprite);
    } else {
      game_begin_activity(NS_activity_tolltroll,SPRITE->itemid_wanted,sprite);
    }
  }
}

const struct sprite_type sprite_type_tolltroll={
  .name="tolltroll",
  .objlen=sizeof(struct sprite_tolltroll),
  .init=_tolltroll_init,
  .update=_tolltroll_update,
  .collide=_tolltroll_collide,
};

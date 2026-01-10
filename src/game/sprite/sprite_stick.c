#include "game/game.h"
#include "sprite.h"

struct sprite_stick {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_stick*)sprite)

static int _stick_init(struct sprite *sprite) {
  sprite->xform=rand()&7;
  return 0;
}

static void _stick_update(struct sprite *sprite,double elapsed) {
  struct sprite *hero=sprites_get_hero();
  if (hero) {
    const double radius=0.5;
    double dx=hero->x-sprite->x;
    if ((dx>-radius)&&(dx<radius)) {
      double dy=hero->y-sprite->y;
      if ((dy>-radius)&&(dy<radius)) {
        struct inventory *inventory=inventory_search(NS_itemid_stick);
        if (inventory) return; // You already have a stick. I'll just chill.
        if (inventory_acquire(NS_itemid_stick,0)) {
          sprite->defunct=1;
        }
      }
    }
  }
}

const struct sprite_type sprite_type_stick={
  .name="stick",
  .objlen=sizeof(struct sprite_stick),
  .init=_stick_init,
  .update=_stick_update,
};

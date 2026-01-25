#include "game/bellacopia.h"

struct sprite_stick {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_stick*)sprite)

/* Init.
 */
 
static int _stick_init(struct sprite *sprite) {
  sprite->xform=rand()&7;
  return 0;
}

/* Update.
 */
 
static void _stick_update(struct sprite *sprite,double elapsed) {
  if (GRP(hero)->sprc>0) {
    struct sprite *hero=GRP(hero)->sprv[0];
    const double radius=0.750;
    double dx=hero->x-sprite->x;
    if ((dx>-radius)&&(dx<radius)) {
      double dy=hero->y-sprite->y;
      if ((dy>-radius)&&(dy<radius)) {
        if (game_get_item(NS_itemid_stick,0)) {
          sprite_kill_soon(sprite);
        }
      }
    }
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_stick={
  .name="stick",
  .objlen=sizeof(struct sprite_stick),
  .init=_stick_init,
  .update=_stick_update,
};

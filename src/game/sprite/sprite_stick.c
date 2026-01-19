#include "game/bellacopia.h"

struct sprite_stick {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_stick*)sprite)

/* Cleanup.
 */
 
static void _stick_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _stick_init(struct sprite *sprite) {
  return 0;
}

/* Update.
 */
 
static void _stick_update(struct sprite *sprite,double elapsed) {
}

/* Render.
 */
 
static void _stick_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
}

/* Collide.
 */
 
static void _stick_collide(struct sprite *sprite,struct sprite *other) {
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_stick={
  .name="stick",
  .objlen=sizeof(struct sprite_stick),
  .del=_stick_del,
  .init=_stick_init,
  .update=_stick_update,
  .render=_stick_render,
  .collide=_stick_collide,
};

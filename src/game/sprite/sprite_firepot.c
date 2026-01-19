#include "game/bellacopia.h"

struct sprite_firepot {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_firepot*)sprite)

/* Cleanup.
 */
 
static void _firepot_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _firepot_init(struct sprite *sprite) {
  return 0;
}

/* Update.
 */
 
static void _firepot_update(struct sprite *sprite,double elapsed) {
}

/* Render.
 */
 
static void _firepot_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
}

/* Collide.
 */
 
static void _firepot_collide(struct sprite *sprite,struct sprite *other) {
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_firepot={
  .name="firepot",
  .objlen=sizeof(struct sprite_firepot),
  .del=_firepot_del,
  .init=_firepot_init,
  .update=_firepot_update,
  .render=_firepot_render,
  .collide=_firepot_collide,
};

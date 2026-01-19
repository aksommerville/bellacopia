#include "game/bellacopia.h"

struct sprite_candy {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_candy*)sprite)

/* Cleanup.
 */
 
static void _candy_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _candy_init(struct sprite *sprite) {
  return 0;
}

/* Update.
 */
 
static void _candy_update(struct sprite *sprite,double elapsed) {
}

/* Render.
 */
 
static void _candy_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
}

/* Collide.
 */
 
static void _candy_collide(struct sprite *sprite,struct sprite *other) {
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_candy={
  .name="candy",
  .objlen=sizeof(struct sprite_candy),
  .del=_candy_del,
  .init=_candy_init,
  .update=_candy_update,
  .render=_candy_render,
  .collide=_candy_collide,
};

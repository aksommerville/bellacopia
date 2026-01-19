#include "game/bellacopia.h"

struct sprite_jigpiece {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_jigpiece*)sprite)

/* Cleanup.
 */
 
static void _jigpiece_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _jigpiece_init(struct sprite *sprite) {
  return 0;
}

/* Update.
 */
 
static void _jigpiece_update(struct sprite *sprite,double elapsed) {
}

/* Render.
 */
 
static void _jigpiece_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
}

/* Collide.
 */
 
static void _jigpiece_collide(struct sprite *sprite,struct sprite *other) {
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_jigpiece={
  .name="jigpiece",
  .objlen=sizeof(struct sprite_jigpiece),
  .del=_jigpiece_del,
  .init=_jigpiece_init,
  .update=_jigpiece_update,
  .render=_jigpiece_render,
  .collide=_jigpiece_collide,
};

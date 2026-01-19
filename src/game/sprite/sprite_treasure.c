#include "game/bellacopia.h"

struct sprite_treasure {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_treasure*)sprite)

/* Cleanup.
 */
 
static void _treasure_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _treasure_init(struct sprite *sprite) {
  return 0;
}

/* Update.
 */
 
static void _treasure_update(struct sprite *sprite,double elapsed) {
}

/* Render.
 */
 
static void _treasure_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
}

/* Collide.
 */
 
static void _treasure_collide(struct sprite *sprite,struct sprite *other) {
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_treasure={
  .name="treasure",
  .objlen=sizeof(struct sprite_treasure),
  .del=_treasure_del,
  .init=_treasure_init,
  .update=_treasure_update,
  .render=_treasure_render,
  .collide=_treasure_collide,
};

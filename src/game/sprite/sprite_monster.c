#include "game/bellacopia.h"

struct sprite_monster {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_monster*)sprite)

/* Cleanup.
 */
 
static void _monster_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _monster_init(struct sprite *sprite) {
  return 0;
}

/* Update.
 */
 
static void _monster_update(struct sprite *sprite,double elapsed) {
}

/* Render.
 */
 
static void _monster_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
}

/* Collide.
 */
 
static void _monster_collide(struct sprite *sprite,struct sprite *other) {
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_monster={
  .name="monster",
  .objlen=sizeof(struct sprite_monster),
  .del=_monster_del,
  .init=_monster_init,
  .update=_monster_update,
  .render=_monster_render,
  .collide=_monster_collide,
};

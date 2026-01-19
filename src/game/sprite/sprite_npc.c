#include "game/bellacopia.h"

struct sprite_npc {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_npc*)sprite)

/* Cleanup.
 */
 
static void _npc_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _npc_init(struct sprite *sprite) {
  return 0;
}

/* Update.
 */
 
static void _npc_update(struct sprite *sprite,double elapsed) {
}

/* Render.
 */
 
static void _npc_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
}

/* Collide.
 */
 
static void _npc_collide(struct sprite *sprite,struct sprite *other) {
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_npc={
  .name="npc",
  .objlen=sizeof(struct sprite_npc),
  .del=_npc_del,
  .init=_npc_init,
  .update=_npc_update,
  .render=_npc_render,
  .collide=_npc_collide,
};

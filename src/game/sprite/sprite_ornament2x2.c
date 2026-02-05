/* sprite_ornament2x2.c
 * No behavior.
 * Just renders 2x2 tiles offset by a half-tile (so, centered on the spawn point).
 */

#include "game/bellacopia.h"

struct sprite_ornament2x2 {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_ornament2x2*)sprite)

static void _ornament2x2_render(struct sprite *sprite,int x,int y) {
  const int ht=NS_sys_tilesize>>1;
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x-ht,y-ht,sprite->tileid+0x00,0);
  graf_tile(&g.graf,x+ht,y-ht,sprite->tileid+0x01,0);
  graf_tile(&g.graf,x-ht,y+ht,sprite->tileid+0x10,0);
  graf_tile(&g.graf,x+ht,y+ht,sprite->tileid+0x11,0);
}

const struct sprite_type sprite_type_ornament2x2={
  .name="ornament2x2",
  .objlen=sizeof(struct sprite_ornament2x2),
  .render=_ornament2x2_render,
};

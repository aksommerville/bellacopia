/* sprite_statue.c
 */
 
#include "game/bellacopia.h"

struct sprite_statue {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_statue*)sprite)

static int _statue_init(struct sprite *sprite) {
  sprite->xform=(rand()&EGG_XFORM_XREV);
  return 0;
}

static void _statue_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
  graf_tile(&g.graf,x,y-NS_sys_tilesize,sprite->tileid-0x10,sprite->xform);
}

const struct sprite_type sprite_type_statue={
  .name="statue",
  .objlen=sizeof(struct sprite_statue),
  .init=_statue_init,
  .render=_statue_render,
};

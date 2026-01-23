#include "game/bellacopia.h"

struct sprite_firepot {
  struct sprite hdr;
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_firepot*)sprite)

/* Update.
 */
 
static void _firepot_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.125;
    if (++(SPRITE->animframe)>=6) SPRITE->animframe=0;
  }
}

/* Render.
 */
 
static void _firepot_render(struct sprite *sprite,int x,int y) {
  uint8_t flametile,flamexform;
  switch (SPRITE->animframe) {
    case 0: flametile=sprite->tileid+2; flamexform=0; break;
    case 1: flametile=sprite->tileid+3; flamexform=0; break;
    case 2: flametile=sprite->tileid+4; flamexform=0; break;
    case 3: flametile=sprite->tileid+2; flamexform=EGG_XFORM_XREV; break;
    case 4: flametile=sprite->tileid+3; flamexform=EGG_XFORM_XREV; break;
    case 5: flametile=sprite->tileid+4; flamexform=EGG_XFORM_XREV; break;
  }
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid+(SPRITE->animframe&1),sprite->xform);
  graf_tile(&g.graf,x,y,flametile,flamexform);
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_firepot={
  .name="firepot",
  .objlen=sizeof(struct sprite_firepot),
  .update=_firepot_update,
  .render=_firepot_render,
};

/* sprite_firepot.c
 * Pretty animated fire, that the camera will use as a light source.
 */
 
#include "game/game.h"
#include "sprite.h"

struct sprite_firepot {
  struct sprite hdr;
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_firepot*)sprite)

static int _firepot_init(struct sprite *sprite) {
  sprite->light_radius=2.0;//TODO from commands or args or both
  return 0;
}

static void _firepot_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.150;
    if (++(SPRITE->animframe)>=6) SPRITE->animframe=0;
  }
}

static void _firepot_render(struct sprite *sprite,int x,int y) {
  uint8_t base=sprite->tileid,flame=sprite->tileid+2;
  uint8_t flamexform=0;
  switch (SPRITE->animframe) {
    case 0: break;
    case 1: flame+=1; break;
    case 2: flame+=2; break;
    case 3: flamexform=EGG_XFORM_XREV; base++; break;
    case 4: flamexform=EGG_XFORM_XREV; flame+=1; base++; break;
    case 5: flamexform=EGG_XFORM_XREV; flame+=2; base++; break;
  }
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,base,0);
  graf_tile(&g.graf,x,y,flame,flamexform);
}

const struct sprite_type sprite_type_firepot={
  .name="firepot",
  .objlen=sizeof(struct sprite_firepot),
  .init=_firepot_init,
  .update=_firepot_update,
  .render=_firepot_render,
};

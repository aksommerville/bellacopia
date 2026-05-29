#include "game/bellacopia.h"

struct sprite_mnneon {
  struct sprite hdr;
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_mnneon*)sprite)

static const uint8_t tile_by_frame[]={
  1,1,1,1, // lit
  0,0,     // dark
  1,1,1,1, // lit
  0,0,     // dark
  2,3,4,5,6,7,8,9, // walking
};

static int _mnneon_init(struct sprite *sprite) {
  return 0;
}

static void _mnneon_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.333;
    const int framec=sizeof(tile_by_frame)/sizeof(tile_by_frame[0]);
    if (++(SPRITE->animframe)>=framec) SPRITE->animframe=0;
  }
}

static void _mnneon_render(struct sprite *sprite,int x,int y) {
  // Reference position is our left tile, but also we're a bit offset from the grid.
  x+=5;
  y-=3;
  graf_set_image(&g.graf,sprite->imageid);
  uint8_t tileid=sprite->tileid;
  tileid+=tile_by_frame[SPRITE->animframe]<<4;
  graf_tile(&g.graf,x,y,tileid,0);
  graf_tile(&g.graf,x+NS_sys_tilesize,y,tileid+1,0);
}

const struct sprite_type sprite_type_mnneon={
  .name="mnneon",
  .objlen=sizeof(struct sprite_mnneon),
  .init=_mnneon_init,
  .update=_mnneon_update,
  .render=_mnneon_render,
};

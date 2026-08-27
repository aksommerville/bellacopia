/* sprite_casneon.c
 * Decorative neon dollar sign for the casino.
 */
 
#include "game/bellacopia.h"

struct sprite_casneon {
  struct sprite hdr;
  uint32_t lit; // Zero if dimmed, primary color if lit.
  double clock; // Counts down to toggling (lit).
};

#define SPRITE ((struct sprite_casneon*)sprite)

static uint32_t casneon_random_color() {
  switch (rand()&7) {
    case 0: return 0xffff00ff;
    case 1: return 0xff0000ff;
    case 2: return 0x00ff00ff;
    case 3: return 0x0000ffff;
    case 4: return 0x00ffffff;
    case 5: return 0xff00ffff;
    case 6: return 0xff8000ff;
    case 7: return 0x80ff00ff;
  }
  return 0xffff00ff;
}

static double casneon_random_interval() {
  const double min=0.300;
  const double max=0.700;
  double n=(rand()&0xffff)/65535.0;
  return n*min+(1.0-n)*max;
}

static int _casneon_init(struct sprite *sprite) {
  if (rand()&1) SPRITE->lit=casneon_random_color();
  SPRITE->clock=casneon_random_interval();
  SPRITE->clock*=(rand()&0xffff)/65535.0;
  return 0;
}

static void _casneon_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->clock-=elapsed)<=0.0) {
    if (SPRITE->lit) SPRITE->lit=0;
    else SPRITE->lit=casneon_random_color();
    SPRITE->clock+=casneon_random_interval();
  }
}

static void _casneon_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  uint32_t color=0x808080ff;
  if (SPRITE->lit) color=SPRITE->lit; // Don't use (lit) if zero! Its alpha is significant too.
  graf_fancy(&g.graf,x,y,sprite->tileid+(SPRITE->lit?1:0),0,0,NS_sys_tilesize,0,color);
}

const struct sprite_type sprite_type_casneon={
  .name="casneon",
  .objlen=sizeof(struct sprite_casneon),
  .init=_casneon_init,
  .update=_casneon_update,
  .render=_casneon_render,
};

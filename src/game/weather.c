#include "bellacopia.h"

#define EQSPEED 2.000
#define EQDURATION 0.500

/* Start earthquake.
 */
 
void bm_start_earthquake(char dir) {
  double dx=0.0,dy=0.0;
  switch (dir) {
    case 'L': dx=1.0; break;
    case 'R': dx=-1.0; break;
    case 'U': dy=1.0; break;
    case 'D': dy=-1.0; break;
    default: return;
  }
  bm_sound(RID_sound_earthquake);
  g.eqclock=EQDURATION;
  g.eqdx=dx;
  g.eqdy=dy;
}

/* Advance earthquake.
 */
 
void bm_update_earthquake(double elapsed) {
  if (g.eqclock<=0.0) return;
  g.eqclock-=elapsed;
  struct sprite **spritep=GRP(moveable)->sprv;
  int i=GRP(moveable)->sprc;
  for (;i-->0;spritep++) {
    struct sprite *sprite=*spritep;
    if (sprite->defunct) continue;
    sprite_move(sprite,g.eqdx*EQSPEED*elapsed,g.eqdy*EQSPEED*elapsed);
  }
}

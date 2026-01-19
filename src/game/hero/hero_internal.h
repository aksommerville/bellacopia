#ifndef HERO_INTERNAL_H
#define HERO_INTERNAL_H

#include "game/bellacopia.h"

struct sprite_hero {
  struct sprite hdr;
  int facedx,facedy; // Always a cardinal unit vector.
  int indx,indy; // State of gamepad. -1..1.
  int item_blackout; // Wait for SOUTH to release.
  uint8_t itemid_in_progress;
  int walking;
  double walkanimclock;
  int walkanimframe;
};

#define SPRITE ((struct sprite_hero*)sprite)

void hero_render(struct sprite *sprite,int x,int y);

// hero_motion.c
void hero_motion_update(struct sprite *sprite,double elapsed);

// hero_item.c
void hero_item_update(struct sprite *sprite,double elapsed);

#endif

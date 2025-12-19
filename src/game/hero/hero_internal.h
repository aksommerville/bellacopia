#ifndef HERO_INTERNAL_H
#define HERO_INTERNAL_H

#include "game/game.h"
#include "game/sprite/sprite.h"
#include "game/world/camera.h"

struct sprite_hero {
  struct sprite hdr;
  int facedx,facedy; // Always cardinal, never zero.
  int indx,indy; // Digested dpad state.
  int walking;
  int qx,qy; // Plane meters, quantized.
};

#define SPRITE ((struct sprite_hero*)sprite)

void hero_update_motion(struct sprite *sprite,double elapsed);
void hero_check_qpos(struct sprite *sprite);

#endif

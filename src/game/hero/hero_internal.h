#ifndef HERO_INTERNAL_H
#define HERO_INTERNAL_H

#include "game/game.h"
#include "game/sprite/sprite.h"

struct sprite_hero {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_hero*)sprite)

#endif

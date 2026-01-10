#include "game/game.h"
#include "game/sprite/sprite.h"

struct sprite_candy {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_candy*)sprite)

static int _candy_init(struct sprite *sprite) {
  return 0;
}

const struct sprite_type sprite_type_candy={
  .name="candy",
  .objlen=sizeof(struct sprite_candy),
  .init=_candy_init,
};

#include "game/bellacopia.h"

struct sprite_candy {
  struct sprite hdr;
  double ttl;
  int visible;
};

#define SPRITE ((struct sprite_candy*)sprite)

static int _candy_init(struct sprite *sprite) {
  SPRITE->ttl=10.0;
  SPRITE->visible=1;
  return 0;
}

static void _candy_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->ttl-=elapsed)<=0.0) {
    sprite_kill_soon(sprite);
  } else if (SPRITE->ttl<2.0) {
    int visible=((int)(SPRITE->ttl*10.0))&1;
    if (visible!=SPRITE->visible) {
      if (SPRITE->visible=visible) {
        sprite_group_add(GRP(visible),sprite);
      } else {
        sprite_group_remove(GRP(visible),sprite);
      }
    }
  }
}
 
const struct sprite_type sprite_type_candy={
  .name="candy",
  .objlen=sizeof(struct sprite_candy),
  .init=_candy_init,
  .update=_candy_update,
};

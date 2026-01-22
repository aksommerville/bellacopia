#include "game/bellacopia.h"

struct sprite_candy {
  struct sprite hdr;
};

#define SPRITE ((struct sprite_candy*)sprite)

/* Type definition.
 */
 
const struct sprite_type sprite_type_candy={
  .name="candy",
  .objlen=sizeof(struct sprite_candy),
};

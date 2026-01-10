#include "game/game.h"
#include "sprite.h"

struct sprite_npc {
  struct sprite hdr;
  int activity;
  double cooldown;
};

#define SPRITE ((struct sprite_npc*)sprite)

static int _npc_init(struct sprite *sprite) {
  SPRITE->activity=(sprite->arg[0]<<8)|sprite->arg[1];
  return 0;
}

static void _npc_update(struct sprite *sprite,double elapsed) {
  if (SPRITE->cooldown>0.0) SPRITE->cooldown-=elapsed;
}

static void _npc_collide(struct sprite *sprite,struct sprite *other) {
  if (SPRITE->cooldown>0.0) return;
  if (other&&(other->type==&sprite_type_hero)) {
    SPRITE->cooldown=0.500;
    bm_begin_activity(SPRITE->activity,sprite);
  }
}

const struct sprite_type sprite_type_npc={
  .name="npc",
  .objlen=sizeof(struct sprite_npc),
  .init=_npc_init,
  .update=_npc_update,
  .collide=_npc_collide,
};

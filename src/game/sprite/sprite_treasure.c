#include "game/game.h"
#include "sprite.h"

struct sprite_treasure {
  struct sprite hdr;
  uint8_t itemid;
  uint8_t quantity;
  uint16_t fld;
  uint8_t tileid0;
};

#define SPRITE ((struct sprite_treasure*)sprite)

/* Init.
 */
 
static int _treasure_init(struct sprite *sprite) {
  SPRITE->itemid=sprite->arg[0];
  SPRITE->quantity=sprite->arg[1];
  SPRITE->fld=(sprite->arg[2]<<8)|sprite->arg[3];
  SPRITE->tileid0=sprite->tileid;
  
  if (SPRITE->fld) { // If (fld) provided, that explicitly controls our presence.
    if (store_get(SPRITE->fld,1)) return -1;
  } else if (!SPRITE->itemid) { // Itemid zero is weird and probably shouldn't be used. But if so, we're present and empty.
    sprite->tileid=SPRITE->tileid0+1;
    sprite->solid=0;
  } else if (inventory_search(SPRITE->itemid)) return -1; // Already have the item? Nix us.
  
  return 0;
}

/* Collide.
 */
 
static void _treasure_collide(struct sprite *sprite,struct sprite *other) {
  if (other->type==&sprite_type_hero) {
    if (inventory_acquire(SPRITE->itemid,SPRITE->quantity)) {
      store_set(SPRITE->fld,1,1);
      sprite->tileid=SPRITE->tileid0+1;
      sprite->solid=0;
    }
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_treasure={
  .name="treasure",
  .objlen=sizeof(struct sprite_treasure),
  .init=_treasure_init,
  .collide=_treasure_collide,
};

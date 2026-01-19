#include "game/bellacopia.h"

struct sprite_treasure {
  struct sprite hdr;
  uint8_t tileid0;
  int itemid,quantity,fld;
};

#define SPRITE ((struct sprite_treasure*)sprite)

/* Indicate I've been collected.
 */
 
static void treasure_set_collected(struct sprite *sprite) {
  sprite->tileid=SPRITE->tileid0+1;
  sprite_group_remove(GRP(solid),sprite);
}

/* Init.
 */
 
static int _treasure_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->itemid=sprite->arg[0];
  SPRITE->quantity=sprite->arg[1];
  SPRITE->fld=(sprite->arg[2]<<8)|sprite->arg[3];
  
  // If a field is specified, it alone drives our collected state. 0=full, 1=empty.
  // Chests of quantity-bearing items or non-inventory items should always use this.
  if (SPRITE->fld) {
    if (store_get_fld(SPRITE->fld)) {
      treasure_set_collected(sprite);
    }
    
  // Otherwise, we're full if they don't have the item yet. Don't care about quantity, just is it listed in inventory.
  } else {
    struct invstore *invstore=store_get_itemid(SPRITE->itemid);
    if (invstore) {
      treasure_set_collected(sprite);
    }
  }
  
  return 0;
}

/* Collide.
 */
 
static void _treasure_collide(struct sprite *sprite,struct sprite *other) {
  if (other->type==&sprite_type_hero) {
    if (sprite->tileid!=SPRITE->tileid0) return; // Already collected.
    if (game_get_item(SPRITE->itemid,SPRITE->quantity)) {
      store_set_fld(SPRITE->fld,1);
      treasure_set_collected(sprite);
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

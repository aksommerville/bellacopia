#include "game/bellacopia.h"

struct sprite_jigpiece {
  struct sprite hdr;
  int mapid;
};

#define SPRITE ((struct sprite_jigpiece*)sprite)

/* Init.
 */
 
static int _jigpiece_init(struct sprite *sprite) {

  // If we can't get the mapid, or already have this piece, request to disappear.
  struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  if (map->parent) {
    if (!(map=map_by_id(map->parent))) return -1;
  }
  if (!map->rid) return -1;
  if (store_get_jigstore(map->rid)) return -1;
  SPRITE->mapid=map->rid;
  
  return 0;
}

/* Collide.
 */
 
static void _jigpiece_collide(struct sprite *sprite,struct sprite *other) {
  if (other->type==&sprite_type_hero) {
    if (game_get_item(NS_itemid_jigpiece,SPRITE->mapid)) {
      sprite_kill_soon(sprite);
    }
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_jigpiece={
  .name="jigpiece",
  .objlen=sizeof(struct sprite_jigpiece),
  .init=_jigpiece_init,
  .collide=_jigpiece_collide,
};

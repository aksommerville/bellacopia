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

/* Update.
 */
 
static void _jigpiece_update(struct sprite *sprite,double elapsed) {
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    const double radius=0.750;
    double dx=hero->x-sprite->x;
    if ((dx>=-radius)&&(dx<=radius)) {
      double dy=hero->y-sprite->y;
      if ((dy>=-radius)&&(dy<=radius)) {
        if (game_get_item(NS_itemid_jigpiece,SPRITE->mapid)) {
          sprite_kill_soon(sprite);
        }
      }
    }
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_jigpiece={
  .name="jigpiece",
  .objlen=sizeof(struct sprite_jigpiece),
  .init=_jigpiece_init,
  .update=_jigpiece_update,
};

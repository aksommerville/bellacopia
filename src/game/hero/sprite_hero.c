#include "hero_internal.h"

/* Cleanup.
 */
 
static void _hero_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _hero_init(struct sprite *sprite) {
  SPRITE->facedy=1;
  SPRITE->item_blackout=1;
  return 0;
}

/* Update.
 */
 
static void _hero_update(struct sprite *sprite,double elapsed) {
  hero_item_update(sprite,elapsed);
  hero_motion_update(sprite,elapsed);
}

/* Injure. Public.
 */
 
void hero_injure(struct sprite *sprite,struct sprite *assailant) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return;
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);
}

/* Collide.
 */
 
static void _hero_collide(struct sprite *sprite,struct sprite *other) {
  if (sprite_group_has(GRP(hazard),other)) {
    hero_injure(sprite,other);
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_hero={
  .name="hero",
  .objlen=sizeof(struct sprite_hero),
  .del=_hero_del,
  .init=_hero_init,
  .update=_hero_update,
  .render=hero_render,
  .collide=_hero_collide,
};

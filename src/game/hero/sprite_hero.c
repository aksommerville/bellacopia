#include "hero_internal.h"

/* Cleanup.
 */
 
static void _hero_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _hero_init(struct sprite *sprite) {
  SPRITE->facedy=1;
  return 0;
}

/* Update.
 */
 
static void _hero_update(struct sprite *sprite,double elapsed) {
  hero_update_motion(sprite,elapsed);
}

/* Render.
 */
 
static void _hero_render(struct sprite *sprite,int dstx,int dsty) {
  graf_set_image(&g.graf,sprite->imageid);
  uint8_t tileid=sprite->tileid;
  uint8_t xform=0;
  if (SPRITE->facedx<0) tileid+=0x02;
  else if (SPRITE->facedx>0) { tileid+=0x02; xform=EGG_XFORM_XREV; }
  else if (SPRITE->facedy<0) tileid+=0x01;
  graf_tile(&g.graf,dstx,dsty,tileid,xform);
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_hero={
  .name="hero",
  .objlen=sizeof(struct sprite_hero),
  .del=_hero_del,
  .init=_hero_init,
  .update=_hero_update,
  .render=_hero_render,
};

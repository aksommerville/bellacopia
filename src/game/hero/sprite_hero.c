#include "hero_internal.h"

/* Cleanup.
 */
 
static void _hero_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _hero_init(struct sprite *sprite) {
  SPRITE->facedy=1;
  
  // Initializing (qx,qy) prevents the cell we're born on from triggering. Important for doors.
  SPRITE->qx=(int)sprite->x; if (sprite->x<0.0) SPRITE->qx--;
  SPRITE->qy=(int)sprite->y; if (sprite->y<0.0) SPRITE->qy--;
  
  return 0;
}

/* Update.
 */
 
static void _hero_update(struct sprite *sprite,double elapsed) {
  hero_update_motion(sprite,elapsed);
  hero_check_qpos(sprite);
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.200;
    if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
  }
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
  
  if (SPRITE->walking) {
    switch (SPRITE->animframe) {
      case 1: tileid+=0x10; break;
      case 3: tileid+=0x20; break;
    }
  }
  
  /* If there's an item equipped, draw it in my hand.
   * Dot's tile shifts 3 to right, and we overlay one at an offset.
   * If facing up, draw the overlay first.
   */
  int itemdx=0,itemdy=0;
  uint8_t itemxform=0;
  uint8_t itemtile=hand_tileid_for_item(g.equipped.itemid,g.equipped.quantity);
  if (itemtile) {
    if (SPRITE->facedy<0) { // Draw it now.
      graf_tile(&g.graf,dstx+7,dsty-1,itemtile,EGG_XFORM_XREV);
      itemtile=0;
      tileid+=3;
    } else if (SPRITE->facedy>0) {
      itemdx=-6;
    } else if (SPRITE->facedx<0) {
      itemdx=-1;
      itemdy=1;
      itemxform=EGG_XFORM_XREV;
    } else if (SPRITE->facedx>0) {
      itemdx=1;
      itemdy=1;
    } else {
      itemtile=0;
    }
  }
  if (itemtile) tileid+=3;
  
  graf_tile(&g.graf,dstx,dsty,tileid,xform);
  if (itemtile) graf_tile(&g.graf,dstx+itemdx,dsty+itemdy,itemtile,itemxform);
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

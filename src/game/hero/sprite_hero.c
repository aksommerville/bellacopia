#include "hero_internal.h"

/* Cleanup.
 */
 
static void _hero_del(struct sprite *sprite) {
  //fprintf(stderr,"%p %s\n",sprite,__func__);
}

/* Init.
 */
 
static int _hero_init(struct sprite *sprite) {
  //fprintf(stderr,"%p %s\n",sprite,__func__);
  SPRITE->facedy=1;
  SPRITE->item_blackout=1;
  
  // Initializing (qx,qy) prevents the cell we're born on from triggering. Important for doors.
  SPRITE->qx=(int)sprite->x; if (sprite->x<0.0) SPRITE->qx--;
  SPRITE->qy=(int)sprite->y; if (sprite->y<0.0) SPRITE->qy--;
  
  return 0;
}

/* Update.
 */
 
static void _hero_update(struct sprite *sprite,double elapsed) {
  hero_update_item(sprite,elapsed);
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
  if ((g.equipped.itemid==NS_itemid_match)&&(SPRITE->matchclock>0.0)) { // If a match is light, draw that. NB even if quantity is zero.
    itemtile=0x84+(SPRITE->animframe&1);
  }
  if (itemtile) {
    if (SPRITE->facedy<0) { // Draw it now.
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
  if (itemtile) {
    tileid+=3;
    if ((itemtile==0x71)&&hero_roots_present(sprite)) { // Divining Rod. If roots are present, animate.
      int frame=(g.framec/3)%6;
      switch (frame) {
        case 0: itemtile=0x86; break;
        case 1: itemtile=0x87; break;
        case 2: itemtile=0x88; break;
        case 3: itemtile=0x89; break;
        case 4: itemtile=0x88; break;
        case 5: itemtile=0x87; break;
      }
    }
    if (SPRITE->facedy<0) { // Facing north, draw the item first.
      graf_tile(&g.graf,dstx+7,dsty-1,itemtile,EGG_XFORM_XREV);
      itemtile=0;
    }
  }
  
  graf_tile(&g.graf,dstx,dsty,tileid,xform);
  if (itemtile) graf_tile(&g.graf,dstx+itemdx,dsty+itemdy,itemtile,itemxform);
  
  // Bugspray indicator.
  if (g.bugspray>0.0) {
    uint8_t bstileid=0x60+((g.framec/4)&7);
    if (g.bugspray<1.0) {
      int alpha=(int)(g.bugspray*255.0);
      if (alpha<0) alpha=0; else if (alpha>0xff) alpha=0xff;
      graf_set_alpha(&g.graf,alpha);
    }
    graf_tile(&g.graf,dstx,dsty-NS_sys_tilesize,bstileid,0);
    graf_set_alpha(&g.graf,0xff);
  }
  
  // Divining Rod indicator.
  if ((g.equipped.itemid==NS_itemid_divining_rod)&&(g.input[0]&EGG_BTN_SOUTH)) {
    uint8_t drtileid=SPRITE->onroot?0x69:0x68;
    graf_tile(&g.graf,dstx,dsty-NS_sys_tilesize,drtileid,0);
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
  .render=_hero_render,
};

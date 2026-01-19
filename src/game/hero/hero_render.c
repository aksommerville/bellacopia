#include "hero_internal.h"

/* Errata: Bugspray indicator and similar.
 */
 
static void hero_render_errata(struct sprite *sprite,int x,int y) {
  //TODO Bugspray.
  //TODO Compass.
}

/* Riding broom, complete replacement.
 */
 
static void hero_render_broom(struct sprite *sprite,int x,int y) {
  //TODO
}

/* Encoding on wand, complete replacement.
 */
 
static void hero_render_wand(struct sprite *sprite,int x,int y) {
  //TODO
}

/* Fishing, complete replacement.
 */
 
static void hero_render_fishpole(struct sprite *sprite,int x,int y) {
  //TODO
}

/* Using hookshot, complete replacement.
 */
 
static void hero_render_hookshot(struct sprite *sprite,int x,int y) {
  //TODO
}

/* Render, main entry point.
 */
 
void hero_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  
  /* Some items are a completely separate thing when engaged.
   */
  switch (SPRITE->itemid_in_progress) {
    case NS_itemid_broom: hero_render_broom(sprite,x,y); return;
    case NS_itemid_wand: hero_render_wand(sprite,x,y); return;
    case NS_itemid_fishpole: hero_render_fishpole(sprite,x,y); return;
    case NS_itemid_hookshot: hero_render_hookshot(sprite,x,y); return;
  }
  
  uint8_t itemtileid=0;
  const struct item_detail *detail=item_detail_for_equipped();
  if (detail) {
    itemtileid=detail->hand_tileid;
  }
  
  uint8_t tileid=sprite->tileid;
  uint8_t xform=0;
  uint8_t itemxform=0;
  int itemfirst=0,itemdx=0,itemdy=0;
  if (SPRITE->facedx<0) {
    tileid+=2;
    itemdx=-1;
    itemdy=1;
  } else if (SPRITE->facedx>0) {
    tileid+=2;
    xform=EGG_XFORM_XREV;
    itemdx=1;
    itemdy=1;
    itemxform=EGG_XFORM_XREV;
  } else if (SPRITE->facedy<0) {
    tileid+=1;
    itemfirst=1;
    itemdx=7;
    itemdy=-1;
    itemxform=EGG_XFORM_XREV;
  } else {
    itemdx=-6;
  }
  
  /* A few items have in-hand animation. Doesn't affect the broader layout.
   */
  if (itemtileid==0x71) { //TODO Divining Rod: If a root is detected, animate spinning. 0x86..0x89 pingponging
  } else if (itemtileid==0x74) { //TODO Match: If lit, animate 0x84,0x85
  } else if ((itemtileid==0x73)&&SPRITE->facedx) { // Fishpole: Natural orientation horizontally conflicts with Dot's face. Turn it around.
    itemxform^=EGG_XFORM_XREV;
  }
  
  if (itemtileid) {
    tileid+=3;
  }
  
  if (SPRITE->walking) {
    switch (SPRITE->walkanimframe) {
      case 1: tileid+=0x10; break;
      case 3: tileid+=0x20; break;
    }
  }
  
  if (!itemtileid) {
    graf_tile(&g.graf,x,y,tileid,xform);
  } else if (itemfirst) {
    graf_tile(&g.graf,x+itemdx,y+itemdy,itemtileid,itemxform);
    graf_tile(&g.graf,x,y,tileid,xform);
  } else {
    graf_tile(&g.graf,x,y,tileid,xform);
    graf_tile(&g.graf,x+itemdx,y+itemdy,itemtileid,itemxform);
  }
  
  hero_render_errata(sprite,x,y);
}

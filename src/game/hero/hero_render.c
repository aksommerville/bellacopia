#include "hero_internal.h"

/* Errata: Bugspray indicator and similar.
 */
 
static void hero_render_errata(struct sprite *sprite,int x,int y) {

  /* Bugspray indicator.
   */
  if (g.bugspray>0.0) {
    int frame=((int)(g.bugspray*6.0))&7;
    const double fadetime=1.000;
    if (g.bugspray<fadetime) {
      int alpha=(int)((g.bugspray*255.0)/fadetime);
      if (alpha<0xff) graf_set_alpha(&g.graf,alpha);
    }
    graf_tile(&g.graf,x,y-NS_sys_tilesize,0x60+frame,0);
    graf_set_alpha(&g.graf,0xff);
  }
  
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

/* Quaffing a potion, complete replacement.
 */
 
static void hero_render_potion(struct sprite *sprite,int x,int y) {
  uint8_t bodytileid=0x19;
  uint8_t bodyxform=0;
  uint8_t bottletileid=0x7a;
  uint8_t bottlexform=0;
  int bottlefirst=0,bottledx=0,bottledy=0;
  if (SPRITE->facedx<0) {
    bodytileid+=2;
    bottlexform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
    bottledx=-3;
    bottledy=3;
  } else if (SPRITE->facedx>0) {
    bodytileid+=2;
    bodyxform=EGG_XFORM_XREV;
    bottlexform=EGG_XFORM_SWAP;
    bottledx=3;
    bottledy=3;
  } else if (SPRITE->facedy<0) {
    bodytileid+=1;
    bottlefirst=1;
    bottlexform=EGG_XFORM_SWAP;
    bottledx=2;
    bottledy=2;
  } else {
    bottlexform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
    bottledx=1;
    bottledy=3;
  }
  if (g.store.invstorev[0].itemid==NS_itemid_potion) { // Technically possible to change items mid-glug, but just let it be the wrong tileid briefly then.
    // Quantity 0 and 3 are both possible -- the quantity changes during the animation.
    switch (g.store.invstorev[0].quantity) {
      case 0: bottletileid=0x8a; break;
      // 1 is the default
      case 2: bottletileid=0x8b; break;
      case 3: bottletileid=0x8c; break;
    }
  }
  if (bottlefirst) {
    graf_tile(&g.graf,x+bottledx,y+bottledy,bottletileid,bottlexform);
    graf_tile(&g.graf,x,y,bodytileid,bodyxform);
  } else {
    graf_tile(&g.graf,x,y,bodytileid,bodyxform);
    graf_tile(&g.graf,x+bottledx,y+bottledy,bottletileid,bottlexform);
  }
  hero_render_errata(sprite,x,y);
}

/* Render, main entry point.
 */
 
void hero_render(struct sprite *sprite,int x,int y) {

  /* When creamed to vanishment, we strobe 1/2.
   */
  if (g.vanishing>0.0) {
    if (g.framec&1) return;
  }

  graf_set_image(&g.graf,sprite->imageid);
  
  /* Some items are a completely separate thing when engaged.
   */
  switch (SPRITE->itemid_in_progress) {
    case NS_itemid_broom: hero_render_broom(sprite,x,y); return;
    case NS_itemid_wand: hero_render_wand(sprite,x,y); return;
    case NS_itemid_fishpole: hero_render_fishpole(sprite,x,y); return;
    case NS_itemid_hookshot: hero_render_hookshot(sprite,x,y); return;
    case NS_itemid_potion: hero_render_potion(sprite,x,y); return;
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
  
  /* A few items make exceptions to tileid or xform.
   * Doesn't affect the broader layout.
   */
  if (itemtileid==0x71) { //TODO Divining Rod: If a root is detected, animate spinning. 0x86..0x89 pingponging
  } else if (itemtileid==0x74) { //TODO Match: If lit, animate 0x84,0x85
  } else if ((itemtileid==0x73)&&SPRITE->facedx) { // Fishpole: Natural orientation horizontally conflicts with Dot's face. Turn it around.
    itemxform^=EGG_XFORM_XREV;
  } else if (itemtileid==0x7a) { // Potion: 4 tiles indicating quantity.
    switch (g.store.invstorev[0].quantity) {
      case 0: itemtileid=0x8a; break;
      // The natural tile 0x7a is quantity==1.
      case 2: itemtileid=0x8b; break;
      case 3: itemtileid=0x8c; break;
    }
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

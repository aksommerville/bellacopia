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

/* Render bits.
 */
 
static void hero_render_bugspray_maybe(struct sprite *sprite,int dstx,int dsty) {
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
}

static void hero_render_spell(struct sprite *sprite,int dstx,int dsty) {
  // It always goes above your head.
  // In theory, you might be on an edge-clamping plane right at the north end, but that really doesn't come up much.
  dsty-=NS_sys_tilesize;
  // 0x6a..0x6d are the arrow (L,R,T,B). Don't use xforms, because width is odd.
  // 0x7a,0x7b,0x7c are the scroll. Width 9. One scroll unit per spell unit, but no fewer than 2.
  int arrowsw=SPRITE->spellc*6-1; // tighest bound around the arrow'd pixels (5 per arrow plus 1 space between).
  if (arrowsw<0) arrowsw=0;
  
  if (arrowsw) { // Body of the banner is raw rectangles.
    int bannerx=dstx-(arrowsw>>1);
    int bannery=dsty-5;
    graf_fill_rect(&g.graf,bannerx,bannery,arrowsw,9,0x000000ff);
    graf_fill_rect(&g.graf,bannerx,bannery+1,arrowsw,7,0xa18c75ff);
  }
  graf_set_image(&g.graf,RID_image_hero);
  graf_tile(&g.graf,dstx-(arrowsw>>1)+2,dsty,0x6e,0);
  graf_tile(&g.graf,dstx-(arrowsw>>1)+arrowsw-1,dsty,0x6f,0);
  int x=dstx-(arrowsw>>1)+3;
  int i=0;
  for (;i<SPRITE->spellc;i++,x+=6) {
    uint8_t tileid;
    switch (SPRITE->spell[i]) {
      case 0x40: tileid=0x6c; break;
      case 0x10: tileid=0x6a; break;
      case 0x08: tileid=0x6b; break;
      case 0x02: tileid=0x6d; break;
      default: continue;
    }
    graf_tile(&g.graf,x,dsty,tileid,0);
  }
}

/* Render.
 */
 
static void _hero_render(struct sprite *sprite,int dstx,int dsty) {
  graf_set_image(&g.graf,sprite->imageid);
  uint8_t tileid=sprite->tileid;
  uint8_t xform=0;
  
  /* Wand in progress is its own thing.
   */
  if (SPRITE->itemid_in_progress==NS_itemid_wand) {
    uint8_t tileid=0x40;
    int extrax=0,extray=0;
    switch (SPRITE->wanddir) {
      case 0x40: tileid+=3; extray=-1; break;
      case 0x10: tileid+=1; extrax=-1; break;
      case 0x08: tileid+=2; extrax=1; break;
      case 0x02: tileid+=4; extray=1; break;
    }
    graf_tile(&g.graf,dstx,dsty,tileid,0);
    if (extrax||extray) graf_tile(&g.graf,dstx+extrax*NS_sys_tilesize,dsty+extray*NS_sys_tilesize,tileid+0x10,0);
    hero_render_bugspray_maybe(sprite,dstx,dsty);
    hero_render_spell(sprite,dstx,dsty);
    return;
  }
  
  /* Likewise hookshot.
   */
  if (SPRITE->itemid_in_progress==NS_itemid_hookshot) {
    uint8_t hookxform; // Natural orientation of hook and chain is down.
    int hookx=dstx+SPRITE->facedx*(int)(SPRITE->hookdistance*NS_sys_tilesize);
    int hooky=dsty+SPRITE->facedy*(int)(SPRITE->hookdistance*NS_sys_tilesize);
    tileid+=6;
    if (SPRITE->facedx<0) {
      tileid+=0x02;
      hookxform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
      hooky+=4;
    } else if (SPRITE->facedx>0) {
      tileid+=0x02;
      xform=EGG_XFORM_XREV;
      hookxform=EGG_XFORM_SWAP;
      hooky+=4;
    } else if (SPRITE->facedy<0) {
      tileid+=0x01;
      hookxform=EGG_XFORM_YREV;
      hookx+=3;
    } else {
      hookxform=0;
      hookx-=3;
    }
    // Chain begins right at the hook, and continues until we reach the hero.
    const int chainspacing=7;
    int chainx=hookx,chainy=hooky;
    for (;;) {
      graf_tile(&g.graf,chainx,chainy,0x26,hookxform);
      if (SPRITE->facedx<0) {
        if ((chainx+=chainspacing)>=dstx) break;
      } else if (SPRITE->facedx>0) {
        if ((chainx-=chainspacing)<=dstx) break;
      } else if (SPRITE->facedy<0) {
        if ((chainy+=chainspacing)>=dsty) break;
      } else {
        if ((chainy-=chainspacing)<=dsty) break;
      }
    }
    // And then the two foreground tiles, and any decoration on top.
    graf_tile(&g.graf,dstx,dsty,tileid,xform);
    graf_tile(&g.graf,hookx,hooky,(SPRITE->hookstage==1)?0x27:0x28,hookxform); // open jaw leaving, closed otherwise
    hero_render_bugspray_maybe(sprite,dstx,dsty);
    return;
  }
  
  /* Most situations...
   */
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
  
  hero_render_bugspray_maybe(sprite,dstx,dsty);
  
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

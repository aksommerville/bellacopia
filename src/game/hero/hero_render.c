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
  
  /* Compass.
   */
  if (g.store.invstorev[0].itemid==NS_itemid_compass) {
    if (g.framec%30<22) {
      if (SPRITE->compassx>=0.0) {
        int cx=x+(int)(sin(SPRITE->compasst)*NS_sys_tilesize);
        int cy=y+(int)(cos(SPRITE->compasst)*NS_sys_tilesize);
        graf_tile(&g.graf,cx,cy,0x7f,0);
      }
    }
  }
  
  /* Divining Rod alerts.
   */
  if (SPRITE->divining_alert_clock>0.0) {
    const double fadetime=0.250;
    if (SPRITE->divining_alert_clock<fadetime) {
      int alpha=(int)((SPRITE->divining_alert_clock*255.0)/fadetime);
      if (alpha<0xff) graf_set_alpha(&g.graf,alpha);
    }
    const struct divining_alert *alert=SPRITE->divining_alertv;
    int i=9;
    for (;i-->0;alert++) {
      if (!alert->tileid) continue;
      graf_tile(&g.graf,alert->x-g.camera.rx,alert->y-g.camera.ry,alert->tileid,0);
    }
    graf_set_alpha(&g.graf,0xff);
  }
}

/* Riding broom, complete replacement.
 */
 
static void hero_render_broom(struct sprite *sprite,int x,int y) {
  int elevation=(g.framec&16)?-1:0;
  graf_tile(&g.graf,x+SPRITE->broomdx,y+8,0x0b+elevation,0); // shadow
  uint8_t tileid=0x0c;
  if (SPRITE->indx) tileid=(g.framec&8)?0x1c:0x2c;
  if (SPRITE->broomdx>0) {
    graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y+elevation,tileid+1,EGG_XFORM_XREV);
    graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y+elevation,tileid,EGG_XFORM_XREV);
  } else {
    graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y+elevation,tileid,0);
    graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y+elevation,tileid+1,0);
  }
}

/* Encoding on wand, complete replacement.
 */
 
static void hero_render_wand(struct sprite *sprite,int x,int y) {

  // Dot with the wand, possibly an extension wand tile.
  uint8_t tileid=0x40;
  uint8_t wtileid=0;
  int wdx=0,wdy=0;
  if (SPRITE->spellrejectclock>0.0) {
    tileid+=5;
  } else switch (SPRITE->wanddir) {
    case 'U': tileid+=3; wtileid=tileid+0x10; wdy=-NS_sys_tilesize; break;
    case 'L': tileid+=1; wtileid=tileid+0x10; wdx=-NS_sys_tilesize; break;
    case 'R': tileid+=2; wtileid=tileid+0x10; wdx=NS_sys_tilesize; break;
    case 'D': tileid+=4; wtileid=tileid+0x10; wdy=NS_sys_tilesize; break;
  }
  graf_tile(&g.graf,x,y,tileid,0);
  if (wtileid) graf_tile(&g.graf,x+wdx,y+wdy,wtileid,0);
  
  // Scroll showing the input so far.
  int dsty=y-NS_sys_tilesize;
  // 0x6a..0x6d are the arrow (L,R,T,B). Don't use xforms, because width is odd.
  // 0x7a,0x7b,0x7c are the scroll. Width 9. One scroll unit per spell unit, but no fewer than 2.
  int arrowsw=SPRITE->spellc*6-1; // tighest bound around the arrow'd pixels (5 per arrow plus 1 space between).
  if (arrowsw<0) arrowsw=0;
  if (arrowsw) { // Body of the banner is raw rectangles.
    int bannerx=x-(arrowsw>>1);
    int bannery=dsty-5;
    graf_fill_rect(&g.graf,bannerx,bannery,arrowsw,9,0x000000ff);
    graf_fill_rect(&g.graf,bannerx,bannery+1,arrowsw,7,0xa18c75ff);
  }
  graf_set_image(&g.graf,RID_image_hero);
  graf_tile(&g.graf,x-(arrowsw>>1)+2,dsty,0x6e,0);
  graf_tile(&g.graf,x-(arrowsw>>1)+arrowsw-1,dsty,0x6f,0);
  int dstx=x-(arrowsw>>1)+3;
  int i=0;
  for (;i<SPRITE->spellc;i++,dstx+=6) {
    uint8_t tileid;
    switch (SPRITE->spell[i]) {
      case 'U': tileid=0x6c; break;
      case 'L': tileid=0x6a; break;
      case 'R': tileid=0x6b; break;
      case 'D': tileid=0x6d; break;
      case '.': tileid=0x7e; break;
      default: continue;
    }
    graf_tile(&g.graf,dstx,dsty,tileid,0);
  }
  
  hero_render_errata(sprite,x,y);
}

/* Fishing, complete replacement.
 */
 
static void hero_render_fishpole(struct sprite *sprite,int x,int y) {
  int frame=(g.framec&32)?1:0;
  uint8_t dottileid,poletileid,xform=0;
  int fishx0=x+SPRITE->facedx*NS_sys_tilesize;
  int fishy0=y+SPRITE->facedy*NS_sys_tilesize;
  if (SPRITE->facedx<0) {
    dottileid=0x3d;
    poletileid=0x3c;
    if (SPRITE->fish) { dottileid+=0x20; poletileid+=0x20; }
    else if (frame) { dottileid+=0x10; poletileid+=0x10; }
  } else if (SPRITE->facedx>0) {
    dottileid=0x3d;
    poletileid=0x3c;
    if (SPRITE->fish) { dottileid+=0x20; poletileid+=0x20; }
    else if (frame) { dottileid+=0x10; poletileid+=0x10; }
    xform=EGG_XFORM_XREV;
  } else if (SPRITE->facedy<0) {
    dottileid=0x49;
    poletileid=0x39;
    if (SPRITE->fish) { dottileid+=2; poletileid+=2; }
    else if (frame) { dottileid+=1; poletileid+=1; }
  } else {
    dottileid=0x36;
    poletileid=0x46;
    // small orientation shift when facing down and caught
    if (SPRITE->fish) { dottileid=0x38; poletileid=0x48; y-=NS_sys_tilesize; }
    else if (frame) { dottileid+=1; poletileid+=1; }
  }
  graf_tile(&g.graf,x,y,dottileid,xform);
  graf_tile(&g.graf,x+SPRITE->facedx*NS_sys_tilesize,y+SPRITE->facedy*NS_sys_tilesize,poletileid,xform);
  hero_render_errata(sprite,x,y);
  
  if (SPRITE->fish) {
    const struct item_detail *detail=item_detail_for_itemid(SPRITE->fish);
    if (detail&&detail->tileid) {
      const double range=-30.0; // px
      int fishy=fishy0+(int)(((FISH_FLY_TIME-SPRITE->fishclock)*range)/FISH_FLY_TIME);
      graf_set_image(&g.graf,RID_image_pause);
      graf_tile(&g.graf,fishx0,fishy,detail->tileid,0);
    }
  }
}

/* Using hookshot, complete replacement.
 */
 
static void hero_render_hookshot_hardware(struct sprite *sprite,int x,int y) {
  const int spacing=8;
  int hx=x+(int)(SPRITE->hookdistance*NS_sys_tilesize*SPRITE->facedx);
  int hy=y+(int)(SPRITE->hookdistance*NS_sys_tilesize*SPRITE->facedy);
  
  if (SPRITE->facedx) hy+=4;
  else if (SPRITE->facedy<0) hx+=3;
  else hx-=3;
  
  uint8_t tileid,xform;
  if (SPRITE->facedx<0) xform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
  else if (SPRITE->facedx>0) xform=EGG_XFORM_SWAP;
  else if (SPRITE->facedy<0) xform=EGG_XFORM_YREV;
  else xform=0;
  if (SPRITE->hookstage>1) tileid=0x28; // Closed if returning or pulling.
  else tileid=0x27;
  graf_tile(&g.graf,hx,hy,tileid,xform);
  
  x+=SPRITE->facedx*8;
  y+=SPRITE->facedy*8;
  for (;;) {
    hx-=SPRITE->facedx*spacing;
    hy-=SPRITE->facedy*spacing;
    if (SPRITE->facedx<0) { if (hx>=x) break; }
    else if (SPRITE->facedx>0) { if (hx<=x) break; }
    else if (SPRITE->facedy<0) { if (hy>=y) break; }
    else { if (hy<=y) break; }
    graf_tile(&g.graf,hx,hy,0x26,0);
  }
}
 
static void hero_render_hookshot(struct sprite *sprite,int x,int y) {

  // Facing north, render the hardware first.
  if (SPRITE->facedy<0) hero_render_hookshot_hardware(sprite,x,y);

  uint8_t tileid,xform=0;
  if (SPRITE->facedx<0) tileid=0x18; 
  else if (SPRITE->facedx>0) { tileid=0x18; xform=EGG_XFORM_XREV; }
  else if (SPRITE->facedy<0) tileid=0x17;
  else tileid=0x16;
  
  graf_tile(&g.graf,x,y,tileid,xform);
  
  // Facing south or horizontal, render the hardware second.
  if (SPRITE->facedy>=0) hero_render_hookshot_hardware(sprite,x,y);
  
  hero_render_errata(sprite,x,y);
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

/* Hurt.
 */
 
static void hero_render_hurt(struct sprite *sprite,int x,int y) {
  uint32_t primary=0xff0000ff;
  if (g.framec&8) primary=0xffff00ff;
  const int dylo=-6,dyhi=-12;
  double norm=SPRITE->hurt/HERO_HURT_TIME;
  int hatdy=(int)(dylo*(1.0-norm)+dyhi*norm);
  graf_fancy(&g.graf,x,y,0x1e,0,0,NS_sys_tilesize,0,primary);
  graf_fancy(&g.graf,x,y+hatdy,0x0e,0,0,NS_sys_tilesize,0,primary);
}

/* Telescope.
 */
 
static void hero_render_telescope(struct sprite *sprite,int x,int y) {
  uint8_t bodytile,scopetile;
  uint8_t xform=0;
  if (SPRITE->facedx<0) {
    bodytile=0x4f;
    scopetile=0x4e;
  } else if (SPRITE->facedx>0) {
    bodytile=0x4f;
    scopetile=0x4e;
    xform=EGG_XFORM_XREV;
  } else if (SPRITE->facedy<0) {
    bodytile=0x3f;
    scopetile=0x2f;
  } else {
    bodytile=0x2e;
    scopetile=0x3e;
  }
  graf_tile(&g.graf,x,y,bodytile,xform);
  graf_tile(&g.graf,x+SPRITE->facedx*NS_sys_tilesize,y+SPRITE->facedy*NS_sys_tilesize,scopetile,xform);
}

/* Bus Stop.
 */
 
static void hero_render_busstop(struct sprite *sprite,int x,int y) {
  graf_tile(&g.graf,x,y,0x09,0);
  graf_tile(&g.graf,x-NS_sys_tilesize,y,0x08,0);
  hero_render_errata(sprite,x,y);
}

/* Tape measure, just the extra bits.
 */
 
static void hero_render_tapemeasure(struct sprite *sprite,int x,int y) {
  int ax=(int)(SPRITE->tapeanchorx*NS_sys_tilesize)-g.camera.rx;
  int ay=(int)(SPRITE->tapeanchory*NS_sys_tilesize)-g.camera.ry;
  graf_set_input(&g.graf,0);
  graf_line(&g.graf,ax,ay,0xffff00ff,x,y,0xffff00ff);
  
  char msg[5];
  int msgc=0;
  int m=(int)SPRITE->tapedistance;
  if (m<0) m=0;
  else if (m>999) m=999;
  if (m>=100) msg[msgc++]='0'+m/100;
  if (m>= 10) msg[msgc++]='0'+(m/10)%10;
  msg[msgc++]='0'+m%10;
  msg[msgc++]=' ';
  msg[msgc++]='m';
  
  graf_set_image(&g.graf,RID_image_fonttiles);
  int texty=y-NS_sys_tilesize;
  int textx=x-(msgc*4)+4;
  int i=0;
  for (;i<msgc;i++,textx+=8) {
    graf_tile(&g.graf,textx,texty,msg[i],0);
  }
  
  graf_set_image(&g.graf,sprite->imageid);
}

/* Marionette. Draw us puppeteering, the marionette itself is its own sprite.
 */
 
static void hero_render_marionette(struct sprite *sprite,int x,int y) {
  uint8_t tileid=0x55;
       if (SPRITE->indx<0) tileid+=3;
  else if (SPRITE->indx>0) tileid+=4;
  else if (SPRITE->indy<0) tileid+=2;
  else if (SPRITE->indy>0) tileid+=1;
  graf_tile(&g.graf,x,y,tileid,0);
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
  
  /* Hurt is a different thing.
   */
  if (SPRITE->hurt>0.0) {
    hero_render_hurt(sprite,x,y);
    return;
  }
  
  /* Some items are a completely separate thing when engaged.
   */
  switch (SPRITE->itemid_in_progress) {
    case NS_itemid_broom: hero_render_broom(sprite,x,y); return;
    case NS_itemid_wand: hero_render_wand(sprite,x,y); return;
    case NS_itemid_fishpole: hero_render_fishpole(sprite,x,y); return;
    case NS_itemid_hookshot: hero_render_hookshot(sprite,x,y); return;
    case NS_itemid_potion: hero_render_potion(sprite,x,y); return;
    case NS_itemid_telescope: hero_render_telescope(sprite,x,y); return;
    case NS_itemid_busstop: hero_render_busstop(sprite,x,y); return;
    case NS_itemid_tapemeasure: hero_render_tapemeasure(sprite,x,y); break; // And proceed with regular update.
    case NS_itemid_marionette: hero_render_marionette(sprite,x,y); return;
  }
  
  /* When the shovel is armed, draw a preview square.
   */
  if (g.store.invstorev[0].itemid==NS_itemid_shovel) {
    int shx=SPRITE->qx*NS_sys_tilesize-g.camera.rx+(NS_sys_tilesize>>1);
    int shy=SPRITE->qy*NS_sys_tilesize-g.camera.ry+(NS_sys_tilesize>>1);
    uint8_t xform=0;
    switch ((g.framec/10)&3) {
      case 1: xform=EGG_XFORM_SWAP|EGG_XFORM_XREV; break;
      case 2: xform=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
      case 3: xform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
    }
    graf_tile(&g.graf,shx,shy,0x00,xform);
  }
  
  /* Acquire the equipped item.
   * If we're walking and blocked, nix it.
   */
  uint8_t itemtileid=0;
  if (SPRITE->walking&&SPRITE->blocked) {
    // Pretend there's no item.
  } else {
    const struct item_detail *detail=item_detail_for_equipped();
    if (detail) {
      itemtileid=detail->hand_tileid;
    }
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
  if (itemtileid==0x71) { // Divining Rod. If (root), 0x86..0x89 pingponging.
    if (SPRITE->root) {
      int frame=(g.framec/5)%6;
      switch (frame) {
        case 0: itemtileid=0x86; break;
        case 1: itemtileid=0x87; break;
        case 2: itemtileid=0x88; break;
        case 3: itemtileid=0x89; break;
        case 4: itemtileid=0x88; break;
        case 5: itemtileid=0x87; break;
      }
    }
  } else if (itemtileid==0x74) { // Match
    if (SPRITE->matchclock>0.0) itemtileid=(g.framec&8)?0x85:0x84;
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
    if (SPRITE->blocked) {
      if (SPRITE->facedx<0) { tileid=0x2b; xform=0; }
      else if (SPRITE->facedx>0) { tileid=0x2b; xform=EGG_XFORM_XREV; }
      else if (SPRITE->facedy<0) { tileid=0x2a; xform=0; }
      else { tileid=0x29; xform=0; }
    } else switch (SPRITE->walkanimframe) {
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

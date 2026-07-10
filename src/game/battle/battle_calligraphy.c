/* battle_calligraphy.c
 */

#include "game/bellacopia.h"

struct battle_calligraphy {
  struct battle hdr;
  double playtime;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    int refx,refy,refw,refh; // Small reference image.
    int boxx,boxy,boxw,boxh; // Large interactive canvas.
    int srcx,srcy; // Origin of 32x32 reference in image:battle_tundra.
    int texid; // boxw,boxh
    int penx,peny; // 0..(boxw,boxh)
    double penspeed; // px/s
    uint8_t ref[32*32*4]; // RGBA for my reference image. Alpha is sacrosanct, but CPU overwrites Red to mark visited pixels.
    
    // Controller sets:
    int indx,indy; // -1,0,1
    int inpaint; // 0,1
    
    double penfx,penfy;
    int score; // 0..999
    int use_indicator;
    int indicator;
    
    // CPU player:
    int ax,ay,bx,by,dx,dy;
    int overshoot; // Extend chosen lines randomly up to this far.
    int fudge; // Move chose lines laterally, randomly, up to this far.
    
  } playerv[2];
};

#define BATTLE ((struct battle_calligraphy*)battle)

/* Delete.
 */
 
static void _calligraphy_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->refw=NS_sys_tilesize*2;
  player->refh=NS_sys_tilesize*2;
  player->boxw=NS_sys_tilesize*8;
  player->boxh=NS_sys_tilesize*8;
  player->refy=7;
  player->boxy=FBH-7-player->boxh;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->refx=7;
    player->boxx=(FBW>>1)-3-player->boxw;
  } else { // Right.
    player->who=1;
    player->refx=FBW-7-player->refw;
    player->boxx=(FBW>>1)+3;
  }
  player->penspeed=50.0*(1.0-player->skill)+90.0*player->skill;
  if (player->human=human) { // Human.
    if (player->skill>=0.400) player->use_indicator=1;
  } else { // CPU.
    player->penspeed*=0.500; // cpu penalty
    player->overshoot=(int)(14.0*(1.0-player->skill));
    player->fudge=(int)(10.0*(1.0-player->skill));
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x8088a0ff;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
  player->srcy=96;
  player->penx=player->boxw>>1;
  player->peny=player->boxh>>1;
  player->penfx=player->penx+0.5;
  player->penfy=player->peny+0.5;
  player->texid=egg_texture_new();
  egg_texture_load_raw(player->texid,player->boxw,player->boxh,player->boxw<<2,0,0);
  egg_texture_clear(player->texid);
  // _calligraphy_init() will set up the reference image
}

/* New.
 */
 
static int _calligraphy_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  BATTLE->playtime=40.0; // This game takes a while. We suppress the universal timeout.
  
  /* We have seven images roughly arranged by difficulty.
   * In a balanced match, we could pick any, and both players get the same image. That's the usual one-player case.
   */
  const int imagec=7;
  int delta=battle->args.bias-0x80;
  if (delta<0) delta=-delta;
  delta=(delta*imagec)/0x80;
  if (delta<0) delta=0; else if (delta>imagec-1) delta=imagec-1;
  int optc=imagec-delta;
  int optp=rand()%optc;
  if (battle->args.bias>=0x80) {
    r->srcx=32+optp*32;
    l->srcx=32+(optp+delta)*32;
  } else {
    l->srcx=32+optp*32;
    r->srcx=32+(optp+delta)*32;
  }
  
  /* With the reference images chosen, yoink their pixels.
   * We render into a temporary 32x32 texture so we don't have to read back the whole tilesheet.
   */
  int reftexid=egg_texture_new();
  if (egg_texture_load_raw(reftexid,32,32,32*4,0,0)<0) return -1;
  egg_texture_clear(reftexid);
  graf_reset(&g.graf);
  graf_set_output(&g.graf,reftexid);
  graf_set_image(&g.graf,RID_image_battle_tundra);
  graf_decal(&g.graf,0,0,l->srcx,l->srcy,32,32);
  graf_flush(&g.graf);
  egg_texture_get_pixels(l->ref,sizeof(l->ref),reftexid);
  if ((l->srcx==r->srcx)&&(l->srcy==r->srcy)) { // They're usually the same, and we can skip a lot of work then.
    memcpy(r->ref,l->ref,sizeof(l->ref));
  } else {
    egg_texture_clear(reftexid);
    graf_reset(&g.graf);
    graf_set_output(&g.graf,reftexid);
    graf_set_image(&g.graf,RID_image_battle_tundra);
    graf_decal(&g.graf,0,0,r->srcx,r->srcy,32,32);
    graf_flush(&g.graf);
    egg_texture_get_pixels(r->ref,sizeof(r->ref),reftexid);
  }
  graf_reset(&g.graf);
  egg_texture_del(reftexid);
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0;
  }
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player->indy=-1; break;
    case EGG_BTN_DOWN: player->indy=1; break;
    default: player->indy=0;
  }
  player->inpaint=(input&EGG_BTN_SOUTH);
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If I have a delta, I'm moving toward one of the points.
   */
  if (player->dx||player->dy) {
    int targetx,targety;
    if (player->inpaint) { // I'm painting; the target is (b).
      targetx=player->bx;
      targety=player->by;
    } else { // Not painting yet; the target is (a).
      targetx=player->ax;
      targety=player->ay;
    }
    if (player->penx<targetx-1) player->indx=1;
    else if (player->penx>targetx+1) player->indx=-1;
    else player->indx=0;
    if (player->peny<targety-1) player->indy=1;
    else if (player->peny>targety+1) player->indy=-1;
    else player->indy=0;
    if (!player->indx&&!player->indy) { // Reached target.
      if (player->inpaint) { // I was painting, so stop and clear my delta.
        player->inpaint=0;
        player->dx=0;
        player->dy=0;
      } else { // Ready to start painting.
        player->inpaint=1;
      }
    }
    
  /* No delta, so we need to pick our next move.
   */
  } else {
    /* First choose a reference pixel at random.
     * If it's transparent or already visited, bail out and we'll try again next frame.
     */
    int qx=rand()&31;
    int qy=rand()&31;
    if (!player->ref[qy*128+qx*4+3]) return;
    if (player->ref[qy*128+qx*4]==0xee) return;
    
    /* Like a King checking for check, radiate outward from (q) and count contiguous opaque pixels along the eight directions.
     * We're only checking opaqueness now. It's ok to cross over a pixel we've already visited.
     */
    struct visitor {
      int x,y;
      int dx,dy;
      int done;
      int c;
    } visitorv[8]={
      {qx,qy, 0,-1,0},
      {qx,qy, 1,-1,0},
      {qx,qy, 1, 0,0},
      {qx,qy, 1, 1,0},
      {qx,qy, 0, 1,0},
      {qx,qy,-1, 1,0},
      {qx,qy,-1, 0,0},
      {qx,qy,-1,-1,0},
    };
    for (;;) {
      int proceed=0;
      struct visitor *visitor=visitorv;
      int i=8;
      for (;i-->0;visitor++) {
        if (visitor->done) continue;
        visitor->x+=visitor->dx;
        visitor->y+=visitor->dy;
        if ((visitor->x<0)||(visitor->x>=32)||(visitor->y<0)||(visitor->y>=32)) {
          visitor->done=1;
          continue;
        }
        if (!player->ref[visitor->y*128+visitor->x*4+3]) {
          visitor->done=1;
          continue;
        }
        visitor->c++;
        proceed=1;
      }
      if (!proceed) break;
    }
    
    /* Combine the two visitors for each of the four travel options.
     * They didn't count the starting point, so it's +1 to each.
     */
    int vertc=visitorv[0].c+visitorv[4].c+1;
    int horzc=visitorv[2].c+visitorv[6].c+1;
    int posc=visitorv[1].c+visitorv[5].c+1;
    int negc=visitorv[3].c+visitorv[7].c+1;
    
    /* Among the four options, filter to the longest length, and pick randomly among those.
     */
    int candidatev[4]; // lower index in visitorv
    int candidatec=0;
    int longest=vertc;
    if (horzc>=longest) longest=horzc;
    if (posc>=longest) longest=posc;
    if (negc>=longest) longest=negc;
    if (vertc>=longest) candidatev[candidatec++]=0;
    if (horzc>=longest) candidatev[candidatec++]=2;
    if (posc>=longest) candidatev[candidatec++]=1;
    if (negc>=longest) candidatev[candidatec++]=3;
    if (!candidatec) return; // oops
    int candidatep=0;
    if (candidatec>1) candidatep=rand()%candidatec;
    int visitorp=candidatev[candidatep];
    
    /* Put the two visitors in random order, just so we're not travelling the same direction every time.
     * A cleverer robot would put the one nearer the current cursor in (a).
     */
    struct visitor *visa,*visb;
    if (rand()&1) {
      visa=visitorv+visitorp;
      visb=visa+4;
    } else {
      visb=visitorv+visitorp;
      visa=visb+4;
    }
    
    /* And those visitors give us our (ax,ay,bx,by). (dx,dy) come from (b).
     */
    player->ax=qx+visa->dx*visa->c;
    player->ay=qy+visa->dy*visa->c;
    player->bx=qx+visb->dx*visb->c;
    player->by=qy+visb->dy*visb->c;
    player->dx=visb->dx;
    player->dy=visb->dy;
    
    /* Oh wait, except of course, the reference image is 1/4 the size of our canvas image.
     * Scale up a little.
     */
    player->ax<<=2;
    player->ay<<=2;
    player->bx<<=2;
    player->by<<=2;
    
    /* And CPU players are maybe too good at it, so deliberately foul it up a little.
     */
    if (player->overshoot) {
      int over=rand()%player->overshoot;
      player->ax-=over*player->dx;
      player->ay-=over*player->dy;
      player->bx+=over*player->dx;
      player->by+=over*player->dy;
    }
    if (player->fudge) {
      int fudge=rand()%player->fudge;
      if (player->dx&&player->dy) {
        switch (rand()&3) {
          case 0: player->ax-=fudge; player->bx-=fudge; break;
          case 1: player->ax+=fudge; player->bx+=fudge; break;
          case 2: player->ay-=fudge; player->by-=fudge; break;
          case 3: player->ay+=fudge; player->by+=fudge; break;
        }
      } else if (player->dx) {
        if (rand()&1) {
          player->ay-=fudge;
          player->by-=fudge;
        } else {
          player->ay+=fudge;
          player->by+=fudge;
        }
      } else {
        if (rand()&1) {
          player->ax-=fudge;
          player->bx-=fudge;
        } else {
          player->ax+=fudge;
          player->bx+=fudge;
        }
      }
    }
    if (player->ax<0) player->ax=0; else if (player->ax>127) player->ax=127;
    if (player->ay<0) player->ay=0; else if (player->ay>127) player->ay=127;
    if (player->bx<0) player->bx=0; else if (player->bx>127) player->bx=127;
    if (player->by<0) player->by=0; else if (player->by>127) player->by=127;
    
    /* Finally, mark all these pixels visited.
     * If our clamping broke the line, whatever, we'll mark something else. It's fine.
     */
    int markx=player->ax>>2;
    int marky=player->ay>>2;
    while ((markx>=0)&&(marky>=0)&&(markx<32)&&(marky<32)) {
      player->ref[marky*128+markx*4]=0xee;
      markx+=player->dx;
      marky+=player->dy;
    }
  }
}

/* Paint to the player's image.
 */
 
static void calligraphy_update_line(struct battle *battle,struct player *player,int ax,int ay,int bx,int by) {
  graf_set_output(&g.graf,player->texid);
  graf_set_image(&g.graf,RID_image_battle_tundra);
  
  /* Egg doesn't do fat lines, so we're going to trace the line ourselves and render it as a series of fancies.
   * (dx,dy) are -1,0,1.
   * (wx) is always positive and (wy) negative. Or zero.
   */
  int dx,dy,wx,wy;
  if (ax<bx) {
    dx=1;
    wx=bx-ax;
  } else if (ax>bx) {
    dx=-1;
    wx=ax-bx;
  } else {
    dx=0;
    wx=0;
  }
  if (ay<by) {
    dy=1;
    wy=ay-by;
  } else if (ay>by) {
    dy=-1;
    wy=by-ay;
  } else {
    dy=0;
    wy=0;
  }
  int w=wx+wy;
  for (;;) {
    graf_fancy(&g.graf,ax,ay,0x59,0,0,NS_sys_tilesize,0,player->color);
    if ((w>0)&&(ax!=bx)) {
      w+=wy;
      ax+=dx;
    } else if ((w<0)&&(ay!=by)) {
      w+=wx;
      ay+=dy;
    } else if (ax!=bx) {
      w+=wy;
      ax+=dx;
    } else if (ay!=by) {
      w+=wx;
      ay+=dy;
    } else {
      break;
    }
  }
  
  graf_set_output(&g.graf,1);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Cursor motion and painting.
  int penx0=player->penx;
  int peny0=player->peny;
  if (player->indx) {
    player->penfx+=player->indx*elapsed*player->penspeed;
    player->penx=(int)player->penfx;
    if (player->penx<0) player->penx=0;
    else if (player->penx>=player->boxw) player->penx=player->boxw-1;
  }
  if (player->indy) {
    player->penfy+=player->indy*elapsed*player->penspeed;
    player->peny=(int)player->penfy;
    if (player->peny<0) player->peny=0;
    else if (player->peny>=player->boxh) player->peny=player->boxh-1;
  }
  if ((penx0!=player->penx)||(peny0!=player->peny)) {
    if (player->inpaint) {
      calligraphy_update_line(battle,player,penx0,peny0,player->penx,player->peny);
    }
    // Optional on-target indicator.
    if (player->use_indicator) {
      int refx=player->penx>>2;
      int refy=player->peny>>2;
      player->indicator=(
        (refx>=0)&&(refy>=0)&&(refx<32)&&(refy<32)&&
        player->ref[refy*128+refx*4+3]
      );
    }
  }
}

/* Compare one pixel of the reference image to a 4x4 section of the player's image.
 * The provided pointers should point to the alpha channels.
 * Returns the count of matching pixels 0..16.
 */
 
static int calligraphy_compare_sub(uint8_t ref,const uint8_t *src) {
  int score=0;
  int yi=4;
  for (;yi-->0;src+=128*4) {
    const uint8_t *p=src;
    int xi=4;
    for (;xi-->0;p+=4) {
      if (*p==ref) score++;
    }
  }
  return score;
}

/* Compare 32x32 reference image to 128x128 player image.
 * Returns 0..999
 */
 
static int calligraphy_compare_images(const uint8_t *ref,const uint8_t *canvas) {

  // We only care about the alpha channels.
  ref+=3;
  canvas+=3;
  
  /* Compare each pixel of (ref) to a 4x4 section of (canvas).
   * Count the pixels with matching alpha.
   * Alpha will only be 0x00 or 0xff.
   */
  int numer=0,denom=0;
  const uint8_t *crow=canvas;
  const int clongstride=128*4*4;
  const int cshortstride=4*4;
  int yi=32;
  for (;yi-->0;crow+=clongstride) {
    const uint8_t *cp=crow;
    int xi=32;
    for (;xi-->0;ref+=4,cp+=cshortstride) {
      int sub=calligraphy_compare_sub(*ref,cp);
      if (*ref) { // Opaque pixel: Always contributes.
        numer+=sub;
        denom+=16;
      } else if (sub==16) { // Transparent pixel the user didn't touch: Doesn't contribute.
      } else { // Transparent pixels but the user touched it. You break it, you bought it.
        numer+=sub;
        denom+=16;
      }
    }
  }
  
  /* If we didn't get a denominator (both images fully transparent), call it zero.
   * This could happen if reading pixels back from the GPU fails.
   * Not that I care what we report in that case, but at least don't divide by zero over it.
   */
  if (!denom) return 0;
  
  /* Scale and clamp to 0..999.
   */
  int score=(numer*999)/denom;
  if (score<0) return 0;
  if (score>999) return 999;
  return score;
}

/* Decide outcome.
 */
 
static void calligraphy_finish(struct battle *battle) {
  battle->outcome=0; // In case we have to bail out, do finish with an answer.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;

  /* Allocate an RGBA buffers for the canvas, for one player at a time.
   */
  int canvasstride=128*4;
  int canvassize=canvasstride*128;
  uint8_t *canvas=malloc(canvassize);
  if (!canvas) return;
  
  /* Acquire and score left player pixels.
   */
  egg_texture_get_pixels(canvas,canvassize,l->texid);
  l->score=calligraphy_compare_images(l->ref,canvas);
  
  /* Acquire and score right player pixels.
   */
  egg_texture_get_pixels(canvas,canvassize,r->texid);
  r->score=calligraphy_compare_images(r->ref,canvas);
  
  /* Clean up and compare.scores.
   */
  free(canvas);
  if (l->score>r->score) battle->outcome=1;
  else if (l->score<r->score) battle->outcome=-1;
  else battle->outcome=0;
}

/* Update.
 */
 
static void _calligraphy_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if ((BATTLE->playtime-=elapsed)<=0.0) {
    calligraphy_finish(battle);
    return;
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  //XXX
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render bits
 */

// (w,h) must be multiples of NS_sys_tilesize.
static void calligraphy_shadow(int x,int y,int w,int h) {
  int colc=w/NS_sys_tilesize;
  int rowc=h/NS_sys_tilesize;
  int i;
  int dstx=x+(NS_sys_tilesize>>1);
  int dsty1=y-(NS_sys_tilesize>>1);
  int dsty2=dsty1+h+NS_sys_tilesize;
  for (i=colc;i-->0;dstx+=NS_sys_tilesize) {
    graf_tile(&g.graf,dstx,dsty1,0x61,0);
    graf_tile(&g.graf,dstx,dsty2,0x61,EGG_XFORM_YREV);
  }
  int dsty=y+(NS_sys_tilesize>>1);
  int dstx1=x-(NS_sys_tilesize>>1);
  int dstx2=dstx1+w+NS_sys_tilesize;
  for (i=rowc;i-->0;dsty+=NS_sys_tilesize) {
    graf_tile(&g.graf,dstx1,dsty,0x61,EGG_XFORM_SWAP);
    graf_tile(&g.graf,dstx2,dsty,0x61,EGG_XFORM_SWAP|EGG_XFORM_YREV);
  }
  graf_tile(&g.graf,dstx1,dsty1,0x60,0);
  graf_tile(&g.graf,dstx2,dsty1,0x60,EGG_XFORM_XREV);
  graf_tile(&g.graf,dstx1,dsty2,0x60,EGG_XFORM_YREV);
  graf_tile(&g.graf,dstx2,dsty2,0x60,EGG_XFORM_XREV|EGG_XFORM_YREV);
}

static void calligraphy_guides(int x0,int y0,int w,int h) {
  uint32_t minor=0x0000ff20;
  uint32_t major=0x0000ff40;
  int x4=x0+w;
  int x2=x0+(w>>1);
  int x1=(x0+x2)>>1;
  int x3=(x4+x2)>>1;
  int y4=y0+h;
  int y2=y0+(h>>1);
  int y1=(y0+y2)>>1;
  int y3=(y4+y2)>>1;
  graf_line(&g.graf,x0,y1,minor,x4,y1,minor);
  graf_line(&g.graf,x0,y3,minor,x4,y3,minor);
  graf_line(&g.graf,x1,y0,minor,x1,y4,minor);
  graf_line(&g.graf,x3,y0,minor,x3,y4,minor);
  graf_line(&g.graf,x0,y2,major,x4,y2,major);
  graf_line(&g.graf,x2,y0,major,x2,y4,major);
}

static void calligraphy_render_int999(int x,int y,int v) {
  if (v<0) v=0;
  else if (v>999) v=999;
  graf_tile(&g.graf,x+8,y,'0'+v%10,0);
  if (v>=10) {
    graf_tile(&g.graf,x,y,'0'+(v/10)%10,0);
    if (v>=100) graf_tile(&g.graf,x-8,y,'0'+v/100,0);
  }
}

/* Render.
 */
 
static void _calligraphy_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  // Background and shadows.
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x100850ff);
  graf_set_image(&g.graf,RID_image_battle_tundra);
  calligraphy_shadow(l->refx,l->refy,l->refw,l->refh);
  calligraphy_shadow(r->refx,r->refy,r->refw,r->refh);
  calligraphy_shadow(l->boxx,l->boxy,l->boxw,l->boxh);
  calligraphy_shadow(r->boxx,r->boxy,r->boxw,r->boxh);
  
  // Canvases and guidelines.
  graf_fill_rect(&g.graf,l->refx,l->refy,l->refw,l->refh,0xc0e0f0ff);
  graf_fill_rect(&g.graf,r->refx,r->refy,r->refw,r->refh,0xc0e0f0ff);
  graf_fill_rect(&g.graf,l->boxx,l->boxy,l->boxw,l->boxh,0xf0f8ffff);
  graf_fill_rect(&g.graf,r->boxx,r->boxy,r->boxw,r->boxh,0xf0f8ffff);
  calligraphy_guides(l->refx,l->refy,l->refw,l->refh);
  calligraphy_guides(r->refx,r->refy,r->refw,r->refh);
  calligraphy_guides(l->boxx,l->boxy,l->boxw,l->boxh);
  calligraphy_guides(r->boxx,r->boxy,r->boxw,r->boxh);
  
  // Reference images.
  graf_set_image(&g.graf,RID_image_battle_tundra);
  graf_decal(&g.graf,l->refx,l->refy,l->srcx,l->srcy,l->refw,l->refh);
  graf_decal(&g.graf,r->refx,r->refy,r->srcx,r->srcy,r->refw,r->refh);
  
  // Canvas images.
  graf_set_input(&g.graf,l->texid);
  graf_decal(&g.graf,l->boxx,l->boxy,0,0,l->boxw,l->boxh);
  graf_set_input(&g.graf,r->texid);
  graf_decal(&g.graf,r->boxx,r->boxy,0,0,r->boxw,r->boxh);
  
  // Cursors and clock.
  if (battle->outcome==-2) {
    graf_set_image(&g.graf,RID_image_battle_tundra);
    graf_fancy(&g.graf,l->boxx+l->penx,l->boxy+l->peny,0x59,0,0,NS_sys_tilesize,0,(g.framec&16)?l->color:0x80808080);
    graf_fancy(&g.graf,r->boxx+r->penx,r->boxy+r->peny,0x59,0,0,NS_sys_tilesize,0,(g.framec&16)?r->color:0x80808080);
    if (l->use_indicator) graf_fancy(&g.graf,l->boxx+l->penx,l->boxy+l->peny,0x5a,0,0,NS_sys_tilesize,0,l->indicator?0x00ff00ff:0xff0000ff);
    if (r->use_indicator) graf_fancy(&g.graf,r->boxx+r->penx,r->boxy+r->peny,0x5a,0,0,NS_sys_tilesize,0,r->indicator?0x00ff00ff:0xff0000ff);
    
    graf_set_image(&g.graf,RID_image_fonttiles);
    int sec=(int)(BATTLE->playtime+0.999);
    if (sec>99) sec=99; else if (sec<1) sec=1;
    graf_tile(&g.graf,(FBW>>1)-4,16,'0'+sec/10,0);
    graf_tile(&g.graf,(FBW>>1)+4,16,'0'+sec%10,0);
    
  // Overlay reference on canvases.
  } else {
    graf_set_image(&g.graf,RID_image_battle_tundra);
    graf_set_tint(&g.graf,0x00c000ff);
    graf_set_alpha(&g.graf,0x80);
    graf_decal_rotate(&g.graf,l->boxx+(l->boxw>>1),l->boxy+(l->boxh>>1),l->srcx,l->srcy,l->refw,0.0,1.0,4.0);
    graf_decal_rotate(&g.graf,r->boxx+(r->boxw>>1),r->boxy+(r->boxh>>1),r->srcx,r->srcy,r->refh,0.0,1.0,4.0);
    graf_set_tint(&g.graf,0);
    graf_set_alpha(&g.graf,0xff);
    
    graf_set_image(&g.graf,RID_image_fonttiles);
    calligraphy_render_int999(FBW/3,16,l->score);
    calligraphy_render_int999((FBW*2)/3,16,r->score);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_calligraphy={
  .name="calligraphy",
  .objlen=sizeof(struct battle_calligraphy),
  .id=NS_battle_calligraphy,
  .strix_name=259,
  .no_article=0,
  .no_contest=0,
  .no_timeout=1,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_dpad_a,
  .del=_calligraphy_del,
  .init=_calligraphy_init,
  .update=_calligraphy_update,
  .render=_calligraphy_render,
};

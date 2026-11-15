/* battle_photography.c
 */

#include "game/batsup/battle_internal.h"

#define GROUNDY 140
#define MOONY 40
#define STARC 128

struct battle_photography {
  struct battle hdr;
  
  double batx,baty;
  double batdx,batdy;
  double batanimclock;
  int batanimframe;
  uint8_t batxform;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // 2x2 tiles, constant
    uint8_t xform;
    int x,y; // Center of top-left tile (not center of sprite!)
    double flash; // Counts down while we display the flash.
    int blackout;
    int done;
    int texid,texw,texh; // Actually, dimensions must be square for graf_decal_rotate. So there's vertical dead space.
    double picx0,picy0; // Where the photo starts sliding in.
    double picx1,picy1; // ...where it ends up.
    double pict0,pict1; // Rotation endpoints.
    double picp; // 0..1, how far along the pic animation.
    double score; // 0..1, final when (done) nonzero.
    double cpuclock; // Full timing established at init. Just count down and snap when expired.
  } playerv[2];
  
  struct egg_render_tile starv[STARC];
};

#define BATTLE ((struct battle_photography*)battle)

/* Delete.
 */
 
static void _photography_del(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  egg_texture_del(l->texid);
  egg_texture_del(r->texid);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->y=GROUNDY-NS_sys_tilesize-(NS_sys_tilesize>>1);
  player->x=FBW/3;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->x=FBW-player->x;
  }
  player->x-=NS_sys_tilesize>>1;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    // Can't make our plan until the bat's course is laid. See below.
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0xad0c0cff;
        player->tileid=0x1a;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x16;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x18;
      } break;
  }
}

/* Prepare CPU player timing.
 */
 
static void photography_plan_cpu(struct battle *battle,struct player *player) {
  if (player->human) return;
  
  /* Predict the time to the exact moon crossing.
   * Motion is perfectly linear, so we only need to examine one axis.
   */
  double speed=BATTLE->batdx;
  double distance=BATTLE->batx-(FBW*0.5);
  if ((distance>-5.0)&&(distance<5.0)) { // Horizontal offset very small (must be approaching from below). Use the vertical instead.
    speed=BATTLE->batdy;
    distance=BATTLE->baty-MOONY;
  }
  double dur=distance/speed;
  if (dur<0.0) dur=-dur;
  
  /* Pick an amount of error based on our skill, with some random fuzz.
   */
  double errhi=1.000*(1.0-player->skill)+0.500*player->skill;
  double errlo=errhi*0.5;
  double err=(rand()&0xffff)/65535.0;
  err=errlo*(1.0-err)+errhi*err;
  if (rand()&1) player->cpuclock=dur+err;
  else player->cpuclock=dur-err;
}

/* New.
 */
 
static int _photography_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  
  /* Prepare a vertex buffer of stars. Random and purely decorative.
   */
  struct egg_render_tile *vtx=BATTLE->starv;
  int i=STARC;
  for (;i-->0;vtx++) {
    vtx->x=rand()%FBW;
    vtx->y=rand()%GROUNDY;
    vtx->tileid=0x09+rand()%7;
  }
  
  /* Choose a random position for the bat, somewhere along the sky's perimeter.
   * Forbid any on the top edge within half a screen width of center -- the moon is very close to top-center.
   */
  const int margin=20;
  int range=(FBW>>1)+GROUNDY+FBW+GROUNDY+(FBW>>1)+margin*8;
  int p=rand()%range;
  if (p<(FBW>>1)+margin) { // top left
    BATTLE->batx=p-margin;
    BATTLE->baty=-margin;
  } else {
    p-=(FBW>>1)+margin;
    if (p<GROUNDY+margin*2) { // left
      BATTLE->batx=-margin;
      BATTLE->baty=p-margin;
    } else {
      p-=GROUNDY+margin*2;
      if (p<FBW+margin*2) { // bottom
        BATTLE->batx=p-margin;
        BATTLE->baty=GROUNDY+margin;
      } else {
        p-=FBW+margin*2;
        if (p<GROUNDY+margin*2) { // right
          BATTLE->batx=FBW+margin;
          BATTLE->baty=p-margin;
        } else { // top right
          p-=GROUNDY+margin*2;
          BATTLE->batx=FBW+margin-p;
          BATTLE->baty=-margin;
        }
      }
    }
  }
  
  /* Then choose a direction that will pass thru the center of the moon.
   * We could play the bat's speed if it's man-vs-cpu. For now, keeping it constant.
   */
  double batspeed=40.0+((rand()&0xffff)*10.0)/65535.0;
  BATTLE->batdx=FBW*0.5-BATTLE->batx;
  BATTLE->batdy=MOONY-BATTLE->baty;
  double distance=sqrt(BATTLE->batdx*BATTLE->batdx+BATTLE->batdy*BATTLE->batdy);
  double nx=BATTLE->batdx/distance;
  double ny=BATTLE->batdy/distance;
  BATTLE->batdx=nx*batspeed;
  BATTLE->batdy=ny*batspeed;
  if (BATTLE->batdx<0.0) BATTLE->batxform=EGG_XFORM_XREV;
  
  /* CPU players decide their timing in advance, after the bat's course is laid.
   */
  photography_plan_cpu(battle,l);
  photography_plan_cpu(battle,r);
  
  return 0;
}

/* Render a photograph, using graf.
 */
 
static void photography_render_subscene(struct battle *battle,int fbw,int fbh) {
  int xo=(FBW>>1)-(fbw>>1);
  int yo=MOONY-(fbh>>1);

  // Sky.
  graf_fill_rect(g_graf,0,0,fbw,fbh,0x000000ff);
  
  // Stars and Moon.
  // The star vertices are prepared, but beware, they need to be offset.
  {
    graf_set_image(g_graf,RID_image_battle_underground2);
    struct egg_render_tile starv[STARC];
    struct egg_render_tile *src=BATTLE->starv,*dst=starv;
    int i=STARC; for (;i-->0;dst++,src++) {
      dst->x=src->x-xo;
      dst->y=src->y-yo;
      dst->tileid=src->tileid;
    }
    graf_tile_batch(g_graf,starv,STARC);
    int dstx=(fbw>>1)-NS_sys_tilesize;
    int dsty=(fbh>>1)-NS_sys_tilesize;
    int srcx=NS_sys_tilesize*14;
    int srcy=NS_sys_tilesize*1;
    graf_decal(g_graf,dstx,dsty,srcx,srcy,NS_sys_tilesize*2,NS_sys_tilesize*2);
  }
  
  // Bat.
  {
    graf_set_image(g_graf,RID_image_battle_underground);
    int x0=(int)BATTLE->batx-(NS_sys_tilesize>>1)-xo;
    int y0=(int)BATTLE->baty-(NS_sys_tilesize>>1)-yo;
    int x1=x0,y1=y0+NS_sys_tilesize;
    if (BATTLE->batxform&EGG_XFORM_XREV) x0+=NS_sys_tilesize;
    else x1+=NS_sys_tilesize;
    uint8_t tileid=0x1e;
    if (BATTLE->batanimframe<6) tileid+=BATTLE->batanimframe*0x20;
    else tileid+=(10-BATTLE->batanimframe)*0x20;
    graf_tile(g_graf,x0,y0,tileid+0x00,BATTLE->batxform);
    graf_tile(g_graf,x1,y0,tileid+0x01,BATTLE->batxform);
    graf_tile(g_graf,x0,y1,tileid+0x10,BATTLE->batxform);
    graf_tile(g_graf,x1,y1,tileid+0x11,BATTLE->batxform);
  }
}

/* Process the photo in local memory.
 * Rewrites (v) in place.
 */
 
static void photography_fill(uint8_t *rgba,int fullw,int fullh,int x,int y,int w,int h,uint32_t pixel) {

  // One can confirm that this chunk is redundant, but I feel dirty without checking bounds.
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x+w>fullw) w=fullw-x;
  if (y+h>fullh) h=fullh-y;
  if ((w<1)||(h<1)) return;
  
  int stride=fullw<<2;
  uint8_t r=pixel>>24,g=pixel>>16,b=pixel>>8,a=pixel;
  rgba+=stride*y+(x<<2);
  int yi=h; for (;yi-->0;rgba+=stride) {
    uint8_t *dstp=rgba;
    int xi=w; for (;xi-->0;dstp+=4) {
      dstp[0]=r;
      dstp[1]=g;
      dstp[2]=b;
      dstp[3]=a;
    }
  }
}
 
static void photography_process_rgba(uint8_t *rgba,int w,int h) {

  /* First force every pixel to gray.
   * We pointlessly operate on the pixels we're going to overwrite, whatever.
   */
  {
    uint8_t *v=rgba;
    int i=w*h; for (;i-->0;v+=4) {
      int y=(v[0]+v[1]+v[2])/3; // Naive grayscale. Is this good enough?
      v[0]=v[1]=v[2]=y;
    }
  }
  
  /* Then erase a little around each side, and fill in a white border inside of that.
   * Erase more on the verticals: The texture has to be square, but we want it to look oblong.
   */
  {
    const int erasey=10,erasex=1,border=2;
    photography_fill(rgba,w,h,0,0,w,erasey,0x00000000);
    photography_fill(rgba,w,h,0,h-erasey,w,erasey,0x00000000);
    photography_fill(rgba,w,h,0,0,erasex,h,0x00000000);
    photography_fill(rgba,w,h,w-erasex,0,erasex,h,0x00000000);
    photography_fill(rgba,w,h,erasex,erasey,w-erasex*2,border,0xffffffff);
    photography_fill(rgba,w,h,erasex,erasey,border,h-erasey*2,0xffffffff);
    photography_fill(rgba,w,h,w-erasex-border,erasey,border,h-erasey*2,0xffffffff);
    photography_fill(rgba,w,h,erasex,h-erasey-border,w-erasex*2,border,0xffffffff);
  }
}

/* Take a picture.
 */
 
static void photography_snap(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_treadle,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->flash=0.500;
  player->done=1;
  
  /* Score it.
   * This is dead simple, it's just how far is the bat from the moon.
   */
  double moonx=FBW*0.5;
  double moony=MOONY;
  double dx=BATTLE->batx-moonx;
  double dy=BATTLE->baty-moony;
  double d2=dx*dx+dy*dy;
  player->score=1.0-d2/1000.0;
  if (player->score<0.0) player->score=0.0;
  
  /* Prepare the slide-in animation.
   * Final position is roughly where the player's sprite sits (but centered vertically in framebuffer).
   * Final rotation should be within like a tenth of a turn from natural.
   * Initial position is offscreen horizontally, random vertically, and rotation offset from the final by some tasteful amount.
   */
  player->picx1=player->x+NS_sys_tilesize*0.5;
  player->picy1=FBH*0.5;
  if (player->who) { // From right.
    player->picx0=FBW+50.0;
  } else { // From left.
    player->picx0=-50.0;
  }
  player->picy0=rand()%FBH;
  player->pict1=((rand()&0xffff)*M_PI*0.333)/65535.0-M_PI*0.166;
  player->pict0=player->pict1+((rand()&1)?-2.0:2.0);
  
  /* Generate the image.
   */
  player->texw=NS_sys_tilesize*5;
  player->texh=NS_sys_tilesize*5;
  int stride=player->texw<<2;
  int imglen=stride*player->texh;
  if (!player->texid) player->texid=egg_texture_new();
  if (egg_texture_load_raw(player->texid,player->texw,player->texh,stride,0,0)>=0) {
  
    // Render a small bit around the Moon, same way the real render does.
    graf_flush(g_graf);
    graf_set_output(g_graf,player->texid);
    photography_render_subscene(battle,player->texw,player->texh);
    graf_flush(g_graf);
    graf_set_output(g_graf,1);
    
    // Download pixels.
    // If we're out of memory -- unlikely! -- it's ok to stop here. They get a color photo instead.
    uint8_t *rgba=malloc(imglen);
    if (!rgba) return;
    int ok=(egg_texture_get_pixels(rgba,imglen,player->texid)>=0);
    if (ok) {
    
      // Convert to grayscale.
      photography_process_rgba(rgba,player->texw,player->texh);
    
      // Upload pixels.
      egg_texture_load_raw(player->texid,player->texw,player->texh,stride,rgba,imglen);
    }
    free(rgba);
  }
}

/* Update players.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->done) return;
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    if (input&EGG_BTN_SOUTH) photography_snap(battle,player);
  }
}
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->done) return;
  if ((player->cpuclock-=elapsed)>0.0) return;
  photography_snap(battle,player);
}
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->flash>0.0) {
    player->flash-=elapsed;
  }
  if (player->done&&(player->picp<1.0)) {
    player->picp+=0.500*elapsed;
    if (player->picp>1.0) player->picp=1.0;
  }
}

/* Update.
 */
 
static void _photography_update(struct battle *battle,double elapsed) {
  
  if (battle->outcome==-2) {
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
      else player_update_cpu(battle,player,elapsed);
      player_update_common(battle,player,elapsed);
    }
  }
  
  BATTLE->batx+=BATTLE->batdx*elapsed;
  BATTLE->baty+=BATTLE->batdy*elapsed;
  if ((BATTLE->batanimclock-=elapsed)<=0.0) {
    BATTLE->batanimclock+=0.100;
    if (++(BATTLE->batanimframe)>=10) BATTLE->batanimframe=0;
  }

  /* Battle is over when both photos have reached their resting positions.
   * In case a player fails to snap, we also terminate when the bat goes offscreen.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if ((l->picp>=1.0)&&(r->picp>=1.0)) {
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    } else if (!l->done||!r->done) {
      if (
        ((BATTLE->batdx<0.0)&&(BATTLE->batx<-10.0))||
        ((BATTLE->batdx>0.0)&&(BATTLE->batx>FBW+10.0))||
        ((BATTLE->batdy<0.0)&&(BATTLE->baty<-10.0))||
        ((BATTLE->batdy>0.0)&&(BATTLE->baty>GROUNDY+10.0))
      ) {
        if (l->done) battle->outcome=1;
        else if (r->done) battle->outcome=-1;
        else battle->outcome=0;
      }
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  // x0 is back and x1 is front; they're sensitive to xform.
  int x0=player->x,y0=player->y;
  int x1=x0,y1=y0+NS_sys_tilesize;
  if (player->xform&EGG_XFORM_XREV) x0+=NS_sys_tilesize;
  else x1+=NS_sys_tilesize;
  if (player->flash>0.0) {
    graf_tile(g_graf,x0,y0,0x1c,player->xform);
    graf_tile(g_graf,x1,y0,0x1d,player->xform);
    graf_tile(g_graf,x0,y1,0x2c,player->xform);
    graf_tile(g_graf,x1,y1,0x2d,player->xform);
  }
  uint8_t tileid=player->tileid;
  graf_tile(g_graf,x0,y0,tileid+0x00,player->xform);
  graf_tile(g_graf,x1,y0,tileid+0x01,player->xform);
  graf_tile(g_graf,x0,y1,tileid+0x10,player->xform);
  graf_tile(g_graf,x1,y1,tileid+0x11,player->xform);
}

/* Render one player's photo.
 */
 
static void photography_render_product(struct battle *battle,struct player *player) {
  
  double xf=player->picx0*(1.0-player->picp)+player->picx1*player->picp;
  double yf=player->picy0*(1.0-player->picp)+player->picy1*player->picp;
  double tf=player->pict0*(1.0-player->picp)+player->pict1*player->picp;
  
  graf_set_input(g_graf,player->texid);
  graf_set_filter(g_graf,1);
  graf_set_tint(g_graf,0x000000ff);
  graf_set_alpha(g_graf,0xc0);
  graf_decal_rotate(g_graf,(int)xf,(int)yf+4,0,0,player->texw,sin(tf),cos(tf),1.0);
  graf_set_tint(g_graf,0);
  graf_set_alpha(g_graf,0xff);
  graf_decal_rotate(g_graf,(int)xf,(int)yf,0,0,player->texw,sin(tf),cos(tf),1.0);
  graf_set_filter(g_graf,0);
  
  /* If this photo won an award, tell everybody about it!
   */
  if (
    ((battle->outcome==1)&&(player->who==0))||
    ((battle->outcome==-1)&&(player->who==1))
  ) {
    graf_set_image(g_graf,RID_image_battle_underground2);
    graf_tile(g_graf,(int)xf+30,(int)yf+30,0x08,0);
  }
}

/* Render.
 */
 
static void _photography_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;

  // Sky and ground.
  graf_fill_rect(g_graf,0,0,FBW,FBH,0x000000ff);
  
  // Stars and Moon.
  {
    graf_set_image(g_graf,RID_image_battle_underground2);
    graf_tile_batch(g_graf,BATTLE->starv,STARC);
    int dstx=(FBW>>1)-NS_sys_tilesize;
    int dsty=MOONY-NS_sys_tilesize;
    int srcx=NS_sys_tilesize*14;
    int srcy=NS_sys_tilesize*1;
    graf_decal(g_graf,dstx,dsty,srcx,srcy,NS_sys_tilesize*2,NS_sys_tilesize*2);
  }
  
  // Bat. Sharing graphics from the cinematography contest.
  {
    graf_set_image(g_graf,RID_image_battle_underground);
    int x0=(int)BATTLE->batx-(NS_sys_tilesize>>1);
    int y0=(int)BATTLE->baty-(NS_sys_tilesize>>1);
    int x1=x0,y1=y0+NS_sys_tilesize;
    if (BATTLE->batxform&EGG_XFORM_XREV) x0+=NS_sys_tilesize;
    else x1+=NS_sys_tilesize;
    uint8_t tileid=0x1e;
    if (BATTLE->batanimframe<6) tileid+=BATTLE->batanimframe*0x20;
    else tileid+=(10-BATTLE->batanimframe)*0x20;
    graf_tile(g_graf,x0,y0,tileid+0x00,BATTLE->batxform);
    graf_tile(g_graf,x1,y0,tileid+0x01,BATTLE->batxform);
    graf_tile(g_graf,x0,y1,tileid+0x10,BATTLE->batxform);
    graf_tile(g_graf,x1,y1,tileid+0x11,BATTLE->batxform);
  }
  
  // Ground. Can't draw with the sky, because it must occlude the Bat.
  graf_fill_rect(g_graf,0,GROUNDY,FBW,FBH-GROUNDY,0x004010ff);
  
  // Players.
  graf_set_image(g_graf,RID_image_battle_underground2);
  player_render(battle,l);
  player_render(battle,r);
  
  // Photos.
  if (l->texid) photography_render_product(battle,l);
  if (r->texid) photography_render_product(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_photography={
  .name="photography",
  .objlen=sizeof(struct battle_photography),
  .id=130,
  .strix_name=423,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_a,
  .imageid_default=0,
  .del=_photography_del,
  .init=_photography_init,
  .update=_photography_update,
  .render=_photography_render,
};

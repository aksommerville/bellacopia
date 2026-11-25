/* battle_shovelling.c
 */

#include "game/batsup/battle_internal.h"

#define GROUNDY 150
#define PLATFORMW 100
#define PLATFORMH 6
#define REFUSE_LIMIT 32
#define SCORE_LIMIT 5

struct battle_shovelling {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // Top-left of a 3x3.
    int platformy;
    int loaded; // Coal in shovel?
    int cartfull; // Coal in cart?
    double cartx;
    double cartspeed;
    int indrop,pvindrop;
    double loadclock;
    double loadlo,loadhi;
    double waste_penalty; // Multiplier on (cartspeed) for each wasted shovelful.
    double coaly; // <platformy if none falling.
    int score;
    struct egg_render_tile scorev[SCORE_LIMIT];
  } playerv[2];
  
  struct refuse {
    int x,y;
  } refusev[REFUSE_LIMIT];
  int refusec;
};

#define BATTLE ((struct battle_shovelling*)battle)

/* Delete.
 */
 
static void _shovelling_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->cartx=-20.0;
  } else { // Right.
    player->who=1;
    player->cartx=FBW+20.0;
  }
  player->loaded=1; // Begin with coal in hand, but the cart isn't there yet.
  player->pvindrop=1; // So we don't need a blackout.
  
  player->platformy=(int)(55.0*(1.0-player->skill)+85.0*player->skill);
  player->cartspeed=90.0*(1.0-player->skill)+110.0*player->skill;
  player->loadhi=0.800*(1.0-player->skill)+0.500*player->skill;
  player->loadlo=player->loadhi*0.750;
  player->waste_penalty=0.900;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    // A wee speed penalty if the other guy is human, otherwise max difficulty is not quite possible.
    if (battle->args.lctl||battle->args.rctl) player->cartspeed*=0.900;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x996022ff;
        player->tileid=0x46;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x40;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x43;
      } break;
  }
  
  /* The entire scoreboard is present at all times.
   * Initially they all have the vacant tile.
   * (xform) gets set randomly here, then we don't touch it again.
   * That's fine; the placeholder is perfectly symmetric.
   */
  int scorex=FBW>>1;
  if (player->who) scorex+=9; else scorex-=9;
  int scorey=(FBH>>1)-20;
  struct egg_render_tile *score=player->scorev;
  int i=SCORE_LIMIT;
  for (;i-->0;score++,scorey-=12) {
    score->x=scorex;
    score->y=scorey;
    score->tileid=0x36;
    score->xform=rand()&7;
  }
}

/* New.
 */
 
static int _shovelling_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  player->indrop=(input&EGG_BTN_SOUTH)?1:0;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->indrop) { // Release button if we were holding it.
    player->indrop=0;
  } else if (player->cartfull) { // Wait for cart to empty.
    player->indrop=0;
  } else if (!player->loaded) { // Wait until my shovel fills up.
    player->indrop=0;
  } else if (player->coaly>0.0) { // Wait for current dump to land.
    player->indrop=0;
  } else { // Wait for the cart to come into range.
    double target=player->who?(FBW-PLATFORMW-15.0):(PLATFORMW+15.0);
    double dx=player->cartx-target;
    if (dx<0.0) dx=-dx;
    if (dx<10.0) {
      player->indrop=1;
    } else {
      player->indrop=0;
    }
  }
}

/* Drop my shovelful, if I have any.
 */
 
static void shovelling_drop_coal(struct battle *battle,struct player *player) {
  if (!player->loaded||(player->coaly>=player->platformy)) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  bm_sound_pan(RID_sound_dig,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->loaded=0;
  double n=(rand()&0xffff)/65535.0;
  player->loadclock=player->loadlo*(1.0-n)+player->loadhi*n;
  player->coaly=player->platformy;
}

/* Nonzero if coal that just reached the threshold should land in the cart.
 */
 
static int shovelling_cart_ready(struct battle *battle,struct player *player) {
  if (player->cartfull) return 0;
  if (player->who) {
    double limit=FBW-PLATFORMW-15.0;
    if (player->cartx>limit) return 0;
  } else {
    double limit=PLATFORMW+15.0;
    if (player->cartx<limit) return 0;
  }
  return 1;
}

/* Add a refuse pile if we have room.
 */
 
static void shovelling_add_refuse(struct battle *battle,int xish) {
  if (BATTLE->refusec>=REFUSE_LIMIT) return;
  int y=GROUNDY+5+rand()%20;
  int lo=0,hi=BATTLE->refusec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct refuse *other=BATTLE->refusev+ck;
         if (y<other->y) hi=ck;
    else if (y>other->y) lo=ck+1;
    else { lo=ck; break; }
  }
  struct refuse *refuse=BATTLE->refusev+lo;
  memmove(refuse+1,refuse,sizeof(struct refuse)*(BATTLE->refusec-lo));
  BATTLE->refusec++;
  refuse->x=xish+(rand()%30)-15;
  refuse->y=y;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (player->loadclock>0.0) {
    player->loadclock-=elapsed;
    if (player->loadclock<=0.0) {
      player->loaded=1;
    }
  }

  if (battle->outcome>-2) {
    player->indrop=0;
  }
  
  if (player->indrop!=player->pvindrop) {
    if (player->pvindrop=player->indrop) {
      shovelling_drop_coal(battle,player);
    }
  }
  
  /* Cart goes outward if full, inward if empty.
   */
  double cartstop;
  double cartdx=player->cartspeed*elapsed;
  if (player->who) {
    if (!player->cartfull) { // Right side enter.
      cartdx=-cartdx;
      cartstop=FBW-PLATFORMW-20.0;
    } else { // Right side exit.
      cartstop=FBW+20.0;
    }
  } else {
    if (!player->cartfull) { // Left side enter.
      cartstop=PLATFORMW+20.0;
    } else { // Left side exit.
      cartdx=-cartdx;
      cartstop=-20.0;
    }
  }
  player->cartx+=cartdx;
  if (
    ((cartdx>0.0)&&(player->cartx>=cartstop))||
    ((cartdx<0.0)&&(player->cartx<=cartstop))
  ) {
    player->cartx=cartstop;
    if (player->cartfull) player->cartfull=0;
  }
  
  /* Coal falls.
   */
  if (player->coaly>=player->platformy) {
    player->coaly+=100.0*elapsed;
    if (player->coaly>=GROUNDY-20.0) {
      if (shovelling_cart_ready(battle,player)) {
        bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->cartfull=1;
        player->coaly=0.0;
        if ((player->score>=0)&&(player->score<SCORE_LIMIT)) player->scorev[player->score].tileid=0x37;
        player->score++;
      } else if (player->coaly>=GROUNDY-5.0) { // There's a brief window where it can catch, not just a single point.
        bm_sound_pan(RID_sound_weebump,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->coaly=0.0;
        player->cartspeed*=player->waste_penalty;
        if (player->cartspeed<25.0) player->cartspeed=25.0;
        shovelling_add_refuse(battle,player->who?(FBW-PLATFORMW-20):(PLATFORMW+20));
      }
    }
  }
}

/* Update.
 */
 
static void _shovelling_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  /* Battle ends when somebody reaches SCORE_LIMIT.
   * Ties are narrowly possible.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->score>=SCORE_LIMIT) battle->outcome=(r->score>=SCORE_LIMIT)?0:1;
    else if (r->score>=SCORE_LIMIT) battle->outcome=-1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  // Hero.
  int y2=player->platformy-(NS_sys_tilesize>>1);
  int y1=y2-NS_sys_tilesize;
  int y0=y1-NS_sys_tilesize;
  int x0,x1,x2;
  uint8_t xform;
  if (player->who) {
    x1=FBW-PLATFORMW+NS_sys_tilesize;
    x0=x1+NS_sys_tilesize;
    x2=x1-NS_sys_tilesize;
    xform=EGG_XFORM_XREV;
  } else {
    x1=PLATFORMW-NS_sys_tilesize;
    x0=x1-NS_sys_tilesize;
    x2=x1+NS_sys_tilesize;
    xform=0;
  }
  int farx=x0;
  int nearx=x2;
  uint8_t pxform=xform;
  if (!player->loaded) { // Face the other way.
    int tmp=x0;
    x0=x2;
    x2=tmp;
    pxform^=EGG_XFORM_XREV;
  }
  uint8_t tileid=player->tileid;
  graf_tile(g_graf,x0,y0,tileid+0x00,pxform);
  graf_tile(g_graf,x1,y0,tileid+0x01,pxform);
  graf_tile(g_graf,x2,y0,tileid+0x02,pxform);
  graf_tile(g_graf,x0,y1,tileid+0x10,pxform);
  graf_tile(g_graf,x1,y1,tileid+0x11,pxform);
  graf_tile(g_graf,x2,y1,tileid+0x12,pxform);
  graf_tile(g_graf,x0,y2,tileid+0x20,pxform);
  graf_tile(g_graf,x1,y2,tileid+0x21,pxform);
  graf_tile(g_graf,x2,y2,tileid+0x22,pxform);
  
  // Shovel.
  int shovelx=x2;
  if (pxform) shovelx-=NS_sys_tilesize;
  else shovelx+=NS_sys_tilesize;
  int shovely=y2-7;
  graf_tile(g_graf,shovelx,shovely,0x33,pxform);
  if (player->loaded) graf_tile(g_graf,shovelx,shovely,0x34,pxform);
  
  // Falling coal.
  if (player->coaly>=player->platformy) {
    int coalx=player->who?(FBW-PLATFORMW-20):(PLATFORMW+20);
    graf_tile(g_graf,coalx,(int)player->coaly,0x35,0);
  }
  
  // Coal pile.
  int coaldx=player->who?NS_sys_tilesize:-NS_sys_tilesize;
  int coalx=farx+coaldx;
  graf_tile(g_graf,coalx,y0,0x4f,xform);
  graf_tile(g_graf,coalx,y1,0x5f,xform);
  graf_tile(g_graf,coalx,y2,0x6f,xform);
  coalx+=coaldx;
  graf_tile(g_graf,coalx,y0,0x4e,xform);
  graf_tile(g_graf,coalx,y1,0x5e,xform);
  graf_tile(g_graf,coalx,y2,0x6e,xform);
  coalx+=coaldx;
  graf_tile(g_graf,coalx,y0,0x4d,xform);
  graf_tile(g_graf,coalx,y1,0x5d,xform);
  graf_tile(g_graf,coalx,y2,0x6d,xform);
  coalx+=coaldx;
  graf_tile(g_graf,coalx,y1,0x5c,xform);
  graf_tile(g_graf,coalx,y2,0x6c,xform);
  coalx+=coaldx;
  graf_tile(g_graf,coalx,y1,0x5b,xform);
  graf_tile(g_graf,coalx,y2,0x6b,xform);
  
  // Cart.
  int cx0=(int)player->cartx-(NS_sys_tilesize>>1);
  int cx1=cx0+NS_sys_tilesize;
  int cy1=GROUNDY-(NS_sys_tilesize>>1);
  int cy0=cy1-NS_sys_tilesize;
  if (player->cartfull) {
    graf_tile(g_graf,cx0,cy0,0x4b,0);
    graf_tile(g_graf,cx1,cy0,0x4c,0);
  } else {
    graf_tile(g_graf,cx0,cy0,0x49,0);
    graf_tile(g_graf,cx1,cy0,0x4a,0);
  }
  graf_tile(g_graf,cx0,cy1,0x59,0);
  graf_tile(g_graf,cx1,cy1,0x5a,0);
  uint8_t rot=(uint8_t)(player->cartx*2.0);
  graf_set_filter(g_graf,1);
  graf_fancy(g_graf,cx0,cy1+4,0x6a,0,rot,NS_sys_tilesize,0,0x808080ff);
  graf_fancy(g_graf,cx1,cy1+4,0x6a,0,rot,NS_sys_tilesize,0,0x808080ff);
  graf_set_filter(g_graf,0);
  
  // Scoreboard, my half.
  graf_tile_batch(g_graf,player->scorev,SCORE_LIMIT);
}

/* Render.
 */
 
static void _shovelling_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,1,0x000000ff);
  
  graf_fill_rect(g_graf,0,l->platformy-1,PLATFORMW+1,PLATFORMH+2,0x000000ff);
  graf_fill_rect(g_graf,0,l->platformy,PLATFORMW,PLATFORMH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,FBW-PLATFORMW-1,r->platformy-1,PLATFORMW+1,PLATFORMH+2,0x000000ff);
  graf_fill_rect(g_graf,FBW-PLATFORMW,r->platformy,PLATFORMW,PLATFORMH,battle->ctab[BATTLE_COLOR_GROUND]);
  
  graf_set_image(g_graf,RID_image_battle_underground2);
  player_render(battle,l);
  player_render(battle,r);
  
  struct refuse *refuse=BATTLE->refusev;
  int i=BATTLE->refusec;
  for (;i-->0;refuse++) graf_tile(g_graf,refuse->x,refuse->y,0x34,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_shovelling={
  .name="shovelling",
  .objlen=sizeof(struct battle_shovelling),
  .id=136,
  .strix_name=435,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .imageid_default=RID_image_caves,
  .del=_shovelling_del,
  .init=_shovelling_init,
  .update=_shovelling_update,
  .render=_shovelling_render,
};

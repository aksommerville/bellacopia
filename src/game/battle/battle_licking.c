/* battle_licking.c
 */

#include "game/batsup/battle_internal.h"

#define CONE_RADIUS 70.0
#define CONE_RATE 3.000 /* rad/sec */
#define HEARTC 5

struct battle_licking {
  struct battle hdr;
  
  double conex;
  int coney;
  double conep; // Radians.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    int x,y; // Center of 2x2-tile head sprite.
    int heartc;
    double score; // 0..1
    double tongue; // 0..1 = closed..out
    double dtongue;
    double tongue_rate; // How many one-way travels of tongue per second.
    double cone_distance_min; // How far before the cone is unreachable, pixels.
    double score_max; // hz, peak score rate, at minimum distance and maximum tongue.
    double cpulo,cpuhi; // Positive radians, how far away do we start licking.
    double cpup; // Choice for next lick. <=0 if unset.
  } playerv[2];
};

#define BATTLE ((struct battle_licking*)battle)

/* Delete.
 */
 
static void _licking_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=60;
  player->y=FBH>>1;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  
  // With these settings, I can beat him at 0xff but it's real chancey, usually he wins.
  player->tongue_rate=1.500*(1.0-player->skill)+3.000*player->skill;
  player->cone_distance_min=20.0;
  player->score_max=0.400*(1.0-player->skill)+0.500*player->skill;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    if (battle->args.lctl||battle->args.rctl) { // CPU penalty if the opponent is human.
      player->score_max*=0.750;
    }
    double perfect=CONE_RATE/player->tongue_rate;
    double range=0.500*(1.0-player->skill)+0.100*player->skill;
    player->cpulo=perfect-range*0.5;
    player->cpuhi=perfect+range*0.5;
    if (player->cpulo<0.001) player->cpulo=0.001;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x997e45ff;
        player->tileid=0x40;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x00;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x20;
      } break;
  }
}

/* New.
 */
 
static int _licking_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  BATTLE->conep=((rand()&0xffff)*M_PI*2.0)/65535.0-M_PI;
  BATTLE->conex=FBW*0.5+sin(BATTLE->conep)*CONE_RADIUS;
  BATTLE->coney=(FBH>>1)+NS_sys_tilesize;
  
  return 0;
}

/* Begin licking if possible.
 */
 
static void licking_lick(struct battle *battle,struct player *player) {
  if (player->tongue>0.0) { // You got just the one tongue.
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  player->tongue=0.001;
  player->dtongue=player->tongue_rate;
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) licking_lick(battle,player);
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // If currently licking, there's nothing to do.
  if (player->tongue>0.0) return;
  
  // Choose a random starting point if we don't have one yet.
  if (player->cpup<=0.0) {
    double n=(rand()&0xffff)/65535.0;
    player->cpup=player->cpulo*(1.0-n)+player->cpuhi*n;
  }
  
  // Rotate the cone's position to phrase it as postive radians, counting down to the nearest point.
  double p;
  if (player->who) {
    p=M_PI*0.5-BATTLE->conep;
  } else {
    p=M_PI*-0.5-BATTLE->conep;
  }
  while (p<0.0) p+=M_PI*2.0;
  
  // And when it crosses the threshold, lick.
  if (p<player->cpup) {
    licking_lick(battle,player);
    player->cpup=0.0;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Move the tongue if in play.
  if (player->tongue>0.0) {
    player->tongue+=player->dtongue*elapsed;
    if (player->tongue>1.0) {
      player->tongue=1.0;
      if (player->dtongue>0.0) player->dtongue=-player->dtongue;
    } else if (player->tongue<0.0) {
      player->tongue=0.0;
    }
    
    /* Check overlap against the cone.
     * Cone's position has already been determined, so work off that instead of recomputing it.
     */
    double distance;
    if (player->who) {
      distance=FBW*0.5+CONE_RADIUS-BATTLE->conex;
    } else {
      distance=BATTLE->conex-(FBW*0.5-CONE_RADIUS);
    }
    if (distance<player->cone_distance_min) {
      double quality=(player->cone_distance_min-distance)/player->cone_distance_min;
      player->score+=quality*player->score_max*elapsed;
      if (player->score>1.0) player->score=1.0;
      int nheartc=(int)(player->score*HEARTC);
      if (nheartc>player->heartc) {
        bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->heartc=nheartc;
      }
    }
  }
}

/* Update.
 */
 
static void _licking_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  BATTLE->conep+=CONE_RATE*elapsed;
  if (BATTLE->conep>=M_PI) BATTLE->conep-=M_PI*2.0;
  BATTLE->conex=FBW*0.5+sin(BATTLE->conep)*CONE_RADIUS;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Game ends when somebody's heart meter is full.
   * Ties are not possible, since only one tongue can reach the cone at a time.
   * But we'll check anyway, just to be consistent.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if ((l->heartc>=HEARTC)&&(r->heartc>=HEARTC)) battle->outcome=0;
  else if (l->heartc>=HEARTC) battle->outcome=1;
  else if (r->heartc>=HEARTC) battle->outcome=-1;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x0,x1; // x0 is the back side, x1 the front.
  if (player->xform) {
    x0=player->x+(NS_sys_tilesize>>1);
    x1=player->x-(NS_sys_tilesize>>1);
  } else {
    x0=player->x-(NS_sys_tilesize>>1);
    x1=player->x+(NS_sys_tilesize>>1);
  }
  int y0=player->y-(NS_sys_tilesize>>1);
  int y1=y0+NS_sys_tilesize;
  uint8_t tileid=player->tileid;
  if (player->tongue>0.0) {
    tileid+=2;
    int tonguelen=lround(5.0+player->tongue*27.0);
    if (tonguelen>0) {
      int srcx=((player->tileid&15)+4)*NS_sys_tilesize;
      int srcy=(player->tileid>>4)*NS_sys_tilesize+NS_sys_tilesize;
      srcx+=32-tonguelen;
      if (player->xform) {
        graf_decal_xform(g_graf,player->x-tonguelen,player->y,srcx,srcy,tonguelen,NS_sys_tilesize,player->xform);
      } else {
        graf_decal(g_graf,player->x,player->y,srcx,srcy,tonguelen,NS_sys_tilesize);
      }
    }
  }
  graf_tile(g_graf,x0,y0,tileid+0x00,player->xform);
  graf_tile(g_graf,x1,y0,tileid+0x01,player->xform);
  graf_tile(g_graf,x0,y1,tileid+0x10,player->xform);
  graf_tile(g_graf,x1,y1,tileid+0x11,player->xform);
}

/* Render.
 */
 
static void _licking_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_set_image(g_graf,RID_image_battle_meadow2);
  
  { // Cone.
    int x0=(int)BATTLE->conex-(NS_sys_tilesize>>1);
    int y0=BATTLE->coney-(NS_sys_tilesize>>1);
    int x1=x0+NS_sys_tilesize;
    int y1=y0+NS_sys_tilesize;
    uint8_t tileidl=0x06+(int)(l->score*10.0); if (tileidl<0x06) tileidl=0x06; else if (tileidl>0x0f) tileidl=0x0f;
    uint8_t tileidr=0x06+(int)(r->score*10.0); if (tileidr<0x06) tileidr=0x06; else if (tileidr>0x0f) tileidr=0x0f;
    graf_tile(g_graf,x0,y0,tileidl,0);
    graf_tile(g_graf,x1,y0,tileidr,EGG_XFORM_XREV);
    graf_tile(g_graf,x0,y1,0x16,0);
    graf_tile(g_graf,x1,y1,0x16,EGG_XFORM_XREV);
  }
  
  player_render(battle,l);
  player_render(battle,r);
  
  { // Scoreboard.
    int lx=(FBW>>1)-6;
    int rx=(FBW>>1)+6;
    int y=(FBH>>1)-20;
    int i=0;
    for (;i<HEARTC;i++,y-=10) {
      graf_fancy(g_graf,lx,y,(i<l->heartc)?0x18:0x17,0,0,NS_sys_tilesize,0,l->color);
      graf_fancy(g_graf,rx,y,(i<r->heartc)?0x18:0x17,0,0,NS_sys_tilesize,0,r->color);
    }
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_licking={
  .name="licking",
  .objlen=sizeof(struct battle_licking),
  .id=126,
  .strix_name=415,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_a,
  .imageid_default=RID_image_meadow,
  .del=_licking_del,
  .init=_licking_init,
  .update=_licking_update,
  .render=_licking_render,
};

/* battle_climbing.c
 */

#include "game/batsup/battle_internal.h"

#define WALL_COLC 20 /* Full screen width. */
#define WALL_ROWC 8 /* Some margin above and below. */
#define BGVTXC (WALL_COLC*WALL_ROWC)
#define GROUNDY 160
#define TOPY (GROUNDY-WALL_ROWC*NS_sys_tilesize)

struct battle_climbing {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    int x;
    double y;
    int lstate,rstate; // State of each side. -1,0,1. Off the ground if both nonzero.
    double fallclock; // >0 and counting up, if we're falling.
    int blackout;
    int pvlstate,pvrstate;
    int lnext,rnext; // -1,0,1: Each stroke on a side must alternate direction.
    int climbside; // -1,0,1; 0 only at the ground
    double climb_interval;
    int win;
    double cpuclock;
    double cputimelo,cputimehi;
  } playerv[2];
  
  struct egg_render_tile bgvtxv[BGVTXC];
};

#define BATTLE ((struct battle_climbing*)battle)

/* Delete.
 */
 
static void _climbing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=130;
  player->y=GROUNDY;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
  }
  
  player->climb_interval=5.0*(1.0-player->skill)+10.0*player->skill;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpuclock=1.000; // Give the puny mortal a wee head start.
    player->cputimehi=0.600*(1.0-player->skill)+0.350*player->skill;
    player->cputimelo=player->cputimelo*0.5;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x808080ff;
        player->tileid=0x46;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x26;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x36;
      } break;
  }
}

/* New.
 */
 
static int _climbing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  /* Generate the background tiles. It's decorative.
   */
  struct egg_render_tile *vtx=BATTLE->bgvtxv;
  int row=0,xi,x,y=TOPY+(NS_sys_tilesize>>1);
  for (xi=WALL_COLC,x=NS_sys_tilesize>>1;xi-->0;x+=NS_sys_tilesize,vtx++) {
    vtx->x=x;
    vtx->y=y;
    vtx->tileid=0x22+(rand()&3);
    vtx->xform=0;
  }
  row++; y+=NS_sys_tilesize;
  for (;row<WALL_ROWC;row++,y+=NS_sys_tilesize) {
    for (xi=WALL_COLC,x=NS_sys_tilesize>>1;xi-->0;x+=NS_sys_tilesize,vtx++) {
      vtx->x=x;
      vtx->y=y;
      vtx->tileid=0x32+(rand()&0x13);
      vtx->xform=0;
    }
  }
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&(EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_SOUTH|EGG_BTN_WEST))) player->blackout=0;
  } else {
    switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
      case EGG_BTN_UP: player->lstate=-1; break;
      case EGG_BTN_DOWN: player->lstate=1; break;
      default: player->lstate=0; break;
    }
    switch (input&(EGG_BTN_SOUTH|EGG_BTN_WEST)) {
      case EGG_BTN_WEST: player->rstate=-1; break;
      case EGG_BTN_SOUTH: player->rstate=1; break;
      default: player->rstate=0; break;
    }
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->fallclock>0.0) {
    player->lstate=player->rstate=0;
    player->cpuclock=0.500;
  } else if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
  } else {
    double n=(rand()&0xffff)/65535.0;
    player->cpuclock+=player->cputimelo*(1.0-n)+player->cputimehi*n;
    if (!player->lstate&&!player->rstate) {
      if (rand()&1) player->lstate=-1; else player->rstate=-1;
    } else if (!player->lstate) {
      if (player->lnext) player->lstate=player->lnext;
      else player->lstate=-1;
    } else if (!player->rstate) {
      if (player->rnext) player->rstate=player->rnext;
      else player->rstate=-1;
    } else { // Both on. Pick one to release.
      if (player->climbside<0) player->rstate=0;
      else player->lstate=0;
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* If we already won, great, stop doing things.
   */
  if (player->win) return;

  /* If we're falling, all we do is fall.
   */
  if (player->fallclock>0.0) {
    player->fallclock+=elapsed;
    if ((player->y+=100.0*elapsed)>=GROUNDY) {
      player->y=GROUNDY;
      player->fallclock=0.0;
      bm_sound_pan(RID_sound_bump,player->who?PLAYER_PAN:-PLAYER_PAN);
    }
    return;
  }
  
  /* Things only happen when input states change.
   */
  if ((player->pvlstate!=player->lstate)||(player->pvrstate!=player->rstate)) {
    if (!player->lstate&&!player->rstate) { // No hands!
      if (player->y<GROUNDY) {
        player->fallclock=0.001;
        player->climbside=0;
        player->lnext=0;
        player->rnext=0;
        bm_sound_pan(RID_sound_oops,player->who?PLAYER_PAN:-PLAYER_PAN);
      }
    } else if (player->lstate&player->rstate) { // Both hands connected: Climb. (or noop, if you touch the same side twice)
      if (player->lstate!=player->pvlstate) {
        if ((player->climbside>=0)&&(!player->lnext||(player->lnext==player->lstate))) {
          bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
          player->climbside=-1;
          player->y-=player->climb_interval;
        }
        player->lnext=-player->lstate;
      } else {
        if ((player->climbside<=0)&&(!player->rnext||(player->rnext==player->rstate))) {
          bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
          player->climbside=1;
          player->y-=player->climb_interval;
        }
        player->rnext=-player->rstate;
      }
      if (player->y<=TOPY) {
        player->y=TOPY-6;
        player->win=1;
        bm_sound_pan(RID_sound_treasure,player->who?PLAYER_PAN:-PLAYER_PAN);
      }
    } else if (!player->climbside) { // First contact, at the ground.
      bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
      if (player->lstate) {
        player->climbside=-1;
      } else {
        player->climbside=1;
      }
    }
    player->pvlstate=player->lstate;
    player->pvrstate=player->rstate;
  }
}

/* Update.
 */
 
static void _climbing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  /* Game ends when a player reaches the top.
   * Narrowly possible for both to reach it on the same frame.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->win&&r->win) battle->outcome=0;
  else if (l->win) battle->outcome=1;
  else if (r->win) battle->outcome=-1;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int y=(int)player->y;
  uint8_t tileid=player->tileid;
  if (player->win) { // Sitting on top.
    tileid+=5;
  } else if (player->y>=GROUNDY) { // On the ground: +0,1,2 = neutral,left,right
    if (player->climbside<0) tileid+=1;
    else if (player->climbside>0) tileid+=2;
  } else if (player->fallclock>0.0) { // Falling.
    tileid+=6;
  } else { // Climbing.
    if (player->climbside<0) tileid+=3;
    else tileid+=4;
  }
  graf_tile(g_graf,player->x,y,tileid,0);
  if (player->fallclock>0.0) { // Draw my hat, catching up.
    int haty=y-4-(int)(player->fallclock*30.0);
    graf_tile(g_graf,player->x,haty,player->tileid+7,0);
  }
}

/* Render.
 */
 
static void _climbing_render(struct battle *battle) {

  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,FBH>>1,FBW,FBH>>1,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(g_graf,RID_image_battle_karatechop);
  graf_tile_batch(g_graf,BATTLE->bgvtxv,BGVTXC);
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_climbing={
  .name="climbing",
  .objlen=sizeof(struct battle_climbing),
  .id=113,
  .strix_name=386,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_vert_ab,
  .imageid_default=RID_image_caves,
  .del=_climbing_del,
  .init=_climbing_init,
  .update=_climbing_update,
  .render=_climbing_render,
};

/* battle_parachute.c
 */

#include "game/batsup/battle_internal.h"

#define GROUNDY 150

struct battle_parachute {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    double animclock;
    int animframe;
    int blackout;
    int x; // constant
    double y;
    double gravity;
    double gravity_rate;
    double gravity_limit;
    int chuted;
    int landed;
    double openy;
    double cpuchutey;
  } playerv[2];
};

#define BATTLE ((struct battle_parachute*)battle)

/* Delete.
 */
 
static void _parachute_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {

  player->x=140;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->x=FBW-player->x;
  }
  
  player->gravity_rate=140.0*(1.0-player->skill)+80.0*player->skill;
  player->gravity_limit=120.0*(1.0-player->skill)+90.0*player->skill;
  if (player->who) player->gravity_limit+=1.0; // Keep them from being identical, even at balanced bias.
  else player->gravity_rate+=1.0;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    if (!(rand()%5)) { // Throw it every so often, regardless of skill, to keep them on their toes. Can't just watch me and hit right after.
      player->cpuchutey=GROUNDY+10.0;
    } else {
      player->cpuchutey=100.0*(1.0-player->skill)+140.0*player->skill;
      player->cpuchutey+=(12.0*(rand()&0xffff)/65535.0)-6.0;
    }
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xad1d11ff;
        player->tileid=0x43;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x23;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x33;
      } break;
  }
}

/* New.
 */
 
static int _parachute_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Open my parachute.
 */
 
static void parachute_open(struct battle *battle,struct player *player) {
  if (player->chuted||player->landed) return;
  player->openy=player->y;
  player->chuted=1;
  player->gravity_limit=30.0;
  bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else if (!player->chuted&&!player->landed) {
    if (input&EGG_BTN_SOUTH) parachute_open(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->y>=player->cpuchutey) parachute_open(battle,player);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->landed) return;
  
  player->y+=player->gravity*elapsed;
  player->gravity+=player->gravity_rate*elapsed;
  if (player->gravity>player->gravity_limit) player->gravity=player->gravity_limit;
  if (player->y>=GROUNDY-4.0) {
    player->y=GROUNDY-4.0;
    player->landed=1;
    if (player->chuted) {
      bm_sound_pan(RID_sound_weebump,player->who?PLAYER_PAN:-PLAYER_PAN);
    } else {
      bm_sound_pan(RID_sound_splat,player->who?PLAYER_PAN:-PLAYER_PAN);
    }
  }
  
  if (!player->chuted&&!player->landed) {
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.100;
      if (++(player->animframe)>=2) player->animframe=0;
    }
  }
}

/* Update.
 */
 
static void _parachute_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Game is over when they've both landed.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->landed&&r->landed) {
    if (l->chuted&&r->chuted) { // Both landed safely. Whoever got there first wins.
      if (l->openy>r->openy) battle->outcome=1;
      else if (l->openy<r->openy) battle->outcome=-1;
      else battle->outcome=0;
    } else if (l->chuted) battle->outcome=1;
    else if (r->chuted) battle->outcome=-1;
    else battle->outcome=0; // double splat
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  // Line in the middle showing where I opened.
  if (player->chuted) {
    graf_set_input(g_graf,0);
    int ax,bx;
    if (player->who) {
      ax=FBW>>1;
      bx=ax+8;
    } else {
      bx=FBW>>1;
      ax=bx-8;
    }
    int y=(int)player->openy;
    graf_line(g_graf,ax,y,player->color,bx,y,player->color);
    graf_set_image(g_graf,RID_image_battle_desert);
  }

  int x=(int)player->x;
  int y=(int)player->y-3;
  uint8_t tileid=player->tileid;
  if (player->chuted) {
    int csx=NS_sys_tilesize*3;
    int csy=NS_sys_tilesize*5;
    if (player->landed) csx+=NS_sys_tilesize*3;
    graf_decal(g_graf,x-NS_sys_tilesize-(NS_sys_tilesize>>1),y-NS_sys_tilesize-(NS_sys_tilesize>>1),csx,csy,NS_sys_tilesize*3,NS_sys_tilesize*2);
    tileid+=2; // Whether landed or not.
  } else if (player->landed) {
    tileid+=5;
  } else {
    tileid+=player->animframe;
  }
  graf_tile(g_graf,x,y,tileid,player->xform);
}

/* Render.
 */
 
static void _parachute_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(g_graf,RID_image_battle_desert);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_parachute={
  .name="parachute",
  .objlen=sizeof(struct battle_parachute),
  .id=116,
  .strix_name=392,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_a,
  .imageid_default=RID_image_desert,
  .del=_parachute_del,
  .init=_parachute_init,
  .update=_parachute_update,
  .render=_parachute_render,
};

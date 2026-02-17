/* battle_chess.c
 * Canned chess scenarios where the player is one move away from checkmate, and they have to find that move.
 * Piece movements should be modelled generically against the real rules of chess.
 * Do allow a 2-player mode, which is actually just chess.
 * But no real man-vs-cpu mode, just those canned scenarios.
 * TODO placeholder
 */

#include "game/bellacopia.h"

struct battle_chess {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
  } playerv[2];
};

#define BATTLE ((struct battle_chess*)battle)

/* Delete.
 */
 
static void _chess_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
      } break;
    case NS_face_dot: {
      } break;
    case NS_face_princess: {
      } break;
  }
}

/* New.
 */
 
static int _chess_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  //TODO
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  //TODO
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  //TODO
}

/* Update.
 */
 
static void _chess_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  //XXX Placeholder UI.
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) { if (--(BATTLE->choice)<-1) BATTLE->choice=1; }
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) { if (++(BATTLE->choice)>1) BATTLE->choice=-1; }
  if ((battle->outcome<-1)&&(g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    bm_sound(RID_sound_uiactivate);
    battle->outcome=BATTLE->choice;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
}

/* Render.
 */
 
static void _chess_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  
  // XXX Placeholder UI.
  int y=FBH/3;
  int boxh=20;
  int boxw=20;
  int y1=y+boxh+1;
  int xv[3]={
    (FBW>>2)-(boxw>>1),
    (FBW>>1)-(boxw>>1),
    ((FBW*3)>>2)-(boxw>>1),
  };
  graf_fill_rect(&g.graf,xv[0],y,boxw,boxh,0xff0000ff);
  graf_fill_rect(&g.graf,xv[1],y,boxw,boxh,0x404040ff);
  graf_fill_rect(&g.graf,xv[2],y,boxw,boxh,0x00ff00ff);
  switch (BATTLE->choice) {
    case -1: graf_fill_rect(&g.graf,xv[0],y1,boxw,boxh,0xffffffff); break;
    case  0: graf_fill_rect(&g.graf,xv[1],y1,boxw,boxh,0xffffffff); break;
    case  1: graf_fill_rect(&g.graf,xv[2],y1,boxw,boxh,0xffffffff); break;
  }
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_chess={
  .name="chess",
  .objlen=sizeof(struct battle_chess),
  .strix_name=180,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_chess_del,
  .init=_chess_init,
  .update=_chess_update,
  .render=_chess_render,
};

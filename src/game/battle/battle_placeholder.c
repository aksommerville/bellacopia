/* battle_placeholder.c
 */

#include "game/bellacopia.h"

struct battle_placeholder {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
  } playerv[2];
};

#define BATTLE ((struct battle_placeholder*)battle)

/* Delete.
 */
 
static void _placeholder_del(struct battle *battle) {
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
        player->color=0x808080ff;//TODO
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
}

/* New.
 */
 
static int _placeholder_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
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
 
static void _placeholder_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
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

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
}

/* Render.
 */
 
static void _placeholder_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_placeholder={
  .name="placeholder",
  .objlen=sizeof(struct battle_placeholder),
  .id=NS_battle_placeholder,
  .strix_name=20,//TODO
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_none,//TODO
  .imageid_default=0,
  .del=_placeholder_del,
  .init=_placeholder_init,
  .update=_placeholder_update,
  .render=_placeholder_render,
};

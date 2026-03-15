/* battle_mindcontrol.c
 * Alternate A/B in the right rhythm to maintain psychic control. While control established, dpad moves your victim.
 * Push a piece of candy thru the hazards.
 * TODO placeholder
 */

#include "game/bellacopia.h"

#define GROUNDY 150

struct battle_mindcontrol {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    int srcx,srcy;
    int frame;
    uint8_t xform;
    int dstx;
  } playerv[2];
};

#define BATTLE ((struct battle_mindcontrol*)battle)

/* Delete.
 */
 
static void _mindcontrol_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->dstx=FBW/3;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->dstx=(FBW*2)/3;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->srcx=0;
        player->srcy=160;
      } break;
    case NS_face_dot: {
        player->srcx=0;
        player->srcy=64;
      } break;
    case NS_face_princess: {
        player->srcx=0;
        player->srcy=112;
      } break;
  }
}

/* New.
 */
 
static int _mindcontrol_init(struct battle *battle) {
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
 
static void _mindcontrol_update(struct battle *battle,double elapsed) {
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
  graf_decal_xform(&g.graf,player->dstx-24,GROUNDY-48,player->srcx+player->frame*48,player->srcy,48,48,player->xform);
}

/* Render.
 */
 
static void _mindcontrol_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x80a0e0ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,0x105020ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_labyrinth2);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_mindcontrol={
  .name="mindcontrol",
  .objlen=sizeof(struct battle_mindcontrol),
  .strix_name=177,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_mindcontrol_del,
  .init=_mindcontrol_init,
  .update=_mindcontrol_update,
  .render=_mindcontrol_render,
};

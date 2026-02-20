/* battle_cakecarrying.c
 * Balance a cake while standing on a train that's stopping.
 */

#include "game/bellacopia.h"

#define LAYERC 4

struct battle_cakecarrying {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid;
    int x,y; // Middle of head, in framebuffer pixels.
    struct layer {
      double x,y; // Position relative to the stack. First, bottom, is always (0,0).
      double t; // Radians clockwise.
      double w,h;
      uint8_t tileid;
    } layerv[LAYERC];
  } playerv[2];
};

#define BATTLE ((struct battle_cakecarrying*)battle)

/* Delete.
 */
 
static void _cakecarrying_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->y=100;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW/3;
  } else { // Right.
    player->who=1;
    player->x=(FBW*2)/3;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x74;
      } break;
    case NS_face_dot: {
        player->tileid=0x34;
      } break;
    case NS_face_princess: {
        player->tileid=0x54;
      } break;
  }
  player->layerv[0]=(struct layer){0.0,  0.0,0.0,12.0,8.0,0xb9};
  player->layerv[1]=(struct layer){0.0, -7.0,0.0,10.0,6.0,0xba};
  player->layerv[2]=(struct layer){0.0,-13.0,0.0, 8.0,6.0,0xbb};
  player->layerv[3]=(struct layer){0.0,-19.0,0.0, 6.0,6.0,0xbc};
}

/* New.
 */
 
static int _cakecarrying_init(struct battle *battle) {
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
 
static void _cakecarrying_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  //XXX Placeholder UI.
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render player.
 * The body and the cake are separate to promote batching, because they'll use tiles and fancies respectively.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  graf_tile(&g.graf,player->x,player->y,player->tileid,0);
  graf_tile(&g.graf,player->x,player->y+NS_sys_tilesize,player->tileid+0x10,0);
}

static void cake_render(struct battle *battle,struct player *player) {
  double x=player->x+4.0;
  double y=player->y+13.0;
  struct layer *layer=player->layerv;
  int i=LAYERC;
  for (;i-->0;layer++) {
    uint8_t ti=(int8_t)((layer->t*128.0)/M_PI);
    graf_fancy(&g.graf,(int)(x+layer->x),(int)(y+layer->y),layer->tileid,0,ti,NS_sys_tilesize,0,0x808080ff);
  }
}

/* Render.
 */
 
static void _cakecarrying_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_fractia);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  cake_render(battle,BATTLE->playerv+0);
  cake_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_cakecarrying={
  .name="cakecarrying",
  .objlen=sizeof(struct battle_cakecarrying),
  .strix_name=154,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_cakecarrying_del,
  .init=_cakecarrying_init,
  .update=_cakecarrying_update,
  .render=_cakecarrying_render,
};

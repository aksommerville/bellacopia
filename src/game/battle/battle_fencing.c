/* battle_fencing.c
 * Tap A to place fenceposts.
 */

#include "game/bellacopia.h"

#define PLANKC_WIN   24 /* One player gets so many, they win. */
#define PLANKC_LIMIT 45 /* Total planks across both players can't exceed this; stop if we reach it. */

struct battle_fencing {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid;
    uint8_t xform;
    int plankc;
    double installclock;
    double worktime;
    double cpulo,cpuhi; // Delay between actions.
    double cpuclock;
  } playerv[2];
};

#define BATTLE ((struct battle_fencing*)battle)

/* Delete.
 */
 
static void _fencing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cpuhi=player->skill*0.150+(1.0-player->skill)*0.500;
    player->cpulo=player->cpuhi*0.500;
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0xb9;
      } break;
    case NS_face_dot: {
        player->tileid=0x79;
      } break;
    case NS_face_princess: {
        player->tileid=0x99;
      } break;
  }
  player->worktime=0.200*player->skill+0.500*(1.0-player->skill);
  player->plankc=3; // Start with the player visible, but don't let there be empty rails behind her.
}

/* New.
 */
 
static int _fencing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->installclock>0.0) {
    if ((player->installclock-=elapsed)<=0.0) {
      player->plankc++;
    }
  } else if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    player->installclock=player->worktime;
    bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->cpuclock>0.0) {
    if ((player->cpuclock-=elapsed)<=0.0) {
      player->installclock=player->worktime;
      bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
    }
  } else if (player->installclock>0.0) {
    if ((player->installclock-=elapsed)<=0.0) {
      player->plankc++;
    }
  } else {
    player->cpuclock=(rand()&0xffff)/65535.0;
    player->cpuclock=player->cpulo*player->cpuclock+player->cpuhi*(1.0-player->cpuclock);
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  //TODO
}

/* Update.
 */
 
static void _fencing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  int lc=l->plankc; if (l->installclock>0.0) lc++;
  int rc=r->plankc; if (r->installclock>0.0) rc++;
  if (lc>=PLANKC_WIN) battle->outcome=1;
  else if (rc>=PLANKC_WIN) battle->outcome=-1;
  else if (lc+rc>=PLANKC_LIMIT) {
    if (lc>rc) battle->outcome=1;
    else if (lc<r->plankc) battle->outcome=-1;
    else battle->outcome=0; // Narrowly possible, if they both place their last plank on the same frame.
  }
}

/* Render player.
 * Also renders the planks associated with this player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int ht=NS_sys_tilesize>>1;
  int x=4,dx=7;
  if (player->who) {
    x=FBW-x;
    dx=-dx;
  }
  int i=player->plankc;
  for (;i-->0;x+=dx) {
    graf_tile(&g.graf,x,152,0x8e,0);
    graf_tile(&g.graf,x,136,0x7e,0);
    graf_tile(&g.graf,x,120,0x6e,0);
  }
  if (player->installclock>0.0) { // One more plank if our hand is up. It's not counted yet. Do not advance (x).
    graf_tile(&g.graf,x,152,0x8e,0);
    graf_tile(&g.graf,x,136,0x7e,0);
    graf_tile(&g.graf,x,120,0x6e,0);
  }
  if (player->who) x+=12; else x-=12;
  int y=144;
  uint8_t tileid=player->tileid;
  if (player->installclock>0.0) tileid+=2;
  if (player->xform) {
    graf_tile(&g.graf,x-ht,y-ht,tileid+0x01,player->xform);
    graf_tile(&g.graf,x+ht,y-ht,tileid+0x00,player->xform);
    graf_tile(&g.graf,x-ht,y+ht,tileid+0x11,player->xform);
    graf_tile(&g.graf,x+ht,y+ht,tileid+0x10,player->xform);
  } else {
    graf_tile(&g.graf,x-ht,y-ht,tileid+0x00,player->xform);
    graf_tile(&g.graf,x+ht,y-ht,tileid+0x01,player->xform);
    graf_tile(&g.graf,x-ht,y+ht,tileid+0x10,player->xform);
    graf_tile(&g.graf,x+ht,y+ht,tileid+0x11,player->xform);
  }
}

/* Render.
 */
 
static void _fencing_render(struct battle *battle) {
  const int groundy=160;
  graf_fill_rect(&g.graf,0,0,FBW,groundy,0x90d0f0ff);
  graf_fill_rect(&g.graf,0,groundy,FBW,FBH,0x104020ff);
  graf_fill_rect(&g.graf,0,groundy,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_athletes);
  
  // Decorative fenceposts at fixed positions, behind the rails.
  graf_tile(&g.graf, 80,152,0x8d,0);
  graf_tile(&g.graf, 80,136,0x7d,0);
  graf_tile(&g.graf, 80,120,0x6d,0);
  graf_tile(&g.graf,160,152,0x8d,0);
  graf_tile(&g.graf,160,136,0x7d,0);
  graf_tile(&g.graf,160,120,0x6d,0);
  graf_tile(&g.graf,240,152,0x8d,0);
  graf_tile(&g.graf,240,136,0x7d,0);
  graf_tile(&g.graf,240,120,0x6d,0);
  
  // Rails for the planks to attach to.
  int dstx=NS_sys_tilesize>>1;
  for (;dstx<FBW;dstx+=NS_sys_tilesize) {
    graf_tile(&g.graf,dstx,152,0x6b,0);
    graf_tile(&g.graf,dstx,136,0x6b,0);
  }
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_fencing={
  .name="fencing",
  .objlen=sizeof(struct battle_fencing),
  .id=NS_battle_fencing,
  .strix_name=159,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_fencing_del,
  .init=_fencing_init,
  .update=_fencing_update,
  .render=_fencing_render,
};

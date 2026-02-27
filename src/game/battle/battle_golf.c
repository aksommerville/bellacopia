/* battle_golf.c
 * Indoor golf. One stroke, and the ball bounces off walls and ceilings. Try to get it closest to the flag.
 */

#include "game/bellacopia.h"

#define WALL_L 70
#define WALL_R 250
#define WALL_T 60
#define WALL_B 120
#define TMAX 1.600
#define TRATE 3.000
#define SWINGRATE 10.000
#define FLOOR_DECAY 0.750
#define SPEED_MIN 40.0
#define SPEED_MAX 100.0
#define GRAVITY 50.0

struct battle_golf {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    int x,y; // Framebuffer pixels, center of club.
    uint8_t xform;
    uint32_t color;
    double clubt; // Volatile.
    double swing_angle; // Committed.
    int swung,flying,stopped;
    int blackout;
    double ballx,bally;
    double balldx,balldy;
    double cpuangle;
    double headstart;
  } playerv[2];
};

#define BATTLE ((struct battle_golf*)battle)

/* Delete.
 */
 
static void _golf_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=WALL_L+20;
    player->y=WALL_B-8;
    player->ballx=player->x;
    player->bally=WALL_B-2.0;
  } else { // Right.
    player->who=1;
    player->x=WALL_R-20;
    player->y=WALL_B-8;
    player->ballx=player->x;
    player->bally=WALL_B-2.0;
    player->xform=EGG_XFORM_XREV;
  }
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpuangle=((rand()&0xffff)*TMAX)/65535.0;
    player->headstart=1.0;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xa1b1b3ff;
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
 
static int _golf_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* End swing.
 */
 
static void player_whack(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_whack,player->who?0.250:-0.250);
  player->flying=1;
  double n=player->swing_angle/TMAX;
  double speed=n*SPEED_MAX+(1.0-n)*SPEED_MIN;
  if (player->xform) {
    player->balldx=-speed;
  } else {
    player->balldx=speed;
  }
  player->balldy=-speed;
}

/* Begin swing. Must have positive (clubt).
 */
 
static void player_swing(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_swing_racket,player->who?0.250:-0.250);
  player->swung=1;
  player->swing_angle=player->clubt;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->swung) return;
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
    return;
  }
  if (input&EGG_BTN_SOUTH) {
    if ((player->clubt+=TRATE*elapsed)>=TMAX) player->clubt=TMAX;
  } else if (player->clubt>0.0) {
    player_swing(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->swung) return;
  if (player->headstart>0.0) {
    player->headstart-=elapsed;
    return;
  }
  if ((player->clubt+=TRATE*elapsed)>=player->cpuangle) {
    player_swing(battle,player);
  }
}

/* Lose velocity due to collision.
 */
 
static void player_decay(struct battle *battle,struct player *player) {
  player->balldx*=FLOOR_DECAY;
  player->balldy*=FLOOR_DECAY;
  double mag2=player->balldx*player->balldx+player->balldy*player->balldy;
  if (mag2<=1.0) {
    player->stopped=1;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->stopped) {
  } else if (player->flying) {
    player->balldy+=GRAVITY*elapsed;
    player->ballx+=player->balldx*elapsed;
    player->bally+=player->balldy*elapsed;
    if (player->ballx<WALL_L) {
      player->ballx=WALL_L;
      if (player->balldx<0.0) {
        if (player->balldx<-5.0) bm_sound(RID_sound_bounce);
        player->balldx=-player->balldx;
      }
      player_decay(battle,player);
    } else if (player->ballx>WALL_R) {
      player->ballx=WALL_R;
      if (player->balldx>0.0) {
        if (player->balldx>5.0) bm_sound(RID_sound_bounce);
        player->balldx=-player->balldx;
        bm_sound(RID_sound_bounce);
      }
      player_decay(battle,player);
    }
    if (player->bally<WALL_T) {
      player->bally=WALL_T;
      if (player->balldy<0.0) {
        if (player->balldy<-5.0) bm_sound(RID_sound_bounce);
        player->balldy=-player->balldy;
      }
      player_decay(battle,player);
    } else if (player->bally>WALL_B) {
      player->bally=WALL_B;
      if (player->balldy>0.0) {
        if (player->balldy>5.0) bm_sound(RID_sound_bounce);
        player->balldy=-player->balldy;
      }
      player_decay(battle,player);
    }
      
  } else if (player->swung&&(player->clubt>0.0)) {
    if ((player->clubt-=SWINGRATE*elapsed)<=0.0) {
      player->clubt=0.0;
      player_whack(battle,player);
    }
  }
}

/* Update.
 */
 
static void _golf_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  // Done when both balls are stopped.
  // Ties are technically possible but vanishingly rare, so just roll them into p1.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->stopped&&r->stopped) {
    double mid=(WALL_L+WALL_R)*0.5;
    double dl=l->ballx-mid; if (dl<0.0) dl=-dl;
    double dr=r->ballx-mid; if (dr<0.0) dr=-dr;
    if (dl<=dr) battle->outcome=1;
    else battle->outcome=-1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  uint8_t rot;
  if (player->xform) {
    rot=(int8_t)((player->clubt*-128.0)/M_PI);
  } else {
    rot=(int8_t)((player->clubt*128.0)/M_PI);
  }
  graf_fancy(&g.graf,player->x,player->y,0x0f,player->xform,rot,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,lround(player->ballx),lround(player->bally),0x1f,0,0,NS_sys_tilesize,0,player->color);
}

/* Render.
 */
 
static void _golf_render(struct battle *battle) {

  const uint32_t wallcolor=0x005020ff;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x80a0c0ff);
  graf_fill_rect(&g.graf,0,0,FBW,WALL_T,wallcolor);
  graf_fill_rect(&g.graf,0,0,WALL_L,FBH,wallcolor);
  graf_fill_rect(&g.graf,WALL_R,0,FBW,FBH,wallcolor);
  graf_fill_rect(&g.graf,0,WALL_B,FBW,FBH,wallcolor);
  
  graf_set_image(&g.graf,RID_image_battle_labyrinth2);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  graf_tile(&g.graf,(WALL_L+WALL_R)>>1,WALL_B-(NS_sys_tilesize>>1),0x2f,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_golf={
  .name="golf",
  .objlen=sizeof(struct battle_golf),
  .strix_name=178,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_golf_del,
  .init=_golf_init,
  .update=_golf_update,
  .render=_golf_render,
};

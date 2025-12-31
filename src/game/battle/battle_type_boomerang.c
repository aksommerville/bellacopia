#include "game/game.h"

#define GROUND_LEVEL 140
#define SKY_COLOR 0x5e9fc7ff
#define SKY_COLOR_KAPOW 0x356380ff
#define GROUND_COLOR 0x126e29ff
#define END_COOLDOWN_TIME 2.000
#define GRAB_TIME_INITIAL 2.000
#define GRAB_TIME_LIMIT   0.250
#define GRAB_TIME_MLT     0.900
#define RANG_SPEED_INITIAL 120.0
#define RANG_SPEED_LIMIT   300.0
#define RANG_SPEED_MLT     1.070
#define RANG_ROTATE_SPEED 8.000 /* rad/s */
#define RANG_X_RANGE 120.0
#define GRAVITY_ACCEL 200.0 /* px/s**2 */
#define GRAVITY_MAX   100.0 /* px/s */
#define JUMP_DECEL    300.0 /* px/s**2 */
#define JUMP_INITIAL -150.0 /* px/s */
#define JUDGMENT_EASIEST 3
#define JUDGMENT_HARDEST 15

#define FACE_IDLE 0
#define FACE_JUMP 1
#define FACE_DUCK 2
#define FACE_HURT 3

struct battle_boomerang {
  int playerc;
  int handicap;
  void (*cb_finish)(void *userdata,int outcome);
  void *userdata;
  int outcome;
  double end_cooldown;
  int input_blackout[3];
  double grab_time;
  double rang_speed;
  
  struct player {
    int human; // 0 for CPU, otherwise the playerid (1,2).
    int srcx,srcy;
    int dstx;
    uint8_t xform;
    int face;
    double gravity; // <0 if jumping, >0 if falling.
    double el; // <0 when airborne, pixels.
    int judgment; // CPU only. Counts down at each pass of the boomerang, and we guess wrong when it hits zero.
    int reacting; // CPU only.
    int jumpok; // Must return to the ground and release button between jumps.
  } playerv[2];
  
  // The whole game is "boomerang", so one of the boomerangs is just a "rang".
  struct rang {
    int row; // Logical row: -1,1 = top,bottom
    int side; // -1,1 = left,right. [0] is always left and [1] always right.
    int grabbed;
    double grabclock; // Counts down to release.
    double x; // Positive horizontal displacement, if not (grabbed).
    double dx; // px/s. Negative when returning.
    double t; // radians
    double yclock; // If >0, we're travelling to (row). Change (row) immediately and set this to 1.
    int dead; // Nonzero if we've whacked somebody. Stop moving horizontally, stop rotating, fall to the ground.
    double deady;
  } rangv[2];
  
  // Kangaroo. (army) is -1 or 1. (grab) nonzero if we're holding the boomerang.
  int larmy,lgrab;
  int rarmy,rgrab;
};

#define CTX ((struct battle_boomerang*)ctx)

/* Delete.
 */
 
static void _boomerang_del(void *ctx) {
  free(ctx);
}

/* Prepare the player records.
 */
 
static void player_init_dot(void *ctx,struct player *player) {
  player->human=1;
  player->face=FACE_IDLE;
  player->srcx=0;
  player->srcy=64;
  player->dstx=44;
  player->xform=0;
  player->gravity=0.0;
  player->el=0.0;
  player->judgment=0;
  player->reacting=0;
}

// Dot, but on the right side facing left, and controlled by player 2.
static void player_init_altdot(void *ctx,struct player *player) {
  player->human=2;
  player->face=FACE_IDLE;
  player->srcx=0;
  player->srcy=64;
  player->dstx=228;
  player->xform=EGG_XFORM_XREV;
  player->gravity=0.0;
  player->el=0.0;
  player->judgment=0;
  player->reacting=0;
}

static void player_init_koala(void *ctx,struct player *player) {
  player->human=0;
  player->face=FACE_IDLE;
  player->srcx=160;
  player->srcy=16;
  player->dstx=228;
  player->xform=EGG_XFORM_XREV;
  player->gravity=0.0;
  player->el=0.0;
  player->judgment=JUDGMENT_EASIEST+((CTX->handicap*(JUDGMENT_HARDEST-JUDGMENT_EASIEST))>>8);
  player->reacting=0;
}

/* Reset a rang.
 * Wipes everything except (side).
 */
 
static double rand_half_to_one() {
  return 0.5+((rand()&0x7fff)/65536.0);
}
 
static void rang_reset(void *ctx,struct rang *rang) {
  // TODO bias grabclock and dx per CTX->handicap and rang->side
  rang->grabclock=CTX->grab_time*rand_half_to_one();
  rang->dx=CTX->rang_speed*rand_half_to_one();
  rang->row=(rand()&1)?-1:1;
  rang->grabbed=1;
  rang->x=0.0;
  rang->t=0.0;
  rang->yclock=0.0;
  //fprintf(stderr,"%s(%d) dx=%f\n",__func__,rang->side,rang->dx);
}

/* Init.
 */
 
static void *_boomerang_init(
  int playerc,int handicap,
  void (*cb_finish)(void *userdata,int outcome),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_boomerang));
  if (!ctx) return 0;
  CTX->playerc=playerc;
  CTX->handicap=handicap;
  CTX->cb_finish=cb_finish;
  CTX->userdata=userdata;
  CTX->outcome=-2;
  CTX->input_blackout[1]=CTX->input_blackout[2];
  CTX->grab_time=GRAB_TIME_INITIAL;
  CTX->rang_speed=RANG_SPEED_INITIAL;
  
  player_init_dot(ctx,CTX->playerv+0);
  if (playerc==2) player_init_altdot(ctx,CTX->playerv+1);
  else player_init_koala(ctx,CTX->playerv+1);
  
  CTX->rangv[0].side=-1;
  CTX->rangv[1].side=1;
  rang_reset(ctx,CTX->rangv+0);
  rang_reset(ctx,CTX->rangv+1);
  
  return ctx;
}

/* Advance grab_time and rang_speed.
 */
 
static void boomerang_harden(void *ctx) {
  if ((CTX->grab_time*=GRAB_TIME_MLT)<GRAB_TIME_LIMIT) CTX->grab_time=GRAB_TIME_LIMIT;
  if ((CTX->rang_speed*=RANG_SPEED_MLT)>RANG_SPEED_LIMIT) CTX->rang_speed=RANG_SPEED_LIMIT;
  //fprintf(stderr,"%s: grab_time=%f, rang_speed=%f\n",__func__,CTX->grab_time,CTX->rang_speed);
}

/* Player faces.
 */
 
static void player_set_face(struct player *player,int face) {
  if (player->face==face) return;
  if (player->face==FACE_HURT) return; // No going back from HURT.
  player->face=face;
  if (player->human) switch (face) {
    case FACE_IDLE: player->srcx=0; player->srcy=64; break;
    case FACE_JUMP: player->srcx=48; player->srcy=64; break;
    case FACE_DUCK: player->srcx=144; player->srcy=64; break;
    case FACE_HURT: player->srcx=96; player->srcy=64; break;
  } else switch (face) {
    case FACE_IDLE: player->srcx=160; player->srcy=16; break;
    case FACE_JUMP: player->srcx=208; player->srcy=16; break;
    case FACE_DUCK: player->srcx=208; player->srcy=64; break;
    case FACE_HURT: player->srcx=208; player->srcy=112; break;
  }
}

/* Jumping.
 * Each player must call either "yes" or "no" during their update.
 */
 
static void player_jump_no(struct player *player,double elapsed) {

  // Aborting jump?
  if (player->gravity<0.0) {
    player->gravity=0.0;
    player_set_face(player,FACE_IDLE);
    player->jumpok=0;
  }
  
  // Increase gravity and fall. It's OK to do this redundantly.
  if ((player->gravity+=GRAVITY_ACCEL*elapsed)>GRAVITY_MAX) player->gravity=GRAVITY_MAX;
  if ((player->el+=player->gravity*elapsed)>=0.0) {
    player->el=0.0;
    player->gravity=0.0;
    if (player->human&&(g.input[player->human]&EGG_BTN_SOUTH)) player->jumpok=0;
    else player->jumpok=1;
  } else {
    player->jumpok=0;
  }
}

static void player_jump_yes(struct player *player,double elapsed) {

  // Can't jump while ducking or falling.
  if (player->face==FACE_DUCK) { player_jump_no(player,elapsed); return; }
  if (!player->jumpok) { player_jump_no(player,elapsed); return; }
  
  // Starting the jump?
  if (player->face==FACE_IDLE) {
    player_set_face(player,FACE_JUMP);
    player->gravity=JUMP_INITIAL;
    bm_sound(RID_sound_jump,player->xform?0.250:-0.250);
  }
  
  // Increase gravity and fall upward.
  // When the gravity goes positive, switch to idle.
  if ((player->gravity+=JUMP_DECEL*elapsed)>=0.0) {
    player_set_face(player,FACE_IDLE);
    player->gravity=0.001; // Ensure it's not exactly zero, otherwise we'd think we're seated next frame.
    player->jumpok=0;
  } else {
    player->el+=player->gravity*elapsed;
  }
}

/* Ducking.
 */
 
static void player_duck_no(struct player *player,double elapsed) {
  if (player->face==FACE_DUCK) player_set_face(player,FACE_IDLE);
}

static void player_duck_yes(struct player *player,double elapsed) {
  if (player->el<0.0) {
    player_duck_no(player,elapsed);
  } else {
    player_set_face(player,FACE_DUCK);
  }
}

/* Human player.
 */
 
static void player_update_human(void *ctx,struct player *player,double elapsed) {
  int input=g.input[player->human];
  if (CTX->input_blackout[player->human]) {
    if (!(input&(EGG_BTN_SOUTH|EGG_BTN_DOWN))) CTX->input_blackout[player->human]=0;
    player_jump_no(player,elapsed);
    player_duck_no(player,elapsed);
  } else {
    if (input&EGG_BTN_SOUTH) player_jump_yes(player,elapsed);
    else player_jump_no(player,elapsed);
    if (input&EGG_BTN_DOWN) player_duck_yes(player,elapsed);
    else player_duck_no(player,elapsed);
  }
}

/* CPU player.
 */
 
static void player_update_cpu(void *ctx,struct player *player,struct rang *rang,double elapsed) {
  // We stand around 70. Rang turns at 120.
  double head= 40.0;
  double tail=100.0;
  if ((rang->dx>-65.0)&&(rang->dx<65.0)) { // Very slow rang? Pull the edges in a little.
    head= 50.0;
    tail= 95.0;
  }
  if ((rang->x>=head)&&(rang->x<=tail)) {
    if (!player->reacting) {
      player->reacting=1;
      player->judgment--;
    }
    int react_row=rang->row;
    if (player->judgment<=0) {
      react_row=-react_row;
    }
    if (react_row<0) {
      player_duck_yes(player,elapsed);
      player_jump_no(player,elapsed);
    } else {
      player_duck_no(player,elapsed);
      player_jump_yes(player,elapsed);
    }
  } else {
    player->reacting=0;
    player_duck_no(player,elapsed);
    player_jump_no(player,elapsed);
  }
}

/* Hurt a player, ie end the game.
 */
 
static void player_hurt(void *ctx,struct player *player,struct rang *rang) {
  player_set_face(player,FACE_HURT);
  if (player==CTX->playerv) { // Right player wins.
    CTX->outcome=-1;
    bm_sound(RID_sound_whack,-0.250);
  } else { // Left player wins.
    CTX->outcome=1;
    bm_sound(RID_sound_whack,0.250);
  }
  CTX->end_cooldown=END_COOLDOWN_TIME;
  rang->dead=1;
  rang->deady=(rang->row<0)?(GROUND_LEVEL-40):(GROUND_LEVEL-9); // Must agree with rang_render.
}

/* Common player logic, applied after the human or cpu update.
 */
 
static void player_update_common(void *ctx,struct player *player,struct rang *rang,double elapsed) {
  if (CTX->outcome>-2) return; // Game is already over, just let it ride.
  const double head=60.0;
  const double tail=80.0;
  if ((rang->x>=head)&&(rang->x<=tail)) {
    if (rang->row<0) {
      if (player->face!=FACE_DUCK) player_hurt(ctx,player,rang);
    } else {
      if (player->el>-12.0) player_hurt(ctx,player,rang);
    }
  }
}

/* Update one rang.
 * Just the motion. player_update_common() manages hit detection.
 */
 
static void rang_update(void *ctx,struct rang *rang,double elapsed) {
  if (rang->grabbed) {
    if (CTX->outcome>-2) return;
    if ((rang->grabclock-=elapsed)<=0.0) {
      rang->grabbed=0;
    }
  } else if (rang->dead){
    if ((rang->deady+=30.0*elapsed)>=GROUND_LEVEL) rang->deady=GROUND_LEVEL;
  } else {
    rang->t+=RANG_ROTATE_SPEED*elapsed;
    if (rang->yclock>0.0) {
      rang->yclock-=elapsed;
    } else {
      rang->x+=rang->dx*elapsed;
      if ((rang->x>RANG_X_RANGE)&&(rang->dx>0.0)) {
        rang->dx=-rang->dx;
        rang->row=-rang->row;
        rang->yclock=1.0;
      } else if ((rang->x<=0.0)&&(rang->dx<0.0)) {
        boomerang_harden(ctx);
        rang_reset(ctx,rang);
      }
    }
  }
}

/* Update.
 */
 
static void _boomerang_update(void *ctx,double elapsed) {
  if (CTX->outcome>-2) {
    if (CTX->end_cooldown>0.0) {
      if ((CTX->end_cooldown-=elapsed)<=0.0) {
        CTX->cb_finish(CTX->userdata,CTX->outcome);
      }
    }
  }
  
  /* Of course we know that there are always 2 players and the first is always human.
   * But it's neater to do it all generically.
   * In theory, we already support a zero-player mode.
   */
  struct player *player=CTX->playerv;
  struct rang *rang=CTX->rangv;
  int i=2;
  if (CTX->outcome>-2) {
    for (;i-->0;player++,rang++) {
      rang_update(ctx,rang,elapsed);
      player_jump_no(player,elapsed);
      player_duck_no(player,elapsed);
    }
  } else {
    for (;i-->0;player++,rang++) {
      rang_update(ctx,rang,elapsed);
      if (player->human) player_update_human(ctx,player,elapsed);
      else player_update_cpu(ctx,player,rang,elapsed);
      player_update_common(ctx,player,rang,elapsed);
    }
  }
}

/* Render bits.
 */
 
static void rang_render(struct rang *rang) {
  int armdstx,armdsty,armsrcx,armsrcy;
  int rangdstx,rangdsty; // if grabbed
  uint8_t armxform,rangxform;
  if (rang->side<0) {
    armdstx=(FBW>>1)-24;
    rangdstx=armdstx+6;
    rangxform=armxform=0;
  } else {
    armdstx=(FBW>>1)+7;
    rangdstx=armdstx+10;
    rangxform=armxform=EGG_XFORM_XREV;
  }
  if (rang->grabbed) {
    armsrcx=48;
  } else {
    armsrcx=64;
  }
  if (rang->row<0) {
    armdsty=GROUND_LEVEL-48;
    rangdsty=armdsty+8;
    armsrcy=112;
  } else {
    armdsty=GROUND_LEVEL-24;
    rangdsty=armdsty+15;
    armsrcy=136;
    rangxform|=EGG_XFORM_YREV;
  }
  if (rang->grabbed) {
    graf_tile(&g.graf,rangdstx,rangdsty,0x5c,rangxform);
  } else if (rang->dead) {
    int rx=rangdstx+rang->side*(int)rang->x;
    int ry=(int)rang->deady;
    graf_tile(&g.graf,rx,ry,0x5c,rangxform);
  } else {
    uint8_t rot=(uint8_t)(int)((rang->t*256.0)/(M_PI*2.0));
    int rx=rangdstx+rang->side*(int)rang->x;
    int ry=rangdsty;
    if (rang->yclock>0.0) {
      ry-=rang->row*(int)(rang->yclock*31.0);
    }
    graf_fancy(&g.graf,rx,ry,0x5c,rangxform,rot,16,0,0x808080ff);
  }
  graf_decal_xform(&g.graf,armdstx,armdsty,armsrcx,armsrcy,16,24,armxform);
}

static void player_render(struct player *player) {
  int dsty=GROUND_LEVEL-48+(int)player->el;
  graf_decal_xform(&g.graf,player->dstx,dsty,player->srcx,player->srcy,48,48,player->xform);
}

/* Render.
 */
 
static void _boomerang_render(void *ctx) {

  graf_fill_rect(&g.graf,0,0,FBW,GROUND_LEVEL,(CTX->outcome>-2)?SKY_COLOR_KAPOW:SKY_COLOR);
  if (CTX->outcome>-2) { // Draw a big "Kapow" starburst where the rang struck. Behind the ground.
    struct rang *rang=CTX->rangv;
    if (!rang->dead) rang++; // If it's not the first, must be the second.
    int hitx=(rang->side<0)?((FBW>>1)-24-(int)rang->x):((FBW>>1)+24+(int)rang->x);
    int hity=(rang->row<0)?(GROUND_LEVEL-40):(GROUND_LEVEL-9);
    graf_set_image(&g.graf,RID_image_battle_fishing);
    graf_decal(&g.graf,hitx-24,hity-24,80,112,48,48);
  }
  graf_fill_rect(&g.graf,0,GROUND_LEVEL,FBW,FBH-GROUND_LEVEL,GROUND_COLOR);
  graf_fill_rect(&g.graf,0,GROUND_LEVEL,FBW,1,0x000000ff);
  
  graf_set_image(&g.graf,RID_image_battle_fishing);
  player_render(CTX->playerv+0);
  player_render(CTX->playerv+1);
  
  // Kangaroo in the middle.
  graf_decal(&g.graf,(FBW>>1)-24,GROUND_LEVEL-48,0,112,48,48);
  rang_render(CTX->rangv+0);
  rang_render(CTX->rangv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_boomerang={
  .name="boomerang",
  .battletype=NS_battletype_boomerang,
  .flags=BATTLE_FLAG_ONEPLAYER|BATTLE_FLAG_TWOPLAYER,
  .foe_name_strix=52,
  .battle_name_strix=53,
  .del=_boomerang_del,
  .init=_boomerang_init,
  .update=_boomerang_update,
  .render=_boomerang_render,
};

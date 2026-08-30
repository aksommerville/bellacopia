/* battle_bodyslam.c
 */

#include "game/bellacopia.h"

#define BGVTX_LIMIT 170
#define GRIDY 4 /* Start the grid a little below the top of the screen, so it fits exactly 20x11 tiles. */
#define BREAK_ROWC 6 /* How many layers of ice to break. */
#define GRAVITY_RATE 12.0
#define GRAVITY_LIMIT 10.0
#define SLAM_SPEED 12.0

struct battle_bodyslam {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    double x,y; // Meters relative to the grid. (vertically there's a small offset GRIDY).
    struct egg_render_tile *break_rowv[BREAK_ROWC]; // Points into (bgvtxv).
    int injump,inslam;
    int blackout;
    int jumpok; // Nonzero if we're seated and jump is ready.
    int pvinjump,pvinslam;
    int jumping;
    double jumppower; // m/s positive, reduces to zero.
    double jumpinitial; // m/s positive, constant.
    double jumpdecay; // m/s**2 positive, constant.
    double gravity; // m/s positive
    double gravitytime; // How long did we fall?
    int slamming;
    int brokec; // How many ice layers currently broken? Advances to (breakc) during animation.
    int breakc; // 0..BREAK_ROWC, how many layers are we going to break. Decided when the bodyslam starts.
    double animclock;
    int animframe;
    int done;
    double waitclock; // cpu
    double slamy; // cpu; begin slam when above this elevation
  } playerv[2];
  
  struct egg_render_tile bgvtxv[BGVTX_LIMIT];
  int bgvtxc;
  double playclock;
};

#define BATTLE ((struct battle_bodyslam*)battle)

/* Delete.
 */
 
static void _bodyslam_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=5.5;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->x=14.5;
  }
  player->y=2.5;
  
  player->jumpinitial=16.0*(1.0-player->skill)+18.0*player->skill;
  player->jumpdecay=50.0;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->waitclock=1.000+(2.000*(rand()&0xffff))/65535.0;
    player->slamy=-0.010*(1.0-player->skill)-0.160*player->skill;
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0x1a130cff;
        player->tileid=0xd0;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0xb0;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0xc0;
      } break;
  }
}

/* Add a background vertex.
 */
 
static struct egg_render_tile *bodyslam_bgvtx(struct battle *battle,int x,int y,uint8_t tileid,uint8_t xform) {
  if (BATTLE->bgvtxc>=BGVTX_LIMIT) {
    fprintf(stderr,"!!! too many vertices !!!\n");
    return 0;
  }
  struct egg_render_tile *vtx=BATTLE->bgvtxv+BATTLE->bgvtxc++;
  vtx->x=x;
  vtx->y=y;
  vtx->tileid=tileid;
  vtx->xform=xform;
  return vtx;
}

/* New.
 */
 
static int _bodyslam_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  BATTLE->playclock=29.0; // We don't advertise our timeout, but we do terminate well before the global timeout, in case one player boycotts.
  
  /* Generate the background vertex batch.
   * These are best thought of as columns, each fitting one of four patterns.
   */
  int y0=GRIDY+(NS_sys_tilesize>>1)+NS_sys_tilesize*2;
  int x0=NS_sys_tilesize>>1;
  int y,i,pc;
  uint8_t tileid;
  for (y=y0,i=9,tileid=0xb6;i-->0;y+=NS_sys_tilesize,tileid=0xc6) { // High plateaus.
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 0,y,tileid,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 1,y,tileid,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 9,y,tileid,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize*10,y,tileid,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize*18,y,tileid,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize*19,y,tileid,0);
  }
  for (y=y0+NS_sys_tilesize,i=6,pc=0;i-->0;y+=NS_sys_tilesize,pc++) { // Ice stacks. Skip the bottom row. Do include the wall joints.
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 2,y,0xc9,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 3,y,0xb8,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 4,y,0xb8,0);
    l->break_rowv[pc]=bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 5,y,0xb8,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 6,y,0xb8,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 7,y,0xb8,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 8,y,0xc8,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize*11,y,0xc9,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize*12,y,0xb8,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize*13,y,0xb8,0);
    r->break_rowv[pc]=bodyslam_bgvtx(battle,x0+NS_sys_tilesize*14,y,0xb8,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize*15,y,0xb8,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize*16,y,0xb8,0);
    bodyslam_bgvtx(battle,x0+NS_sys_tilesize*17,y,0xc8,0);
  }
  // Corners and floors, a la carte.
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 2,y0+NS_sys_tilesize* 0,0xb7,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 8,y0+NS_sys_tilesize* 0,0xb5,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*11,y0+NS_sys_tilesize* 0,0xb7,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*17,y0+NS_sys_tilesize* 0,0xb5,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 2,y0+NS_sys_tilesize* 7,0xd7,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 8,y0+NS_sys_tilesize* 7,0xd5,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*11,y0+NS_sys_tilesize* 7,0xd7,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*17,y0+NS_sys_tilesize* 7,0xd5,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 2,y0+NS_sys_tilesize* 8,0xc6,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 3,y0+NS_sys_tilesize* 8,0xd8,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 4,y0+NS_sys_tilesize* 8,0xb6,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 5,y0+NS_sys_tilesize* 8,0xb6,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 6,y0+NS_sys_tilesize* 8,0xb6,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 7,y0+NS_sys_tilesize* 8,0xd9,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize* 8,y0+NS_sys_tilesize* 8,0xc6,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*11,y0+NS_sys_tilesize* 8,0xc6,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*12,y0+NS_sys_tilesize* 8,0xd8,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*13,y0+NS_sys_tilesize* 8,0xb6,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*14,y0+NS_sys_tilesize* 8,0xb6,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*15,y0+NS_sys_tilesize* 8,0xb6,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*16,y0+NS_sys_tilesize* 8,0xd9,0);
  bodyslam_bgvtx(battle,x0+NS_sys_tilesize*17,y0+NS_sys_tilesize* 8,0xc6,0);
   
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&(EGG_BTN_SOUTH|EGG_BTN_DOWN))) player->blackout=0;
  } else {
    player->injump=(input&EGG_BTN_SOUTH)?1:0;
    player->inslam=(input&EGG_BTN_DOWN)?1:0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->waitclock>0.0) {
    player->injump=0;
    player->inslam=0;
    player->waitclock-=elapsed;
  } else if (player->y<=player->slamy) {
    player->injump=0;
    player->inslam=1;
  } else if (player->gravitytime>=0.050) {
    player->injump=0;
    player->inslam=0;
  } else if (player->jumping) {
    player->injump=1;
    player->inslam=0;
  } else {
    player->slamy+=0.500*elapsed; // Inch slamy down in case we don't make it initially.
    player->injump=1;
    player->inslam=0;
  }
}

/* Check vertical position after applying gravity or bodyslam.
 * Rectifies vertical and returns nonzero if there was a collision.
 */
 
static int player_check_feet(struct battle *battle,struct player *player) {

  // Where is the current floor?
  double bottomy;
  if (player->brokec<6) {
    bottomy=3.0+player->brokec;
  } else {
    bottomy=10.125; // The snowy floor is inset to its tile a little.
  }
  bottomy-=0.5; // Compare to (player->y), which is its center, not its feet.
  
  if (player->y<=bottomy) return 0;
  player->y=bottomy;
  return 1;
}

/* Body slam is starting.
 * Decide how many layers it will break.
 */
 
static void player_judge_slam(struct battle *battle,struct player *player) {
  double best=-0.180;
  double worst=0.500;
  double q=(player->y-worst)/(best-worst);
  player->breakc=BREAK_ROWC*q;
  if (player->breakc<1) player->breakc=1;
  else if (player->breakc>BREAK_ROWC) player->breakc=BREAK_ROWC;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* If we're done, just neutralize everything to be safe.
   */
  if (player->done) {
    player->jumping=0;
    player->slamming=0;
    player->gravity=0.0;
    return;
  }

  /* Start or curtail a jump per input.
   */
  if (player->injump!=player->pvinjump) {
    if (player->pvinjump=player->injump) {
      if (player->jumpok) {
        bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->jumpok=0;
        player->jumping=1;
        player->jumppower=player->jumpinitial;
        player->gravity=0.0;
        player->gravitytime=0.0;
      }
    } else if (player->jumping) {
      player->jumping=0;
      player->gravity=0.0;
      player->gravitytime=0.001;
      if (player->inslam&&!player->pvinslam) { // For this one frame, we could miss a slam start. Pick it off here.
        player->slamming=1;
        player_judge_slam(battle,player);
      }
    }
  }
  
  /* Start bodyslam per input.
   */
  if (player->inslam!=player->pvinslam) {
    if (player->pvinslam=player->inslam) {
      if (!player->slamming) {
        if (player->jumping||(player->gravitytime>0.0)) {
          player->slamming=1;
          player->gravity=0.0;
          player->jumping=0;
          player_judge_slam(battle,player);
        }
      }
    }
  }
  
  /* Advance bodyslam if that's happening.
   */
  if (player->slamming) {
    player->y+=SLAM_SPEED*elapsed;
    double yrestore=player->y;
    if (player_check_feet(battle,player)) {
      if (player->brokec<player->breakc) {
        player->break_rowv[player->brokec]->tileid=0xb9;
        player->brokec++;
        bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->y=yrestore;
      } else {
        bm_sound_pan(RID_sound_treasure,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->done=1;
        player->jumping=0;
        player->slamming=0;
        player->gravity=0.0;
      }
    } else {
      if ((player->animclock-=elapsed)<=0.0) {
        player->animclock+=0.125;
        if (++(player->animframe)>=2) player->animframe=0;
      }
    }
  
  /* Advance jump if that's happening.
   */
  } else if (player->jumping) {
    player->y-=player->jumppower*elapsed;
    player->jumppower-=player->jumpdecay*elapsed;
    if (player->jumppower<=0.0) { // Reached peak, let gravity take over.
      player->jumping=0;
      player->gravity=0.0;
      player->gravitytime=0.001;
    }
  
  /* If not jumping or slamming, apply gravity.
   */
  } else {
    player->gravity+=GRAVITY_RATE*elapsed;
    if (player->gravity>GRAVITY_LIMIT) player->gravity=GRAVITY_LIMIT;
    player->y+=player->gravity*elapsed;
    if (player_check_feet(battle,player)) {
      if (player->gravitytime>0.250) bm_sound_pan(RID_sound_bump,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->jumpok=1;
      player->gravity=0.0;
      player->gravitytime=0.0;
    } else {
      player->gravitytime+=elapsed;
    }
  }
}

/* Update.
 */
 
static void _bodyslam_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* If our timeout expires, call it.
   * Important that we not wait for the global timeout, since we might have one finished player.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if ((BATTLE->playclock-=elapsed)<=0.0) {
    if (l->done&&r->done) {
      if (l->breakc>r->breakc) battle->outcome=1;
      else if (l->breakc<r->breakc) battle->outcome=-1;
      else battle->outcome=0;
    } else if (l->done) battle->outcome=1;
    else if (r->done) battle->outcome=-1;
    else battle->outcome=0;
  } else if (l->done&&r->done) {
    if (l->breakc>r->breakc) battle->outcome=1;
    else if (l->breakc<r->breakc) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=(int)(player->x*NS_sys_tilesize);
  int y=(int)(player->y*NS_sys_tilesize)+GRIDY;
  uint8_t tileid=player->tileid;
  if (player->jumping) tileid+=1;
  else if (player->gravity>0.0) tileid+=2;
  else if (player->slamming) tileid+=3+player->animframe;
  graf_tile(&g.graf,x,y,tileid,player->xform);
}

/* Render.
 */
 
static void _bodyslam_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_set_image(&g.graf,RID_image_battle_tundra);
  graf_tile_batch(&g.graf,BATTLE->bgvtxv,BATTLE->bgvtxc);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
static const struct battle_input _bodyslam_input[]={
  {.dur=2,.state=EGG_BTN_SOUTH},
  {.dur=1,.state=EGG_BTN_DOWN},
  {.dur=2,.state=0},
{0}};
 
const struct battle_type battle_type_bodyslam={
  .name="bodyslam",
  .objlen=sizeof(struct battle_bodyslam),
  .id=NS_battle_bodyslam,
  .strix_name=326,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=_bodyslam_input,
  .imageid_default=RID_image_tundra,
  .del=_bodyslam_del,
  .init=_bodyslam_init,
  .update=_bodyslam_update,
  .render=_bodyslam_render,
};

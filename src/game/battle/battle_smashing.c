/* battle_smashing.c
 * A to jump. Smash the melons as they roll by.
 */

#include "game/bellacopia.h"

#define GROUNDY 140
#define GRAVITY_ACCEL 300.0
#define GRAVITY_LIMIT 200.0
#define THROW_MIN  3 /* How long before the CPU player makes a mistake. 0x6d strikes him on the fifth, 0x6e and above will not strike him. */
#define THROW_MAX 10 /* Greater than SCORE_MAX, ie at most difficulties, the CPU player won't miss. */
#define SCORE_MAX 5

#define POSE_IDLE 0
#define POSE_LOSE 1
#define POSE_JUMP 2
#define POSE_FALL 3
#define POSE_COUNT 4

struct battle_smashing {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t xform;
    double x,y; // Framebuffer pixels. (x) is constant. Bottom middle of sprite.
    struct pose {
      int srcx,srcy;
    } posev[POSE_COUNT];
    int posep;
    int input; // Boolean, you just get one button.
    int pvinput;
    int seated;
    int jumping;
    double jumpforce; // px/s positive
    double jumpforce_initial; // px/s positive
    double jumpforce_decel; // px/s**2 positive
    double gravity; // px/s**2 positive
    int score;
    uint32_t color;
    // For CPU players:
    int throw; // Counts down until the one we're going to miss. Only relevant at low difficulties.
    int vegid;
    double leadlo,leadhi; // Timing range per difficulty, constant. This is how much time we waste per vegetable.
    double falldx,jumpdx; // Selected timing for the current vegetable. Phrased in terms of distance to vegetable.
  } playerv[2];
  
  /* There's two possible vegetables, one for each player.
   * [0] has (dx>0) and [1] has (dx<0).
   */
  struct vegetable {
    double x,y;
    double dx;
    double t;
    double dt;
    uint8_t tileid;
    int still;
    int vegid; // Positive and unique, or zero if this vegetable not in use.
  } vegetablev[2];
  int vegid_next;
};

#define BATTLE ((struct battle_smashing*)battle)

/* Delete.
 */
 
static void _smashing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->y=GROUNDY;
  player->pvinput=1; // If it's on initially, wait for it to go off.
  player->seated=1;
  player->jumpforce_initial=250.0;
  player->jumpforce_decel=400.0;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.5-40.0;
    player->xform=EGG_XFORM_XREV; // Face outward. Natural orientation is right.
  } else { // Right.
    player->who=1;
    player->x=FBW*0.5+40.0;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    player->throw=(int)((1.0-player->skill)*THROW_MIN+player->skill*THROW_MAX);
    if (player->throw<1) player->throw=1;
    player->leadhi=(1.0-player->skill)*0.250;
    player->leadlo=player->leadhi*0.500;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x546a35ff;
        player->posev[POSE_IDLE]=(struct pose){  0,160};
        player->posev[POSE_LOSE]=(struct pose){ 48,160};
        player->posev[POSE_JUMP]=(struct pose){ 96,160};
        player->posev[POSE_FALL]=(struct pose){144,160};
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->posev[POSE_IDLE]=(struct pose){  0, 64};
        player->posev[POSE_LOSE]=(struct pose){ 48, 64};
        player->posev[POSE_JUMP]=(struct pose){  0,208};
        player->posev[POSE_FALL]=(struct pose){ 48,208};
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->posev[POSE_IDLE]=(struct pose){  0,112};
        player->posev[POSE_LOSE]=(struct pose){ 48,112};
        player->posev[POSE_JUMP]=(struct pose){ 96,208};
        player->posev[POSE_FALL]=(struct pose){144,208};
      } break;
  }
}

/* New.
 */
 
static int _smashing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->vegid_next=1;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  player->input=(input&EGG_BTN_SOUTH)?1:0;
}

/* CPU player is targetting a new vegetable.
 * Establish (jumpdx,falldx), the critical parameters for his timing.
 */
 
static void player_prepare_jump(struct battle *battle,struct player *player,struct vegetable *veg) {

  // If throwing, we don't need parameters. They won't be used.
  if (player->throw<=0) return;
  
  /* Select (veg)'s position when we intend to strike it. Relative to our center, like the thresholds.
   * This is constant per (skill) but we recalculate every time. Can randomize if we like.
   * We compute against a narrower radius than player_update_common(), to avoid the ugly edges.
   */
  const double radius=12.0;
  double strikex_best,strikex_worst;
  if (player->who) {
    strikex_best=radius;
    strikex_worst=-radius;
  } else {
    strikex_best=-radius;
    strikex_worst=radius;
  }
  double strikex=strikex_best*player->skill+strikex_worst*(1.0-player->skill);
  
  /* Select the desired height of the jump.
   * This is decorative; the only thing that matters is when we land.
   * It's useful as a signal to the human, how smart this CPU player really is.
   */
  const double hlo=   30.0;
  const double hhi=  100.0;
  const double hfuzz= 20.0;
  double n=(rand()&0xffff)/65535.0;
  double h=player->skill*hlo+(1.0-player->skill)*hhi+n*hfuzz;
  
  /* How long does it take to reach this height, and how long to return from it?
   * Determine these iteratively, rather than working out the exact calculus. That's not really something I can be trusted with.
   */
  const double interval=0.016666;
  const double contacth=10.0;
  double upt=0.0;
  double qh=0.0;
  double jumpforce=player->jumpforce_initial;
  while (qh<h) {
    upt+=interval;
    jumpforce-=player->jumpforce_decel*interval;
    if (jumpforce<=0.0) break; // Too high! Let whatever happens happen.
    qh+=jumpforce*interval;
  }
  double downt=0.0;
  double gravity=0.0;
  while (qh>contacth) {
    downt+=interval;
    gravity+=GRAVITY_ACCEL*interval;
    if (gravity>=GRAVITY_LIMIT) gravity=GRAVITY_LIMIT;
    qh-=gravity*interval;
  }
  
  /* Now it's a simple matter of turning those up and down times into distances from (strikex).
   */
  double vegspeed=veg->dx;
  if (vegspeed<0.0) vegspeed=-vegspeed;
  player->falldx=vegspeed*downt+strikex;
  player->jumpdx=vegspeed*(downt+upt)+strikex;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Find the next vegetable.
   * If it's not spawned yet, go idle.
   */
  struct vegetable *veg=BATTLE->vegetablev+player->who;
  if (!veg->vegid) {
    player->input=0;
    return;
  }
  
  /* If this is not the vegetable we saw last time, tick (throw).
   * And if (throw) reaches zero, we become noop, let the next one hit us.
   * Also, establish our timing for this jump.
   */
  if (veg->vegid!=player->vegid) {
    player->vegid=veg->vegid;
    player->throw--;
    player_prepare_jump(battle,player,veg);
  }
  if (player->throw<=0) {
    player->input=0;
    return;
  }
  
  /* Get positive distance to vegetable and check it against our thresholds.
   */
  double distance;
  if (player->who) distance=veg->x-player->x;
  else distance=player->x-veg->x;
  if (distance<player->falldx) player->input=0;
  else if (distance<player->jumpdx) player->input=1;
  else player->input=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  int interactive=(battle->outcome==-2);
  if (!interactive) player->input=0;
  
  /* Begin a jump, cut the jump short, or update input state.
   */
  if (player->input&&!player->pvinput) {
    player->pvinput=1;
    if (player->seated) { // Begin jump.
      bm_sound_pan(RID_sound_jump,player->who?0.250:-0.250);
      player->jumping=1;
      player->seated=0;
      player->gravity=0.0;
      player->jumpforce=player->jumpforce_initial;
    }
  } else if (!player->input) {
    player->pvinput=0;
    if (player->jumping) {
      player->jumping=0;
    }
  }
  
  /* Apply jump or gravity.
   */
  if (player->jumping) {
    if ((player->jumpforce-=player->jumpforce_decel*elapsed)<=0.0) {
      // Crested, restart gravity.
      player->jumping=0;
    } else {
      // Jump ongoing.
      player->y-=player->jumpforce*elapsed;
    }
  } else if (!player->seated) {
    if ((player->gravity+=GRAVITY_ACCEL*elapsed)>=GRAVITY_LIMIT) {
      player->gravity=GRAVITY_LIMIT;
    }
    player->y+=player->gravity*elapsed;
    if (player->y>=GROUNDY) {
      // Landed.
      player->y=GROUNDY;
      player->seated=1;
      player->gravity=0.0;
    }
  }
  
  /* Check vegetables.
   * Note that we check *both* vegetables.
   * See vegetable_update(); a wee change there can allow veg to attack the other guy from behind.
   * But I'm not using that feature, after a little consideration.
   */
  if ((battle->outcome==-2)&&(player->y>GROUNDY-15.0)) {
    const double radius=15.0;
    struct vegetable *veg=BATTLE->vegetablev;
    int i=2;
    for (;i-->0;veg++) {
      if (!veg->vegid) continue;
      double dx=veg->x-player->x;
      if ((dx<-radius)||(dx>radius)) continue;
      if (player->seated||player->jumping) {
        bm_sound_pan(RID_sound_ouch,player->who?0.250:-0.250);
        battle->outcome=player->who?1:-1;
        veg->still=1;
        break;
      } else {
        bm_sound_pan(RID_sound_pop,player->who?0.250:-0.250);
        veg->vegid=0;
        player->score++;
        //TODO splatter
      }
    }
  }
  
  /* Set the appropriate face.
   */
  if (battle->outcome>-2) {
    if (player->who) {
      player->posep=(battle->outcome==1)?POSE_LOSE:POSE_IDLE;
    } else {
      player->posep=(battle->outcome==-1)?POSE_LOSE:POSE_IDLE;
    }
  } else if (player->jumping) player->posep=POSE_JUMP;
  else if (player->seated) player->posep=POSE_IDLE;
  else player->posep=POSE_FALL;
}

/* Update one vegetable.
 */
 
static void vegetable_update(struct battle *battle,struct vegetable *veg,double elapsed) {
  if (veg->still||!veg->vegid) return;
  veg->x+=veg->dx*elapsed;
  
  /* Eliminate when we reach the horizontal midpoint.
   * It's fun to play without this, because you can let one thru in the hopes it will strike your opponent from behind.
   * But our CPU player isn't built for it. I don't really like the extra complexity.
   */
  if (((veg->dx>0.0)&&(veg->x>FBW*0.450))||((veg->dx<0.0)&&(veg->x<FBW*0.550))) {
    struct player *player=BATTLE->playerv;
    if (veg->dx<0.0) player+=1;
    if (player->score>0) player->score--;
    veg->vegid=0;
    return;
  }
  
  veg->t+=veg->dt*elapsed;
  while (veg->t<-M_PI) veg->t+=M_PI*2.0;
  while (veg->t>M_PI) veg->t-=M_PI*2.0;
}

/* Generate the next vegetable.
 */
 
static void smashing_generate_vegetable(struct battle *battle,int who) {
  if ((who<0)||(who>1)) return;
  struct vegetable *veg=BATTLE->vegetablev+who;
  if (veg->vegid) return;
  veg->vegid=BATTLE->vegid_next++;
  veg->dx=80.0;
  veg->dt=5.0;
  veg->still=0;
  if (who) {
    veg->x=FBW+NS_sys_tilesize;
    veg->dx=-veg->dx;
    veg->dt=-veg->dt;
  } else {
    veg->x=-NS_sys_tilesize;
  }
  veg->y=GROUNDY-7.0;
  switch (rand()%6) {
    case 0: veg->tileid=0xdc; break; // tomato
    case 1: veg->tileid=0xdd; break; // plum
    case 2: veg->tileid=0xec; break; // pumpkin
    case 3: veg->tileid=0xed; break; // acorn
    case 4: veg->tileid=0xfc; break; // watermelon
    case 5: veg->tileid=0xfd; break; // cantaloupe
  }
}

/* Update.
 */
 
static void _smashing_update(struct battle *battle,double elapsed) {

  /* Generate a vegetable?
   */
  if (battle->outcome==-2) {
    if (!BATTLE->vegetablev[0].vegid) smashing_generate_vegetable(battle,0);
    if (!BATTLE->vegetablev[1].vegid) smashing_generate_vegetable(battle,1);
  }
  
  /* Update vegetables.
   */
  struct vegetable *veg=BATTLE->vegetablev;
  int i=2;
  for (;i-->0;veg++) {
    vegetable_update(battle,veg,elapsed);
  }
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  for (i=2;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Check for completion.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->score>=SCORE_MAX) {
      if (r->score>=SCORE_MAX) battle->outcome=0; // Shouldn't be possible, except mathematically.
      else battle->outcome=1;
    } else if (r->score>=SCORE_MAX) {
      battle->outcome=-1;
    }
  }
}

/* Render bits.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const struct pose *pose=player->posev+player->posep;
  int dstx=(int)player->x-24;
  int dsty=(int)player->y-48;
  graf_decal_xform(&g.graf,dstx,dsty,pose->srcx,pose->srcy,48,48,player->xform);
}

static void scoreboard_render(struct battle *battle,struct player *player) {
  int x=(FBW>>1);
  if (player->who) x+=10;
  else x-=10;
  int y=GROUNDY-40;
  int i=0;
  for (;i<SCORE_MAX;i++,y-=12) {
    uint32_t color=(i<player->score)?player->color:0x808080ff;
    graf_fancy(&g.graf,x,y,0xde,0,0,NS_sys_tilesize,0,color);
  }
}

static void vegetable_render(struct battle *battle,struct vegetable *veg) {
  if (!veg->vegid) return;
  int x=(int)veg->x;
  int y=(int)veg->y;
  int8_t rot=(int8_t)((veg->t*128.0)/M_PI);
  graf_fancy(&g.graf,x,y,veg->tileid,0,rot,NS_sys_tilesize,0,0x808080ff);
}

/* Render.
 */
 
static void _smashing_render(struct battle *battle) {

  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,0x80b0d0ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,0x006020ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_labyrinth2);
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  scoreboard_render(battle,BATTLE->playerv+0);
  scoreboard_render(battle,BATTLE->playerv+1);
  
  struct vegetable *veg=BATTLE->vegetablev;
  int i=2;
  for (;i-->0;veg++) vegetable_render(battle,veg);
}

/* Type definition.
 */
 
const struct battle_type battle_type_smashing={
  .name="smashing",
  .objlen=sizeof(struct battle_smashing),
  .strix_name=174,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_smashing_del,
  .init=_smashing_init,
  .update=_smashing_update,
  .render=_smashing_render,
};

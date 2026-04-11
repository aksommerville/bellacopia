/* battle_sawing.c
 * L/R to saw the log, with some obscure optimal rhythm.
 */

#include "game/bellacopia.h"

#define PLAN_LIMIT 10

struct battle_sawing {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    // Left and right are absolute, regardless of the player's side.
    int dir; // -1,1. Controller sets this and nothing else.
    int pvdir; // Generic controller's, for tracking changes.
    double penalty; // s, counts down after an edge fault.
    double phase; // -1..1 = left..right, where is the saw relative to the log. Anything <-1 or >1 causes a fault.
    double rate; // hz, advancement of (phase). Always positive.
    double efficacy; // hz. Highly volatile, how much wood are we currently chucking per unit time.
    double chuckedness; // 0..1, the bottom line.
    double falltime; // s, for decorative purposes after completing.
    // Constants per skill:
    double rate_init; // (rate) immediately after changing direction.
    double rate_rate; // Acceleration of (rate).
    double efficacy_init; // (efficacy) immediately after changing direction.
    double efficacy_rate; // Acceleration of (efficacy).
    double penalty_time; // s
    // More state for CPU players:
    double plan[PLAN_LIMIT]; // Positive. At what point in each stroke will we turn around.
    int planp; // Wraps around at PLAN_LIMIT.
  } playerv[2];
};

#define BATTLE ((struct battle_sawing*)battle)

/* Delete.
 */
 
static void _sawing_del(struct battle *battle) {
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
  
    /* Generate the plan.
     * A CPU player that turns at 0.800 is defeatable but it's hard.
     * Start by selecting randomly within a range determined by skill.
     */
    double hi=0.500*(1.0-player->skill)+0.900*player->skill;
    double lo=hi*0.500;
    double plansum=0.0;
    int i=PLAN_LIMIT;
    while (i-->0) {
      double n=(rand()&0xffff)/65535.0;
      player->plan[i]=lo*(1.0-n)+hi*n;
      plansum+=player->plan[i];
    }
    /* Now scale all planned intervals to match the skill range exactly.
     * But clamp the intervals at 0.900. Forcing faults will be done more explicitly.
     */
    double targetsum=hi*PLAN_LIMIT;
    double adjust=targetsum/plansum;
    for (i=PLAN_LIMIT;i-->0;) {
      player->plan[i]*=adjust;
      if (player->plan[i]>0.900) player->plan[i]=0.900;
    }
    /* And finally, pick a count of strokes and randomly force so many to be faults.
     */
    int faultc=(int)(1.0*player->skill+7.0*(1.0-player->skill));
    if (faultc>0) {
      if (faultc>PLAN_LIMIT) faultc=PLAN_LIMIT;
      while (faultc-->0) {
        int panic=100;
        while (panic-->0) {
          int p=rand()%PLAN_LIMIT;
          if (player->plan[p]>=1.0) continue; // oops already planning a fault here
          player->plan[p]=1.500;
          break;
        }
      }
    }
  
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x8c2544ff;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
  player->rate_init=0.750;
  player->rate_rate=1.000;
  player->efficacy_init=0.050;
  player->efficacy_rate=0.100;
  player->penalty_time=0.500;
}

/* New.
 */
 
static int _sawing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  int ndir=0;
  switch (g.input[player->human]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: ndir=-1; break;
    case EGG_BTN_RIGHT: ndir=1; break;
  }
  if (!ndir||(ndir==player->dir)) return;
  player->dir=ndir;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If I'm not moving yet, start moving in the direction with more freedom.
   * If it's close to zero, choose randomly instead of breaking the same way every time.
   * That should only come up for the first first stroke.
   */
  if (!player->dir) {
    if (player->phase>0.100) player->dir=-1;
    else if (player->phase<-0.100) player->dir=1;
    else player->dir=(rand()&1)?1:-1;
    if (++(player->planp)>=PLAN_LIMIT) player->planp=0;
    return;
  }
  
  /* When we approach the planned threshold, reverse direction.
   */
  int done=0;
  double thresh=player->plan[player->planp];
  if (player->dir<0) {
    if (player->phase<-thresh) done=1;
  } else {
    if (player->phase>thresh) done=1;
  }
  if (!done) return;
  player->dir=-player->dir;
  if (++(player->planp)>=PLAN_LIMIT) player->planp=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* If it's already chucked, advance (falltime) and nothing else.
   */
  if (player->chuckedness>=1.0) {
    player->falltime+=elapsed;
    return;
  }
  
  /* If the battle's over, stop.
   */
  if (battle->outcome>-2) return;

  /* If there's a penalty, it counts down and nothing else happens.
   */
  if (player->penalty>0.0) {
    if ((player->penalty-=elapsed)<=0.0) {
      // Force the phase valid, and dir zero -- they have to manually restart.
      if (player->phase<0.0) {
        player->phase=-1.0;
      } else {
        player->phase=1.0;
      }
      player->dir=0;
      player->rate=player->rate_init;
      player->efficacy=player->efficacy_init;
    }
    return;
  }
  
  /* If they haven't requested motion, don't give them any motion.
   */
  if (!player->dir) return;
  
  /* If the direction changed, reset rate and efficacy.
   */
  if (player->dir!=player->pvdir) {
    player->pvdir=player->dir;
    player->rate=player->rate_init;
    player->efficacy=player->efficacy_init;
    bm_sound_pan(RID_sound_saw,player->who?0.250:-0.250);
  }
  
  /* Accelerate rate and efficacy.
   * These have no upper bound. Just they reset when you fault or change direction.
   */
  player->rate+=player->rate_rate*elapsed;
  player->efficacy+=player->efficacy_rate*elapsed;
  
  /* Apply phase change and fault if OOB.
   */
  player->phase+=player->dir*player->rate*elapsed;
  if ((player->phase<-1.0)||(player->phase>1.0)) {
    player->penalty=player->penalty_time;
    bm_sound_pan(RID_sound_reject,player->who?0.250:-0.250);
    return;
  }
  
  /* Apply efficacy.
   */
  player->chuckedness+=player->efficacy*elapsed;
}

/* Update.
 */
 
static void _sawing_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Game ends when one player reaches full chuckedness.
   * It's unlikely, but if they both cross the threshold on the same frame, it's a tie.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->chuckedness>=1.0) {
      if (r->chuckedness>=1.0) {
        battle->outcome=0;
      } else {
        battle->outcome=1;
      }
    } else if (r->chuckedness>=1.0) {
      battle->outcome=-1;
    }
  }

  //XXX
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  int topx,topy; // Top-left of log, the top half.
  int btmx,btmy; // Top-left of log, the bottom half.
  int sawx,sawy; // Top-left of saw.
  uint8_t sawxform;
  if (player->who) {
    topx=(FBW>>1)+40;
    sawxform=EGG_XFORM_XREV;
    sawx=topx-24;
  } else {
    topx=(FBW>>1)-40-64;
    sawxform=0;
    sawx=topx-56;
  }
  topy=30;
  sawy=topy+16;
  btmy=topy+64;
  btmy+=player->falltime*80.0;
  btmx=topx;
  sawx+=lround(player->phase*24.0);
  sawy+=lround(player->chuckedness*24.0); // Technically 16 is correct, but give it a little overshoot for aesthetic purposes.
  
  // For no particular reason, retract the saw once chopped.
  if (player->falltime>0.0) {
    int d=(int)(player->falltime*50.0);
    if (player->who) sawx+=d;
    else sawx-=d;
  }
  
  graf_decal(&g.graf,topx,topy,0,96,64,80);
  graf_decal_xform(&g.graf,sawx,sawy,0,48,144,48,sawxform);
  graf_decal(&g.graf,btmx,btmy,0,176,64,64);
  
  if (player->penalty>0.0) {
    int penx=topx+16;
    int peny=topy-16;
    if (player->phase<0.0) penx+=32;
    else penx-=32;
    graf_decal(&g.graf,penx,peny,(g.framec&8)?144:176,64,32,32);
  }
}

/* Render.
 */
 
static void _sawing_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_labor);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_sawing={
  .name="sawing",
  .objlen=sizeof(struct battle_sawing),
  .strix_name=171,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_sawing_del,
  .init=_sawing_init,
  .update=_sawing_update,
  .render=_sawing_render,
};

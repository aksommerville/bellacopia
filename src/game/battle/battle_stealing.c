/* battle_stealing.c
 * Dragon in the middle with a pile of gold.
 * Players are free to move when the dragon's looking the other way.
 * If it sees you moving, or sees you holding gold, it burninates: You drop the gold and get pushed to the edge.
 */

#include "game/bellacopia.h"

#define PLAY_TIME 20.0
#define START_MARGIN 36.0 /* Pixels from horizontal edge, where we start. */
#define GROUNDY 140
#define END_COOLDOWN 1.0 /* Interval between clock running out and us reporting it. */
#define END_ZONE_WIDTH 32.0 /* Either piggy bank or dragon hoard, depending which side we're on. */
#define HEAD_TRAVEL_TIME 0.500
#define PERIOD_COUNT 5 /* Dragon will observe each side exactly so many times. */
#define BURNINATE_TIME 0.250 /* How long mouth stays open. Beware, watch cycles can stretch by this much. */
#define RECHARGE_TIME 1.000
#define FIREBALL_LIMIT 8
#define FIREBALL_SPEED 200.0 /* px/s */
#define HURT_SPEED 200.0
#define WALK_SPEED_LOW 70.0
#define WALK_SPEED_HIGH 120.0
#define HUMAN_SPEED_BIAS 1.125 /* Humans get to go a little faster, to make up for the CPU's perfect reaction time. */
#define CPU_TURNAROUND_TIME 0.300 /* CPU players changing direction (aside from stopping for the dragon), they wait this long first. */
#define SPARKLE_LIMIT 3
#define SPARKLE_TTL 1.0
#define SPARKLE_WAIT 0.4

struct battle_stealing {
  uint8_t handicap;
  uint8_t players;
  void (*cb_end)(int outcome,void *userdata);
  void *userdata;
  int outcome;
  double playclock;
  double cooldown;
  
  struct player {
    int who;
    int human;
    double skill; // Normalized from handicap.
    uint8_t tileid; // First of 8. Hold, Throw, (Win), (Lose), Ball, (Walk1), (Walk2), (Idle).
    double x;
    int indx; // -1,0,1
    int facedx; // -1,1
    int outcome; // -1,0,1 = lose,undetermined,win. As pertains to this player.
    int carrying;
    int score; // How many coins delivered.
    double animclock;
    int animframe;
    double walk_speed; // px/s
    double xlo,xhi; // How far I'm allowed to travel. Knowable from (who).
    int hurt; // Fireball sets, and player unsets after knocking to the edge.
    double turnaround; // CPU only
  } playerv[2];
  
  struct {
    double headx; // -1..1
    uint8_t headtile;
    uint8_t headxform;
    double watchtimev[PERIOD_COUNT*2];
    int watchtimep; // Points to the next cycle, if one is in progress (ie never zero).
    double watchclock;
    double burnclock; // Also counts as watch time while running.
    double recharge; // Counts down after burning.
    double travelclock;
    double traveldx; // -1,1. Direction of next or current move. Opposite of (headx) during a watch cycle.
  } dragon;
  
  struct fireball {
    double x,y; // px
    double dx,dy; // px/s
    double animclock;
    int animframe; // 0..3
  } fireballv[FIREBALL_LIMIT];
  int fireballc;
  
  struct sparkle {
    int x,y;
    double ttl;
  } sparklev[SPARKLE_LIMIT];
  int sparklec;
};

#define CTX ((struct battle_stealing*)ctx)

/* Delete.
 */
 
static void _stealing_del(void *ctx) {
  free(ctx);
}

/* Initialize player.
 */
 
static void player_init(void *ctx,struct player *player,int human,int appearance) {
  player->human=human;
  player->walk_speed=WALK_SPEED_LOW*(1.0-player->skill)+WALK_SPEED_HIGH*player->skill;
  if (human) player->walk_speed*=HUMAN_SPEED_BIAS;
  if (!player->who) { // Left.
    player->facedx=1;
    player->x=START_MARGIN;
    player->xlo=0.0;
    player->xhi=FBW*0.5;
  } else { // Right.
    player->facedx=-1;
    player->x=FBW-START_MARGIN;
    player->xlo=FBW*0.5;
    player->xhi=FBW;
  }
  switch (appearance) {
    case 0: { // Roblin.
        player->tileid=0xa0;
      } break;
    case 1: { // Dot.
        player->tileid=0x70;
      } break;
    case 2: { // Princess.
        player->tileid=0x90;
      } break;
  }
}

/* Initialize the dragon, his whole action plan.
 */
 
static double nearish(double target) {
  double ish=1.0+(((rand()&0xffff)-0x8000)/100000.0);
  return target*ish;
}
 
static void dragon_init(void *ctx) {
  
  /* Regardless of handicap, the dragon will spend exactly the same amount of time facing left as right.
   * Furthermore, that time will be divided into chunks such that each side gets the same set of chunks (not necessarily in the same order).
   */
  int tripc=PERIOD_COUNT*2-1;
  double triptime=HEAD_TRAVEL_TIME*tripc;
  double watchtime=PLAY_TIME-triptime;
  
  // Select (PERIOD_COUNT) durations, first fuzzing a bit around (1/n), then the last one is whatever's left.
  double lv[PERIOD_COUNT];
  double remaining=watchtime*0.5; // *0.5 because we're only generating half of them.
  int n=PERIOD_COUNT;
  while (n-->1) {
    lv[n]=nearish(remaining/(n+1));
    remaining-=lv[n];
  }
  lv[0]=remaining;
  
  // Then randomly reorder (lv) into (rv).
  double tmpv[PERIOD_COUNT];
  memcpy(tmpv,lv,sizeof(tmpv));
  double rv[PERIOD_COUNT];
  int tmpc=PERIOD_COUNT,rc=0;
  for (;rc<PERIOD_COUNT;rc++) {
    int tmpp=rand()%tmpc;
    rv[rc]=tmpv[tmpp];
    tmpc--;
    memmove(tmpv+tmpp,tmpv+tmpp+1,sizeof(double)*(tmpc-tmpp));
  }
  
  // Interleave (lv[n],rv[n]) into (watchtimev).
  // We called them "l" and "r", but they might be the reverse. Doesn't matter.
  double *dst=CTX->dragon.watchtimev;
  const double *srcl=lv,*srcr=rv;
  int i=PERIOD_COUNT;
  while (i-->0) {
    *(dst++)=*srcl++;
    *(dst++)=*srcr++;
  }
  CTX->dragon.watchtimep=0;
  
  /* We start at the beginning of a watch cycle.
   * Choose randomly left or right.
   */
  if (rand()&1) { // Left.
    CTX->dragon.headx=-1.0;
    CTX->dragon.headxform=EGG_XFORM_XREV;
  } else { // Right.
    CTX->dragon.headx=1.0;
    CTX->dragon.headxform=0;
  }
  CTX->dragon.headtile=0x26;
  CTX->dragon.watchclock=CTX->dragon.watchtimev[CTX->dragon.watchtimep++];
  CTX->dragon.traveldx=-CTX->dragon.headx;
}

/* New.
 */
 
static void *_stealing_init(
  uint8_t handicap,
  uint8_t players,
  void (*cb_end)(int outcome,void *userdata),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_stealing));
  if (!ctx) return 0;
  CTX->handicap=handicap;
  CTX->players=players;
  CTX->cb_end=cb_end;
  CTX->userdata=userdata;
  CTX->outcome=-2;
  CTX->playclock=PLAY_TIME;
  
  dragon_init(ctx);
  
  CTX->playerv[0].who=0;
  CTX->playerv[1].who=1;
  CTX->playerv[1].skill=(double)handicap/256.0;
  CTX->playerv[0].skill=1.0-CTX->playerv[1].skill;
  
  switch (CTX->players) {
    case NS_players_cpu_cpu: {
        player_init(ctx,CTX->playerv+0,0,2);
        player_init(ctx,CTX->playerv+1,0,0);
      } break;
    case NS_players_cpu_man: {
        player_init(ctx,CTX->playerv+0,0,0);
        player_init(ctx,CTX->playerv+1,2,2);
      } break;
    case NS_players_man_cpu: {
        player_init(ctx,CTX->playerv+0,1,1);
        player_init(ctx,CTX->playerv+1,0,0);
      } break;
    case NS_players_man_man: {
        player_init(ctx,CTX->playerv+0,0,0);
        player_init(ctx,CTX->playerv+1,2,2);
      } break;
    default: _stealing_del(ctx); return 0;
  }
  return ctx;
}

/* Update human player.
 */
 
static void player_update_man(void *ctx,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(void *ctx,struct player *player,double elapsed) {

  /* If the dragon is looking at me, or within a certain threshold of it, stop.
   * This is immune to the turnaround penalty.
   */
  const double SCARE_THRESHOLD=0.500;
  int scared=0;
  if (player->who) {
    if (CTX->dragon.headx>=SCARE_THRESHOLD) scared=1;
  } else {
    if (CTX->dragon.headx<=-SCARE_THRESHOLD) scared=1;
  }
  if (scared) {
    // No need to check whether I'm showing treasure to the dragon -- if we're holding treasure, we're facing piggyward.
    player->indx=0;
    return;
  }
  
  /* Walk toward the hoard or the piggy bank, depending on whether I'm carrying gold.
   * But if the desired direction is a change, tick out the turnaround clock first.
   * Using '!=' instead of '=-' means we apply the turnaround penalty resuming after the dragon too.
   */
  int nindx;
  if (player->carrying) { // piggyward
    nindx=player->who?1:-1;
  } else { // hoardward
    nindx=player->who?-1:1;
  }
  if (nindx!=player->indx) {
    if (player->turnaround<=0.0) {
      player->turnaround=CPU_TURNAROUND_TIME;
      player->indx=0;
    } else if ((player->turnaround-=elapsed)<=0.0) {
      player->indx=nindx;
    }
  }
}

/* Called when player is in the end zone.
 * If gathering or depositing is possible, we effect it.
 */
 
static void player_check_gather(void *ctx,struct player *player) {
  if (player->carrying) return;
  player->carrying=1;
  bm_sound(RID_sound_collect);
}

static void player_check_deposit(void *ctx,struct player *player) {
  if (!player->carrying) return;
  player->carrying=0;
  bm_sound(RID_sound_deposit_coin);
  player->score++;
}

/* Update all players, after the man/cpu controller bit.
 * Controllers are expected to set (indx) and that's about it.
 */
 
static void player_update_common(void *ctx,struct player *player,double elapsed) {

  // If hurt, we slide fast to the outer edge.
  if (player->hurt) {
    double dx=(player->who?HURT_SPEED:-HURT_SPEED)*elapsed;
    player->x+=dx;
    if (player->x<=0.0) {
      player->x=0.0;
      player->hurt=0;
    } else if (player->x>=FBW) {
      player->x=FBW;
      player->hurt=0;
    }
    return;
  }

  // Motion and animation.
  if (player->indx) {
    player->facedx=player->indx;
    player->x+=player->walk_speed*elapsed*player->indx;
    if (player->x<player->xlo) player->x=player->xlo;
    else if (player->x>player->xhi) player->x=player->xhi;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.200;
      if (++(player->animframe)>=4) player->animframe=0;
    }
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
  
  // Check gathitude and deposition.
  if (player->x<player->xlo+END_ZONE_WIDTH) {
    if (player->who) player_check_gather(ctx,player);
    else player_check_deposit(ctx,player);
  } else if (player->x>player->xhi-END_ZONE_WIDTH) {
    if (player->who) player_check_deposit(ctx,player);
    else player_check_gather(ctx,player);
  }
}

/* Generate a new fireball.
 */
 
static void stealing_generate_fireball(void *ctx,double x,double y,double xtra) {
  if (CTX->fireballc>=FIREBALL_LIMIT) return;
  struct fireball *fireball=CTX->fireballv+CTX->fireballc++;
  fireball->x=x;
  fireball->y=y;
  struct player *victim;
  if (x<FBW>>1) victim=CTX->playerv+0;
  else victim=CTX->playerv+1;
  double dx=victim->x+xtra-fireball->x;
  double dy=GROUNDY-(NS_sys_tilesize>>1)-y;
  double distance=sqrt(dx*dx+dy*dy); // Always nonzero, because (y) is always above the players.
  fireball->dx=(FIREBALL_SPEED*dx)/distance;
  fireball->dy=(FIREBALL_SPEED*dy)/distance;
}

/* Nonzero if the dragon ought to spit a fireball.
 */
 
static int dragon_should_burninate(void *ctx) {

  // Which player and which direction?
  struct player *player;
  int facedx_toward_me;
  if (CTX->dragon.headx<0.0) {
    player=CTX->playerv+0;
    facedx_toward_me=1;
  } else {
    player=CTX->playerv+1;
    facedx_toward_me=-1;
  }
  
  // Is she hurt? OK, ok, she's had enough.
  if (player->hurt) return 0;
  
  // Is she moving? BURN!
  if (player->indx) return 1;
  
  // Is she carrying my gold and looking at me? The cheek! BURN!
  if (player->carrying&&(player->facedx==facedx_toward_me)) return 1;
  
  // Not worth my trouble.
  return 0;
}

/* Update the dragon.
 */
 
static void dragon_update(void *ctx,double elapsed) {

  /* Travelling?
   * Update clock and position.
   */
  if (CTX->dragon.travelclock>0.0) {
    if ((CTX->dragon.travelclock-=elapsed)<=0.0) { // Arrived!
      CTX->dragon.headx=CTX->dragon.traveldx;
      CTX->dragon.traveldx=-CTX->dragon.traveldx;
      CTX->dragon.headtile=0x26;
      if (CTX->dragon.headx<0.0) CTX->dragon.headxform=EGG_XFORM_XREV;
      else CTX->dragon.headxform=0;
      if (CTX->dragon.watchtimep>=PERIOD_COUNT*2) { // No more durations... shouldn't happen.
        CTX->dragon.watchclock=1.0;
      } else {
        CTX->dragon.watchclock=CTX->dragon.watchtimev[CTX->dragon.watchtimep++];
      }
    } else { // Absolute positioning based on the clock, no need to do it incrementally.
      double from=-CTX->dragon.traveldx;
      double t=CTX->dragon.travelclock/HEAD_TRAVEL_TIME; // 0..1 = to..from, reverse of what you'd expect
      CTX->dragon.headx=from*t+CTX->dragon.traveldx*(1.0-t);
    }
    return;
  }
  
  /* If we had been burninating, tick that clock down and close mouth when appropriate.
   * If not burninating, should we be?
   */
  if (CTX->dragon.burnclock>0.0) {
    if ((CTX->dragon.burnclock-=elapsed)<=0.0) {
      CTX->dragon.headtile=0x26;
    }
  } else if (CTX->dragon.recharge>0.0) {
    CTX->dragon.recharge-=elapsed;
  } else if (dragon_should_burninate(ctx)) {
    CTX->dragon.burnclock=BURNINATE_TIME;
    CTX->dragon.recharge=RECHARGE_TIME;
    CTX->dragon.headtile=0x07;
    double x=(FBW>>1)+CTX->dragon.headx*NS_sys_tilesize;
    double y=GROUNDY-NS_sys_tilesize*2;
    stealing_generate_fireball(ctx,x,y,0.0);
    stealing_generate_fireball(ctx,x,y,-2.0*NS_sys_tilesize);
    stealing_generate_fireball(ctx,x,y, 2.0*NS_sys_tilesize);
    bm_sound(RID_sound_breathe_fire);
  }
  
  /* Tick down watchclock.
   * When both watchclock and burnclock are negative, begin the next travel.
   * Due to the burnclock, we can be a little late to plan. Should be ok; the last phase is a travel.
   */
  if ((CTX->dragon.watchclock-=elapsed)<=0.0) {
    if (CTX->dragon.burnclock<=0.0) {
      CTX->dragon.travelclock=HEAD_TRAVEL_TIME;
      CTX->dragon.headtile=0x06;
      CTX->dragon.recharge=0.0;
    }
  }
}

/* Update a fireball.
 * Return zero if defunct or nonzero to carry on.
 */
 
static int fireball_update(void *ctx,struct fireball *fireball,double elapsed) {

  fireball->x+=fireball->dx*elapsed;
  fireball->y+=fireball->dy*elapsed;
  if (fireball->x<0.0) return 0;
  if (fireball->x>FBW) return 0;
  if (fireball->y>GROUNDY) return 0;
  
  if ((fireball->animclock-=elapsed)<=0.0) {
    fireball->animclock+=0.100;
    if (++(fireball->animframe)>=4) fireball->animframe=0;
  }
  
  const double radius=NS_sys_tilesize;
  struct player *player=CTX->playerv;
  int i=2; for (;i-->0;player++) {
    double dx=player->x-fireball->x;
    if ((dx<-radius)||(dx>radius)) continue;
    double dy=GROUNDY-(NS_sys_tilesize>>1)-fireball->y;
    if ((dy<-radius)||(dy>radius)) continue;
    bm_sound(RID_sound_ouch);
    player->hurt=1;
    player->carrying=0;
    return 0;
  }
  
  return 1;
}

/* Cooldown expired.
 * (playerv->outcome) must be established already.
 * Set (CTX->outcome) accordingly, and call the owner.
 */
 
static void stealing_assign_outcome(void *ctx) {
  if (CTX->playerv[0].outcome>0) {
    if (CTX->playerv[1].outcome>0) CTX->outcome=0; // Everybody wins!
    else CTX->outcome=1; // Left wins!
  } else if (CTX->playerv[1].outcome>0) CTX->outcome=-1; // Right wins!
  else CTX->outcome=0; // Everybody loses! Hooray!
  if (CTX->cb_end) {
    CTX->cb_end(CTX->outcome,CTX->userdata);
    CTX->cb_end=0;
  }
}

/* Play clock expired.
 * Select winner and assign (playerv->outcome).
 * Do not assign (CTX->outcome) yet -- that will happen after a cooldown.
 */
 
static void stealing_select_winners(void *ctx) {
  struct player *l=CTX->playerv+0;
  struct player *r=CTX->playerv+1;
  
  // More coins wins, obviously.
  if (l->score>r->score) {
    l->outcome=1;
    r->outcome=-1;
    return;
  }
  if (l->score<r->score) {
    l->outcome=-1;
    r->outcome=1;
    return;
  }
  
  /* Piggy bank ties are common.
   * Break based on who is closest to delivering.
   * First off, if one is carrying and the other not, the carrier wins.
   */
  if (l->carrying&&!r->carrying) {
    l->outcome=1;
    r->outcome=-1;
    return;
  }
  if (!l->carrying&&r->carrying) {
    l->outcome=-1;
    r->outcome=1;
    return;
  }
  
  /* Calculate distance to the next checkpoint for each.
   * If one is greater by some arbitrary margin, that one loses.
   * The margin should be wide enough for humans to see the difference plainly.
   */
  const double margin=NS_sys_tilesize*2.0;
  double ld,rd;
  if (l->carrying) { // Distance to outside.
    ld=l->x;
    rd=FBW-r->x;
  } else { // Distance to inside.
    ld=FBW-l->x;
    rd=r->x;
  }
  if (ld<rd-margin) {
    l->outcome=1;
    r->outcome=-1;
  } else if (rd<ld-margin) {
    l->outcome=-1;
    r->outcome=1;
  } else { // Hooray everybody loses!
    l->outcome=-1;
    r->outcome=-1;
  }
}

/* Update sparkles.
 */
 
static void sparkles_update(void *ctx,double elapsed) {
  struct sparkle *sparkle;
  int i;

  // Create a new one if there's room, and existing ones are at least a tasteful interval old.
  if (CTX->sparklec<SPARKLE_LIMIT) {
    int oldenough=1;
    for (sparkle=CTX->sparklev,i=CTX->sparklec;i-->0;sparkle++) {
      if (sparkle->ttl>SPARKLE_TTL-SPARKLE_WAIT) {
        oldenough=0;
        break;
      }
    }
    if (oldenough) {
      sparkle=CTX->sparklev+CTX->sparklec++;
      sparkle->ttl=SPARKLE_TTL;
      sparkle->x=(FBW>>1)-(NS_sys_tilesize*2)-(NS_sys_tilesize>>1)+rand()%(NS_sys_tilesize*5);
      sparkle->y=GROUNDY-rand()%NS_sys_tilesize;
    }
  }
  
  // Advance timers and drop expired ones.
  for (i=CTX->sparklec,sparkle=CTX->sparklev+CTX->sparklec-1;i-->0;sparkle--) {
    if ((sparkle->ttl-=elapsed)<=0.0) {
      CTX->sparklec--;
      memmove(sparkle,sparkle+1,sizeof(struct sparkle)*(CTX->sparklec-i));
    }
  }
}

/* Update.
 */
 
static void _stealing_update(void *ctx,double elapsed) {

  // Done?
  if (CTX->outcome>-2) return;
  
  // Cooldown in progress?
  if (CTX->cooldown>0.0) {
    if ((CTX->cooldown-=elapsed)<=0.0) {
      stealing_assign_outcome(ctx);
    }
    return;
  }
  
  // Tick playclock.
  if ((CTX->playclock-=elapsed)<=0.0) {
    CTX->cooldown=END_COOLDOWN;
    stealing_select_winners(ctx);
    return;
  }
  
  // Update players.
  struct player *player=CTX->playerv;
  int i=2; for (;i-->0;player++) {
    if (player->human) player_update_man(ctx,player,elapsed,g.input[player->human]);
    else player_update_cpu(ctx,player,elapsed);
    player_update_common(ctx,player,elapsed);
  }
  
  // And the dragon.
  dragon_update(ctx,elapsed);
  
  // And the fireballs.
  struct fireball *fireball=CTX->fireballv+CTX->fireballc-1;
  for (i=CTX->fireballc;i-->0;fireball--) {
    if (fireball_update(ctx,fireball,elapsed)) continue;
    CTX->fireballc--;
    memmove(fireball,fireball+1,sizeof(struct fireball)*(CTX->fireballc-i));
  }
  
  sparkles_update(ctx,elapsed);
}

/* Render player.
 */
 
static void player_render(void *ctx,struct player *player) {
  int x=(int)player->x;
  int y=GROUNDY-(NS_sys_tilesize>>1);
  uint8_t xform=(player->facedx<0)?EGG_XFORM_XREV:0;
  
  // Celebration or misery dance, if established.
  if (player->outcome>0) {
    if (g.framec&8) y--;
    graf_tile(&g.graf,x,y,player->tileid+2,xform);
    return;
  } else if (player->outcome<0) {
    graf_tile(&g.graf,x,y,player->tileid+3,xform);
    return;
  }
  
  // Main body per (animframe).
  uint8_t tileid=player->tileid+7;
  switch (player->animframe) {
    case 1: tileid-=1; break;
    case 3: tileid-=2; break;
  }
  if (player->hurt) graf_set_tint(&g.graf,0xff0000c0);
  graf_tile(&g.graf,x,y,tileid,xform);
  graf_set_tint(&g.graf,0);
  
  // Draw a coin on top if we're carrying one.
  if (player->carrying) {
    int coinx=x+(NS_sys_tilesize>>1)*player->facedx;
    int coiny=y+2;
    graf_tile(&g.graf,coinx,coiny,0x6b,0);
  }
}

/* Render fireball.
 */
 
static void fireball_render(void *ctx,struct fireball *fireball) {
  uint8_t tileid=0x17;
  uint8_t xform=0;
  switch (fireball->animframe) {
    case 1: xform=EGG_XFORM_XREV; break;
    case 2: tileid+=0x10; break;
    case 3: tileid+=0x10; xform=EGG_XFORM_XREV; break;
  }
  graf_tile(&g.graf,(int)fireball->x,(int)fireball->y,tileid,xform);
}

/* Render sparkle.
 */
 
static void sparkle_render(void *ctx,struct sparkle *sparkle) {
  // 3 frames pingponging, so 5.
  int frame=(int)((sparkle->ttl*5.0)/SPARKLE_TTL);
  if (frame<0) frame=0; else if (frame>4) frame=4;
  uint8_t tileid=0x05;
  switch (frame) {
    case 1: tileid+=0x10; break;
    case 2: tileid+=0x20; break;
    case 3: tileid+=0x10; break;
  }
  graf_tile(&g.graf,sparkle->x,sparkle->y,tileid,0);
}

/* Render.
 */
 
static void _stealing_render(void *ctx) {

  // Background. Then everything else comes from the one image.
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,0x785830ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,0x3c2011ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_goblins);
  
  // Dragon's body and the pile of gold, a static image.
  const int dragonw=NS_sys_tilesize*5;
  const int dragonh=NS_sys_tilesize*3;
  graf_decal(&g.graf,(FBW>>1)-(dragonw>>1),GROUNDY-dragonh,0,0,dragonw,dragonh);
  
  // Sparkles on the gold.
  struct sparkle *sparkle=CTX->sparklev;
  int i=CTX->sparklec;
  for (;i-->0;sparkle++) sparkle_render(ctx,sparkle);
  
  // Fireballs.
  struct fireball *fireball=CTX->fireballv;
  for (i=CTX->fireballc;i-->0;fireball++) fireball_render(ctx,fireball);
  
  // Dragon's head.
  int headx=(int)((FBW>>1)+CTX->dragon.headx*NS_sys_tilesize);
  int heady=GROUNDY-NS_sys_tilesize*2-(NS_sys_tilesize>>1);
  graf_tile(&g.graf,headx,heady,CTX->dragon.headtile,CTX->dragon.headxform);
  
  // Decorative piggy banks.
  graf_tile(&g.graf,NS_sys_tilesize,GROUNDY-(NS_sys_tilesize>>1),0x6c,0);
  graf_tile(&g.graf,FBW-NS_sys_tilesize,GROUNDY-(NS_sys_tilesize>>1),0x6c,EGG_XFORM_XREV);
  
  // Players.
  player_render(ctx,CTX->playerv+0);
  player_render(ctx,CTX->playerv+1);
  
  // Number overlays.
  // Scores can't get anywhere near 10, don't worry.
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_tile(&g.graf,NS_sys_tilesize,GROUNDY+(NS_sys_tilesize>>1),'0'+CTX->playerv[0].score,0);
  graf_tile(&g.graf,FBW-NS_sys_tilesize,GROUNDY+(NS_sys_tilesize>>1),'0'+CTX->playerv[1].score,0);
  // Could render the clock if we like. I don't really feel a need for it.
}

/* Type definition.
 */
 
const struct battle_type battle_type_stealing={
  .name="stealing",
  .strix_name=50,
  .no_article=0,
  .no_contest=0,
  .supported_players=(1<<NS_players_cpu_cpu)|(1<<NS_players_cpu_man)|(1<<NS_players_man_cpu)|(1<<NS_players_man_man),
  .del=_stealing_del,
  .init=_stealing_init,
  .update=_stealing_update,
  .render=_stealing_render,
};

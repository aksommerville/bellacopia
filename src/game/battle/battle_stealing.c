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
  struct battle hdr;
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

#define BATTLE ((struct battle_stealing*)battle)

/* Delete.
 */
 
static void _stealing_del(struct battle *battle) {
}

/* Initialize player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int appearance) {
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
 
static void dragon_init(struct battle *battle) {
  
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
  double *dst=BATTLE->dragon.watchtimev;
  const double *srcl=lv,*srcr=rv;
  int i=PERIOD_COUNT;
  while (i-->0) {
    *(dst++)=*srcl++;
    *(dst++)=*srcr++;
  }
  BATTLE->dragon.watchtimep=0;
  
  /* We start at the beginning of a watch cycle.
   * Choose randomly left or right.
   */
  if (rand()&1) { // Left.
    BATTLE->dragon.headx=-1.0;
    BATTLE->dragon.headxform=EGG_XFORM_XREV;
  } else { // Right.
    BATTLE->dragon.headx=1.0;
    BATTLE->dragon.headxform=0;
  }
  BATTLE->dragon.headtile=0x26;
  BATTLE->dragon.watchclock=BATTLE->dragon.watchtimev[BATTLE->dragon.watchtimep++];
  BATTLE->dragon.traveldx=-BATTLE->dragon.headx;
}

/* New.
 */
 
static int _stealing_init(struct battle *battle) {
  BATTLE->playclock=PLAY_TIME;
  
  dragon_init(battle);
  
  BATTLE->playerv[0].who=0;
  BATTLE->playerv[1].who=1;
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If the dragon is looking at me, or within a certain threshold of it, stop.
   * This is immune to the turnaround penalty.
   */
  const double SCARE_THRESHOLD=0.500;
  int scared=0;
  if (player->who) {
    if (BATTLE->dragon.headx>=SCARE_THRESHOLD) scared=1;
  } else {
    if (BATTLE->dragon.headx<=-SCARE_THRESHOLD) scared=1;
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
 
static void player_check_gather(struct battle *battle,struct player *player) {
  if (player->carrying) return;
  player->carrying=1;
  bm_sound(RID_sound_collect);
}

static void player_check_deposit(struct battle *battle,struct player *player) {
  if (!player->carrying) return;
  player->carrying=0;
  bm_sound(RID_sound_deposit_coin);
  player->score++;
}

/* Update all players, after the man/cpu controller bit.
 * Controllers are expected to set (indx) and that's about it.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

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
    if (player->who) player_check_gather(battle,player);
    else player_check_deposit(battle,player);
  } else if (player->x>player->xhi-END_ZONE_WIDTH) {
    if (player->who) player_check_deposit(battle,player);
    else player_check_gather(battle,player);
  }
}

/* Generate a new fireball.
 */
 
static void stealing_generate_fireball(struct battle *battle,double x,double y,double xtra) {
  if (BATTLE->fireballc>=FIREBALL_LIMIT) return;
  struct fireball *fireball=BATTLE->fireballv+BATTLE->fireballc++;
  fireball->x=x;
  fireball->y=y;
  struct player *victim;
  if (x<FBW>>1) victim=BATTLE->playerv+0;
  else victim=BATTLE->playerv+1;
  double dx=victim->x+xtra-fireball->x;
  double dy=GROUNDY-(NS_sys_tilesize>>1)-y;
  double distance=sqrt(dx*dx+dy*dy); // Always nonzero, because (y) is always above the players.
  fireball->dx=(FIREBALL_SPEED*dx)/distance;
  fireball->dy=(FIREBALL_SPEED*dy)/distance;
}

/* Nonzero if the dragon ought to spit a fireball.
 */
 
static int dragon_should_burninate(struct battle *battle) {

  // Which player and which direction?
  struct player *player;
  int facedx_toward_me;
  if (BATTLE->dragon.headx<0.0) {
    player=BATTLE->playerv+0;
    facedx_toward_me=1;
  } else {
    player=BATTLE->playerv+1;
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
 
static void dragon_update(struct battle *battle,double elapsed) {

  /* Travelling?
   * Update clock and position.
   */
  if (BATTLE->dragon.travelclock>0.0) {
    if ((BATTLE->dragon.travelclock-=elapsed)<=0.0) { // Arrived!
      BATTLE->dragon.headx=BATTLE->dragon.traveldx;
      BATTLE->dragon.traveldx=-BATTLE->dragon.traveldx;
      BATTLE->dragon.headtile=0x26;
      if (BATTLE->dragon.headx<0.0) BATTLE->dragon.headxform=EGG_XFORM_XREV;
      else BATTLE->dragon.headxform=0;
      if (BATTLE->dragon.watchtimep>=PERIOD_COUNT*2) { // No more durations... shouldn't happen.
        BATTLE->dragon.watchclock=1.0;
      } else {
        BATTLE->dragon.watchclock=BATTLE->dragon.watchtimev[BATTLE->dragon.watchtimep++];
      }
    } else { // Absolute positioning based on the clock, no need to do it incrementally.
      double from=-BATTLE->dragon.traveldx;
      double t=BATTLE->dragon.travelclock/HEAD_TRAVEL_TIME; // 0..1 = to..from, reverse of what you'd expect
      BATTLE->dragon.headx=from*t+BATTLE->dragon.traveldx*(1.0-t);
    }
    return;
  }
  
  /* If we had been burninating, tick that clock down and close mouth when appropriate.
   * If not burninating, should we be?
   */
  if (BATTLE->dragon.burnclock>0.0) {
    if ((BATTLE->dragon.burnclock-=elapsed)<=0.0) {
      BATTLE->dragon.headtile=0x26;
    }
  } else if (BATTLE->dragon.recharge>0.0) {
    BATTLE->dragon.recharge-=elapsed;
  } else if (dragon_should_burninate(battle)) {
    BATTLE->dragon.burnclock=BURNINATE_TIME;
    BATTLE->dragon.recharge=RECHARGE_TIME;
    BATTLE->dragon.headtile=0x07;
    double x=(FBW>>1)+BATTLE->dragon.headx*NS_sys_tilesize;
    double y=GROUNDY-NS_sys_tilesize*2;
    stealing_generate_fireball(battle,x,y,0.0);
    stealing_generate_fireball(battle,x,y,-2.0*NS_sys_tilesize);
    stealing_generate_fireball(battle,x,y, 2.0*NS_sys_tilesize);
    bm_sound(RID_sound_breathe_fire);
  }
  
  /* Tick down watchclock.
   * When both watchclock and burnclock are negative, begin the next travel.
   * Due to the burnclock, we can be a little late to plan. Should be ok; the last phase is a travel.
   */
  if ((BATTLE->dragon.watchclock-=elapsed)<=0.0) {
    if (BATTLE->dragon.burnclock<=0.0) {
      BATTLE->dragon.travelclock=HEAD_TRAVEL_TIME;
      BATTLE->dragon.headtile=0x06;
      BATTLE->dragon.recharge=0.0;
    }
  }
}

/* Update a fireball.
 * Return zero if defunct or nonzero to carry on.
 */
 
static int fireball_update(struct battle *battle,struct fireball *fireball,double elapsed) {

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
  struct player *player=BATTLE->playerv;
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
 * Set (BATTLE->outcome) accordingly, and call the owner.
 */
 
static void stealing_assign_outcome(struct battle *battle) {
  if (BATTLE->playerv[0].outcome>0) {
    if (BATTLE->playerv[1].outcome>0) battle->outcome=0; // Everybody wins!
    else battle->outcome=1; // Left wins!
  } else if (BATTLE->playerv[1].outcome>0) battle->outcome=-1; // Right wins!
  else battle->outcome=0; // Everybody loses! Hooray!
}

/* Play clock expired.
 * Select winner and assign (playerv->outcome).
 * Do not assign (BATTLE->outcome) yet -- that will happen after a cooldown.
 */
 
static void stealing_select_winners(struct battle *battle) {
  struct player *l=BATTLE->playerv+0;
  struct player *r=BATTLE->playerv+1;
  
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
 
static void sparkles_update(struct battle *battle,double elapsed) {
  struct sparkle *sparkle;
  int i;

  // Create a new one if there's room, and existing ones are at least a tasteful interval old.
  if (BATTLE->sparklec<SPARKLE_LIMIT) {
    int oldenough=1;
    for (sparkle=BATTLE->sparklev,i=BATTLE->sparklec;i-->0;sparkle++) {
      if (sparkle->ttl>SPARKLE_TTL-SPARKLE_WAIT) {
        oldenough=0;
        break;
      }
    }
    if (oldenough) {
      sparkle=BATTLE->sparklev+BATTLE->sparklec++;
      sparkle->ttl=SPARKLE_TTL;
      sparkle->x=(FBW>>1)-(NS_sys_tilesize*2)-(NS_sys_tilesize>>1)+rand()%(NS_sys_tilesize*5);
      sparkle->y=GROUNDY-rand()%NS_sys_tilesize;
    }
  }
  
  // Advance timers and drop expired ones.
  for (i=BATTLE->sparklec,sparkle=BATTLE->sparklev+BATTLE->sparklec-1;i-->0;sparkle--) {
    if ((sparkle->ttl-=elapsed)<=0.0) {
      BATTLE->sparklec--;
      memmove(sparkle,sparkle+1,sizeof(struct sparkle)*(BATTLE->sparklec-i));
    }
  }
}

/* Update.
 */
 
static void _stealing_update(struct battle *battle,double elapsed) {

  // Done?
  if (battle->outcome>-2) return;
  
  // Cooldown in progress?
  if (BATTLE->cooldown>0.0) {
    if ((BATTLE->cooldown-=elapsed)<=0.0) {
      stealing_assign_outcome(battle);
    }
    return;
  }
  
  // Tick playclock.
  if ((BATTLE->playclock-=elapsed)<=0.0) {
    BATTLE->cooldown=END_COOLDOWN;
    stealing_select_winners(battle);
    return;
  }
  
  // Update players.
  struct player *player=BATTLE->playerv;
  int i=2; for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  // And the dragon.
  dragon_update(battle,elapsed);
  
  // And the fireballs.
  struct fireball *fireball=BATTLE->fireballv+BATTLE->fireballc-1;
  for (i=BATTLE->fireballc;i-->0;fireball--) {
    if (fireball_update(battle,fireball,elapsed)) continue;
    BATTLE->fireballc--;
    memmove(fireball,fireball+1,sizeof(struct fireball)*(BATTLE->fireballc-i));
  }
  
  sparkles_update(battle,elapsed);
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
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
 
static void fireball_render(struct battle *battle,struct fireball *fireball) {
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
 
static void sparkle_render(struct battle *battle,struct sparkle *sparkle) {
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
 
static void _stealing_render(struct battle *battle) {

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
  struct sparkle *sparkle=BATTLE->sparklev;
  int i=BATTLE->sparklec;
  for (;i-->0;sparkle++) sparkle_render(battle,sparkle);
  
  // Fireballs.
  struct fireball *fireball=BATTLE->fireballv;
  for (i=BATTLE->fireballc;i-->0;fireball++) fireball_render(battle,fireball);
  
  // Dragon's head.
  int headx=(int)((FBW>>1)+BATTLE->dragon.headx*NS_sys_tilesize);
  int heady=GROUNDY-NS_sys_tilesize*2-(NS_sys_tilesize>>1);
  graf_tile(&g.graf,headx,heady,BATTLE->dragon.headtile,BATTLE->dragon.headxform);
  
  // Decorative piggy banks.
  graf_tile(&g.graf,NS_sys_tilesize,GROUNDY-(NS_sys_tilesize>>1),0x6c,0);
  graf_tile(&g.graf,FBW-NS_sys_tilesize,GROUNDY-(NS_sys_tilesize>>1),0x6c,EGG_XFORM_XREV);
  
  // Players.
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // Number overlays.
  // Scores can't get anywhere near 10, don't worry.
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_tile(&g.graf,NS_sys_tilesize,GROUNDY+(NS_sys_tilesize>>1),'0'+BATTLE->playerv[0].score,0);
  graf_tile(&g.graf,FBW-NS_sys_tilesize,GROUNDY+(NS_sys_tilesize>>1),'0'+BATTLE->playerv[1].score,0);
  // Could render the clock if we like. I don't really feel a need for it.
}

/* Type definition.
 */
 
const struct battle_type battle_type_stealing={
  .name="stealing",
  .objlen=sizeof(struct battle_stealing),
  .strix_name=50,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_stealing_del,
  .init=_stealing_init,
  .update=_stealing_update,
  .render=_stealing_render,
};

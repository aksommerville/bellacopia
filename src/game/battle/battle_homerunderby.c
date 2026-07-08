/* battle_homerunderby.c
 * A to swing.
 */

#include "game/bellacopia.h"

#define SWINGT_LO (0.200*M_PI)
#define SWINGT_HI (1.700*M_PI)
#define SWING_SPEED  20.000
#define RETURN_SPEED  6.000
#define SCORE_LIMIT 6 /* 3 at-bats per player */
#define GRAVITY 300.0 /* px/s**2 */
#define GRAVITY_LIMIT 500.0 /* px/s */
#define BOUNCE_PENALTY 0.800 /* Velocity multiplier. */
#define MARGIN 8.0 /* px, ball goes so far offscreen before we call it */
#define XMARGIN 24.0 /* px. I stupidly drew the baselines 14 px lower than the natural diagonal, so this corrects for it. */

struct battle_homerunderby {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid;
    uint8_t placard;
    double swingt; // Radians clockwise from upward. Horizontally flipped for the right player.
    double speedlo,speedhi; // Pitch speed is the batters' property.
    int blackout; // Humans only.
    int swing; // Controller sets.
    int computed; // All go zero at the start of a pitch. CPU uses to signal it determined the swing time.
    double swingy; // CPU swings when ball crosses this threshold.
    double error; // Maximum CPU misjudgement of ball's position, in pixels.
  } playerv[2];
  
  struct player *batter; // One of (playerv) or null.
  struct score {
    int who; // Should alternate 0,1
    int homerun;
    double quality; // 0..1, If homerun.
  } scorev[SCORE_LIMIT];
  int scorec;
  
  double ballx,bally; // Framebuffer pixels of the shadow.
  double ballz; // Positive elevation, in projected framebuffer pixels. ie how far is the shadow from the ball.
  double balldx,balldy,balldz;
  double ballpvy; // Previous position of the ball.
  double windup; // Counts down while pitcher holding the ball.
  double reportclock;
  uint8_t rpttileid,rpttilec;
};

#define BATTLE ((struct battle_homerunderby*)battle)

/* Delete.
 */
 
static void _homerunderby_del(struct battle *battle) {
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
    player->error=(1.0-player->skill)*30.0;
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x29;
        player->placard=0x5c;
      } break;
    case NS_face_dot: {
        player->tileid=0x09;
        player->placard=0x3c;
      } break;
    case NS_face_princess: {
        player->tileid=0x19;
        player->placard=0x4c;
      } break;
  }
  player->swingt=SWINGT_HI;
  player->speedhi=100.0*player->skill+200.0*(1.0-player->skill);
  player->speedlo=player->speedhi*0.333;
  player->blackout=1;
}

/* Start the next pitch.
 */
 
static void homerunderby_begin_pitch(struct battle *battle) {
  if (BATTLE->scorec>=SCORE_LIMIT) {
    BATTLE->batter=0;
  } else {
    if (BATTLE->batter==BATTLE->playerv) BATTLE->batter=BATTLE->playerv+1;
    else BATTLE->batter=BATTLE->playerv;
  }
  BATTLE->playerv[0].computed=0;
  BATTLE->playerv[1].computed=0;
  BATTLE->ballx=FBW*0.5;
  BATTLE->bally=FBH*0.5;
  BATTLE->ballz=4.0;
  BATTLE->balldx=0.0;
  if (BATTLE->batter) {
    double n=(rand()&0xffff)/65535.0;
    BATTLE->balldy=n*BATTLE->batter->speedlo+(1.0-n)*BATTLE->batter->speedhi;
  } else {
    BATTLE->balldy=0.0;
  }
  BATTLE->balldz=0.0;
  BATTLE->windup=1.000;
}

/* New.
 */
 
static int _homerunderby_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  homerunderby_begin_pitch(battle);
  return 0;
}

/* Pitch outcomes.
 */
 
static void homerunderby_score(struct battle *battle,int homerun,double quality) {
  if (BATTLE->scorec>=SCORE_LIMIT) return;
  struct score *score=BATTLE->scorev+BATTLE->scorec++;
  if (BATTLE->batter) score->who=BATTLE->batter->who;
  score->homerun=homerun;
  score->quality=quality;
}
 
static void homerunderby_strike(struct battle *battle) {
  homerunderby_score(battle,0,0.0);
  homerunderby_begin_pitch(battle);
  BATTLE->reportclock=0.500;
  BATTLE->rpttileid=0x1a;
  BATTLE->rpttilec=4;
}
 
static void homerunderby_foul(struct battle *battle) {
  homerunderby_score(battle,0,0.0);
  homerunderby_begin_pitch(battle);
  BATTLE->reportclock=0.500;
  BATTLE->rpttileid=0x0a;
  BATTLE->rpttilec=3;
}
 
static void homerunderby_homerun(struct battle *battle) {
  double quality=((BATTLE->ballx/FBW)-0.5)*2.0;
  if (quality<0.0) quality=-quality;
  quality=1.0-quality;
  if (quality<0.0) quality=0.0; else if (quality>1.0) quality=1.0;
  homerunderby_score(battle,1,quality);
  homerunderby_begin_pitch(battle);
  BATTLE->reportclock=1.000;
  BATTLE->rpttileid=0x2a;
  BATTLE->rpttilec=5;
}

/* Player controls.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
    player->swing=0;
  } else {
    player->swing=(input&EGG_BTN_SOUTH);
  }
}
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (!player->computed) {
    player->computed=1;
    // (swingtime) is in fact constant, but let's recompute each time in case I change my mind.
    double targett=M_PI*0.500;
    double swingtime=(SWINGT_HI-targett)/SWING_SPEED;
    double targety=168.0; // Not sure why this is 168 and not 165, but 168 hits right up the middle, and 165 always swings a bit early.
    player->swingy=targety-swingtime*BATTLE->balldy;
    double error=(rand()&0xffff)/65535.0;
    error*=player->error;
    if (rand()&1) player->swingy+=error;
    else player->swingy-=error;
  }
  if (BATTLE->bally>player->swingy) {
    player->swing=1;
  } else {
    player->swing=0;
  }
}

/* Vertical position of my bat's business point, for comparison to (BATTLE->bally).
 * If negative, we are not eligible to hit the ball right now.
 */
static double player_get_bat_y(struct battle *battle,struct player *player) {
  if (player->swingt>M_PI) return -1.0; // Bat must be on the Home side of South.
  if (player->swingt<=SWINGT_LO) return -1.0; // Swing fully extended. There's no bunting in Home Run Derby!
  double y=165.0; // A little up from its rendered midpoint, because we compare to the ball's shadow, not the ball itself.
  double radius=10.0;
  y+=-cos(player->swingt)*radius;
  return y;
}
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->swing) {
    double ya=player_get_bat_y(battle,player);
    player->swingt-=SWING_SPEED*elapsed;
    if (player->swingt<SWINGT_LO) player->swingt=SWINGT_LO;
    double yz=player_get_bat_y(battle,player);
    if ((ya>0.0)&&(yz>0.0)&&(BATTLE->balldy>0.0)) {
      // (yz<ya) and (ballpvy<bally). If they overlap, we hit it.
      if ((yz<=BATTLE->bally)&&(ya>=BATTLE->ballpvy)) {
        bm_sound(RID_sound_whack);
        double ballt=player->swingt-M_PI*0.5;
        ballt*=2.0; // The quickie subtraction gives it a range close to the fair range. We need the ball to operate wider, to make fouls more likely.
        double velocity=(rand()&0xffff)/65535.0; // (x,y) velocity in m/s. We'll manage (z) separate.
        velocity=velocity*150.0+(1.0-velocity)*300.0;
        BATTLE->balldx=sin(ballt)*velocity;
        if (player->who) BATTLE->balldx=-BATTLE->balldx;
        BATTLE->balldy=-cos(ballt)*velocity;
        BATTLE->balldz=(rand()&0xffff)/65535.0;
        BATTLE->balldz=BATTLE->balldz*100.0+(1.0-BATTLE->balldz)*200.0;
      }
    }
  } else {
    player->swingt+=RETURN_SPEED*elapsed;
    if (player->swingt>SWINGT_HI) player->swingt=SWINGT_HI;
  }
}

/* Update.
 */
 
static void _homerunderby_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* Move the ball.
   * If it leaves the framebuffer along the bottom, that's a strike.
   * Striking with the bat is players' concern, not ours.
   */
  BATTLE->ballpvy=BATTLE->bally;
  if (BATTLE->reportclock>0.0) {
    BATTLE->reportclock-=elapsed;
  } else if (BATTLE->windup>0.0) {
    if ((BATTLE->windup-=elapsed)<=0.0) {
      bm_sound(RID_sound_throw);
    }
  } else {
    BATTLE->ballx+=BATTLE->balldx*elapsed;
    BATTLE->bally+=BATTLE->balldy*elapsed;
    BATTLE->ballz+=BATTLE->balldz*elapsed;
    if (BATTLE->ballz<0.0) { // Bounce if we hit the ground.
      bm_sound(RID_sound_bump);
      BATTLE->ballz=0.0;
      BATTLE->balldz=-BATTLE->balldz*BOUNCE_PENALTY;
      if (BATTLE->balldz<2.0) BATTLE->balldz=0.0;
    }
    if (BATTLE->balldy<=0.0) { // Moving outfieldward, apply gravity. And if we leave the framebuffer, it's foul or homerun.
      BATTLE->balldz-=GRAVITY*elapsed;
      if (BATTLE->balldz<-GRAVITY_LIMIT) BATTLE->balldz=-GRAVITY_LIMIT;
      if (BATTLE->bally<-MARGIN) {
        homerunderby_homerun(battle);
      } else if ((BATTLE->ballx<-XMARGIN)||(BATTLE->ballx>FBW+XMARGIN)) {
        homerunderby_foul(battle);
      }
    } else { // Moving homeward, strike if we leave the screen. Foul if horizontal. Beware that (balldy) might be extremely small.
      if (BATTLE->bally>FBH+MARGIN) {
        homerunderby_strike(battle);
      } else if ((BATTLE->ballx<-XMARGIN)||(BATTLE->ballx>FBW+XMARGIN)) {
        homerunderby_foul(battle);
      }
    }
  }
  
  /* Update players.
   * Only the active player gets updated, the other gets defaulted.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player==BATTLE->batter) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
      player_update_common(battle,player,elapsed);
    } else {
      player->swingt=SWINGT_HI;
      player->blackout=1;
      player->swing=0;
    }
  }
  
  /* If we filled the scoreboard, we're done.
   */
  if (BATTLE->scorec>=SCORE_LIMIT) {
    int lscore=0,rscore=0;
    const struct score *score=BATTLE->scorev;
    int i=BATTLE->scorec;
    for (;i-->0;score++) {
      if (!score->homerun) continue;
      if (score->who==0) lscore++;
      else if (score->who==1) rscore++;
    }
    if (lscore>rscore) battle->outcome=1;
    else if (lscore<rscore) battle->outcome=-1;
    else {
      // When the discrete score is tied, compare the sum of quality. This will almost never tie.
      double lq=0.0,rq=0.0;
      for (score=BATTLE->scorev,i=BATTLE->scorec;i-->0;score++) {
        if (!score->homerun) continue;
        if (score->who==0) lq+=score->quality;
        else if (score->who==1) rq+=score->quality;
      }
      if (lq>rq) battle->outcome=1;
      else if (lq<rq) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render.
 */
 
static uint32_t reportcolors[8]={
  0xff0000ff,0x00ff00ff,0x0000ffff,0xffff00ff,
  0x00ffffff,0xff00ffff,0xffffffff,0xff8000ff,
};
 
static void _homerunderby_render(struct battle *battle) {
  // Background is a static image, full framebuffer.
  graf_set_image(&g.graf,RID_image_ballpark);
  graf_decal(&g.graf,0,0,0,0,FBW,FBH);
  
  // Main sprites.
  graf_set_image(&g.graf,RID_image_battle_athletes);
  graf_tile(&g.graf,160,84,0x39,0); // Pitcher.
  graf_tile(&g.graf,(int)BATTLE->ballx,(int)BATTLE->bally,0x6a,0); // Shadow.
  graf_tile(&g.graf,(int)BATTLE->ballx,(int)(BATTLE->bally-BATTLE->ballz),0x69,0); // Ball.
  if (BATTLE->batter) {
    int batterx=BATTLE->batter->who?170:150;
    int battery=166;
    uint8_t xform=BATTLE->batter->who?EGG_XFORM_XREV:0;
    graf_tile(&g.graf,batterx,battery,BATTLE->batter->tileid,xform);
    int batx=batterx;
    int baty=battery+3;
    if (xform) batx-=4; else batx+=4;
    double t=BATTLE->batter->swingt;
    if (xform) t=-t;
    double batsint=sin(t);
    double batcost=cos(t);
    double batscale=0.750;
    graf_set_filter(&g.graf,1);
    graf_decal_rotate(&g.graf,batx,baty,NS_sys_tilesize*9,NS_sys_tilesize*4,NS_sys_tilesize*2,batsint,batcost,batscale);
    graf_set_filter(&g.graf,0);
  }
  
  // Report outcome of last at-bat.
  if (BATTLE->reportclock>0.0) {
    int dsty=(FBH>>1)+20;
    int dstx=(FBW>>1)-((BATTLE->rpttilec*NS_sys_tilesize)>>1)+(NS_sys_tilesize>>1);
    uint8_t tileid=BATTLE->rpttileid;
    uint32_t color=reportcolors[(g.framec>>3)&7];
    int i=BATTLE->rpttilec;
    for (;i-->0;dstx+=NS_sys_tilesize,tileid++) {
      graf_fancy(&g.graf,dstx,dsty,tileid,0,0,NS_sys_tilesize,0,color);
    }
  }
  
  // Scoreboard.
  graf_tile(&g.graf,13,160,BATTLE->playerv[0].placard,0);
  graf_tile(&g.graf,29,160,BATTLE->playerv[0].placard+1,0);
  graf_tile(&g.graf,13,170,BATTLE->playerv[1].placard,0);
  graf_tile(&g.graf,29,170,BATTLE->playerv[1].placard+1,0);
  if (BATTLE->scorec>0) {
    graf_set_image(&g.graf,RID_image_fonttiles);
    int lx=43,rx=43;
    const struct score *score=BATTLE->scorev;
    int i=BATTLE->scorec;
    for (;i-->0;score++) {
      if (score->who==0) {
        graf_tile(&g.graf,lx,161,'0'+score->homerun,0);
        lx+=10;
      } else if (score->who==1) {
        graf_tile(&g.graf,rx,171,'0'+score->homerun,0);
        rx+=10;
      }
    }
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_homerunderby={
  .name="homerunderby",
  .objlen=sizeof(struct battle_homerunderby),
  .id=NS_battle_homerunderby,
  .strix_name=161,
  .no_article=0,
  .no_contest=1,
  .support_pvp=1,
  .support_cvc=1,
  .del=_homerunderby_del,
  .init=_homerunderby_init,
  .update=_homerunderby_update,
  .render=_homerunderby_render,
};

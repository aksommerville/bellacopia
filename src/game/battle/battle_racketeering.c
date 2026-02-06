/* battle_racketeering.c
 * 3D racquetball.
 */

#include "game/bellacopia.h"

#define ZLIMIT FBH /* All three axes are measured in pixels. Pick a depth agreeable to the framebuffer. Positive Z is far, zero is the viewer. */
#define SPEED_MIN  80.0 /* px/s, MIN is at the least favorable handicap. */
#define SPEED_MAX 150.0
#define SWING_TIME 0.500
#define BALL_SPEED_INITIAL 100.0 /* px/s */
#define BALL_SPEED_INCREASE 1.125 /* Multiplier, at each stroke. */
#define WHACK_BIAS 40.0 /* Adjust per axis by so many m/s, according to position of stroke. */
#define BALL_GRAVITY 50.0 /* px/s**2. 80 feels realistic but more gravity makes it a lot harder to play. */
#define STRIKE_ZONE 16.0 /* pixels, 3-dimensional, how close at a swing to hit it. */
#define ANTI_WHIFF_Z_SCALE 0.200 /* 0..1, for swinging purposes, scale down the depth delta. */
#define BLINK_Z 50.0 /* Ball blinks when it's within range on Z. */
#define CPU_SERVE_DELAY 0.500 /* No serving this fast. */
#define SERVE_TIMEOUT 2.000 /* Must serve after so long. Otherwise you could just hold the ball and force a tie. */
#define CPU_SPEED_PENALTY 0.800 /* Humanity is at a severe disadvantage because the projection is hard to grok, and yknow, we're made of meat. Slow down CPU players artificially. */
#define END_COOLDOWN 1.0
#define THROW_LIMIT 3 /* CPU player with maximum skill will hit so many per volley, then miss deliberately. */

struct battle_racketeering {
  struct battle hdr;
  double cooldown;
  double serveto;
  
  struct player {
    int who;
    int human;
    double x,y; // In framebuffer pixels, which we'll align to z==0 in model space.
    double speed; // Governed by handicap.
    double skill;
    uint32_t color;
    double swing; // Counts down while visually swinging. The swing is effective the moment it goes positive.
    int blackout;
    double delay;
    double anyx,anyy; // CPU players choose a new resting position each time the ball is struck, just for variety's sake.
    int score;
    int throw; // CPU players count down and when this hits zero, they deliberately miss. Resets at each serve.
  } playerv[2];
  struct player *serving; // Null during a volley. If not null, it's one of (playerv).
  struct player *volley; // Null during serve. Otherwise, it's the player who's supposed to hit it next.
  
  struct ball {
    double x,y,z;
    double dx,dy,dz;
  } ball;
};

#define BATTLE ((struct battle_racketeering*)battle)

/* Delete.
 */
 
static void _racketeering_del(struct battle *battle) {
}

/* The other guy.
 */
 
static struct player *other_player(struct battle *battle,struct player *notme) {
  if (notme==BATTLE->playerv) return BATTLE->playerv+1;
  return BATTLE->playerv;
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int appearance) {
  player->blackout=1;
  player->delay=CPU_SERVE_DELAY;
  player->human=human;
  player->anyy=player->y=FBH>>1;
  if (!player->who) { // Left
    player->anyx=player->x=FBW*0.333;
    player->skill=1.0-battle->args.bias/255.0;
  } else { // Right
    player->anyx=player->x=FBW*0.666;
    player->skill=battle->args.bias/255.0;
  }
  player->speed=SPEED_MIN*(1.0-player->skill)+SPEED_MAX*player->skill;
  if (!human) player->speed*=CPU_SPEED_PENALTY;
  switch (appearance) {
    case 0: { // Moblin
        player->color=0x405010ff;
      } break;
    case 1: { // Dot
        player->color=0x411775ff;
      } break;
    case 2: { // Princess
        player->color=0x0d3ac1ff;
      } break;
  }
}

/* New.
 */

static int _racketeering_init(struct battle *battle) {
  
  BATTLE->playerv[0].who=0;
  BATTLE->playerv[1].who=1;
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  // With a discrepancy of skill, let the unskilled player serve first.
  // Very unskilled CPU players will miss every volley, so basically whoever gets the first serve wins.
  if (battle->args.bias<=0x70) BATTLE->serving=BATTLE->playerv+1;
  else if (battle->args.bias>=0x90) BATTLE->serving=BATTLE->playerv+0;
  else BATTLE->serving=BATTLE->playerv+(rand()&1);
  
  return 0;
}

/* Align ball to a player, when serving.
 * We cheat it a little for visual appeal, so don't make any assumptions.
 */
 
static void ball_align_to_player(struct battle *battle,struct ball *ball,struct player *player) {
  ball->x=BATTLE->serving->x;
  ball->y=BATTLE->serving->y-2.0;
  ball->z=0.0;
}

/* Choose a random serve vector.
 */
 
static void ball_random_serve(struct battle *battle,struct ball *ball) {

  // Pick three random numbers, and bias Z substantially -- it should go mostly toward the back wall.
  double x=((rand()&0xffff)-0x8000)/32768.0;
  double y=((rand()&0xffff)-0x8000)/32768.0;
  double z=1.0+(rand()&0xffff)/65536.0;
  
  // Normalize.
  double d=sqrt(x*x+y*y+z*z);
  x/=d;
  y/=d;
  z/=d;
  
  // Scale by constant initial speed.
  ball->dx=x*BALL_SPEED_INITIAL;
  ball->dy=y*BALL_SPEED_INITIAL;
  ball->dz=z*BALL_SPEED_INITIAL;
}

/* Each time the ball is served or struck, reset all CPU "any" positions.
 * More skilled players will approach the center.
 */
 
static void racketeering_reset_anys(struct battle *battle) {
  const double mx=FBW*0.5;
  const double my=FBH*0.5;
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) continue;
    double rx=rand()%FBW;
    double ry=rand()%FBH;
    player->anyx=rx*(1.0-player->skill)+mx*player->skill;
    player->anyy=ry*(1.0-player->skill)+my*player->skill;
  }
}

/* Each time the ball is served, reset CPU players' (throw).
 */
 
static void racketeering_reset_throws(struct battle *battle) {
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) continue;
    player->throw=(int)(player->skill*THROW_LIMIT)+(rand()%3)-1;
  }
}

/* Bias the ball's motion per (dx,dy), increase speed, and reverse dz.
 */
 
static void ball_bias_and_increase(struct battle *battle,struct ball *ball,double dx,double dy) {
  double ospeed=sqrt(ball->dx*ball->dx+ball->dy*ball->dy+ball->dz*ball->dz);
  ball->dz=-ball->dz;
  ball->dx+=(dx*WHACK_BIAS)/STRIKE_ZONE;
  ball->dy+=(dy*WHACK_BIAS)/STRIKE_ZONE;
  ball->dx+=(((rand()&0xffff)-0x8000)*20.0)/32768.0; // Add some X noise, because CPU players tend to hit X exactly.
  ball->dy-=((rand()&0xffff)*WHACK_BIAS)/65535.0; // Also give it a random upward push.
  double nspeed=sqrt(ball->dx*ball->dx+ball->dy*ball->dy+ball->dz*ball->dz)*BALL_SPEED_INCREASE;
  double scale=nspeed/ospeed;
  ball->dx*=scale;
  ball->dy*=scale;
  ball->dz*=scale;
}

/* Begin swing.
 * Don't call if (player->swing>0).
 */
 
static void racketeering_swing(struct battle *battle,struct player *player) {
  player->swing=SWING_TIME;
  player->blackout=1;
  
  if (player==BATTLE->serving) {
    bm_sound(RID_sound_tennis_serve);
    ball_align_to_player(battle,&BATTLE->ball,player);
    ball_random_serve(battle,&BATTLE->ball);
    BATTLE->volley=other_player(battle,BATTLE->serving);
    BATTLE->serving=0;
    racketeering_reset_anys(battle);
    racketeering_reset_throws(battle);
    
  } else if (BATTLE->serving) {
    bm_sound(RID_sound_swing_racket);
    
  } else {
    bm_sound(RID_sound_swing_racket);
    if (BATTLE->ball.dz>0.0) {
    } else {
      double dx=BATTLE->ball.x-player->x;
      double dy=BATTLE->ball.y-player->y;
      double dz=BATTLE->ball.z;
      dz*=ANTI_WHIFF_Z_SCALE; // Scale down the Z difference because it's very hard to judge.
      double d2=dx*dx+dy*dy+dz*dz;
      if (d2>STRIKE_ZONE*STRIKE_ZONE) {
        // Whiff!
      } else {
        double distance=sqrt(d2);
        ball_bias_and_increase(battle,&BATTLE->ball,dx,dy);
        BATTLE->volley=other_player(battle,BATTLE->volley);
        racketeering_reset_anys(battle);
      }
    }
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
    else input&=~EGG_BTN_SOUTH;
  }
  if (player->delay>0.0) {
    player->delay-=elapsed;
  }
  if (player->swing>0.0) {
    player->swing-=elapsed;
  } else {
    double dx=0.0,dy=0.0;
    switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
      case EGG_BTN_LEFT: dx=-1.0; break;
      case EGG_BTN_RIGHT: dx=1.0; break;
    }
    switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
      case EGG_BTN_UP: dy=-1.0; break;
      case EGG_BTN_DOWN: dy=1.0; break;
    }
    player->x+=dx*player->speed*elapsed;
    player->y+=dy*player->speed*elapsed;
    if (player->x<0.0) player->x=0.0; else if (player->x>FBW) player->x=FBW;
    if (player->y<0.0) player->y=0.0; else if (player->y>FBH) player->y=FBH;
    if ((input&EGG_BTN_SOUTH)&&(player->delay<=0.0)) {
      racketeering_swing(battle,player);
    }
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // Complete my swing.
  if (player->swing>0.0) {
    player->swing-=elapsed;
    return;
  }

  // My serve?
  if (BATTLE->serving==player) {
    if ((player->delay-=elapsed)<=0.0) {
      racketeering_swing(battle,player);
    }
    return;
  }
  
  /* Select a target position.
   * We'll determine two exact possibilities:
   *  - A: Orthographic projection of the ball's current position. The way an idiot would play.
   *  - B: Predicted position based on current velocity, and some eyeballing of gravity. More precise than a human could do.
   * When it's not my turn, use the screen's center instead of (B). Since we don't know what the other guy's stroke will do to the ball.
   */
  double ax,ay,bx,by;
  ax=BATTLE->ball.x;
  ay=BATTLE->ball.y;
  if ((player!=BATTLE->volley)||BATTLE->serving) {
    ax=bx=player->anyx;
    ay=by=player->anyy;
  } else {
    // First determine how far we are from the breach. That's either (z,dz) directly, or when moving away, add the time to reach the rear wall.
    double ballz=BATTLE->ball.z,balldz=BATTLE->ball.dz;
    if (balldz>0.0) {
      balldz=-balldz;
      ballz+=ZLIMIT-ballz;
    }
    double time_to_breach=-ballz/balldz;
    // (bx) is pretty straightforward: Extrapolate, mod, then reverse if negative. dx doesn't change, it just flips signs.
    bx=BATTLE->ball.x+BATTLE->ball.dx*time_to_breach;
    double bx0=bx;
    int eo=0;
    if (bx<0.0) bx=-bx;
    eo+=(int)(bx/FBW);
    bx=fmod(bx,FBW);
    if (eo&1) bx=FBW-bx;
    // (by) is much more interesting. The velocity changes over time and also reverses at each bounce.
    // A good estimate would be really tricky. Let me try cheesing it: Aim for half of current when it's at the rear wall, sliding toward current at the front wall.
    double reary=BATTLE->ball.y+(FBH-BATTLE->ball.y)*0.5;
    double fronty=BATTLE->ball.y;
    double nz=BATTLE->ball.z/ZLIMIT;
    by=reary*nz+fronty*(1.0-nz);
  }
  
  /* Interpolate between the two target candidates based on skill.
   */
  double targetx=ax*(1.0-player->skill)+bx*player->skill;
  double targety=ay*(1.0-player->skill)+by*player->skill;
  
  /* If we're past the throw limit, nudge target so as to miss every time.
   */
  if (player->throw<=0) {
    if (targetx<FBW*0.5) targetx+=20; else targetx-=20;
    if (targety<FBH*0.5) targety+=20; else targety-=20;
  }
  
  /* Approach the target.
   */
  if (player->x<targetx) {
    if ((player->x+=player->speed*elapsed)>targetx) player->x=targetx;
  } else {
    if ((player->x-=player->speed*elapsed)<targetx) player->x=targetx;
  }
  if (player->y<targety) {
    if ((player->y+=player->speed*elapsed)>targety) player->y=targety;
  } else {
    if ((player->y-=player->speed*elapsed)<targety) player->y=targety;
  }
  
  /* If the ball is near and moving toward the front, swing at it.
   * But only when it is actually my turn. I missed that at first and it's hilarious.
   */
  if ((BATTLE->volley==player)&&(BATTLE->ball.dz<0.0)) {
    const double inner_strike_zone=STRIKE_ZONE*0.5;
    double dx=BATTLE->ball.x-player->x;
    if ((dx>-inner_strike_zone)&&(dx<inner_strike_zone)) {
      double dy=BATTLE->ball.y-player->y;
      if ((dx>-inner_strike_zone)&&(dy<inner_strike_zone)) {
        double dz=BATTLE->ball.z*ANTI_WHIFF_Z_SCALE;
        if (dz<inner_strike_zone) {
          racketeering_swing(battle,player);
          player->throw--;
        }
      }
    }
  }
}

/* Update any player.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
}

/* Ball has crossed into negative Z.
 * Score a point and begin the next serve.
 */
 
static void racketeering_breach(struct battle *battle) {
  struct player *winner=other_player(battle,BATTLE->volley);
  winner->score++;
  if (winner->score>=2) {
    if (winner->who) {
      battle->outcome=-1;
    } else {
      battle->outcome=1;
    }
    BATTLE->cooldown=END_COOLDOWN;
    return;
  }
  BATTLE->serving=other_player(battle,BATTLE->volley);
  BATTLE->volley=0;
  BATTLE->serving->delay=CPU_SERVE_DELAY;
  BATTLE->serveto=0.0;
}

/* Update ball.
 */
 
static void ball_update(struct battle *battle,struct ball *ball,double elapsed) {

  if (BATTLE->serving) {
    ball_align_to_player(battle,ball,BATTLE->serving);
    return;
  }
  
  ball->dy+=BALL_GRAVITY*elapsed;

  ball->x+=ball->dx*elapsed;
  if (ball->x<0.0) {
    ball->x=0.0;
    if (ball->dx<0.0) {
      bm_sound(RID_sound_bounce);
      ball->dx=-ball->dx;
    }
  } else if (ball->x>FBW) {
    ball->x=FBW;
    if (ball->dx>0.0) {
      bm_sound(RID_sound_bounce);
      ball->dx=-ball->dx;
    }
  }
  #define AXIS(tag,limit,breach_negative) { \
    ball->tag+=ball->d##tag*elapsed; \
    if (ball->tag<0.0) { \
      if (breach_negative) { \
        racketeering_breach(battle); \
        return; \
      } else { \
        bm_sound(RID_sound_bounce); \
        ball->d##tag=-ball->d##tag; \
      } \
    } else if (ball->tag>limit) { \
      bm_sound(RID_sound_bounce); \
      ball->d##tag=-ball->d##tag; \
    } \
  }
  AXIS(x,FBW,0)
  AXIS(y,FBH,0)
  AXIS(z,ZLIMIT,1)
  #undef AXIS
}

/* Update.
 */
 
static void _racketeering_update(struct battle *battle,double elapsed) {

  // Done?
  if (battle->outcome>-2) return;
  
  // Players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  // Ball.
  ball_update(battle,&BATTLE->ball,elapsed);
  
  // Enforce serve timeout.
  if (BATTLE->serving) {
    if ((BATTLE->serveto+=elapsed)>=SERVE_TIMEOUT) {
      racketeering_swing(battle,BATTLE->serving);
    }
  }
}

/* Project point from model space onto framebuffer.
 */
 
static void racketeering_project_point(int *fbx,int *fby,struct battle *battle,double mx,double my,double mz) {
  // Shift (mx,my) such that (0,0) is the camera's center.
  const double halfw=FBW*0.5;
  const double halfh=FBH*0.5;
  mx-=halfw;
  my-=halfh;
  // Normalize (mz) such that 0 is the vanishing point (distant, way behind the back wall), and 1 is the camera.
  const double vanish=ZLIMIT;
  mz=(ZLIMIT-mz+vanish)/(ZLIMIT+vanish);
  // Multiply (mx,my) by (mz), translate back to the corner origin, and that's all.
  mx*=mz;
  my*=mz;
  *fbx=(int)(mx+halfw);
  *fby=(int)(my+halfh);
}

/* Render ball.
 */
 
static void ball_render(struct battle *battle,struct ball *ball) {
  double nz=ball->z/ZLIMIT;
  int size=(int)(nz*(NS_sys_tilesize>>1)+(1.0-nz)*NS_sys_tilesize);
  int x,y;
  graf_set_filter(&g.graf,1);
  
  // Cast a shadow on all five walls. Yes, I know, this is not how light works.
  #define SHADOW(sx,sy,sz) { \
    racketeering_project_point(&x,&y,battle,sx,sy,sz); \
    int bsize=size; if (sz>=ZLIMIT) bsize=NS_sys_tilesize>>1; \
    uint32_t bcolor; \
    double distance=(sx-ball->x)+(sy-ball->y)+(sz-ball->z); \
    if (distance<0.0) distance=-distance; \
    distance/=FBW; \
    bcolor=(int)(128.0*distance+16.0*(1.0-distance)); \
    if (bcolor<0) bcolor=0; else if (bcolor>255) bcolor=255; \
    bcolor=(bcolor<<24)|(bcolor<<16)|(bcolor<<8)|0xff; \
    graf_fancy(&g.graf,x,y,0x29,0,0,bsize,bcolor,0x808080c0); \
  }
  SHADOW(0.0,ball->y,ball->z)
  SHADOW(FBW,ball->y,ball->z)
  SHADOW(ball->x,0.0,ball->z)
  SHADOW(ball->x,FBH,ball->z)
  SHADOW(ball->x,ball->y,ZLIMIT)
  #undef SHADOW
  
  // And finally, the ball itself.
  uint32_t tint=0;
  if ((ball->z<BLINK_Z)&&(ball->dz<0.0)) {
    tint=0x00ff0080;
  }
  racketeering_project_point(&x,&y,battle,ball->x,ball->y,ball->z);
  graf_fancy(&g.graf,x,y,0x29,0,0,size,tint,0);
  graf_set_filter(&g.graf,0);
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=(int)player->y;
  uint8_t tileid=0x09;
  uint8_t alpha=0xff;
  if (BATTLE->serving) {
    if (BATTLE->serving!=player) alpha=0x40;
  } else if (BATTLE->volley) {
    if (BATTLE->volley!=player) alpha=0x40;
  }
  if (player->swing>0.0) tileid=0x19;
  graf_fancy(&g.graf,x,y,tileid,0,0,NS_sys_tilesize,0,(player->color&0xffffff00)|alpha);
}

/* Render.
 */
 
static void _racketeering_render(struct battle *battle) {

  /* Background.
   * The room must be symmetric, so we only project the upper-left corner, and infer the others.
   * Actually we shouldn't even need to project that; it's constant.
   * But this way ensures the room and ball project the same way, if I tweak the parameters or something.
   */
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  uint32_t near_color=0x404040ff,far_color=0x000000ff;
  int cornerx,cornery;
  racketeering_project_point(&cornerx,&cornery,battle,0.0,0.0,ZLIMIT);
  graf_line(&g.graf,0,0,near_color,cornerx,cornery,far_color);
  graf_line(&g.graf,FBW,0,near_color,FBW-cornerx,cornery,far_color);
  graf_line(&g.graf,0,FBH,near_color,cornerx,FBH-cornery,far_color);
  graf_line(&g.graf,FBW,FBH,near_color,FBW-cornerx,FBH-cornery,far_color);
  graf_line(&g.graf,cornerx,cornery,far_color,FBW-cornerx,cornery,far_color);
  graf_line(&g.graf,FBW-cornerx,cornery,far_color,FBW-cornerx,FBH-cornery,far_color);
  graf_line(&g.graf,FBW-cornerx,FBH-cornery,far_color,cornerx,FBH-cornery,far_color);
  graf_line(&g.graf,cornerx,FBH-cornery,far_color,cornerx,cornery,far_color);
  
  // Scoreboard.
  // Before sprites. It's meant to look fastened to the rear wall.
  graf_set_image(&g.graf,RID_image_battle_goblins);
  int lighty=55;
  int lightxv[3]={
    (FBW>>1)-NS_sys_tilesize,
    (FBW>>1),
    (FBW>>1)+NS_sys_tilesize,
  };
  int i=0;
  for (;i<3;i++) {
    int lightx=lightxv[i];
    uint32_t color=0x808080ff;
    if (i<BATTLE->playerv[0].score) color=BATTLE->playerv[0].color;
    else if (i>=3-BATTLE->playerv[1].score) color=BATTLE->playerv[1].color;
    graf_fancy(&g.graf,lightx,lighty,0x08,0,0,NS_sys_tilesize,0,color);
  }
  
  // Sprites. Ball goes on top if it's breached.
  if (BATTLE->ball.z<0.0) {
    player_render(battle,BATTLE->playerv+0);
    player_render(battle,BATTLE->playerv+1);
    ball_render(battle,&BATTLE->ball);
  } else {
    ball_render(battle,&BATTLE->ball);
    player_render(battle,BATTLE->playerv+0);
    player_render(battle,BATTLE->playerv+1);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_racketeering={
  .name="racketeering",
  .objlen=sizeof(struct battle_racketeering),
  .strix_name=52,
  .no_article=0,
  .no_contest=0,
  .no_timeout=1,
  .support_pvp=1,
  .support_cvc=1,
  .del=_racketeering_del,
  .init=_racketeering_init,
  .update=_racketeering_update,
  .render=_racketeering_render,
};

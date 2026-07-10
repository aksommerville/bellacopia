/* battle_curling.c
 */

#include "game/bellacopia.h"

#define MIDX 160
#define MIDY  45

#define THROWSTATE_ANGLE 1
#define THROWSTATE_SPEED 2
#define THROWSTATE_DONE 3

#define STONE_RADIUS 6.0
#define STONE_RADIUS_2 (STONE_RADIUS*STONE_RADIUS)
#define STONE_DISTANCE_2 (STONE_RADIUS*STONE_RADIUS*4.0)

#define COLLISION_PENALTY 0.750

/* Walls are oriented such that their left-hand side is open and right-hand side is blocked.
 * (If you're standing on (a) looking at (b)).
 */
static struct wall {
  double ax,ay,bx,by,len;
} wallv[]={
  {160.0,  8.0, 27.0,139.0},
  { 27.0,139.0, 63.0,175.0},
  { 63.0,175.0,160.0, 79.0},
  {160.0, 79.0,256.0,175.0},
  {256.0,175.0,292.0,139.0},
  {292.0,139.0,160.0,  8.0},
};
static const int wallc=sizeof(wallv)/sizeof(wallv[0]);

struct battle_curling {
  struct battle hdr;
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double x,y;
    
    // CPU control.
    double angleclock;
    double speedclock;
    
    // Throw parameters.
    double angle; // Radians. Centered is +pi/4. Range 0..pi/2
    double speed; // Normalized.
    int throwstate; // THROWSTATE_*
    double selectt; // Positive radians.
    double selectdt; // rad/sec. Speed of the input selection's cycle, main difficulty driver.
    double dx,dy; // Unit vector equivalent to (angle) once committed.
    
    // Stone.
    double stonex,stoney;
    double stonedx,stonedy;
    double stone_decel; // Positive px/s**2. Constant?
    int stopped; // Nonzero when we're done moving. May reacquire velocity due to collisions tho.
    
  } playerv[2];
};

#define BATTLE ((struct battle_curling*)battle)

/* Delete.
 */
 
static void _curling_del(struct battle *battle) {
}

/* Begin throwstate.
 */
 
static void curling_begin_throwstate(struct battle *battle,struct player *player,int throwstate) {
  player->throwstate=throwstate;
  switch (throwstate) {
    case THROWSTATE_ANGLE: {
        // Initial position is random at each transition, so you can't just learn one input rhythm.
        player->selectt=((rand()&0xffff)*M_PI*2.0)/65535.0;
      } break;
    case THROWSTATE_SPEED: {
        // Initial position is random at each transition, so you can't just learn one input rhythm.
        player->selectt=((rand()&0xffff)*M_PI*2.0)/65535.0;
      } break;
    case THROWSTATE_DONE: {
        // Prepare the stone.
        player->stonex=player->x+player->dx*10.0;
        player->stoney=player->y+player->dy*10.0;
        player->stonedx=player->dx*player->speed;
        player->stonedy=player->dy*player->speed;
      } break;
  }
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {

  // Calculate wall lengths. We could do this at compile time but whatever.
  struct wall *wall=wallv;
  int i=wallc;
  for (;i-->0;wall++) {
    double dx=wall->bx-wall->ax;
    double dy=wall->by-wall->ay;
    wall->len=sqrt(dx*dx+dy*dy);
  }

  const double legdist=100.0;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=MIDX-legdist;
    player->y=MIDY+legdist;
  } else { // Right.
    player->who=1;
    player->x=MIDX+legdist;
    player->y=MIDY+legdist;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    // Trying a pure drunk-walk CPU, it just picks a random interval and presses the button.
    player->angleclock=1.000+((rand()&0xffff)*1.000)/65535.0;
    player->speedclock=1.000+((rand()&0xffff)*1.000)/65535.0;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xe0f6f1ff;
        player->tileid=0x09;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x17;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x19;
      } break;
  }
  player->stone_decel=50.0;
  player->selectdt=4.0*player->skill+10.0*(1.0-player->skill);
  curling_begin_throwstate(battle,player,THROWSTATE_ANGLE);
}

/* New.
 */
 
static int _curling_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  // And I keep getting this backward: At HIGH skill, the player is favored to win. High bias means high skill for the CPU and low skill for Dot.
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playclock=20.0;
  return 0;
}

/* Throw parameters from circular input.
 */
 
static double curling_angle_from_input(double src,int who) {
  double t=M_PI*0.125+((sin(src)+1.0)*M_PI*0.250)/2.0;
  if (who) {
    t=M_PI*2.0-t;
  }
  return t;
}

static double curling_normal_speed_from_input(double src) {
  double n=sin(src);
  if (n<0.0) n=-n;
  return 1.0-n;
}

static double curling_speed_from_input(double src) {
  return 90.0+80.0*curling_normal_speed_from_input(src);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    switch (player->throwstate) {
      case THROWSTATE_ANGLE: {
          bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
          player->angle=curling_angle_from_input(player->selectt,player->who);
          player->dx=sin(player->angle);
          player->dy=-cos(player->angle);
          curling_begin_throwstate(battle,player,THROWSTATE_SPEED);
        } break;
      case THROWSTATE_SPEED: {
          bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
          player->speed=curling_speed_from_input(player->selectt);
          curling_begin_throwstate(battle,player,THROWSTATE_DONE);
        } break;
    }
  } else if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) { // Allow takesie-backsies before the stone leaves.
    switch (player->throwstate) {
      case THROWSTATE_SPEED: {
          bm_sound_pan(RID_sound_uicancel,player->who?PLAYER_PAN:-PLAYER_PAN);
          curling_begin_throwstate(battle,player,THROWSTATE_ANGLE);
        } break;
    }
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  switch (player->throwstate) {
    case THROWSTATE_ANGLE: {
        if (player->angleclock>0.0) {
          player->angleclock-=elapsed;
        } else {
          bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
          player->angle=curling_angle_from_input(player->selectt,player->who);
          player->dx=sin(player->angle);
          player->dy=-cos(player->angle);
          curling_begin_throwstate(battle,player,THROWSTATE_SPEED);
        }
      } break;
    case THROWSTATE_SPEED: {
        if (player->speedclock>0.0) {
          player->speedclock-=elapsed;
        } else {
          bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
          player->speed=curling_speed_from_input(player->selectt);
          curling_begin_throwstate(battle,player,THROWSTATE_DONE);
        }
      } break;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  
  if ((player->selectt+=player->selectdt*elapsed)>=M_PI*2.0) {
    player->selectt-=M_PI*2.0;
  }
  
  // Move stone.
  if (player->throwstate==THROWSTATE_DONE) {
  
    // Decay velocity. Or if it drops below 1 m/s, call it kaput.
    double d2=player->stonedx*player->stonedx+player->stonedy*player->stonedy;
    if (d2<1.0) {
      player->stonedx=player->stonedy=0.0;
      player->stopped=1;
    } else {
      player->stopped=0; // We can unstop, if the other stone hits us after stopping. No worries. We are the authority.
      double velocity=sqrt(d2);
      double nvel=velocity-player->stone_decel*elapsed;
      if (nvel<0.0) nvel=0.0;
      double adjust=nvel/velocity;
      player->stonedx*=adjust;
      player->stonedy*=adjust;
  
      player->stonex+=player->stonedx*elapsed;
      player->stoney+=player->stonedy*elapsed;
      
      // Collide with walls?
      const struct wall *wall=wallv;
      int i=wallc;
      for (;i-->0;wall++) {
        double rej=((player->stonex-wall->ax)*(wall->by-wall->ay)-(player->stoney-wall->ay)*(wall->bx-wall->ax))/wall->len;
        if (rej>STONE_RADIUS) continue; // We're safely on its open side.
        double proj=((player->stonex-wall->ax)*(wall->bx-wall->ax)+(player->stoney-wall->ay)*(wall->by-wall->ay))/wall->len;
        if (proj<0.0) continue; // Out of range.
        if (proj>wall->len) continue; // Out of range.
        // A wall collision exists.
        // Force the stone to a safe distance from the wall.
        double wallnx=(wall->bx-wall->ax)/wall->len;
        double wallny=(wall->by-wall->ay)/wall->len;
        double awaynx=wallny;
        double awayny=-wallnx;
        double safex=wall->ax+wallnx*proj+awaynx*STONE_RADIUS;
        double safey=wall->ay+wallny*proj+awayny*STONE_RADIUS;
        player->stonex=safex;
        player->stoney=safey;
        // Reflect velocity over (awaynx,awayny).
        double pn=(player->stonedx*awaynx)+(player->stonedy*awayny);
        double pnx=awaynx*pn*2.0;
        double pny=awayny*pn*2.0;
        player->stonedx-=pnx;
        player->stonedy-=pny;
        // Lose a huge amount of velocity per collision.
        player->stonedx*=COLLISION_PENALTY;
        player->stonedy*=COLLISION_PENALTY;
        break;
      }
      
      // Collide with other stone?
      struct player *other=BATTLE->playerv;
      if (!player->who) other+=1;
      double sdx=other->stonex-player->stonex;
      double sdy=other->stoney-player->stoney;
      double sd2=sdx*sdx+sdy*sdy;
      if (sd2<STONE_DISTANCE_2) {
        // Calculate penetration and escape the collision balanced.
        double sd=sqrt(sd2);
        double pen=STONE_RADIUS*2.0-sd;
        double ndx=sdx/sd;
        double ndy=sdy/sd;
        other->stonex+=ndx*pen*0.5;
        other->stoney+=ndy*pen*0.5;
        player->stonex-=ndx*pen*0.5;
        player->stoney-=ndy*pen*0.5;
        
        // Project each stone's velocity on to the collision normal.
        double spa=player->stonedx*ndx+player->stonedy*ndy;
        double spb=other->stonedx*-ndx+other->stonedy*-ndy;
        double addvel=spa+spb;
        if (addvel>0.0) {
          other->stonedx+=addvel*ndx;
          other->stonedy+=addvel*ndy;
          player->stonedx-=addvel*ndx;
          player->stonedy-=addvel*ndy;
        }
      }
    }
  }
}

/* Update.
 */
 
static void _curling_update(struct battle *battle,double elapsed) {
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
  
  // When both stones are stopped, the nearest to the target wins.
  if (l->stopped&&r->stopped) {
    double ldx=l->stonex-MIDX;
    double ldy=l->stoney-MIDY;
    double ld2=ldx*ldx+ldy*ldy;
    double rdx=r->stonex-MIDX;
    double rdy=r->stoney-MIDY;
    double rd2=rdx*rdx+rdy*rdy;
    if (ld2<=rd2) { // Give ties to the left player, usually Dot, they're very unusual.
      battle->outcome=1;
    } else {
      battle->outcome=-1;
    }
    return;
  }
  
  // (playclock) expires, anyone that hasn't thrown yet loses.
  if ((BATTLE->playclock-=elapsed)<=0.0) {
    if (l->throwstate==THROWSTATE_ANGLE) {
      if (r->throwstate==THROWSTATE_ANGLE) battle->outcome=0;
      else battle->outcome=-1;
      return;
    } else if (r->throwstate==THROWSTATE_ANGLE) {
      battle->outcome=1;
      return;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int px=(int)player->x;
  int py=(int)player->y;
  uint8_t pxform=0;
  uint8_t tileid=player->tileid;
  if (player->who) {
    pxform=EGG_XFORM_XREV;
  }
  if (player->throwstate==THROWSTATE_DONE) tileid+=1;
  graf_set_image(&g.graf,RID_image_icepalace_sprites);
  graf_fancy(&g.graf,px,py,tileid,pxform,0,NS_sys_tilesize,0,player->color);
  
  if (player->throwstate==THROWSTATE_DONE) {
    graf_fancy(&g.graf,player->stonex,player->stoney,0x08,0,0,NS_sys_tilesize,0,player->color);
  }
  
  // Draw a guideline if collecting angle or speed.
  switch (player->throwstate) {
    case THROWSTATE_ANGLE: {
        double t=curling_angle_from_input(player->selectt,player->who);
        const double r1=10.0;
        const double r2=30.0;
        double sint=sin(t);
        double cost=cos(t);
        graf_set_input(&g.graf,0);
        graf_line(&g.graf,
          (int)(player->x+sint*r1),
          (int)(player->y-cost*r1),
          0xff0000ff,
          (int)(player->x+sint*r2),
          (int)(player->y-cost*r2),
          0xff0000ff
        );
      } break;
    case THROWSTATE_SPEED: {
        double n=curling_normal_speed_from_input(player->selectt);
        const double r1=10.0;
        double r2=r1+n*20.0;
        graf_set_input(&g.graf,0);
        graf_line(&g.graf,
          (int)(player->x+player->dx*r1),
          (int)(player->y+player->dy*r1),
          0xff0000ff,
          (int)(player->x+player->dx*r2),
          (int)(player->y+player->dy*r2),
          0xff0000ff
        );
      } break;
  }
}

/* Render.
 */
 
static void _curling_render(struct battle *battle) {
  graf_set_image(&g.graf,RID_image_curling);
  graf_decal(&g.graf,0,0,0,0,FBW,FBH);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_curling={
  .name="curling",
  .objlen=sizeof(struct battle_curling),
  .id=NS_battle_curling,
  .strix_name=228,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_a,
  .del=_curling_del,
  .init=_curling_init,
  .update=_curling_update,
  .render=_curling_render,
};

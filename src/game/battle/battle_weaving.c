/* battle_weaving.c
 */

#include "game/bellacopia.h"

#define WARPCLO 5
#define WARPCHI 7
#define SPEEDLO  40.0
#define SPEEDHI  40.0
#define DTLO 5.000
#define DTHI 5.000
#define OVERSHOOTLO 15.0 /* CPU aims for a triangle wave. Its points will be so far from the foci. */
#define OVERSHOOTHI 30.0
#define TMIN 0.100 /* Smallest allowable angle in 0..1/2. We must keep them off the vertical. */
#define SAMPLE_LIMIT 256 /* 249 at the limit where you ride an edge all the way. 100-ish in a typical run. */
#define SAMPLE_PERIOD 0.100

struct battle_weaving {
  struct battle hdr;
  double countdown;
  double termclock;
  int termp;
  int term; // nonzero if we're wrapping up (outcome would not be set yet)
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    int warpc; // Determined at init, per skill.
    int warpxv[WARPCHI];
    int ratev[WARPCHI]; // Populated progressively after term.
    int throwv[WARPCHI]; // For CPU, nonzero to deliberately crash at this warp.
    double speed; // ''
    double dt; // ''
    double needlex; // Framebuffer pixels.
    double needley; // Relative to section.
    double needlet;
    double needledx,needledy; // Derived from (speed,needlet)
    double tmin,tmax; // Const, depends on direction.
    int indt; // Controller sets.
    int term; // Set when (needlex) clamps, ie we reached the end.
    double runclock;
    
    struct sample {
      double x,y;
      int ix,iy;
    } samplev[SAMPLE_LIMIT];
    int samplec;
    double sampleclock;
    
    int warpp;
    double overshootlo,overshoothi;
    double dstx,dsty;
    int nextdir;
    
    int score;
  } playerv[2];
};

#define BATTLE ((struct battle_weaving*)battle)

/* Delete.
 */
 
static void _weaving_del(struct battle *battle) {
}

/* Recalculate (needledx,needledy).
 */
 
static void player_recalc_delta(struct player *player) {
  player->needledx=sin(player->needlet)*player->speed;
  player->needledy=-cos(player->needlet)*player->speed;
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->needlex=10.0;
    player->needlet=M_PI*0.5;
    player->tmin=M_PI*TMIN;
    player->tmax=M_PI*(1.0-TMIN);
    player->samplev[0]=(struct sample){0.0,FBH*0.25,0,FBH>>2};
    player->samplev[1]=(struct sample){player->needlex,FBH*0.25,player->needlex,FBH>>2};
    player->samplec=2;
  } else { // Right.
    player->who=1;
    player->needlex=FBW-10.0;
    player->needlet=M_PI*-0.5;
    player->tmin=M_PI*-(1.0-TMIN);
    player->tmax=M_PI*-TMIN;
    player->samplev[0]=(struct sample){FBW,FBH*0.25,FBW,FBH>>2};
    player->samplev[1]=(struct sample){player->needlex,FBH*0.25,player->needlex,FBH>>2};
    player->samplec=2;
  }
  player->needley=FBH>>2;
  
  player->warpc=(int)(WARPCLO*player->skill+WARPCHI*(1.0-player->skill));
  if (player->warpc<WARPCLO) player->warpc=WARPCLO;
  else if (player->warpc>WARPCHI) player->warpc=WARPCHI;
  player->speed=SPEEDLO*(1.0-player->skill)+SPEEDHI*player->skill;
  player->dt=DTLO*(1.0-player->skill)+DTHI*player->skill;
  player_recalc_delta(player);
  
  int xoffset=player->who?-10:10;
  int i=0; for (;i<player->warpc;i++) {
    player->warpxv[i]=xoffset+((i+1)*FBW)/(player->warpc+1);
  }
  if (player->who) { // Right player, (warpxv) should run right-to-left.
    int ai=player->warpc>>1;
    int bi=ai;
    if (!(player->warpc&1)) ai--;
    for (;(ai>=0)&&(bi<player->warpc);ai--,bi++) {
      int tmp=player->warpxv[ai];
      player->warpxv[ai]=player->warpxv[bi];
      player->warpxv[bi]=tmp;
    }
  }
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->overshoothi=OVERSHOOTLO*player->skill+OVERSHOOTHI*(1.0-player->skill);
    player->overshootlo=player->overshoothi*0.5;
    if (player->who) player->dstx=FBW;
    player->nextdir=(rand()&1);
    player->speed*=0.900; // CPU penalty.
    player_recalc_delta(player);
    
    // Hit so many warps, depending on skill.
    int throwc;
         if (player->skill>=0.750) throwc=0;
    else if (player->skill>=0.480) throwc=1; // The usual case, and also against Princess.
    else if (player->skill>=0.220) throwc=2; // Princess and goodluck.
    else throwc=3;
    if (throwc>=player->warpc) throwc=player->warpc-1; // Not possible but maybe I change constants in the future.
    int candidatev[WARPCHI];
    int i=player->warpc;
    while (i-->1) candidatev[i-1]=i; // sic 1 not zero: Never throw the first warp.
    int candidatec=player->warpc-1;
    while (throwc-->0) {
      int candidatep=rand()%candidatec;
      player->throwv[candidatev[candidatep]]=1;
      candidatec--;
      memmove(candidatev+candidatep,candidatev+candidatep+1,sizeof(int)*(candidatec-candidatep));
    }
  
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x200810ff;
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
 
static int _weaving_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->countdown=3.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  // LEFT,RIGHT are preferred for turning, with RIGHT being clockwise as usual.
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indt=-1; break;
    case EGG_BTN_RIGHT: player->indt=1; break;
    default: player->indt=0; break;
  }
  // They may also use UP,DOWN, since we're limited to half a circle. Direction depends on which way we're going.
  // (who) nonzero means we're going leftward.
  if (!player->indt) switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: {
        if (player->who) player->indt=1;
        else player->indt=-1;
      } break;
    case EGG_BTN_DOWN: {
        if (player->who) player->indt=-1;
        else player->indt=1;
      } break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* At the initial countdown, wait to the last second, then turn.
   * Just to look lively.
   */
  if (BATTLE->countdown>1.0) return;

  /* Do we need to prepare a new destination point?
   */
  int exceedx;
  if (player->who) {
    exceedx=player->needlex<=player->dstx;
  } else {
    exceedx=player->needlex>=player->dstx;
  }
  if (exceedx) {
    if (player->warpp>=player->warpc) { // To the goal!
      if (player->who) {
        player->dstx=0.0;
      } else {
        player->dstx=FBW;
      }
      player->dsty=FBH*0.250;
    } else { // To the next warp.
      int throw=player->throwv[player->warpp];
      player->dstx=player->warpxv[player->warpp++];
      double offy=(rand()&0xffff)/65535.0;
      offy=player->overshootlo*(1.0-offy)+player->overshoothi*offy;
      if (player->nextdir) {
        player->dsty=FBH*0.250+offy;
        player->nextdir=0;
      } else {
        player->dsty=FBH*0.250-offy;
        player->nextdir=1;
      }
      if (throw) player->dsty=FBH*0.250+((rand()&0xffff)*8.0/65535.0)-4.0;
    }
  }
  
  /* Steer toward the destination.
   */
  double dstt=atan2(player->dstx-player->needlex,player->needley-player->dsty);
  double dt=dstt-player->needlet;
  if (dt<=-M_PI) dt+=M_PI*2.0;
  else if (dt>=M_PI) dt-=M_PI*2.0;
  if (dt<-0.001) player->indt=-1;
  else if (dt>0.001) player->indt=1;
  else player->indt=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Rotate?
  if (player->indt) {
    player->needlet+=player->dt*player->indt*elapsed;
    if (player->needlet<player->tmin) player->needlet=player->tmin;
    else if (player->needlet>player->tmax) player->needlet=player->tmax;
    player_recalc_delta(player);
  }

  // Only rotation if the countdown is running.
  if (BATTLE->countdown>0.0) return;
  player->runclock+=elapsed;

  // Advance.
  player->needlex+=player->needledx*elapsed;
  player->needley+=player->needledy*elapsed;
  if (player->needlex<5.0) {
    player->needlex=5.0;
    player->term=1;
    return;
  } else if (player->needlex>315.0) {
    player->needlex=315.0;
    player->term=1;
    return;
  }
  if (player->needley<10.0) player->needley=10.0;
  else if (player->needley>80.0) player->needley=80.0;
  
  // Drop a sample point at regular intervals.
  if ((player->sampleclock-=elapsed)<=0.0) {
    player->sampleclock+=SAMPLE_PERIOD;
    if (player->samplec>=SAMPLE_LIMIT) {
      player->sampleclock=999.999;
    } else {
      struct sample *sample=player->samplev+player->samplec++;
      sample->x=player->needlex;
      sample->y=player->needley;
      sample->ix=(int)sample->x;
      sample->iy=(int)sample->y;
    }
  }
}

/* Examine my thread against one of my warps and update my score accordingly.
 * Return nonzero if (warpp) is a real point, zero if OOB.
 */
 
static int player_score_warp(struct battle *battle,struct player *player,int warpp) {
  if ((warpp<0)||(warpp>=player->warpc)) return 0;
  const double radius=6.0;
  const double radius2=radius*radius;
  
  double wx=player->warpxv[warpp];
  double wy=FBH*0.25;
   
  /* First, find the sample point closest to the first warp, and that tells us the phase.
   * (ie even-up or even-down, which side do we want to be on).
   */
  int sunnyside=0;
  double wx0=player->warpxv[0];
  const struct sample *sample=player->samplev;
  int i=player->samplec;
  if (player->who) {
    for (;i-->0;sample++) {
      if (sample->x<=wx0) {
        if (sample->y<wy) sunnyside=-1;
        else sunnyside=1;
        break;
      }
    }
  } else {
    for (;i-->0;sample++) {
      if (sample->x>=wx0) {
        if (sample->y<wy) sunnyside=-1;
        else sunnyside=1;
        break;
      }
    }
  }
  if (warpp&1) sunnyside=-sunnyside;
  
  /* Find all sample points within (radius) of (wx).
   * If any is on the moony side or too close, it's a miss.
   */
  int good=1;
  double xa=wx-radius;
  double xz=wx+radius;
  for (sample=player->samplev,i=player->samplec;i-->0;sample++) {
    if (player->who) {
      if (sample->x<xa) break;
      if (sample->x>xz) continue;
    } else {
      if (sample->x<xa) continue;
      if (sample->x>xz) break;
    }
    if (sunnyside<0) {
      if (sample->y>=wy) {
        good=0;
        break;
      }
    } else {
      if (sample->y<=wy) {
        good=0;
        break;
      }
    }
    double dx=sample->x-wx;
    double dy=sample->y-wy;
    double d2=dx*dx+dy*dy;
    if (d2<radius2) {
      good=0;
      break;
    }
  }
  
  /* Record it.
   */
  if (good) {
    player->score++;
    player->ratev[warpp]=1;
  } else {
    player->ratev[warpp]=-1;
  }
  return 1;
}

/* Update.
 */
 
static void _weaving_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if (BATTLE->countdown>0.0) {
    BATTLE->countdown-=elapsed;
  }
  
  /* After the players finish, we animate the tallying.
   */
  if (BATTLE->term) {
    if ((BATTLE->termclock-=elapsed)>0.0) return;
    BATTLE->termclock+=0.500;
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    int lok=player_score_warp(battle,l,BATTLE->termp);
    int rok=player_score_warp(battle,r,BATTLE->termp);
    if (lok||rok) {
      bm_sound(RID_sound_uimotion);
      BATTLE->termp++;
    } else {
    
      // One last correction: If one player has fewer warps, pretend the missing ones weft successfully for scoring purposes.
      if (l->warpc>r->warpc) r->score+=l->warpc-r->warpc;
      else if (l->warpc<r->warpc) l->score+=r->warpc-l->warpc;
    
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else if (l->runclock<r->runclock) battle->outcome=1;
      else if (l->runclock>r->runclock) battle->outcome=-1;
      else battle->outcome=0;
    }
    return;
  }
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->term) continue;
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Game enters termination stage when they both terminate.
   * It has to happen eventually, because they're constrained to forward motion.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->term&&r->term) {
    BATTLE->term=1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player,int top) {

  graf_set_image(&g.graf,RID_image_battle_underground);
  int y=top+(FBH>>2);
  int i=player->warpc;
  int rates_present=0;
  while (i-->0) {
    graf_tile(&g.graf,player->warpxv[i],y,0x01,0);
    if (player->ratev[i]) rates_present=1;
  }
  
  if (player->samplec>=2) {
    graf_set_input(&g.graf,0);
    const struct sample *sample=player->samplev;
    graf_line_strip_begin(&g.graf,sample->ix,top+sample->iy,player->color);
    sample++;
    for (i=player->samplec-1;i-->0;sample++) {
      graf_line_strip_more(&g.graf,sample->ix,top+sample->iy,player->color);
    }
  }

  graf_set_image(&g.graf,RID_image_battle_underground);
  int x=(int)player->needlex;
  y=top+(int)player->needley;
  uint8_t rot=(int8_t)((player->needlet*128.0)/M_PI);
  graf_set_filter(&g.graf,1);
  graf_fancy(&g.graf,x,y,0x00,0,rot,NS_sys_tilesize,0,0x808080ff);
  graf_set_filter(&g.graf,0);
  
  // Show rates on top if we have any.
  if (rates_present) {
    y=top+(FBH>>2)-8;
    for (i=player->warpc;i-->0;) {
      if (player->ratev[i]) {
        graf_tile(&g.graf,player->warpxv[i],y,(player->ratev[i]>0)?0x02:0x03,0);
      }
    }
  }
  
  // If terminated, show my runclock.
  if (player->term) {
    x=(int)player->needlex;
    y=top+(int)player->needley-6;
    if (y<5) y=5; // Left player saturated high, text would go offscreen.
    if (!player->who) x-=8*6;
    int ms=(int)(player->runclock*1000.0);
    if (ms<0) ms=0;
    int sec=ms/1000; ms%=1000;
    if (sec>99) {
      sec=99;
      ms=999;
    }
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_tile(&g.graf,x,y,'0'+sec/10,0); x+=8;
    graf_tile(&g.graf,x,y,'0'+sec%10,0); x+=8;
    graf_tile(&g.graf,x,y,'.',0); x+=8;
    graf_tile(&g.graf,x,y,'0'+ms/100,0); x+=8;
    graf_tile(&g.graf,x,y,'0'+(ms/10)%10,0); x+=8;
    graf_tile(&g.graf,x,y,'0'+ms%10,0);
  }
}

/* Render.
 */
 
static void _weaving_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  player_render(battle,BATTLE->playerv+0,0);
  player_render(battle,BATTLE->playerv+1,FBH>>1);
  
  if (BATTLE->countdown>0.0) {
    int s=(int)(BATTLE->countdown+0.999);
    if (s<1) s=1; else if (s>9) s=9;
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0x10080480);
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_tile(&g.graf,FBW>>1,FBH>>1,'0'+s,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_weaving={
  .name="weaving",
  .objlen=sizeof(struct battle_weaving),
  .id=NS_battle_weaving,
  .strix_name=263,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_horz, // Maybe we should declare 'dpad'? Verts work too and are arguably more comprehensible.
  .del=_weaving_del,
  .init=_weaving_init,
  .update=_weaving_update,
  .render=_weaving_render,
};

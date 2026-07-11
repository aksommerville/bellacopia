/* battle_frogeating.c
 */

#include "game/bellacopia.h"

#define POND_RADIUS 62.0 /* The pond is a perfect circle, centered in the framebuffer. */
#define FROG_RADIUS 7.0
#define FROG_RADIUS_2 (FROG_RADIUS*FROG_RADIUS)
#define FROG_DIAMETER_2 (FROG_RADIUS*FROG_RADIUS*4.0)
#define FROG_LIMIT 9 /* Should be odd. */
#define COLLISION_PENALTY 0.750 /* Reduce force globally on all collisions. This is the only way the system's energy is lost. */

struct battle_frogeating {
  struct battle hdr;
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // Points to an arm we don't use. Main face is 2x2 at +1. Back arm is +0x10.
    int blackout;
    double padx,pady; // Paddle center, in pond pixels.
    double padrad,padrad2; // Paddle effective radius and its square. Constant.
    double padaccel; // px/s**2, maximum acceleration of frogs by paddling.
    double gobrad,gobrad2; // Radius from (padx,pady) where a frog is gobble-eligible.
    int pvingobble;
    int score;
    struct frog *target; // Optional. Nearest present frog, if in gobble range. So we can highlight it.
    double gobbleclock;
    double arm; // 0..1 = in..out. Animates for decoration.
    
    // Controller sets:
    int inpaddle; // -1,0,1 = inward,none,outward. From the dpad horz.
    int ingobble; // Button, to grab a frog.
    
    // CPU:
    double holdclock; // Hold (inpaddle) constant after a change, counts down.
    double holdtimelo,holdtimehi; // Still time after a direction change.
    double postholdlo,postholdhi; // Still time after gobbling.
    double paddlerange2;
    struct frog *lockon; // Artificial delay after a frog comes into range before we gobble it.
    double lockonclock;
    double lockontimelo,lockontimehi;
    
  } playerv[2];
  
  /* Note that a lily pad with no frog on it is still a "frog" for all but culinary purposes.
   */
  struct frog {
    double x,y; // Framebuffer pixels, but translated so (0,0) is the center of the pond.
    double dx,dy; // px/s
    double blinkclock;
    int blink;
    int present; // Nonzero if a frog exists, otherwise it's an empty lily pad.
    uint8_t xform;
  } frogv[FROG_LIMIT];
  int frogc;
  double decayclock;
};

#define BATTLE ((struct battle_frogeating*)battle)

/* Delete.
 */
 
static void _frogeating_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->padx=-POND_RADIUS;
    player->pady=0.0;
  } else { // Right.
    player->who=1;
    player->padx=POND_RADIUS;
    player->pady=0.0;
  }
  
  const double padradlo=POND_RADIUS*1.500;
  const double padradhi=POND_RADIUS*2.000;
  const double gobradlo=POND_RADIUS*0.400;
  const double gobradhi=POND_RADIUS*0.600;
  const double accello=33.0;
  const double accelhi=40.0;
  player->padrad=player->skill*padradhi+(1.0-player->skill)*padradlo;
  player->gobrad=player->skill*gobradhi+(1.0-player->skill)*gobradlo;
  player->padrad2=player->padrad*player->padrad;
  player->gobrad2=player->gobrad*player->gobrad;
  player->padaccel=player->skill*accelhi+(1.0-player->skill)*accello;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->holdtimehi=  0.500*player->skill+0.900*(1.0-player->skill); // Minimum time between direction changes.
    player->lockontimehi=0.200*player->skill+0.500*(1.0-player->skill); // Delay before gobbling.
    player->postholdhi=  0.900*player->skill+1.900*(1.0-player->skill); // Delay after gobbling.
    player->holdtimelo=player->holdtimehi*0.5;
    double paddlerange=POND_RADIUS;
    player->paddlerange2=paddlerange*paddlerange;
    player->lockontimelo=player->lockontimehi*0.5;
    player->postholdlo=player->postholdhi*0.5;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xe0e0e0ff;
        player->tileid=0x44;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x04;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x24;
      } break;
  }
}

/* Spawn a new frog.
 * May fail if we randomly fail to locate a valid position.
 */
 
static struct frog *frogeating_spawn_frog(struct battle *battle) {
  if (BATTLE->frogc>=FROG_LIMIT) return 0;
  
  /* Select a uniformly random position in the pond.
   * Then iterate existing frogs and if they overlap, try again.
   * The sqrt() here scales magnitude to create uniform distribution, how neat!
   */
  double x,y;
  int panic=40;
  for (;;) {
    if (panic--<0) return 0;
    double t=((rand()&0xffff)*M_PI*2.0)/65535.0;
    double m=sqrt((rand()&0xffff)/65535.0)*(POND_RADIUS-FROG_RADIUS);
    x=sin(t)*m;
    y=cos(t)*m;
    int lebensraum=1;
    const struct frog *other=BATTLE->frogv;
    int i=BATTLE->frogc;
    for (;i-->0;other++) {
      double dx=x-other->x;
      double dy=y-other->y;
      double d2=dx*dx+dy*dy;
      if (d2<FROG_DIAMETER_2) {
        lebensraum=0;
        break;
      }
    }
    if (lebensraum) break;
  }
  
  /* OK, make a frog.
   */
  const double velmax=20.0; // per axis
  struct frog *frog=BATTLE->frogv+BATTLE->frogc++;
  frog->x=x;
  frog->y=y;
  frog->dx=((rand()&0xffff)*velmax*2.0)/65535.0-velmax;
  frog->dy=((rand()&0xffff)*velmax*2.0)/65535.0-velmax;
  frog->blinkclock=1.000+((rand()&0xffff)*3.000)/65535.0;
  frog->blink=0;
  frog->present=1;
  frog->xform=(rand()&1)?EGG_XFORM_XREV:0; // decorative
  return frog;
}

/* New.
 */
 
static int _frogeating_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  /* Generate a random set of frogs.
   */
  int panic=FROG_LIMIT*6;
  while ((panic-->0)&&(BATTLE->frogc<FROG_LIMIT)) frogeating_spawn_frog(battle);
  if (BATTLE->frogc<1) return -1;
  
  /* Now ensure that (frogv[].dx) have signs proportionate to our bias.
   * Low bias, more should point left. High bias, more should point right.
   * Flip signs to make it so.
   */
  int biaslo=0x30,biashi=0xd0; // Outside these ranges, they all point one way or the other.
  int rightc=((battle->args.bias-biaslo)*BATTLE->frogc)/(biashi-biaslo);
  if (rightc<0) rightc=0; else if (rightc>BATTLE->frogc) rightc=BATTLE->frogc;
  struct frog *frog=BATTLE->frogv;
  int i=BATTLE->frogc;
  int have_rightc=0;
  for (;i-->0;frog++) if (frog->dx>=0.0) have_rightc++;
  int change=rightc-have_rightc;
  if (change) {
    for (i=BATTLE->frogc,frog=BATTLE->frogv;i-->0;frog++) {
      if (change<0) {
        if (frog->dx>0.0) {
          frog->dx=-frog->dx;
          change++;
        }
      } else if (change>0) {
        if (frog->dx<0.0) {
          frog->dx=-frog->dx;
          change--;
        }
      }
    }
  }
  
  BATTLE->playclock=15.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  int indx=0;
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: indx=-1; break;
    case EGG_BTN_RIGHT: indx=1; break;
  }
  if (player->who) indx=-indx;
  player->inpaddle=indx;
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->ingobble=(input&EGG_BTN_SOUTH)?1:0;
  }
}

/* Check line of sight to a frog.
 * Returns -2 for Hard Yes: This frog is very close.
 * -1 for Soft Yes: An unobstructed line of sight exists.
 * Caller checks (present), otherwise we assume you're also interested in empty lily pads.
 */
 
static int frogeating_check_line_of_sight(struct battle *battle,struct player *player,struct frog *frog) {
  double dx=frog->x-player->padx;
  double dy=frog->y-player->pady;
  double d2=dx*dx+dy*dy;
  if (d2<50.0) return -2; // Can't let it be zero, but also anything below 49 is very improbable. Just stop looking and paddle in.
  const double barrel2=FROG_RADIUS*FROG_RADIUS*4.0;
  const struct frog *other=BATTLE->frogv;
  int i=BATTLE->frogc;
  for (;i-->0;other++) {
    if (other==frog) continue;
    //if (other->present) continue; // Just an optimization, but ignore present others since they are also cool to paddle in.
    double odx=other->x-player->padx;
    double ody=other->y-player->pady;
    double proj=dx*odx+dy*ody;
    if ((proj<0.0)||(proj>d2)) continue; // Too near (impossible) or far. No occlusion.
    double cp=dx*ody-dy*odx;
    if ((cp>barrel2)||(cp<-barrel2)) continue;
    return 1;
  }
  return -1;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If my holdclock is running, tick it down before doing anything else.
   */
  if (player->holdclock>0.0) {
    player->holdclock-=elapsed;
    return;
  }

  /* Gobble a frog that stays targetted thru my lockontime.
   */
  if (player->ingobble) {
    player->ingobble=0;
    player->inpaddle=0;
    player->lockon=0;
    return;
  } else if (player->target&&(player->target==player->lockon)) {
    if ((player->lockonclock-=elapsed)<=0.0) {
      player->ingobble=1;
      player->inpaddle=0;
      player->lockon=0;
      double t=(rand()&0xffff)/65535.0;
      player->holdclock=t*player->postholdlo+(1.0-t)*player->postholdhi;
    }
    return;
  } else if (player->target) {
    player->inpaddle=0;
    player->lockon=player->target;
    double t=(rand()&0xffff)/65535.0;
    player->lockonclock=t*player->lockontimehi+(1.0-t)*player->lockontimelo;
    return;
  }
  
  int ninpaddle=0;
  
  /* If all frogs are far away, paddle inward.
   * Otherwise if we have a line of sight to a present frog, paddle inward.
   * Otherwise paddle outward.
   * Never sit idle.
   */
  struct frog *nearest=0;
  double neard2=999999.999;
  struct frog *frog=BATTLE->frogv;
  int i=BATTLE->frogc;
  for (;i-->0;frog++) {
    double dx=frog->x-player->padx;
    double dy=frog->y-player->pady;
    double d2=dx*dx+dy*dy;
    if (!nearest||(d2<neard2)) {
      nearest=frog;
      neard2=d2;
    }
  }
  if (neard2>player->paddlerange2) {
    ninpaddle=-1;
  } else {
    for (i=BATTLE->frogc,frog=BATTLE->frogv;i-->0;frog++) {
      if (!frog->present) continue;
      int assessment=frogeating_check_line_of_sight(battle,player,frog);
      if (assessment<0) {
        ninpaddle=-1;
        if (assessment<-1) break; // Hard yes.
      }
    }
    // No line of sight to a present frog? Paddle outward to shuffle them.
    if (!ninpaddle) ninpaddle=1;
  }
  
  /* Apply new paddle direction if it changed.
   */
  if (ninpaddle!=player->inpaddle) {
    player->inpaddle=ninpaddle;
    double t=(rand()&0xffff)/65535.0;
    player->holdclock=t*player->holdtimelo+(1.0-t)*player->holdtimehi;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Animate arm.
   */
  const double armrate=2.000;
  if (player->inpaddle<0) {
    if ((player->arm-=armrate*elapsed)<0.0) player->arm+=1.0;
  } else if (player->inpaddle>0) {
    if ((player->arm+=armrate*elapsed)>1.0) player->arm-=1.0;
  } else {
    player->arm=0.5;
  }

  /* If paddling, we either attract (<0) or repel (>0) frogs according to an inverse square law.
   */
  if (player->inpaddle) {
    struct frog *frog=BATTLE->frogv;
    int i=BATTLE->frogc;
    for (;i-->0;frog++) {
      double dx=frog->x-player->padx;
      double dy=frog->y-player->pady;
      double d2=dx*dx+dy*dy;
      if (d2<1.0) continue; // Too close, math gets weird.
      double n=1.0-d2/player->padrad2;
      if (n<=0.0) continue; // Out of range.
      double dist=sqrt(d2);
      double nx=dx/dist;
      double ny=dy/dist;
      frog->dx+=player->padaccel*n*nx*player->inpaddle*elapsed;
      frog->dy+=player->padaccel*n*ny*player->inpaddle*elapsed;
    }
  }
  
  /* Refresh my target.
   * Do this constantly, not just when attempting gobble, because we highlight the gobble candidate.
   */
  {
    struct frog *nearest=0;
    double neard2=999999.999;
    struct frog *frog=BATTLE->frogv;
    int i=BATTLE->frogc;
    for (;i-->0;frog++) {
      if (!frog->present) continue;
      double dx=frog->x-player->padx;
      double dy=frog->y-player->pady;
      double d2=dx*dx+dy*dy;
      if (!nearest||(d2<neard2)) {
        nearest=frog;
        neard2=d2;
      }
    }
    if (neard2<player->gobrad2) {
      player->target=nearest;
    } else {
      player->target=0;
    }
  }
  
  /* Gobble a frog?
   */
  if (player->ingobble!=player->pvingobble) {
    if (player->pvingobble=player->ingobble) {
      if (player->target&&player->target->present) {
        bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->score++;
        player->target->present=0;
        player->target=0;
        player->gobbleclock=0.500;
      }
    }
  }
}

/* Update a frog.
 */
 
static void frog_update(struct battle *battle,struct frog *frog,double elapsed) {

  // Decorative blinking animation.
  if ((frog->blinkclock-=elapsed)<=0.0) {
    if (frog->blink^=1) {
      frog->blinkclock=0.250;
    } else {
      frog->blinkclock=0.500+((rand()&0xffff)*3.000)/65535.0;
    }
  }
  
  // Move optimistically.
  frog->x+=frog->dx*elapsed;
  frog->y+=frog->dy*elapsed;
  
  // Bounce off the pond edges.
  const double pondr2=(POND_RADIUS-FROG_RADIUS)*(POND_RADIUS-FROG_RADIUS);
  double m2=frog->x*frog->x+frog->y*frog->y;
  if (m2>pondr2) {
    // Because our origin is the center of the pond and we're both circles, the frog's position is also the incident normal, how convenient.
    double m=sqrt(m2);
    double nx=frog->x/m;
    double ny=frog->y/m;
    double proj=frog->dx*nx+frog->dy*ny;
    double px=proj*nx;
    double py=proj*ny;
    frog->dx-=2.0*px;//*COLLISION_PENALTY;
    frog->dy-=2.0*py;//*COLLISION_PENALTY;
    double pen=m-(POND_RADIUS-FROG_RADIUS);
    frog->x-=pen*nx;
    frog->y-=pen*ny;
  }
  
  // Bounce off other frogs.
  struct frog *other=BATTLE->frogv;
  int i=BATTLE->frogc;
  for (;i-->0;other++) {
    if (frog==other) continue;
    double dx=other->x-frog->x;
    double dy=other->y-frog->y;
    double d2=dx*dx+dy*dy;
    if (d2>=FROG_DIAMETER_2) continue;
    double dist=sqrt(d2);
    double pen=FROG_RADIUS*2.0-dist;
    double nx=dx/dist; // Unit vector pointing from me to him.
    double ny=dy/dist;
    // Rectify equally.
    other->x+=nx*pen*0.5;
    other->y+=ny*pen*0.5;
    frog->x-=nx*pen*0.5;
    frog->y-=ny*pen*0.5;
    // Reflect both.
    double fproj=frog->dx*nx+frog->dy*ny;
    double oproj=other->dx*nx+other->dy*ny;
    double force=(fproj-oproj)*COLLISION_PENALTY;
    frog->dx-=force*nx;
    frog->dy-=force*ny;
    other->dx+=force*nx;
    other->dy+=force*ny;
  }
}

/* Update.
 */
 
static void _frogeating_update(struct battle *battle,double elapsed) {
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->gobbleclock>0.0) {
      player->gobbleclock-=elapsed;
      player->inpaddle=0;
      player->ingobble=0;
    } else if (battle->outcome>-2) {
      player->inpaddle=0;
      player->ingobble=0;
    } else {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
    player_update_common(battle,player,elapsed);
  }
  
  /* Update frogs.
   */
  struct frog *frog=BATTLE->frogv;
  for (i=BATTLE->frogc;i-->0;frog++) {
    frog_update(battle,frog,elapsed);
  }
  
  /* Diminish frog velocities proportionately at each tick of a clock.
   * Doing this continuously would be painful.
   */
  if ((BATTLE->decayclock-=elapsed)<=0.0) {
    BATTLE->decayclock+=0.100;
    double decay=0.995;
    for (i=BATTLE->frogc,frog=BATTLE->frogv;i-->0;frog++) {
      frog->dx*=decay;
      frog->dy*=decay;
    }
  }
  
  /* Game ends when playclock expires or one player crosses the midpoint.
   * Ties are not at all unusual.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    int mid=(BATTLE->frogc>>1)+1;
    if ((BATTLE->playclock-=elapsed)<=0.0) {
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    } else if (l->score>=mid) battle->outcome=1;
    else if (r->score>=mid) battle->outcome=-1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int ht=NS_sys_tilesize>>1;
  uint8_t bodytile=player->tileid+1;
  if (player->gobbleclock>0.0) bodytile+=2;
  int midx=FBW>>1;
  if (player->who) midx+=(int)POND_RADIUS+6;
  else midx-=(int)POND_RADIUS+6;
  int midy=(FBH>>1)+3;
  uint8_t xform=player->who?EGG_XFORM_XREV:0;
  
  // Back arm.
  int armd=3+lround(player->arm*7.0);
  if (armd<3) armd=3; else if (armd>10) armd=10;
  if (player->who) armd=-armd;
  int armx=midx+armd;
  int army=midy+2;
  graf_tile(&g.graf,armx,army,player->tileid+0x10,xform);
  
  int lx=midx-ht,rx=midx+ht;
  if (player->who) {
    lx=midx+ht;
    rx=midx-ht;
  }
  graf_tile(&g.graf,lx,midy-ht,bodytile+0x00,xform);
  graf_tile(&g.graf,rx,midy-ht,bodytile+0x01,xform);
  graf_tile(&g.graf,lx,midy+ht,bodytile+0x10,xform);
  graf_tile(&g.graf,rx,midy+ht,bodytile+0x11,xform);
}

/* Render.
 */
 
static void _frogeating_render(struct battle *battle) {

  /* Background. Ground and the pond image.
   */
  const int midx=FBW>>1;
  const int midy=(FBH>>1)+15; // lil offset to account for the scoreboard up top
  const int pondw=NS_sys_tilesize*8;
  const int pondh=NS_sys_tilesize*8;
  const int pondx=(FBW>>1)-(pondw>>1);
  const int pondy=midy-(pondh>>1);
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(&g.graf,RID_image_battle_forest);
  graf_decal(&g.graf,pondx,pondy,0,NS_sys_tilesize*8,pondw,pondh);
  
  /* On the water, below the frogs, render input indicators.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->inpaddle) {
    int x=midx+(int)l->padx+NS_sys_tilesize;
    int y=midy+(int)l->pady;
    graf_tile(&g.graf,x,y,(l->inpaddle>0)?0x8c:0x8d,0);
  }
  if (r->inpaddle) {
    int x=midx+(int)r->padx-NS_sys_tilesize;
    int y=midy+(int)r->pady;
    graf_tile(&g.graf,x,y,(r->inpaddle>0)?0x8c:0x8d,EGG_XFORM_XREV);
  }
  
  /* Frogs.
   */
  struct frog *frog=BATTLE->frogv;
  int i=BATTLE->frogc;
  for (;i-->0;frog++) {
    int x=midx+(int)frog->x;
    int y=midy+(int)frog->y;
    graf_tile(&g.graf,x,y,0x88,0);
    if (frog->present) {
      graf_tile(&g.graf,x,y,frog->blink?0x8a:0x89,frog->xform);
      uint32_t highlight=0;
           if (BATTLE->playerv[0].target==frog) highlight=BATTLE->playerv[0].color;
      else if (BATTLE->playerv[1].target==frog) highlight=BATTLE->playerv[1].color;
      if (highlight) {
        graf_fancy(&g.graf,x,y-3,0x8b,0,0,NS_sys_tilesize,0,highlight);
      }
    }
  }
  
  /* Players.
   */
  player_render(battle,l);
  player_render(battle,r);
  
  /* Scoreboard.
   */
  int sbw=BATTLE->frogc*NS_sys_tilesize;
  int sbx=(FBW>>1)-(sbw>>1);
  int sby=12;
  sbx+=NS_sys_tilesize>>1;
  for (i=0;i<BATTLE->frogc;i++,sbx+=NS_sys_tilesize) {
    uint8_t tileid=0x8f; // -1 for eyes open
    uint32_t color=0x808080ff;
    if (i<l->score) { tileid--; color=l->color; }
    else if (i>=BATTLE->frogc-r->score) { tileid--; color=r->color; }
    graf_fancy(&g.graf,sbx,sby,tileid,0,0,NS_sys_tilesize,0,color);
  }
   
  /* Clock.
   */
  if (BATTLE->playclock>0.0) {
    int clocky=28;
    int s=(int)(BATTLE->playclock+0.999);
    if (s<1) s=1;
    else if (s>99) s=99; // huh?
    graf_set_image(&g.graf,RID_image_fonttiles);
    if (s>=10) graf_tile(&g.graf,(FBW>>1)-4,clocky,'0'+s/10,0);
    graf_tile(&g.graf,(FBW>>1)+4,clocky,'0'+s%10,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_frogeating={
  .name="frogeating",
  .objlen=sizeof(struct battle_frogeating),
  .id=NS_battle_frogeating,
  .strix_name=274,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_dpad_a,
  .del=_frogeating_del,
  .init=_frogeating_init,
  .update=_frogeating_update,
  .render=_frogeating_render,
};

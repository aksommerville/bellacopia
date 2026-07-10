/* battle_broomrace.c
 * Thirty Seconds Apothecary, played competitively in a fixed field like Super Sprint.
 */

#include "game/bellacopia.h"

#define RADIUS 9.0
#define THING_LIMIT 5
#define TURNSPEED_WORST 2.000
#define TURNSPEED_BEST  4.000
#define ACCEL_WORST     200.0
#define ACCEL_BEST      500.0
#define SPEEDMAX_WORST  120.0
#define SPEEDMAX_BEST   200.0
#define DECAY_WORST      80.0
#define DECAY_BEST      400.0
#define FUDGE_WORST      40.0
#define FUDGE_BEST        2.0
#define FUDGE_TIME        5.0
#define CPU_PENALTY     0.666
#define FLDTOP             20

struct battle_broomrace {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    int blackout;
    int srcy;
    double skill; // 0..1, reverse of each other.
    int indt,ingas; // Human and CPU controllers set this, the rest is generic.
    double facet; // Radians, display angle. Zero is up.
    double turnspeed; // rad/sec
    double x,y; // Framebuffer pixels, center of 32x32-ish sprite.
    double dx,dy;
    double accel; // px/s**2 positive
    double speedmax; // px/s
    double decay; // px/s**2 positive
    double bounce; // -1..0, velocity change on a collision
    uint8_t thingv[THING_LIMIT]; // tileid
    int thingc;
    int thingseq;
    double thingx,thingy;
    double fudge;
    double fudgeclock; // Resets when new items appear. Fudge only counts for so long.
    double animclock;
    int animframe;
  } playerv[2];
  
  double thingx;
  double thingy;
  uint8_t thingtile;
  int thingseq;
};

#define BATTLE ((struct battle_broomrace*)battle)

/* Delete.
 */
 
static void _broomrace_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->turnspeed=TURNSPEED_WORST*(1.0-player->skill)+TURNSPEED_BEST*player->skill;
  player->y=FBH*0.5;
  player->accel=ACCEL_WORST*(1.0-player->skill)+ACCEL_BEST*player->skill;
  player->speedmax=SPEEDMAX_WORST*(1.0-player->skill)+SPEEDMAX_BEST*player->skill;
  player->decay=DECAY_WORST*(1.0-player->skill)+DECAY_BEST*player->skill;
  player->bounce=-0.900;
  player->fudge=FUDGE_WORST*(1.0-player->skill)+FUDGE_BEST*player->skill;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.333;
  } else { // Right.
    player->who=1;
    player->x=FBW*0.666;
  }
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->speedmax*=CPU_PENALTY;
  }
  switch (face) {
    // 64 is Moon Song, if we want it.
    case NS_face_monster: {
        player->srcy=192;
      } break;
    case NS_face_dot: {
        player->srcy=0;
      } break;
    case NS_face_princess: {
        player->srcy=128;
      } break;
  }
}

/* New.
 */
 
static int _broomrace_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indt=-1; break;
    case EGG_BTN_RIGHT: player->indt=1; break;
    default: player->indt=0;
  }
  
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else if (input&EGG_BTN_SOUTH) {
    player->ingas=1;
  } else {
    player->ingas=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  double targetx,targety;
  if (BATTLE->thingseq!=player->thingseq) {
    player->thingseq=BATTLE->thingseq;
    player->thingx=BATTLE->thingx+(((rand()&0xffff)-0x800)*player->fudge)/32768.0;
    player->thingy=BATTLE->thingy+(((rand()&0xffff)-0x800)*player->fudge)/32768.0;
  }
  if (BATTLE->thingtile) {
    targetx=player->thingx;
    targety=player->thingy;
  } else {
    player->indt=0;
    player->ingas=0;
    return;
  }
  
  if (player->fudgeclock>0.0) {
    if ((player->fudgeclock-=elapsed)<0.0) {
      player->fudgeclock=0.0;
      player->thingx=BATTLE->thingx;
      player->thingy=BATTLE->thingy;
    }
    double n=player->fudgeclock/FUDGE_TIME;
    targetx=player->thingx*n+BATTLE->thingx*(1.0-n);
    targety=player->thingy*n+BATTLE->thingy*(1.0-n);
  }
  
  double targett=atan2(targetx-player->x,player->y-targety);
  double dt=targett-player->facet;
  if (dt<-M_PI) dt+=M_PI*2.0;
  else if (dt>M_PI) dt-=M_PI*2.0;
  if (dt<-0.010) player->indt=-1;
  else if (dt>0.010) player->indt=1;
  else player->indt=0;
  if ((dt>-0.500)&&(dt<0.500)) player->ingas=1;
  else player->ingas=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Turn if requested.
  if (player->indt) {
    player->facet+=player->turnspeed*elapsed*player->indt;
    if (player->facet<-M_PI) player->facet+=M_PI*2.0;
    else if (player->facet>M_PI) player->facet-=M_PI*2.0;
  }
  
  // Apply acceleration or deceleration.
  if (player->ingas) {
    player->dx+=sin(player->facet)*player->accel*elapsed;
    player->dy-=cos(player->facet)*player->accel*elapsed;
    double mag=player->dx*player->dx+player->dy*player->dy;
    double max2=player->speedmax*player->speedmax;
    if (mag>max2) {
      mag=sqrt(mag);
      double scale=player->speedmax/mag;
      player->dx*=scale;
      player->dy*=scale;
    }
  } else {
    double mag=player->dx*player->dx+player->dy*player->dy;
    if (mag>1.0) {
      mag=sqrt(mag);
      double nx=player->dx/mag; if (nx<0.0) nx=-nx;
      double ny=player->dy/mag; if (ny<0.0) ny=-ny;
      if (player->dx<0.0) {
        if ((player->dx+=player->decay*nx*elapsed)>=0.0) player->dx=0.0;
      } else if (player->dx>0.0) {
        if ((player->dx-=player->decay*nx*elapsed)<=0.0) player->dx=0.0;
      }
      if (player->dy<0.0) {
        if ((player->dy+=player->decay*ny*elapsed)>=0.0) player->dy=0.0;
      } else if (player->dy>0.0) {
        if ((player->dy-=player->decay*ny*elapsed)<=0.0) player->dy=0.0;
      }
    } else {
      player->dx=player->dy=0.0;
    }
  }
  
  // Apply velocity and bounce on edges.
  if ((player->dx<-0.0)||(player->dy<-0.0)||(player->dx>0.0)||(player->dy>0.0)) {
    player->x+=player->dx*elapsed;
    player->y+=player->dy*elapsed;
    if (player->x<0.0) {
      player->x=0.0;
      if (player->dx<0.0) player->dx*=player->bounce;
    } else if (player->x>FBW) {
      player->x=FBW;
      if (player->dx>0.0) player->dx*=player->bounce;
    }
    if (player->y<0.0) {
      player->y=0.0;
      if (player->dy<0.0) player->dy*=player->bounce;
    } else if (player->y>FBH) {
      player->y=FBH;
      if (player->dy>0.0) player->dy*=player->bounce;
    }
  } else {
    player->dx=player->dy=0.0;
  }
  
  // If velocity above some threshold, animate.
  double dx2=player->dx*player->dx;
  double dy2=player->dy*player->dy;
  double d2=dx2+dy2;
  if (d2>=100.0) {
    // If far from the direction of travel, use the side-slide frames.
    double motiont=atan2(player->dx,-player->dy);
    double offt=motiont-player->facet;
    while (offt<-M_PI) offt+=M_PI*2.0;
    while (offt>M_PI) offt-=M_PI*2.0;
    if (offt>=0.500) {
      player->animframe=2;
    } else if (offt<=-0.500) {
      player->animframe=3;
    } else if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.150;
      if (++(player->animframe)>=2) player->animframe=0;
    }
  } else {
    player->animframe=0;
  }
  
  // Pick up things.
  if ((player->thingc<THING_LIMIT)&&BATTLE->thingtile) {
    const double radius=10.0;
    double dx=BATTLE->thingx-player->x;
    if ((dx>=-radius)&&(dx<=radius)) {
      double dy=BATTLE->thingy-player->y;
      if ((dy>=-radius)&&(dy<=radius)) {
        bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->thingv[player->thingc++]=BATTLE->thingtile;
        BATTLE->thingtile=0;
      }
    }
  }
}

/* Update.
 */
 
static void _broomrace_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  // Update each player individually.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Physics is a joke: There's just two bodies and they're equal-sized circles.
   * Edge bouncing is managed during player update.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  const double radsum2=(RADIUS*2.0)*(RADIUS*2.0);
  double dx=r->x-l->x;
  double dy=r->y-l->y;
  double d2=dx*dx+dy*dy;
  if (d2<radsum2) {
    const double radsum=RADIUS*2.0;
    double distance=sqrt(d2);
    double pen=radsum-distance; // positive
    double fx=(l->x+r->x)*0.5;
    double fy=(l->y+r->y)*0.5;
    // Correct each equally, away from (fx,fy).
    double nx=dx/distance;
    double ny=dy/distance;
    r->x+=nx*pen*0.5;
    r->y+=ny*pen*0.5;
    l->x-=nx*pen*0.5;
    l->y-=ny*pen*0.5;
    // Project both velocities onto the collision vector to assess the force of impact.
    double projl=l->dx*nx+l->dy*ny;
    double projr=r->dx*-nx+r->dy*-ny;
    double force=projl+projr;
    if (force>0.0) {
      l->dx+=force*nx*l->bounce;
      l->dy+=force*ny*l->bounce;
      r->dx-=force*nx*r->bounce;
      r->dy-=force*ny*r->bounce;
    }
  }
  
  /* If one player has all the things, they win.
   * Ties are not possible, since just one thing can be collected at a time.
   * Or if there's no thing, create one.
   */
  if (l->thingc>=THING_LIMIT) {
    battle->outcome=1;
  } else if (r->thingc>=THING_LIMIT) {
    battle->outcome=-1;
  } else if (!BATTLE->thingtile) {
    BATTLE->thingtile=0x17+rand()%9;
    BATTLE->thingx=10.0+(rand()%(FBW-20));
    BATTLE->thingy=FLDTOP+10.0+(rand()%(FBH-20-FLDTOP));
    BATTLE->thingseq++;
    l->fudgeclock=r->fudgeclock=FUDGE_TIME;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int srcx=player->animframe*64,srcy=player->srcy;
  graf_set_filter(&g.graf,1);
  graf_set_tint(&g.graf,0x000000ff);
  graf_set_alpha(&g.graf,0x80);
  graf_decal_rotate(&g.graf,(int)player->x,(int)player->y+4,srcx,srcy,64,sin(player->facet),cos(player->facet),0.333);
  graf_set_tint(&g.graf,0);
  graf_set_alpha(&g.graf,0xff);
  graf_decal_rotate(&g.graf,(int)player->x,(int)player->y,srcx,srcy,64,sin(player->facet),cos(player->facet),0.333);
  graf_set_filter(&g.graf,0);
}

/* Render.
 */
 
static void _broomrace_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_labyrinth);
  
  if (BATTLE->thingtile) {
    graf_tile(&g.graf,(int)BATTLE->thingx,(int)BATTLE->thingy,BATTLE->thingtile,0);
  }
  
  graf_set_image(&g.graf,RID_image_broomrace);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  graf_set_input(&g.graf,0);
  graf_fill_rect(&g.graf,0,0,FBW,FLDTOP,0x00000090);
  
  graf_set_image(&g.graf,RID_image_battle_labyrinth);
  int y=10;
  int x=10;
  const int spacing=10;
  int i=BATTLE->playerv[0].thingc;
  const uint8_t *src=BATTLE->playerv[0].thingv;
  for (;i-->0;src++,x+=spacing) graf_tile(&g.graf,x,y,*src,0);
  x=FBW-10;
  for (i=BATTLE->playerv[1].thingc,src=BATTLE->playerv[1].thingv;i-->0;src++,x-=spacing) graf_tile(&g.graf,x,y,*src,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_broomrace={
  .name="broomrace",
  .objlen=sizeof(struct battle_broomrace),
  .id=NS_battle_broomrace,
  .strix_name=181,
  .no_article=0,
  .no_contest=1,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_horz_a,
  .del=_broomrace_del,
  .init=_broomrace_init,
  .update=_broomrace_update,
  .render=_broomrace_render,
};

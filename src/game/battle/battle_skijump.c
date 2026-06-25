/* battle_skijump.c
 * Hold and release south at the right moments.
 */

#include "game/bellacopia.h"

#define SCOREC 4 /* How many judges. */

static const struct coursepoint {
  double x,y; // Pixels relative to the player's section. (320x90)
  double t;
} coursepointv[]={
  { 10.0,10.0,0.0}, // top of hill
  { 22.0,12.0,0.800},
  { 58.0,37.0,0.800},
  { 98.0,62.0,0.600},
  {118.0,67.0,-0.100}, // start of ramp
  {147.0,61.0,-0.150},
  {156.0,53.0,-2.0}, // end of ramp
  {187.0,15.0,-M_PI*3.0}, // peak
  {204.0,78.0,-M_PI*6.0}, // landing
};
static const int coursepointc=sizeof(coursepointv)/sizeof(coursepointv[0]);

struct battle_skijump {
  struct battle hdr;
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid;
    double speed; // px/s. The main driver of difficulty.
    
    int button; // Controller sets this and nothing else.
    
    double cpuclock;
    double targetxon,targetxoff; // Where I will press and release, accounting for skill etc.
    
    double x,y; // Pixels, in player's section.
    double t; // Radians clockwise from upright.
    double dx,dy,dt;
    int coursepointp; // Index we are heading toward. If zero, we haven't started. If >=coursepointc, we're done.
    int pvbutton;
    double xon,xoff; // Positions where the button was pressed and released.
    double pointtime;
    int scorev[SCOREC]; // Populated when we reach the end of the course.
    int score; // Sum of scorev.
  } playerv[2];
};

#define BATTLE ((struct battle_skijump*)battle)

/* Delete.
 */
 
static void _skijump_del(struct battle *battle) {
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
    player->cpuclock=0.500+((rand()&0xffff)*1.500)/65535.0;
    // Select total error in pixels based on skill.
    const double minerr= 3.000; // Never be perfect.
    const double maxerr=40.000; // ...but when bad be real bad.
    double err=minerr*player->skill+maxerr*(1.0-player->skill);
    // Divide randomly among the on and off moments.
    double onerr=((rand()&0xffff)*err)/65535.0;
    double offerr=err-onerr;
    // Positive or negative, let it be random.
    switch (rand()&3) {
      case 1: onerr=-onerr; break;
      case 2: offerr=-offerr; break;
      case 3: onerr=-onerr; offerr=-offerr; break;
    }
    // Don't let onerr be greater than negative offerr -- if so, flip onerr negative.
    if (onerr>-offerr) onerr=-onerr;
    // And finally, commit our on and off moments.
    player->targetxon=coursepointv[4].x+onerr;
    player->targetxoff=coursepointv[6].x+offerr;
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x06;
      } break;
    case NS_face_dot: {
        player->tileid=0x05;
      } break;
    case NS_face_princess: {
        player->tileid=0x07;
      } break;
  }
  player->speed=130.0*(1.0-player->skill)+90.0*player->skill;
  player->x=coursepointv[0].x;
  player->y=coursepointv[0].y;
  player->t=coursepointv[0].t;
  player->coursepointp=0; // Wait for a keystroke to start.
  player->pvbutton=1;
}

/* New.
 */
 
static int _skijump_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  // And I keep getting this backward: At HIGH skill, the player is favored to win. High bias means high skill for the CPU and low skill for Dot.
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playclock=20.0;
  return 0;
}

/* Decide score if we haven't yet.
 */
 
static void skijump_require_score(struct battle *battle,struct player *player) {
  if (player->score) return;
  double da=player->xon-coursepointv[4].x;
  double db=player->xoff-coursepointv[6].x;
  if (da<0.0) da=-da;
  if (db<0.0) db=-db;
  double err=da+db; // Sum of misses in pixels.
  const double maxerr=30.0;
  double q=1.0-err/maxerr;
  if (q<0.0) q=0.0;
  else if (q>1.0) q=1.0;
  const int max=SCOREC*9;
  player->score=(int)(q*max);
  if (player->score<1) player->score=1;
  
  // Apportion among the judges.
  int each=player->score/SCOREC;
  int more=player->score%SCOREC;
  int *dst=player->scorev;
  int i=SCOREC;
  for (;i-->0;dst++) *dst=each;
  while (more-->0) {
    int candidatev[SCOREC];
    int candidatec=0;
    for (dst=player->scorev,i=0;i<SCOREC;i++,dst++) if (*dst<9) candidatev[candidatec++]=i;
    if (candidatec<1) break;
    int p=rand()%candidatec;
    player->scorev[candidatev[p]]++;
  }
}

/* Advance player to the next coursepoint.
 */
 
static void skijump_next_point(struct battle *battle,struct player *player) {
  player->coursepointp++;
  if (player->coursepointp<=0) {
    player->x=coursepointv[0].x;
    player->y=coursepointv[0].y;
    player->t=coursepointv[0].t;
    player->dx=player->dy=player->dt=0.0;
    player->pointtime=999.999;
  } else if (player->coursepointp>=coursepointc) {
    player->x=coursepointv[coursepointc-1].x;
    player->y=coursepointv[coursepointc-1].y;
    player->t=coursepointv[coursepointc-1].t;
    player->dx=player->dy=player->dt=0.0;
    player->pointtime=999.999;
    skijump_require_score(battle,player);
  } else {
    const struct coursepoint *b=coursepointv+player->coursepointp;
    const struct coursepoint *a=b-1;
    player->x=a->x;
    player->y=a->y;
    player->t=a->t;
    player->dx=b->x-a->x;
    player->dy=b->y-a->y;
    player->dt=b->t-a->t;
    double d2=player->dx*player->dx+player->dy*player->dy;
    double distance=sqrt(d2);
    player->dx=(player->dx*player->speed)/distance;
    player->dy=(player->dy*player->speed)/distance;
    player->pointtime=distance/player->speed;
    player->dt/=player->pointtime;
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  player->button=(input&EGG_BTN_SOUTH)?1:0;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // Waiting to begin?
  if (player->cpuclock>0.0) {
    if ((player->cpuclock-=elapsed)<=0.0) player->button=1;
    return;
  }
  
  // Release button early in the descent.
  if (player->x<40.0) {
    player->button=0;
    return;
  }
  
  // When we cross (targetxoff), release the button.
  if (player->x>=player->targetxoff) {
    player->button=0;
    return;
  }
  
  // When we cross (targetxon), press the button.
  if (player->x>=player->targetxon) {
    player->button=1;
    return;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Protection against player never pressing button, or holding forever.
  if (player->coursepointp>=coursepointc) {
    player->button=0;
    if (player->xon<=0.0) player->xon=player->x;
  }

  if (player->button!=player->pvbutton) {
    player->pvbutton=player->button;
    if (player->button&&!player->coursepointp) { // Start the descent.
      skijump_next_point(battle,player);
    } else if (player->button&&(player->xon<=0.0)) {
      player->xon=player->x;
    } else if (!player->button&&(player->xon>0.0)&&(player->xoff<=0.0)) {
      player->xoff=player->x;
    }
  }
  
  if ((player->coursepointp>0)&&(player->coursepointp<coursepointc)) {
    player->x+=player->dx*elapsed;
    player->y+=player->dy*elapsed;
    player->t+=player->dt*elapsed;
    if ((player->pointtime-=elapsed)<=0.0) {
      skijump_next_point(battle,player);
    }
  }
}

/* Update.
 */
 
static void _skijump_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  // Update players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  // If both players are finished, game over.
  if ((l->coursepointp>=coursepointc)&&(r->coursepointp>=coursepointc)) {
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
    return;
  }
  
  // Tick down playclock. If it expires, wait for anyone in progress to finish, then it's over.
  // If you don't start, you're not allowed to win or tie.
  if ((BATTLE->playclock-=elapsed)<=0.0) {
    if (!l->coursepointp) {
      if (!r->coursepointp) battle->outcome=0;
      if (r->coursepointp>=coursepointc) battle->outcome=-1;
    } else if (!r->coursepointp) {
      if (l->coursepointp>=coursepointc) battle->outcome=1;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player,int y0) {
  const int fldh=FBH>>1;
  graf_set_image(&g.graf,RID_image_skijump);
  graf_decal(&g.graf,0,y0,0,0,FBW,fldh);
  graf_set_image(&g.graf,RID_image_icepalace_sprites);
  int px=(int)player->x;
  int py=y0+(int)player->y;
  uint8_t rotate=(int8_t)((player->t*128.0)/M_PI);
  graf_set_filter(&g.graf,1);
  graf_fancy(&g.graf,px,py,player->tileid,0,rotate,NS_sys_tilesize,0,0x808080ff);
  graf_set_filter(&g.graf,0);
  
  /* Highlight the hold range.
   */
  if (player->xon>0.0) {
    graf_tile(&g.graf,(int)player->xon,y0+40,0x15,0);
    if (player->xoff>0.0) {
      graf_tile(&g.graf,(int)player->xoff,y0+40,0x16,0);
    }
  }
  
  /* Judges.
   */
  int dstx=300;
  int dsty=y0+76;
  uint8_t tileid=0x30;
  if (player->score) tileid++; // arms raised
  int i=SCOREC;
  for (;i-->0;dstx-=20,tileid+=2) {
    graf_tile(&g.graf,dstx,dsty,tileid,EGG_XFORM_XREV);
    if (player->score) {
      graf_tile(&g.graf,dstx,dsty-NS_sys_tilesize,0x20+player->scorev[i],0);
    }
  }
}

/* Render.
 */
 
static void _skijump_render(struct battle *battle) {
  // Draw the bottom half first in case something breaches its top edge.
  player_render(battle,BATTLE->playerv+1,FBH>>1);
  player_render(battle,BATTLE->playerv+0,0);
  graf_set_input(&g.graf,0);
  graf_fill_rect(&g.graf,0,FBH>>1,FBW,1,0x000000ff);
}

/* Type definition.
 */
 
const struct battle_type battle_type_skijump={
  .name="skijump",
  .objlen=sizeof(struct battle_skijump),
  .id=NS_battle_skijump,
  .strix_name=227,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_skijump_del,
  .init=_skijump_init,
  .update=_skijump_update,
  .render=_skijump_render,
};

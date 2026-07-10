/* battle_dissection.c
 * Place your scalpel, tap A to commit position, then it rotates and you hold A to slice along that direction.
 * Sever the blood vessels but don't touch the organs.
 */

#include "game/bellacopia.h"

#define FLDW 150
#define FLDH 150

#define CUTTED_NW 0x0001
#define CUTTED_NE 0x0002
#define CUTTED_SW 0x0004
#define CUTTED_SE 0x0008
#define CUTTED_GOOD (CUTTED_NW|CUTTED_NE|CUTTED_SW|CUTTED_SE)
#define CUTTED_NWORG 0x0010
#define CUTTED_NEORG 0x0020
#define CUTTED_SWORG 0x0040
#define CUTTED_SEORG 0x0080
#define CUTTED_HEART 0x0100
#define CUTTED_BAD 0x01f0

#define BLOOD_LIMIT 64
#define BLOOD_PERIOD 0.125

#define SPEED_LO 35.0
#define SPEED_HI 70.0
#define SPEED_CPU_PENALTY 0.800
#define DT_LO 2.500
#define DT_HI 4.000
#define DT_CPU_PENALTY 0.800

struct battle_dissection {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    int go; // Input state, the only thing controllers should touch.
    int pvgo;
    double animclock; // Counts up.
    double animperiod;
    double t;
    double dt;
    double tlo,thi;
    double x,y; // Pixels, relative to my field.
    double speed; // px/sec
    
    // Organ positions in field. These are probably constant.
    double nwx,nwy;
    double nex,ney;
    double swx,swy;
    double sex,sey;
    double hx,hy;
    
    // Length of each vessel, center to center.
    double nwlen,nelen,swlen,selen;
    
    // Which things have been cut?
    uint16_t cutted;
    
    // CPU player only.
    double changeclock;
    double changetime;
    
    // Decorative gore for when you cut an organ.
    struct blood {
      double x,y;
      double dx,dy;
      double animclock;
      int animframe;
      double ttl;
    } bloodv[BLOOD_LIMIT];
    int bloodc;
    double bloodclock;
  } playerv[2];
};

#define BATTLE ((struct battle_dissection*)battle)

/* Delete.
 */
 
static void _dissection_del(struct battle *battle) {
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
    player->changetime=0.250;
    player->changeclock=player->changetime; // Start with a delay, to give the pathetic human a chance.
  }
  switch (face) {
    case NS_face_monster: {
      } break;
    case NS_face_dot: {
      } break;
    case NS_face_princess: {
      } break;
  }
  player->animperiod=1.000;
  player->animclock=(player->who?0.5:0.0)*player->animperiod;
  player->t=0.0;
  player->dt=DT_HI*player->skill+DT_LO*(1.0-player->skill);
  if (player->who) player->dt*=-1.0;
  player->tlo=M_PI*-0.25;
  player->thi=M_PI*0.25;
  player->x=FLDW*0.5;
  player->y=FLDH-10.0;
  player->speed=SPEED_HI*player->skill+SPEED_LO*(1.0-player->skill);
  if (!player->human) {
    // CPU takes a penalty on both speeds, otherwise it's beatable but a little too good.
    player->dt*=DT_CPU_PENALTY;
    player->speed*=SPEED_CPU_PENALTY;
  }
  
  player->hx=FLDW*0.5;
  player->hy=FLDH*0.5;
  player->nwx=player->swx=FLDW*0.200;
  player->nwy=player->ney=FLDH*0.200;
  player->nex=player->sex=FLDW-player->nwx;
  player->swy=player->sey=FLDH-player->nwy;
  player->nwlen=sqrt((player->nwx-player->hx)*(player->nwx-player->hx)+(player->nwy-player->hy)*(player->nwy-player->hy));
  player->nelen=sqrt((player->nex-player->hx)*(player->nex-player->hx)+(player->ney-player->hy)*(player->ney-player->hy));
  player->swlen=sqrt((player->swx-player->hx)*(player->swx-player->hx)+(player->swy-player->hy)*(player->swy-player->hy));
  player->selen=sqrt((player->sex-player->hx)*(player->sex-player->hx)+(player->sey-player->hy)*(player->sey-player->hy));
}

/* New.
 */
 
static int _dissection_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  player->go=(input&EGG_BTN_SOUTH);
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* After changing state, delay for a constant interval.
   */
  if (player->changeclock>0.0) {
    player->changeclock-=elapsed;
    return;
  }
  player->go=0;
   
  /* Where to go? Get out if everything is cut already.
   * Need to aim more than halfway from heart to extremity.
   */
  double targetx,targety;
       if (!(player->cutted&CUTTED_SE)) { targetx=player->sex; targety=player->sey; }
  else if (!(player->cutted&CUTTED_NE)) { targetx=player->nex; targety=player->ney; }
  else if (!(player->cutted&CUTTED_NW)) { targetx=player->nwx; targety=player->nwy; }
  else if (!(player->cutted&CUTTED_SW)) { targetx=player->swx; targety=player->swy; }
  else return;
  const double outness=0.666;
  targetx=targetx*outness+player->hx*(1.0-outness);
  targety=targety*outness+player->hy*(1.0-outness);
  
  /* Turn target into an angle. Either the true angle, or the nearer limit.
   * The player angles don't wrap but we need them wrapped.
   * Mind that we must wrap them all together, their relative values are most important.
   * One of the limits might go outside pi.
   */
  double targett;
  double tt=atan2(targetx-player->x,player->y-targety);
  double dt=tt-player->t;
  while (dt<-M_PI) dt+=M_PI*2.0;
  while (dt>M_PI) dt-=M_PI*2.0;
  double dlo=player->tlo-player->t;
  double dhi=player->thi-player->t;
  if (dt<dlo) targett=player->tlo;
  else if (dt>dhi) targett=player->thi;
  else {
    targett=tt;
    dt=targett-player->t;
    while (dt<-M_PI) { dt+=M_PI*2.0; targett+=M_PI*2.0; }
    while (dt>M_PI) { dt-=M_PI*2.0; targett-=M_PI*2.0; }
  }
  
  /* If we are within some threshold of the target angle, and rotating away from it, go.
   */
  const double thresh=0.100;
  dt=targett-player->t;
  if ((dt>=-thresh)&&(dt<=thresh)) {
    if (((dt<0.0)&&(player->dt>0.0))||((dt>0.0)&&(player->dt<0.0))) {
      player->go=1;
    }
  }
  
  /* If our new state is not what we had before, commit to it for a few frames.
   */
  if (player->go!=player->pvgo) {
    player->changeclock=player->changetime;
  }
}

/* After a scalpel moves, check whether it cut anything.
 * We don't check crossings, it's all just proximity.
 */
 
static void dissection_check_cuts(struct battle *battle,struct player *player) {

  /* First check the organs.
   * They're all circular. Within a certain distance, slice.
   * We're checking the scapel's center, not its tip, so no need to build in any margin here.
   */
  const double heart_radius2=24.0*24.0;
  const double extr_radius2=16.0*16.0;
  double dx,dy,d2;
  dx=player->x-player->nwx;
  dy=player->y-player->nwy;
  d2=dx*dx+dy*dy;
  if (d2<extr_radius2) { player->cutted|=CUTTED_NWORG; return; }
  dx=player->x-player->nex;
  dy=player->y-player->ney;
  d2=dx*dx+dy*dy;
  if (d2<extr_radius2) { player->cutted|=CUTTED_NEORG; return; }
  dx=player->x-player->swx;
  dy=player->y-player->swy;
  d2=dx*dx+dy*dy;
  if (d2<extr_radius2) { player->cutted|=CUTTED_SWORG; return; }
  dx=player->x-player->sex;
  dy=player->y-player->sey;
  d2=dx*dx+dy*dy;
  if (d2<extr_radius2) { player->cutted|=CUTTED_SEORG; return; }
  dx=player->x-player->hx;
  dy=player->y-player->hy;
  d2=dx*dx+dy*dy;
  if (d2<heart_radius2) { player->cutted|=CUTTED_HEART; return; }
  
  /* Then the vessels.
   * They will always be perfect eighth-turn diagonals, but we'll measure generically anyway in case something changes.
   * Only check one vessel, according the quadrant we're in relative to the heart.
   * Then we don't need to bother checking the scalar projection (tho at the limit, it might be possible to cut a vessel from the far side of its extremity!)
   */
  const double vessel_radius=5.0;
  double cp;
  uint16_t mask;
  #define GETDISTANCE(bx,by,len) { \
    cp=((bx-player->hx)*(player->y-player->hy)-(by-player->hy)*(player->x-player->hx))/len; \
  }
  if (player->x<player->hx) {
    if (player->y<player->hy) {
      GETDISTANCE(player->nwx,player->nwy,player->nwlen)
      mask=CUTTED_NW;
    } else {
      GETDISTANCE(player->swx,player->swy,player->nelen)
      mask=CUTTED_SW;
    }
  } else {
    if (player->y<player->hy) {
      GETDISTANCE(player->nex,player->ney,player->nelen)
      mask=CUTTED_NE;
    } else {
      GETDISTANCE(player->sex,player->sey,player->selen)
      mask=CUTTED_SE;
    }
  }
  #undef GETDISTANCE
  if (!(player->cutted&mask)&&(cp>-vessel_radius)&&(cp<vessel_radius)) {
    player->cutted|=mask;
    bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Decorative animation of the organs.
  if ((player->animclock+=elapsed)>=player->animperiod) {
    player->animclock=0.0;
    bm_sound_pan(RID_sound_heartbeat,player->who?PLAYER_PAN:-PLAYER_PAN);
  }
  
  // With a cut organ, animate blood. No further updating in this case.
  if (player->cutted&CUTTED_BAD) {
    if (player->bloodc<BLOOD_LIMIT) {
      if ((player->bloodclock-=elapsed)<=0.0) {
        player->bloodclock+=BLOOD_PERIOD;
        struct blood *blood=player->bloodv+player->bloodc++;
        blood->ttl=1.000;
        blood->x=player->x;
        blood->y=player->y;
        const double speed=30.0;
        double t=((rand()&0xffff)*M_PI*2.0)/65535.0;
        blood->dx=speed*cos(t);
        blood->dy=speed*sin(t);
        blood->animframe=(rand()&7);
      }
    }
    struct blood *blood=player->bloodv;
    int i=player->bloodc;
    for (;i-->0;blood++) {
      blood->x+=blood->dx*elapsed;
      blood->y+=blood->dy*elapsed;
      if ((blood->animclock-=elapsed)<=0.0) {
        blood->animclock+=0.100;
        if (++(blood->animframe)>=8) blood->animframe=0;
      }
      blood->ttl-=elapsed;
    }
    for (i=player->bloodc,blood=player->bloodv+player->bloodc-1;i-->0;blood--) {
      if (blood->ttl>0.0) continue;
      player->bloodc--;
      memmove(blood,blood+1,sizeof(struct blood)*(player->bloodc-i));
    }
    return;
  }
  
  // Also if outcome is established, we're done.
  if (battle->outcome>-2) return;
  
  // Input state changes.
  if (player->go!=player->pvgo) {
    if (player->go) {
      player->tlo=player->t-M_PI*0.250;
      player->thi=player->t+M_PI*0.250;
    }
    player->pvgo=player->go;
  }
  
  /* Advance if button held.
   */
  if (player->go) {
    player->x+=sin(player->t)*player->speed*elapsed;
    player->y-=cos(player->t)*player->speed*elapsed;
    if (player->x<0.0) player->x=0.0; else if (player->x>FLDW) player->x=FLDW;
    if (player->y<0.0) player->y=0.0; else if (player->y>FLDH) player->y=FLDH;
    dissection_check_cuts(battle,player);
    if (player->cutted&CUTTED_BAD) {
      bm_sound_pan(RID_sound_fart,player->who?PLAYER_PAN:-PLAYER_PAN);
    }
    
  /* Rotate back and forth if button released.
   */
  } else {
    player->t+=player->dt*elapsed;
    if ((player->t<player->tlo)&&(player->dt<0.0)) player->dt=-player->dt;
    else if ((player->t>player->thi)&&(player->dt>0.0)) player->dt=-player->dt;
  }
}

/* Update.
 */
 
static void _dissection_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Lose if you touch an organ, then win if you cut all the vessels.
   * Ties are possible but unlikely; you'd have to meet both players' conditions on the same frame.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->cutted&CUTTED_BAD) {
      if (r->cutted&CUTTED_BAD) battle->outcome=0;
      else battle->outcome=-1;
    } else if (r->cutted&CUTTED_BAD) {
      battle->outcome=1;
    } else if ((l->cutted&CUTTED_GOOD)==CUTTED_GOOD) {
      if ((r->cutted&CUTTED_GOOD)==CUTTED_GOOD) battle->outcome=0;
      else battle->outcome=1;
    } else if ((r->cutted&CUTTED_GOOD)==CUTTED_GOOD) {
      battle->outcome=-1;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int fldx=(FBW>>2)-(FLDW>>1);
  if (player->who) fldx+=FBW>>1;
  int fldy=(FBH>>1)-(FLDH>>1);
  graf_fill_rect(&g.graf,fldx,fldy,FLDW,FLDH,0x8fcec3ff);
  
  graf_set_image(&g.graf,RID_image_battle_fractia2);
  
  /* Turn animclock into three discrete frame indicators.
   * If the heart is cut, nothing animates.
   */
  int engorge_heart=0,engorge_vessels=0,engorge_extremities=0;
  if (player->cutted&CUTTED_HEART) ;
  else if (player->animclock<player->animperiod*0.200) engorge_heart=1;
  else if (player->animclock<player->animperiod*0.400) engorge_vessels=1;
  else if (player->animclock<player->animperiod*0.600) engorge_extremities=1;
  
  /* Render organs in vertical order so they can overlap each other.
   */
  graf_decal(&g.graf,
    fldx+(int)player->nwx-16,
    fldy+(int)player->nwy-16,
    (engorge_extremities&&!(player->cutted&(CUTTED_NW|CUTTED_NWORG)))?128:96,
    144,32,32
  );
  graf_decal(&g.graf,
    fldx+(int)player->nex-16,
    fldy+(int)player->ney-16,
    (engorge_extremities&&!(player->cutted&(CUTTED_NE|CUTTED_NEORG)))?128:96,
    144,32,32
  );
  int ax=fldx+(int)player->nwx+8;
  int bx=fldx+(int)player->nex-8;
  int y=fldy+(int)player->nwy+8;
  int stopy=fldy+(int)player->hy;
  int i=0;
  for (;y<stopy;ax+=12,bx-=12,y+=12,i++) {
    uint8_t tidl,tidr;
    if ((i==1)&&(player->cutted&CUTTED_NW)) tidl=0xba;
    else if ((i==0)&&engorge_vessels&&!(player->cutted&CUTTED_NW)) tidl=0xb7;
    else if (i==0) tidl=0xb6;
    else if (engorge_vessels&&!(player->cutted&CUTTED_NW)) tidl=0xb9;
    else tidl=0xb8;
    if ((i==1)&&(player->cutted&CUTTED_NE)) tidr=0xba;
    else if ((i==0)&&engorge_vessels&&!(player->cutted&CUTTED_NE)) tidr=0xb7;
    else if (i==0) tidr=0xb6;
    else if (engorge_vessels&&!(player->cutted&CUTTED_NE)) tidr=0xb9;
    else tidr=0xb8;
    graf_tile(&g.graf,ax,y,tidl,0);
    graf_tile(&g.graf,bx,y,tidr,EGG_XFORM_XREV);
  }
  graf_decal(&g.graf,
    fldx+(int)player->hx-24,
    fldy+(int)player->hy-24,
    engorge_heart?48:0,
    144,48,48
  );
  ax=fldx+(int)player->hx-14;
  bx=fldx+(int)player->hx+14;
  y=fldy+(int)player->hy+14;
  stopy=fldy+(int)player->swy;
  for (i=0;y<stopy;ax-=12,bx+=12,y+=12,i++) {
    uint8_t tidl,tidr;
    if ((i==1)&&(player->cutted&CUTTED_SW)) tidl=0xba;
    else if ((i==0)&&engorge_vessels&&!(player->cutted&CUTTED_SW)) tidl=0xb7;
    else if (i==0) tidl=0xb6;
    else if (engorge_vessels&&!(player->cutted&CUTTED_SW)) tidl=0xb9;
    else tidl=0xb8;
    if ((i==1)&&(player->cutted&CUTTED_SE)) tidr=0xba;
    else if ((i==0)&&engorge_vessels&&!(player->cutted&CUTTED_SE)) tidr=0xb7;
    else if (i==0) tidr=0xb6;
    else if (engorge_vessels&&!(player->cutted&CUTTED_SE)) tidr=0xb9;
    else tidr=0xb8;
    graf_tile(&g.graf,ax,y,tidl,EGG_XFORM_XREV);
    graf_tile(&g.graf,bx,y,tidr,0);
  }
  graf_decal(&g.graf,
    fldx+(int)player->swx-16,
    fldy+(int)player->swy-16,
    (engorge_extremities&&!(player->cutted&(CUTTED_SW|CUTTED_SWORG)))?128:96,
    144,32,32
  );
  graf_decal(&g.graf,
    fldx+(int)player->sex-16,
    fldy+(int)player->sey-16,
    (engorge_extremities&&!(player->cutted&(CUTTED_SE|CUTTED_SEORG)))?128:96,
    144,32,32
  );
  
  /* Below the scapel, a guide showing the rotation limits.
   */
  graf_set_filter(&g.graf,1);
  int sx=fldx+lround(player->x);
  int sy=fldy+lround(player->y);
  double guidet=(player->tlo+player->thi)*0.5;
  int8_t rot=(int8_t)((guidet*128.0)/M_PI); // NB signed, important for wasm and arm.
  graf_fancy(&g.graf,sx,sy,0x0f,0,rot,NS_sys_tilesize,0,0x808080ff);
  
  /* The scapel.
   */
  rot=(int8_t)((player->t*128.0)/M_PI);
  graf_fancy(&g.graf,sx,sy,0x0e,0,rot,NS_sys_tilesize,0,0x808080ff);
  graf_set_filter(&g.graf,0);
  
  /* If an organ has been touched, render the bloody mess.
   */
  if (player->cutted&CUTTED_BAD) {
    struct blood *blood=player->bloodv;
    int i=player->bloodc;
    for (;i-->0;blood++) {
      int x=fldx+(int)blood->x;
      int y=fldy+(int)blood->y;
      uint8_t tileid;
      switch (blood->animframe) {
        case 0: tileid=0x1e; break;
        case 1: tileid=0x1f; break;
        case 2: tileid=0x2d; break;
        case 3: tileid=0x2e; break;
        case 4: tileid=0x2f; break;
        case 5: tileid=0x2e; break;
        case 6: tileid=0x2d; break;
        case 7: tileid=0x1f; break;
      }
      graf_tile(&g.graf,x,y,tileid,0);
    }
  }
}

/* Render.
 */
 
static void _dissection_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x404040ff);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_dissection={
  .name="dissection",
  .objlen=sizeof(struct battle_dissection),
  .id=NS_battle_dissection,
  .strix_name=162,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz_a,
  .del=_dissection_del,
  .init=_dissection_init,
  .update=_dissection_update,
  .render=_dissection_render,
};

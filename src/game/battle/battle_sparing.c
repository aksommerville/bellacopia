/* battle_sparing.c
 */

#include "game/bellacopia.h"
#include <stdarg.h>

#define PIN_LIMIT 10
#define ANGLE_LO -1.000 /* rad */
#define ANGLE_HI  1.000 /* rad */
#define FORCE_LO  50.0 /* px/s */
#define FORCE_HI 200.0 /* px/s */

struct battle_sparing {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    int blackout;
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    int fldx,fldy,fldw,fldh; // Bounds of my lane in framebuffer pixels.
    double previewspeed; // rad/sec
    double ballr,pinr;
    double pindist2; // (pinr*2)**2
    double balldist2; // (pinr+ballr)**2
    double ttl; // If they don't commit all data points within so long, explode.
    double explode;
    double cpuwait;
    double cpuwaitlo,cpuwaithi;
    double dithertime; // Counts up while interactive. Ties break to least ditherful.
    
    struct pin {
      int present;
      int struck;
      int id; // 1..10, unique in this set.
      double x,y; // Field pixels.
      double dx,dy;
      double t;
      double dt;
    } pinv[PIN_LIMIT];
    int pinc;
    
    /* The inputs are expressed as three normalized floats:
     *  [0] signed position
     *  [1] signed angle
     *  [2] unsigned power
     */
    double datav[3];
    int datac;
    double preview;
    double previewp;
    
    /* The ball runs once (datac>=3).
     */
    double ballx,bally;
    double balldx,balldy;

  } playerv[2];
};

#define BATTLE ((struct battle_sparing*)battle)

/* Delete.
 */
 
static void _sparing_del(struct battle *battle) {
}

/* Generate pins.
 * Var args are int 1..10 and must terminate with a zero.
 */
 
static int player_generate_pins_array(struct battle *battle,struct player *player,const int *idv,int idc) {

  /* Pins will be arranged in 7 columns and 4 rows. (adjacent columns are never populated; they stagger).
   * Position this grid in the player's field first.
   * The row stride should be exactly double the column stride.
   */
  double xv[7],yv[4];
  double unit=(double)player->fldw/8.0;
  xv[0]=unit;
  int i;
  for (i=1;i<7;i++) xv[i]=xv[i-1]+unit;
  yv[0]=unit;
  for (i=1;i<4;i++) yv[i]=yv[i-1]+unit*2.0;
  
  for (;idc-->0;idv++) {
    int id=*idv;
    if ((id<1)||(id>10)) return -1;
    struct pin *pin=player->pinv;
    for (i=player->pinc;i-->0;pin++) {
      if (pin->id==id) return -1;
    }
    if (player->pinc>=PIN_LIMIT) return -1; // Not possible.
    pin=player->pinv+player->pinc++;
    pin->present=1;
    pin->struck=0;
    pin->id=id;
    pin->dx=pin->dy=pin->dt=pin->t=0.0;
    switch (id) {
      case 1: pin->x=xv[3]; pin->y=yv[3]; break;
      case 2: pin->x=xv[2]; pin->y=yv[2]; break;
      case 3: pin->x=xv[4]; pin->y=yv[2]; break;
      case 4: pin->x=xv[1]; pin->y=yv[1]; break;
      case 5: pin->x=xv[3]; pin->y=yv[1]; break;
      case 6: pin->x=xv[5]; pin->y=yv[1]; break;
      case 7: pin->x=xv[0]; pin->y=yv[0]; break;
      case 8: pin->x=xv[2]; pin->y=yv[0]; break;
      case 9: pin->x=xv[4]; pin->y=yv[0]; break;
      case 10:pin->x=xv[6]; pin->y=yv[0]; break;
      default: return -1;
    }
  }
  return 0;
}

static int player_generate_pins(struct battle *battle,struct player *player,...) {
  int idv[10];
  int idc=0;
  va_list vargs;
  va_start(vargs,player);
  for (;;) {
    int id=va_arg(vargs,int);
    if (!id) break;
    if (idc>=10) return -1;
    idv[idc++]=id;
  }
  return player_generate_pins_array(battle,player,idv,idc);
}

/* Generate pins with a count and column-span limit.
 */
 
static int player_generate_pins_anywhere(struct battle *battle,struct player *player,int count,int colspan) {
  if ((count<1)||(count>10)||(colspan<1)) return -1;
  int cola,colz;
  if (colspan<7) {
    cola=rand()%(7-colspan);
    colz=cola+colspan-1;
  } else {
    cola=0;
    colz=6;
  }
  int candidatev[10];
  int candidatec=0;
  if ((cola<=0)&&(colz>=0)) candidatev[candidatec++]=7;
  if ((cola<=1)&&(colz>=1)) candidatev[candidatec++]=4;
  if ((cola<=2)&&(colz>=2)) { candidatev[candidatec++]=8; candidatev[candidatec++]=2; }
  if ((cola<=3)&&(colz>=3)) { candidatev[candidatec++]=5; candidatev[candidatec++]=1; }
  if ((cola<=4)&&(colz>=4)) { candidatev[candidatec++]=9; candidatev[candidatec++]=3; }
  if ((cola<=5)&&(colz>=5)) candidatev[candidatec++]=6;
  if ((cola<=6)&&(colz>=6)) candidatev[candidatec++]=10;
  int idv[10];
  int idc=0;
  for (;idc<count;idc++) {
    if (candidatec<1) return -1; // Could happen if we asked for more pins than fit in the column span. (we don't do that, but it's fuzzy enough to make me worry)
    int candidatep=rand()%candidatec;
    idv[idc]=candidatev[candidatep];
    candidatec--;
    memmove(candidatev+candidatep,candidatev+candidatep+1,sizeof(int)*(candidatec-candidatep));
  }
  return player_generate_pins_array(battle,player,idv,idc);
}

/* Init player.
 */
 
static int player_init(struct battle *battle,struct player *player,int human,int face) {
  player->fldw=60;
  player->fldh=140;
  player->fldy=(FBH>>1)-(player->fldh>>1);
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->fldx=(FBW>>1)-10-player->fldw;
  } else { // Right.
    player->who=1;
    player->fldx=(FBW>>1)+10;
  }
  player->ttl=15.0;
  player->previewspeed=2.000*(1.0-player->skill)+3.000*player->skill;
  player->previewp=((rand()&0xffff)*M_PI*2.0)/65535.0; // Random initial data-collection position so the two sides don't look sync'd.
  player->ballr=8.0; // Can cheat the radii in or out per skill if we feel it helps. ...doesn't seem necessary.
  player->pinr=5.0;
  player->pindist2=player->pinr*player->pinr*4.0;
  player->balldist2=(player->pinr+player->ballr)*(player->pinr+player->ballr);
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpuwaitlo=0.500*player->skill+1.000*(1.0-player->skill);
    player->cpuwaithi=player->cpuwaitlo*2.0;
    player->cpuwait=(rand()&0xffff)/65535.0;
    player->cpuwait=player->cpuwaitlo*(1.0-player->cpuwait)+player->cpuwaithi*player->cpuwait;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xa5461dff;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
  
  /* Generate pins.
   */
  if (player->skill>=0.900) { // Easiest: One pin in position 1 or 5.
    return player_generate_pins(battle,player,(rand()&1)?1:5,0);
  } else if (player->skill>=0.800) { // Also very easy: one pin in the 8 other positions.
    int id=2+rand()%8;
    if (id>=5) id++;
    return player_generate_pins(battle,player,id,0);
  } else if (player->skill>=0.700) { // Two diagonally-adjacent pins.
    return player_generate_pins_anywhere(battle,player,2,2);
  } else if (player->skill>=0.600) { // Three, delta formation.
    return player_generate_pins_anywhere(battle,player,3,3);
  } else if (player->skill>=0.500) { // Four pins spanning no more than three columns. The usual case.
    return player_generate_pins_anywhere(battle,player,4,3);
  } else if (player->skill>=0.400) { // Five pins anywhere.
    return player_generate_pins_anywhere(battle,player,5,8);
  } else if (player->skill>=0.300) { // Six pins anywhere.
    return player_generate_pins_anywhere(battle,player,6,8);
  } else if (player->skill>=0.200) { // Seven pins anywhere.
    return player_generate_pins_anywhere(battle,player,7,8);
  } else if (player->skill>=0.100) { // Eight pins anywhere.
    return player_generate_pins_anywhere(battle,player,8,8);
  } else if (player->skill>=0.050) { // Four-Six.
    return player_generate_pins(battle,player,4,6,0);
  } else { // And finally the dreaded Seven-Ten. Bias 0xf3 or greater.
    return player_generate_pins(battle,player,7,10,0);
  }
}

/* New.
 */
 
static int _sparing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  if (player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface)<0) return -1;
  if (player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface)<0) return -1;
  return 0;
}

/* Done collecting data. Get the ball positioned for launch.
 */
 
static void player_ready(struct battle *battle,struct player *player) {

  // Position from datav[0]. Make sure we're doing it exactly the same as render.
  player->ballx=(player->datav[0]+1.0)*player->fldw*0.5;
  player->bally=player->fldh;
  
  // Angle from datav[1]. Initially a unit vector. Again, ensure we're doing it just like render.
  double t=(player->datav[1]+1.0)*(ANGLE_HI-ANGLE_LO)*0.5+ANGLE_LO;
  player->balldx=sin(t);
  player->balldy=-cos(t);
  
  // Scale that vector by force, datav[2].
  double force=FORCE_LO*(1.0-player->datav[2])+FORCE_HI*player->datav[2];
  player->balldx*=force;
  player->balldy*=force;
}

/* Check the player's ball against this one still pin.
 */
 
static void sparing_check_ball_collision(struct battle *battle,struct player *player,struct pin *pin) {
  double dx=pin->x-player->ballx;
  double dy=pin->y-player->bally;
  double d2=dx*dx+dy*dy;
  if (d2>=player->balldist2) return;
  double distance=sqrt(d2);
  double nx=dx/distance;
  double ny=dy/distance;
  double vel=nx*player->balldx+ny*player->balldy;
  if (vel<=0.0) return; // Shouldn't be possible for the ball to be travelling away from the collision but hey.
  vel*=1.25;
  pin->dx=nx*vel;
  pin->dy=ny*vel;
  // In real life, the ball's velocity is also impacted. Does that matter to us?
  pin->t=atan2(nx,-ny);
  pin->dt=pin->t*2.000;
  pin->struck=1;
  bm_sound_pan(RID_sound_whack,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Check this moving pin for collisions against still ones.
 */
 
static void sparing_check_pin_collisions(struct battle *battle,struct player *player,struct pin *pin) {
  struct pin *other=player->pinv;
  int i=player->pinc;
  for (;i-->0;other++) {
    if (other->struck) continue; // It can't get any strucker.
    if (other==pin) continue;
    double dx=other->x-pin->x;
    double dy=other->y-pin->y;
    double d2=dx*dx+dy*dy;
    if (d2>=player->pindist2) continue;
    double distance=sqrt(d2);
    double nx=dx/distance;
    double ny=dy/distance;
    double vel=nx*pin->dx+ny*pin->dy;
    if (vel<=0.0) continue;
    vel*=1.25;
    other->dx=nx*vel;
    other->dy=ny*vel;
    // In real life, the striking pin's velocity is also impacted. Does that matter to us?
    other->t=atan2(nx,-ny);
    other->dt=pin->t*2.000;
    other->struck=1;
    bm_sound_pan(RID_sound_tick,player->who?PLAYER_PAN:-PLAYER_PAN);
  }
}

/* Update player.
 */
 
static void player_update(struct battle *battle,struct player *player,double elapsed) {

  /* Once all data is collected, the physics play out on their own.
   */
  if (player->datac>=3) {
  
    // As kind of an emergency measure, force a minimum upward velocity. 10 px/s is painfully slow.
    if (player->balldy>-10.0) {
      fprintf(stderr,"%s:%d: Emergency velocity correction from %+f\n",__FILE__,__LINE__,player->balldy);
      player->balldy=-10.0;
    }
  
    // Apply ball velocity.
    player->ballx+=player->balldx*elapsed;
    player->bally+=player->balldy*elapsed;
    
    // If the ball touches our axis-aligned edges, it reflects with perfect elasticity. Bumper bowling; no gutters.
    const double edgew=8.0;
    if (player->ballx<edgew) {
      player->ballx=edgew;
      if (player->balldx<0.0) player->balldx=-player->balldx;
    } else if (player->ballx>player->fldw-edgew) {
      player->ballx=player->fldw-edgew;
      if (player->balldx>0.0) player->balldx=-player->balldx;
    }
    
    // Move any struck pins, and check for ball collision if unstruck.
    struct pin *pin=player->pinv;
    int i=player->pinc;
    for (;i-->0;pin++) {
      if (pin->struck) {
        pin->x+=pin->dx*elapsed;
        pin->y+=pin->dy*elapsed;
        pin->t+=pin->dt*elapsed;
        if (pin->t>M_PI) pin->t-=M_PI*2.0;
        else if (pin->t<-M_PI) pin->t+=M_PI*2.0;
        sparing_check_pin_collisions(battle,player,pin);
      } else {
        sparing_check_ball_collision(battle,player,pin);
      }
    }
    
    return;
  }
  
  /* TTL is only relevant when collecting input.
   * (you can finish the input with a millisecond left of TTL, and the automated part still plays out normally).
   */
  if (player->explode>0.0) {
    if ((player->explode-=elapsed)<=0.0) {
      player->bally=-100.0;
      player->balldy=-20.0;
      player->datac=3;
    }
    return;
  }
  player->ttl-=elapsed;
  if (player->ttl<=0.0) {
    player->explode=1.0;
    bm_sound_pan(RID_sound_bombblow,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  
  player->dithertime+=elapsed;
  
  /* Collecting data, we advance (previewp) monotonically, and normalize into (preview).
   */
  player->previewp+=player->previewspeed*elapsed;
  if (player->previewp>M_PI) player->previewp-=M_PI*2.0;
  player->preview=sin(player->previewp);
  if (player->datac==2) { // [2] is unsigned and upside-down. Let it bounce at the top.
    if (player->preview<0.0) player->preview=-player->preview;
    player->preview=1.0-player->preview;
  }
  
  /* If human, data is sampled on keystrokes.
   */
  if (player->human) {
    if (player->blackout) {
      if (!(g.input[player->human]&EGG_BTN_SOUTH)) player->blackout=0;
    } else if ((g.input[player->human]&EGG_BTN_SOUTH)&&!(g.pvinput[player->human]&EGG_BTN_SOUTH)) {
      bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->datav[player->datac++]=player->preview;
      if (player->datac>=3) player_ready(battle,player);
    }
  
  /* CPU, just press A three times with random waits before each.
   */
  } else {
    if (player->cpuwait>0.0) {
      player->cpuwait-=elapsed;
    } else {
      player->cpuwait=(rand()&0xffff)/65535.0;
      player->cpuwait=player->cpuwaitlo*(1.0-player->cpuwait)+player->cpuwaithi*player->cpuwait;
      bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->datav[player->datac++]=player->preview;
      if (player->datac>=3) player_ready(battle,player);
    }
  }
}

/* Count pins standing, for scoring purposes.
 */
 
static int sparing_count_pins(const struct battle *battle,const struct player *player) {
  int c=0;
  const struct pin *pin=player->pinv;
  int i=player->pinc;
  for (;i-->0;pin++) {
    if (!pin->struck) c++;
  }
  return c;
}

/* Update.
 */
 
static void _sparing_update(struct battle *battle,double elapsed) {
  
  /* Update both players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    player_update(battle,player,elapsed);
  }
  
  if (battle->outcome>-2) return;
  
  /* Game ends when both balls have left the screen.
   * It's guaranteed to happen eventually due to player TTL.
   * Fewest pins wins, but ties are likely, so we break them by dithertime.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if ((l->bally<-20.0)&&(r->bally<-20.0)) {
    int lc=sparing_count_pins(battle,l);
    int rc=sparing_count_pins(battle,r);
    if (lc<rc) battle->outcome=1;
    else if (lc>rc) battle->outcome=-1;
    else if (l->dithertime<r->dithertime) battle->outcome=1;
    else if (l->dithertime>r->dithertime) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render_bg(struct battle *battle,struct player *player) {
  graf_fill_rect(&g.graf,player->fldx,player->fldy,player->fldw,player->fldh,0x9c8255ff);
  int x=4;
  for (;x<player->fldw;x+=3) {
    graf_line(&g.graf,player->fldx+x,player->fldy,0x856f47ff,player->fldx+x,player->fldy+player->fldh,0x856f47ff);
  }
}

static void player_render_fg(struct battle *battle,struct player *player) {
  graf_set_image(&g.graf,RID_image_battle_desert);
  graf_set_filter(&g.graf,1);
  
  // Pins.
  const struct pin *pin=player->pinv;
  int i=player->pinc;
  for (;i-->0;pin++) {
    uint8_t tileid=0x01;
    uint8_t rot=0;
    if (pin->struck) {
      tileid=0x02;
      rot=(int8_t)((pin->t*128.0)/M_PI);
    }
    graf_fancy(&g.graf,player->fldx+(int)pin->x,player->fldy+(int)pin->y,tileid,0,rot,NS_sys_tilesize,0,player->color);
  }
  
  // Ball. Either in flight or in the preview space. Or don't render, if it crossed the end of the lane.
  int ballx,bally;
  if ((player->datac<3)||(player->bally>=0.0)) {
    if (player->datac>=3) {
      ballx=player->fldx+(int)player->ballx;
      bally=player->fldy+(int)player->bally;
    } else if (player->datac>0) {
      ballx=player->fldx+(int)((player->datav[0]+1.0)*player->fldw*0.5);
      bally=player->fldy+player->fldh;
    } else {
      ballx=player->fldx+(int)((player->preview+1.0)*player->fldw*0.5);
      bally=player->fldy+player->fldh;
    }
    if (player->explode>0.0) {
      graf_set_image(&g.graf,RID_image_hero);
      graf_decal(&g.graf,ballx-16,bally-16,160,160,32,32);
      graf_decal(&g.graf,ballx-16,bally-16,192,160,32,32);
      graf_set_image(&g.graf,RID_image_battle_desert);
    } else {
      graf_fancy(&g.graf,ballx,bally,0x00,0,0,NS_sys_tilesize,0,player->color);
    }
  }
  
  // Rotation input indicator. Keep it on while choosing power too.
  if ((player->datac==1)||(player->datac==2)) {
    double n=(player->datac==2)?player->datav[1]:player->preview;
    double t=(n+1.0)*(ANGLE_HI-ANGLE_LO)*0.5+ANGLE_LO;
    int ax=ballx+lround(sin(t)*8.0);
    int ay=bally+lround(cos(t)*-8.0);
    uint8_t rot=(int8_t)((t*128.0)/M_PI);
    graf_fancy(&g.graf,ax,ay,0x03,0,rot,NS_sys_tilesize,0,player->color);
  }
  graf_set_filter(&g.graf,0);
  
  // Power input indicator.
  if (player->datac==2) {
    int boxw=4;
    int boxh=30;
    int boxx=ballx-(boxw>>1);
    int boxy=bally-10-boxh;
    graf_fill_rect(&g.graf,boxx-1,boxy-1,boxw+2,boxh+2,0x000000ff);
    int fillh=(int)(player->preview*boxh);
    if (fillh>0) {
      if (fillh>boxh) fillh=boxh;
      uint32_t color=0xff0000ff;//TODO Maybe a gradient green..red?
      graf_fill_rect(&g.graf,boxx,boxy+boxh-fillh,boxw,fillh,color);
    }
  }
  
  // If my TTL is close to expiring, show a warning clock.
  if ((player->datac<3)&&(player->ttl>0.0)&&(player->ttl<5.0)) {
    int s=(int)(player->ttl+0.999);
    if (s<1) s=1; else if (s>9) s=9;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_tile(&g.graf,player->fldx+(player->fldw>>1),player->fldy+player->fldh+12,'0'+s,0);
  }
}

/* Render.
 */
 
static void _sparing_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  
  /* Do both backgrounds then both foregrounds.
   * So a wild pin that enters the opponent's airspace is always visible over their lane. Maybe under their sprites, meh.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  player_render_bg(battle,l);
  player_render_bg(battle,r);
  player_render_fg(battle,l);
  player_render_fg(battle,r);
  
  /* Once either player has committed, show who committed first.
   */
  if ((l->datac>=3)||(r->datac>=3)) {
    graf_set_image(&g.graf,RID_image_battle_desert);
    if (l->dithertime<r->dithertime) {
      graf_tile(&g.graf,FBW>>1,FBH>>1,0x04,EGG_XFORM_XREV);
    } else if (l->dithertime>r->dithertime) {
      graf_tile(&g.graf,FBW>>1,FBH>>1,0x04,0);
    }
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_sparing={
  .name="sparing",
  .objlen=sizeof(struct battle_sparing),
  .id=NS_battle_sparing,
  .strix_name=286,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .del=_sparing_del,
  .init=_sparing_init,
  .update=_sparing_update,
  .render=_sparing_render,
};

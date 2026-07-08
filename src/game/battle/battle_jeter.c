/* battle_jeter.c
 * Spin the ballerina then throw her into the pillows; try not to throw her into the briar patch.
 */

#include "game/bellacopia.h"

#define SWING_RATE 6.000 /* rad/sec */
#define MAG_PENALTY   0.500
#define MAG_DECAY     0.100
#define MAG_REINFORCE 0.400
#define GRAVITY       130.0 /* px/s**2 */
#define GRAVITY_LIMIT 200.0 /* px/s */

struct battle_jeter {
  struct battle hdr;
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    int face;
    double skill; // 0..1, reverse of each other.
    uint8_t tileid_body; // NW corner of a 2x3. Static.
    uint8_t tileid_arm; // NW corner of a 2x2. Rotates.
    double t; // Effective angle of arms and ballerina. Zero is down.
    double swingp; // Phase of the swing expressed as a perfect circle. Constant frequency.
    double swingmag; // Magnitude of swing, 0..1.
    int input; // -1,0,1 = clockwise,neutral,counterclockwise or left,neutral,right. Controllers set this.
    int heave; // Controller sets nonzero when we're ready to throw the ballerina.
    int heft; // Permanent, if nonzero the ballerina is in flight.
    int landed;
    double balx,baly; // Ballerina's position in frame pixels.
    double baldx,baldy;
    int distance; // mm, can be negative, starts tabulating when (heft) and final when (landed).
    double xscroll; // Only in play after a negative throw.
    double heavelo,heavehi; // CPU player decides at init at what phase it will throw. Between -pi and -pi/2, to throw the right way.
  } playerv[2];
};

#define BATTLE ((struct battle_jeter*)battle)

/* Delete.
 */
 
static void _jeter_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left, ie top.
    player->who=0;
  } else { // Right, ie bottom.
    player->who=1;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    double target=M_PI*-0.750;
    double miss=(rand()&0xffff)/65535.0;
    miss*=1.0-player->skill;
    if (rand()&1) miss=-miss;
    miss*=M_PI*0.125;
    player->heavelo=target+miss-0.080;
    player->heavehi=target+miss+0.080;
  }
  switch (player->face=face) {
    case NS_face_monster: {
        player->tileid_body=0xdb;
        player->tileid_arm=0xed;
      } break;
    case NS_face_dot: {
        player->tileid_body=0xd7;
        player->tileid_arm=0xad;
      } break;
    case NS_face_princess: {
        player->tileid_body=0xd9;
        player->tileid_arm=0xcd;
      } break;
  }
  player->swingmag=0.250;
}

/* New.
 */
 
static int _jeter_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playclock=10.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->input=-1; break;
    case EGG_BTN_RIGHT: player->input=1; break;
    default: player->input=0; break;
  }
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    player->heave=1;
  } else {
    player->heave=0;
  }
}

/* Update CPU player.
 * I think the CPU can be perfect. Skill is already baked into the toss velocity, so we shouldn't need it here.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->swingp>-M_PI*0.5)&&(player->swingp<M_PI*0.5)) {
    player->input=-1;
  } else {
    player->input=1;
    if ((player->swingmag>=0.900)&&(player->swingp>=player->heavelo)&&(player->swingp<=player->heavehi)) {
      player->heave=1;
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Landed already? Only thing we do is scroll left if required.
  if (player->landed) {
    if (player->distance<-1000) {
      if ((player->xscroll-=100.0*elapsed)<=-220.0) player->xscroll=-220.0;
    }
    return;
  }
  
  // Otherwise, no updating after finish.
  if (battle->outcome>-2) return;

  // If already heft, advance the ballerina.
  if (player->heft) {
    if ((player->baldy+=GRAVITY*elapsed)>GRAVITY_LIMIT) player->baldy=GRAVITY_LIMIT;
    player->balx+=player->baldx*elapsed;
    if ((player->baly+=player->baldy*elapsed)>=70.0) {
      if (player->baldy>0.0) {
        player->landed=1;
        player->baly=70.0;
      }
    }
    player->distance=(int)((player->balx-56.0)*31.25); // Arbitrarily declaring 1 m = 32 px.
    return;
  }

  // The ol heave-ho?
  // Ballerina's initial velocity is exactly perpendicular to the swing, in a direction determined the same way as (agreement).
  // And its magnitude is derived from (swingmag) and (skill).
  if (player->heave) {
    player->heft=1;
    double rmax=player->swingmag*M_PI*0.500;
    double t=sin(player->swingp)*rmax;
    double cost=cos(t);
    double sint=sin(t);
    double np=player->swingp+M_PI*0.5;
    if (np>M_PI) np-=M_PI*2.0;
    const double bradius=10.0; // Agree with render.
    player->balx=56.0-sint*bradius;
    player->baly=75.0-16.0+cost*bradius;
    if (np>0.0) {
      player->baldx=-cost;
      player->baldy=-sint;
    } else {
      player->baldx=cost;
      player->baldy=sint;
    }
    double bvmax=180.0*player->skill+120.0*(1.0-player->skill);
    double bvmin=bvmax*0.250;
    double balvel=bvmin*(1.0-player->swingmag)+bvmax*player->swingmag;
    player->baldx*=balvel;
    player->baldy*=balvel;
    return;
  }
  
  // Swing position advances monotonically.
  player->swingp+=SWING_RATE*elapsed;
  if (player->swingp>=M_PI) player->swingp-=M_PI*2.0;
  
  // Determine bounds of the effective rotation.
  if (player->swingmag>0.0) {
    double rmax=player->swingmag*M_PI*0.500;
    player->t=sin(player->swingp)*rmax;
  } else {
    player->t=0.0;
  }
  
  // Does the input direction agree with the direction of motion?
  int agreement=0; // >0=reinforce, <0=diminish, 0=decay
  if (player->input) {
    // If the current magnitude is very low, player can't be expected to track it (eg at zero!). So below some threshold, call it good always.
    if (player->swingmag<0.125) {
      agreement=1;
    } else {
      // Rotate (swingp) a quarter turn.
      double np=player->swingp+M_PI*0.5;
      if (np>M_PI) np-=M_PI*2.0;
      // Negative (np) goes to the right and positive to the left.
      if (np<0.0) agreement=player->input>0;
      else agreement=player->input<0;
      if (!agreement) agreement=-1;
    }
  }
  
  // Adjust (swingmag) per (agreement).
  if (agreement<0) {
    if ((player->swingmag-=MAG_PENALTY*elapsed)<0.0) player->swingmag=0.0;
  } else if (!agreement) {
    if ((player->swingmag-=MAG_DECAY*elapsed)<0.0) player->swingmag=0.0;
  } else {
    if ((player->swingmag+=MAG_REINFORCE*elapsed)>1.0) player->swingmag=1.0;
  }
}

/* Update.
 */
 
static void _jeter_update(struct battle *battle,double elapsed) {
  
  // (playclock) stops ticking once both players heave, it's all automatic from then on, no sense timing out.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (!l->heft||!r->heft) {
    BATTLE->playclock-=elapsed;
  }
  
  // Update players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (battle->outcome==-2) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
    player_update_common(battle,player,elapsed);
  }
  
  // Game ends when both players have landed, or the playclock expires with one unhefted.
  // Ties are possible but unlikely when they both play. Trivial to tie, if both players do nothing.
  // If one player refuses to heave, the other wins, even if they went negative.
  if (battle->outcome==-2) {
    if (l->landed&&r->landed) {
      if (l->distance>r->distance) battle->outcome=1;
      else if (l->distance<r->distance) battle->outcome=-1;
      else battle->outcome=0;
    } else if ((BATTLE->playclock<0.0)&&(!l->heft||!r->heft)) {
      if (l->heft) battle->outcome=1;
      else if (r->heft) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static void player_render_bg(struct battle *battle,struct player *player,int fldy) {
  const int fldh=FBH>>1;
  const int groundy=75;
  graf_fill_rect(&g.graf,0,fldy,FBW,fldh,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,fldy+groundy,FBW,fldh-groundy,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,fldy+groundy,FBW,1,0x000000ff);
}
 
static void player_render_fg(struct battle *battle,struct player *player,int fldy) {
  const int fldh=FBH>>1;
  const int groundy=75;
  int srcx=(player->tileid_body&0x0f)*NS_sys_tilesize;
  int srcy=(player->tileid_body>>4)*NS_sys_tilesize;
  int scrollx=0;
  scrollx-=(int)player->xscroll;
  graf_decal(&g.graf,scrollx+40,fldy+groundy-47,srcx,srcy,32,48);
  
  // Once heft, the arms are static, upwardish, and the ballerina flies on her own.
  int armlx=scrollx;
  int armly=fldy+groundy;
  int armrx=scrollx;
  int armry=fldy+groundy;
  if (player->face==NS_face_monster) {
    armlx+=54;
    armrx+=63;
    armly-=18;
    armry-=19;
  } else {
    armlx+=56;
    armrx+=64;
    armly-=18;
    armry-=18;
  }
  int armsrcx=(player->tileid_arm&0x0f)*NS_sys_tilesize;
  int armsrcy=(player->tileid_arm>>4)*NS_sys_tilesize;
  if (player->heft) {
    const double ROOT_TWO_OVER_TWO=0.7071067811865476;
    graf_set_filter(&g.graf,1);
    graf_decal_rotate(&g.graf,armlx,armly,armsrcx,armsrcy,NS_sys_tilesize*2,ROOT_TWO_OVER_TWO,-ROOT_TWO_OVER_TWO,1.0);
    graf_decal_rotate(&g.graf,armrx,armry,armsrcx,armsrcy,NS_sys_tilesize*2,-ROOT_TWO_OVER_TWO,-ROOT_TWO_OVER_TWO,1.0);
    graf_set_filter(&g.graf,0);
    if (player->landed) {
      if (player->distance<=-1000) {
        srcx=0;
        srcy=14*NS_sys_tilesize;
      } else {
        srcx=1*NS_sys_tilesize;
        srcy=14*NS_sys_tilesize;
      }
    } else {
      srcx=3*NS_sys_tilesize;
      srcy=14*NS_sys_tilesize;
    }
    int dstx=scrollx+(int)player->balx-NS_sys_tilesize;
    int dsty=fldy+(int)player->baly-NS_sys_tilesize;
    if (!srcx) { // Angry face in the briar patch. Single column.
      dstx+=NS_sys_tilesize>>1;
      dsty-=11;
      graf_decal(&g.graf,dstx,dsty,srcx,srcy,16,32);
    } else { // Normal 2x2 faces.
      graf_decal(&g.graf,dstx,dsty,srcx,srcy,32,32);
    }
  
  } else {
    double cost=cos(player->t);
    double sint=sin(player->t);
    double bradius=10.0;
    int dstx,dsty;
    // Arms:
    graf_set_filter(&g.graf,1);
    graf_decal_rotate(&g.graf,armlx,armly,armsrcx,armsrcy,NS_sys_tilesize*2,sint,cost,1.0);
    graf_decal_rotate(&g.graf,armrx,armry,armsrcx,armsrcy,NS_sys_tilesize*2,sint,cost,1.0);
    graf_set_filter(&g.graf,0);
    // Ballerina, always the same frame:
    srcx=5*NS_sys_tilesize;
    srcy=14*NS_sys_tilesize;
    dstx=scrollx+56-(int)(sint*bradius);
    dsty=fldy+groundy-16+(int)(cost*bradius);
    graf_decal(&g.graf,dstx-16,dsty-16,srcx,srcy,32,32);
  }
  
  // Briar patch, if we've scrolled left.
  if (scrollx>0) {
    int dsty=fldy+groundy-7;
    int dstx=scrollx-10;
    uint8_t tileid=0xd6;
    for (;dstx>=-8;dstx-=NS_sys_tilesize) {
      graf_tile(&g.graf,dstx,dsty,tileid,0);
      tileid=0xd5;
    }
  }
}

static void player_render_distance(struct battle *battle,struct player *player,int fldy) {
  const int fldh=FBH>>1;
  int mm=player->distance;
  int dstx=85-(int)player->xscroll;
  int dsty=fldy+fldh-7;
  int positive=1;
  if (mm<0) { positive=0; mm=-mm; }
  if (mm>99999) mm=99999;
  graf_tile(&g.graf,dstx,dsty,'m',0); dstx-=16;
  graf_tile(&g.graf,dstx,dsty,'0'+(mm%10),0); dstx-=8;
  graf_tile(&g.graf,dstx,dsty,'0'+((mm/10)%10),0); dstx-=8;
  graf_tile(&g.graf,dstx,dsty,'0'+((mm/100)%10),0); dstx-=8;
  graf_tile(&g.graf,dstx,dsty,'.',0); dstx-=8;
  graf_tile(&g.graf,dstx,dsty,'0'+((mm/1000)%10),0); dstx-=8;
  if (mm>=10000) { graf_tile(&g.graf,dstx,dsty,'0'+((mm/10000)%10),0); dstx-=8; }
  if (!positive) graf_tile(&g.graf,dstx,dsty,'-',0);
}

/* Render.
 */
 
static void _jeter_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  /* I wanted to keep the player renders batchable, but we need to cheat a little.
   * Do bg and fg for the bottom player first, since ballerinas can exceed the top edge.
   */
  player_render_bg(battle,r,FBH>>1);
  graf_set_image(&g.graf,RID_image_battle_athletes);
  player_render_fg(battle,r,FBH>>1);
  graf_set_input(&g.graf,0);
  player_render_bg(battle,l,0);
  graf_set_image(&g.graf,RID_image_battle_athletes);
  player_render_fg(battle,l,0);
  /* Distance for any player that heaved.
   */
  if (l->heft||r->heft) {
    graf_set_image(&g.graf,RID_image_fonttiles);
    if (l->heft) player_render_distance(battle,l,0);
    if (r->heft) player_render_distance(battle,r,FBH>>1);
  }
  /* Playclock if one player hasn't heft yet.
   * Once they both heave, the clock is no longer in play.
   * Time will always be one digit.
   */
  if ((!l->heft||!r->heft)&&(BATTLE->playclock>0.0)) {
    int s=(int)(BATTLE->playclock+1.0);
    if (s<1) s=1; else if (s>9) s=9;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_tile(&g.graf,FBW>>1,FBH>>1,'0'+s,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_jeter={
  .name="jeter",
  .objlen=sizeof(struct battle_jeter),
  .id=NS_battle_jeter,
  .strix_name=160,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_jeter_del,
  .init=_jeter_init,
  .update=_jeter_update,
  .render=_jeter_render,
};

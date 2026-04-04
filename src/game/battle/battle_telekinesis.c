/* battle_telekinesis.c
 * Use your psychic power to tip the bottle of Old Janx Spirit into your opponent's glass.
 */

#include "game/bellacopia.h"

#define GROUNDY 160

struct battle_telekinesis {
  struct battle hdr;
  int choice;
  double wobblephase; // Radians. It advances faster according to the sum of players' power.
  double wobble; // Momentary wobble, -1..1.
  double victory_distance; // One score must be so far ahead of the other to win. Decreases over time.
  double preview; // -1..1, snapshot of current score, for populating the scales.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    int srcx,srcy; // For the single 32x48 body frame. Arm is immediately below.
    uint8_t xform;
    int x,y; // Top left of body decal.
    int shoulder; // Absolute framebuffer vertical, where arms center.
    double target; // Signed radians.
    double targetd; // rad/sec
    uint8_t input; // Direction: 0x40,0x08,0x02,0x10. Or zero if nothing held.
    double power; // 0..1, how in tune to the spirits are we? Extremely volatile.
    double score; // Arbitrary scale, running integral of (power). Only meaningful in comparison to the other guy's.
    double error_range; // Radians, maximum error for CPU player's guess.
    double errort;
    double changeclock; // CPU players hold their input state for a certain interval.
    double armphase;
    
    // Precalculated output positions for the player's zodiac calendar:
    struct sign {
      int x,y;
      uint8_t tileid;
      double t;
    } signv[12];
    int zx,zy; // ...and the center of that circle.
    
  } playerv[2];
};

#define BATTLE ((struct battle_telekinesis*)battle)

/* Delete.
 */
 
static void _telekinesis_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->y=GROUNDY-47;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=(FBW>>1)-32-32;
    player->targetd=3.000;//TODO vary?
  } else { // Right.
    player->who=1;
    player->x=(FBW>>1)+32;
    player->xform=EGG_XFORM_XREV;
    player->targetd=-3.000;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    player->error_range=(1.0-player->skill)*3.0;
  }
  switch (face) {
    case NS_face_monster: {
        player->srcx=64;
        player->srcy=192;
        player->shoulder=player->y+26;
      } break;
    case NS_face_dot: {
        player->srcx=0;
        player->srcy=192;
        player->shoulder=player->y+31;
      } break;
    case NS_face_princess: {
        player->srcx=32;
        player->srcy=192;
        player->shoulder=player->y+31;
      } break;
  }
  player->target=(((rand()&0xffff)-32768)*M_PI)/32768.0;
  
  /* Precalculate the zodiac calendar's positions, so we're not doing it every render.
   */
  double midx=player->x+16.0;
  if (player->who) midx+=40.0; else midx-=40.0;
  double midy=player->y-46.0;
  player->zx=(int)midx;
  player->zy=(int)midy;
  const double radius=30.0;
  double t=0.0;
  double vt=M_PI/4.0;
  double dt=(M_PI*2.0)/12.0;
  if (player->who) dt=-dt;
  int i=12,tilep=0;
  struct sign *sign=player->signv;
  for (;i-->0;sign++,t+=dt,vt+=dt,tilep++) {
    if (t>M_PI) t-=M_PI*2.0; else if (t<-M_PI) t+=M_PI*2.0;
    if (vt>M_PI) vt-=M_PI*2.0; else if (vt<-M_PI) vt+=M_PI*2.0;
    double sint=sin(vt);
    double cost=cos(vt);
    sign->x=(int)(midx+sint*radius-cost*radius);
    sign->y=(int)(midy-cost*radius-sint*radius);
    sign->t=t;
    sign->tileid=0xea+(tilep/6)*16+tilep%6;
  }
}

/* New.
 */
 
static int _telekinesis_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->victory_distance=10.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_LEFT: player->input=0x10; break;
    case EGG_BTN_RIGHT: player->input=0x08; break;
    case EGG_BTN_UP: player->input=0x40; break;
    case EGG_BTN_DOWN: player->input=0x02; break;
    default: player->input=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  player->errort+=elapsed*6.0;
  if (player->errort>M_PI) player->errort-=M_PI*2.0;
  double error=sin(player->errort)*player->error_range;

  if (player->changeclock>0.0) {
    player->changeclock-=elapsed;
    return;
  }

  uint8_t nstate;
  double guess=player->target+error;
  if (guess>M_PI) guess-=M_PI*2.0;
  if (guess<-M_PI) guess+=M_PI*2.0;
  if (guess<M_PI*-0.750) nstate=0x02;
  else if (guess<M_PI*-0.250) nstate=0x10;
  else if (guess<M_PI*0.250) nstate=0x40;
  else if (guess<M_PI*0.750) nstate=0x08;
  else nstate=0x02;
  
  if (nstate!=player->input) {
    player->input=nstate;
    player->changeclock=0.250;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Rotate the target.
   */
  player->target+=player->targetd*elapsed;
  if (player->target<-M_PI) player->target+=M_PI*2.0;
  else if (player->target>M_PI) player->target-=M_PI*2.0;
  
  /* Rate the player's input against current target.
   * It's automatically the worst if no input provided. You snooze, you lose.
   */
  player->power=0.0;
  if (player->input) {
    double guess=0.0;
    switch (player->input) {
      case 0x40: guess=0.0; break;
      case 0x08: guess=M_PI*0.5; break;
      case 0x02: guess=M_PI; break;
      case 0x10: guess=M_PI*-0.5; break;
    }
    double d=guess-player->target;
    while (d<-M_PI) d+=M_PI*2.0;
    while (d>M_PI) d-=M_PI*2.0;
    if (d<0.0) d=-d;
    player->power=1.0-d/M_PI;
    player->score+=player->power*elapsed;
  }
  
  /* Advance decorative armphase, faster when more power.
   */
  player->armphase+=player->power*elapsed*20.0;
  if (player->armphase>M_PI) player->armphase-=M_PI*2.0;
}

/* Update.
 */
 
static void _telekinesis_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Wobble the bottle.
   */
  double power=(BATTLE->playerv[0].power+BATTLE->playerv[1].power)*0.5;
  BATTLE->wobblephase+=power*elapsed*15.0;
  if (BATTLE->wobblephase>M_PI) BATTLE->wobblephase-=M_PI*2.0;
  BATTLE->wobble=sin(BATTLE->wobblephase)*power;
  
  /* Check completion.
   * Scores can be thought of as seconds, it's seconds at full power.
   */
  if ((BATTLE->victory_distance-=elapsed)<=0.0) BATTLE->victory_distance=0.001;
  double scdiff=BATTLE->playerv[0].score-BATTLE->playerv[1].score;
  if (scdiff>=BATTLE->victory_distance) battle->outcome=1;
  else if (scdiff<=-BATTLE->victory_distance) battle->outcome=-1;
  
  /* Similar to completion, set the preview.
   */
  BATTLE->preview=scdiff/BATTLE->victory_distance;
  if (BATTLE->preview<-1.0) BATTLE->preview=-1.0;
  else if (BATTLE->preview>1.0) BATTLE->preview=1.0;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  // Arm positions, and render the back one.
  int armby=player->shoulder-8-4;
  int armfy=player->shoulder-8;
  int armbx=player->x;
  int armfx=player->x;
  if (player->xform) {
    armbx-=8;
    armfx-=6;
  } else {
    armbx+=8;
    armfx+=6;
  }
  int armrange=lround(player->power*3.0);
  if (armrange>0) {
    int armd=lround(sin(player->armphase)*armrange);
    armby+=armd;
    armfy-=armd;
  }
  graf_decal_xform(&g.graf,armbx,armby,player->srcx,player->srcy+48,32,16,player->xform);
  
  // Body.
  graf_decal_xform(&g.graf,player->x,player->y,player->srcx,player->srcy,32,48,player->xform);
  
  // Front arm.
  graf_decal_xform(&g.graf,armfx,armfy,player->srcx,player->srcy+48,32,16,player->xform);
}

/* Render one player's target wheel and input state.
 */
 
static void zodiac_render(struct battle *battle,struct player *player) {
  const double highlight_distance=0.250; // units
  struct sign *sign=player->signv;
  int i=12;
  for (;i-->0;sign++) {
    uint32_t tint=0x0080ff00;
    double dt=player->target-sign->t;
    while (dt<-M_PI) dt+=M_PI*2.0;
    while (dt>M_PI) dt-=M_PI*2.0;
    dt/=M_PI;
    if ((dt>-highlight_distance)&&(dt<highlight_distance)) {
      // The tile's natural color is our default. So we tint toward a constant and only have to touch the tint alpha.
      dt/=highlight_distance;
      if (dt<0.0) dt=-dt;
      int alpha=(int)((1.0-dt)*256.0);
      if (alpha<0) alpha=0; else if (alpha>0xff) alpha=0xff;
      tint|=alpha;
    }
    graf_fancy(&g.graf,sign->x,sign->y,sign->tileid,0,0,NS_sys_tilesize,tint,0x808080ff);
  }
  
  // Star indicating input state.
  int starx=player->zx,stary=player->zy;
  uint8_t startile=0xe9;
  switch (player->input) {
    case 0x40: stary-=20; break;
    case 0x08: starx+=20; break;
    case 0x02: stary+=20; break;
    case 0x10: starx-=20; break;
    default: startile=0;
  }
  if (startile) {
    uint32_t tint;
    if (player->power>=0.5) {
      int g=0x80+(int)((player->power-0.5)*256.0);
      if (g>0xff) g=0xff;
      tint=(g<<16)|0xff;
    } else {
      tint=0xff0000ff;
    }
    graf_fancy(&g.graf,starx,stary,startile,0,0,NS_sys_tilesize,0,tint);
  }
}

/* Render.
 */
 
static void _telekinesis_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x805020ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH,0x402010ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_labyrinth4);
  
  int midx=FBW>>1;
  graf_decal(&g.graf,midx-24,GROUNDY-18,96,220,48,19);
  
  if (battle->outcome==1) {
    graf_tile(&g.graf,midx,GROUNDY-25,0xf7,0);
  } else if (battle->outcome==-1) {
    graf_tile(&g.graf,midx,GROUNDY-25,0xf7,EGG_XFORM_XREV);
  } else if (battle->outcome==0) {
    graf_tile(&g.graf,midx,GROUNDY-25,0xf6,0);
  } else {
    int rot=(int)(BATTLE->wobble*20.0);
    double scsum=BATTLE->playerv[0].score+BATTLE->playerv[1].score;
    if (scsum>0.0) {
      double tip=(BATTLE->playerv[0].score-BATTLE->playerv[1].score)/scsum;
      rot+=(int)(tip*20.0);
    }
    graf_fancy(&g.graf,midx,GROUNDY-25,0xf6,0,(int8_t)rot,NS_sys_tilesize,0,0x808080ff);
  }
  
  uint8_t cupl=0xf8,cupr=0xf8;
  if (battle->outcome==1) cupr+=1;
  else if (battle->outcome==-1) cupl+=1;
  graf_tile(&g.graf,midx-10,GROUNDY-25,cupl,0);
  graf_tile(&g.graf,midx+10,GROUNDY-25,cupr,0);
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  if (battle->outcome==-2) {
    zodiac_render(battle,BATTLE->playerv+0);
    zodiac_render(battle,BATTLE->playerv+1);
  }
  
  // A scale in the middle showing preview.
  int dy=lround(BATTLE->preview*11.0);
  graf_tile(&g.graf,(FBW>>1)-9,42+dy,0xd9,0);
  graf_tile(&g.graf,(FBW>>1)+9,42-dy,0xd9,EGG_XFORM_XREV);
  graf_tile(&g.graf,FBW>>1,30,0xca,0);
  graf_tile(&g.graf,FBW>>1,46,0xda,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_telekinesis={
  .name="telekinesis",
  .objlen=sizeof(struct battle_telekinesis),
  .strix_name=173,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_telekinesis_del,
  .init=_telekinesis_init,
  .update=_telekinesis_update,
  .render=_telekinesis_render,
};

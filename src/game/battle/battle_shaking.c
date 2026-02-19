/* battle_shaking.c
 * U/D to shake the bottle until time runs out.
 */

#include "game/bellacopia.h"

#define GROUNDY 150
#define GRAVITY 200.0
#define CPUTIME_WORST 0.090 /* cputime is the time between input state changes; it does pass thru zero between the significant states. */
#define CPUTIME_BEST  0.050

struct battle_shaking {
  struct battle hdr;
  double clock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    int indy;
    uint8_t tileid;
    uint8_t xform;
    int arm; // -1,0,1 like indy but official.
    int strokec; // Count of changes to a nonzero (arm).
    int lc,rc; // Count of accidental left and right strokes. I might use these against you.
    double cx,cy; // Cork position, framebuffer pixels.
    double cdx,cdy;
    int done; // Cork has returned to earth.
    double peak;
    double cpuclock;
    double cputime;
    int nextdy;
  } playerv[2];
};

#define BATTLE ((struct battle_shaking*)battle)

/* Delete.
 */
 
static void _shaking_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cputime=CPUTIME_WORST*(1.0-player->skill)+CPUTIME_BEST*player->skill;
    player->cpuclock=player->cputime*5.0; // Wee initial penalty.
    player->nextdy=(rand()&1)?1:-1;
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x73;
      } break;
    case NS_face_dot: {
        player->tileid=0x33;
      } break;
    case NS_face_princess: {
        player->tileid=0x53;
      } break;
  }
  player->peak=GROUNDY;
}

/* New.
 */
 
static int _shaking_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->clock=6.0;
  return 0;
}

/* Pop a cork.
 */
 
static void pop_cork(struct battle *battle,struct player *player) {
  if (player->who) {
    player->cx=FBW-120.0-12.0;
  } else {
    player->cx=120.0+12.0;
  }
  player->cy=GROUNDY-20.0;
  // Randomizing dx to an extent that really shouldn't matter much, but makes it look realistically chaotic on landing.
  // I thought about involving accidental Left and Right strokes here but meh, how likely even are they?
  double t=(((rand()&0xffff)-0x8000)*0.050)/32768.0;
  /* (mag==225) takes you right about to the top of the screen.
   * 60 seems about my limit for (strokec) under a 6-second clock.
   * But I'm a clumsy old man, so let's figure some whippersnapper out there can pull 70.
   * It's not a big deal if it goes offscreen, tho would be unfortunate if two players both go offscreen.
   * fwiw, the cpu player strikes 31..58 times.
   */
  double mag=(player->strokec*225.0)/70.0;
  player->cdx=sin(t)*mag;
  player->cdy=-cos(t)*mag;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player->indy=-1; break;
    case EGG_BTN_DOWN: player->indy=1; break;
    default: player->indy=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->cpuclock-=elapsed)<=0.0) {
    player->cpuclock+=player->cputime;
    if (player->indy) {
      player->indy=0;
    } else {
      player->indy=player->nextdy;
      if (player->nextdy==-1) player->nextdy=1; else player->nextdy=-1;
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->indy) {
    if (player->indy!=player->arm) {
      player->arm=player->indy;
      player->strokec++;
    }
  } else if (player->arm) {
    player->arm=0;
  }
}

/* Update player, after pop.
 */
 
static void player_update_post(struct battle *battle,struct player *player,double elapsed) {
  if (player->done) return;
  player->cdy+=GRAVITY*elapsed;
  player->cx+=player->cdx*elapsed;
  player->cy+=player->cdy*elapsed;
  if (player->cy<player->peak) player->peak=player->cy;
  if (player->cy>=GROUNDY) {
    player->cy=GROUNDY;
    player->cdy*=-0.500;
    if (player->cdy>-10.0) player->done=1;
  }
}

/* Update.
 */
 
static void _shaking_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if (BATTLE->clock>0.0) {
    if ((BATTLE->clock-=elapsed)<=0.0) {
      bm_sound(RID_sound_pop);
      pop_cork(battle,BATTLE->playerv+0);
      pop_cork(battle,BATTLE->playerv+1);
    }
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (BATTLE->clock>0.0) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
      else player_update_cpu(battle,player,elapsed);
      player_update_common(battle,player,elapsed);
    } else {
      player_update_post(battle,player,elapsed);
    }
  }
  
  // Declare winner after both corks stop. Ties are so vanishingly unlikely that we just break them left, whatever.
  if (BATTLE->playerv[0].done&&BATTLE->playerv[1].done) {
    double ly=BATTLE->playerv[0].peak;
    double ry=BATTLE->playerv[1].peak;
    if (ly<=ry) battle->outcome=1;
    else battle->outcome=-1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int ht=NS_sys_tilesize>>1;
  int x=120;
  if (player->who) x=FBW-x;
  int y=GROUNDY-NS_sys_tilesize-(NS_sys_tilesize>>1)+1;
  int armx=x;
  if (player->xform) armx-=ht; else armx+=ht;
  int army=y+ht+3+player->arm;
  uint8_t armtile=player->tileid+0x0e; // 0x0f after popped
  if (BATTLE->clock<=0.0) armtile+=1;
  graf_tile(&g.graf,armx,army,armtile,player->xform);
  graf_tile(&g.graf,x,y,player->tileid,player->xform);
  graf_tile(&g.graf,x,y+NS_sys_tilesize,player->tileid+0x10,player->xform);
  if (BATTLE->clock<=0.0) { // Cork.
    graf_tile(&g.graf,(int)player->cx,(int)player->cy,0xa7,0);
  }
  int finger=(int)player->peak;
  if (finger>army) finger=army;
  int fingx=FBW>>1;
  uint8_t fingxform=0;
  if (player->who) fingx+=8;
  else { fingx-=8; fingxform=EGG_XFORM_XREV; }
  graf_tile(&g.graf,fingx,finger,0xa9,fingxform);
}

/* Render.
 */
 
static void _shaking_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0xa0d0f0ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,0x206030ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_fractia);
  
  // Ruler down the middle.
  int y=GROUNDY-(NS_sys_tilesize>>1);
  for (;y>=-8;y-=NS_sys_tilesize) {
    graf_tile(&g.graf,FBW>>1,y,0xa8,0);
  }
  
  // Players, including cork.
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // Clock.
  if (BATTLE->clock>0) {
    int sec=(int)(BATTLE->clock)+1;
    if (sec>9) sec=9; else if (sec<0) sec=0;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_tile(&g.graf,FBW>>1,GROUNDY+20,'0'+sec,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_shaking={
  .name="shaking",
  .objlen=sizeof(struct battle_shaking),
  .strix_name=153,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_shaking_del,
  .init=_shaking_init,
  .update=_shaking_update,
  .render=_shaking_render,
};

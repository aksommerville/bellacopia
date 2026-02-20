/* battle_armwrestling.c
 * Karate chop contest skinned as arm wrestling against a Minotaur.
 */

#include "game/bellacopia.h"

#define STROKEMIN_WORST 0.280
#define STROKEMIN_BEST  0.280
#define DECAY_WORST     0.500
#define DECAY_BEST      0.500
#define CPUTIME_WORST   0.110
#define CPUTIME_BEST    0.080
#define CPU_HEADSTART   0.500
#define CPU_WEAR_OUT    0.003 /* CPU's tap clock increases continuously. If you last long enough, you can win. */

struct battle_armwrestling {
  struct battle hdr;
  double balance; // -1..0..1 = left lose, neutral, right lose; same as (outcome) but continuous.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid;
    double power; // 0..1, generic. How much input power total.
    double strokeclock; // Counts up always, and resets on each stroke.
    double strokemin; // Constant; the interval at which a stroke's power is zero.
    double decay; // Constant; unit/sec of power loss between strokes.
    double cpuclock;
    double cputime;
  } playerv[2];
};

#define BATTLE ((struct battle_armwrestling*)battle)

/* Delete.
 */
 
static void _armwrestling_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->strokemin=STROKEMIN_WORST*(1.0-player->skill)+STROKEMIN_BEST*player->skill;
  player->decay=DECAY_WORST*(1.0-player->skill)+DECAY_BEST*player->skill;

  if (player==BATTLE->playerv) { // Left. Only Dot and Princess are allowed.
    player->who=0;
    switch (face) {
      case NS_face_princess: {
          player->tileid=0x30;
        } break;
      default: {
          player->tileid=0x00;
        }
    }
    
  } else { // Right. Only Minotaur is allowed.
    player->who=1;
    player->tileid=0x60;
  }
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cputime=CPUTIME_WORST*(1.0-player->skill)+CPUTIME_BEST*player->skill;
    player->cpuclock=CPU_HEADSTART;
  }
}

/* New.
 */
 
static int _armwrestling_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Record a keystroke.
 */
 
static void player_strike(struct battle *battle,struct player *player,double elapsed) {
  double power=player->strokemin-player->strokeclock;
  if (power<=0.0) power=0.0;
  player->power+=power;
  if (player->power>=1.0) player->power=1.0;
  player->strokeclock=0.0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    player_strike(battle,player,elapsed);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  player->cputime+=CPU_WEAR_OUT*elapsed;
  if ((player->cpuclock-=elapsed)>0.0) return;
  player->cpuclock+=player->cputime+((rand()&0xffff)*player->cputime)/65535.0;
  player_strike(battle,player,elapsed);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  //TODO
}

/* Update.
 */
 
static void _armwrestling_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    player->strokeclock+=elapsed;
    if ((player->power-=player->decay*elapsed)<0.0) player->power=0.0;
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  double lpower=BATTLE->playerv[0].power;
  double rpower=BATTLE->playerv[1].power;
  BATTLE->balance+=(lpower-rpower)*elapsed;
  if (BATTLE->balance<=-1.0) {
    battle->outcome=-1;
  } else if (BATTLE->balance>=1.0) {
    battle->outcome=1;
  }

  //XXX Placeholder UI.
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render a player: Just a 2x3 grid of tiles.
 */
 
static void render2x3(int x,int y,uint8_t tileid) {
  graf_tile(&g.graf,x+NS_sys_tilesize*0,y+NS_sys_tilesize*0,tileid+0x00,0);
  graf_tile(&g.graf,x+NS_sys_tilesize*1,y+NS_sys_tilesize*0,tileid+0x01,0);
  graf_tile(&g.graf,x+NS_sys_tilesize*0,y+NS_sys_tilesize*1,tileid+0x10,0);
  graf_tile(&g.graf,x+NS_sys_tilesize*1,y+NS_sys_tilesize*1,tileid+0x11,0);
  graf_tile(&g.graf,x+NS_sys_tilesize*0,y+NS_sys_tilesize*2,tileid+0x20,0);
  graf_tile(&g.graf,x+NS_sys_tilesize*1,y+NS_sys_tilesize*2,tileid+0x21,0);
}

/* Power meter.
 */
 
static void render_meter(int x,int y,double v) {
  const int ht=NS_sys_tilesize>>1;
  graf_tile(&g.graf,x-ht,y,0x2d,0);
  graf_tile(&g.graf,x+ht,y,0x2e,0);
  uint8_t bicep=0x2a;
       if (v>=1.000) { v=1.0; bicep=0x2c; }
  else if (v>=0.666) bicep=0x2c;
  else if (v>=0.333) bicep=0x2b;
  else if (v>=0.000) bicep=0x2a;
  else { v=0.0; bicep=0x2a; }
  if (v<=0.0) {
    graf_tile(&g.graf,x-ht+1,y+ht,0x29,0);
  } else {
    const double tmax=M_PI*0.750;
    const double radius=NS_sys_tilesize*0.5-1.0;
    double t=v*tmax;
    int8_t rotate=(int8_t)((t*128.0)/M_PI);
    int fax=lround(x-cos(t)*radius);
    int fay=lround(y+ht-sin(t)*radius);
    graf_fancy(&g.graf,fax,fay,0x29,0,rotate,NS_sys_tilesize,0,0x808080ff);
  }
  graf_tile(&g.graf,x+ht,y+ht,bicep,0);
}

/* Render.
 */
 
static void _armwrestling_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_labyrinth);
  
  /* The action happens in a 5x3 grid.
   * Two columns left and right are static, from the players.
   * Middle column, top row is vacant, bottom row constant, and the very middle tile is where pretty much the whole game is expressed.
   * Left player's tileid controls the middle cell appearance; the Minotaur doesn't have arm tiles.
   */
  int actionw=NS_sys_tilesize*5;
  int actionh=NS_sys_tilesize*3;
  int actionx=(FBW>>1)-(actionw>>1);
  int actiony=(FBH>>1)-(actionh>>1);
  int lx=actionx,ty=actiony;
  int ibalance=(int)(BATTLE->balance*-5.0);
  if (ibalance<-4) ibalance=-4; else if (ibalance>4) ibalance=4;
  uint8_t armtile=BATTLE->playerv[0].tileid+0x0a+ibalance;
  actionx+=NS_sys_tilesize>>1;
  actiony+=NS_sys_tilesize>>1;
  render2x3(actionx,actiony,BATTLE->playerv[0].tileid);
  render2x3(actionx+NS_sys_tilesize*3,actiony,BATTLE->playerv[1].tileid);
  graf_tile(&g.graf,actionx+NS_sys_tilesize*2,actiony+NS_sys_tilesize,armtile,0);
  graf_tile(&g.graf,actionx+NS_sys_tilesize*2,actiony+NS_sys_tilesize*2,0x16,0);
  
  // Power meters.
  render_meter(lx,ty-10,BATTLE->playerv[0].power);
  render_meter(lx+actionw,ty-10,BATTLE->playerv[1].power);
}

/* Type definition.
 */
 
const struct battle_type battle_type_armwrestling={
  .name="armwrestling",
  .objlen=sizeof(struct battle_armwrestling),
  .strix_name=182,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_armwrestling_del,
  .init=_armwrestling_init,
  .update=_armwrestling_update,
  .render=_armwrestling_render,
};

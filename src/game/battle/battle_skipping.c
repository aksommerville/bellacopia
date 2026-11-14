/* battle_skipping.c
 */

#include "game/batsup/battle_internal.h"

#define RIPPLE_LIMIT 16

struct battle_skipping {
  struct battle hdr;
  
  /* After one player throws, we start ticking down this clock.
   * If it expires, the other player throws automatically.
   * So you can't just wait and force a tie.
   */
  double deadbeatclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid_body; // 2x2 constant
    uint8_t tileid_arm; // +0x10=thrown
    int thrown,landed,sunk;
    double stonex,stoney; // x in framebuffer pixels; y relative to baseline
    double stonedx,stonedy;
    int skipstate; // 0,1,2=none,struck,foul
    double skipclock; // Counts up after the skip stroke. Can be zero at the skip, if they strike on the very last frame.
    double tolerance; // How far ahead of the breach can you strike, in seconds. Variable.
    double gravity_rate; // px/s**2, variable.
    double cpuclock; // Just for the first throw, and it's really just decorative.
    int cpuokc; // CPU makes so many skips, determined at init. Once exhausted, we miss. We never foul.
  } playerv[2];
  
  struct ripple {
    int x,y;
    uint8_t tileid;
    double animclock;
  } ripplev[RIPPLE_LIMIT];
  int ripplec;
};

#define BATTLE ((struct battle_skipping*)battle)

/* Delete.
 */
 
static void _skipping_del(struct battle *battle) {
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
    player->cpuclock=0.700+((rand()&0xffff)*0.600)/65535.0; // Random so Princess and Duck don't throw at the same moment every time.
    
    /* (cpuokc) is the main ingredient in difficulty.
     * We'll skip so many times, then throw the match.
     * It usually takes four skips to reach the far shore.
     * Make it perfect only at very high skill, because if we land, the best the human can do is tie.
     */
    if (player->skill>=0.950) {
      player->cpuokc=999;
    } else {
      player->cpuokc=(int)(player->skill*5.0)+((rand()%3)-1);
      if (player->cpuokc<0) player->cpuokc=0;
    }
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x1b7407ff;
        player->tileid_body=0xc8;
        player->tileid_arm=0xcc;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid_body=0x0a;
        player->tileid_arm=0xca;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid_body=0x2a;
        player->tileid_arm=0xcb;
      } break;
  }
}

/* New.
 */
 
static int _skipping_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Add ripples.
 */
 
static void skipping_add_ripple(struct battle *battle,int x,int y) {
  if (BATTLE->ripplec>=RIPPLE_LIMIT) return;
  struct ripple *ripple=BATTLE->ripplev+BATTLE->ripplec++;
  ripple->x=x;
  ripple->y=y;
  ripple->tileid=0xf8;
  ripple->animclock=0.100;
}

/* Stroke.
 */
 
static void skipping_stroke(struct battle *battle,struct player *player) {
  
  /* First stroke throws the stone.
   */
  if (!player->thrown) {
    bm_sound_pan(RID_sound_throw,0.0);
    BATTLE->deadbeatclock=10.0;
    player->thrown=1;
    player->stonex=45.0;
    player->stoney=-16.0;
    
    // Initial speed is a little random, but not dependent on skill.
    player->stonedx=40.0+((rand()&0xffff)*20.0)/65535.0;
    player->stonedy=-15.0-((rand()&0xffff)*10.0)/65535.0;
    
    // Less-skilled players get a wider initial tolerance.
    player->tolerance=0.150*(1.0-player->skill)+0.250*player->skill;
    
    // Gravity is a little lighter for more-skilled players.
    player->gravity_rate=120.0*(1.0-player->skill)+100.0*player->skill;
    
    return;
  }
  
  /* If a stroke was already recorded, that's a foul and it's going to sink.
   * Only foul once per stroke, drop the rest.
   */
  if (player->skipstate==2) return;
  if (player->skipstate==1) {
    bm_sound_pan(RID_sound_reject,0.0);
    player->skipstate=2;
    return;
  }
  
  /* Record the stroke and start a clock.
   */
  player->skipstate=1;
  player->skipclock=0.0;
}

/* Skip the stone.
 */
 
static void skipping_skip(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_glug,0.0);
  player->stoney=-0.001;
  if (player->stonedy>=0.0) {
    player->stonedy=-player->stonedy;
  }
  player->skipstate=0;
  player->skipclock=0.0;
  skipping_add_ripple(battle,(int)player->stonex,player->who?152:102);
  
  // Tolerance decreases with each skip, but never below 100 ms.
  if ((player->tolerance-=0.025)<0.100) player->tolerance=0.100;
  
  // Gravity and horizontal speed increase with each skip.
  player->gravity_rate+=20.0;
  player->stonedx+=5.0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) skipping_stroke(battle,player);
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // If we landed or sunk, wait for the game to end.
  if (player->landed||player->sunk) return;

  // Wait a tasteful interval before the initial throw.
  if (!player->thrown) {
    if ((player->cpuclock-=elapsed)>0.0) return;
    skipping_stroke(battle,player);
    return;
  }
  
  /* We're going to skip at the next opportunity.
   * If we've already registered that (ie skipstate==1), force (skipclock=0) to make the timing look perfect.
   * We can get away with that because there's no feedback at the moment of the stroke.
   * (fwiw, cheating like this is not my usual pattern in battles, I think this is the first time I've done it).
   */
  if (player->skipstate) {
    player->skipclock=0.0;
  } else if (player->cpuokc) {
    player->skipstate=1;
    player->skipclock=0.0;
    player->cpuokc--;
  } else {
    // Throwing the match due to (cpuokc). Just let it ride.
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->thrown&&!player->landed&&!player->sunk) {
    player->stonedy+=player->gravity_rate*elapsed;
    player->stonex+=player->stonedx*elapsed;
    player->stoney+=player->stonedy*elapsed;
    
    // Hit the surface?
    if (player->stoney>=0.0) {
      if (player->stonex>=280.0) { // Reached the far shore?
        player->landed=1;
        bm_sound_pan(RID_sound_treasure,0.0);
      } else if ((player->skipstate!=1)||(player->skipclock>player->tolerance)) { // Sunk.
        bm_sound_pan(RID_sound_splash,0.0);
        player->sunk=1;
        skipping_add_ripple(battle,(int)player->stonex,player->who?152:102);
      } else { // Skipped.
        skipping_skip(battle,player);
      }
    
    // If struck, advance the clock.
    } else if (player->skipstate==1) {
      player->skipclock+=elapsed;
    }
  }
}

/* Update.
 */
 
static void _skipping_update(struct battle *battle,double elapsed) {
  
  /* If the deadbeat clock expires, force any unthrown player to throw.
   */
  if ((battle->outcome==-2)&&(BATTLE->deadbeatclock>0.0)) {
    if ((BATTLE->deadbeatclock-=elapsed)<=0.0) {
      struct player *player=BATTLE->playerv;
      int i=2;
      for (;i-->0;player++) {
        if (!player->thrown) skipping_stroke(battle,player);
      }
    }
  }
  
  if (battle->outcome==-2) {
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
      player_update_common(battle,player,elapsed);
    }
  }
  
  struct ripple *ripple=BATTLE->ripplev;
  int i=BATTLE->ripplec;
  for (;i-->0;ripple++) {
    if ((ripple->animclock-=elapsed)<=0.0) {
      ripple->animclock+=0.100;
      if (ripple->tileid==0xfe) ripple->tileid=0xfb;
      else ripple->tileid++;
    }
  }
  
  /* Game ends when both players are landed or sunk.
   * If they both reach the far shore, it's a tie.
   * If they both sink, whoever went furthest wins.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    int lscore=l->landed?1:l->sunk?-1:0;
    int rscore=r->landed?1:r->sunk?-1:0;
    if (lscore&&rscore) {
      if (l->landed&&r->landed) battle->outcome=0; // Both reached the far shore: tie.
      else if (l->landed) battle->outcome=1; // One landed, they win.
      else if (r->landed) battle->outcome=-1;
      else if (l->stonex>r->stonex) battle->outcome=1; // Both sunk, whoever went furthest wins.
      else if (l->stonex<r->stonex) battle->outcome=-1;
      else battle->outcome=0; // Vanishingly unlikely to sink at exactly the same point, but ok, that's a tie.
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  /* Body and arm. Pretty much static.
   */
  int x0=15;
  int x1=x0+NS_sys_tilesize;
  int y0=player->who?130:80;
  int y1=y0+NS_sys_tilesize;
  int armx=x1+3;
  int army=y1-9;
  graf_tile(g_graf,armx,army,player->tileid_arm+(player->thrown?0x10:0x00),0);
  graf_tile(g_graf,x0,y0,player->tileid_body+0x00,0);
  graf_tile(g_graf,x1,y0,player->tileid_body+0x01,0);
  graf_tile(g_graf,x0,y1,player->tileid_body+0x10,0);
  graf_tile(g_graf,x1,y1,player->tileid_body+0x11,0);
  
  /* Stone.
   */
  if (player->thrown&&!player->sunk) {
    int basey=y1+6;
    int stonex=(int)player->stonex;
    int stoney=basey+(int)player->stoney;
    graf_tile(g_graf,stonex,basey,0xe9,0);
    graf_tile(g_graf,stonex,stoney,0xe8,0);
  }
  
  /* Foul indicator.
   */
  if (player->skipstate==2) {
    graf_tile(g_graf,x0,y0-10,0x7a,0);
    graf_tile(g_graf,x1,y0-10,0x7b,0);
  }
}

/* Render.
 */
 
static void _skipping_render(struct battle *battle) {

  /* Order matters: Sky, water, shores, players.
   * Naively drawing the whole background, then all the sprites, permits ripples to overlap the shore sometimes.
   */
  const int ghorizon=50;
  const int whorizon=54;
  const int lbankw=40;
  const int rbankw=40;
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,whorizon,FBW,FBH-whorizon,0x0e55a4ff);
  graf_fill_rect(g_graf,0,whorizon,FBW,1,0x000000ff);
  
  graf_set_image(g_graf,RID_image_battle_forest);
  struct ripple *ripple=BATTLE->ripplev;
  int i=BATTLE->ripplec;
  for (;i-->0;ripple++) {
    graf_tile(g_graf,ripple->x,ripple->y,ripple->tileid,0);
  }
  
  graf_fill_rect(g_graf,0,ghorizon,lbankw,FBH-ghorizon,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,FBW-rbankw,ghorizon,rbankw,FBH-ghorizon,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,ghorizon,lbankw,1,0x000000ff);
  graf_fill_rect(g_graf,lbankw,ghorizon,1,FBH-ghorizon,0x000000ff);
  graf_fill_rect(g_graf,FBW-rbankw,ghorizon,rbankw,1,0x000000ff);
  graf_fill_rect(g_graf,FBW-rbankw,ghorizon,1,FBH-ghorizon,0x000000ff);
  
  graf_set_image(g_graf,RID_image_battle_forest);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_skipping={
  .name="skipping",
  .objlen=sizeof(struct battle_skipping),
  .id=128,
  .strix_name=419,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .imageid_default=RID_image_meadow,
  .del=_skipping_del,
  .init=_skipping_init,
  .update=_skipping_update,
  .render=_skipping_render,
};

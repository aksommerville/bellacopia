/* battle_topping.c
 * Hold A to pour ketchup on a giant moving hotdog.
 */

#include "game/bellacopia.h"

// We need lots of dribble slots. They remain discrete objects even when bound to the dog.
#define DRIBBLE_LIMIT 256

#define GRAVITY 120.0 /* px/s */
#define DOGMARGIN 28
#define DOGY 160
#define DOGSPEED 80.0
#define TIMELIMIT 19.800 /* Makes exactly three cycles, given the above margin and speed. It's important that we stop around the same phase we started at. */

struct battle_topping {
  struct battle hdr;
  int choice;
  double dogx,dogdx;
  double clock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    int dribblereq; // Input state.
    int dribbling; // Actual dribbling.
    double stateclock; // Generic rate limit on state changes.
    int x,y; // Midpoint of bottle. 1x2 tiles.
    int topc,wastec;
    int score; // Recalc when (topc) or (wastec) changes.
    double dribbleclock;
    double dribbletime;
    int blackout;
    double rxa,rxz,lxa,lxz; // CPU. Dog's range when we'll dribble, for a given direction of movement.
  } playerv[2];
  
  struct dribble {
    double x,y; // Framebuffer pixels if unbound, or relative to (dogx) if bound.
    int bound;
    uint8_t tileid;
    uint8_t xform;
    uint32_t color;
  } dribblev[DRIBBLE_LIMIT];
  int dribblec;
};

#define BATTLE ((struct battle_topping*)battle)

/* Delete.
 */
 
static void _topping_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->y=60;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW/3;
  } else { // Right.
    player->who=1;
    player->x=(FBW*2)/3;
  }
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    const double dogwidth=NS_sys_tilesize*3.0;
    const double travely=DOGY-(player->y+NS_sys_tilesize); // Vertical center of dog; not exactly correct.
    // A naive player would dribble where the dog *is*. The physics are such that he just barely misses, and gets a 0% hit rate.
    player->rxz=player->lxa=player->x;
    player->rxz+=dogwidth*0.5;
    player->lxa-=dogwidth*0.5;
    // A more asute player would dribble where the dog is *going* to be. Experimentally, 100% hit rate. But it was really close, didn't look exactly perfect.
    double droptime=travely/GRAVITY;
    double leaddx=droptime*DOGSPEED;
    leaddx*=player->skill; // <-- The secret sauce, nice and simple scales from 0..100% hit rate usually.
    player->rxz-=leaddx;
    player->lxa+=leaddx;
    player->rxa=player->rxz-dogwidth;
    player->lxz=player->lxa+dogwidth;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x8d2c42ff;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
  player->dribbletime=0.070;
  // No handicap for pvp. We could make one by adjusting dribbletime here.
}

/* New.
 */
 
static int _topping_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  BATTLE->dogx=DOGMARGIN+(rand()%(FBW-(DOGMARGIN<<1)));
  BATTLE->dogdx=DOGSPEED;
  if (rand()&1) BATTLE->dogdx=-BATTLE->dogdx;
  BATTLE->clock=TIMELIMIT;
  
  return 0;
}

/* Score for a player.
 */
 
static int calc_score(const struct battle *battle,const struct player *player) {
  int s=player->topc-player->wastec/2;
  if (s<0) return 0;
  return s;
}

/* Player by color.
 */
 
static struct player *topping_player_by_color(struct battle *battle,uint32_t color) {
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->color==color) return player;
  }
  return 0;
}

/* Make a new dribble.
 */
 
static void topping_create_dribble(struct battle *battle,struct player *player) {
  if (BATTLE->dribblec>=DRIBBLE_LIMIT) return;
  if (BATTLE->clock<=0.0) return;
  struct dribble *dribble=BATTLE->dribblev+BATTLE->dribblec++;
  dribble->x=player->x;
  dribble->y=player->y+NS_sys_tilesize;
  dribble->bound=0;
  dribble->color=player->color;
  dribble->xform=rand()&(EGG_XFORM_XREV|EGG_XFORM_YREV); // They're oriented vertically. No SWAP while falling.
  switch (rand()&3) {
    case 0: dribble->tileid=0xa2; break;
    case 1: dribble->tileid=0xa3; break;
    case 2: dribble->tileid=0xb2; break;
    case 3: dribble->tileid=0xb3; break;
  }
  bm_sound_pan(RID_sound_dribble,player->who?0.250:-0.250);
}

/* Record waste.
 */
 
static void topping_waste(struct battle *battle,struct dribble *dribble) {
  struct player *player=topping_player_by_color(battle,dribble->color);
  if (!player) return;
  player->wastec++;
  player->score=calc_score(battle,player);
  //TODO sound?
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->dribblereq=(input&EGG_BTN_SOUTH);
  }
}

/* Update CPU player.
 * Super simple because the dog's speed is constant.
 * We computed two ranges at init, one for each direction of travel.
 * Dribble when it's in the appropriate range.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (BATTLE->dogdx>0.0) {
    player->dribblereq=((BATTLE->dogx>=player->rxa)&&(BATTLE->dogx<=player->rxz));
  } else {
    player->dribblereq=((BATTLE->dogx>=player->lxa)&&(BATTLE->dogx<=player->lxz));
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  
  // Commit new dribbling state, with a blackout period.
  if (player->stateclock>0.0) {
    player->stateclock-=elapsed;
  } else if (player->dribblereq&&!player->dribbling) {
    player->stateclock=0.250;
    player->dribbling=1;
    player->dribbleclock=0.0;
    bm_sound_pan(RID_sound_fart,player->who?0.250:-0.250);
  } else if (!player->dribblereq&&player->dribbling) {
    player->stateclock=0.250;
    player->dribbling=0;
    bm_sound_pan(RID_sound_unfart,player->who?0.250:-0.250);
  }
  
  // Create a new dribble, if state and clock say so.
  if (player->dribbling) {
    if (player->dribbleclock>0.0) {
      player->dribbleclock-=elapsed;
    } else {
      player->dribbleclock+=player->dribbletime;
      topping_create_dribble(battle,player);
    }
  }
}

/* Update.
 */
 
static void _topping_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  BATTLE->clock-=elapsed;
  
  // Players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  // Dribbles.
  int all_bound=1;
  struct dribble *dribble=BATTLE->dribblev+BATTLE->dribblec-1;
  for (i=BATTLE->dribblec;i-->0;dribble--) {
    if (dribble->bound) continue;
    all_bound=0;
    dribble->y+=GRAVITY*elapsed;
    if (dribble->y>FBH+8.0) {
      topping_waste(battle,dribble);
      BATTLE->dribblec--;
      memmove(dribble,dribble+1,sizeof(struct dribble)*(BATTLE->dribblec-i));
      continue;
    }
    double dy=dribble->y-DOGY;
    if ((dy>=-8.0)&&(dy<=2.0)) {
      double dx=dribble->x-BATTLE->dogx;
      if ((dx>=-24.0)&&(dx<=24.0)) {
        dribble->bound=1;
        dribble->x=dx;
        dribble->y=-5.0+(rand()%3);
        dribble->xform|=EGG_XFORM_SWAP;
        struct player *player=topping_player_by_color(battle,dribble->color);
        if (player) {
          player->topc++;
          player->score=calc_score(battle,player);
        }
        //TODO sound?
      }
    }
  }
  
  // Hot dog.
  BATTLE->dogx+=BATTLE->dogdx*elapsed;
  if ((BATTLE->dogx<DOGMARGIN)&&(BATTLE->dogdx<0.0)) BATTLE->dogdx=-BATTLE->dogdx;
  else if ((BATTLE->dogx>FBW-DOGMARGIN)&&(BATTLE->dogdx>0.0)) BATTLE->dogdx=-BATTLE->dogdx;
  
  // Game ends when the clock is expired and all dribbles are bound.
  // If the scores are tied, try to break based on (topc) alone.
  if (all_bound&&(BATTLE->clock<=0.0)) {
    const struct player *l=BATTLE->playerv;
    const struct player *r=BATTLE->playerv+1;
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else if (l->topc>r->topc) battle->outcome=1;
    else if (l->topc<r->topc) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int ht=NS_sys_tilesize>>1;
  uint8_t tileid=0xa0;
  if (player->dribbling) tileid+=1;
  graf_fancy(&g.graf,player->x,player->y-ht,tileid,0,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,player->x,player->y+ht,tileid+0x10,0,0,NS_sys_tilesize,0,player->color);
  
  graf_fancy(&g.graf,player->x-ht,player->y-NS_sys_tilesize*2,0xb5,0,0,NS_sys_tilesize,0,player->color); // bottle
  graf_fancy(&g.graf,player->x-ht,player->y-NS_sys_tilesize*3-1,0xb4,0,0,NS_sys_tilesize,0,player->color); // waste
}

static void player_render_score(struct battle *battle,struct player *player) {
  int x0=player->x+8;
  int y=player->y-NS_sys_tilesize*2;
  int x,v;
  #define DECUINT(n) { \
    v=(n); \
    x=x0; \
    if (v>=100) { graf_tile(&g.graf,x,y,0x30+(v/100)%10,0); x+=8; } \
    if (v>=10) { graf_tile(&g.graf,x,y,0x30+(v/10)%10,0); x+=8; } \
    graf_tile(&g.graf,x,y,0x30+v%10,0); \
  }
  DECUINT(player->topc)
  y-=NS_sys_tilesize+1;
  DECUINT(player->wastec)
  
  if (player->who) x0+=NS_sys_tilesize*3;
  else x0-=NS_sys_tilesize*5;
  y=player->y-NS_sys_tilesize*2;
  DECUINT(player->score)
  
  if (!player->who) { // Draw the clock too, while we're at it.
    int sec=(int)BATTLE->clock;
    if (BATTLE->clock<=0.0) sec=0; else sec++;
    if (sec<0) sec=0;
    x0=FBW>>1;
    if (sec>=10) x0-=8; else x0-=4;
    y=24;
    DECUINT(sec)
  }
  #undef DECUINT
}

/* Render.
 */
 
static void _topping_render(struct battle *battle) {

  // Background. Blink at the last three seconds.
  if (BATTLE->clock<=0.0) {
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0x204028ff);
  } else {
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0x105020ff);
    if ((BATTLE->clock<=3.0)&&(BATTLE->clock>0.0)) {
      int ms=(int)(BATTLE->clock*1000.0)%1000;
      if (ms>750) {
        int alpha=ms-750; // 250 vs 255, close enough, just go with it.
        graf_fill_rect(&g.graf,0,0,FBW,FBH,0x40802000|alpha);
      }
    }
  }
  graf_set_image(&g.graf,RID_image_battle_fractia);
  
  // Hot dog.
  int dogx=(int)BATTLE->dogx;
  graf_tile(&g.graf,dogx-NS_sys_tilesize,DOGY,0xa4,0);
  graf_tile(&g.graf,dogx,DOGY,0xa5,0);
  graf_tile(&g.graf,dogx+NS_sys_tilesize,DOGY,0xa6,0);
  
  // Dribbles.
  struct dribble *dribble=BATTLE->dribblev;
  int i=BATTLE->dribblec;
  for (;i-->0;dribble++) {
    int x=(int)dribble->x;
    int y=(int)dribble->y;
    if (dribble->bound) {
      x+=dogx;
      y+=DOGY;
    }
    graf_fancy(&g.graf,x,y,dribble->tileid,dribble->xform,0,NS_sys_tilesize,0,dribble->color);
  }
  
  // Bottles and scoreboard framing.
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // Scoreboard.
  graf_set_image(&g.graf,RID_image_fonttiles);
  player_render_score(battle,BATTLE->playerv+0);
  player_render_score(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_topping={
  .name="topping",
  .objlen=sizeof(struct battle_topping),
  .strix_name=151,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_topping_del,
  .init=_topping_init,
  .update=_topping_update,
  .render=_topping_render,
};

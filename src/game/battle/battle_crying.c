/* battle_crying.c
 * Some observations in general for two-fisted karate chop contests, gathered for this one:
 *  - 77 ms was my best average period in a short experiment.
 *  - ~120 ms when I go so slow it feels like throwing the match.
 *  - 80..90 ms playing for real.
 *  - Got to 100 ms exactly, and it feels on the very slow end of realistic.
 * Also tried with South only, since we're set up for it.
 *  - 150 ms feels pretty sustainable, I could do it all day.
 *  - 114 ms was my best in 5 trials.
 *  - The other three landed right around 130 ms. And that's with full effort, my thumb hurts.
 * So some standing guidance for Karate Chop Contests:
 *  - 140 ms target for single button.
 *  - 90 ms target for alternating.
 */

#include "game/bellacopia.h"

#define END_COOLDOWN 1.0
#define UPFORCE_EASY   0.150 /* Ended up not using force biases. The only variables are headstart time and tap interval. */
#define UPFORCE_HARD   0.150 /* This means that 2-player games are never adjustable. Might need to revist that decision. */
#define DOWNFORCE_EASY 1.000
#define DOWNFORCE_HARD 1.000
#define HOLD_TIME 0.500 /* Must stay above 1 for so long to latch. */
#define SQUALL_TIME 0.200
#define CPU_TIME_WORST 0.120 /* With 90..120, and 200..500 headstart, I can win consistently at 0xff, but it's always close. */
#define CPU_TIME_BEST  0.090 /* A human playing seriously can do way better than 90. */
#define CPU_HEADSTART_SLOW 0.500 /* Humans take a sec to get started; so have the CPU give a little grace. */
#define CPU_HEADSTART_FAST 0.200

struct battle_crying {
  uint8_t handicap;
  uint8_t players;
  void (*cb_end)(int outcome,void *userdata);
  void *userdata;
  int outcome;
  double end_cooldown;
  
  struct player {
    int who;
    int human;
    int srcx,srcy; // 32x32. Two frames: idle,cry
    int dstx,dsty; // Center of the main 32x32 decal.
    uint8_t xform;
    int pvinput;
    int btnid; // The next input expected.
    double misery; // 0..1, win at 1.
    double upforce; // Impulse, per keystroke. Must be positive.
    double downforce; // Units per second.
    double skill; // 0..1, per (who) and (handicap).
    double holdclock;
    double squallclock;
    double cpuclock;
    double cputime;
  } playerv[2];
};

#define CTX ((struct battle_crying*)ctx)

/* Delete.
 */
 
static void _crying_del(void *ctx) {
  free(ctx);
}

/* Initialize player.
 */
 
static void player_init(void *ctx,struct player *player,int human,int appearance) {
  if (player==CTX->playerv) {
    player->who=0;
    player->dstx=FBW/3+10;
    player->dsty=100;
    player->skill=1.0-CTX->handicap/255.0;
  } else {
    player->who=1;
    player->dstx=(FBW*2)/3-10;
    player->dsty=100;
    player->xform=EGG_XFORM_XREV;
    player->skill=CTX->handicap/255.0;
  }
  if (player->human=human) {
    player->pvinput=EGG_BTN_DOWN|EGG_BTN_SOUTH;
  } else {
    player->cputime=CPU_TIME_WORST*(1.0-player->skill)+CPU_TIME_BEST*player->skill;
    player->cpuclock=CPU_HEADSTART_SLOW*(1.0-player->skill)+CPU_HEADSTART_FAST*player->skill;
  }
  switch (appearance) {
    case 0: { // Soblin
        player->srcx=128;
        player->srcy=112;
      } break;
    case 1: { // Dot
        player->srcx=128;
        player->srcy=144;
      } break;
    case 2: { // Princess
        player->srcx=128;
        player->srcy=176;
      } break;
  }
  player->btnid=EGG_BTN_SOUTH;
  player->upforce=UPFORCE_EASY*player->skill+UPFORCE_HARD*(1.0-player->skill);
  player->downforce=DOWNFORCE_EASY*player->skill+DOWNFORCE_HARD*(1.0-player->skill);
}

/* New.
 */
 
static void *_crying_init(
  uint8_t handicap,
  uint8_t players,
  void (*cb_end)(int outcome,void *userdata),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_crying));
  if (!ctx) return 0;
  CTX->handicap=handicap;
  CTX->players=players;
  CTX->cb_end=cb_end;
  CTX->userdata=userdata;
  CTX->outcome=-2;
  
  switch (CTX->players) {
    case NS_players_cpu_cpu: {
        player_init(ctx,CTX->playerv+0,0,2);
        player_init(ctx,CTX->playerv+1,0,0);
      } break;
    case NS_players_cpu_man: {
        player_init(ctx,CTX->playerv+0,0,0);
        player_init(ctx,CTX->playerv+1,2,2);
      } break;
    case NS_players_man_cpu: {
        player_init(ctx,CTX->playerv+0,1,1);
        player_init(ctx,CTX->playerv+1,0,0);
      } break;
    case NS_players_man_man: {
        player_init(ctx,CTX->playerv+0,1,1);
        player_init(ctx,CTX->playerv+1,2,2);
      } break;
    default: _crying_del(ctx); return 0;
  }
  return ctx;
}

/* Process a keystroke.
 */
 
static void player_strike(void *ctx,struct player *player,int btnid) {
  if (btnid!=player->btnid) return;
  player->misery+=player->upforce;
  player->squallclock=SQUALL_TIME;
  if (btnid==EGG_BTN_SOUTH) {
    player->btnid=EGG_BTN_DOWN;
  } else {
    player->btnid=EGG_BTN_SOUTH;
  }
}

/* Update human player.
 */
 
static void player_update_man(void *ctx,struct player *player,double elapsed,int input) {
  if (input!=player->pvinput) {
    if ((input&EGG_BTN_SOUTH)&&!(player->pvinput&EGG_BTN_SOUTH)) player_strike(ctx,player,EGG_BTN_SOUTH);
    if ((input&EGG_BTN_DOWN)&&!(player->pvinput&EGG_BTN_DOWN)) player_strike(ctx,player,EGG_BTN_DOWN);
    player->pvinput=input;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(void *ctx,struct player *player,double elapsed) {
  if ((player->cpuclock-=elapsed)>0.0) return;
  player->cpuclock+=player->cputime;
  player_strike(ctx,player,player->btnid);
}

/* Update either player, after specific controller.
 * Controllers should apply up force directly to (misery).
 * They should not check overflow or apply down force.
 */
 
static void player_update_common(void *ctx,struct player *player,double elapsed) {
  player->squallclock-=elapsed;
  if (player->misery>=1.0) {
    if ((player->holdclock-=elapsed)<=0.0) {
      CTX->outcome=player->who?-1:1;
      CTX->end_cooldown=END_COOLDOWN;
      return;
    }
  } else {
    player->holdclock=HOLD_TIME;
  }
  player->misery-=elapsed*player->downforce;
  if (player->misery<0.0) player->misery=0.0;
}

/* Update.
 */
 
static void _crying_update(void *ctx,double elapsed) {

  if (CTX->outcome>-2) {
    if (CTX->end_cooldown>0.0) {
      CTX->end_cooldown-=elapsed;
    } else if (CTX->cb_end) {
      CTX->cb_end(CTX->outcome,CTX->userdata);
      CTX->cb_end=0;
    }
    return;
  }
  
  struct player *player=CTX->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(ctx,player,elapsed,g.input[player->human]);
    else player_update_cpu(ctx,player,elapsed);
    player_update_common(ctx,player,elapsed);
    if (CTX->outcome>-2) return;
  }
}

/* Render player.
 */
 
static void player_render(void *ctx,struct player *player) {
  int frame=(player->squallclock>0.0)?1:0;
  graf_decal_xform(&g.graf,player->dstx-16,player->dsty-16,player->srcx+frame*32,player->srcy,32,32,player->xform);
  if (frame) {
    graf_tile(&g.graf,player->dstx,player->dsty-24,0x6d,0);
  }
}

/* Render meter.
 */
 
static void meter_render(void *ctx,double misery,int x,int w,int victory) {
  const uint32_t bgcolor=0x000020ff;
  const uint32_t fgcolor=victory?0xffff00ff:0x80a0ffff;
  int y=75;
  int fullh=40;
  int barh=(int)(misery*fullh);
  if (barh>=fullh) {
    graf_fill_rect(&g.graf,x,y,w,fullh,fgcolor);
  } else if (barh<=0) {
    graf_fill_rect(&g.graf,x,y,w,fullh,bgcolor);
  } else {
    graf_fill_rect(&g.graf,x,y,w,fullh-barh,bgcolor);
    graf_fill_rect(&g.graf,x,y+fullh-barh,w,barh,fgcolor);
  }
}

/* Render.
 */
 
static void _crying_render(void *ctx) {

  // Background.
  const uint32_t light=0x8090a0ff;
  const uint32_t dark= 0x406090ff;
  const int horizon=100;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x8090a0ff);
  graf_gradient_rect(&g.graf,0,0,FBW,horizon,dark,dark,light,light);
  graf_gradient_rect(&g.graf,0,horizon,FBW,FBH-horizon,light,light,dark,dark);
  
  // Sprites.
  graf_set_image(&g.graf,RID_image_battle_goblins);
  player_render(ctx,CTX->playerv+0);
  player_render(ctx,CTX->playerv+1);
  
  // Power meters.
  const int meterw=6;
  meter_render(ctx,CTX->playerv[0].misery,CTX->playerv[0].dstx+34,meterw,CTX->outcome==1);
  meter_render(ctx,CTX->playerv[1].misery,CTX->playerv[1].dstx-34-meterw,meterw,CTX->outcome==-1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_crying={
  .name="crying",
  .strix_name=56,
  .no_article=0,
  .no_contest=0,
  .supported_players=(1<<NS_players_cpu_cpu)|(1<<NS_players_cpu_man)|(1<<NS_players_man_cpu)|(1<<NS_players_man_man),
  .del=_crying_del,
  .init=_crying_init,
  .update=_crying_update,
  .render=_crying_render,
};

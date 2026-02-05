/* battle_apples.c
 */

#include "game/bellacopia.h"

#define END_COOLDOWN 1.0
#define SKY_COLOR 0x785830ff
#define GROUND_COLOR 0x3c2011ff
#define GROUNDY 110
#define APPLE_LIMIT 12
#define APPLE_SPEED_LO 20.0 /* px/s */
#define APPLE_SPEED_HI 30.0
#define APPLE_RANGE ((FBW>>1)-24)
#define APPLE_X0 ((FBW>>1)-(APPLE_RANGE>>1))
#define GRAB_TIME 0.333
#define CPU_DELAY_SHORT 1.000 /* Time from end of chewing to first possible bob. */
#define CPU_DELAY_LONG  3.000
#define CPU_HEADSTART 1.000 /* No bobbing this early in any match. */

struct battle_apples {
  uint8_t handicap;
  uint8_t players;
  void (*cb_end)(int outcome,void *userdata);
  void *userdata;
  int outcome;
  double end_cooldown;
  
  struct player {
    int who;
    int human;
    int gbsrcx,gbsrcy; // 64x48 pixels from the Gobbling Contest. We borrow the idle and eating top halves.
    int knsrcx,knsrcy; // 32x24 pixels for the bottom half, kneeling.
    int bsrcx,bsrcy; // 48x24 pixels of bobbing, the whole frame.
    int dstx; // Center of decal.
    uint8_t xform;
    double bobclock;
    double bobtime;
    int pvinput;
    double eatclock;
    double eattime;
    int score;
    int eating;
    uint32_t color;
    double cpuclock;
    double cputime;
  } playerv[2];
  
  struct apple {
    double x; // framebuffer pixels, center of apple
    double dx; // px/s, speed baked in
    double animclock;
    int animframe;
    struct player *distress; // if not null, the guy that's eating me
  } applev[APPLE_LIMIT];
  int applec;
};

#define CTX ((struct battle_apples*)ctx)

/* Delete.
 */
 
static void _apples_del(void *ctx) {
  free(ctx);
}

/* Initialize player.
 */
 
static void player_init(void *ctx,struct player *player,int human,int appearance) {
  if (player==CTX->playerv) {
    player->who=0;
    player->dstx=FBW>>2;
  } else {
    player->who=1;
    player->dstx=(FBW*3)>>2;
    player->xform=EGG_XFORM_XREV;
  }
  if (player->human=human) {
    player->pvinput=EGG_BTN_SOUTH;
  } else {
    double skill=CTX->handicap/255.0;
    if (!player->who) skill=1.0-skill;
    player->cputime=CPU_DELAY_LONG*(1.0-skill)+CPU_DELAY_SHORT*skill;
    player->cpuclock=player->cputime+CPU_HEADSTART;
  }
  switch (appearance) {
    case 0: { // Boblin
        player->gbsrcx=80;
        player->gbsrcy=208;
        player->knsrcx=80;
        player->knsrcy=232;
        player->bsrcx=144;
        player->bsrcy=232;
        player->color=0x983969ff;
      } break;
    case 1: { // Dot
        player->gbsrcx=0;
        player->gbsrcy=48;
        player->knsrcx=48;
        player->knsrcy=208;
        player->bsrcx=32;
        player->bsrcy=232;
        player->color=0x411775ff;
      } break;
    case 2: { // Princess
        player->gbsrcx=128;
        player->gbsrcy=48;
        player->knsrcx=144;
        player->knsrcy=208;
        player->bsrcx=192;
        player->bsrcy=232;
        player->color=0x0d3ac1ff;
      } break;
  }
  player->bobtime=0.250; // TODO Maybe variable?
  player->eattime=0.750; // ''
}

/* New.
 */
 
static void *_apples_init(
  uint8_t handicap,
  uint8_t players,
  void (*cb_end)(int outcome,void *userdata),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_apples));
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
    default: _apples_del(ctx); return 0;
  }
  
  CTX->applec=APPLE_LIMIT;
  struct apple *apple=CTX->applev;
  int i=CTX->applec;
  for (;i-->0;apple++) {
    apple->animclock=((rand()&0xffff)*0.200)/65535.0;
    apple->animframe=rand()&3;
    apple->x=APPLE_X0+rand()%APPLE_RANGE;
    apple->dx=(rand()&0xffff)/65535.0;
    apple->dx=apple->dx*APPLE_SPEED_LO+(1.0-apple->dx)*APPLE_SPEED_HI;
    if (i&1) apple->dx=-apple->dx; // Initial direction, give half each way.
  }
  
  return ctx;
}

/* End of bob. If an apple is in range, start eating it.
 */
 
static void player_check_apples(void *ctx,struct player *player) {
  if (player->eating) {
    player->eating=0;
    struct apple *apple=CTX->applev+CTX->applec-1;
    int i=CTX->applec;
    for (;i-->0;apple--) {
      if (apple->distress==player) {
        CTX->applec--;
        memmove(apple,apple+1,sizeof(struct apple)*(CTX->applec-i));
      }
    }
    return;
  }
  player->bobclock=0.0;
  if (CTX->outcome>-2) return;
  const double radius=8.0;
  double focusx=player->dstx;
  if (player->xform) focusx-=20.0;
  else focusx+=20.0;
  struct apple *apple=CTX->applev+CTX->applec-1;
  int i=CTX->applec;
  for (;i-->0;apple--) {
    double dx=apple->x-focusx;
    if ((dx<-radius)||(dx>radius)) continue;
    // Got an apple!
    apple->x=focusx;
    apple->distress=player;
    player->bobclock=GRAB_TIME;
    player->eating=1;
    bm_sound(RID_sound_collect);
    player->eatclock=player->eattime;
    player->score++;
    return;
  }
}

/* Begin bobbing.
 */
 
static void player_bob(void *ctx,struct player *player) {
  if (CTX->outcome>-2) return;
  player->bobclock=player->bobtime;
}

/* Update human player.
 */
 
static void player_update_man(void *ctx,struct player *player,double elapsed,int input) {
  if (input!=player->pvinput) {
    if ((input&EGG_BTN_SOUTH)&&!(player->pvinput&EGG_BTN_SOUTH)) player_bob(ctx,player);
    player->pvinput=input;
  }
}

/* Update CPU player.
 */
 
static int should_bob(void *ctx,struct player *player) {
  double focusx=player->dstx;
  if (player->xform) focusx-=20.0;
  else focusx+=20.0;
  struct apple *apple=CTX->applev;
  int i=CTX->applec;
  for (;i-->0;apple++) {
    double dx=apple->x-focusx;
    if (dx<-6.0) continue;
    if (dx>6.0) continue;
    return 1;
  }
  return 0;
}
 
static void player_update_cpu(void *ctx,struct player *player,double elapsed) {
  if ((player->cpuclock-=elapsed)>0.0) return;
  if (should_bob(ctx,player)) {
    player->cpuclock+=player->cputime;
    player_bob(ctx,player);
  }
}

/* Update apple.
 */
 
static void apple_update(void *ctx,struct apple *apple,double elapsed) {
  if (apple->distress) return;
  apple->x+=apple->dx*elapsed;
  if (apple->x<APPLE_X0) {
    apple->x=APPLE_X0;
    if (apple->dx<0.0) apple->dx=-apple->dx;
  } else if (apple->x>APPLE_X0+APPLE_RANGE) {
    apple->x=APPLE_X0+APPLE_RANGE;
    if (apple->dx>0.0) apple->dx=-apple->dx;
  }
  if ((apple->animclock-=elapsed)<=0.0) {
    apple->animclock+=0.200;
    if (++(apple->animframe)>=4) apple->animframe=0;
  }
}

/* Update.
 */
 
static void _apples_update(void *ctx,double elapsed) {

  if (CTX->outcome>-2) {
    if (CTX->end_cooldown>0.0) {
      CTX->end_cooldown-=elapsed;
    } else if (CTX->cb_end) {
      CTX->cb_end(CTX->outcome,CTX->userdata);
      CTX->cb_end=0;
    }
    // Do allow update to proceed during the cooldown.
    // But we'll forbid any further bobbing.
  }
  
  struct player *player=CTX->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->bobclock>0.0) {
      if ((player->bobclock-=elapsed)<=0.0) {
        player_check_apples(ctx,player);
      }
    } else if (player->eatclock>0.0) {
      player->eatclock-=elapsed;
    } else {
      if (player->human) player_update_man(ctx,player,elapsed,g.input[player->human]);
      else player_update_cpu(ctx,player,elapsed);
    }
  }
  
  struct apple *apple=CTX->applev;
  for (i=CTX->applec;i-->0;apple++) {
    apple_update(ctx,apple,elapsed);
  }
  
  // First to 4 wins.
  // Ties are very unlikely. Break to the left, because that's how the scoreboard will render.
  if (CTX->outcome==-2) {
    if (CTX->playerv[0].score>=4) {
      CTX->outcome=1;
      CTX->end_cooldown=END_COOLDOWN;
    } else if (CTX->playerv[1].score>=4) {
      CTX->outcome=-1;
      CTX->end_cooldown=END_COOLDOWN;
    }
  }
}

/* Render player.
 */
 
static void player_render(void *ctx,struct player *player) {
  int topy=GROUNDY-38;
  if (player->bobclock>0.0) {
    int dstx=player->dstx;
    if (player->xform) dstx-=32; else dstx-=16;
    graf_decal_xform(&g.graf,dstx,topy+16,player->bsrcx,player->bsrcy,48,24,player->xform);
  } else {
    int topsrcx=player->gbsrcx;
    int topsrcy=player->gbsrcy;
    int btmsrcx=player->knsrcx;
    int btmsrcy=player->knsrcy;
    if (player->eatclock>0.0) {
      topsrcx+=32;
      int frame=((int)(player->eatclock*5.0))&1;
      if (frame) topsrcy+=24;
    }
    graf_decal_xform(&g.graf,player->dstx-16,topy,topsrcx,topsrcy,32,24,player->xform);
    graf_decal_xform(&g.graf,player->dstx-16,topy+24,btmsrcx,btmsrcy,32,24,player->xform);
  }
}

/* Render apple.
 */
 
static void apple_render(void *ctx,struct apple *apple) {
  uint8_t tileid,xform=0;
  if (apple->dx<0.0) xform=EGG_XFORM_XREV;
  if (apple->distress) {
    tileid=0xe1;
  } else switch (apple->animframe) {
    case 0: tileid=0xe0; break;
    case 1: tileid=0xf0; break;
    case 2: tileid=0xe0; break;
    case 3: tileid=0xf1; break;
  }
  graf_tile(&g.graf,(int)apple->x,GROUNDY-4,tileid,xform);
}

/* Render.
 */
 
static void _apples_render(void *ctx) {

  // Background.
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,SKY_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,GROUND_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  
  // Sprites.
  graf_set_image(&g.graf,RID_image_battle_goblins);
  player_render(ctx,CTX->playerv+0);
  player_render(ctx,CTX->playerv+1);
  struct apple *apple=CTX->applev;
  int i=CTX->applec;
  for (;i-->0;apple++) apple_render(ctx,apple);
  
  // Scoreboard
  const int lighty=GROUNDY+NS_sys_tilesize;
  const int lightspacing=NS_sys_tilesize;
  int lightsw=7*lightspacing;
  int lightsx=(FBW>>1)-(lightsw>>1)+(NS_sys_tilesize>>1);
  for (i=0;i<7;i++,lightsx+=lightspacing) {
    uint32_t color=0x808080ff;
    if (i<CTX->playerv[0].score) color=CTX->playerv[0].color;
    else if (i>=7-CTX->playerv[1].score) color=CTX->playerv[1].color;
    graf_fancy(&g.graf,lightsx,lighty,0xd2,0,0,NS_sys_tilesize,0,color);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_apples={
  .name="apples",
  .strix_name=58,
  .no_article=0,
  .no_contest=0,
  .supported_players=(1<<NS_players_cpu_cpu)|(1<<NS_players_cpu_man)|(1<<NS_players_man_cpu)|(1<<NS_players_man_man),
  .del=_apples_del,
  .init=_apples_init,
  .update=_apples_update,
  .render=_apples_render,
};

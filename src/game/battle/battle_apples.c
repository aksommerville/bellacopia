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
  struct battle hdr;
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

#define BATTLE ((struct battle_apples*)battle)

/* Delete.
 */
 
static void _apples_del(struct battle *battle) {
}

/* Initialize player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int appearance) {
  if (player==BATTLE->playerv) {
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
    double skill=battle->args.bias/255.0;
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

static int _apples_init(struct battle *battle) {
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  BATTLE->applec=APPLE_LIMIT;
  struct apple *apple=BATTLE->applev;
  int i=BATTLE->applec;
  for (;i-->0;apple++) {
    apple->animclock=((rand()&0xffff)*0.200)/65535.0;
    apple->animframe=rand()&3;
    apple->x=APPLE_X0+rand()%APPLE_RANGE;
    apple->dx=(rand()&0xffff)/65535.0;
    apple->dx=apple->dx*APPLE_SPEED_LO+(1.0-apple->dx)*APPLE_SPEED_HI;
    if (i&1) apple->dx=-apple->dx; // Initial direction, give half each way.
  }
  
  return 0;
}

/* End of bob. If an apple is in range, start eating it.
 */
 
static void player_check_apples(struct battle *battle,struct player *player) {
  if (player->eating) {
    player->eating=0;
    struct apple *apple=BATTLE->applev+BATTLE->applec-1;
    int i=BATTLE->applec;
    for (;i-->0;apple--) {
      if (apple->distress==player) {
        BATTLE->applec--;
        memmove(apple,apple+1,sizeof(struct apple)*(BATTLE->applec-i));
      }
    }
    return;
  }
  player->bobclock=0.0;
  if (battle->outcome>-2) return;
  const double radius=8.0;
  double focusx=player->dstx;
  if (player->xform) focusx-=20.0;
  else focusx+=20.0;
  struct apple *apple=BATTLE->applev+BATTLE->applec-1;
  int i=BATTLE->applec;
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
 
static void player_bob(struct battle *battle,struct player *player) {
  if (battle->outcome>-2) return;
  player->bobclock=player->bobtime;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (input!=player->pvinput) {
    if ((input&EGG_BTN_SOUTH)&&!(player->pvinput&EGG_BTN_SOUTH)) player_bob(battle,player);
    player->pvinput=input;
  }
}

/* Update CPU player.
 */
 
static int should_bob(struct battle *battle,struct player *player) {
  double focusx=player->dstx;
  if (player->xform) focusx-=20.0;
  else focusx+=20.0;
  struct apple *apple=BATTLE->applev;
  int i=BATTLE->applec;
  for (;i-->0;apple++) {
    double dx=apple->x-focusx;
    if (dx<-6.0) continue;
    if (dx>6.0) continue;
    return 1;
  }
  return 0;
}
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->cpuclock-=elapsed)>0.0) return;
  if (should_bob(battle,player)) {
    player->cpuclock+=player->cputime;
    player_bob(battle,player);
  }
}

/* Update apple.
 */
 
static void apple_update(struct battle *battle,struct apple *apple,double elapsed) {
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
 
static void _apples_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->bobclock>0.0) {
      if ((player->bobclock-=elapsed)<=0.0) {
        player_check_apples(battle,player);
      }
    } else if (player->eatclock>0.0) {
      player->eatclock-=elapsed;
    } else {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
  }
  
  struct apple *apple=BATTLE->applev;
  for (i=BATTLE->applec;i-->0;apple++) {
    apple_update(battle,apple,elapsed);
  }
  
  // First to 4 wins.
  // Ties are very unlikely. Break to the left, because that's how the scoreboard will render.
  if (battle->outcome==-2) {
    if (BATTLE->playerv[0].score>=4) {
      battle->outcome=1;
      BATTLE->end_cooldown=END_COOLDOWN;
    } else if (BATTLE->playerv[1].score>=4) {
      battle->outcome=-1;
      BATTLE->end_cooldown=END_COOLDOWN;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
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
 
static void apple_render(struct battle *battle,struct apple *apple) {
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
 
static void _apples_render(struct battle *battle) {

  // Background.
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,SKY_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,GROUND_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  
  // Sprites.
  graf_set_image(&g.graf,RID_image_battle_goblins);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  struct apple *apple=BATTLE->applev;
  int i=BATTLE->applec;
  for (;i-->0;apple++) apple_render(battle,apple);
  
  // Scoreboard
  const int lighty=GROUNDY+NS_sys_tilesize;
  const int lightspacing=NS_sys_tilesize;
  int lightsw=7*lightspacing;
  int lightsx=(FBW>>1)-(lightsw>>1)+(NS_sys_tilesize>>1);
  for (i=0;i<7;i++,lightsx+=lightspacing) {
    uint32_t color=0x808080ff;
    if (i<BATTLE->playerv[0].score) color=BATTLE->playerv[0].color;
    else if (i>=7-BATTLE->playerv[1].score) color=BATTLE->playerv[1].color;
    graf_fancy(&g.graf,lightsx,lighty,0xd2,0,0,NS_sys_tilesize,0,color);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_apples={
  .name="apples",
  .objlen=sizeof(struct battle_apples),
  .strix_name=58,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_apples_del,
  .init=_apples_init,
  .update=_apples_update,
  .render=_apples_render,
};

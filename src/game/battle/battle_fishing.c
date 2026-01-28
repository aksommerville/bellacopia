#include "game/bellacopia.h"

#define BASKET_LEVEL 120 /* Fish get caught only on the frame that they cross this. */
#define GROUND_LEVEL 140
#define SKY_COLOR 0x5e9fc7ff
#define GROUND_COLOR 0x126e29ff
#define DOT_X 100
#define CAT_X 172
#define COL0X 108
#define COL1X 140
#define COL2X 180
#define COL3X 212
#define FISH_LIMIT 16
#define GRAVITY 70.000 /* px/s (NB not acceleration) */
#define SPAWN_INTERVAL 0.750
#define MASS_MIN 1
#define MASS_MAX 25
#define FISH_TILE_FIRST 0x06
#define FISH_TILE_COUNT 10
#define DECISION_COUNT 15 /* How many sets of fish will fall. */
#define END_COOLDOWN_TIME 1.0

struct battle_fishing {
  uint8_t handicap;
  uint8_t players;
  void (*cb_end)(int outcome,void *userdata);
  void *userdata;
  int outcome;
  uint8_t dot_xform,cat_xform; // Natural orientation is rightward.
  int dot_mass,cat_mass;
  int dot_mass_disp,cat_mass_disp; // Displayed mass. Steps by 1 kg/frame when out of sync (managed at render).
  struct fish {
    int x; // Framebuffer pixels.
    double y; // Also framebuffer pixels, but motion is continuous.
    uint8_t tileid; // Zero if defunct.
    uint8_t xform; // Random, decorative.
    int mass;
    int col; // 0..3 = Dot L, Dot R, Cat L, Cat R
    int cat_prefer; // Cat should catch this one, whether heavier or not.
  } fishv[FISH_LIMIT];
  int fishc;
  double spawnclock;
  int decisionv[DECISION_COUNT]; // Nonzero if the cat will choose correctly on that round. The actual masses etc are chosen jit.
  int decisionp;
  double end_cooldown;
};

#define CTX ((struct battle_fishing*)ctx)

/* Delete.
 */
 
static void _fishing_del(void *ctx) {
  free(ctx);
}

/* 0..DECISION_COUNT-1, a random index where value is currently zero.
 */
 
static int fishing_random_zero_decision(void *ctx) {
  int panic=200;
  while (panic-->0) {
    int p=rand()%DECISION_COUNT;
    if (!CTX->decisionv[p]) return p;
  }
  return 0;
}

/* New.
 */
 
static void *_fishing_init(
  uint8_t handicap,
  uint8_t players,
  void (*cb_end)(int outcome,void *userdata),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_fishing));
  if (!ctx) return 0;
  CTX->handicap=handicap;
  CTX->players=players;
  CTX->cb_end=cb_end;
  CTX->userdata=userdata;
  CTX->cat_xform=EGG_XFORM_XREV; // Face each other initially.
  CTX->outcome=-2;
  
  /* Prepare the decision list.
   * We decide in advance how many mistakes the cat will make, directly proportionate to the handicap.
   * There will always be at least one correct decision and at least one incorrect.
   */
  if ((CTX->players==NS_players_man_cpu)||(CTX->players==NS_players_cpu_cpu)) {
    int correctc=1+((handicap*(DECISION_COUNT-2))>>8);
    if (correctc<1) correctc=1; else if (correctc>=DECISION_COUNT) correctc=DECISION_COUNT-1;
    while (correctc-->0) {
      int p=fishing_random_zero_decision(ctx);
      CTX->decisionv[p]=1;
    }
  }
  
  return ctx;
}

/* If space exists, spawn four new fish.
 * Each fish gets spawned twice; Dot and the Cat both see the same two fish.
 */
 
static void fishing_spawn(void *ctx) {

  // If we're already over the limit, get out. Shouldn't have called us.
  if (CTX->decisionp>=DECISION_COUNT) return;

  // First drop any defunct.
  int i=CTX->fishc;
  struct fish *fish=CTX->fishv+i-1;
  for (;i-->0;fish--) {
    if (fish->tileid) continue;
    CTX->fishc--;
    memmove(fish,fish+1,sizeof(struct fish)*(CTX->fishc-i));
  }
  
  // If we don't have at least 4 slots available, forget it.
  if (CTX->fishc>FISH_LIMIT-4) return;
  
  // Select the two masses. Then tileid is related to mass.
  int massa=MASS_MIN+(((MASS_MAX-MASS_MIN)*(rand()&0xffff))>>16);
  int massb;
  do {
    massb=MASS_MIN+(((MASS_MAX-MASS_MIN)*(rand()&0xffff))>>16);
  } while (massa==massb); // Don't let masses be equal.
  int tileida=(massa*FISH_TILE_COUNT)/(MASS_MAX-MASS_MIN);
  int tileidb=(massb*FISH_TILE_COUNT)/(MASS_MAX-MASS_MIN);
  if (tileida<0) tileida=0; else if (tileida>=FISH_TILE_COUNT) tileida=FISH_TILE_COUNT-1;
  if (tileidb<0) tileidb=0; else if (tileidb>=FISH_TILE_COUNT) tileidb=FISH_TILE_COUNT-1;
  tileida+=FISH_TILE_FIRST;
  tileidb+=FISH_TILE_FIRST;
  
  // If the tiles ended up the same, knock one up or down. They should always be visually different.
  if (tileida==tileidb) {
    if (massa<massb) {
      if (tileida==FISH_TILE_FIRST) tileidb++;
      else tileida--;
    } else {
      if (tileidb==FISH_TILE_FIRST) tileida++;
      else tileidb--;
    }
  }
  
  // Spawn.
  fish=CTX->fishv+CTX->fishc++;
  fish->x=COL0X;
  fish->y=-8.0;
  fish->col=0;
  fish->mass=massa;
  fish->tileid=tileida;
  fish->xform=rand()&EGG_XFORM_XREV;
  
  fish=CTX->fishv+CTX->fishc++;
  fish->x=COL1X;
  fish->y=-8.0;
  fish->col=1;
  fish->mass=massb;
  fish->tileid=tileidb;
  fish->xform=rand()&EGG_XFORM_XREV;
  
  struct fish *catl=CTX->fishv+CTX->fishc++;
  catl->x=COL2X;
  catl->y=-8.0;
  catl->col=2;
  catl->xform=rand()&EGG_XFORM_XREV;
  
  struct fish *catr=CTX->fishv+CTX->fishc++;
  catr->x=COL3X;
  catr->y=-8.0;
  catr->col=3;
  catr->xform=rand()&EGG_XFORM_XREV;
  
  // The two sides don't always match left/right.
  if (rand()&1) {
    catl->mass=massa;
    catl->tileid=tileida;
    catr->mass=massb;
    catr->tileid=tileidb;
  } else {
    catl->mass=massb;
    catl->tileid=tileidb;
    catr->mass=massa;
    catr->tileid=tileida;
  }
  
  // Whether the cat will choose correctly was determined in advance at init.
  catl->cat_prefer=catr->cat_prefer=0;
  if (CTX->decisionv[CTX->decisionp++]) {
    if (catl->mass>catr->mass) catl->cat_prefer=1;
    else catr->cat_prefer=1;
  } else {
    if (catl->mass>catr->mass) catr->cat_prefer=1;
    else catl->cat_prefer=1;
  }
}

/* Update one fish.
 */
 
static void fish_update(void *ctx,struct fish *fish,double elapsed) {
  int yia=(int)fish->y;
  fish->y+=GRAVITY*elapsed;
  int yiz=(int)fish->y;
  if (yiz>=GROUND_LEVEL) {
    fish->tileid=0;
    return;
  }
  if ((yia<BASKET_LEVEL)&&(yiz>=BASKET_LEVEL)) {
    bm_sound(RID_sound_collect); // Tempting to pan based on (fish->col), but there will always be two collects at the same moment, it's pointless.
    switch (fish->col) {
      case 0: if (CTX->dot_xform&EGG_XFORM_XREV) {
          CTX->dot_mass+=fish->mass;
          fish->tileid=0;
        } break;
      case 1: if (!(CTX->dot_xform&EGG_XFORM_XREV)) {
          CTX->dot_mass+=fish->mass;
          fish->tileid=0;
        } break;
      case 2: if (CTX->cat_xform&EGG_XFORM_XREV) {
          CTX->cat_mass+=fish->mass;
          fish->tileid=0;
        } break;
      case 3: if (!(CTX->cat_xform&EGG_XFORM_XREV)) {
          CTX->cat_mass+=fish->mass;
          fish->tileid=0;
        } break;
    }
  }
}

/* Update a manual player.
 */
 
static void fishing_update_manual(void *ctx,uint8_t *xform,int input,int pvinput) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: *xform=EGG_XFORM_XREV; break;
    case EGG_BTN_RIGHT: *xform=0; break;
  }
}

/* Update a CPU player.
 */
 
static void fishing_update_auto(void *ctx,uint8_t *xform,int coll,int colr) {
  // Find the nearest fish in each column, whose (y) is above BASKET_LEVEL.
  struct fish *fishl=0,*fishr=0;
  struct fish *fish=CTX->fishv;
  int i=CTX->fishc;
  for (;i-->0;fish++) {
    if (!fish->tileid) continue;
    if (fish->y>=BASKET_LEVEL) continue;
    if (fish->col==coll) {
      if (!fishl||(fish->y>fishl->y)) fishl=fish;
    } else if (fish->col==colr) {
      if (!fishr||(fish->y>fishr->y)) fishr=fish;
    }
  }
  // If either one is missing, stay put. This shouldn't happen. But maybe briefly. Who cares.
  if (!fishl||!fishr) return;
  // Exactly one of them should have (cat_prefer) set. Prefer that one.
  //TODO cpu-vs-cpu, they make the same decisions. This was written before consideration of the cpu-vs-cpu case. Revisit that.
  if (fishl->cat_prefer) *xform=EGG_XFORM_XREV;
  else if (fishr->cat_prefer) *xform=0;
}

/* Test completion.
 * If all fish are gone, set (outcome) and return nonzero.
 */
 
static int fishing_finished(void *ctx) {
  struct fish *fish=CTX->fishv;
  int i=CTX->fishc;
  for (;i-->0;fish++) {
    if (fish->tileid) return 0;
  }
  if (CTX->dot_mass>CTX->cat_mass) {
    CTX->outcome=1;
  } else if (CTX->dot_mass<CTX->cat_mass) {
    CTX->outcome=-1;
  } else {
    CTX->outcome=0;
  }
  return 1;
}

/* Update.
 */
 
static void _fishing_update(void *ctx,double elapsed) {
  if (CTX->outcome>-2) {
    if (CTX->cb_end) {
      if (CTX->end_cooldown>0.0) {
        if ((CTX->end_cooldown-=elapsed)<=0.0) {
          CTX->cb_end(CTX->outcome,CTX->userdata);
          CTX->cb_end=0;
        }
      }
    }
    return;
  }

  struct fish *fish=CTX->fishv;
  int i=CTX->fishc;
  for (;i-->0;fish++) {
    if (!fish->tileid) continue;
    fish_update(ctx,fish,elapsed);
  }
  
  if (CTX->decisionp<DECISION_COUNT) {
    if ((CTX->spawnclock-=elapsed)<=0.0) {
      CTX->spawnclock+=SPAWN_INTERVAL;
      fishing_spawn(ctx);
    }
  } else {
    if (fishing_finished(ctx)) {
      CTX->end_cooldown=END_COOLDOWN_TIME;
      return;
    }
  }
  
  switch (CTX->players) {
    case NS_players_cpu_cpu: {
        fishing_update_auto(ctx,&CTX->dot_xform,0,1);
        fishing_update_auto(ctx,&CTX->cat_xform,2,3);
      } break;
    case NS_players_cpu_man: {
        fishing_update_auto(ctx,&CTX->dot_xform,0,1);
        fishing_update_manual(ctx,&CTX->cat_xform,g.input[2],g.pvinput[2]);
      } break;
    case NS_players_man_cpu: {
        fishing_update_manual(ctx,&CTX->dot_xform,g.input[1],g.pvinput[1]);
        fishing_update_auto(ctx,&CTX->cat_xform,2,3);
      } break;
    case NS_players_man_man: {
        fishing_update_manual(ctx,&CTX->dot_xform,g.input[1],g.pvinput[1]);
        fishing_update_manual(ctx,&CTX->cat_xform,g.input[2],g.pvinput[2]);
      } break;
  }
}

/* Render one scale. Caller loads image.
 * Size is 32x32.
 */
 
static void fishing_render_scale(int dstx,int dsty,int mass) {
  if (mass<0) mass=0; else if (mass>999) mass=999;
  graf_decal(&g.graf,dstx,dsty,96,16,32,32);
  graf_tile(&g.graf,dstx+ 6,dsty+15,0x30+(mass/100)%10,0);
  graf_tile(&g.graf,dstx+12,dsty+15,0x30+(mass/ 10)%10,0);
  graf_tile(&g.graf,dstx+18,dsty+15,0x30+(mass    )%10,0);
}

/* Render.
 */
 
static void _fishing_render(void *ctx) {
  graf_fill_rect(&g.graf,0,0,FBW,GROUND_LEVEL,SKY_COLOR);
  graf_fill_rect(&g.graf,0,GROUND_LEVEL,FBW,FBH-GROUND_LEVEL,GROUND_COLOR);
  graf_fill_rect(&g.graf,0,GROUND_LEVEL,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_fishing);
  if (CTX->dot_mass_disp<CTX->dot_mass) CTX->dot_mass_disp++;
  if (CTX->cat_mass_disp<CTX->cat_mass) CTX->cat_mass_disp++;
  fishing_render_scale(DOT_X-32-32,GROUND_LEVEL-32,CTX->dot_mass_disp);
  fishing_render_scale(CAT_X+48+32,GROUND_LEVEL-32,CTX->cat_mass_disp);
  const int playerw=48;
  const int playerh=48;
  graf_decal_xform(&g.graf,DOT_X,GROUND_LEVEL-playerh,0,0,playerw,playerh,CTX->dot_xform);
  graf_decal_xform(&g.graf,CAT_X,GROUND_LEVEL-playerh,playerw,0,playerw,playerh,CTX->cat_xform);
  struct fish *fish=CTX->fishv;
  int i=CTX->fishc;
  for (;i-->0;fish++) {
    if (!fish->tileid) continue;
    graf_tile(&g.graf,fish->x,(int)fish->y,fish->tileid,fish->xform);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_fishing={
  .name="fishing",
  .strix_name=13,
  .no_article=0,
  .no_contest=0,
  .supported_players=(1<<NS_players_cpu_cpu)|(1<<NS_players_cpu_man)|(1<<NS_players_man_cpu)|(1<<NS_players_man_man),
  .del=_fishing_del,
  .init=_fishing_init,
  .update=_fishing_update,
  .render=_fishing_render,
};

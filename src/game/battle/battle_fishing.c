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
  struct battle hdr;
  uint8_t dot_xform,cat_xform; // Natural orientation is rightward.
  int dot_srcx,dot_srcy;
  int cat_srcx,cat_srcy;
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

#define BATTLE ((struct battle_fishing*)battle)

/* Delete.
 */
 
static void _fishing_del(struct battle *battle) {
}

/* 0..DECISION_COUNT-1, a random index where value is currently zero.
 */
 
static int fishing_random_zero_decision(struct battle *battle) {
  int panic=200;
  while (panic-->0) {
    int p=rand()%DECISION_COUNT;
    if (!BATTLE->decisionv[p]) return p;
  }
  return 0;
}

/* New.
 */

static int _fishing_init(struct battle *battle) {
  BATTLE->cat_xform=EGG_XFORM_XREV; // Face each other initially.
  
  /* Prepare images.
   * We have just one image per player, it never changes.
   */
  switch (battle->args.lface) {
    case NS_face_monster: {
        BATTLE->dot_srcx=48;
        BATTLE->dot_srcy=0;
      } break;
    case NS_face_dot: {
        BATTLE->dot_srcx=0;
        BATTLE->dot_srcy=0;
      } break;
    case NS_face_princess: {
        BATTLE->dot_srcx=192;
        BATTLE->dot_srcy=64;
      } break;
  }
  switch (battle->args.rface) {
    case NS_face_monster: {
        BATTLE->cat_srcx=48;
        BATTLE->cat_srcy=0;
      } break;
    case NS_face_dot: {
        BATTLE->cat_srcx=0;
        BATTLE->cat_srcy=0;
      } break;
    case NS_face_princess: {
        BATTLE->cat_srcx=192;
        BATTLE->cat_srcy=64;
      } break;
  }
  
  /* Prepare the decision list.
   * We decide in advance how many mistakes the cat will make, directly proportionate to the handicap.
   * There will always be at least one correct decision and at least one incorrect.
   */
  if (!battle->args.lctl||!battle->args.rctl) {
    //TODO Need separate decision lists, if both players are cpu
    int correctc=1+((battle->args.bias*(DECISION_COUNT-2))>>8);
    if (correctc<1) correctc=1; else if (correctc>=DECISION_COUNT) correctc=DECISION_COUNT-1;
    while (correctc-->0) {
      int p=fishing_random_zero_decision(battle);
      BATTLE->decisionv[p]=1;
    }
  }
  
  return 0;
}

/* If space exists, spawn four new fish.
 * Each fish gets spawned twice; Dot and the Cat both see the same two fish.
 */
 
static void fishing_spawn(struct battle *battle) {

  // If we're already over the limit, get out. Shouldn't have called us.
  if (BATTLE->decisionp>=DECISION_COUNT) return;

  // First drop any defunct.
  int i=BATTLE->fishc;
  struct fish *fish=BATTLE->fishv+i-1;
  for (;i-->0;fish--) {
    if (fish->tileid) continue;
    BATTLE->fishc--;
    memmove(fish,fish+1,sizeof(struct fish)*(BATTLE->fishc-i));
  }
  
  // If we don't have at least 4 slots available, forget it.
  if (BATTLE->fishc>FISH_LIMIT-4) return;
  
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
  fish=BATTLE->fishv+BATTLE->fishc++;
  fish->x=COL0X;
  fish->y=-8.0;
  fish->col=0;
  fish->mass=massa;
  fish->tileid=tileida;
  fish->xform=rand()&EGG_XFORM_XREV;
  
  fish=BATTLE->fishv+BATTLE->fishc++;
  fish->x=COL1X;
  fish->y=-8.0;
  fish->col=1;
  fish->mass=massb;
  fish->tileid=tileidb;
  fish->xform=rand()&EGG_XFORM_XREV;
  
  struct fish *catl=BATTLE->fishv+BATTLE->fishc++;
  catl->x=COL2X;
  catl->y=-8.0;
  catl->col=2;
  catl->xform=rand()&EGG_XFORM_XREV;
  
  struct fish *catr=BATTLE->fishv+BATTLE->fishc++;
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
  if (BATTLE->decisionv[BATTLE->decisionp++]) {
    if (catl->mass>catr->mass) catl->cat_prefer=1;
    else catr->cat_prefer=1;
  } else {
    if (catl->mass>catr->mass) catr->cat_prefer=1;
    else catl->cat_prefer=1;
  }
}

/* Update one fish.
 */
 
static void fish_update(struct battle *battle,struct fish *fish,double elapsed) {
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
      case 0: if (BATTLE->dot_xform&EGG_XFORM_XREV) {
          BATTLE->dot_mass+=fish->mass;
          fish->tileid=0;
        } break;
      case 1: if (!(BATTLE->dot_xform&EGG_XFORM_XREV)) {
          BATTLE->dot_mass+=fish->mass;
          fish->tileid=0;
        } break;
      case 2: if (BATTLE->cat_xform&EGG_XFORM_XREV) {
          BATTLE->cat_mass+=fish->mass;
          fish->tileid=0;
        } break;
      case 3: if (!(BATTLE->cat_xform&EGG_XFORM_XREV)) {
          BATTLE->cat_mass+=fish->mass;
          fish->tileid=0;
        } break;
    }
  }
}

/* Update a manual player.
 */
 
static void fishing_update_manual(struct battle *battle,uint8_t *xform,int input,int pvinput) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: *xform=EGG_XFORM_XREV; break;
    case EGG_BTN_RIGHT: *xform=0; break;
  }
}

/* Update a CPU player.
 */
 
static void fishing_update_auto(struct battle *battle,uint8_t *xform,int coll,int colr) {
  // Find the nearest fish in each column, whose (y) is above BASKET_LEVEL.
  struct fish *fishl=0,*fishr=0;
  struct fish *fish=BATTLE->fishv;
  int i=BATTLE->fishc;
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
 
static int fishing_finished(struct battle *battle) {
  struct fish *fish=BATTLE->fishv;
  int i=BATTLE->fishc;
  for (;i-->0;fish++) {
    if (fish->tileid) return 0;
  }
  if (BATTLE->dot_mass>BATTLE->cat_mass) {
    battle->outcome=1;
  } else if (BATTLE->dot_mass<BATTLE->cat_mass) {
    battle->outcome=-1;
  } else {
    battle->outcome=0;
  }
  return 1;
}

/* Update.
 */
 
static void _fishing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;

  struct fish *fish=BATTLE->fishv;
  int i=BATTLE->fishc;
  for (;i-->0;fish++) {
    if (!fish->tileid) continue;
    fish_update(battle,fish,elapsed);
  }
  
  if (BATTLE->decisionp<DECISION_COUNT) {
    if ((BATTLE->spawnclock-=elapsed)<=0.0) {
      BATTLE->spawnclock+=SPAWN_INTERVAL;
      fishing_spawn(battle);
    }
  } else {
    if (fishing_finished(battle)) {
      BATTLE->end_cooldown=END_COOLDOWN_TIME;
      return;
    }
  }
  
  if (battle->args.lctl) fishing_update_manual(battle,&BATTLE->dot_xform,g.input[battle->args.lctl],g.pvinput[battle->args.lctl]);
  else fishing_update_auto(battle,&BATTLE->dot_xform,0,1);
  if (battle->args.rctl) fishing_update_manual(battle,&BATTLE->cat_xform,g.input[battle->args.rctl],g.pvinput[battle->args.rctl]);
  else fishing_update_auto(battle,&BATTLE->cat_xform,2,3);
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
 
static void _fishing_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,GROUND_LEVEL,SKY_COLOR);
  graf_fill_rect(&g.graf,0,GROUND_LEVEL,FBW,FBH-GROUND_LEVEL,GROUND_COLOR);
  graf_fill_rect(&g.graf,0,GROUND_LEVEL,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_fishing);
  if (BATTLE->dot_mass_disp<BATTLE->dot_mass) BATTLE->dot_mass_disp++;
  if (BATTLE->cat_mass_disp<BATTLE->cat_mass) BATTLE->cat_mass_disp++;
  fishing_render_scale(DOT_X-32-32,GROUND_LEVEL-32,BATTLE->dot_mass_disp);
  fishing_render_scale(CAT_X+48+32,GROUND_LEVEL-32,BATTLE->cat_mass_disp);
  const int playerw=48;
  const int playerh=48;
  graf_decal_xform(&g.graf,DOT_X,GROUND_LEVEL-playerh,BATTLE->dot_srcx,BATTLE->dot_srcy,playerw,playerh,BATTLE->dot_xform);
  graf_decal_xform(&g.graf,CAT_X,GROUND_LEVEL-playerh,BATTLE->cat_srcx,BATTLE->cat_srcy,playerw,playerh,BATTLE->cat_xform);
  struct fish *fish=BATTLE->fishv;
  int i=BATTLE->fishc;
  for (;i-->0;fish++) {
    if (!fish->tileid) continue;
    graf_tile(&g.graf,fish->x,(int)fish->y,fish->tileid,fish->xform);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_fishing={
  .name="fishing",
  .objlen=sizeof(struct battle_fishing),
  .strix_name=13,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_fishing_del,
  .init=_fishing_init,
  .update=_fishing_update,
  .render=_fishing_render,
};

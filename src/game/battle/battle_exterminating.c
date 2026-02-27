#include "game/bellacopia.h"

#define FLDW 5
#define FLDH 5
#define END_COOLDOWN_TIME 1.0
#define THINKYTIME_MIN 0.128
#define THINKYTIME_MAX 0.450
#define THINKYTIME_FUZZ 0.050 /* Add up to so much to each cycle, randomly. */
#define THINKYTIME_POSTMOVE 0.100 /* Extra delay after each play, makes it feel more human. */

struct battle_exterminating {
  struct battle hdr;
  double end_cooldown;
  double canimclock;
  int canimframe;
  uint8_t cxform;
  double xformclock;
  uint8_t xformv[FLDW*FLDH];
  struct player {
    int who; // 0,1: My index.
    int human; // 0=cpu, or playerid
    uint8_t fld[FLDW*FLDH]; // 0=vacant, 1=bug
    int cx,cy; // -1..FLDW,FLDH
    int clear;
    double thinkyclock; // Counts down, for CPU player cycles.
    double thinkytime; // Constant interval between cycles, determined by handicap.
    int targetx,targety; // For CPU.
    int target_acquired;
  } playerv[2];
};

#define BATTLE ((struct battle_exterminating*)battle)

/* Delete.
 */
 
static void _exterminating_del(struct battle *battle) {
}

/* New.
 */

static int _exterminating_init(struct battle *battle) {
  
  // Fill xformv randomly. This is entirely decorative.
  uint8_t *p=BATTLE->xformv;
  int i=FLDW*FLDH;
  for (;i-->0;p++) *p=rand()&7;
  
  // Bug half of player zero's field, then copy it to player one. They start with the same field.
  for (i=(FLDW*FLDH)>>1;i-->0;) {
    for (;;) {
      int ix=rand()%(FLDW*FLDH);
      if (BATTLE->playerv[0].fld[ix]) continue;
      BATTLE->playerv[0].fld[ix]=1;
      break;
    }
  }
  memcpy(BATTLE->playerv[1].fld,BATTLE->playerv[0].fld,FLDW*FLDH);
  
  // Both players start in the middle.
  BATTLE->playerv[0].cx=BATTLE->playerv[1].cx=FLDW>>1;
  BATTLE->playerv[0].cy=BATTLE->playerv[1].cy=FLDH>>1;
  BATTLE->playerv[0].human=battle->args.lctl;
  BATTLE->playerv[1].human=battle->args.rctl;
  BATTLE->playerv[0].who=0;
  BATTLE->playerv[1].who=1;
  // (thinkytime) only matters for CPU players, but no harm setting it in both.
  // Note that (handicap) is not used in man-vs-man mode.
  BATTLE->playerv[0].thinkytime=THINKYTIME_MAX+((THINKYTIME_MIN-THINKYTIME_MAX)*(0xff-battle->args.bias))/255.0;
  BATTLE->playerv[1].thinkytime=THINKYTIME_MAX+((THINKYTIME_MIN-THINKYTIME_MAX)*battle->args.bias)/255.0;
  BATTLE->playerv[0].targetx=BATTLE->playerv[0].targety=-1;
  BATTLE->playerv[1].targetx=BATTLE->playerv[1].targety=-1;
  
  return 0;
}

/* Swap tiles under cursor.
 */
 
static void player_swap_1(struct battle *battle,struct player *player,int x,int y) {
  if ((x<0)||(x>=FLDW)||(y<0)||(y>=FLDH)) return;
  player->fld[y*FLDW+x]^=1;
}
 
static void player_swap(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_uiactivate,player->who?0.250:-0.250);
  player_swap_1(battle,player,player->cx,player->cy-1);
  player_swap_1(battle,player,player->cx-1,player->cy);
  player_swap_1(battle,player,player->cx,player->cy);
  player_swap_1(battle,player,player->cx+1,player->cy);
  player_swap_1(battle,player,player->cx,player->cy+1);
  // Am I clear now?
  player->clear=0;
  const uint8_t *v=player->fld;
  int i=FLDW*FLDH;
  for (;i-->0;v++) if (*v) return;
  player->clear=1;
}

/* Move cursor.
 */
 
static void player_move(struct battle *battle,struct player *player,int dx,int dy) {
  player->cx+=dx;
  player->cy+=dy;
  if (player->cx<-1) player->cx=-1; else if (player->cx>FLDW) player->cx=FLDW;
  if (player->cy<-1) player->cy=-1; else if (player->cy>FLDH) player->cy=FLDH;
  bm_sound_pan(RID_sound_uimotion,player->who?0.250:-0.250);
}

/* Update human player.
 */
 
static void player_update_human(struct battle *battle,struct player *player,double elapsed) {
  if ((g.input[player->human]&EGG_BTN_LEFT)&&!(g.pvinput[player->human]&EGG_BTN_LEFT)) player_move(battle,player,-1,0);
  if ((g.input[player->human]&EGG_BTN_RIGHT)&&!(g.pvinput[player->human]&EGG_BTN_RIGHT)) player_move(battle,player,1,0);
  if ((g.input[player->human]&EGG_BTN_UP)&&!(g.pvinput[player->human]&EGG_BTN_UP)) player_move(battle,player,0,-1);
  if ((g.input[player->human]&EGG_BTN_DOWN)&&!(g.pvinput[player->human]&EGG_BTN_DOWN)) player_move(battle,player,0,1);
  if ((g.input[player->human]&EGG_BTN_SOUTH)&&!(g.pvinput[player->human]&EGG_BTN_SOUTH)) player_swap(battle,player);
}

/* Search and filter ops for CPU player.
 */

struct target {
  int8_t x,y; // NB can be negative!
  uint8_t cellc; // How many real cells does it touch, 0..5. Doesn't depend on field state.
  uint8_t addmidc,rmmidc; // How many bugs added or removed at the center, 0..1.
  uint8_t addinc,rminc; // How many bugs added or removed in the inner 3x3 ring, 0..4.
  uint8_t addoutc,rmoutc; // How many bugs added or removed in the outer 5x5 ring, 0..3.
}; // (any)'s fields are nonzero if there's at least one target with a nonzero value there.

static int targets_has_rmmidc1(const struct target *targetv,int targetc) {
  for (;targetc-->0;targetv++) {
    if (targetv->rmmidc) return 1;
  }
  return 0;
}

static int targets_filter_rmmidc1(struct target *targetv,int targetc) {
  int i=targetc;
  struct target *target=targetv+targetc-1;
  for (;i-->0;target--) {
    if (!target->rmmidc) {
      targetc--;
      memmove(target,target+1,sizeof(struct target)*(targetc-i));
    }
  }
  return targetc;
}

static int targets_filter_addmidc0(struct target *targetv,int targetc) {
  int i=targetc;
  struct target *target=targetv+targetc-1;
  for (;i-->0;target--) {
    if (target->addmidc) {
      targetc--;
      memmove(target,target+1,sizeof(struct target)*(targetc-i));
    }
  }
  return targetc;
}

static int targets_rminc_best(const struct target *targetv,int targetc) {
  int best=0;
  for (;targetc-->0;targetv++) {
    int score=targetv->rminc;
    if (score>best) best=score;
  }
  return best;
}

static int targets_filter_rminc(struct target *targetv,int targetc,int best) {
  int i=targetc;
  struct target *target=targetv+targetc-1;
  for (;i-->0;target--) {
    int score=target->rminc;
    if (score<best) {
      targetc--;
      memmove(target,target+1,sizeof(struct target)*(targetc-i));
    }
  }
  return targetc;
}

static int targets_rmtotal_best(const struct target *targetv,int targetc) {
  int best=-5;
  for (;targetc-->0;targetv++) {
    int score=targetv->rminc+targetv->rmmidc+targetv->rmoutc-targetv->addinc-targetv->addmidc-targetv->addoutc;
    if (score>best) best=score;
  }
  return best;
}

static int targets_filter_rmtotal(struct target *targetv,int targetc,int best) {
  int i=targetc;
  struct target *target=targetv+targetc-1;
  for (;i-->0;target--) {
    int score=target->rminc+target->rmmidc+target->rmoutc-target->addinc-target->addmidc-target->addoutc;
    if (score<best) {
      targetc--;
      memmove(target,target+1,sizeof(struct target)*(targetc-i));
    }
  }
  return targetc;
}

static int targets_mdist_best(const struct target *targetv,int targetc,int x,int y) {
  int best=100;
  for (;targetc-->0;targetv++) {
    int dx=targetv->x-x; if (dx<0) dx=-dx;
    int dy=targetv->y-y; if (dy<0) dy=-dy;
    int score=dx+dy;
    if (!score) return 0;
    if (score<best) best=score;
  }
  return best;
}

static int targets_filter_mdist(struct target *targetv,int targetc,int x,int y,int best) {
  int i=targetc;
  struct target *target=targetv+targetc-1;
  for (;i-->0;target--) {
    int dx=target->x-x; if (dx<0) dx=-dx;
    int dy=target->y-y; if (dy<0) dy=-dy;
    int score=dx+dy;
    if (score>best) {
      targetc--;
      memmove(target,target+1,sizeof(struct target)*(targetc-i));
    }
  }
  return targetc;
}

/* Choose next target for CPU player.
 * I'm using a strategy the works inside-out. Clear the middle cell, then the intermediate layer, then the outer layer.
 * That's probably not the mathematical ideal in every case, and that's OK.
 */
 
static void player_choose_target(struct battle *battle,struct player *player) {
  #define MW (FLDW+2)
  #define MH (FLDH+2)
  struct target targetv[MW*MH];
  int targetc=0;
  
  /* Tally the bugs by ring.
   * There are three rings, and once a ring is cleared, we will only touch those outside it.
   * ie we'll never create a bug on the inside of the current set.
   */
  int midc=0,inc=0,outc=0;
  const uint8_t *src=player->fld;
  int y=0; for (;y<FLDH;y++) {
    int x=0; for (;x<FLDW;x++,src++) {
      if (!*src) continue;
      if (!x||!y||(x==FLDW-1)||(y==FLDH-1)) outc++;
      else if ((x==1)||(y==1)||(x==FLDW-2)||(y==FLDH-2)) inc++;
      else midc++;
    }
  }
  
  /* Assess every possible position, all 49 of them.
   */
  for (y=-1;y<6;y++) {
    int x=-1; for (;x<6;x++) {
      struct target *target=targetv+targetc++;
      memset(target,0,sizeof(struct target)); // Must zero each time, since we might have backed out a prior step.
      target->x=x;
      target->y=y;
      #define CHECK1(_x,_y) { \
        int cx=(_x),cy=(_y); \
        if ((cx>=0)&&(cy>=0)&&(cx<FLDW)&&(cy<FLDH)) { \
          target->cellc++; \
          int isbug=player->fld[cy*FLDW+cx]; \
          int depth; \
          if ((cx==2)&&(cy==2)) depth=3; \
          else if ((cx>=1)&&(cy>=1)&&(cx<=3)&&(cy<=3)) depth=2; \
          else depth=1; \
          if (isbug) switch (depth) { \
            case 1: target->rmoutc++; break; \
            case 2: target->rminc++; break; \
            case 3: target->rmmidc++; break; \
          } else switch (depth) { \
            case 1: target->addoutc++; break; \
            case 2: target->addinc++; break; \
            case 3: target->addmidc++; break; \
          } \
        } \
      }
      CHECK1(x,y)
      CHECK1(x-1,y)
      CHECK1(x+1,y)
      CHECK1(x,y-1)
      CHECK1(x,y+1)
      #undef CHECK1
      // Discard any target that adds more than it removes at the inner ring.
      if (target->addinc>target->rminc) { targetc--; continue; }
      // Discard any targets that don't remove bugs, or that add bugs to a cleared ring.
      if (!(target->rmoutc+target->rminc+target->rmmidc)) { // Doesn't kill any bugs.
        targetc--;
      } else if (midc) { // There's a bug in the middle. We only want targets that remove it.
        if (!target->rmmidc) targetc--;
      } else if (target->addmidc) { // Forbid adding to middle if it was clear.
        targetc--;
      } else if (inc) { // There's a bug in the inner ring. We only want targets that remove it.
        if (!target->rminc) targetc--;
      } else if (target->addinc) { // Forbid adding to inner ring if it was clear.
        targetc--;
      }
    }
  }
  
  /* If anything removes from the middle, filter to just those.
   * Otherwise, filter out those that add to the middle.
   */
  if (targets_has_rmmidc1(targetv,targetc)) targetc=targets_filter_rmmidc1(targetv,targetc);
  else targetc=targets_filter_addmidc0(targetv,targetc);
  
  /* Find the best-case of removals from the intermediate ring, and filter to those.
   */
  int rminc_best=targets_rminc_best(targetv,targetc);
  targetc=targets_filter_rminc(targetv,targetc,rminc_best);
  
  /* Find the best-case of total removals, and filter to those.
   */
  int rmtotal_best=targets_rmtotal_best(targetv,targetc);
  targetc=targets_filter_rmtotal(targetv,targetc,rmtotal_best);
  
  /* Filter by Manhattan distance to current cursor.
   */
  int mdist_best=targets_mdist_best(targetv,targetc,player->cx,player->cy);
  targetc=targets_filter_mdist(targetv,targetc,player->cx,player->cy,mdist_best);
  
  /* Did I mess this up?
   * If there's no targets remaining, go to dead center and toggle away, so I notice.
   */
  if (targetc<1) {
    fprintf(stderr,"***** %s:%d: No targets! *****\n",__FILE__,__LINE__);
    player->targetx=2;
    player->targety=2;
    return;
  }
  
  /* The remaining targets are equally good. Pick at random.
   * There's usually just one, so only call rand() if we actually need to.
   */
  struct target *target=targetv;
  if (targetc>1) target+=rand()%targetc;
  player->targetx=target->x;
  player->targety=target->y;
  
  #undef MW
  #undef MH
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->thinkyclock-=elapsed)>0.0) return;
  player->thinkyclock+=player->thinkytime+((rand()&0xffff)*THINKYTIME_FUZZ)/65535.0;
  
  if (!player->target_acquired) {
    player->target_acquired=1;
    player_choose_target(battle,player);
    player->thinkyclock+=THINKYTIME_POSTMOVE;
    return;
  }
  
  int dx=player->targetx-player->cx;
  int dy=player->targety-player->cy;
  if (!dx&&!dy) {
    player_swap(battle,player);
    player->target_acquired=0;
  } else {
    int adx=dx; if (adx<0) adx=-adx;
    int ady=dy; if (ady<0) ady=-ady;
    if (dx&&((adx<=ady)||!dy)) player_move(battle,player,(dx<0)?-1:1,0);
    else player_move(battle,player,0,(dy<0)?-1:1);
  }
}

/* Update.
 */
 
static void _exterminating_update(struct battle *battle,double elapsed) {
  if ((BATTLE->xformclock-=elapsed)<=0.0) {
    BATTLE->xformclock+=0.100;
    int p=rand()%(FLDW*FLDH);
    BATTLE->xformv[p]=rand()&7;
  }
  
  if (battle->outcome<=-2) {
    if ((BATTLE->canimclock-=elapsed)<=0.0) {
      BATTLE->canimclock+=0.100;
      if (++(BATTLE->canimframe)>=4) BATTLE->canimframe=0;
      switch (BATTLE->canimframe) {
        case 0: BATTLE->cxform=0; break;
        case 1: BATTLE->cxform=EGG_XFORM_SWAP|EGG_XFORM_XREV; break;
        case 2: BATTLE->cxform=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
        case 3: BATTLE->cxform=EGG_XFORM_YREV|EGG_XFORM_SWAP; break;
      }
    }
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (player->human) player_update_human(battle,player,elapsed);
      else player_update_cpu(battle,player,elapsed);
    }
    if (BATTLE->playerv[0].clear) {
      if (BATTLE->playerv[1].clear) { // It's just barely possible that they clear at the same moment.
        battle->outcome=0;
      } else {
        battle->outcome=1;
      }
      BATTLE->end_cooldown=END_COOLDOWN_TIME;
    } else if (BATTLE->playerv[1].clear) {
      battle->outcome=-1;
      BATTLE->end_cooldown=END_COOLDOWN_TIME;
    }
  }
}

/* Render one player's field.
 */
 
static void player_render(struct battle *battle,struct player *player,int midx) {
  const int tilesize=16;
  int x0=midx-((FLDW*tilesize)>>1)+(tilesize>>1);
  int y0=(FBH>>1)-((FLDH*tilesize)>>1)+(tilesize>>1);
  
  int y=y0;
  const uint8_t *p=player->fld;
  const uint8_t *xform=BATTLE->xformv;
  int yi=FLDH;
  for (;yi-->0;y+=tilesize) {
    int x=x0;
    int xi=FLDW;
    for (;xi-->0;x+=tilesize,p++,xform++) {
      graf_tile(&g.graf,x,y,((xi&1)==(yi&1))?0x7b:0x8b,0);
      if (*p) graf_tile(&g.graf,x,y,0x8a,*xform);
    }
  }
  
  if (battle->outcome<=-2) {
    graf_tile(&g.graf,x0+(player->cx  )*tilesize,y0+(player->cy-1)*tilesize,0x7a,BATTLE->cxform);
    graf_tile(&g.graf,x0+(player->cx-1)*tilesize,y0+(player->cy  )*tilesize,0x7a,BATTLE->cxform);
    graf_tile(&g.graf,x0+(player->cx  )*tilesize,y0+(player->cy  )*tilesize,0x7a,BATTLE->cxform);
    graf_tile(&g.graf,x0+(player->cx+1)*tilesize,y0+(player->cy  )*tilesize,0x7a,BATTLE->cxform);
    graf_tile(&g.graf,x0+(player->cx  )*tilesize,y0+(player->cy+1)*tilesize,0x7a,BATTLE->cxform);
  }
}

/* Render.
 */
 
static void _exterminating_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_early);
  player_render(battle,BATTLE->playerv+0,(FBW*1)/3);
  player_render(battle,BATTLE->playerv+1,(FBW*2)/3);
}

/* Type definition.
 */
 
const struct battle_type battle_type_exterminating={
  .name="exterminating",
  .objlen=sizeof(struct battle_exterminating),
  .strix_name=17,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_exterminating_del,
  .init=_exterminating_init,
  .update=_exterminating_update,
  .render=_exterminating_render,
};

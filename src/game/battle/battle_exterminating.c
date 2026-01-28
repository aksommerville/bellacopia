#include "game/bellacopia.h"

#define FLDW 5
#define FLDH 5
#define END_COOLDOWN_TIME 1.0
#define THINKYTIME_MIN 0.128
#define THINKYTIME_MAX 0.500

struct battle_exterminating {
  uint8_t handicap;
  uint8_t players;
  void (*cb_end)(int outcome,void *userdata);
  void *userdata;
  int outcome;
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
    int lastx,lasty;
  } playerv[2];
};

#define CTX ((struct battle_exterminating*)ctx)

/* Delete.
 */
 
static void _exterminating_del(void *ctx) {
  free(ctx);
}

/* New.
 */
 
static void *_exterminating_init(
  uint8_t handicap,
  uint8_t players,
  void (*cb_end)(int outcome,void *userdata),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_exterminating));
  if (!ctx) return 0;
  CTX->handicap=handicap;
  CTX->players=players;
  CTX->cb_end=cb_end;
  CTX->userdata=userdata;
  CTX->outcome=-2;
  
  // Fill xformv randomly. This is entirely decorative.
  uint8_t *p=CTX->xformv;
  int i=FLDW*FLDH;
  for (;i-->0;p++) *p=rand()&7;
  
  // Bug half of player zero's field, then copy it to player one. They start with the same field.
  for (i=(FLDW*FLDH)>>1;i-->0;) {
    for (;;) {
      int ix=rand()%(FLDW*FLDH);
      if (CTX->playerv[0].fld[ix]) continue;
      CTX->playerv[0].fld[ix]=1;
      break;
    }
  }
  memcpy(CTX->playerv[1].fld,CTX->playerv[0].fld,FLDW*FLDH);
  
  // Both players start in the middle.
  CTX->playerv[0].cx=CTX->playerv[1].cx=FLDW>>1;
  CTX->playerv[0].cy=CTX->playerv[1].cy=FLDH>>1;
  
  // And set (player.human) per (playerc).
  switch (CTX->players) {
    case NS_players_cpu_cpu: {
        CTX->playerv[0].human=0;
        CTX->playerv[1].human=0;
      } break;
    case NS_players_cpu_man: {
        CTX->playerv[0].human=0;
        CTX->playerv[1].human=2;
      } break;
    case NS_players_man_cpu: {
        CTX->playerv[0].human=1;
        CTX->playerv[1].human=0;
      } break;
    case NS_players_man_man: {
        CTX->playerv[0].human=1;
        CTX->playerv[1].human=2;
      } break;
  }
  CTX->playerv[0].who=0;
  CTX->playerv[1].who=1;
  CTX->playerv[0].lastx=CTX->playerv[1].lastx=CTX->playerv[0].lasty=CTX->playerv[1].lasty=-2;
  // (thinkytime) only matters for CPU players, but no harm setting it in both.
  // Note that (handicap) is not used in man-vs-man mode.
  CTX->playerv[0].thinkytime=THINKYTIME_MAX+((THINKYTIME_MIN-THINKYTIME_MAX)*(0xff-CTX->handicap))/255.0;
  CTX->playerv[1].thinkytime=THINKYTIME_MAX+((THINKYTIME_MIN-THINKYTIME_MAX)*handicap)/255.0;
  
  return ctx;
}

/* Swap tiles under cursor.
 */
 
static void player_swap_1(void *ctx,struct player *player,int x,int y) {
  if ((x<0)||(x>=FLDW)||(y<0)||(y>=FLDH)) return;
  player->fld[y*FLDW+x]^=1;
}
 
static void player_swap(void *ctx,struct player *player) {
  bm_sound_pan(RID_sound_uiactivate,player->who?0.250:-0.250);
  player_swap_1(ctx,player,player->cx,player->cy-1);
  player_swap_1(ctx,player,player->cx-1,player->cy);
  player_swap_1(ctx,player,player->cx,player->cy);
  player_swap_1(ctx,player,player->cx+1,player->cy);
  player_swap_1(ctx,player,player->cx,player->cy+1);
  // Am I clear now?
  player->clear=0;
  const uint8_t *v=player->fld;
  int i=FLDW*FLDH;
  for (;i-->0;v++) if (*v) return;
  player->clear=1;
}

/* Move cursor.
 */
 
static void player_move(void *ctx,struct player *player,int dx,int dy) {
  player->cx+=dx;
  player->cy+=dy;
  if (player->cx<-1) player->cx=-1; else if (player->cx>FLDW) player->cx=FLDW;
  if (player->cy<-1) player->cy=-1; else if (player->cy>FLDH) player->cy=FLDH;
  bm_sound_pan(RID_sound_uimotion,player->who?0.250:-0.250);
}

/* Update human player.
 */
 
static void player_update_human(void *ctx,struct player *player,double elapsed) {
  if ((g.input[player->human]&EGG_BTN_LEFT)&&!(g.pvinput[player->human]&EGG_BTN_LEFT)) player_move(ctx,player,-1,0);
  if ((g.input[player->human]&EGG_BTN_RIGHT)&&!(g.pvinput[player->human]&EGG_BTN_RIGHT)) player_move(ctx,player,1,0);
  if ((g.input[player->human]&EGG_BTN_UP)&&!(g.pvinput[player->human]&EGG_BTN_UP)) player_move(ctx,player,0,-1);
  if ((g.input[player->human]&EGG_BTN_DOWN)&&!(g.pvinput[player->human]&EGG_BTN_DOWN)) player_move(ctx,player,0,1);
  if ((g.input[player->human]&EGG_BTN_SOUTH)&&!(g.pvinput[player->human]&EGG_BTN_SOUTH)) player_swap(ctx,player);
}

/* Update CPU player.
 */
 
static int bugs_exist_in_middle(const uint8_t *v) {
  const uint8_t *row=v+FLDW+1;
  int yi=FLDH-2;
  for (;yi-->0;row+=FLDW) {
    const uint8_t *p=row;
    int xi=FLDW-2;
    for (;xi-->0;p++) if (*p) return 1;
  }
  return 0;
}

static void assess_candidate_1(int *bugc,int *expc,const uint8_t *fld,int x,int y) {
  if ((x<0)||(y<0)||(x>=FLDW)||(y>=FLDH)) return;
  (*expc)++;
  if (fld[y*FLDW+x]) (*bugc)++;
}

static void assess_candidate(int *bugc,int *expc,const uint8_t *fld,int x,int y) {
  *bugc=*expc=0;
  assess_candidate_1(bugc,expc,fld,x  ,y-1);
  assess_candidate_1(bugc,expc,fld,x-1,y);
  assess_candidate_1(bugc,expc,fld,x  ,y);
  assess_candidate_1(bugc,expc,fld,x+1,y);
  assess_candidate_1(bugc,expc,fld,x  ,y+1);
}
 
static void player_update_cpu(void *ctx,struct player *player,double elapsed) {
  if ((player->thinkyclock-=elapsed)>0.0) return;
  player->thinkyclock+=player->thinkytime;
  
  /* We're not going to be super smart, but we will execute a strategy that wins eventually.
   *  - If any bugs remain off the edge, do not play far outside. ie toggle at least 3 cells.
   *  - Scan all possible moves exhaustively (there's only 25 or 49 of them) and take whichever yields the highest bug/empty ratio.
   *  - That's likely to be more than one, so select the one nearest my cursor.
   * We run that whole algorithm every cycle, even when the ultimate decision is just "move toward it".
   * *** It's not perfect! ***
   * He'll avoid the most trivial loops, won't touch the same cell twice in a row.
   * But he does fall into multistage loops sometimes.
   */
  struct candidate { int x,y; } candidatev[49];
  int candidatec=0;
  int can_bugc=0;
  int can_expc=0;
  double can_ratio=0.0;
  int xa=0,xz=FLDW-1,ya=0,yz=FLDH-1;
  int edge_only=0;
  if (!bugs_exist_in_middle(player->fld)) {
    xa--; ya--; xz++; yz++;
    edge_only=1;
  }
  int y=ya; for (;y<=yz;y++) {
    int x=xa; for (;x<=xz;x++) {
      if (edge_only) {
        if ((y==-1)||(y==FLDH)||(x==-1)||(x==FLDW)) ;
        else continue;
      }
      if ((x==player->lastx)&&(y==player->lasty)) {
        // Don't toggle the same cell twice in a row!
        continue;
      }
      int bugc=0,expc=0;
      assess_candidate(&bugc,&expc,player->fld,x,y);
      if (expc<1) continue; // Impossible but play it safe.
      if ((bugc==can_bugc)&&(expc==can_expc)) { // If it's the same as our current candidates, append to the list.
        candidatev[candidatec++]=(struct candidate){x,y};
        continue;
      }
      double ratio=(double)bugc/(double)expc;
      if (ratio<can_ratio) continue;
      if ((ratio>can_ratio)||(bugc>can_bugc)) { // If it's better than our current candidates, clear the list then append.
        can_bugc=bugc;
        can_expc=expc;
        can_ratio=ratio;
        candidatec=0;
        candidatev[candidatec++]=(struct candidate){x,y};
      }
    }
  }
  if (candidatec<1) return;
  // Pick the nearest candidate by Manhattan distance.
  int bestx=candidatev[0].x;
  int besty=candidatev[0].y;
  int bestscore=999;
  struct candidate *candidate=candidatev;
  int i=candidatec;
  for (;i-->0;candidate++) {
    int dx=candidate->x-player->cx; if (dx<0) dx=-dx;
    int dy=candidate->y-player->cy; if (dy<0) dy=-dy;
    int score=dx+dy;
    if (score<bestscore) {
      bestx=candidate->x;
      besty=candidate->y;
      bestscore=score;
    }
  }
  
  // If we're already at the best cell, toggle it.
  if ((player->cx==bestx)&&(player->cy==besty)) {
    player_swap(ctx,player);
    player->lastx=player->cx;
    player->lasty=player->cy;
    return;
  }
  
  // If we're diagonal, prefer the nearer axis. (that leads us to move manhattanly rather than approximating the diagonal, which I think looks more human).
  int dx=bestx-player->cx;
  int dy=besty-player->cy;
  if (dx&&dy) {
    int adx=(dx<0)?-dx:dx;
    int ady=(dy<0)?-dy:dy;
    if (adx<ady) dy=0; else dx=0;
  }
  if (dx<0) dx=-1; else if (dx>0) dx=1;
  if (dy<0) dy=-1; else if (dy>0) dy=1;
  player_move(ctx,player,dx,dy);
}

/* Update.
 */
 
static void _exterminating_update(void *ctx,double elapsed) {
  if (CTX->outcome>-2) {
    if (CTX->cb_end) {
      if (CTX->end_cooldown>0.0) {
        if ((CTX->end_cooldown-=elapsed)<=0.0) {
          CTX->cb_end(CTX->outcome,CTX->userdata);
          CTX->cb_end=0;
        }
      }
    }
  }
  if ((CTX->xformclock-=elapsed)<=0.0) {
    CTX->xformclock+=0.100;
    int p=rand()%(FLDW*FLDH);
    CTX->xformv[p]=rand()&7;
  }
  
  if (CTX->outcome<=-2) {
    if ((CTX->canimclock-=elapsed)<=0.0) {
      CTX->canimclock+=0.100;
      if (++(CTX->canimframe)>=4) CTX->canimframe=0;
      switch (CTX->canimframe) {
        case 0: CTX->cxform=0; break;
        case 1: CTX->cxform=EGG_XFORM_SWAP|EGG_XFORM_XREV; break;
        case 2: CTX->cxform=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
        case 3: CTX->cxform=EGG_XFORM_YREV|EGG_XFORM_SWAP; break;
      }
    }
    struct player *player=CTX->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (player->human) player_update_human(ctx,player,elapsed);
      else player_update_cpu(ctx,player,elapsed);
    }
    if (CTX->playerv[0].clear) {
      if (CTX->playerv[1].clear) { // It's just barely possible that they clear at the same moment.
        CTX->outcome=0;
      } else {
        CTX->outcome=1;
      }
      CTX->end_cooldown=END_COOLDOWN_TIME;
    } else if (CTX->playerv[1].clear) {
      CTX->outcome=-1;
      CTX->end_cooldown=END_COOLDOWN_TIME;
    }
  }
}

/* Render one player's field.
 */
 
static void player_render(void *ctx,struct player *player,int midx) {
  const int tilesize=16;
  int x0=midx-((FLDW*tilesize)>>1)+(tilesize>>1);
  int y0=(FBH>>1)-((FLDH*tilesize)>>1)+(tilesize>>1);
  
  int y=y0;
  const uint8_t *p=player->fld;
  const uint8_t *xform=CTX->xformv;
  int yi=FLDH;
  for (;yi-->0;y+=tilesize) {
    int x=x0;
    int xi=FLDW;
    for (;xi-->0;x+=tilesize,p++,xform++) {
      graf_tile(&g.graf,x,y,((xi&1)==(yi&1))?0x7b:0x8b,0);
      if (*p) graf_tile(&g.graf,x,y,0x8a,*xform);
    }
  }
  
  if (CTX->outcome<=-2) {
    graf_tile(&g.graf,x0+(player->cx  )*tilesize,y0+(player->cy-1)*tilesize,0x7a,CTX->cxform);
    graf_tile(&g.graf,x0+(player->cx-1)*tilesize,y0+(player->cy  )*tilesize,0x7a,CTX->cxform);
    graf_tile(&g.graf,x0+(player->cx  )*tilesize,y0+(player->cy  )*tilesize,0x7a,CTX->cxform);
    graf_tile(&g.graf,x0+(player->cx+1)*tilesize,y0+(player->cy  )*tilesize,0x7a,CTX->cxform);
    graf_tile(&g.graf,x0+(player->cx  )*tilesize,y0+(player->cy+1)*tilesize,0x7a,CTX->cxform);
  }
}

/* Render.
 */
 
static void _exterminating_render(void *ctx) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_fishing);
  player_render(ctx,CTX->playerv+0,(FBW*1)/3);
  player_render(ctx,CTX->playerv+1,(FBW*2)/3);
}

/* Type definition.
 */
 
const struct battle_type battle_type_exterminating={
  .name="exterminating",
  .strix_name=17,
  .no_article=0,
  .no_contest=0,
  .supported_players=(1<<NS_players_cpu_cpu)|(1<<NS_players_cpu_man)|(1<<NS_players_man_cpu)|(1<<NS_players_man_man),
  .del=_exterminating_del,
  .init=_exterminating_init,
  .update=_exterminating_update,
  .render=_exterminating_render,
};

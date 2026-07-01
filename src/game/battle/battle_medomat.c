/* battle_medomat.c
 * Tiny Dr Mario clone.
 * No pvp or cvc, just one player.
 * A two-field two-player mode wouldn't be super hard to pull off, if we want it.
 */

#include "game/bellacopia.h"

#define COLW 10
#define ROWH 10
#define COLC 10
#define ROWC 12

#define CELL_VACANT           0 /* Vacant cells are all zero, which would not be legal. */
#define CELL_JOIN_MASK     0x0f /* Only applicable to CELL_CONTENT_PILL. */
#define CELL_JOIN_NONE     0x00
#define CELL_JOIN_N        0x01
#define CELL_JOIN_S        0x02
#define CELL_JOIN_W        0x03
#define CELL_JOIN_E        0x04
#define CELL_COLOR_MASK    0x30
#define CELL_COLOR_RED     0x10
#define CELL_COLOR_GREEN   0x20
#define CELL_COLOR_BLUE    0x30
#define CELL_CONTENT_MASK  0xc0
#define CELL_CONTENT_HEART 0x40
#define CELL_CONTENT_PILL  0x80

#define ELIMINATION_LIMIT 3
#define PRIZE_LIMIT 6

struct battle_medomat {
  struct battle hdr;
  
  uint8_t fld[COLC*ROWC]; // LRTB CELL_*
  
  int fallx,fally; // Focus point.
  int twindx,twindy; // Cardinal vector to the other half of the falling pill.
  uint8_t fallcolor,twincolor; // CELL_COLOR_*
  
  int reservec; // How many pills left to play.
  double fallclock;
  double falltime; // constish
  int turbopoison;
  double pauseclock; // >0 if we're paused due to elimination.
  struct elimination {
    int x,y,w,h;
  } eliminationv[ELIMINATION_LIMIT];
  int eliminationc;
  uint8_t prizev[PRIZE_LIMIT]; // cell
  int prizec;
  int indx;
  double dxclock;
  uint8_t colorv[3]; // Starts with all three, but we remove when the last heart of a given color is eliminated.
  int colorc;
};

#define BATTLE ((struct battle_medomat*)battle)

/* Delete.
 */
 
static void _medomat_del(struct battle *battle) {
}

/* Place a random heart, during init.
 */
 
static void medomat_random_heart(struct battle *battle,uint8_t color) {
  const int minrow=4; // Place nothing above this.
  int panic=100;
  while (panic-->0) {
    int x=rand()%COLC;
    int y=minrow+rand()%(ROWC-minrow);
    if (BATTLE->fld[y*COLC+x]) continue;
    BATTLE->fld[y*COLC+x]=color|CELL_CONTENT_HEART;
    return;
  }
}

/* New random pill.
 */
 
static uint8_t medomat_random_color(struct battle *battle) {
  if ((BATTLE->colorc<1)||(BATTLE->colorc>3)) {
    switch (rand()%3) {
      case 0: return CELL_COLOR_RED;
      case 1: return CELL_COLOR_GREEN;
      case 2: return CELL_COLOR_BLUE;
    }
  } else {
    return BATTLE->colorv[rand()%BATTLE->colorc];
  }
  return 0;
}
 
static void medomat_random_pill(struct battle *battle) {
  BATTLE->fallx=COLC>>1;
  BATTLE->fally=0;
  BATTLE->twindx=-1;
  BATTLE->twindy=0;
  BATTLE->fallcolor=medomat_random_color(battle);
  BATTLE->twincolor=medomat_random_color(battle);
  // If the entry-zone cells are occupied, you lose:
  if (
    BATTLE->fld[BATTLE->fallx]||
    BATTLE->fld[BATTLE->fallx+BATTLE->twindx]
  ) {
    battle->outcome=-1;
  }
}

/* New.
 */
 
static int _medomat_init(struct battle *battle) {

  /* Generate a random field of hearts.
   */
  medomat_random_heart(battle,CELL_COLOR_RED);
  medomat_random_heart(battle,CELL_COLOR_RED);
  medomat_random_heart(battle,CELL_COLOR_GREEN);
  medomat_random_heart(battle,CELL_COLOR_GREEN);
  medomat_random_heart(battle,CELL_COLOR_BLUE);
  medomat_random_heart(battle,CELL_COLOR_BLUE);
  
  /* Start with a random pill.
   */
  medomat_random_pill(battle);
  
  BATTLE->reservec=8;
  BATTLE->falltime=0.500;
  BATTLE->fallclock=BATTLE->falltime*2.0;
  BATTLE->colorv[0]=CELL_COLOR_RED;
  BATTLE->colorv[1]=CELL_COLOR_GREEN;
  BATTLE->colorv[2]=CELL_COLOR_BLUE;
  BATTLE->colorc=3;

  return 0;
}

/* Nonzero if the coords are OOB or occupied.
 * Does not consider the falling piece.
 */
 
static int medomat_is_solid(const struct battle *battle,int x,int y) {
  if ((x<0)||(y<0)||(x>=COLC)||(y>=ROWC)) return 1;
  return BATTLE->fld[y*COLC+x];
}

/* Validate coords and write a cell.
 */
 
static void medomat_set_cell(struct battle *battle,int x,int y,uint8_t cell) {
  if ((x<0)||(y<0)||(x>=COLC)||(y>=ROWC)) return;
  BATTLE->fld[y*COLC+x]=cell;
}

/* How many occupied cells of the same color as the first, reading rightward or downward.
 */
 
static int medomat_scan_horz(const uint8_t *p,int a) {
  if (a<1) return 0;
  uint8_t color=((*p)&CELL_COLOR_MASK);
  int c=0;
  while ((c<a)&&(((*p)&CELL_COLOR_MASK)==color)) { c++; p++; }
  return c;
}
 
static int medomat_scan_vert(const uint8_t *p,int a) {
  if (a<1) return 0;
  uint8_t color=((*p)&CELL_COLOR_MASK);
  int c=0;
  while ((c<a)&&(((*p)&CELL_COLOR_MASK)==color)) { c++; p+=COLC; }
  return c;
}

/* Scan the entire field for eliminations.
 * If we find any, set the appropriate state and return >0.
 * Return zero if none.
 * Of course, we only need to check from the falling pill's position, which would still be readable here,
 * but I'm reading the whole field from scratch just to be on the safe side.
 */
 
static int medomat_check_elimination(struct battle *battle) {
  BATTLE->eliminationc=0;
  const uint8_t *p=BATTLE->fld;
  int y=0;
  for (;y<ROWC;y++) {
    int x=0;
    for (;x<COLC;x++,p++) {
      if (!*p) continue;
      
      int visited=0;
      const struct elimination *el=BATTLE->eliminationv;
      int i=BATTLE->eliminationc;
      for (;i-->0;el++) {
        if (el->x>x) continue;
        if (el->y>y) continue;
        if (el->x+el->w<=x) continue;
        if (el->y+el->h<=y) continue;
        visited=1;
        break;
      }
      if (visited) continue;
      
      int samec=medomat_scan_horz(p,COLC-x);
      if ((samec>=4)&&(BATTLE->eliminationc<ELIMINATION_LIMIT)) {
        BATTLE->eliminationv[BATTLE->eliminationc++]=(struct elimination){x,y,samec,1};
      }
      samec=medomat_scan_vert(p,ROWC-y);
      if ((samec>=4)&&(BATTLE->eliminationc<ELIMINATION_LIMIT)) {
        BATTLE->eliminationv[BATTLE->eliminationc++]=(struct elimination){x,y,1,samec};
      }
    }
  }
  if (!BATTLE->eliminationc) return 0;
  BATTLE->pauseclock=1.000;
  return BATTLE->eliminationc;
}

/* Scan the field and pretend anything under an elimination is already gone.
 * If any heart color is no longer present, remove it from (colorv).
 * Call this after creating new eliminations but before spawning the new fall piece.
 */
 
static void medomat_update_colorv(struct battle *battle) {
  if (BATTLE->colorc<1) return;
  int red=0,green=0,blue=0;
  const uint8_t *p=BATTLE->fld;
  int y=0;
  for (;y<ROWC;y++) {
    int x=0;
    for (;x<COLC;x++,p++) {
      if (((*p)&CELL_CONTENT_MASK)!=CELL_CONTENT_HEART) continue;
      int eliminated=0;
      const struct elimination *el=BATTLE->eliminationv;
      int ei=BATTLE->eliminationc;
      for (;ei-->0;el++) {
        if ((el->x<=x)&&(el->y<=y)&&(el->x+el->w>x)&&(el->y+el->h>y)) {
          eliminated=1;
          break;
        }
      }
      if (eliminated) continue; // Pretend we didn't see it.
      switch ((*p)&CELL_COLOR_MASK) {
        case CELL_COLOR_RED: red=1; break;
        case CELL_COLOR_GREEN: green=1; break;
        case CELL_COLOR_BLUE: blue=1; break;
      }
      if (red&&green&&blue) return; // Won't be eliminating anything; stop looking.
    }
  }
  int i=BATTLE->colorc;
  while (i-->0) {
    int drop=0;
    switch (BATTLE->colorv[i]) {
      case CELL_COLOR_RED: if (!red) drop=1; break;
      case CELL_COLOR_GREEN: if (!green) drop=1; break;
      case CELL_COLOR_BLUE: if (!blue) drop=1; break;
    }
    if (!drop) continue;
    BATTLE->colorc--;
    memmove(BATTLE->colorv+i,BATTLE->colorv+i+1,BATTLE->colorc-i);
  }
}

/* Apply one scoop of gravity.
 */
 
static void medomat_fall(struct battle *battle) {

  /* Should we stick here?
   */
  int cement=0;
  if (BATTLE->twindx) { // Two cells below me.
    if (medomat_is_solid(battle,BATTLE->fallx,BATTLE->fally+1)||medomat_is_solid(battle,BATTLE->fallx+BATTLE->twindx,BATTLE->fally+1)) cement=1;
  } else { // One cell below me.
    int y=BATTLE->fally+1;
    if (BATTLE->twindy>0) y++;
    if (medomat_is_solid(battle,BATTLE->fallx,y)) cement=1;
  }
  if (cement) {
    uint8_t cella=CELL_CONTENT_PILL|BATTLE->fallcolor;
    uint8_t cellb=CELL_CONTENT_PILL|BATTLE->twincolor;
         if (BATTLE->twindx<0) { cella|=CELL_JOIN_W; cellb|=CELL_JOIN_E; }
    else if (BATTLE->twindx>0) { cella|=CELL_JOIN_E; cellb|=CELL_JOIN_W; }
    else if (BATTLE->twindy<0) { cella|=CELL_JOIN_N; cellb|=CELL_JOIN_S; }
    else if (BATTLE->twindy>0) { cella|=CELL_JOIN_S; cellb|=CELL_JOIN_N; }
    medomat_set_cell(battle,BATTLE->fallx,BATTLE->fally,cella);
    medomat_set_cell(battle,BATTLE->fallx+BATTLE->twindx,BATTLE->fally+BATTLE->twindy,cellb);
    
    if (medomat_check_elimination(battle)) {
      medomat_update_colorv(battle);
      bm_sound(RID_sound_treasure);
    } else {
      bm_sound(RID_sound_chopmiss);
    }
    
    if (BATTLE->reservec>0) {
      if (BATTLE->pauseclock>0.0) {
        BATTLE->fallcolor=0;
      } else {
        BATTLE->reservec--;
        medomat_random_pill(battle);
      }
      BATTLE->fallclock+=BATTLE->falltime; // Allow an extra beat when the new pill appears.
    } else {
      // Out of pills. Wait for any pause actions to complete, then you win.
      BATTLE->fallcolor=0;
    }
    BATTLE->turbopoison=1;
    return;
  }
  
  /* No? Cool, fall one row.
   */
  bm_sound(RID_sound_tick);
  BATTLE->fally+=1;
}

/* Move piece horizontally.
 */
 
static void medomat_move(struct battle *battle,int dx) {
  if (medomat_is_solid(battle,BATTLE->fallx+dx,BATTLE->fally)||medomat_is_solid(battle,BATTLE->fallx+BATTLE->twindx+dx,BATTLE->fally+BATTLE->twindy)) {
    // If dropping one row would make it passable (including the near row), do that and reset fallclock.
    int ax=BATTLE->fallx;
    int ay=BATTLE->fally;
    int bx=ax+BATTLE->twindx;
    int by=ay+BATTLE->twindy;
    if (
      !medomat_is_solid(battle,ax,ay+1)&&
      !medomat_is_solid(battle,bx,by+1)&&
      !medomat_is_solid(battle,ax+dx,ay+1)&&
      !medomat_is_solid(battle,bx+dx,by+1)
    ) {
      bm_sound(RID_sound_uimotion);
      BATTLE->fallx+=dx;
      BATTLE->fally+=1;
      BATTLE->fallclock=BATTLE->falltime;
      return;
    }
    // OK, nope.
    bm_sound(RID_sound_reject);
    return;
  }
  bm_sound(RID_sound_uimotion);
  BATTLE->fallx+=dx;
}

/* Rotate piece.
 * d>0 clockwise, d<0 deasil.
 */
 
static void medomat_rotate(struct battle *battle,int d) {
  int ndx=0,ndy=0;
  if (d>0) {
         if (BATTLE->twindx<0) ndy=-1;
    else if (BATTLE->twindy<0) ndx=1;
    else if (BATTLE->twindx>0) ndy=1;
    else if (BATTLE->twindy>0) ndx=-1;
    else return;
  } else {
         if (BATTLE->twindx<0) ndy=1;
    else if (BATTLE->twindy>0) ndx=1;
    else if (BATTLE->twindx>0) ndy=-1;
    else if (BATTLE->twindy<0) ndx=-1;
    else return;
  }
  if (medomat_is_solid(battle,BATTLE->fallx+ndx,BATTLE->fally+ndy)) {
    /* Wall kick: If this move can be made legal by moving one meter cardinally, do it.
     */
    const struct adjust { int dx,dy; } adjustv[]={{-1,0},{1,0},{0,1},{0,-1}};
    const struct adjust *adjust=adjustv;
    int i=4;
    for (;i-->0;adjust++) {
      int ax=BATTLE->fallx+adjust->dx;
      int ay=BATTLE->fally+adjust->dy;
      int bx=ax+ndx;
      int by=ay+ndy;
      if (!medomat_is_solid(battle,ax,ay)&&!medomat_is_solid(battle,bx,by)) {
        BATTLE->fallx+=adjust->dx;
        BATTLE->fally+=adjust->dy;
        BATTLE->twindx=ndx;
        BATTLE->twindy=ndy;
        bm_sound(RID_sound_uimotion);
        return;
      }
    }
    bm_sound(RID_sound_reject);
    return;
  }
  BATTLE->twindx=ndx;
  BATTLE->twindy=ndy;
  bm_sound(RID_sound_uimotion);
}

/* Clear and score eliminations.
 */
 
static void medomat_clear_eliminations(struct battle *battle) {
  while (BATTLE->eliminationc>0) {
    BATTLE->eliminationc--;
    struct elimination *el=BATTLE->eliminationv+BATTLE->eliminationc;
    uint8_t *rowp=BATTLE->fld+el->y*COLC+el->x;
    int yi=el->h;
    for (;yi-->0;rowp+=COLC) {
      const uint8_t *p=rowp;
      int xi=el->w;
      for (;xi-->0;p++) {
        if (((*p)&CELL_CONTENT_MASK)==CELL_CONTENT_HEART) {
          if (BATTLE->prizec<PRIZE_LIMIT) BATTLE->prizev[BATTLE->prizec++]=*p;
        }
      }
      memset(rowp,0,el->w);
    }
  }
  /* I guess we should have checked for this at the elimination sites, but meh:
   * Scan the field for any joined pills whose neighbor isn't there anymore.
   * If we find any, make them unjoined.
   */
  uint8_t *p=BATTLE->fld;
  int i=COLC*ROWC;
  int y=0,x=0;
  for (;i-->0;p++) {
    if (((*p)&CELL_CONTENT_MASK)==CELL_CONTENT_PILL) {
      int nx=x,ny=y;
      uint8_t expect=0;
      switch ((*p)&CELL_JOIN_MASK) {
        case CELL_JOIN_N: ny--; expect=CELL_JOIN_S; break;
        case CELL_JOIN_S: ny++; expect=CELL_JOIN_N; break;
        case CELL_JOIN_W: nx--; expect=CELL_JOIN_E; break;
        case CELL_JOIN_E: nx++; expect=CELL_JOIN_W; break;
      }
      if (expect&&(nx>=0)&&(ny>=0)&&(nx<COLC)&&(ny<ROWC)) {
        uint8_t neighbor=BATTLE->fld[ny*COLC+nx];
        if (((neighbor&CELL_CONTENT_MASK)!=CELL_CONTENT_PILL)||((neighbor&CELL_JOIN_MASK)!=expect)) {
          (*p)=((*p)&~CELL_JOIN_MASK)|CELL_JOIN_NONE;
        }
      }
    }
    if (++x>=COLC) {
      x=0;
      y++;
    }
  }
}

/* Any cell that isn't supported anymore, drop it by one row.
 * Nonzero if anything moved.
 */
 
static int medomat_drop_disconnected(struct battle *battle) {
  int c=0;
  // Read bottom-up so we can effect the drops in place.
  uint8_t *p=BATTLE->fld+COLC*ROWC;
  int y=ROWC;
  while (y-->1) { // Don't scan the top row, nothing can fall into it.
    int x=COLC;
    while (x-->0) {
      p--;
      if (*p) continue; // We're looking for empties.
      uint8_t neighbor=p[-COLC];
      if ((neighbor&CELL_CONTENT_MASK)!=CELL_CONTENT_PILL) continue; // Only pills can fall.
      // If the neighbor is joined horizontally, confirm that its partner is also free, and move both of them.
      switch (neighbor&CELL_JOIN_MASK) {
        case CELL_JOIN_W: {
            if ((x>0)&&!p[-1]&&((p[-1-COLC]&(CELL_CONTENT_MASK|CELL_JOIN_MASK))==(CELL_CONTENT_PILL|CELL_JOIN_E))) {
              *p=neighbor;
              p[-COLC]=0;
              p[-1]=p[-1-COLC];
              p[-1-COLC]=0;
              c++;
            }
          } break;
        case CELL_JOIN_E: {
            if ((x<COLC-1)&&!p[1]&&((p[1-COLC]&(CELL_CONTENT_MASK|CELL_JOIN_MASK))==(CELL_CONTENT_PILL|CELL_JOIN_W))) {
              *p=neighbor;
              p[-COLC]=0;
              p[1]=p[1-COLC];
              p[1-COLC]=0;
              c++;
            }
          } break;
        default: { // Solo or joined vertically, just drop it.
            *p=neighbor;
            p[-COLC]=0;
            c++;
          }
      }
    }
  }
  return c;
}

/* Update.
 */
 
static void _medomat_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* Update automatic actions like elimination and static drops.
   */
  if (BATTLE->pauseclock>0.0) {
    BATTLE->pauseclock-=elapsed;
    if (BATTLE->pauseclock<=0.0) {
      medomat_clear_eliminations(battle);
      if (medomat_drop_disconnected(battle)) {
        BATTLE->pauseclock=0.200;
      } else if (medomat_check_elimination(battle)) {
        bm_sound(RID_sound_treasure);
        medomat_update_colorv(battle);
        if ((BATTLE->pauseclock<=0.0)&&(BATTLE->reservec>0)) {
          BATTLE->reservec--;
          medomat_random_pill(battle);
        }
      } else if (BATTLE->reservec>0) {
        BATTLE->reservec--;
        medomat_random_pill(battle);
      }
    }
  }
  
  /* If we're out of pieces to drop, and not paused, game over.
   */
  if (!BATTLE->fallcolor&&(BATTLE->pauseclock<=0.0)) {
    battle->outcome=1;
    return;
  }
  
  /* Horizontal motion is slightly complicated due to auto-repeat.
   * This does continue during pause.
   */
  int indx=0;
  switch (g.input[0]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: indx=-1; break;
    case EGG_BTN_RIGHT: indx=1; break;
  }
  if (indx==BATTLE->indx) { // No dpad change...
    if (indx) { // ...nonzero...
      if ((BATTLE->dxclock-=elapsed)<=0.0) { // ...autorepeat
        BATTLE->dxclock+=0.100;
        medomat_move(battle,indx);
      }
    }
  } else { // Dpad changed.
    BATTLE->indx=indx;
    if (indx) {
      BATTLE->dxclock=0.200;
      medomat_move(battle,indx);
    }
  }
  
  /* Rotation is permitted during pause.
   */
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) medomat_rotate(battle,1);
  else if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) medomat_rotate(battle,-1);
  
  /* Beyond this point is only gravity.
   * Not applicable during pause.
   */
  if (BATTLE->pauseclock>0.0) return;
  
  /* Hold down to reduce drop time.
   */
  const double turbodrop=0.050;
  if (BATTLE->turbopoison) {
    if (!(g.input[0]&EGG_BTN_DOWN)) BATTLE->turbopoison=0;
  } else if ((g.input[0]&EGG_BTN_DOWN)&&(BATTLE->fallclock>turbodrop)) {
    BATTLE->fallclock=turbodrop;
  }
  
  /* Drop when the clock cycles.
   */
  if ((BATTLE->fallclock-=elapsed)<=0.0) {
    BATTLE->fallclock+=BATTLE->falltime;
    medomat_fall(battle);
  }
}

/* Render.
 */
 
static void medomat_tile(int x,int y,uint8_t cell) {
  uint8_t tileid=0x49,xform=0; // 0x49..0x4b = heart,joined,solo
  uint32_t color=0x808080ff;
  switch (cell&CELL_COLOR_MASK) {
    case CELL_COLOR_RED: color=0xff0000ff; break;
    case CELL_COLOR_GREEN: color=0x00c000ff; break;
    case CELL_COLOR_BLUE: color=0x4050ffff; break;
    default: return;
  }
  switch (cell&CELL_CONTENT_MASK) {
    case CELL_CONTENT_HEART: break;
    case CELL_CONTENT_PILL: {
        switch (cell&CELL_JOIN_MASK) {
          case CELL_JOIN_NONE: tileid+=2; break;
          case CELL_JOIN_N: tileid+=1; xform=EGG_XFORM_YREV; break;
          case CELL_JOIN_S: tileid+=1; break;
          case CELL_JOIN_W: tileid+=1; xform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
          case CELL_JOIN_E: tileid+=1; xform=EGG_XFORM_SWAP; break;
          default: return;
        }
      } break;
    default: return;
  }
  graf_fancy(&g.graf,x,y,tileid,xform,0,NS_sys_tilesize,0,color);
}
 
static void _medomat_render(struct battle *battle) {

  // Background.
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x3664beff); // Agree with the tile color in image:fractia_int.
  const int fldw=COLC*COLW;
  const int fldh=ROWC*ROWH;
  const int fldx=(FBW>>1)-(fldw>>1);
  const int fldy=(FBH>>1)-(fldh>>1)+6;
  graf_fill_rect(&g.graf,fldx,fldy,fldw+1,fldh+1,0x000000ff);
  
  // Field, fixed bits only.
  graf_set_image(&g.graf,RID_image_battle_fractia);
  int dsty0=fldy+(ROWH>>1);
  int dstx0=fldx+(COLW>>1);
  int dsty=dsty0;
  int yi=ROWC;
  const uint8_t *src=BATTLE->fld;
  for (;yi-->0;dsty+=ROWH) {
    int dstx=dstx0;
    int xi=COLC;
    for (;xi-->0;dstx+=COLW,src++) {
      if (!*src) continue;
      medomat_tile(dstx,dsty,*src);
    }
  }
  
  // Flash eliminations.
  if ((BATTLE->pauseclock>0.0)&&(BATTLE->eliminationc>0)) {
    graf_set_input(&g.graf,0);
    const struct elimination *el=BATTLE->eliminationv;
    int i=BATTLE->eliminationc;
    for (;i-->0;el++) {
      int frame=(int)(BATTLE->pauseclock*5.0);
      graf_fill_rect(&g.graf,fldx+el->x*COLW,fldy+el->y*ROWH,el->w*COLW,el->h*ROWH,(frame&1)?0x00000040:0x000000c0);
    }
    graf_set_image(&g.graf,RID_image_battle_fractia);
  }
  
  // The falling pill.
  if (BATTLE->fallcolor) {
    uint8_t ajoin,bjoin;
    if (BATTLE->twindx==1) { ajoin=CELL_JOIN_E; bjoin=CELL_JOIN_W; }
    else if (BATTLE->twindx==-1) { ajoin=CELL_JOIN_W; bjoin=CELL_JOIN_E; }
    else if (BATTLE->twindy==1) { ajoin=CELL_JOIN_S; bjoin=CELL_JOIN_N; }
    else { ajoin=CELL_JOIN_N; bjoin=CELL_JOIN_S; }
    medomat_tile(dstx0+BATTLE->fallx*COLW,dsty0+BATTLE->fally*ROWH,BATTLE->fallcolor|CELL_CONTENT_PILL|ajoin);
    medomat_tile(dstx0+(BATTLE->fallx+BATTLE->twindx)*COLW,dsty0+(BATTLE->fally+BATTLE->twindy)*ROWH,BATTLE->twincolor|CELL_CONTENT_PILL|bjoin);
  }
  
  // Reserve pills, outside the field.
  int dstx=dstx0+3;
  dsty=dsty0-12;
  int i=BATTLE->reservec;
  const uint32_t deadpillcolor=0xc0c0c0ff;
  for (;i-->0;dstx+=12) {
    graf_fancy(&g.graf,dstx,dsty,0x4a,EGG_XFORM_YREV,0,NS_sys_tilesize,0,deadpillcolor);
    graf_fancy(&g.graf,dstx,dsty-ROWH,0x4a,0,0,NS_sys_tilesize,0,deadpillcolor);
  }
  
  // Show collected hearts.
  dstx=dstx0+15;
  dsty=fldy+fldh+10;
  for (i=0;i<PRIZE_LIMIT;i++,dstx+=12) {
    uint32_t color=0x202020ff;
    if (i<BATTLE->prizec) color=0xffffffff;
    graf_fancy(&g.graf,dstx,dsty,0x4c,0,0,NS_sys_tilesize,0,color);
  }
}

/* Get prizes.
 */
 
static int _medomat_get_prizes(struct prize *v,int a,struct battle *battle) {
  if ((a>=1)&&(BATTLE->prizec>0)) {
    v[0]=(struct prize){.itemid=NS_itemid_heart,.quantity=BATTLE->prizec};
    return 1;
  }
  return 0;
}

/* Type definition.
 */
 
const struct battle_type battle_type_medomat={
  .name="medomat",
  .objlen=sizeof(struct battle_medomat),
  .id=NS_battle_medomat,
  .strix_name=235,
  .no_article=0,
  .no_contest=0,
  .support_pvp=0,
  .support_cvc=0,
  .del=_medomat_del,
  .init=_medomat_init,
  .update=_medomat_update,
  .render=_medomat_render,
  .get_prizes=_medomat_get_prizes,
};

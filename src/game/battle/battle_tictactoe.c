/* battle_tictactoe.c
 */

#include "game/bellacopia.h"

#define CELL_NONE 0
#define CELL_X    1
#define CELL_O    2

#define CPU_THINK_TIME 0.500
#define CPU_MOVE_TIME 0.250

struct battle_tictactoe {
  struct battle hdr;
  uint8_t fld[9];
  int nowplaying; // 0,1, or <0 if terminated
  int selx,sely; // Not part of the player state. It resets at each turn.
  uint8_t hlt[3]; // Three cell indices to highlight at gameover.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    uint8_t cell;
    int inputpoison;
    
    int nextx,nexty;
    double cpuclock;
    double checkodds; // 0..1 = never..always, how often do we check for force plays
  } playerv[2];
};

#define BATTLE ((struct battle_tictactoe*)battle)

/* Delete.
 */
 
static void _tictactoe_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->xform=0;
    player->tileid=0;
    player->cell=CELL_X;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->tileid=1;
    player->cell=CELL_O;
  }
  if (player->human=human) { // Human.
    player->inputpoison=1;
  } else { // CPU.
    player->cpuclock=CPU_THINK_TIME;
    player->nextx=player->nexty=-1;
    player->checkodds=0.100*(1.0-player->skill)+0.900*player->skill;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x808080ff;
        player->tileid+=0x24;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid+=0x04;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid+=0x14;
      } break;
  }
}

/* New.
 */
 
static int _tictactoe_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->nowplaying=rand()&1;
  BATTLE->selx=1;
  BATTLE->sely=1;
  BATTLE->hlt[0]=BATTLE->hlt[1]=BATTLE->hlt[2]=0xff;
  return 0;
}

/* One player's turn just ended.
 * Check for completion or start the next turn.
 */
 
static int tictactoe_check_line(struct battle *battle,int ap,int bp,int cp) {
  uint8_t a=BATTLE->fld[ap];
  uint8_t b=BATTLE->fld[bp];
  uint8_t c=BATTLE->fld[cp];
  if (!a||!b||!c) return 0;
  if (a!=b) return 0;
  if (b!=c) return 0;
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->cell==a) {
    battle->outcome=1;
  } else if (r->cell==a) {
    battle->outcome=-1;
  } else {
    fprintf(stderr,"%s:%d: Unexpected cell value %d\n",__FILE__,__LINE__,a);
    battle->outcome=0;
  }
  
  BATTLE->nowplaying=-1;
  BATTLE->hlt[0]=ap;
  BATTLE->hlt[1]=bp;
  BATTLE->hlt[2]=cp;
  return 1;
}
 
static void tictactoe_change_turn(struct battle *battle) {

  // Has anybody won?
  if (tictactoe_check_line(battle,0,1,2)) return;
  if (tictactoe_check_line(battle,3,4,5)) return;
  if (tictactoe_check_line(battle,6,7,8)) return;
  if (tictactoe_check_line(battle,0,3,6)) return;
  if (tictactoe_check_line(battle,1,4,7)) return;
  if (tictactoe_check_line(battle,2,5,8)) return;
  if (tictactoe_check_line(battle,0,4,8)) return;
  if (tictactoe_check_line(battle,2,4,6)) return;
  
  // Is there any open space?
  const uint8_t *v=BATTLE->fld;
  int i=9;
  for (;i-->0;v++) if (*v==CELL_NONE) {
    BATTLE->nowplaying^=1;
    BATTLE->selx=1;
    BATTLE->sely=1;
    return;
  }
  
  // Stalemate, as usual.
  battle->outcome=0;
}

/* Player activates.
 */
 
static void player_activate(struct battle *battle,struct player *player) {
  if ((BATTLE->selx<0)||(BATTLE->selx>=3)||(BATTLE->sely<0)||(BATTLE->sely>=3)) return;
  int p=BATTLE->sely*3+BATTLE->selx;
  if (BATTLE->fld[p]!=CELL_NONE) {
    bm_sound(RID_sound_reject);
    return;
  }
  bm_sound(RID_sound_uiactivate);
  BATTLE->fld[p]=player->cell;
  tictactoe_change_turn(battle);
}

/* Move cursor.
 */
 
static void player_move(struct battle *battle,struct player *player,int dx,int dy) {
  BATTLE->selx+=dx;
  if (BATTLE->selx<0) BATTLE->selx=2;
  else if (BATTLE->selx>=3) BATTLE->selx=0;
  BATTLE->sely+=dy;
  if (BATTLE->sely<0) BATTLE->sely=2;
  else if (BATTLE->sely>=3) BATTLE->sely=0;
  bm_sound(RID_sound_uimotion);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1,0);
  else if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1,0);
  else if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) player_move(battle,player,0,-1);
  else if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) player_move(battle,player,0,1);
  else if (player->inputpoison) {
    if (!(input&EGG_BTN_SOUTH)) player->inputpoison=0;
  } else if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) player_activate(battle,player);
}

/* If among these three positions, two have the given value and the other is empty, return the empty one.
 * Otherwise negative.
 */
 
static int tictactoe_all_but_one(struct battle *battle,int ap,int bp,int cp,int q) {
  int a=BATTLE->fld[ap];
  int b=BATTLE->fld[bp];
  int c=BATTLE->fld[cp];
  if (!a&&(b==q)&&(c==q)) return ap;
  if ((a==q)&&!b&&(c==q)) return bp;
  if ((a==q)&&(b==q)&&!c) return cp;
  return -1;
}

/* Update CPU player.
 * So.... we could write an intelligent Tic-Tac-Toe player that plays into a stalemate every time.
 * But that would get kind of dull, wouldn't it? You'd never be able to win. Like the real game.
 * I'm going to keep it simple then, and have our CPU player just pick an open slot at random.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // We'll spend most of our time waiting.
  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    return;
  }

  // If we haven't pick a move yet, pick it.
  if (player->nextx<0) {
    int availablev[9]; // fld index
    int availablec=0;
    
    /* With (checkodds) zero, we play randomly.
     * At one, we check for two-in-a-rows every turn, like a sane player.
     */
    double checkn=(rand()&0xffff)/65535.0;
    if (checkn<player->checkodds) {
      int ix;
      if ((ix=tictactoe_all_but_one(battle,0,1,2,player->cell))>=0) availablev[availablec++]=ix;
      if ((ix=tictactoe_all_but_one(battle,3,4,5,player->cell))>=0) availablev[availablec++]=ix;
      if ((ix=tictactoe_all_but_one(battle,6,7,8,player->cell))>=0) availablev[availablec++]=ix;
      if ((ix=tictactoe_all_but_one(battle,0,3,6,player->cell))>=0) availablev[availablec++]=ix;
      if ((ix=tictactoe_all_but_one(battle,1,4,7,player->cell))>=0) availablev[availablec++]=ix;
      if ((ix=tictactoe_all_but_one(battle,2,5,8,player->cell))>=0) availablev[availablec++]=ix;
      if ((ix=tictactoe_all_but_one(battle,0,4,8,player->cell))>=0) availablev[availablec++]=ix;
      if ((ix=tictactoe_all_but_one(battle,2,4,6,player->cell))>=0) availablev[availablec++]=ix;
      if (!availablec) {
        uint8_t cell=(player->cell==CELL_X)?CELL_O:CELL_X;
        if ((ix=tictactoe_all_but_one(battle,0,1,2,cell))>=0) availablev[availablec++]=ix;
        if ((ix=tictactoe_all_but_one(battle,3,4,5,cell))>=0) availablev[availablec++]=ix;
        if ((ix=tictactoe_all_but_one(battle,6,7,8,cell))>=0) availablev[availablec++]=ix;
        if ((ix=tictactoe_all_but_one(battle,0,3,6,cell))>=0) availablev[availablec++]=ix;
        if ((ix=tictactoe_all_but_one(battle,1,4,7,cell))>=0) availablev[availablec++]=ix;
        if ((ix=tictactoe_all_but_one(battle,2,5,8,cell))>=0) availablev[availablec++]=ix;
        if ((ix=tictactoe_all_but_one(battle,0,4,8,cell))>=0) availablev[availablec++]=ix;
        if ((ix=tictactoe_all_but_one(battle,2,4,6,cell))>=0) availablev[availablec++]=ix;
      }
    }
    
    if (!availablec) {
      const uint8_t *p=BATTLE->fld;
      int i=0;
      for (;i<9;i++,p++) {
        if (*p==CELL_NONE) availablev[availablec++]=i;
      }
    }
    if (!availablec) {
      fprintf(stderr,"tictactoe panic! It's the CPU's turn but nothing is open.\n");
      battle->outcome=1;
      return;
    }
    int availablep=rand()%availablec;
    int i=availablev[availablep];
    player->nextx=i%3;
    player->nexty=i/3;
    player->cpuclock=CPU_MOVE_TIME;
    return;
  }
  
  // If we're on the target cell, play.
  if ((BATTLE->selx==player->nextx)&&(BATTLE->sely==player->nexty)) {
    player_activate(battle,player);
    player->cpuclock=CPU_THINK_TIME;
    player->nextx=player->nexty=-1;
    return;
  }
  
  // Move a little. If we're off both axes, randomize so we look a little more human.
  struct option { int dx,dy; } optionv[2];
  int optionc=0;
  if (BATTLE->selx!=player->nextx) optionv[optionc++]=(struct option){(BATTLE->selx<player->nextx)?1:-1,0};
  if (BATTLE->sely!=player->nexty) optionv[optionc++]=(struct option){0,(BATTLE->sely<player->nexty)?1:-1};
  int optionp=rand()%optionc;
  player_move(battle,player,optionv[optionp].dx,optionv[optionp].dy);
  player->cpuclock=CPU_MOVE_TIME;
}

/* Update.
 */
 
static void _tictactoe_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* Only the focussed player updates, and mind that there might not be one.
   */
  if ((BATTLE->nowplaying>=0)&&(BATTLE->nowplaying<2)) {
    struct player *player=BATTLE->playerv+BATTLE->nowplaying;
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
  }
}

/* Render.
 */
 
static void _tictactoe_render(struct battle *battle) {

  const int cellw=31;
  const int cellh=31;
  const int sepw=5;
  const int seph=5;
  const int fldw=cellw*3+sepw*2;
  const int fldh=cellh*3+seph*2;
  const int fldx=(FBW>>1)-(fldw>>1);
  const int fldy=(FBH>>1)-(fldh>>1);
  const uint32_t bgcolor=0x1a2786ff;
  const uint32_t linecolor=0xffffffff;
  
  // Background and grid lines.
  graf_fill_rect(&g.graf,0,0,FBW,FBH,bgcolor);
  graf_fill_rect(&g.graf,fldx+cellw,fldy,sepw,fldh,linecolor);
  graf_fill_rect(&g.graf,fldx+cellw*2+sepw,fldy,sepw,fldh,linecolor);
  graf_fill_rect(&g.graf,fldx,fldy+cellh,fldw,seph,linecolor);
  graf_fill_rect(&g.graf,fldx,fldy+cellh*2+seph,fldw,seph,linecolor);
  
  // Played pieces.
  graf_set_image(&g.graf,RID_image_battle_underground);
  int y=fldy+(NS_sys_tilesize>>1);
  int x0=fldx+(NS_sys_tilesize>>1);
  const uint8_t *p=BATTLE->fld;
  int i=0;
  int yi=3; for (;yi-->0;y+=cellh+seph) {
    int x=x0;
    int xi=3; for (;xi-->0;x+=cellw+sepw,p++,i++) {
      uint8_t tileid=0;
      switch (*p) {
        case CELL_X: tileid=0x10; break;
        case CELL_O: tileid=0x12; break;
      }
      if (tileid) {
        if ((i==BATTLE->hlt[0])||(i==BATTLE->hlt[1])||(i==BATTLE->hlt[2])) graf_set_tint(&g.graf,0x80ff80ff);
        graf_tile(&g.graf,x,y,tileid,0);
        graf_tile(&g.graf,x+NS_sys_tilesize,y,tileid+0x01,0);
        graf_tile(&g.graf,x,y+NS_sys_tilesize,tileid+0x10,0);
        graf_tile(&g.graf,x+NS_sys_tilesize,y+NS_sys_tilesize,tileid+0x11,0);
        graf_set_tint(&g.graf,0);
      }
    }
  }
  
  // Cursor.
  if (BATTLE->nowplaying>=0) {
    struct player *player=BATTLE->playerv+BATTLE->nowplaying;
    if (g.framec&16) graf_set_alpha(&g.graf,0x80);
    graf_tile(&g.graf,
      fldx+BATTLE->selx*(cellw+sepw)+(cellw>>1),
      fldy+BATTLE->sely*(cellh+seph)+(cellh>>1),
      player->tileid,player->xform
    );
    graf_set_alpha(&g.graf,0xff);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_tictactoe={
  .name="tictactoe",
  .objlen=sizeof(struct battle_tictactoe),
  .id=NS_battle_tictactoe,
  .strix_name=265,
  .no_article=1,
  .no_contest=1,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_dpad_a,
  .del=_tictactoe_del,
  .init=_tictactoe_init,
  .update=_tictactoe_update,
  .render=_tictactoe_render,
};

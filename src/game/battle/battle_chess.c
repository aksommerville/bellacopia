/* battle_chess.c
 * All the interesting stuff lives in the dedicated "chess" unit.
 * Our concern is just setup and presentation, mostly.
 */
 
//TODO When losing the minigame, can we discover and highlight the reason you lose?
// ...If the Black King is not in check, highlight him.
// ...If he is in check, highlight any of the Black pieces that could move to escape it. Oooh and also show the proposal indicator, for the cell it would move to.
 
#include "game/bellacopia.h"
#include "game/chess/chess.h"

#define CLOCK_EASY 45.0
#define CLOCK_HARD 20.0

struct battle_chess {
  struct battle hdr;
  struct chess_game chess_game;
  int use_clock;
  double clock;
  double animclock;
  int animframe;
  
  // Our player[0] is white and player[1] is black, which is reverse of chess_game's convention.
  struct player {
    int who,human;
    uint32_t color;
  } playerv[2];
};

#define BATTLE ((struct battle_chess*)battle)

/* Cleanup.
 */
 
static void _chess_del(struct battle *battle) {
}

/* Player params.
 */
 
static uint32_t chess_color(int face) {
  switch (face) {
    case NS_face_monster: return 0x5a432aff;
    case NS_face_princess: return 0x0d3ac1ff;
    case NS_face_dot: default: return 0x9c6fd2ff; // Her usual color is a bit dark for this, we want the outlines to show.
  }
}

/* Init.
 */
 
//static const uint32_t seedv[3]={0x699d0422,0x8e0751b0,0xcd5c5304};
//static int seedp=0;
 
static int _chess_init(struct battle *battle) {
  //srand(0x82b83fcd); // Generates an impossible minigame. They want us to check with the upper Rook, but it's blocked by one of our Pawns. ...fixed
  //srand(0x2845eb57); // Proposes checkmating the Black King with our own King. ...fixed
  //srand(0x699ceab4); // kingnrook generates a scenario with a Queen who is initially checking the Black King. ...fixed
  //srand(0x04fbfc0f); // corneredking produces a Black King already in check. By a Pawn, no less. ...fixed
  //srand(0xca4b2a37); // corneredking with no mate, not sure what it intended. ...Pawn blocks, and Queen was the original agent. ...It wants us to move a Pawn diagonally without capturing. ...fixed
  //srand(0x3a46f0d2); // corneredking: Agent Knight cuts off the Holder Bishop's line of sight. ...fixed
  //srand(0xcd5c5304); // foolsmate: My Queen is not given all of her options. What the hell? ...on retry, it does offer all the options.
  //if (seedp>=3) seedp=0; // ...even repeating the 3 games from that session, it didn't happen again.
  //srand(seedv[seedp++]); // ...i bet i selected the King instead of the Queen by accident. Move along.
  //srand(0x2ec926c0); // nope, yep, my bad
  //srand(0x699d15ed); // scholarsmate: Unexpected blocking Black Pawn. Not mateable. ...fixed
  //srand(0x9075c213); // ...my bad
  fprintf(stderr,"%s: seed 0x%08x\n",__func__,get_rand_seed());

  BATTLE->playerv[0].who=0;
  BATTLE->playerv[1].who=1;
  
  // The two players must not use the same face. Most games it wouldn't matter, but for us it would render the board incomprehensible.
  if (battle->args.lface==battle->args.rface) {
    if (battle->args.lface==NS_face_dot) battle->args.rface=NS_face_princess; // Both Dot, make black player Princess.
    else battle->args.lface=NS_face_dot; // Any other duplication, make white player Dot.
  }
  BATTLE->playerv[0].color=chess_color(battle->args.lface);
  BATTLE->playerv[1].color=chess_color(battle->args.rface);
  
  // We support (man,man), (man,cpu), and (cpu,cpu). The (cpu,cpu) mode is asymmetric like (man,cpu).
  // So if they requested (cpu,man), swap them.
  if (!battle->args.lctl&&battle->args.rctl) {
    battle->args.lctl=battle->args.rctl;
    battle->args.rctl=0;
    battle->args.bias=0xff-battle->args.bias;
  }
  BATTLE->playerv[0].human=battle->args.lctl;
  BATTLE->playerv[1].human=battle->args.rctl;
  
  if (battle->args.lctl&&battle->args.rctl) { // Man vs Man.
    chess_game_init(&BATTLE->chess_game,'2',0.5); // Difficulty not relevant.
    BATTLE->use_clock=0;
    
  } else { // Man vs CPU or CPU vs CPU.
    double difficulty=battle_scalar_difficulty(battle);
    chess_game_init(&BATTLE->chess_game,'1',difficulty);
    BATTLE->use_clock=1;
    BATTLE->clock=CLOCK_HARD*difficulty+CLOCK_EASY*(1.0-difficulty);
  }
  
  return 0;
}

/* Update player. Both man and CPU.
 * This is only called for the player whose turn it is right now.
 */
 
static void chess_update_player(struct battle *battle,struct player *player,double elapsed) {
  if (player->human) {
    int input=g.input[player->human];
    int pvinput=g.pvinput[player->human];
    
    // Motion.
    int dx=0,dy=0;
    if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) dx=-1;
    else if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) dx=1;
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) dy=-1;
    else if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) dy=1;
    if (dx||dy) {
      if (chess_game_move(&BATTLE->chess_game,dx,dy)<0) {
        bm_sound(RID_sound_reject);
      } else {
        bm_sound(RID_sound_uimotion);
      }
    }
    
    // Cancellation.
    if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) {
      if (chess_game_cancel(&BATTLE->chess_game)<0) {
        bm_sound(RID_sound_reject);
      } else {
        bm_sound(RID_sound_uicancel);
      }
    }
    
    // Activation.
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
      int err=chess_game_activate(&BATTLE->chess_game);
      if (err<0) {
        bm_sound(RID_sound_reject);
      } else if (err>0) {
        bm_sound(RID_sound_collect);
      } else {
        bm_sound(RID_sound_uiactivate);
      }
    }
    
  // Black CPU player. He doesn't play. If his turn arises it means the white player failed to achieve checkmate.
  } else if (player->who) {
    fprintf(stderr,"%s:%d: Player reached the dummy CPU player. Assume the human failed to achieve checkmate as required.\n",__FILE__,__LINE__);
    battle->outcome=-1;
    
  // White CPU player. ie the Princess, playing like a human.
  } else {
    fprintf(stderr,"%s:%d: TODO: Princess playing chess. For now let's just assume she loses.\n",__FILE__,__LINE__);
    battle->outcome=-1;
  }
}

/* Update.
 */
 
static void _chess_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  // Time up?
  if (BATTLE->use_clock) {
    if (BATTLE->clock>0.0) {
      if ((BATTLE->clock-=elapsed)<=0.0) {
        battle->outcome=-1;
        return;
      }
    }
  }
  
  // Game over?
  if (!(BATTLE->chess_game.assess&CHESS_ASSESS_RUNNING)) {
    if (BATTLE->chess_game.assess&CHESS_ASSESS_CHECK) {
      if (BATTLE->chess_game.assess&CHESS_ASSESS_WHO) { // Black wins.
        battle->outcome=-1;
      } else { // White wins.
        battle->outcome=1;
      }
    } else { // Stalemate or invalid game.
      battle->outcome=0;
    }
    return;
  }
  
  // Animate cursor.
  if ((BATTLE->animclock-=elapsed)<=0.0) {
    BATTLE->animclock+=0.200;
    if (++(BATTLE->animframe)>=2) BATTLE->animframe=0;
  }
  
  // Update the appropriate player.
  struct player *white=BATTLE->playerv;
  struct player *black=white+1;
  if (BATTLE->chess_game.turn) { // White's turn.
    chess_update_player(battle,white,elapsed);
  } else {
    chess_update_player(battle,black,elapsed);
  }
  
  // AUX1 for a modal to declare stalemate or forfeit.
  //TODO
  
  //XXX
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render.
 */
 
static void _chess_render(struct battle *battle) {

  /* Background.
   * If the clock is in play, blink at the last 5 seconds.
   */
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x304060ff);
  int sec=0;
  if (BATTLE->use_clock&&(BATTLE->clock>0.0)) {
    int ms=(int)(BATTLE->clock*1000.0);
    sec=ms/1000+1;
    if (sec<1) sec=1;
    if (sec<=5) {
      ms%=1000;
      if (ms>750) {
        int alpha=ms-750;
        uint32_t rgb=(sec==1)?0xff000000:0xe0a00000;
        graf_fill_rect(&g.graf,0,0,FBW,FBH,rgb|alpha);
      }
    }
  }
  graf_set_image(&g.graf,RID_image_battle_labyrinth2);
  
  // The 8x8 board.
  int boardw=NS_sys_tilesize*8;
  int boardh=NS_sys_tilesize*8;
  int boardx=(FBW>>1)-(boardw>>1);
  int boardy=(FBH>>1)-(boardh>>1);
  int dstx0=boardx+(NS_sys_tilesize>>1);
  int dsty0=boardy+(NS_sys_tilesize>>1);
  int dsty=dsty0,dstx;
  int yi=8;
  for (;yi-->0;dsty+=NS_sys_tilesize) {
    dstx=dstx0;
    int xi=8;
    for (;xi-->0;dstx+=NS_sys_tilesize) {
      graf_tile(&g.graf,dstx,dsty,((xi&1)==(yi&1))?0x00:0x01,0);
    }
  }
  
  // A tasteful boarder.
  graf_tile(&g.graf,dstx0-NS_sys_tilesize,dsty0-NS_sys_tilesize,0x10,0);
  graf_tile(&g.graf,dstx0+boardw         ,dsty0-NS_sys_tilesize,0x12,0);
  graf_tile(&g.graf,dstx0-NS_sys_tilesize,dsty0+boardh         ,0x30,0);
  graf_tile(&g.graf,dstx0+boardw         ,dsty0+boardh         ,0x32,0);
  for (yi=8,dstx=dstx0,dsty=dsty0;yi-->0;dstx+=NS_sys_tilesize,dsty+=NS_sys_tilesize) {
    graf_tile(&g.graf,dstx,dsty0-NS_sys_tilesize,0x11,0);
    graf_tile(&g.graf,dstx,dsty0+boardh         ,0x31,0);
    graf_tile(&g.graf,dstx0-NS_sys_tilesize,dsty,0x20,0);
    graf_tile(&g.graf,dstx0+boardw         ,dsty,0x22,0);
  }
  
  // Pieces. Not interleaved with the board, because those are tiles and these are fancies.
  const uint8_t *boardp=BATTLE->chess_game.board;
  for (yi=8,dsty=dsty0;yi-->0;dsty+=NS_sys_tilesize) {
    dstx=dstx0;
    int xi=8;
    for (;xi-->0;dstx+=NS_sys_tilesize,boardp++) {
      uint8_t tileid=(*boardp)&PIECE_ROLE_MASK;
      if (tileid) {
        tileid+=1;
        uint8_t xform=0;
        if ((((*boardp)&PIECE_ROLE_MASK)==PIECE_KING)&&(battle->outcome>-2)) {
          if ((*boardp)&PIECE_WHITE) { // White King. Is he surrendered?
            if (battle->outcome<=0) xform=EGG_XFORM_SWAP;
          } else { // Black King. Is he surrendered?
            if (battle->outcome>=0) xform=EGG_XFORM_SWAP;
          }
        }
        uint32_t color=((*boardp)&PIECE_WHITE)?BATTLE->playerv[0].color:BATTLE->playerv[1].color;
        graf_fancy(&g.graf,dstx,dsty,tileid,xform,0,NS_sys_tilesize,0,color);
      }
    }
  }
  
  // Cursor.
  if (BATTLE->chess_game.assess&CHESS_ASSESS_RUNNING) {
    struct player *player=BATTLE->playerv+(BATTLE->chess_game.turn?0:1);
    if (player->who&&!player->human) {
      // Don't draw the dummy CPU player's cursor. It does appear active for exactly one frame every time.
    } else {
      int x,y;
      if (player->who) {
        x=BATTLE->chess_game.bx;
        y=BATTLE->chess_game.by;
      } else {
        x=BATTLE->chess_game.wx;
        y=BATTLE->chess_game.wy;
      }
      graf_fancy(&g.graf,
        dstx0+x*NS_sys_tilesize,dsty0+y*NS_sys_tilesize,
        0x08+BATTLE->animframe,0,
        0,NS_sys_tilesize,0,player->color
      );
    }
  }
  
  // Proposals. Should go before cursor, but they won't overlap, so let cursor join the pieces' batch.
  if (BATTLE->chess_game.fmovec) {
    uint16_t *move=BATTLE->chess_game.fmovev;
    int i=BATTLE->chess_game.fmovec;
    for (;i-->0;move++) {
      int x=CHESS_MOVE_TO_COL(*move);
      int y=CHESS_MOVE_TO_ROW(*move);
      x=dstx0+x*NS_sys_tilesize;
      y=dsty0+y*NS_sys_tilesize;
      graf_tile(&g.graf,x,y,0x0a,0);
    }
  }
  
  // Clock, optionally.
  if (sec>0) {
    int cx=(FBW>>1)-4;
    int cy=dsty0-20;
    graf_set_image(&g.graf,RID_image_fonttiles);
    if (sec>=10) graf_tile(&g.graf,cx,cy,'0'+sec/10,0); cx+=8;
    graf_tile(&g.graf,cx,cy,'0'+sec%10,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_chess={
  .name="chess",
  .objlen=sizeof(struct battle_chess),
  .strix_name=180,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .no_timeout=1, // For 2-player mode. Don't expect them to finish in 60 seconds :D
  .del=_chess_del,
  .init=_chess_init,
  .update=_chess_update,
  .render=_chess_render,
};

#if 0 /* XXX First attempt. Mostly works but some fatal flaws. */
//XXX Rip this out, replace with a dedicated model unit. battle_chess should ultimately be lean, just presentation and config concerns.
/* battle_chess.c
 * Canned chess scenarios where the player is one move away from checkmate, and they have to find that move.
 * Piece movements should be modelled generically against the real rules of chess.
 * Do allow a 2-player mode, which is actually just chess.
 * But no real man-vs-cpu mode, just those canned scenarios.
 * Chess is traditionally presented with the players separated vertically, where Bellacopia's general preference is horizontal.
 * We'll go vertical. Player zero, usually Left, is Bottom. Player one, usually Right, is Top.
 */

#include "game/bellacopia.h"

#define TILEID_PAWN 0x02
#define TILEID_KNIGHT 0x03
#define TILEID_BISHOP 0x04
#define TILEID_ROOK 0x05
#define TILEID_QUEEN 0x06
#define TILEID_KING 0x07

#define CLOCK_WORST 20.0
#define CLOCK_BEST  45.0

struct battle_chess {
  struct battle hdr;
  int turn; // (-1,0,1)=waiting,player0,player1. Only names a player if they are human.
  double animclock;
  int animframe;
  int use_clock; // For 1-player mode only.
  double clock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    int selx,sely;
  } playerv[2];
  
  struct piece {
    uint8_t tileid;
    uint32_t color;
    int x,y; // In board cells.
    // ^ Don't change order of the above! They're initialized inline.
    int movec; // How many times have I moved? Pawns need to know.
    uint8_t xform;
  } piecev[32];
  int piecec;
  
  /* Buffer for proposed moves.
   * (x<<4)|y.
   */
  uint8_t propv[64];
  int propc;
  struct piece *selpiece;
};

#define BATTLE ((struct battle_chess*)battle)

static int chess_propose_moves(uint8_t *propv,int propa,struct battle *battle,struct piece *piece,int recursion_limit);

/* XXX highly temporary: dump the pieces' positions to stderr for troubleshooting.
 */
 
static void log_board(struct battle *battle) {
  char msg[17*8];
  int y=0; for (;y<8;y++) {
    char *line=msg+y*17;
    line[16]=0x0a;
    int x=0; for (;x<8;x++,line+=2) {
      if ((x&1)==(y&1)) memcpy(line,"..",2);
      else memcpy(line,"  ",2);
    }
  }
  struct piece *piece=BATTLE->piecev;
  int i=BATTLE->piecec;
  for (;i-->0;piece++) {
    char id='?';
    switch (piece->tileid) {
      case TILEID_PAWN: id='p'; break;
      case TILEID_ROOK: id='r'; break;
      case TILEID_KNIGHT: id='n'; break;
      case TILEID_BISHOP: id='b'; break;
      case TILEID_QUEEN: id='q'; break;
      case TILEID_KING: id='k'; break;
    }
    if (piece->color==BATTLE->playerv[0].color) {
      if ((id>='a')&&(id<='z')) id-=0x20;
    }
    char *dst=msg+17*piece->y+2*piece->x;
    dst[0]=dst[1]=id;
  }
  fprintf(stderr,"%.*s\n",(int)sizeof(msg),msg);
}

/* Delete.
 */
 
static void _chess_del(struct battle *battle) {
}

/* Return the color that this piece isn't.
 */
 
static uint32_t chess_other_color(struct battle *battle,uint32_t not_this) {
  if (BATTLE->playerv[0].color==not_this) return BATTLE->playerv[1].color;
  return BATTLE->playerv[0].color;
}

/* Piece at cell.
 */
 
static struct piece *chess_piece_at_cell(struct battle *battle,int x,int y) {
  struct piece *piece=BATTLE->piecev;
  int i=BATTLE->piecec;
  for (;i-->0;piece++) {
    if (piece->x!=x) continue;
    if (piece->y!=y) continue;
    return piece;
  }
  return 0;
}

/* Remove a piece, by location.
 */
 
static void chess_remove_piece_at_cell(struct battle *battle,int x,int y) {
  struct piece *piece=BATTLE->piecev+BATTLE->piecec-1;
  int i=BATTLE->piecec;
  for (;i-->0;piece--) {
    if (piece->x!=x) continue;
    if (piece->y!=y) continue;
    BATTLE->piecec--;
    memmove(piece,piece+1,sizeof(struct piece)*(BATTLE->piecec-i));
    return;
  }
}

/* Nonzero if the given cell is in bounds and has no piece present.
 */
 
static int chess_cell_is_vacant(struct battle *battle,int x,int y) {
  if ((x<0)||(y<0)||(x>=8)||(y>=8)) return 0;
  if (chess_piece_at_cell(battle,x,y)) return 0;
  return 1;
}

/* Scan a proposal list for a given position.
 */
 
static int chess_propv_contains(const uint8_t *propv,int propc,int x,int y) {
  if ((x<0)||(y<0)||(x>=8)||(y>=8)) return 0;
  uint8_t q=(x<<4)|y;
  for (;propc-->0;propv++) if (*propv==q) return 1;
  return 0;
}

/* Nonzero if the king of the given color is currently in check.
 */
 
static int chess_is_check(struct battle *battle,uint32_t color,int recursion_limit) {

  // Find our King. If he's not there, um, I guess the answer is Yes?
  int kingx=-1,kingy=-1;
  struct piece *piece=BATTLE->piecev;
  int i=BATTLE->piecec;
  for (;i-->0;piece++) {
    if (piece->color!=color) continue;
    if (piece->tileid!=TILEID_KING) continue;
    kingx=piece->x;
    kingy=piece->y;
    break;
  }
  if (kingx<0) return 1;
  
  // Iterate all pieces of the other color, check all their moves, any of those moves is the King's position, the answer is Yes.
  for (piece=BATTLE->piecev,i=BATTLE->piecec;i-->0;piece++) {
    if (piece->color==color) continue;
    uint8_t propv[64];
    int propc=chess_propose_moves(propv,64,battle,piece,recursion_limit);
    const uint8_t *prop=propv;
    for (;propc-->0;prop++) {
      int x=(*prop)>>4;
      int y=(*prop)&15;
      if ((x==kingx)&&(y==kingy)) return 1;
    }
  }
  
  return 0;
}

/* Nonzero if (piece)'s king would be in check, if (piece) were moved to (x,y).
 */
 
static int chess_would_be_check(struct battle *battle,struct piece *piece,int x,int y,int recursion_limit) {

  // If it's taking a piece, throw that other guy off the board temporarily.
  struct piece *taken=chess_piece_at_cell(battle,x,y);
  if (taken) {
    taken->x-=8;
    taken->y-=8;
  }
  
  // Remember where (piece) was originally, we'll have to restore it.
  int x0=piece->x;
  int y0=piece->y;
  piece->x=x;
  piece->y=y;
  
  // Draw the rest of the owl.
  int result=chess_is_check(battle,piece->color,recursion_limit);
  
  // Put things back where they were.
  piece->x=x0;
  piece->y=y0;
  if (taken) {
    taken->x+=8;
    taken->y+=8;
  }
  return result;
}

/* Nonzero if the given color has no moves.
 * Returns one of:
 *   0: Moves can be made.
 *   1: Stalemate. No moves but he's not in check either.
 *   2: Checkmate.
 */
 
static int chess_is_mate(struct battle *battle,uint32_t color) {
  int kingx=-1,kingy=-1;
  struct piece *piece=BATTLE->piecev;
  int i=BATTLE->piecec;
  for (;i-->0;piece++) {
    if (piece->color!=color) continue;
    uint8_t propv[64];
    if (chess_propose_moves(propv,64,battle,piece,2)) return 0;
    if (piece->tileid==TILEID_KING) {
      kingx=piece->x;
      kingy=piece->y;
    }
  }
  // We're out of moves, so it's one of the mates. Look at the other side's pieces to determine whether we're in check right now.
  for (piece=BATTLE->piecev,i=BATTLE->piecec;i-->0;piece++) {
    if (piece->color==color) continue;
    uint8_t propv[64];
    int propc=chess_propose_moves(propv,64,battle,piece,2);
    if (chess_propv_contains(propv,propc,kingx,kingy)) return 2;
  }
  return 1;
}

/* Propose moves.
 * Fills (propv) with ((x<<4)|y) for all the positions (piece) can move to.
 */
 
static int chess_propose_moves(uint8_t *propv,int propa,struct battle *battle,struct piece *piece,int recursion_limit) {
  if (!piece) return 0;
  if (--recursion_limit<0) return 0;
  int propc=0;
  #define ADDPROP(x,y) { \
    if (propc<propa) propv[propc]=((x)<<4)|(y); \
    propc++; \
  }
  // Resolves to null or the piece at this cell. If it's an enemy piece or vacant, we propose it too.
  #define VACANT_OR_ENEMY(x,y) ({ \
    struct piece *result=0; \
    int _x=(x),_y=(y); \
    if ((_x>=0)&&(_y>=0)&&(_x<8)&&(_y<8)) { \
      result=chess_piece_at_cell(battle,_x,_y); \
      if (!result||(result->color!=piece->color)) { \
        ADDPROP(_x,_y) \
      } \
    } \
    (result); \
  })
  switch (piece->tileid) {
  
    case TILEID_PAWN: {
        int dy=(piece->color==BATTLE->playerv[0].color)?-1:1;
        if (chess_cell_is_vacant(battle,piece->x,piece->y+dy)) {
          ADDPROP(piece->x,piece->y+dy)
          if (!piece->movec&&chess_cell_is_vacant(battle,piece->x,piece->y+dy*2)) {
            ADDPROP(piece->x,piece->y+dy*2)
          }
        }
        struct piece *other;
        if (other=chess_piece_at_cell(battle,piece->x-1,piece->y+dy)) {
          if (other->color!=piece->color) {
            ADDPROP(piece->x-1,piece->y+dy)
          }
        }
        if (other=chess_piece_at_cell(battle,piece->x+1,piece->y+dy)) {
          if (other->color!=piece->color) {
            ADDPROP(piece->x+1,piece->y+dy)
          }
        }
      } break;
      
    case TILEID_KNIGHT: {
        const int moves[]={ 1,-2, 2,-1, 2,1, 1,2, -1,2, -2,1, -2,-1, -1,-2 }; // (dx0,dy0,...,dx7,dy7)
        const int *delta=moves;
        int i=8;
        for (;i-->0;delta+=2) {
          VACANT_OR_ENEMY(piece->x+delta[0],piece->y+delta[1]);
        }
      } break;
    
    // Bishop, Rook, and Queen are the same thing, just Bishop and Rook only get half of the possible vectors.
    case TILEID_BISHOP:
    case TILEID_ROOK:
    case TILEID_QUEEN: {
        struct cursor { int dx,dy,x,y,ok; } cursorv[8]={
          {-1,-1,piece->x,piece->y,1},
          { 1,-1,piece->x,piece->y,1},
          {-1, 1,piece->x,piece->y,1},
          { 1, 1,piece->x,piece->y,1},
          {-1, 0,piece->x,piece->y,1},
          { 1, 0,piece->x,piece->y,1},
          { 0,-1,piece->x,piece->y,1},
          { 0, 1,piece->x,piece->y,1},
        };
        if (piece->tileid==TILEID_BISHOP) {
          cursorv[4].ok=cursorv[5].ok=cursorv[6].ok=cursorv[7].ok=0;
        } else if (piece->tileid==TILEID_ROOK) {
          cursorv[0].ok=cursorv[1].ok=cursorv[2].ok=cursorv[3].ok=0;
        }
        for (;;) {
          int anyok=0,i=8;
          struct cursor *cursor=cursorv;
          for (;i-->0;cursor++) {
            if (!cursor->ok) continue;
            cursor->x+=cursor->dx;
            cursor->y+=cursor->dy;
            if ((cursor->x<0)||(cursor->y<0)||(cursor->x>=8)||(cursor->y>=8)) {
              cursor->ok=0;
              continue;
            }
            if (VACANT_OR_ENEMY(cursor->x,cursor->y)) {
              cursor->ok=0;
              continue;
            }
            anyok=1;
          }
          if (!anyok) break;
        }
      } break;
      
    case TILEID_KING: {
        VACANT_OR_ENEMY(piece->x-1,piece->y-1);
        VACANT_OR_ENEMY(piece->x  ,piece->y-1);
        VACANT_OR_ENEMY(piece->x+1,piece->y-1);
        VACANT_OR_ENEMY(piece->x-1,piece->y);
        VACANT_OR_ENEMY(piece->x+1,piece->y);
        VACANT_OR_ENEMY(piece->x-1,piece->y+1);
        VACANT_OR_ENEMY(piece->x  ,piece->y+1);
        VACANT_OR_ENEMY(piece->x+1,piece->y+1);
      } break;
      
  }
  #undef ADDPROP
  #undef VACANT_OR_ENEMY
  
  /* Any moves that would put (piece)'s king in check are not valid.
   * By extension, if the king is in check elsewhere and this doesn't resolve it, there are no moves for this piece.
   */
  if ((recursion_limit>0)&&(propc<=propa)) {
    int i=propc;
    while (i-->0) {
      int x=propv[i]>>4;
      int y=propv[i]&15;
      if (chess_would_be_check(battle,piece,x,y,recursion_limit)) {
        propc--;
        memmove(propv+i,propv+i+1,propc-i);
      }
    }
  }
  
  return propc;
}

/* Find the given King and turn him sideways.
 */
 
static void chess_kill_king(struct battle *battle,uint32_t color) {
  struct piece *deadking=BATTLE->piecev;
  int i=BATTLE->piecec;
  for (;i-->0;deadking++) {
    if (deadking->tileid!=TILEID_KING) continue;
    if (deadking->color!=color) continue;
    deadking->xform=EGG_XFORM_SWAP;
    return;
  }
}

/* Move a piece to the given position, make sound effects, and advance state accordingly.
 * Caller is responsible for ensuring it's a legal move. (eg get it from chess_propose_moves()).
 */
 
static void chess_move_piece(struct battle *battle,struct piece *piece,int x,int y) {
  struct piece *other=chess_piece_at_cell(battle,x,y);
  if (other) {
    bm_sound(RID_sound_collect);
    int otherp=other-BATTLE->piecev;
    if ((otherp>=0)&&(otherp<BATTLE->piecec)) {
      BATTLE->piecec--;
      memmove(other,other+1,sizeof(struct piece)*(BATTLE->piecec-otherp));
      if (piece>other) piece--;
    }
  } else {
    bm_sound(RID_sound_wandstroke);
  }
  piece->x=x;
  piece->y=y;
  
  BATTLE->selpiece=0;
  BATTLE->propc=0;
  BATTLE->turn^=1;
  
  uint32_t nextcolor=chess_other_color(battle,piece->color);
  int mate=chess_is_mate(battle,nextcolor);
  if (mate>=2) { // Checkmate.
    chess_kill_king(battle,nextcolor);
    battle->outcome=(nextcolor==BATTLE->playerv[0].color)?-1:1;
  } else if (mate==1) { // Stalemate.
    battle->outcome=0;
  } else {
    // In one-player mode, you only get one move. There's no actual CPU player.
    // So if it wasn't checkmate, you lose.
    if (battle->args.lctl&&!battle->args.rctl) {
      battle->outcome=-1;
    }
  }
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xd9d1c6ff;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
}

static int chess_valid_face(int face) {
  switch (face) {
    case NS_face_monster:
    case NS_face_dot:
    case NS_face_princess:
      return face;
  }
  return NS_face_monster;
}

/* Prepare the board for a clean start, ie a real game of chess.
 * For 2-player mode.
 */
 
static void chess_add_back_row(struct battle *battle,int y,uint32_t color) {
  if (BATTLE->piecec>64-8) return;
  // At render, we'll arrange for this to have been the correct order of Queen and King.
  const uint8_t tileidv[]={TILEID_ROOK,TILEID_KNIGHT,TILEID_BISHOP,TILEID_QUEEN,TILEID_KING,TILEID_BISHOP,TILEID_KNIGHT,TILEID_ROOK};
  const uint8_t *tileid=tileidv;
  int x=0;
  for (;x<8;x++,tileid++) BATTLE->piecev[BATTLE->piecec++]=(struct piece){*tileid,color,x,y};
}

static void chess_add_front_row(struct battle *battle,int y,uint32_t color) {
  if (BATTLE->piecec>64-8) return;
  int x=0;
  for (;x<8;x++) BATTLE->piecev[BATTLE->piecec++]=(struct piece){TILEID_PAWN,color,x,y};
}
 
static void chess_init_clean(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  chess_add_back_row(battle,0,r->color);
  chess_add_front_row(battle,1,r->color);
  chess_add_front_row(battle,6,l->color);
  chess_add_back_row(battle,7,l->color);
  // Initially focus a middle pawn.
  l->selx=3;
  l->sely=6;
  r->selx=3;
  r->sely=1;
}

/* Nonzero if the current board is not mate, and one move from player zero could mate it.
 * ie the condition of a fresh one-player game.
 */
 
static int chess_is_one_move_from_mate(struct battle *battle) {

  // Locate the kings.
  struct piece *lking=0,*rking=0;
  struct piece *piece=BATTLE->piecev;
  int i=BATTLE->piecec;
  for (;i-->0;piece++) {
    if (piece->tileid==TILEID_KING) {
      if (piece->color==BATTLE->playerv[0].color) lking=piece;
      else rking=piece;
    }
  }
  if (!lking||!rking) return 0;
  
  /* Consider every possible move from this board.
   * This pass will detect whether anyone is currently in check, and bail out.
   * Also collect every move for player zero; if this pass succeeds, we're going to check each of those one step further.
   */
  #define STEP_LIMIT (15*14+21) /* Most pieces have 14 or fewer moves at the limit. Only the Queen has more, she has 21. */
  struct step { struct piece *piece; uint8_t prop; } stepv[STEP_LIMIT];
  int stepc=0;
  int lok=0,rok=0;
  for (piece=BATTLE->piecev,i=BATTLE->piecec;i-->0;piece++) {
    uint8_t propv[64];
    int propc=chess_propose_moves(propv,64,battle,piece,2);
    if (!propc) continue; // This piece can't move. No worries, move along.
    if (piece->color==BATTLE->playerv[0].color) {
      lok=1;
      if (chess_propv_contains(propv,propc,rking->x,rking->y)) return 0; // Right is in check.
      const uint8_t *prop=propv;
      for (;propc-->0;prop++) {
        if (stepc>=STEP_LIMIT) break; // oops
        stepv[stepc++]=(struct step){piece,*prop};
      }
    } else {
      rok=1;
      if (chess_propv_contains(propv,propc,lking->x,lking->y)) return 0; // Left is in check.
    }
  }
  if (!lok||!rok) return 0; // One side is already mated.
  
  /* We've captured some number of next moves in (stepv).
   * Try each of them and test whether it's checkmate.
   * First positive response and we're done with a Yes.
   */
  struct step *step=stepv;
  for (i=stepc;i-->0;step++) {
    int x0=step->piece->x;
    int y0=step->piece->y;
    step->piece->x=step->prop>>4;
    step->piece->y=step->prop&15;
    int mate=chess_is_mate(battle,BATTLE->playerv[1].color);
    step->piece->x=x0;
    step->piece->y=y0;
    if (mate>=2) {
      fprintf(stderr,"%s: Mate if piece 0x%02x at (%d,%d) moves to (%d,%d)\n",__func__,step->piece->tileid,step->piece->x,step->piece->y,step->prop>>4,step->prop&15);
      return 1;
    }
  }
  
  #undef STEP_LIMIT
  return 0;
}

/* Place a distraction piece.
 * Select randomly among the pieces not already on the board, and select a random vacant position.
 * Returns nonzero if we placed one, or zero if full.
 */
 
static int chess_place_distraction(struct battle *battle) {
  if (BATTLE->piecec>=32) return 0;

  // Select tileid and color from all the missing pieces. Don't bother looking for kings; they are required to exist already.
  uint32_t lcolor=BATTLE->playerv[0].color;
  uint32_t rcolor=BATTLE->playerv[1].color;
  int lpawnc=0,lrookc=0,lbishopc=0,lknightc=0,lqueenc=0;
  int rpawnc=0,rrookc=0,rbishopc=0,rknightc=0,rqueenc=0;
  const struct piece *piece=BATTLE->piecev;
  int i=BATTLE->piecec;
  for (;i-->0;piece++) {
    switch (piece->tileid) {
      case TILEID_PAWN: if (piece->color==lcolor) lpawnc++; else rpawnc++; break;
      case TILEID_ROOK: if (piece->color==lcolor) lrookc++; else rrookc++; break;
      case TILEID_BISHOP: if (piece->color==lcolor) lbishopc++; else rbishopc++; break;
      case TILEID_KNIGHT: if (piece->color==lcolor) lknightc++; else rknightc++; break;
      case TILEID_QUEEN: if (piece->color==lcolor) lqueenc++; else rqueenc++; break;
    }
  }
  struct candidate { uint8_t tileid; uint32_t color; } candidatev[32];
  int candidatec=0;
  while (lpawnc++<8) candidatev[candidatec++]=(struct candidate){TILEID_PAWN,lcolor};
  while (rpawnc++<8) candidatev[candidatec++]=(struct candidate){TILEID_PAWN,rcolor};
  while (lrookc++<2) candidatev[candidatec++]=(struct candidate){TILEID_ROOK,lcolor};
  while (rrookc++<2) candidatev[candidatec++]=(struct candidate){TILEID_ROOK,rcolor};
  while (lbishopc++<2) candidatev[candidatec++]=(struct candidate){TILEID_BISHOP,lcolor};
  while (rbishopc++<2) candidatev[candidatec++]=(struct candidate){TILEID_BISHOP,rcolor};
  while (lknightc++<2) candidatev[candidatec++]=(struct candidate){TILEID_KNIGHT,lcolor};
  while (rknightc++<2) candidatev[candidatec++]=(struct candidate){TILEID_KNIGHT,rcolor};
  if (!lqueenc) candidatev[candidatec++]=(struct candidate){TILEID_QUEEN,lcolor};
  if (!rqueenc) candidatev[candidatec++]=(struct candidate){TILEID_QUEEN,rcolor};
  if (!candidatec) return 0;
  int candidatep=rand()%candidatec;
  uint8_t tileid=candidatev[candidatep].tileid;
  uint32_t color=candidatev[candidatep].color;
  
  // Generate a board where nonzero means occupied or otherwise ineligible.
  const struct piece *otherbishop=0;
  uint8_t grid[64]={0};
  for (piece=BATTLE->piecev,i=BATTLE->piecec;i-->0;piece++) {
    grid[piece->y*8+piece->x]=1;
    if ((piece->color==color)&&(piece->tileid==TILEID_BISHOP)) otherbishop=piece;
  }
  if (tileid==TILEID_PAWN) { // Forbid pawns on both back rows.
    memset(grid,1,8);
    memset(grid+7*8,1,8);
  } else if (tileid==TILEID_BISHOP) { // Two bishops of the same color must occupy different cell colors.
    if (otherbishop) {
      // There are more efficient ways to do this but I messed it up the first time and now want to be pedantic about it:
      int even=((otherbishop->x&1)==(otherbishop->y&1));
      uint8_t *p=grid;
      int y=0;
      for (;y<8;y++) {
        int x=0;
        for (;x<8;x++,p++) {
          if (even) {
            if ((x&1)==(y&1)) *p=1;
          } else {
            if ((x&1)!=(y&1)) *p=1;
          }
        }
      }
    }
  }
  
  // Count candidate cells. Zero is not possible (worst case scenario: 31 are occupied already and we're a bishop so 32 are eliminated, exactly 1 remains vacant).
  // But it's easy to check so we will.
  // Then pick one randomly, seek to it again, and put our guy there.
  int cellc=0;
  const uint8_t *p=grid;
  for (i=64;i-->0;p++) if (!*p) cellc++;
  if (!cellc) return 0;
  int cellp=rand()%cellc;
  for (i=0;i<64;i++) {
    if (grid[i]) continue;
    if (!cellp--) {
      int x=i%8;
      int y=i/8;
      BATTLE->piecev[BATTLE->piecec++]=(struct piece){tileid,color,x,y};
      return 1;
    }
  }
  return 0;
}

/* Remove any pieces at or above index (p) if they are immediately threatening the enemy king.
 * We have to do this, and repeatedly, because that check causes forced play which could make us miss that we're in danger.
 * Which is to say, the random-puzzle generator is working backward at this point, and any check is a paradox that it can't resolve.
 */
 
static void chess_remove_impudents(struct battle *battle,int p) {
  struct piece *lking=0,*rking=0;
  int i=BATTLE->piecec;
  struct piece *piece=BATTLE->piecev;
  for (;i-->0;piece++) {
    if (piece->tileid==TILEID_KING) {
      if (piece->color==BATTLE->playerv[0].color) lking=piece;
      else rking=piece;
    }
  }
  if (!lking||!rking) return; // oops?
  for (i=BATTLE->piecec,piece=BATTLE->piecev+BATTLE->piecec-1;i-->p;piece--) {
    uint8_t kingprop;
    if (piece->color==lking->color) {
      kingprop=(rking->x<<4)|rking->y;
    } else {
      kingprop=(lking->x<<4)|lking->y;
    }
    uint8_t propv[64];
    /* Generate proposals with a recursion limit of 1, ie no recursion.
     * This means it won't filter out moves that are made illegal by check states.
     */
    int propc=chess_propose_moves(propv,64,battle,piece,1);
    const uint8_t *prop=propv;
    for (;propc-->0;prop++) {
      if (*prop==kingprop) {
        //fprintf(stderr,"*** %s: removing tile 0x%02x at (%d,%d) due to checking the enemy king ***\n",__func__,piece->tileid,piece->x,piece->y);
        BATTLE->piecec--;
        memmove(piece,piece+1,sizeof(struct piece)*(BATTLE->piecec-i));
        break;
      }
    }
  }
}

/* Perform a random axiswise transform on all pieces.
 * We check first whether any pawns are present; if so, only XREV is available.
 * If there's no pawns, all 8 axiswise transforms are available.
 * Scenario generators might find it convenient to compose the scenario in a fixed orientation, then call this to shake it up.
 */
 
static void chess_transform_board(struct battle *battle) {
  uint8_t mask=EGG_XFORM_XREV|EGG_XFORM_YREV|EGG_XFORM_SWAP;
  struct piece *piece=BATTLE->piecev;
  int i=BATTLE->piecec;
  for (;i-->0;piece++) if (piece->tileid==TILEID_PAWN) { mask=EGG_XFORM_XREV; break; }
  uint8_t xform=rand()&mask;
  if (!xform) return;
  if (xform&EGG_XFORM_SWAP) {
    for (piece=BATTLE->piecev,i=BATTLE->piecec;i-->0;piece++) {
      int tmp=piece->y;
      piece->y=piece->x;
      piece->x=tmp;
    }
  }
  if (xform&EGG_XFORM_XREV) {
    for (piece=BATTLE->piecev,i=BATTLE->piecec;i-->0;piece++) {
      piece->x=7-piece->x;
    }
  }
  if (xform&EGG_XFORM_YREV) {
    for (piece=BATTLE->piecev,i=BATTLE->piecec;i-->0;piece++) {
      piece->y=7-piece->y;
    }
  }
}

/* Make a new piece.
 * Caller ensures there's room; at the init functions the set is always initially empty.
 * Caller must set (x,y) after.
 * This is just to save a little typing in the init functions.
 */
 
static struct piece *genpiece(struct battle *battle,int who,uint8_t tileid) {
  struct piece *piece=BATTLE->piecev+BATTLE->piecec++;
  piece->color=BATTLE->playerv[who].color;
  piece->tileid=tileid;
  return piece;
}

/* Initial scenarios, with only the critical pieces.
 * These will leave the board in a usable state, ie player zero can make one move to create checkmate.
 * Nothing unnecessary should be added.
 */
 
typedef void (*init1_fn)(struct battle *battle);

static void init1_rookroll(struct battle *battle) {
  // Enemy King is alone, and my King has two Rooks.
  struct piece *eking=genpiece(battle,1,TILEID_KING);
  struct piece *mking=genpiece(battle,0,TILEID_KING);
  struct piece *rook1=genpiece(battle,0,TILEID_ROOK);
  struct piece *rook2=genpiece(battle,0,TILEID_ROOK);
  
  // Enemy King goes on the top row, leaving at least 3 columns vacant on the right.
  eking->y=0;
  eking->x=rand()%5;
  
  // First Rook goes in the second row, at least 2 columns right of Enemy King.
  rook1->y=1;
  rook1->x=eking->x+2+rand()%(8-eking->x-2);
  
  // Second Rook goes in any row below the second, in one of 4 columns: Not facing or adjacent to eking, or facing rook1.
  rook2->y=2+rand()%6;
  int xchoice=rand()%4;
  if (xchoice>=eking->x-1) xchoice+=3;
  if (xchoice>=rook1->x) xchoice++;
  rook2->x=xchoice;
  
  // My King goes in the six bottom rows. Forbid rook2's column, in case the king ends up forward of him.
  mking->y=2+rand()%6;
  mking->x=rand()%7;
  if (mking->x>=rook2->x) mking->x++;
  
  chess_transform_board(battle);
}

static void init1_kingnqueen(struct battle *battle) {
  // Enemy King is against a wall, My King is blocking him, and My Queen is positioned to move to the space between them.
  struct piece *eking=genpiece(battle,1,TILEID_KING);
  struct piece *mking=genpiece(battle,0,TILEID_KING);
  struct piece *queen=genpiece(battle,0,TILEID_QUEEN);
  eking->y=0;
  eking->x=rand()&7;
  mking->y=2;
  mking->x=eking->x;
  // Queen goes at least two columns away, to ensure she's not already checking the King. Row or diagonal.
  queen->x=rand()%5;
  if (queen->x>=eking->x-1) queen->x+=3;
  if (rand()&1) { // On the in-between row, for a horizontal move.
    queen->y=1;
  } else { // On the diagonal.
    queen->y=queen->x-eking->x;
    if (queen->y<0) queen->y=-queen->y;
    queen->y+=1;
  }
  chess_transform_board(battle);
}

static void init1_kingnrook(struct battle *battle) {
  // Almost the same setup as kingnqueen, but here we're going to Rook to the King's row instead of Queening to the inbetween.
  struct piece *eking=genpiece(battle,1,TILEID_KING);
  struct piece *mking=genpiece(battle,0,TILEID_KING);
  struct piece *rook=genpiece(battle,0,TILEID_ROOK);
  eking->y=0;
  eking->x=rand()&7;
  mking->y=2;
  mking->x=eking->x;
  rook->x=rand()%5;
  if (rook->x>=eking->x-1) rook->x+=3;
  rook->y=1+(rand()%7);
  chess_transform_board(battle);
}

static void init1_priestandarabbi(struct battle *battle) {
  // Enemy King is cornered and held by our King from below, with a Bishop guarding the adjacent horizontal. Second Bishop ready to move to x==y.
  struct piece *eking=genpiece(battle,1,TILEID_KING);
  struct piece *mking=genpiece(battle,0,TILEID_KING);
  struct piece *still=genpiece(battle,0,TILEID_BISHOP);
  struct piece *agent=genpiece(battle,0,TILEID_BISHOP);
  eking->x=0;
  eking->y=0;
  mking->x=0;
  mking->y=2;
  still->x=2+rand()%6;
  still->y=still->x-1;
  // Agent can go on any cell where (x!=y) and ((x&1)==(y&1)) and either (x>=2) or (y>=2). Except (0,2) which is our liege lord's domain.
  // There are 23 such cells.
  int choice=rand()%23;
  int y=0; for (;y<8;y++) {
    int x=0; for (;x<8;x++) {
      if (x==y) continue; // Checks the enemy King.
      if ((x==0)&&(y==2)) continue; // Occupied by our King already.
      if ((x&1)!=(y&1)) continue; // Those are the priest's squares; I'm the rabbi.
      //if ((x<2)&&(y<2)) continue; // Don't actually need to check this; the only two such spaces are also (x==y).
      if (choice--) continue; // This space is good but I want a different one.
      agent->x=x;
      agent->y=y;
      chess_transform_board(battle);
      return;
    }
  }
  // Oops. Well whatever, put agent in a known legal position.
  agent->x=5;
  agent->y=3;
  chess_transform_board(battle);
}

static void init1_kingbishopknight(struct battle *battle) {
  // Enemy King in the corner, held in by my King. A Knight by my King's side finishes the box. Bishop is floating free and will clinch it.
  struct piece *eking=genpiece(battle,1,TILEID_KING);
  struct piece *mking=genpiece(battle,0,TILEID_KING);
  struct piece *knight=genpiece(battle,0,TILEID_KNIGHT);
  struct piece *bishop=genpiece(battle,0,TILEID_BISHOP);
  eking->x=0;
  eking->y=0;
  int occludable=0;
  switch (rand()%3) { // Three equally valid arrangements for my King and Knight.
    case 0: {
        mking->x=1;
        mking->y=2;
        knight->x=0;
        knight->y=2;
      } break;
    case 1: {
        mking->x=1;
        mking->y=2;
        knight->x=3;
        knight->y=1;
        occludable=1;
      } break;
    default: {
        mking->x=0;
        mking->y=2;
        knight->x=3;
        knight->y=1;
        occludable=1;
      } break;
  }
  // The Bishop goes on any ((x&1)==(y&1)) cell, but not (x==y), not (0,2) if it's occupied, and not (4,0) if our Knight is at (3,1).
  for (;;) {
    bishop->x=rand()&7;
    bishop->y=(rand()&6)|(bishop->x&1);
    if (bishop->x==bishop->y) continue;
    if ((bishop->x==knight->x)&&(bishop->y==knight->y)) continue;
    if ((bishop->x==mking->x)&&(bishop->y==mking->y)) continue;
    if (occludable&&(bishop->x==4)&&(bishop->y==0)) continue;
    break;
  }
  chess_transform_board(battle);
}

static void init1_arabian(struct battle *battle) {
  // Enemy King is cornered. Knight watches his cardinal neighbors, and Rook is ready to enter one of those cardinal neighbors.
  struct piece *eking=genpiece(battle,1,TILEID_KING);
  struct piece *mking=genpiece(battle,0,TILEID_KING);
  struct piece *knight=genpiece(battle,0,TILEID_KNIGHT);
  struct piece *rook=genpiece(battle,0,TILEID_ROOK);
  eking->x=0;
  eking->y=0;
  mking->x=3+rand()%5; // Anywhere out of the way.
  mking->y=rand()&7;
  knight->x=2;
  knight->y=2;
  rook->x=1;
  rook->y=1+(rand()%7);
  chess_transform_board(battle);
}

static void init1_foolsmate(struct battle *battle) {
  // An extremely specific 2-moves-in setup.
  chess_init_clean(battle);
  struct piece *piece=BATTLE->piecev+BATTLE->piecec-1;
  int i=BATTLE->piecec;
  for (;i-->0;piece--) {
    if (piece->tileid!=TILEID_PAWN) continue; // Only pawns will move.
    if (piece->x<=2) { // The left 3 columns are completely irrelevant, so shake them up. Just the pawns.
      int d=rand()%3;
      if (d) {
        if (piece->y==1) piece->y+=d;
        else if (piece->y==6) piece->y-=d;
      }
    } else if (piece->color==BATTLE->playerv[0].color) {
      if ((piece->x==4)&&(piece->y==6)) { // Important pawn.
        piece->y=5;
      } else if ((piece->x==3)||(piece->x==7)) { // Two of my other pawns can move 0,1,2 forward.
        piece->y-=rand()%3;
      } else if (piece->x==5) { // 2 or 0 for this guy.
        if (rand()&1) piece->y-=2;
      } else if (piece->x==6) { // 1 or 0 for this guy.
        if (rand()&1) piece->y-=1;
      }
    } else {
      if ((piece->x==5)&&(piece->y==1)) {
        piece->y=2;
      } else if ((piece->x==6)&&(piece->y==1)) {
        piece->y=3;
      } else if (piece->x==7) { // Can advance 1, decoratively.
        if (rand()&1) piece->y++;
      }
    }
  }
}

static void init1_scholarsmate(struct battle *battle) {
  // Similar to foolsmate, but a couple more steps.
  // Our side moves the King's pawn, the Queen, and the King's Bishop.
  // Their side must move the King's pawn by two. Plus two dummy moves to sync up.
  chess_init_clean(battle);
  struct piece *piece;
  if (piece=chess_piece_at_cell(battle,4,6)) { // My King's Pawn.
    piece->y=(rand()&1)?4:5;
  }
  if (piece=chess_piece_at_cell(battle,3,7)) { // My Queen.
    piece->x=7;
    piece->y=3;
  }
  if (piece=chess_piece_at_cell(battle,5,7)) { // My King's Bishop.
    piece->x=2;
    piece->y=4;
  }
  if (piece=chess_piece_at_cell(battle,4,1)) { // Enemy King's Pawn.
    piece->y=3;
  }
  // Then the two garbage moves. Let's say Queen's Knight and the Pawn in column 0..2 (advacing double, so it can't conflict with the Knight).
  if (piece=chess_piece_at_cell(battle,1,0)) { // Enemy Queen's Knight.
    piece->x=(rand()&1)?0:2;
    piece->y=2;
  }
  if (piece=chess_piece_at_cell(battle,rand()%3,1)) {
    piece->y=3;
  }
}

static void init1_legalsmate(struct battle *battle) {
  // Another opening gambit, and just a bit more involved than scholarsmate.
  chess_init_clean(battle);
  struct piece *piece;
  chess_remove_piece_at_cell(battle,3,7); // Our Queen is lost.
  chess_remove_piece_at_cell(battle,4,1); // Enemy King's Pawn is lost.
  chess_remove_piece_at_cell(battle,5,1); // and his little buddy too.
  if (piece=chess_piece_at_cell(battle,1,0)) { // Enemy Queen's Knight forward either way. I think it's a dummy move.
    piece->x=(rand()&1)?0:2;
    piece->y=2;
  }
  if (piece=chess_piece_at_cell(battle,2,0)) { // Enemy Queen's Bishop has taken our Queen.
    piece->x=3;
    piece->y=7;
  }
  if (piece=chess_piece_at_cell(battle,4,0)) { // Enemy King has advanced by one.
    piece->y=1;
  }
  if (piece=chess_piece_at_cell(battle,3,1)) { // Enemy Queen's Pawn has advanced by one -- how their assassin Bishop got loose.
    piece->y=2;
  }
  if (piece=chess_piece_at_cell(battle,4,6)) { // Our King's Pawn has advanced by two.
    piece->y=4;
  }
  if (piece=chess_piece_at_cell(battle,1,7)) { // Our Queen's Knight advanced to the right. This is the one you need to check with.
    piece->x=2;
    piece->y=5;
  }
  if (piece=chess_piece_at_cell(battle,6,7)) { // Our King's Knight has advanced twice, to be in front of the Enemy King.
    piece->x=4;
    piece->y=3;
  }
  if (piece=chess_piece_at_cell(battle,5,7)) { // Our King's Bishop has had a long merry adventure.
    piece->x=5;
    piece->y=1;
  }
}

static void init1_backrank(struct battle *battle) {
  // Enemy King is anywhere in row zero, with 2 or 3 Pawns in front. We have a Rook or Queen that can reach that far row.
  struct piece *eking=genpiece(battle,1,TILEID_KING);
  struct piece *mking=genpiece(battle,0,TILEID_KING);
  struct piece *agent=genpiece(battle,0,TILEID_ROOK);
  
  // Enemy King goes wherever he wants in the back row. Then cover his front with Pawns.
  eking->x=rand()&7;
  eking->y=0;
  struct piece *pawn;
  if (pawn=genpiece(battle,1,TILEID_PAWN)) {
    pawn->x=eking->x;
    pawn->y=1;
  }
  if ((eking->x>0)&&(pawn=genpiece(battle,1,TILEID_PAWN))) {
    pawn->x=eking->x-1;
    pawn->y=1;
  }
  if ((eking->x<7)&&(pawn=genpiece(battle,1,TILEID_PAWN))) {
    pawn->x=eking->x+1;
    pawn->y=1;
  }
  
  // Our King goes in the 2 or 3 columns shaded by those Pawns, in row 3 or greater. Just keeps him out of the way.
  int xlo=eking->x,xhi=eking->x;
  if (xlo>0) xlo--;
  if (xhi<7) xhi++;
  mking->x=xlo+rand()%(xhi-xlo+1);
  mking->y=3+rand()%5;
  
  // Our agent goes in any of the 5 or 6 non-pawned columns, at row 2 or greater.
  // Flip a coin to turn it into a Queen. We don't require her diagonals, so just think of it as decorative.
  if (xhi==1) agent->x=2+rand()%6;
  else if (xlo==6) agent->x=rand()%6;
  else {
    agent->x=rand()%5;
    if (agent->x>=xlo) agent->x+=3;
  }
  agent->y=2+rand()%6;
  if (rand()&1) agent->tileid=TILEID_QUEEN;
  
  chess_transform_board(battle);
}

/*XXX Chess.com lists a bunch more checkmate scenarios, but I'm kind of running out of steam.

static void init1_smother(struct battle *battle) {
  fprintf(stderr,"%s\n",__func__);//TODO
  init1_rookroll(battle);
}

static void init1_anastasia(struct battle *battle) {
  fprintf(stderr,"%s\n",__func__);//TODO
  init1_rookroll(battle);
}

static void init1_epaulette(struct battle *battle) {
  fprintf(stderr,"%s\n",__func__);//TODO
  init1_rookroll(battle);
}

static void init1_boden(struct battle *battle) {
  fprintf(stderr,"%s\n",__func__);//TODO
  init1_rookroll(battle);
}

static void init1_dovetail(struct battle *battle) {
  fprintf(stderr,"%s\n",__func__);//TODO
  init1_rookroll(battle);
}

static void init1_swallowtail(struct battle *battle) {
  fprintf(stderr,"%s\n",__func__);//TODO
  init1_rookroll(battle);
}

static void init1_opera(struct battle *battle) {
  fprintf(stderr,"%s\n",__func__);//TODO
  init1_rookroll(battle);
}

static void init1_blackburn(struct battle *battle) {
  fprintf(stderr,"%s\n",__func__);//TODO
  init1_rookroll(battle);
}

static void init1_damiano(struct battle *battle) {
  fprintf(stderr,"%s\n",__func__);//TODO
  init1_rookroll(battle);
}

static void init1_morphy(struct battle *battle) {
  fprintf(stderr,"%s\n",__func__);//TODO
  init1_rookroll(battle);
}
/**/

/* Initialize with a one-move checkmate for player zero.
 */
 
static void chess_init_onemove(struct battle *battle,double difficulty) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  struct piece *piece;
  int i,initc,piecec_required,distractc;
  init1_fn initv[20];
  init1_fn init;
 _again_:;

  /* Select a canned scenario and fill in the critical pieces for it.
   * I'm endebted to Chess.com for their great list of checkmate scenarios: https://www.chess.com/terms/checkmate-chess
   * The pieces that get placed here are not negotiable. Record how many.
   */
  BATTLE->piecec=0;
  initc=0;
  initv[initc++]=init1_rookroll;
  initv[initc++]=init1_kingnqueen;
  initv[initc++]=init1_kingnrook;
  initv[initc++]=init1_priestandarabbi;
  initv[initc++]=init1_kingbishopknight;
  initv[initc++]=init1_arabian;
  initv[initc++]=init1_foolsmate;
  initv[initc++]=init1_scholarsmate;
  initv[initc++]=init1_legalsmate;
  initv[initc++]=init1_backrank;
  /* Others listed at Chess.com that I haven't got to.
  initv[initc++]=init1_smother;
  initv[initc++]=init1_anastasia;
  initv[initc++]=init1_epaulette;
  initv[initc++]=init1_boden;
  initv[initc++]=init1_dovetail;
  initv[initc++]=init1_swallowtail;
  initv[initc++]=init1_opera;
  initv[initc++]=init1_blackburn;
  initv[initc++]=init1_damiano;
  initv[initc++]=init1_morphy;
  */
  init=initv[rand()%initc];
  init(battle);
  piecec_required=BATTLE->piecec;
  
  // Fill in random distraction pieces until we run out or (difficulty) is satisfied.
  distractc=(int)((32-BATTLE->piecec)*difficulty);
  while (distractc-->0) {
    if (!chess_place_distraction(battle)) break;
  }
  
  /* Validate a few things, because I don't really trust myself.
   */
  {
    int lkingc=0,rkingc=0;
    for (piece=BATTLE->piecev,i=BATTLE->piecec;i-->0;piece++) {
      if ((piece->color!=l->color)&&(piece->color!=r->color)) {
        fprintf(stderr,"%s:ERROR: Tile 0x%02x at (%d,%d), invalid color 0x%08x (must be 0x%08x or 0x%08x)\n",__func__,piece->tileid,piece->x,piece->y,piece->color,l->color,r->color);
        goto _again_;
      }
      if (piece->tileid==TILEID_KING) {
        if (piece->color==l->color) lkingc++;
        else rkingc++;
      }
      if ((piece->x<0)||(piece->y<0)||(piece->x>=8)||(piece->y>=8)) {
        fprintf(stderr,"%s:ERROR: Tile 0x%02x color 0x%08x at invalid position (%d,%d)\n",__func__,piece->tileid,piece->color,piece->x,piece->y);
        goto _again_;
      }
    }
    if ((lkingc!=1)||(rkingc!=1)) {
      fprintf(stderr,"%s:ERROR: Both sides must have exactly one king. Ended up with %d and %d\n",__func__,lkingc,rkingc);
      goto _again_;
    }
  }
  
  /* Select the king for both sides. Just so it's universally consistent.
   * And while we're looping, check all the pawns.
   * Set pawns' (movec) nonzero if they're off their starting row.
   * Do this *before* eliminating the distractions: The pawns change does bear on gameplay.
   */
  for (piece=BATTLE->piecev,i=BATTLE->piecec;i-->0;piece++) {
    if (piece->tileid==TILEID_KING) {
      if (piece->color==l->color) {
        l->selx=piece->x;
        l->sely=piece->y;
      } else if (piece->color==r->color) {
        r->selx=piece->x;
        r->sely=piece->y;
      }
    } else if (piece->tileid==TILEID_PAWN) {
      if ((piece->color==l->color)&&(piece->y!=6)) piece->movec=1;
      else if ((piece->color==r->color)&&(piece->y!=1)) piece->movec=1;
    }
  }
  
  /* The distraction pieces are allowed to foil the checkmate, we don't bother checking along the way.
   * In a loop, check whether we're still one move away from player zero's checkmate.
   * If not, remove a random piece above (piecec_required), and try again.
   * If we breach (piecec_required), we fucked something up, so start over.
   */
  for (;;) {
    chess_remove_impudents(battle,piecec_required);
    if (chess_is_one_move_from_mate(battle)) break;
    if (BATTLE->piecec<=piecec_required) {
      //TODO This does happen. At least with 'arabian' and 'kingbishopknight'. Log the board before restarting.
      // (it always seems to recover after the retry). But really shouldn't happen at all.
      // ...fixed a few things and now I think we're cool? Not 100%.
      fprintf(stderr,"%s:WARNING: Scenario was not solvable even after removing all distraction pieces. Trying again.\n",__func__);
      log_board(battle);
      goto _again_;
    }
    int rmp=piecec_required+rand()%(BATTLE->piecec-piecec_required);
    piece=BATTLE->piecev+rmp;
    BATTLE->piecec--;
    memmove(piece,piece+1,sizeof(struct piece)*(BATTLE->piecec-rmp));
  }
}

/* New.
 */
 
static int _chess_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  /* Faces must be one of (monster,dot,princess) and can't be the same.
   * It's not just a visual thing; we use color to associate players with their pieces.
   */
  battle->args.lface=chess_valid_face(battle->args.lface);
  battle->args.rface=chess_valid_face(battle->args.rface);
  if (battle->args.lface==battle->args.rface) {
    if (battle->args.lface==NS_face_dot) battle->args.rface=NS_face_princess;
    else battle->args.rface=NS_face_dot;
  }
  
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  
  if (battle->args.lctl&&battle->args.rctl) {
    chess_init_clean(battle);
    BATTLE->use_clock=0;
  } else {
    srand(0x51c030e1); r->skill=1.0; // <-- This produces an incorrect board where it wants to use a pawn but the pawn could be captured by the enemy queen.
    // ...even if you do what it says, once running it correctly recognizes that it's not mate.
    fprintf(stderr,"%s 1-player, seed = 0x%08x\n",__func__,get_rand_seed());
    chess_init_onemove(battle,r->skill);
    BATTLE->use_clock=1;
    BATTLE->clock=CLOCK_WORST*(1.0-l->skill)+CLOCK_BEST*l->skill;
  }
  
  // Dot's turn initially.
  BATTLE->turn=0;
  
  return 0;
}

/* Activate selection.
 */
 
static void player_activate(struct battle *battle,struct player *player) {
  
  // If she's selecting a proposal, commit the move.
  if (BATTLE->selpiece) {
    const uint8_t *prop=BATTLE->propv;
    int i=BATTLE->propc;
    for (;i-->0;prop++) {
      int px=(*prop)>>4;
      int py=(*prop)&15;
      if ((px==player->selx)&&(py==player->sely)) {
        chess_move_piece(battle,BATTLE->selpiece,px,py);
        return;
      }
    }
  }
  
  // Find a piece at this cell and select it if it's one of ours and has moves.
  struct piece *piece=chess_piece_at_cell(battle,player->selx,player->sely);
  if (!piece||(piece->color!=player->color)) {
    bm_sound(RID_sound_reject);
    return;
  }
  BATTLE->propc=chess_propose_moves(BATTLE->propv,64,battle,piece,2);
  if (!BATTLE->propc) {
    BATTLE->selpiece=0;
    bm_sound(RID_sound_reject);
    return;
  }
  BATTLE->selpiece=piece;
  bm_sound(RID_sound_uiactivate);
}

/* Cancel.
 */
 
static void player_cancel(struct battle *battle,struct player *player) {
  if (BATTLE->propc) {
    BATTLE->propc=0;
    bm_sound(RID_sound_uicancel);
  } else {
    bm_sound(RID_sound_reject);
  }
}

/* Move player's cursor.
 */
 
static void player_move(struct battle *battle,struct player *player,int dx,int dy) {
  player->selx+=dx;
  if (player->selx<0) player->selx=7; else if (player->selx>=8) player->selx=0;
  player->sely+=dy;
  if (player->sely<0) player->sely=7; else if (player->sely>=8) player->sely=0;
  bm_sound(RID_sound_uimotion);
}

/* Update human control.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed) {
  int input=g.input[player->human];
  int pvinput=g.pvinput[player->human];
  if (input!=pvinput) {
    if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1,0);
    if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1,0);
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) player_move(battle,player,0,-1);
    if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) player_move(battle,player,0,1);
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) player_activate(battle,player);
    else if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) player_cancel(battle,player);
  }
}

/* Update CPU control.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  //TODO
}

/* Update.
 */
 
static void _chess_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if ((BATTLE->animclock-=elapsed)<=0.0) {
    BATTLE->animclock+=0.200;
    if (++(BATTLE->animframe)>=2) BATTLE->animframe=0;
  }
  
  if ((BATTLE->turn>=0)&&(BATTLE->turn<=1)) {
    struct player *player=BATTLE->playerv+BATTLE->turn;
    if (player->human) {
      player_update_man(battle,player,elapsed);
    } else {
      player_update_cpu(battle,player,elapsed);
    }
  }
  
  if (BATTLE->use_clock&&(battle->outcome<=-2)) {
    if ((BATTLE->clock-=elapsed)<=0.0) {
      battle->outcome=-1;
    }
  }
}

/* Render.
 */
 
static void _chess_render(struct battle *battle) {

  /* Background.
   * If the clock is in play, blink at the last 5 seconds.
   */
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x304060ff);
  int sec=0;
  if (BATTLE->use_clock&&(BATTLE->clock>0.0)) {
    int ms=(int)(BATTLE->clock*1000.0);
    sec=ms/1000+1;
    if (sec<1) sec=1;
    if (sec<=5) {
      ms%=1000;
      if (ms>750) {
        int alpha=ms-750;
        uint32_t rgb=(sec==1)?0xff000000:0xe0a00000;
        graf_fill_rect(&g.graf,0,0,FBW,FBH,rgb|alpha);
      }
    }
  }
  graf_set_image(&g.graf,RID_image_battle_labyrinth2);
  
  // The 8x8 board.
  int boardw=NS_sys_tilesize*8;
  int boardh=NS_sys_tilesize*8;
  int boardx=(FBW>>1)-(boardw>>1);
  int boardy=(FBH>>1)-(boardh>>1);
  int dstx0=boardx+(NS_sys_tilesize>>1);
  int dsty0=boardy+(NS_sys_tilesize>>1);
  int dsty=dsty0,dstx;
  int yi=8;
  for (;yi-->0;dsty+=NS_sys_tilesize) {
    dstx=dstx0;
    int xi=8;
    for (;xi-->0;dstx+=NS_sys_tilesize) {
      graf_tile(&g.graf,dstx,dsty,((xi&1)==(yi&1))?0x01:0x00,0);
    }
  }
  
  // A tasteful boarder.
  graf_tile(&g.graf,dstx0-NS_sys_tilesize,dsty0-NS_sys_tilesize,0x10,0);
  graf_tile(&g.graf,dstx0+boardw         ,dsty0-NS_sys_tilesize,0x12,0);
  graf_tile(&g.graf,dstx0-NS_sys_tilesize,dsty0+boardh         ,0x30,0);
  graf_tile(&g.graf,dstx0+boardw         ,dsty0+boardh         ,0x32,0);
  for (yi=8,dstx=dstx0,dsty=dsty0;yi-->0;dstx+=NS_sys_tilesize,dsty+=NS_sys_tilesize) {
    graf_tile(&g.graf,dstx,dsty0-NS_sys_tilesize,0x11,0);
    graf_tile(&g.graf,dstx,dsty0+boardh         ,0x31,0);
    graf_tile(&g.graf,dstx0-NS_sys_tilesize,dsty,0x20,0);
    graf_tile(&g.graf,dstx0+boardw         ,dsty,0x22,0);
  }
  
  // Pieces.
  const struct piece *piece=BATTLE->piecev;
  int i=BATTLE->piecec;
  for (;i-->0;piece++) {
    graf_fancy(&g.graf,dstx0+piece->x*NS_sys_tilesize,dsty0+piece->y*NS_sys_tilesize,piece->tileid,piece->xform,0,NS_sys_tilesize,0,piece->color);
  }
  
  // Cursor.
  if ((BATTLE->turn>=0)&&(BATTLE->turn<=1)&&(battle->outcome<-1)) {
    struct player *player=BATTLE->playerv+BATTLE->turn;
    graf_fancy(&g.graf,
      dstx0+player->selx*NS_sys_tilesize,dsty0+player->sely*NS_sys_tilesize,
      0x08+BATTLE->animframe,0,
      0,NS_sys_tilesize,0,player->color
    );
  }
  
  // Proposals. Should go before cursor, but they won't overlap, so let cursor join the pieces' batch.
  const uint8_t *prop=BATTLE->propv;
  for (i=BATTLE->propc;i-->0;prop++) {
    dstx=((*prop)>>4)*NS_sys_tilesize+dstx0;
    dsty=((*prop)&15)*NS_sys_tilesize+dsty0;
    graf_tile(&g.graf,dstx,dsty,0x0a,0);
  }
  
  // Clock, optionally.
  if (sec>0) {
    int cx=(FBW>>1)-4;
    int cy=dsty0-20;
    graf_set_image(&g.graf,RID_image_fonttiles);
    if (sec>=10) graf_tile(&g.graf,cx,cy,'0'+sec/10,0); cx+=8;
    graf_tile(&g.graf,cx,cy,'0'+sec%10,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_chess={
  .name="chess",
  .objlen=sizeof(struct battle_chess),
  .strix_name=180,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=0,
  .no_timeout=1, // For 2-player mode. Don't expect them to finish in 60 seconds :D
  .del=_chess_del,
  .init=_chess_init,
  .update=_chess_update,
  .render=_chess_render,
};
#endif

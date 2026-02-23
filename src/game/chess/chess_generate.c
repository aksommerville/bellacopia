/* chess_generate.c
 * Bits and bobs for generating a fresh board.
 */
 
#include "chess_internal.h"
 
/* New clean board.
 */
 
static void chess_populate_back_row(uint8_t *board,int y,uint8_t color) {
  board+=y*8;
  *board++=color|PIECE_ROOK;
  *board++=color|PIECE_KNIGHT;
  *board++=color|PIECE_BISHOP;
  *board++=color|PIECE_QUEEN;
  *board++=color|PIECE_KING;
  *board++=color|PIECE_BISHOP;
  *board++=color|PIECE_KNIGHT;
  *board++=color|PIECE_ROOK;
}

static void chess_fill_row(uint8_t *board,int y,uint8_t piece) {
  board+=y*8;
  memset(board,piece,8);
}
 
void chess_generate_clean(uint8_t *board) {
  chess_populate_back_row(board,0,PIECE_BLACK);
  chess_fill_row(board,1,PIECE_BLACK|PIECE_PAWN);
  chess_fill_row(board,2,0);
  chess_fill_row(board,3,0);
  chess_fill_row(board,4,0);
  chess_fill_row(board,5,0);
  chess_fill_row(board,6,PIECE_WHITE|PIECE_PAWN);
  chess_populate_back_row(board,7,PIECE_WHITE);
}

/* Generate one-move scenario.
 */

void chess_generate_onemove(uint8_t *board,double difficulty) {
  memset(board,0,64);
  chess_generate_fn generate=chess_choose_generator(difficulty);
  if (!generate) return;
  generate(board);
  chess_board_transform_in_place(board);
  int piecec=chess_count_pieces(board);
  int requirec=0;
  if (piecec<32) {
    requirec=piecec+(int)(difficulty*(32-piecec));
  }
  chess_board_generate_distractions_carefully(board,requirec-piecec);
}

/* Transform, explicitly.
 */

void chess_board_transform(uint8_t *dst,const uint8_t *src,uint8_t xform) {
  int srcp=0,srcdminor=1,srcdmajor=8;
  if (xform&EGG_XFORM_XREV) {
    srcp+=7;
    srcdminor=-1;
  }
  if (xform&EGG_XFORM_YREV) {
    srcp+=56;
    srcdmajor=-8;
  }
  if (xform&EGG_XFORM_SWAP) {
    int tmp=srcdminor;
    srcdminor=srcdmajor;
    srcdmajor=tmp;
  }
  int yi=8;
  for (;yi-->0;srcp+=srcdmajor) {
    int xi=8;
    const uint8_t *srcminor=src+srcp;
    for (;xi-->0;srcminor+=srcdminor,dst++) *dst=*srcminor;
  }
}

/* Broker a transform.
 */
 
void chess_board_transform_in_place(uint8_t *board) {
  int have_pawn=0,have_anything=0,i=64;
  const uint8_t *p=board;
  for (;i-->0;p++) {
    int role=(*p)&PIECE_ROLE_MASK;
    if (role==PIECE_PAWN) have_pawn=have_anything=1;
    else if (role) have_anything=1;
  }
  if (!have_anything) return;
  uint8_t xform=rand()&(have_pawn?EGG_XFORM_XREV:7);
  if (!xform) return;
  uint8_t tmp[64];
  chess_board_transform(tmp,board,xform);
  memcpy(board,tmp,64);
}

/* Generate distractions but try not to break the one-move-to-mate-ness of it.
 */

int chess_board_generate_distractions_carefully(uint8_t *board,int addc) {

  // If they ask for an upper limit less than one, cool, done.
  if (addc<1) return 0;

  // If we're not one move from mate to begin with, nothing is going to work.
  if (!chess_one_move_from_mate(board)) {
    fprintf(stderr,"%s: Initial board is not one move from mate.\n",__func__);
    return 0;
  }
  
  /* Scan the board to generate a list of vacant cells, and a list of present pieces.
   */
  uint8_t vacantv[64]; // 0..63, index in (board)
  int vacantc=0;
  uint8_t bpiececv[8]={0},wpiececv[8]={0}; // Indexed by PIECE_*, ie the low 3 bits of a cell value.
  const uint8_t *boardp=board;
  int i=0;
  for (;i<64;i++,boardp++) {
    if (*boardp) {
      if ((*boardp)&PIECE_WHITE) {
        wpiececv[(*boardp)&7]++;
      } else {
        bpiececv[(*boardp)&7]++;
      }
    } else {
      vacantv[vacantc++]=i;
    }
  }
  
  /* Enumerate the missing pieces.
   * We could also take the opportunity to bail out if a king is missing, but chess_one_move_from_mate() should have noticed that already.
   */
  uint8_t missingv[32];
  int missingc=0,n;
  if ((n=8-bpiececv[PIECE_PAWN  ])>0) { while (n-->0) missingv[missingc++]=PIECE_BLACK|PIECE_PAWN; }
  if ((n=2-bpiececv[PIECE_ROOK  ])>0) { while (n-->0) missingv[missingc++]=PIECE_BLACK|PIECE_ROOK; }
  if ((n=2-bpiececv[PIECE_KNIGHT])>0) { while (n-->0) missingv[missingc++]=PIECE_BLACK|PIECE_KNIGHT; }
  if ((n=2-bpiececv[PIECE_BISHOP])>0) { while (n-->0) missingv[missingc++]=PIECE_BLACK|PIECE_BISHOP; }
  if (!bpiececv[PIECE_QUEEN]) missingv[missingc++]=PIECE_BLACK|PIECE_QUEEN;
  if ((n=8-wpiececv[PIECE_PAWN  ])>0) { while (n-->0) missingv[missingc++]=PIECE_WHITE|PIECE_PAWN; }
  if ((n=2-wpiececv[PIECE_ROOK  ])>0) { while (n-->0) missingv[missingc++]=PIECE_WHITE|PIECE_ROOK; }
  if ((n=2-wpiececv[PIECE_KNIGHT])>0) { while (n-->0) missingv[missingc++]=PIECE_WHITE|PIECE_KNIGHT; }
  if ((n=2-wpiececv[PIECE_BISHOP])>0) { while (n-->0) missingv[missingc++]=PIECE_WHITE|PIECE_BISHOP; }
  if (!wpiececv[PIECE_QUEEN]) missingv[missingc++]=PIECE_WHITE|PIECE_QUEEN;
  
  /* Add pieces until (addc) is satisfied, we run out, or we spin for too long.
   * We're checking (vacantc>0) just to be pedantic; in truth it can't reach zero (missingc would go zero first).
   */
  int actualc=0;
  int panic=1000; // Give up eventually. Our operation is such that we can stop at any time.
  int repc=0;
  while ((actualc<addc)&&(vacantc>0)&&(missingc>0)&&(panic-->0)) {
    repc++;
  
    // Select a vacancy and a piece at random.
    int vacantp=rand()%vacantc;
    int missingp=rand()%missingc;
    int x=vacantv[vacantp]&7;
    int y=vacantv[vacantp]>>3;
    uint8_t piece=missingv[missingp];
    
    // Double-check that it is in fact vacant. If not, we messed up badly somehow.
    if ((x>=8)||(y>=8)||board[y*8+x]) {
      fprintf(stderr,"%s:%d:PANIC: Cell (%d,%d) was expected to be vacant but is OOB or occupied.\n",__FILE__,__LINE__,x,y);
      return actualc;
    }
    
    // Don't put Pawns in the top or bottom row; they would never be there in real life.
    // There are other Pawn patterns that also couldn't happen, but we'll leave it at just this obvious one.
    if ((piece&PIECE_ROLE_MASK)==PIECE_PAWN) {
      if ((y==0)||(y==7)) continue;
    }
    
    // Put the piece there, then test whether we're still one move from mate.
    // If so, remove both vacancy and piece from their list, and count the addition.
    // If not, keep them both, revert the move, and try again. This combination might come up again... I'm not convinced it's a serious problem.
    board[y*8+x]=piece;
    if (chess_one_move_from_mate(board)) {
      vacantc--;
      memmove(vacantv+vacantp,vacantv+vacantp+1,vacantc-vacantp);
      missingc--;
      memmove(missingv+missingp,missingv+missingp+1,missingc-missingp);
      actualc++;
    } else {
      board[y*8+x]=0;
    }
  }
  //fprintf(stderr,"%s: Added %d pieces in %d reps.\n",__func__,actualc,repc);
  return actualc;
}

/* chess_model.c
 * The stripped-down model of a chess game, as stateless as possible.
 */

#include "chess_internal.h"

/* Validate and commit move.
 */
 
int chess_commit_move(uint8_t *board,uint16_t move) {
  int err=chess_validate_move(board,move);
  if (err<0) return err;
  int fromx=CHESS_MOVE_FROM_COL(move);
  int fromy=CHESS_MOVE_FROM_ROW(move);
  int tox=CHESS_MOVE_TO_COL(move);
  int toy=CHESS_MOVE_TO_ROW(move);
  int fromp=fromy*8+fromx;
  int top=toy*8+tox;
  board[top]=board[fromp];
  board[fromp]=0;
  return err;
}

/* Validate move.
 */
 
int chess_validate_move(const uint8_t *board,uint16_t move) {

  /* First check the simplest things.
   * Two valid points, "from" contains a piece, "to" is not on the same team.
   */
  if (!board) return -1;
  int fromx=CHESS_MOVE_FROM_COL(move);
  int fromy=CHESS_MOVE_FROM_ROW(move);
  int tox=CHESS_MOVE_TO_COL(move);
  int toy=CHESS_MOVE_TO_ROW(move);
  if ((fromx<0)||(fromy<0)||(fromx>=8)||(fromy>=8)) return -1; // Invalid cell.
  uint8_t piece=board[fromy*8+fromx];
  if (!piece) return -1; // Can't move an empty cell.
  uint8_t color=piece&PIECE_WHITE;
  uint8_t topiece=board[toy*8+tox];
  if (topiece&&((topiece&PIECE_WHITE)==color)) return -1; // Occupied by same team. (this also catches same-cell).
  
  /* Confirm that the delta is valid for the piece's role,
   * and that there's no pieces jumped along the way.
   * These tests are allowed to modify (dx,dy) along the way, so don't use them after.
   */
  int dx=tox-fromx;
  int dy=toy-fromy;
  uint8_t role=(piece&PIECE_ROLE_MASK);
  switch (role) {
  
    case PIECE_PAWN: {
        if (dx) { // (dx) may be -1 or 1, (dy) must be forward*1, and there must be a (topiece).
          if ((dx<-1)||(dx>1)) return -1; // Diagonal yes, but not *that* diagonal.
          if (color) { // White moves up.
            if (dy!=-1) return -1;
          } else { // Black moves down.
            if (dy!=1) return -1;
          }
          if (!topiece) return -1; // Diagonal is only for capture.
        } else { // Capture forbidden, and (dy) is forward*1 or forward*2. *2 is only permitted from the starting row, and not allowed to jump anything.
          if (topiece) return -1; // Capture may only be diagonal.
          if (color) { // White moves up.
            if (dy==-2) {
              if (fromy!=6) return -1; // Double-advance only from the starting row.
              if (board[(fromy-1)*8+fromx]) return -1; // Can't double-advance over another piece.
            } else if (dy!=-1) return -1; // Wrong direction or too much of it.
          } else { // Black moves down.
            if (dy==2) {
              if (fromy!=1) return -1; // Double-advance only from the starting row.
              if (board[(fromy+1)*8+fromx]) return -1; // Can't double-advance over another piece.
            } else if (dy!=1) return -1; // Wrong direction or too much of it.
          }
        }
      } break;
      
    case PIECE_KNIGHT: {
        if (dx<0) dx=-dx;
        if (dy<0) dy=-dy;
        if ((dx==1)&&(dy==2)) ;
        else if ((dx==2)&&(dy==1)) ;
        else return -1; // (+-1,+-2) or (+-2,+-1), anything else is below a Knight's dignity.
        // Jumping is fine, no need to check the intervening cells.
      } break;
      
    case PIECE_KING: {
        if ((dx<-1)||(dy<-1)||(dx>1)||(dy>1)) return -1;
      } break;
      
    case PIECE_ROOK:
    case PIECE_BISHOP:
    case PIECE_QUEEN: {
        // Rook must be cardinal, Bishop diagonal, and Queen either.
        if (role==PIECE_ROOK) {
          if (dx&&dy) return -1;
        } else if (role==PIECE_BISHOP) {
          int adx=dx; if (adx<0) adx=-adx;
          int ady=dy; if (ady<0) ady=-ady;
          if (adx!=ady) return -1;
        } else {
          if (!dx||!dy) ; // Cardinal, cool.
          else if (dx==dy) ; // Diagonal positive slope, cool.
          else if (dx==-dy) ; // Diagonal negative slope, cool.
          else return -1; // Uncool, lady. And I don't care who you are.
        }
        // Trace the deltas back to zero and if we strike any piece, it's invalid.
        for (;;) {
          if (dx<0) dx++; else if (dx>0) dx--;
          if (dy<0) dy++; else if (dy>0) dy--;
          if (!dx&&!dy) break;
          if (board[(fromy+dy)*8+fromx+dx]) return -1; // No jumping!
        }
      } break;
      
    default: return -1; // No such role.
  }
  
  /* If the moving piece's King is in check after this move, it's invalid.
   * Perform the move against a copy of the board and assess.
   */
  uint8_t next[64];
  memcpy(next,board,64);
  next[toy*8+tox]=next[fromy*8+fromx];
  next[fromy*8+fromx]=0;
  int kingx,kingy;
  if (chess_find_piece(&kingx,&kingy,next,color|PIECE_KING)<0) return -1;
  if (chess_is_check(next,kingx,kingy)) return -1;
  
  /* It's valid. Return 0 or 1, according to (topiece).
   */
  return (topiece?1:0);
}

/* Helpers for a sorted list of pieces.
 */
 
int chesspiecev_search(struct chesspiece *v,int c,uint8_t piece) {
  int lo=0,hi=c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    uint8_t q=v[ck].piece;
         if (piece<q) hi=ck;
    else if (piece>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct chesspiece *chesspiece_get(struct chesspiece *v,int c,uint8_t piece) {
  int p=chesspiecev_search(v,c,piece);
  if (p<0) return 0;
  return v+p;
}

/* Assess board.
 */

int chess_assess(const uint8_t *board) {
  if (!board) return 0;
  
  /* Scan the board and record all the pieces.
   * Fail INVALID if there's more than 32.
   * Also count by color and role.
   * If there's any unknown role, fail.
   */
  struct chesshist { int pawnc,rookc,knightc,bishopc,queenc,kingc; } whist={0},bhist={0};
  struct chesspiece chesspiecev[32];
  int chesspiecec=0,y=0;
  const uint8_t *boardp=board;
  for (;y<8;y++) {
    int x=0; for (;x<8;x++,boardp++) {
      if (!*boardp) continue;
      if (chesspiecec>=32) return 0; // Too many pieces on board.
      int p=chesspiecev_search(chesspiecev,chesspiecec,*boardp);
      if (p<0) p=-p-1;
      memmove(chesspiecev+p+1,chesspiecev+p,sizeof(struct chesspiece)*(chesspiecec-p));
      chesspiecec++;
      chesspiecev[p]=(struct chesspiece){*boardp,x,y};
      if ((*boardp)&PIECE_WHITE) switch ((*boardp)&PIECE_ROLE_MASK) {
        case PIECE_PAWN: whist.pawnc++; break;
        case PIECE_ROOK: whist.rookc++; break;
        case PIECE_KNIGHT: whist.knightc++; break;
        case PIECE_BISHOP: whist.bishopc++; break;
        case PIECE_QUEEN: whist.queenc++; break;
        case PIECE_KING: whist.kingc++; break;
      } else switch ((*boardp)&PIECE_ROLE_MASK) {
        case PIECE_PAWN: bhist.pawnc++; break;
        case PIECE_ROOK: bhist.rookc++; break;
        case PIECE_KNIGHT: bhist.knightc++; break;
        case PIECE_BISHOP: bhist.bishopc++; break;
        case PIECE_QUEEN: bhist.queenc++; break;
        case PIECE_KING: bhist.kingc++; break;
      }
    }
  }
  
  /* Check each histogram and fail if there's too many of any piece.
   * Queens are exempt due to promotion.
   */
  if (whist.pawnc>8) return 0;
  if (whist.rookc>2) return 0;
  if (whist.knightc>2) return 0;
  if (whist.bishopc>2) return 0;
  if (whist.kingc>1) return 0;
  if (bhist.pawnc>8) return 0;
  if (bhist.rookc>2) return 0;
  if (bhist.knightc>2) return 0;
  if (bhist.bishopc>2) return 0;
  if (bhist.kingc>1) return 0;
  
  /* At least one King must exist.
   * For tax purposes, an absent King counts as In Check.
   * If both Kings are in check, report invalid.
   */
  int wcheck=1,bcheck=1; // Assume check if we can't find a king. (ie assume he was just captured, in a forward-testing case).
  struct chesspiece *king;
  if (king=chesspiece_get(chesspiecev,chesspiecec,PIECE_BLACK|PIECE_KING)) {
    if (!chess_is_check(board,king->x,king->y)) bcheck=0;
  }
  if (king=chesspiece_get(chesspiecev,chesspiecec,PIECE_WHITE|PIECE_KING)) {
    if (!chess_is_check(board,king->x,king->y)) wcheck=0;
  }
  if (wcheck&&bcheck) return 0; // Can't both be in check. (also means at least one must exist)
  
  /* Start composing the assessment.
   */
  int assess=CHESS_ASSESS_VALID;
  if (wcheck) assess|=CHESS_ASSESS_CHECK|CHESS_ASSESS_WHO;
  else if (bcheck) assess|=CHESS_ASSESS_CHECK;
  
  /* When CHECK, we know whose turn it is and we can examine only those moves that do not leave that team in check.
   */
  uint16_t movev[CHESS_MOVES_LIMIT_ALL];
  if (wcheck) {
    if (chess_list_team_moves(movev,CHESS_MOVES_LIMIT_ALL,board,PIECE_WHITE,1)) assess|=CHESS_ASSESS_RUNNING;
  } else if (bcheck) {
    if (chess_list_team_moves(movev,CHESS_MOVES_LIMIT_ALL,board,PIECE_BLACK,1)) assess|=CHESS_ASSESS_RUNNING;
    
  /* Not in check, so we don't know whose turn it is.
   * Call it RUNNING if either team has at least one move.
   */
  } else {
    if (chess_list_team_moves(movev,CHESS_MOVES_LIMIT_ALL,board,PIECE_WHITE,1)) assess|=CHESS_ASSESS_RUNNING;
    else if (chess_list_team_moves(movev,CHESS_MOVES_LIMIT_ALL,board,PIECE_BLACK,1)) assess|=CHESS_ASSESS_RUNNING;
  }
  
  return assess;
}

/* List moves from one cell.
 */

int chess_list_moves(uint16_t *dstv,int dsta,const uint8_t *board,int x,int y,int validate) {
  if (!dstv||(dsta<0)) dsta=0;
  if ((x<0)||(y<0)||(x>=8)||(y>=8)) return 0; // Pieces that left the board (foul balls? home runs?), are not permitted to return to it.
  int dstc=0;
  
  #define APPEND(tox,toy) { \
    if (dstc<dsta) dstv[dstc]=CHESS_MOVE_COMPOSE(tox,toy,x,y); \
    dstc++; \
  }
  #define VACANT_OR_OPPONENT(tox,toy) { \
    int _tox=(tox),_toy=(toy); \
    if ((_tox>=0)&&(_toy>=0)&&(_tox<8)&&(_toy<8)) { \
      uint8_t topiece=board[_toy*8+_tox]; \
      if (!topiece||((topiece&PIECE_WHITE)!=color)) { \
        APPEND(_tox,_toy) \
      } \
    } \
  }
  
  uint8_t piece=board[y*8+x];
  if (!piece) return 0; // A piece that doesn't exist can't move anywhere.
  uint8_t color=piece&PIECE_WHITE;
  uint8_t role=piece&PIECE_ROLE_MASK;
  switch (role) {
  
    case PIECE_PAWN: {
        int dy=color?-1:1; // White always starts at the bottom in our models.
        int y1=y+dy,y2=y+dy*2;
        if ((y1>=0)&&(y1<8)) { // Advance one cardinally or capture diagonally.
          if (!board[y1*8+x]) APPEND(x,y1)
          if ((x>0)&&board[y1*8+x-1]&&((board[y1*8+x-1]&PIECE_WHITE)!=color)) APPEND(x-1,y1)
          if ((x<7)&&board[y1*8+x+1]&&((board[y1*8+x+1]&PIECE_WHITE)!=color)) APPEND(x+1,y1)
        }
        int y0=color?6:1;
        if (y==y0) { // On starting row, allow to advance by two, if vacant.
          if (!board[y2*8+x]) APPEND(x,y2)
        }
      } break;
      
    case PIECE_KNIGHT: {
        VACANT_OR_OPPONENT(x-1,y-2)
        VACANT_OR_OPPONENT(x+1,y-2)
        VACANT_OR_OPPONENT(x-2,y-1)
        VACANT_OR_OPPONENT(x+2,y-1)
        VACANT_OR_OPPONENT(x-2,y+1)
        VACANT_OR_OPPONENT(x+2,y+1)
        VACANT_OR_OPPONENT(x-1,y+2)
        VACANT_OR_OPPONENT(x+1,y+2)
      } break;
      
    case PIECE_KING: {
        VACANT_OR_OPPONENT(x-1,y-1)
        VACANT_OR_OPPONENT(x  ,y-1)
        VACANT_OR_OPPONENT(x+1,y-1)
        VACANT_OR_OPPONENT(x-1,y  )
        VACANT_OR_OPPONENT(x+1,y  )
        VACANT_OR_OPPONENT(x-1,y+1)
        VACANT_OR_OPPONENT(x  ,y+1)
        VACANT_OR_OPPONENT(x+1,y+1)
      } break;
    
    // Rook, Bishop, and Queen are the same thing, just Rook and Bishop each ignore four of eight vectors.
    case PIECE_ROOK:
    case PIECE_BISHOP:
    case PIECE_QUEEN: {
        struct vector { int x,y,dx,dy,ok; } vectorv[8]={
          {x,y,-1,-1,role!=PIECE_ROOK},
          {x,y, 1,-1,role!=PIECE_ROOK},
          {x,y,-1, 1,role!=PIECE_ROOK},
          {x,y, 1, 1,role!=PIECE_ROOK},
          {x,y, 0,-1,role!=PIECE_BISHOP},
          {x,y,-1, 0,role!=PIECE_BISHOP},
          {x,y, 1, 0,role!=PIECE_BISHOP},
          {x,y, 0, 1,role!=PIECE_BISHOP},
        };
        for (;;) {
          struct vector *vector=vectorv;
          int i=8,any=0;
          for (;i-->0;vector++) {
            if (!vector->ok) continue;
            vector->x+=vector->dx;
            vector->y+=vector->dy;
            if ((vector->x<0)||(vector->y<0)||(vector->x>=8)||(vector->y>=8)) {
              vector->ok=0;
              continue;
            }
            any=1;
            uint8_t target=board[vector->y*8+vector->x];
            if (!target) APPEND(vector->x,vector->y) // Vacant. Add it and carry on.
            else if ((target&PIECE_WHITE)==color) vector->ok=0; // Teammate. Ignore it and drop the vector.
            else { vector->ok=0; APPEND(vector->x,vector->y) } // Opponent. Add it and drop the vector.
          }
          if (!any) break;
        }
      } break;
  }
  #undef APPEND
  #undef VACANT_OR_OPPONENT
  
  // If they didn't request validation, we're done.
  if (!validate) return dstc;
  
  /* Painstakingly validate each proposal and remove those that fail.
   * If we've overfilled the list, discard the excess immediately.
   */
  if (dstc>dsta) dstc=dsta;
  int i=dstc;
  while (i-->0) {
    uint16_t move=dstv[i];
    if (chess_validate_move(board,move)>=0) continue;
    dstc--;
    memmove(dstv+i,dstv+i+1,sizeof(uint16_t)*(dstc-i));
  }
  
  return dstc;
}

/* List moves for an entire team.
 */

int chess_list_team_moves(uint16_t *dstv,int dsta,const uint8_t *board,int team,int validate) {
  int dstc=0;
  uint8_t match=team?PIECE_WHITE:PIECE_BLACK;
  const uint8_t *boardp=board;
  int y=0; for (;y<8;y++) {
    int x=0; for (;x<8;x++,boardp++) {
      if (!*boardp) continue; // Vacant.
      if (((*boardp)&PIECE_WHITE)!=match) continue; // Wrong team.
      int addc=chess_list_moves(dstv+dstc,dsta-dstc,board,x,y,validate);
      if (addc<1) continue;
      dstc+=addc;
    }
  }
  return dstc;
}

/* List pieces checking their opponent King.
 */

int chess_list_checkers(uint16_t *dstv,int dsta,const uint8_t *board,int attacking_team,int validate) {

  // Find the opponent King. If he's missing, our answer is zero.
  int targetx,targety;
  if (chess_find_piece(&targetx,&targety,board,PIECE_KING|(attacking_team?PIECE_BLACK:PIECE_WHITE))<0) return 0;
  uint16_t match=CHESS_MOVE_COMPOSE(targetx,targety,0,0);
  uint16_t mask=CHESS_MOVE_COMPOSE(15,15,0,0);
  
  // Get all of our moves, and filter to those landing on the opponent King.
  int dstc=0;
  uint16_t movev[CHESS_MOVES_LIMIT_ALL];
  int movec=chess_list_team_moves(movev,CHESS_MOVES_LIMIT_ALL,board,attacking_team,validate);
  const uint16_t *move=movev;
  for (;movec-->0;move++) {
    if (((*move)&mask)==match) {
      if (dstc<dsta) dstv[dstc]=*move;
      dstc++;
    }
  }
  return dstc;
}

/* Nonzero if the king at this position is in check.
 */
 
int chess_is_check(const uint8_t *board,int kingx,int kingy) {

  // Must be a valid position with a piece on it.
  // We don't need to care whether it really is a King.
  if (!board||(kingx<0)||(kingy<0)||(kingx>=8)||(kingy>=8)) return 0;
  uint8_t kingpiece=board[kingy*8+kingx];
  if (!kingpiece) return 0;
  //if ((kingpiece&PIECE_ROLE_MASK)!=PIECE_KING) return 0;
  uint8_t attackcolor=(kingpiece&PIECE_WHITE)?PIECE_BLACK:PIECE_WHITE;
  
  #define IFKNIGHT(dx,dy) { \
    int x=kingx+(dx),y=kingy+(dy); \
    if ((x>=0)&&(y>=0)&&(x<8)&&(y<8)) { \
      uint8_t knight=board[y*8+x]; \
      if ((knight&PIECE_ROLE_MASK)==PIECE_KNIGHT) { \
        if ((knight&PIECE_WHITE)==attackcolor) return 1; \
      } \
    } \
  }
  IFKNIGHT(-1,-2)
  IFKNIGHT( 1,-2)
  IFKNIGHT(-2,-1)
  IFKNIGHT( 2,-1)
  IFKNIGHT(-2, 1)
  IFKNIGHT( 2, 1)
  IFKNIGHT(-1, 2)
  IFKNIGHT( 1, 2)
  #undef IFKNIGHT
  
  #define IFPAWN(dx,dy) { \
    int x=kingx+(dx),y=kingy+(dy); \
    if ((x>=0)&&(y>=0)&&(x<8)&&(y<8)) { \
      uint8_t pawn=board[y*8+x]; \
      if ((pawn&PIECE_ROLE_MASK)==PIECE_PAWN) { \
        if ((pawn&PIECE_WHITE)==attackcolor) return 1; \
      } \
    } \
  }
  if (kingpiece&PIECE_WHITE) { // Attacking pawns moving downard.
    IFPAWN(-1,-1)
    IFPAWN( 1,-1)
  } else { // Attacking pawns moving upward.
    IFPAWN(-1, 1)
    IFPAWN( 1, 1)
  }
  #undef IFPAWN
  
  /* We do look for the opponent King.
   * We are used for assessing future moves, and tho Kings can't be adjacent to each other, we might be assessing a future where they are.
   * Meaning, this is the bit that prevents them from getting adjacent.
   */
  #define IFKING(dx,dy) { \
    int x=kingx+(dx),y=kingy+(dy); \
    if ((x>=0)&&(y>=0)&&(x<8)&&(y<8)) { \
      uint8_t other=board[y*8+x]; \
      if ((other&PIECE_ROLE_MASK)==PIECE_KING) { \
        if ((other&PIECE_WHITE)==attackcolor) return 1; \
      } \
    } \
  }
  IFKING(-1,-1)
  IFKING( 0,-1)
  IFKING( 1,-1)
  IFKING(-1, 0)
  IFKING( 1, 0)
  IFKING(-1, 1)
  IFKING( 0, 1)
  IFKING( 1, 1)
  #undef IFKING
  
  /* Now radiate outward along 8 vectors.
   * Stop each vector separately when it touches something.
   * If that something is an opponent Queen, we're in check.
   * If it's an opponent Rook or Bishop, some vectors do call it a check.
   */
  struct vector { int x,y,dx,dy,ok,role; } vectorv[8]={
    {kingx,kingy,-1,-1,1,PIECE_BISHOP},
    {kingx,kingy, 0,-1,1,PIECE_ROOK},
    {kingx,kingy, 1,-1,1,PIECE_BISHOP},
    {kingx,kingy,-1, 0,1,PIECE_ROOK},
    {kingx,kingy, 1, 0,1,PIECE_ROOK},
    {kingx,kingy,-1, 1,1,PIECE_BISHOP},
    {kingx,kingy, 0, 1,1,PIECE_ROOK},
    {kingx,kingy, 1, 1,1,PIECE_BISHOP},
  };
  for (;;) {
    struct vector *vector=vectorv;
    int i=8,done=1;
    for (;i-->0;vector++) {
      if (!vector->ok) continue;
      vector->x+=vector->dx;
      vector->y+=vector->dy;
      if ((vector->x<0)||(vector->y<0)||(vector->x>=8)||(vector->y>=8)) {
        vector->ok=0;
        continue;
      }
      done=0;
      uint8_t other=board[vector->y*8+vector->x];
      if (!other) continue;
      if ((other&PIECE_WHITE)!=attackcolor) { // My loyal servant is in the way.
        vector->ok=0;
        continue;
      }
      uint8_t orole=other&PIECE_ROLE_MASK;
      if (orole==PIECE_QUEEN) return 1;
      if (orole==vector->role) return 1;
      vector->ok=0; // Some other opponent in the way, drop the vector.
    }
    if (done) break;
  }
  
  // OK nope, not in check.
  return 0;
}

/* Test checkmate.
 * Caller should know the King's position already. If not, figure that out first.
 * You must provide a mutable board, and we will modify it, but we promise to restore it before returning.
 */
 
static int chess_is_mate(uint8_t *board,int kingx,int kingy) {

  // King must be in check to begin with.
  if (!chess_is_check(board,kingx,kingy)) return 0;
  
  // Try all of black's moves. If there's one after which black is not in check, the answer is No.
  // Don't ask for validation; we're validating ourselves.
  uint16_t movev[CHESS_MOVES_LIMIT_ALL];
  int movec=chess_list_team_moves(movev,CHESS_MOVES_LIMIT_ALL,board,0,0);
  //fprintf(stderr,"%s(%d,%d): Trying %d moves...\n",__func__,kingx,kingy,movec);
  uint16_t *move=movev;
  for (;movec-->0;move++) {
    int tox=CHESS_MOVE_TO_COL(*move);
    int toy=CHESS_MOVE_TO_ROW(*move);
    int fromx=CHESS_MOVE_FROM_COL(*move);
    int fromy=CHESS_MOVE_FROM_ROW(*move);
    uint8_t toprev=board[toy*8+tox];
    uint8_t fromprev=board[fromy*8+fromx];
    board[toy*8+tox]=fromprev;
    board[fromy*8+fromx]=0;
    int stillcheck;
    if ((fromx==kingx)&&(fromy==kingy)) stillcheck=chess_is_check(board,tox,toy);
    else stillcheck=chess_is_check(board,kingx,kingy);
    board[toy*8+tox]=toprev;
    board[fromy*8+fromx]=fromprev;
    //fprintf(stderr,"  ...0x%02x from (%d,%d) to (%d,%d): %s\n",fromprev,fromx,fromy,tox,toy,stillcheck?"Still check":"NOT CHECK");
    if (!stillcheck) return 0;
  }
  
  // Couldn't find a way out, that's mate.
  return 1;
}

/* One move from mate.
 */
 
int chess_one_move_from_mate(const uint8_t *board) {
  if (!board) return 0;
  
  // Locate the Black King. If there isn't one, he can't be in check, so No.
  int kingx,kingy;
  if (chess_find_piece(&kingx,&kingy,board,PIECE_BLACK|PIECE_KING)<0) return 0;
  
  // Black King must not currently be in check.
  // That's not really what this function is for, but we should test it, since it means the current board is impossible.
  // (the prior Black move must not have left Black in check).
  if (chess_is_check(board,kingx,kingy)) return 0;
  
  // We need the White King's position too. Doesn't matter whether he's in check right now, but he must not be after the move.
  // I'll grudgingly allow that we can proceed without a White King too.
  int wx=-1,wy=-1;
  chess_find_piece(&wx,&wy,board,PIECE_WHITE|PIECE_KING);
  
  // Take a copy of (board) that we can modify.
  uint8_t tmp[64];
  memcpy(tmp,board,64);
  
  /* List all white moves.
   * Don't bother with validation at this level: If it leaves the White King in check, we'll find out in due course.
   */
  uint16_t movev[CHESS_MOVES_LIMIT_ALL];
  int movec=chess_list_team_moves(movev,CHESS_MOVES_LIMIT_ALL,board,1,0);
  //fprintf(stderr,"%s: Considering %d White moves...\n",__func__,movec);
  
  /* Now the expensive part.
   * Examine every White move.
   * Discard any White move that leaves the White King in check.
   * Those that put the Black King in check, go further to determine whether it's mate.
   * When we find a checkmate, verify that it doesn't put the White King in check, then answer Yes.
   */
  uint16_t *move=movev;
  for (;movec-->0;move++) {
    int tox=CHESS_MOVE_TO_COL(*move);
    int toy=CHESS_MOVE_TO_ROW(*move);
    int fromx=CHESS_MOVE_FROM_COL(*move);
    int fromy=CHESS_MOVE_FROM_ROW(*move);
    uint8_t toprev=tmp[toy*8+tox];
    uint8_t fromprev=tmp[fromy*8+fromx];
    tmp[toy*8+tox]=fromprev;
    tmp[fromy*8+fromx]=0;
    
    // If the White King remains in check after this move, forget it.
    int whitecheck;
    if ((fromx==wx)&&(fromy==wy)) whitecheck=chess_is_check(tmp,tox,toy);
    else whitecheck=chess_is_check(tmp,wx,wy);
    if (whitecheck) {
      //fprintf(stderr,"%s: Disregarding (%d,%d)=>(%d,%d) because it leaves the White King in check.\n",__func__,fromx,fromy,tox,toy);
      tmp[toy*8+tox]=toprev;
      tmp[fromy*8+fromx]=fromprev;
      continue;
    }
    
    if (chess_is_mate(tmp,kingx,kingy)) {
      //fprintf(stderr,"%s: 0x%02x@(%d,%d)=>0x%02x@(%d,%d) yields mate.\n",__func__,fromprev,fromx,fromy,toprev,tox,toy);
      int wx,wy;
      if (chess_find_piece(&wx,&wy,board,PIECE_WHITE|PIECE_KING)<0) return 1; // No White King? Well, um, I guess it's still mate.
      if (!chess_is_check(tmp,wx,wy)) { // Black is mated and White is not in check. Gotcha!
        fprintf(stderr,"%s: ...mate by moving 0x%02x from (%d,%d) to (%d,%d)\n",__func__,fromprev,fromx,fromy,tox,toy);
        return 1;
      }
      //fprintf(stderr,"%s: ...but it checks the White King so never mind.\n",__func__);
    } else {
      //fprintf(stderr,"%s: (%d,%d)=>(%d,%d) does not mate the Black King.\n",__func__,fromx,fromy,tox,toy);
    }
    
    // Back it out and try the next.
    tmp[toy*8+tox]=toprev;
    tmp[fromy*8+fromx]=fromprev;
  }
  
  // If we fell thru, the answer is No.
  return 0;
}

/* Search board for a given piece.
 */

int chess_find_piece(int *x,int *y,const uint8_t *board,uint8_t piece) {
  if (piece&PIECE_PROMOTABLE) {
    const uint8_t *row;
    uint8_t target=(piece&PIECE_WHITE)|PIECE_PAWN;
    if (target&PIECE_WHITE) {
      if (y) *y=0;
      row=board;
    } else {
      if (y) *y=7;
      row=board+8*7;
    }
    int xx=0; for (;xx<8;xx++,row++) {
      if (*row==target) {
        if (x) *x=xx;
        return row-board;
      }
    }
  } else {
    int i=0;
    for (;i<64;i++,board++) {
      if (*board==piece) {
        if (y) *y=i>>3;
        if (x) *x=i&7;
        return i;
      }
    }
  }
  return -1;
}

/* Count all pieces on board.
 */

int chess_count_pieces(const uint8_t *board) {
  int c=0,i=64;
  for (;i-->0;board++) if (*board) c++;
  return c;
}

/* Index of a given move in list.
 */
 
int chess_moves_find(const uint16_t *movev,int movec,uint16_t move) {
  int i=0;
  for (;i<movec;i++,movev++) if (*movev==move) return i;
  return -1;
}

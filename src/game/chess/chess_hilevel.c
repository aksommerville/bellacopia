/* chess_hilevel.c
 */

#include "chess_internal.h"

/* Init.
 */
 
int chess_game_init(struct chess_game *game,char mode,double difficulty) {
  memset(game,0,sizeof(struct chess_game));
  switch (mode) {
    case '1': chess_generate_onemove(game->board,difficulty); break;
    case '2': chess_generate_clean(game->board); break;
    default: return -1;
  }
  // Cursors begin on the King, and fail if either King is missing.
  if (chess_find_piece(&game->wx,&game->wy,game->board,PIECE_WHITE|PIECE_KING)<0) return -1;
  if (chess_find_piece(&game->bx,&game->by,game->board,PIECE_BLACK|PIECE_KING)<0) return -1;
  game->turn=1; // White starts.
  game->fx=game->fy=-1; // Nothing focussed.
  chess_game_reassess(game);
  if (!(game->assess&CHESS_ASSESS_VALID)) return -1; // Should never happen but who knows.
  return 0;
}

/* Move the current cursor.
 */

int chess_game_move(struct chess_game *game,int dx,int dy) {
  if (!(game->assess&CHESS_ASSESS_RUNNING)) return -1;
  if (game->turn) { // White playing.
    game->wx+=dx; game->wx&=7;
    game->wy+=dy; game->wy&=7;
  } else { // Black playing.
    game->bx+=dx; game->bx&=7;
    game->by+=dy; game->by&=7;
  }
  return 0;
}

/* Activate.
 * Either reject, focus a piece, or commit the move.
 */
  
int chess_game_activate(struct chess_game *game) {

  // Not running: Reject.
  if (!(game->assess&CHESS_ASSESS_RUNNING)) return -1;
  
  // Get the selected cell.
  int selx,sely;
  if (game->turn) {
    selx=game->wx;
    sely=game->wy;
  } else {
    selx=game->bx;
    sely=game->by;
  }
  uint8_t selpiece=game->board[sely*8+selx];
  
  // No piece focussed: If a piece of the turn's team is under the cursor, focus it. Otherwise reject.
  if (game->fx<0) {
    if (!selpiece) return -1; // Vacant cell: Reject.
    if (game->turn) {
      if (!(selpiece&PIECE_WHITE)) return -1; // White's turn, only a white piece may be focussed.
    } else {
      if (selpiece&PIECE_WHITE) return -1; // Black's turn, only a black piece may be focussed.
    }
    game->fx=selx;
    game->fy=sely;
    chess_game_rebuild_fmovev(game);
    if (!game->fmovec) { // Trying to focus a piece with no moves: Reject.
      game->fx=game->fy=-1;
      return -1;
    }
    return 0; // OK, focussed.
  }
  
  // If (selx,sely) is (fx,fy), unfocus. We prefer that they press B to do this, but whatever.
  if ((selx==game->fx)&&(sely==game->fy)) {
    game->fx=game->fy=-1;
    game->fmovec=0;
    return 0;
  }
  
  // Selection must be contained by (fmovev).
  if (chess_moves_find(game->fmovev,game->fmovec,CHESS_MOVE_COMPOSE(selx,sely,game->fx,game->fy))<0) return -1;
  
  // Commit the move from (fx,fy) to (selx,sely).
  int err=chess_commit_move(game->board,CHESS_MOVE_COMPOSE(selx,sely,game->fx,game->fy));
  if (err<0) {
    fprintf(stderr,"%s:%d:WARNING: Move from (%d,%d) to (%d,%d) was in our proposals but rejected by chess_commit_move().\n",__FILE__,__LINE__,game->fx,game->fy,selx,sely);
    return -1;
  }
  
  // Eagerly advance the turn. If the game is over, we call it the loser's turn, ie who would go next if still running.
  if (game->turn) game->turn=0; else game->turn=1;
  int fromx=game->fx,fromy=game->fy; // Capture these in case we need to log after assessment.
  game->fx=game->fy=-1;
  chess_game_reassess(game);
  
  // If the game became invalid, leave it as-is and log it loudly.
  // This shouldn't happen; chess_commit_move() should have prevented it.
  if (!(game->assess&CHESS_ASSESS_VALID)) {
    fprintf(stderr,"%s:%d:WARNING: Move from (%d,%d) to (%d,%d) was accepted by chess_commit_move() but rendered the game invalid.\n",__FILE__,__LINE__,fromx,fromy,selx,sely);
    return err;
  }
  
  // And finally return zero or one depending on whether a piece was taken. chess_commit_move() already told us this.
  return err;
}

/* Cancel.
 * Only applicable if a piece is focussed, unfocus it.
 */
 
int chess_game_cancel(struct chess_game *game) {
  if (!(game->assess&CHESS_ASSESS_RUNNING)) return -1;
  if (game->fx<0) return -1;
  game->fx=game->fy=-1;
  game->fmovec=0;
  return 0;
}

/* Rebuild (assess,movev,fmovev).
 */

void chess_game_reassess(struct chess_game *game) {
  game->assess=chess_assess(game->board);
  game->movec=0;
  game->fmovec=0;
  if (!(game->assess&CHESS_ASSESS_RUNNING)) return;
  if ((game->movec=chess_list_team_moves(game->movev,CHESS_MOVES_LIMIT_ALL,game->board,game->turn,1))<1) {
    //TODO This is probably OK, it's probably the normal way for stalemate to report. Assess told us RUNNING because the *other* team had available moves.
    fprintf(stderr,"%s:%d:WARNING: Assessment said RUNNING but failed to identify any moves. Ending game.\n",__FILE__,__LINE__);
    game->assess=0;
    return;
  }
  chess_game_rebuild_fmovev(game);
}

/* Rebuild just (fmovev).
 */
 
void chess_game_rebuild_fmovev(struct chess_game *game) {
  game->fmovec=0;
  if (game->fx<0) return;
  uint16_t match=CHESS_MOVE_COMPOSE(0,0,game->fx,game->fy);
  uint16_t mask=CHESS_MOVE_COMPOSE(0,0,15,15);
  uint16_t *move=game->movev;
  int i=game->movec;
  for (;i-->0;move++) {
    if (((*move)&mask)==match) {
      game->fmovev[game->fmovec++]=*move;
      if (game->fmovec>=CHESS_MOVES_LIMIT_ONE) return;
    }
  }
}

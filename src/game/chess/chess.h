/* chess.h
 * There's this one little minigame where you play the last move of a Chess game against the Sage.
 * To support that, we have a complete model of Chess. Because why kill a problem when you can *over*kill it?
 *
 * Some things to know:
 *  - White always starts at the bottom and black at the top. If you're going to support otherwise, flip at render and input.
 *  - En-passant is not supported, because we don't track whether each pawn has just recently moved.
 *  - Castling is not supported because it's complicated and would never happen in the minigame.
 *  - The minigame scenarios are always valid but not necessarily reachable in real life. (eg pawns can cluster in impossible ways).
 *  - We detect only a few obvious stalemate scenarios (no moves, two kings, and two kings + bishop). Wrapper should allow manual stalemate and forfeit.
 *  - If you want a clock, that's your own concern.
 */
 
#ifndef CHESS_H
#define CHESS_H

/* Model constants.
 **************************************************************************/

/* Pieces are modelled as a nonzero 8-bit integer. We only use the low 4 bits.
 */
#define PIECE_WHITE      0x08 /* Combines with one of the others. Black if unset. */
#define PIECE_BLACK      0x00
#define PIECE_ROLE_MASK  0x07 /* AND against this for just the role... */
#define PIECE_PAWN       0x01 /* Role IDs are in the same order as the tiles in RID_image_labyrinth2. */
#define PIECE_ROOK       0x02
#define PIECE_KNIGHT     0x03
#define PIECE_BISHOP     0x04
#define PIECE_QUEEN      0x05
#define PIECE_KING       0x06
#define PIECE_PROMOTABLE 0x80 /* Special value for chess_find_piece(): "any promotable pawn of the given color". */

/* Boards store as uint8_t, LRTB 8x8.
 * Value is zero if vacant, or PIECE_*.
 * Feels a little silly to declare these, obviously they won't change, but just for the sake of explicitness.
 */
#define CHESS_BOARD_SIZE 64
#define CHESS_BOARD_W 8
#define CHESS_BOARD_H 8

/* Moves are expressed as 16 bits (12 actually used):
 *  0x0000
 *    |||+- to row
 *    ||+-- to column
 *    |+--- from row
 *    +---- from column
 */
#define CHESS_MOVE_COMPOSE(tocol,torow,fromcol,fromrow) (((fromcol)<<12)|((fromrow)<<8)|((tocol)<<4)|(torow))
#define CHESS_MOVE_TO_ROW(move) ((move)&7)
#define CHESS_MOVE_TO_COL(move) (((move)>>4)&7)
#define CHESS_MOVE_FROM_ROW(move) (((move)>>8)&7)
#define CHESS_MOVE_FROM_COL(move) (((move)>>12)&7)

#define CHESS_MOVES_LIMIT_ONE 28 /* A free Queen has 28 moves, Rook and Bishop 14, Knight and King 8, and Pawns 4 at best. */
#define CHESS_MOVES_LIMIT_ALL 133 /* 133 definitely isn't reachable, it's the naive sum of per-piece maximums. */

/* High-level conveniences to run a game with some UI-specific features.
 **********************************************************************************/
 
struct chess_game {
  uint8_t board[CHESS_BOARD_SIZE];
  int wx,wy,bx,by; // Player's cursors.
  int fx,fy; // Focussed piece or (-1,-1).
  int turn; // 0=black, 1=white
  int assess; // Latest assessment.
  uint16_t movev[CHESS_MOVES_LIMIT_ALL]; // All available moves for this turn.
  int movec;
  uint16_t fmovev[CHESS_MOVES_LIMIT_ONE]; // All available moves for the given focus. Empty if (fx<0).
  int fmovec;
};

/* (mode) is '2' for a clean two-player game, (difficulty) ignored.
 * Or '1' for a single-player single-move game, (difficulty) in 0..1.
 */
int chess_game_init(struct chess_game *game,char mode,double difficulty);

/* Report UI activity, for whoever's turn it is.
 * These all fail if the action is not applicable to current state.
 * (so you can do separate "reject" and "commit" sound effects).
 * Activate may change turn and may end the game.
 * Activate returns 0 on most success, and 1 if a piece was captured, because you probably want a different sound effect then.
 */
int chess_game_move(struct chess_game *game,int dx,int dy);
int chess_game_activate(struct chess_game *game);
int chess_game_cancel(struct chess_game *game);

/* Freshens (assess,movev,movec,fmovev,fmovec).
 * Normally this is only called by other chess_game functions.
 * Caller should toggle (turn) and invalidate (fx,fy) first.
 */
void chess_game_reassess(struct chess_game *game);

/* Rebuild (fmovev) based on (fx,fy,movev).
 * Normally only called internally.
 */
void chess_game_rebuild_fmovev(struct chess_game *game);

/* Playing the game, as stateless as possible.
 ************************************************************************************/

/* Validate and commit a move in place.
 * If the move is illegal for any reason, returns <0.
 * We validate extensively. eg if this puts the piece's King in check we reject.
 * Commit is just validate followed by two writes.
 * Both return >0 if there will be a capture, 0 if moving to a vacant space, or <0 if invalid.
 */
int chess_commit_move(uint8_t *board,uint16_t move);
int chess_validate_move(const uint8_t *board,uint16_t move);

/* Report the game's state for this board, in 4 bits.
 * First, if VALID unset, something is technically wrong and we can't say more.
 * All nonzero reports will have VALID set.
 * WHO is relevant only if CHECK -- it says which team is in check.
 * If CHECK but not RUNNING, it's checkmate.
 * If not CHECK and not RUNNING, it's stalemate.
 * CHECK and RUNNING can appear together; that's an ordinary check that can be escaped.
 *
 * An edge case: If there's only one King, we still call the board valid.
 * We need this, in order for the move that takes the King to be understood as valid in advance.
 */
int chess_assess(const uint8_t *board);
#define CHESS_ASSESS_VALID       0x0001 /* If unset, the board is impossible, eg missing a King. */
#define CHESS_ASSESS_RUNNING     0x0002 /* Further moves can be made. */
#define CHESS_ASSESS_CHECK       0x0004 /* One King is in check. See WHO. If not RUNNING, it's checkmate. */
#define CHESS_ASSESS_WHO         0x0008 /* Unset=black, set=white. */

/* Generate a list of valid moves starting at (x,y).
 * Empty if there's no piece there, or if the piece has no moves.
 * With (validate), we'll filter out moves that leave the moving piece's King in check.
 * When not validating, we may return >dsta. But if validating, we discard that excess.
 * CHESS_MOVES_LIMIT_ONE is always enough to hold any move set from here.
 */
int chess_list_moves(uint16_t *dstv,int dsta,const uint8_t *board,int x,int y,int validate);

/* Same idea as chess_list_moves, but run for an entire team (zero=black, nonzero=white).
 */
int chess_list_team_moves(uint16_t *dstv,int dsta,const uint8_t *board,int team,int validate);

/* Return the positions of pieces on the given team which are currently checking their enemy King.
 * Everything we return will have the King's position as "to", and the interesting position as "from".
 * Without (validate) we use naive move listing and may include pieces constrained due to blocking.
 * So use (validate) unless you have a good reason not to.
 */
int chess_list_checkers(uint16_t *dstv,int dsta,const uint8_t *board,int attacking_team,int validate);

/* (kingx,kingy) must point to a King on this board.
 * Returns nonzero if any opponent piece can reach it.
 * We do not consider pinning of those opponent pieces, and the consensus I've seen online that we're not supposed to, either.
 * Update: The piece pointed to doesn't have to be a King. Maybe I should have called it "is_threatened", more general.
 */
int chess_is_check(const uint8_t *board,int kingx,int kingy);

/* Nonzero if there is one legal move for White which would leave Black in checkmate.
 * All boards for our one-player minigame will satisfy this.
 */
int chess_one_move_from_mate(const uint8_t *board);

/* <0 if not found, or the index in (board) 0..63.
 * Optionally turns that index into (x,y) for you.
 */
int chess_find_piece(int *x,int *y,const uint8_t *board,uint8_t piece);

/* How many nonzero cells? Usually (0..32), tho invalid boards could return up to 64.
 */
int chess_count_pieces(const uint8_t *board);

/* Index of (move) in (movev) or -1.
 */
int chess_moves_find(const uint16_t *movev,int movec,uint16_t move);

/* Generating boards, high-level.
 * We assume that cell (0,0) is black. It matters, for placing the royalty.
 * And we assume that white is on the bottom, and white is the attacking team for onemove.
 *********************************************************************************************/

/* For starting a real game.
 */
void chess_generate_clean(uint8_t *board);

/* For starting our one-player single-move minigame.
 * (difficulty) in 0..1.
 */
void chess_generate_onemove(uint8_t *board,double difficulty);

/* Detailed support for generating boards.
 * Probably only the generators themselves need to use these.
 **********************************************************************************/

/* One function to perform an axiswise transform (EGG_XFORM_*), no questions asked.
 * Another to select an appropriate transform, perform it into a buffer, and copy back.
 * All boards can be flipped horizontally. Those with no pawns can also flip vertically and swap axes.
 */
void chess_board_transform(uint8_t *dst,const uint8_t *src,uint8_t xform);
void chess_board_transform_in_place(uint8_t *board);

/* Generic generator functions.
 * These expect an all-zeroes board initially, and fill it with the minimal setup for a given scenario.
 * Generated boards typically have a fixed orientation. Caller is expected to transform after, and then fill with distractions.
 * We can also choose a generator randomly for a given difficulty.
 */
typedef void (*chess_generate_fn)(uint8_t *board);
int chess_count_generators();
chess_generate_fn chess_get_generator(int p);
chess_generate_fn chess_choose_generator(double difficulty);

/* Try to add (addc) pieces.
 * (board) must initially be one move away from checkmate for white, and we will preserve that at each step.
 * It's possible, though not likely, that the final board will have a different one-move checkmate.
 * Returns the count of pieces added, never more than (addc).
 */
int chess_board_generate_distractions_carefully(uint8_t *board,int addc);

#endif

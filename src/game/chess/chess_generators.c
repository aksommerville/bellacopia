/* chess_generators.c
 * Little bits that generate our canned one-move-to-mate scenarios.
 */

#include "chess_internal.h"

/* Pick something at random from a list of ints, or remove entries by value.
 * Both return the new length.
 */
 
static int pickone(int *dst,int *v,int c) {
  if (c<1) { *dst=0; return 0; }
  int p=rand()%c;
  *dst=v[p];
  c--;
  memmove(v+p,v+p+1,sizeof(int)*(c-p));
  return c;
}

static int rmvalue(int *v,int c,int value) {
  int i=c;
  while (i-->0) {
    if (v[i]==value) {
      c--;
      memmove(v+i,v+i+1,sizeof(int)*(c-i));
    }
  }
  return c;
}

/* Set a cell.
 */
 
static void setboard(uint8_t *board,int x,int y,uint8_t v) {
  if ((x<0)||(y<0)||(x>=8)||(y>=9)) {
    fprintf(stderr,"%s:%d: Illegal setboard: %d,%d = 0x%02x\n",__FILE__,__LINE__,x,y,v);
    return;
  }
  uint8_t *p=board+y*8+x;
  if (*p) {
    fprintf(stderr,"%s:%d: setboard(%d,%d,0x%02x) overwriting nonzero 0x%02x\n",__FILE__,__LINE__,x,y,v,*p);
  }
  *p=v;
}

/* rookroll: Black King is at the edge and White has two Rooks or a Rook and a Queen, with one at row 1.
 */
 
static void gen_rookroll(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  // Choose 3 distinct columns, and the Rooks are not in the columns adjacent to the Black King.
  // Actually make it 4. Let's put White King in his own column so we don't have to worry about blocking.
  int colv[8]={0,1,2,3,4,5,6,7};
  int colc=8,bkingx,wkingx,rook1x,rook2x;
  colc=pickone(&bkingx,colv,colc);
  colc=pickone(&wkingx,colv,colc);
  colc=rmvalue(colv,colc,bkingx-1);
  colc=rmvalue(colv,colc,bkingx+1);
  colc=pickone(&rook1x,colv,colc);
  colc=pickone(&rook2x,colv,colc);
  int rook2y=1+rand()%7;
  uint8_t rook2role=PIECE_ROOK; // When rook2 is not diagonal to bking, and randomly, make it a queen instead.
  int dx=rook2x-bkingx; if (dx<0) dx=-dx;
  int dy=rook2y-0; if (dy<0) dy=-dy;
  if ((dx!=dy)&&(rand()&1)) rook2role=PIECE_QUEEN;
  setboard(board,bkingx,0,PIECE_BLACK|PIECE_KING);
  setboard(board,wkingx,2+rand()%6,PIECE_WHITE|PIECE_KING);
  setboard(board,rook1x,1,PIECE_WHITE|PIECE_ROOK);
  setboard(board,rook2x,rook2y,PIECE_WHITE|rook2role);
}

/* kingnrook: White has only a Rook or Queen. Black King is held in by White King.
 */
 
static void gen_kingnrook(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  // Random column for the two Kings, then the agent goes at least 2 away from there.
  int colv[8]={0,1,2,3,4,5,6,7};
  int colc=8,kingx,agentx;
  colc=pickone(&kingx,colv,colc);
  colc=rmvalue(colv,colc,kingx-1);
  colc=rmvalue(colv,colc,kingx+1);
  colc=pickone(&agentx,colv,colc);
  int agenty=1+rand()%7;
  uint8_t agentpiece=PIECE_WHITE|PIECE_ROOK;
  int dx=agentx-kingx; if (dx<0) dx=-dx;
  int dy=agenty;
  if ((dx!=dy)&&(rand()&1)) agentpiece=PIECE_WHITE|PIECE_QUEEN; // Randomly Queen, if not checking the Black King.
  setboard(board,kingx,0,PIECE_BLACK|PIECE_KING);
  setboard(board,kingx,2,PIECE_WHITE|PIECE_KING);
  setboard(board,agentx,agenty,agentpiece);
}

/* corneredking: Black King is cornered, held in by White King and anything, with one other piece ready to finish it.
 */
 
static void gen_corneredking(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  int wkingx=rand()&1;
  setboard(board,0,0,PIECE_BLACK|PIECE_KING);
  setboard(board,wkingx,2,PIECE_WHITE|PIECE_KING);
  // Holder piece can be Knight(2), Bishop(6), Rook(0|6--don't block the diagonal), Queen(any Bishop or Rook position), or Pawn(1).
  int holderx,holdery,holderpiece;
  int candidatec=wkingx?15:27; // Rook or Queen in column 1 is only an option when our King isn't blocking it.
  int choice=rand()%candidatec;
       if (!choice--) { holderx=2; holdery=2; holderpiece=PIECE_WHITE|PIECE_KNIGHT; }
  else if (!choice--) { holderx=3; holdery=1; holderpiece=PIECE_WHITE|PIECE_KNIGHT; }
  else if (!choice--) { holderx=2; holdery=1; holderpiece=PIECE_WHITE|PIECE_PAWN; }
  else if (choice<6) { holderx=2+choice; holdery=1+choice; holderpiece=PIECE_WHITE|PIECE_BISHOP; }
  else if (choice<12) { choice-=6; holderx=2+choice; holdery=1+choice; holderpiece=PIECE_WHITE|PIECE_QUEEN; }
  else if (choice<18) { choice-=12; holderx=1; holdery=2+choice; holderpiece=PIECE_WHITE|PIECE_ROOK; }
  else { choice-=18; holderx=1; holdery=2+choice; holderpiece=PIECE_WHITE|PIECE_QUEEN; }
  setboard(board,holderx,holdery,holderpiece);
  // Agent can be almost anything. One King position rules out Pawn, and one combination of (King,Holder) rules out Knight.
  int agentpiecev[5]={PIECE_ROOK,PIECE_BISHOP};
  int agentpiecec=2;
  if (holderpiece!=(PIECE_WHITE|PIECE_QUEEN)) agentpiecev[agentpiecec++]=PIECE_QUEEN;
  if (!wkingx&&(holderx!=1)) agentpiecev[agentpiecec++]=PIECE_PAWN;
  if (!wkingx||(holderx!=2)||(holdery!=1)) {
    if (wkingx&&(holderpiece==(PIECE_WHITE|PIECE_BISHOP))) {
      // Can't use Knight here, because it would cut off the holder's line of sight.
    } else {
      agentpiecev[agentpiecec++]=PIECE_KNIGHT;
    }
  }
  uint8_t agentpiece=agentpiecev[rand()%agentpiecec];
  // Place the agent randomly so it's one move from check. Varies according to role.
  int agentx,agenty;
  switch (agentpiece) {
    case PIECE_PAWN: {
        agentx=1;
        agenty=2;
      } break;
    case PIECE_ROOK: {
        // There are more valid positions than this, but whatever this is plenty.
        int colv[6]={2,3,4,5,6,7};
        int colc=rmvalue(colv,6,holderx);
        pickone(&agentx,colv,colc);
        agenty=1+rand()%7;
      } break;
    case PIECE_BISHOP: {
        // Any unoccupied even cell where (x!=y). (x==y) is the attack line.
        // I don't like these unbounded loops, but come on, almost all of the board is valid. (we can force evenness)
        for (;;) {
          agentx=rand()&7;
          agenty=(rand()&6)|(agentx&1);
          if (agentx==agenty) continue;
          if (board[agenty*8+agentx]) continue;
          break;
        }
      } break;
    case PIECE_QUEEN: {
        // Do the same as Rook, but forbid the attack line since that would be an illegal initial check.
        int colv[6]={2,3,4,5,6,7};
        int colc=rmvalue(colv,6,holderx);
        pickone(&agentx,colv,colc);
        for (;;) { // Again, very unlikely to spend much time in this loop, don't worry about it.
          agenty=1+rand()%7;
          if (agentx!=agenty) break;
        }
      } break;
    case PIECE_KNIGHT: {
        // 8 possible positions. Must check each for both the before and after, both must be vacant. There will always be at least 3 options.
        uint8_t candidatev[8]; // (x<<4)|y
        candidatec=0;
        #define IFVACANT(x,y) if (!board[(y)*8+(x)]) candidatev[candidatec++]=((x)<<4)|(y);
        if (!board[2*8+1]) { // Four possible ways to approach our King's right hand.
          IFVACANT(0,4)
          IFVACANT(2,4)
          IFVACANT(3,3)
          IFVACANT(3,1)
        }
        if (holderpiece!=(PIECE_WHITE|PIECE_BISHOP)) { // Don't use the far position if a Bishop is holding; we'd cut off his line of sight.
          if (!board[1*8+2]) { // Four possible ways to the attack position further from our King. Don't block a holding Rook or Queen!
            IFVACANT(4,0)
            IFVACANT(4,2)
            IFVACANT(3,3)
            if (holderx!=1) { IFVACANT(1,3) }
          }
        }
        #undef IFVACANT
        int candidatep=rand()%candidatec;
        agentx=candidatev[candidatep]>>4;
        agenty=candidatev[candidatep]&7;
      } break;
  }
  setboard(board,agentx,agenty,PIECE_WHITE|agentpiece);
}

/* arabian: Black King is cornered with a White Knight watching both cardinal escapes. White Rook is ready to enter one of those cardinals.
 */
 
static void gen_arabian(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  setboard(board,0,0,PIECE_BLACK|PIECE_KING);
  setboard(board,2,2,PIECE_WHITE|PIECE_KNIGHT);
  setboard(board,1,2+rand()%6,PIECE_WHITE|((rand()&1)?PIECE_ROOK:PIECE_QUEEN));
  setboard(board,3+rand()%5,3+rand()%5,PIECE_WHITE|PIECE_KING);
}

/* foolsmate: The fastest possible checkmate.
 */
 
static void gen_foolsmate(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  // Black back row is always its initial state. Some things could be moved decoratively, but meh, it's complicated.
  setboard(board,0,0,PIECE_BLACK|PIECE_ROOK);
  setboard(board,1,0,PIECE_BLACK|PIECE_KNIGHT);
  setboard(board,2,0,PIECE_BLACK|PIECE_BISHOP);
  setboard(board,3,0,PIECE_BLACK|PIECE_QUEEN);
  setboard(board,4,0,PIECE_BLACK|PIECE_KING);
  setboard(board,5,0,PIECE_BLACK|PIECE_BISHOP);
  setboard(board,6,0,PIECE_BLACK|PIECE_KNIGHT);
  setboard(board,7,0,PIECE_BLACK|PIECE_ROOK);
  // Black Pawns: Three have a single position, two have two choices, and the first three have three choices.
  setboard(board,0,1+rand()%3,PIECE_BLACK|PIECE_PAWN);
  setboard(board,1,1+rand()%3,PIECE_BLACK|PIECE_PAWN);
  setboard(board,2,1+rand()%3,PIECE_BLACK|PIECE_PAWN);
  setboard(board,3,1,PIECE_BLACK|PIECE_PAWN);
  setboard(board,4,1,PIECE_BLACK|PIECE_PAWN);
  setboard(board,5,2+rand()%2,PIECE_BLACK|PIECE_PAWN);
  setboard(board,6,3,PIECE_BLACK|PIECE_PAWN);
  setboard(board,7,1+rand()%2,PIECE_BLACK|PIECE_PAWN);
  // White Pawns: One must advance, one must stay home or advance by two, one can't advance more than one, and the other five have complete freedom.
  setboard(board,0,6-rand()%3,PIECE_WHITE|PIECE_PAWN);
  setboard(board,1,6-rand()%3,PIECE_WHITE|PIECE_PAWN);
  setboard(board,2,6-rand()%3,PIECE_WHITE|PIECE_PAWN);
  setboard(board,3,6-rand()%3,PIECE_WHITE|PIECE_PAWN);
  setboard(board,4,5-rand()%2,PIECE_WHITE|PIECE_PAWN);
  setboard(board,5,(rand()&1)?6:4,PIECE_WHITE|PIECE_PAWN);
  setboard(board,6,6-rand()%2,PIECE_WHITE|PIECE_PAWN);
  setboard(board,7,6-rand()%3,PIECE_WHITE|PIECE_PAWN);
  // White back row is initial.
  setboard(board,0,7,PIECE_WHITE|PIECE_ROOK);
  setboard(board,1,7,PIECE_WHITE|PIECE_KNIGHT);
  setboard(board,2,7,PIECE_WHITE|PIECE_BISHOP);
  setboard(board,3,7,PIECE_WHITE|PIECE_QUEEN);
  setboard(board,4,7,PIECE_WHITE|PIECE_KING);
  setboard(board,5,7,PIECE_WHITE|PIECE_BISHOP);
  setboard(board,6,7,PIECE_WHITE|PIECE_KNIGHT);
  setboard(board,7,7,PIECE_WHITE|PIECE_ROOK);
}

/* scholarsmate: Similar to foolsmate, but the White Queen must move under her Bishop's protection.
 */
 
static void gen_scholarsmate(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  // Black back row is always its initial state.
  setboard(board,0,0,PIECE_BLACK|PIECE_ROOK);
  setboard(board,1,0,PIECE_BLACK|PIECE_KNIGHT);
  setboard(board,2,0,PIECE_BLACK|PIECE_BISHOP);
  setboard(board,3,0,PIECE_BLACK|PIECE_QUEEN);
  setboard(board,4,0,PIECE_BLACK|PIECE_KING);
  setboard(board,5,0,PIECE_BLACK|PIECE_BISHOP);
  setboard(board,6,0,PIECE_BLACK|PIECE_KNIGHT);
  setboard(board,7,0,PIECE_BLACK|PIECE_ROOK);
  // Black King's Pawn must advance by two, and the next one must not advance. Of the six others, we will advance 2, to preserve the sequence.
  // Two pawns on the right are limited; the four on the left can advance by either one or two.
  setboard(board,4,3,PIECE_BLACK|PIECE_PAWN);
  setboard(board,5,1,PIECE_BLACK|PIECE_PAWN);
  int colv[6]={0,1,2,3,6,7};
  int pawn1x,pawn2x;
  int colc=pickone(&pawn1x,colv,6);
  colc=pickone(&pawn2x,colv,colc);
  setboard(board,0,((pawn1x==0)||(pawn2x==0))?(2+rand()%2):1,PIECE_BLACK|PIECE_PAWN);
  setboard(board,1,((pawn1x==1)||(pawn2x==1))?(2+rand()%2):1,PIECE_BLACK|PIECE_PAWN);
  setboard(board,2,((pawn1x==2)||(pawn2x==2))?(2+rand()%2):1,PIECE_BLACK|PIECE_PAWN);
  setboard(board,3,((pawn1x==3)||(pawn2x==3))?2:1,PIECE_BLACK|PIECE_PAWN);
  setboard(board,6,((pawn1x==6)||(pawn2x==6))?3:1,PIECE_BLACK|PIECE_PAWN);
  setboard(board,7,((pawn1x==7)||(pawn2x==7))?2:1,PIECE_BLACK|PIECE_PAWN);
  // White Pawns are in their initial state except the King's, which can advance by one or two.
  setboard(board,0,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,1,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,2,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,3,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,4,(rand()&1)?5:4,PIECE_WHITE|PIECE_PAWN);
  setboard(board,5,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,6,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,7,6,PIECE_WHITE|PIECE_PAWN);
  // White back row is initial except Queen and King's Bishop, which are in fixed forward positions.
  setboard(board,0,7,PIECE_WHITE|PIECE_ROOK);
  setboard(board,1,7,PIECE_WHITE|PIECE_KNIGHT);
  setboard(board,2,7,PIECE_WHITE|PIECE_BISHOP);
  setboard(board,7,3,PIECE_WHITE|PIECE_QUEEN);
  setboard(board,4,7,PIECE_WHITE|PIECE_KING);
  setboard(board,2,4,PIECE_WHITE|PIECE_BISHOP);
  setboard(board,6,7,PIECE_WHITE|PIECE_KNIGHT);
  setboard(board,7,7,PIECE_WHITE|PIECE_ROOK);
}

/* legalsmate: Another opening gambit, with two White Knights doing most of the work.
 */
 
static void gen_legalsmate(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  // We're seven moves in. On the Black side, they're mostly dummy Pawn moves, one King move, and one unfortunate Pawn move that matters (column 3).
  setboard(board,0,0,PIECE_BLACK|PIECE_ROOK);
  setboard(board,1,0,PIECE_BLACK|PIECE_KNIGHT);
  setboard(board,2,0,PIECE_BLACK|PIECE_BISHOP);
  setboard(board,3,0,PIECE_BLACK|PIECE_QUEEN);
  setboard(board,4,1,PIECE_BLACK|PIECE_KING);
  setboard(board,5,0,PIECE_BLACK|PIECE_BISHOP);
  setboard(board,6,0,PIECE_BLACK|PIECE_KNIGHT);
  setboard(board,7,0,PIECE_BLACK|PIECE_ROOK);
  setboard(board,0,2+rand()%2,PIECE_BLACK|PIECE_PAWN); // dummy
  setboard(board,1,2+rand()%2,PIECE_BLACK|PIECE_PAWN); // dummy
  setboard(board,2,3,PIECE_BLACK|PIECE_PAWN); // dummy but can't be in row 2; he'd threaten the agent Knight.
  setboard(board,3,2,PIECE_BLACK|PIECE_PAWN); // important
  // King's Pawn is presumed to have advanced by 2 and been eaten by our Knight.
  // The Black column-5 Pawn was eaten by our Bishop.
  setboard(board,6,1,PIECE_BLACK|PIECE_PAWN); // stationary to lend credence to our Bishop's presence.
  setboard(board,7,2,PIECE_BLACK|PIECE_PAWN); // dummy, keep out of row 3 to avoid our Bishop's path.
  // White Pawns are all in place except the King's Pawn has advanced by one. Two is valid too, but would block one of the agent's wrong moves.
  setboard(board,0,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,1,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,2,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,3,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,4,5,PIECE_WHITE|PIECE_PAWN);
  setboard(board,5,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,6,6,PIECE_WHITE|PIECE_PAWN);
  setboard(board,7,6,PIECE_WHITE|PIECE_PAWN);
  // White back row has both Knights and the King's Bishop afield in specific places.
  setboard(board,0,7,PIECE_WHITE|PIECE_ROOK);
  setboard(board,2,5,PIECE_WHITE|PIECE_KNIGHT);
  setboard(board,2,7,PIECE_WHITE|PIECE_BISHOP);
  setboard(board,3,7,PIECE_WHITE|PIECE_QUEEN);
  setboard(board,4,7,PIECE_WHITE|PIECE_KING);
  setboard(board,5,1,PIECE_WHITE|PIECE_BISHOP);
  setboard(board,4,3,PIECE_WHITE|PIECE_KNIGHT);
  setboard(board,7,7,PIECE_WHITE|PIECE_ROOK);
}

/* backrank: Black King is trapped behind his pawns. White has a Rook or Queen ready to enter that back row.
 */
 
static void gen_backrank(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  int colv[8]={0,1,2,3,4,5,6,7};
  int kingx;
  int colc=pickone(&kingx,colv,8);
  setboard(board,kingx,0,PIECE_BLACK|PIECE_KING);
  setboard(board,kingx,1,PIECE_BLACK|PIECE_PAWN);
  if (kingx>0) { setboard(board,kingx-1,1,PIECE_BLACK|PIECE_PAWN); colc=rmvalue(colv,colc,kingx-1); }
  if (kingx<7) { setboard(board,kingx+1,1,PIECE_BLACK|PIECE_PAWN); colc=rmvalue(colv,colc,kingx+1); }
  int agentx,agenty,wx,wy;
  colc=pickone(&agentx,colv,colc);
  agenty=2+rand()%6;
  colc=pickone(&wx,colv,colc); // Put the White King in an occupied column. Not really necessary, but keeps it simple.
  wy=2+rand()%6;
  setboard(board,agentx,agenty,PIECE_WHITE|((rand()&1)?PIECE_ROOK:PIECE_QUEEN));
  setboard(board,wx,wy,PIECE_WHITE|PIECE_KING);
}

/* smother: Black King is cornered and surrounded by his own men, and a White Knight is positioned to check him.
 */
 
static void gen_smother(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  setboard(board,0,0,PIECE_BLACK|PIECE_KING);
  // The two pieces in front of the Black King are Bishop, Rook, or Knight. Rook can't be adjacent to the threatening position.
  // Not using Pawn for these, because those would prevent the board from transforming.
  setboard(board,0,1,PIECE_BLACK|((rand()&1)?PIECE_BISHOP:PIECE_ROOK));
  setboard(board,1,1,PIECE_BLACK|((rand()&1)?PIECE_BISHOP:PIECE_KNIGHT));
  // The last piece locking in the Black King is a Knight or Rook. Queen or Bishop would be able to take the White Knight in his threatening position.
  setboard(board,1,0,PIECE_BLACK|((rand()&1)?PIECE_KNIGHT:PIECE_ROOK));
  // Put the White King anywhere that he's not in check, before the White Knight appears. Also exclude the four White Knight candidates.
  for (;;) {
    int x=rand()&7;
    int y=rand()&7;
    if ((x==1)&&(y==3)) continue;
    if ((x==3)&&(y==3)) continue;
    if ((x==4)&&(y==2)) continue;
    if ((x==4)&&(y==0)) continue;
    if (board[y*8+x]) continue;
    board[y*8+x]=PIECE_WHITE|PIECE_KING;
    if (!chess_is_check(board,x,y)) break;
    board[y*8+x]=0; // Try again. There's fiftiesh valid positions, it shouldn't take long.
  }
  // Then four options for the critical White Knight:
  switch (rand()&3) {
    case 0: setboard(board,1,3,PIECE_WHITE|PIECE_KNIGHT); break;
    case 1: setboard(board,3,3,PIECE_WHITE|PIECE_KNIGHT); break;
    case 2: setboard(board,4,2,PIECE_WHITE|PIECE_KNIGHT); break;
    case 3: setboard(board,4,0,PIECE_WHITE|PIECE_KNIGHT); break;
  }
}

/* anastasia: Black King is standing behind one of his own, with a White Knight guarding the two sides of that blocker, then White Queen or Rook delivers mate.
 */
 
static void gen_anastasia(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  // Use a Black Rook for the blocker.
  // Pawn would prevent transform, Knight would extend the blackout range, and Bishop or Queen could block the agent.
  int kingx=rand()&7;
  setboard(board,kingx,0,PIECE_BLACK|PIECE_KING);
  setboard(board,kingx,1,PIECE_BLACK|PIECE_ROOK);
  setboard(board,kingx,3,PIECE_WHITE|PIECE_KNIGHT);
  int colv[]={0,1,2,3,4,5,6,7,8};
  int colc=rmvalue(colv,8,kingx);
  colc=rmvalue(colv,colc,kingx-1);
  colc=rmvalue(colv,colc,kingx+1);
  int agentx;
  colc=pickone(&agentx,colv,colc);
  int agenty=1+rand()%7;
  // The agent could be Queen, but then we'd have to ensure it's not already check. Meh. Stick to Rook.
  setboard(board,agentx,agenty,PIECE_WHITE|PIECE_ROOK);
  // White King goes anywhere that isn't check, or in the agent's line of attack. Shouldn't be hard to find one.
  for (;;) {
    int wx=rand()&7;
    int wy=rand()&7;
    if (board[wy*8+wx]) continue;
    if ((wx==agentx)&&(wy<=agenty)) continue; // outta my way, your highness!
    board[wy*8+wx]=PIECE_WHITE|PIECE_KING;
    if (!chess_is_check(board,wx,wy)) return;
    board[wy*8+wx]=0;
  }
}

/* epaulette: Black King on the edge with both of his Rooks adjacent. White Queen delivers mate to his doorstep.
 * We really can't substitute anything for the Rooks. Pawns would be hard to believe, Knights could capture the Queen, and Bishop or Queen could block the check.
 */
 
static void gen_epaulette(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  int kingx=rand()&7;
  setboard(board,kingx,0,PIECE_BLACK|PIECE_KING);
  if (kingx>0) setboard(board,kingx-1,0,PIECE_BLACK|PIECE_ROOK);
  if (kingx<7) setboard(board,kingx+1,0,PIECE_BLACK|PIECE_ROOK);
  // White Queen goes either in row 2, or on the diagonals crossing (kingx,2), excluding row 1 since that would pre-check the Black King.
  // She *can* go in row zero, and in fact that's an interesting variation, I want it to happen sometimes.
  // If we're using row 2, ensure there's no initial diagonal line of sight to the Black King.
  struct vector { int x,y,dx,dy; } vectorv[6]={ // Only 6 because Up and Down are definitely not allowed.
    {kingx,2,-1,-1},
    {kingx,2, 1,-1},
    {kingx,2,-1, 0},
    {kingx,2, 1, 0},
    {kingx,2,-1, 1},
    {kingx,2, 1, 1},
  };
  struct candidate { int x,y; } candidatev[21];
  int candidatec=0;
  for (;;) {
    int i=6,done=1;
    struct vector *vector=vectorv;
    for (;i-->0;vector++) {
      vector->x+=vector->dx;
      vector->y+=vector->dy;
      if (vector->x<0) continue;
      if (vector->y<0) continue;
      if (vector->x>=8) continue;
      if (vector->y>=8) continue;
      done=0;
      if (vector->y==1) continue; // Both candidates in row 1 check the Black King initially.
      if (vector->y==2) { // Check for a diagonal line of sight to the Black King. Can happen twice.
        int dx=vector->x-kingx; if (dx<0) dx=-dx;
        int dy=vector->y;
        if (dx==dy) continue;
      }
      if (candidatec>=21) { // oops
        done=1;
        break;
      }
      candidatev[candidatec++]=(struct candidate){vector->x,vector->y};
    }
    if (done) break;
  }
  int candidatep=rand()%candidatec;
  setboard(board,candidatev[candidatep].x,candidatev[candidatep].y,PIECE_WHITE|PIECE_QUEEN);
  // White King can go anywhere that he isn't in check or blocking the White Queen.
  // To simplify a little, exclude the three columns around the Black King, all the diagonals from (kingx,2), and rows 0 and 2.
  for (;;) {
    int x=rand()%5;
    if (x>=kingx-1) x+=3;
    int y=1+rand()%6;
    if (y>=2) y++;
    int dx=x-kingx; if (dx<0) dx=-dx;
    int dy=y-2; if (dy<0) dy=-dy;
    if (dx==dy) continue; // On a diagonal that might block the White Queen. Don't worry about exactness, we just rule out the whole "X".
    setboard(board,x,y,PIECE_WHITE|PIECE_KING);
    return;
  }
}

/* boden: Black King has two pieces blocking on his right, and the White Bishops deliver mate.
 */
 
static void gen_boden(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  int kingx=2+rand()%5; // Leave at least one free column on the right, and two on the left.
  setboard(board,kingx,0,PIECE_BLACK|PIECE_KING);
  uint8_t backblockpiece;
  switch (rand()%3) {
    case 0: backblockpiece=PIECE_BLACK|PIECE_ROOK; break;
    case 1: backblockpiece=PIECE_BLACK|PIECE_BISHOP; break;
    case 2: backblockpiece=PIECE_BLACK|PIECE_QUEEN; break;
  }
  setboard(board,kingx+1,0,backblockpiece);
  setboard(board,kingx+1,1,PIECE_BLACK|PIECE_PAWN);
  // A White Bishop blocks the Black King's one or two cardinal moves. Always starting right of the Black King.
  int blockx=kingx+1+rand()%(7-kingx);
  int blocky=2+blockx-kingx-1;
  setboard(board,blockx,blocky,PIECE_WHITE|PIECE_BISHOP);
  // The other White Bishop is ready to move to the Black King's diagonal, at least two steps away from him.
  // Start by walking some random distance southwest, then move randomly northwest or southeast.
  int agentx=kingx-2;
  int agenty=2;
  if (agentx>0) {
    int stepc=rand()%agentx;
    agentx-=stepc;
    agenty+=stepc;
  }
  int lstepc=agentx;
  int rstepc=7-agentx;
  int dstepc=7-agenty;
  if (dstepc<rstepc) rstepc=dstepc;
  int choice=rand()%(lstepc+rstepc);
  if (choice<lstepc) {
    agentx-=choice+1;
    agenty-=choice+1;
  } else {
    choice-=lstepc;
    agentx+=choice+1;
    agenty+=choice+1;
  }
  setboard(board,agentx,agenty,PIECE_WHITE|PIECE_BISHOP);
  // White King in any vacant space on the bottom row, just to keep him out of the way.
  for (;;) {
    int wx=rand()&7;
    if (board[7*8+wx]) continue;
    setboard(board,wx,7,PIECE_WHITE|PIECE_KING);
    if (chess_is_check(board,wx,7)) { // Rare but possible. Try again.
      setboard(board,wx,7,0);
    } else {
      break;
    }
  }
}

/* For dovetail and swallowtail.
 * Place a White Queen such that she can reach (atkx,atky) in one move, and be protected there.
 * Also places the White King, randomly.
 * Give us the Black King position because you must already know it, and we need to check for check.
 */
 
static void protected_white_queen(uint8_t *board,int atkx,int atky,int kingx,int kingy) {
  struct point { int x,y; } candidatev[21];
  int candidatec=0;
  int atkp=atky*8+atkx;
  uint8_t protpiece;
 _again_:;
 
  // First arrange a protector. Rook, Bishop, Knight, or King.
  for (;;) {
    protpiece=PIECE_WHITE;
    switch (rand()&3) {
      case 0: protpiece|=PIECE_ROOK; break;
      case 1: protpiece|=PIECE_BISHOP; break;
      case 2: protpiece|=PIECE_KNIGHT; break;
      case 3: protpiece|=PIECE_KING; break;
    }
    board[atkp]=protpiece;
    // List its moves and zero the attack cell.
    uint16_t movev[CHESS_MOVES_LIMIT_ONE];
    int movec=chess_list_moves(movev,CHESS_MOVES_LIMIT_ONE,board,atkx,atky,0);
    board[atkp]=0;
    // Eliminate any protector moves that take a piece or check the Black King.
    int i=movec;
    while (i-->0) {
      int protx=CHESS_MOVE_TO_COL(movev[i]);
      int proty=CHESS_MOVE_TO_ROW(movev[i]);
      int protp=proty*8+protx;
      if (board[protp]) {
        movec--;
        memmove(movev+i,movev+i+1,sizeof(uint16_t)*(movec-i));
        continue;
      }
      board[protp]=protpiece;
      if (chess_is_check(board,kingx,kingy)) {
        movec--;
        memmove(movev+i,movev+i+1,sizeof(uint16_t)*(movec-i));
        board[protp]=0;
        continue;
      }
      if ((protpiece==(PIECE_WHITE|PIECE_KING))&&chess_is_check(board,protx,proty)) {
        movec--;
        memmove(movev+i,movev+i+1,sizeof(uint16_t)*(movec-i));
        board[protp]=0;
        continue;
      }
      board[protp]=0;
    }
    // If there's nowhere to put the protector, try again. Caller guarantees there's a valid position somewhere.
    if (!movec) continue;
    // Pick randomly from the protector's moves, and commit it.
    int choice=rand()%movec;
    setboard(board,CHESS_MOVE_TO_COL(movev[choice]),CHESS_MOVE_TO_ROW(movev[choice]),protpiece);
    break;
  }
  
  // Similar idea, this time with the White Queen.
  {
    board[atkp]=PIECE_WHITE|PIECE_QUEEN;
    uint16_t movev[CHESS_MOVES_LIMIT_ONE];
    int movec=chess_list_moves(movev,CHESS_MOVES_LIMIT_ONE,board,atkx,atky,0);
    board[atkp]=0;
    int i=movec;
    while (i-->0) {
      int qx=CHESS_MOVE_TO_COL(movev[i]);
      int qy=CHESS_MOVE_TO_ROW(movev[i]);
      int qp=qy*8+qx;
      if (board[qp]) {
        movec--;
        memmove(movev+i,movev+i+1,sizeof(uint16_t)*(movec-i));
        continue;
      }
      board[qp]=PIECE_WHITE|PIECE_QUEEN;
      if (chess_is_check(board,kingx,kingy)) {
        movec--;
        memmove(movev+i,movev+i+1,sizeof(uint16_t)*(movec-i));
        board[qp]=0;
        continue;
      }
      board[qp]=0;
    }
    int choice=rand()%movec;
    setboard(board,CHESS_MOVE_TO_COL(movev[choice]),CHESS_MOVE_TO_ROW(movev[choice]),PIECE_WHITE|PIECE_QUEEN);
  }
  
  // If we chose White King as the protector, we're done. Otherwise pick a random position for him that doesn't interfere with the Queen or protector.
  if (protpiece==(PIECE_WHITE|PIECE_KING)) return;
  for (;;) {
    int wx=rand()&7;
    int wy=rand()&7;
    int wp=wy*8+wx;
    if (board[wp]) continue;
    board[wp]=PIECE_WHITE|PIECE_KING;
    if (!chess_is_check(board,wx,wy)&&chess_one_move_from_mate(board)) return;
    board[wp]=0;
  }
}

/* dovetail: Black King has two cardinal moves blocked, and White Queen can move to the corner furthest from those, under some other protection.
 */
 
static void gen_dovetail(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  // Black King goes anywhere but the edges.
  int kingx=1+rand()%6;
  int kingy=1+rand()%6;
  setboard(board,kingx,kingy,PIECE_BLACK|PIECE_KING);
  // Put a non-Knight blocker on his top and right. Don't use Pawn either, since they prevent transformation.
  setboard(board,kingx,kingy-1,PIECE_BLACK|((rand()&1)?PIECE_BISHOP:PIECE_ROOK));
  switch (rand()%3) {
    case 0: setboard(board,kingx+1,kingy,PIECE_BLACK|PIECE_BISHOP); break;
    case 1: setboard(board,kingx+1,kingy,PIECE_BLACK|PIECE_ROOK); break;
    case 2: setboard(board,kingx+1,kingy,PIECE_BLACK|PIECE_QUEEN); break;
  }
  protected_white_queen(board,kingx-1,kingy+1,kingx,kingy);
}

/* swallowtail: Black King with two bodyguards diagonally behind. White Queen checks from the front, under protection.
 */
 
static void gen_swallowtail(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  // Black King goes anywhere except the edges.
  int kingx=1+rand()%6;
  int kingy=1+rand()%6;
  setboard(board,kingx,kingy,PIECE_BLACK|PIECE_KING);
  // His bodyguards can be anything but Knight or Pawn. (Pawn is valid but prevents transformation).
  switch (rand()%3) {
    case 0: setboard(board,kingx-1,kingy-1,PIECE_BLACK|PIECE_ROOK); break;
    case 1: setboard(board,kingx-1,kingy-1,PIECE_BLACK|PIECE_BISHOP); break;
    case 2: setboard(board,kingx-1,kingy-1,PIECE_BLACK|PIECE_QUEEN); break;
  }
  switch (rand()%2) { // But omit Queen from the right, just in case we used her on the left.
    case 0: setboard(board,kingx+1,kingy-1,PIECE_BLACK|PIECE_ROOK); break;
    case 1: setboard(board,kingx+1,kingy-1,PIECE_BLACK|PIECE_BISHOP); break;
  }
  protected_white_queen(board,kingx,kingy+1,kingx,kingy);
}

/* opera: Just like boden, except the agent is a Rook instead of a Bishop.
 */
 
static void gen_opera(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  int kingx=1+rand()%6; // Needs one column left and right.
  setboard(board,kingx,0,PIECE_BLACK|PIECE_KING);
  // Rear bodyguard can be Rook, Bishop, Knight, or Queen.
  switch (rand()&3) {
    case 0: setboard(board,kingx+1,0,PIECE_BLACK|PIECE_ROOK); break;
    case 1: setboard(board,kingx+1,0,PIECE_BLACK|PIECE_BISHOP); break;
    case 2: setboard(board,kingx+1,0,PIECE_BLACK|PIECE_KNIGHT); break;
    case 3: setboard(board,kingx+1,0,PIECE_BLACK|PIECE_QUEEN); break;
  }
  // Front bodyguard can't be Knight (would threaten our attack position), and also eliminate Queen in case we already chose her.
  switch (rand()%2) {
    case 0: setboard(board,kingx+1,1,PIECE_BLACK|PIECE_ROOK); break;
    case 1: setboard(board,kingx+1,1,PIECE_BLACK|PIECE_BISHOP); break;
  }
  // Our protecting Bishop goes on the (x==y) diagonal from (kingx-1,0).
  int choice=rand()%(7-kingx);
  setboard(board,kingx+1+choice,choice+2,PIECE_WHITE|PIECE_BISHOP);
  // Agent White Rook somewhere on (kingx-1), at least one row down.
  int rooky=1+rand()%7;
  setboard(board,kingx-1,rooky,PIECE_WHITE|PIECE_ROOK);
  // White King anywhere, just make sure he doesn't interfere or get himself hurt.
  for (;;) {
    int wx=rand()&7;
    int wy=rand()&7;
    int wp=wy*8+wx;
    if (board[wp]) continue;
    board[wp]=PIECE_WHITE|PIECE_KING;
    if (!chess_is_check(board,wx,wy)&&chess_one_move_from_mate(board)) return;
    board[wp]=0;
  }
}

/* blackburn: Black King is cornered and his cardinals are covered by a White Bishop and White Knight.
 * The other White Bishop delivers mate.
 */
 
static void gen_blackburn(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  setboard(board,0,0,PIECE_BLACK|PIECE_KING);
  setboard(board,0,1,PIECE_WHITE|PIECE_BISHOP);
  setboard(board,1,0,PIECE_WHITE|PIECE_KNIGHT);
  // The other White Bishop goes on any free even cell except where (x==y).
  // Oh also, one of (x,y) must be >2. When it's in that first ribbon, the attack position is not necessarily covered.
  for (;;) {
    int x=rand()&7;
    int y=(rand()&6)|(x&1);
    if (x==y) continue;
    if ((x<=2)&&(y<=2)) continue;
    int p=y*8+x;
    if (board[p]) continue;
    setboard(board,x,y,PIECE_WHITE|PIECE_BISHOP);
    break;
  }
  // And the White King anywhere that doesn't interfere.
  // There are no check positions for him that don't also interfere.
  for (;;) {
    int wx=rand()&7;
    int wy=rand()&7;
    int wp=wy*8+wx;
    if (board[wp]) continue;
    board[wp]=PIECE_WHITE|PIECE_KING;
    if (chess_one_move_from_mate(board)) return;
    board[wp]=0;
  }
}

/* damiano: Black King is on the left edge or blocked by his own Rook to the left, and a Black Bishop in front.
 * White Queen approaches to his southeast, protected by a White Bishop.
 * Chess.com describes it with Pawns instead of Bishops, but I avoid that, to promote transforms.
 */
 
static void gen_damiano(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  // Black King, his coterie, and a White Bishop holding them in.
  int kingx=rand()%7;
  setboard(board,kingx,0,PIECE_BLACK|PIECE_KING);
  if (kingx) setboard(board,kingx-1,0,PIECE_BLACK|PIECE_ROOK);
  setboard(board,kingx,1,PIECE_BLACK|PIECE_BISHOP);
  setboard(board,kingx,2,PIECE_WHITE|PIECE_BISHOP);
  
  // White Queen is one move away from the kill spot.
  int atkx=kingx+1,atky=1;
  int atkp=atky*8+atkx;
  board[atkp]=PIECE_WHITE|PIECE_QUEEN;
  uint16_t movev[CHESS_MOVES_LIMIT_ONE];
  int movec=chess_list_moves(movev,CHESS_MOVES_LIMIT_ONE,board,atkx,atky,0);
  board[atkp]=0;
  int i=movec;
  while (i-->0) {
    int qx=CHESS_MOVE_TO_COL(movev[i]);
    int qy=CHESS_MOVE_TO_ROW(movev[i]);
    int qp=qy*8+qx;
    if (board[qp]) {
      movec--;
      memmove(movev+i,movev+i+1,sizeof(uint16_t)*(movec-i));
      continue;
    }
    board[qp]=PIECE_WHITE|PIECE_QUEEN;
    if (chess_is_check(board,kingx,0)) {
      movec--;
      memmove(movev+i,movev+i+1,sizeof(uint16_t)*(movec-i));
      board[qp]=0;
      continue;
    }
    board[qp]=0;
  }
  int choice=rand()%movec;
  setboard(board,CHESS_MOVE_TO_COL(movev[choice]),CHESS_MOVE_TO_ROW(movev[choice]),PIECE_WHITE|PIECE_QUEEN);
  
  // Then the White King wherever he won't get in the way.
  for (;;) {
    int wx=rand()&7;
    int wy=rand()&7;
    int wp=wy*8+wx;
    if (board[wp]) continue;
    board[wp]=PIECE_WHITE|PIECE_KING;
    if (!chess_is_check(board,wx,wy)&&chess_one_move_from_mate(board)) return;
    board[wp]=0;
  }
}

/* morphy: Black King is cornered and protected by his Rook from the White Bishop.
 * White Rooks or Black dummies hold the Black King in place.
 * Checkmate when you take the Black Rook with your Bishop.
 */
 
static void gen_morphy(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);
  setboard(board,0,0,PIECE_BLACK|PIECE_KING);
  // On the King's right, either his Bishop, or a White Rook guarding the column.
  if (rand()&1) {
    setboard(board,1,0,PIECE_BLACK|PIECE_BISHOP);
  } else {
    setboard(board,1,2+rand()%6,PIECE_WHITE|PIECE_ROOK);
  }
  // Below the King, either his Pawn, or a White Rook guarding the row.
  // Alas, it really does need to be a Pawn. Can't have the two Bishops on the same color, Queen or Rook could block the check, and Knight might take our Bishop.
  if (rand()&1) {
    setboard(board,0,1,PIECE_BLACK|PIECE_PAWN);
  } else {
    setboard(board,2+rand()%6,1,PIECE_WHITE|PIECE_ROOK);
  }
  // A Black Rook on his King's diagonal, leaving at least one space outside.
  int rookx=2+rand()%5;
  setboard(board,rookx,rookx,PIECE_BLACK|PIECE_ROOK);
  // A White Bishop in any position threatening (rookx,rookx), destined to mate the Black King.
  uint16_t movev[CHESS_MOVES_LIMIT_ONE];
  board[rookx*8+rookx]=PIECE_WHITE|PIECE_BISHOP;
  int movec=chess_list_moves(movev,CHESS_MOVES_LIMIT_ONE,board,rookx,rookx,0);
  board[rookx*8+rookx]=PIECE_BLACK|PIECE_ROOK;
  int i=movec;
  while (i-->0) {
    int bishopx=CHESS_MOVE_TO_COL(movev[i]);
    int bishopy=CHESS_MOVE_TO_ROW(movev[i]);
    int bishopp=bishopy*8+bishopx;
    if (board[bishopp]) {
      movec--;
      memmove(movev+i,movev+i+1,sizeof(uint16_t)*(movec-i));
      continue;
    }
    board[bishopp]=PIECE_WHITE|PIECE_BISHOP;
    int ischeck=chess_is_check(board,0,0);
    board[bishopp]=0;
    if (ischeck) {
      movec--;
      memmove(movev+i,movev+i+1,sizeof(uint16_t)*(movec-i));
      continue;
    }
  }
  int movep=rand()%movec;
  int bishopx=CHESS_MOVE_TO_COL(movev[movep]);
  int bishopy=CHESS_MOVE_TO_ROW(movev[movep]);
  setboard(board,bishopx,bishopy,PIECE_WHITE|PIECE_BISHOP);
  // Then the White King wherever he won't get in the way.
  if (!chess_one_move_from_mate(board)) { // Pretty confident that this can't happen anymore but leaving just in case.
    fprintf(stderr,"!!!!! Morphy board is not one move from mate, before placing the White King.\n");
    memset(board,0,64);
    gen_backrank(board);
    return;
  }
  for (;;) {
    int wx=rand()&7;
    int wy=rand()&7;
    int wp=wy*8+wx;
    if (board[wp]) continue;
    board[wp]=PIECE_WHITE|PIECE_KING;
    if (!chess_is_check(board,wx,wy)&&chess_one_move_from_mate(board)) return;
    //fprintf(stderr,"%d,%d? no. is_check?%d one_move_from_mate?%d\n",wx,wy,chess_is_check(board,wx,wy),chess_one_move_from_mate(board));
    board[wp]=0;
  }
}

/* Generators list.
 */
 
static struct gendesc {
  double mindiff;
  double maxdiff;
  chess_generate_fn fn;
} generatorv[]={
  //TODO Set difficulty ranges.
  //TODO Also include an arbitrary weight. The more randomizable ones should have higher weights, and eg scholarsmate, due to its fixedness, should be rare.
  {0.000,0.999,gen_rookroll},
  {0.000,0.999,gen_kingnrook},
  {0.000,0.999,gen_corneredking},
  {0.000,0.999,gen_arabian},
  {0.000,0.999,gen_foolsmate},
  {0.000,0.999,gen_scholarsmate},
  {0.000,0.999,gen_legalsmate},
  {0.000,0.999,gen_backrank},
  {0.000,0.999,gen_smother},
  {0.000,0.999,gen_anastasia},
  {0.000,0.999,gen_epaulette},
  {0.000,0.999,gen_boden},
  {0.000,0.999,gen_dovetail},
  {0.000,0.999,gen_swallowtail},
  {0.000,0.999,gen_opera},
  {0.000,0.999,gen_blackburn},
  {0.000,0.999,gen_damiano},
  {0.000,0.999,gen_morphy},
};

int chess_count_generators() {
  return sizeof(generatorv)/sizeof(generatorv[0]);
}

chess_generate_fn chess_get_generator(int p) {
  if (p<0) return 0;
  int c=chess_count_generators();
  if (p>=c) return 0;
  return generatorv[p].fn;
}

/* Choose a generator, opinionated and random.
 */
 
#define FALLBACK gen_rookroll

chess_generate_fn chess_choose_generator(double difficulty) {
  if (difficulty<0.0) difficulty=0.0;
  else if (difficulty>0.999) difficulty=0.999;
  int candidatec=0;
  int i=sizeof(generatorv)/sizeof(generatorv[0]);
  const struct gendesc *gendesc=generatorv;
  for (;i-->0;gendesc++) {
    if ((difficulty>=gendesc->mindiff)&&(difficulty<=gendesc->maxdiff)) candidatec++;
  }
  if (candidatec<1) return FALLBACK;
  int candidatep=rand()%candidatec;
  for (i=sizeof(generatorv)/sizeof(generatorv[0]),gendesc=generatorv;i-->0;gendesc++) {
    if ((difficulty>=gendesc->mindiff)&&(difficulty<=gendesc->maxdiff)) {
      if (!candidatep--) return gendesc->fn;
    }
  }
  return FALLBACK;
}

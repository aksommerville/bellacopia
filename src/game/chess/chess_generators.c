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
  /* TODO This is where I stopped, first time thru. Return to Chess.com and repro all these:
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
  */
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

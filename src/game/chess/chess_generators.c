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
  fprintf(stderr,"%s\n",__func__);//TODO
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
  fprintf(stderr,"%s\n",__func__);//TODO
  gen_rookroll(board);
}

/* corneredking: Black King is cornered, held in by White King and anything, with one other piece ready to finish it.
 */
 
static void gen_corneredking(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);//TODO
  gen_rookroll(board);
}

/* arabian: Black King is cornered with a White Knight watching both cardinal escapes. White Rook is ready to enter one of those cardinals.
 */
 
static void gen_arabian(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);//TODO
  gen_rookroll(board);
}

/* foolsmate: The fastest possible checkmate.
 */
 
static void gen_foolsmate(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);//TODO
  gen_rookroll(board);
}

/* scholarsmate: Similar to foolsmate, but the White Queen must move under her Bishop's protection.
 */
 
static void gen_scholarsmate(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);//TODO
  gen_rookroll(board);
}

/* legalsmate: Another opening gambit, with two White Knights doing most of the work.
 */
 
static void gen_legalsmate(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);//TODO
  gen_rookroll(board);
}

/* backrank: Black King is trapped behind his pawns. White has a Rook or Queen ready to enter that back row.
 */
 
static void gen_backrank(uint8_t *board) {
  fprintf(stderr,"%s\n",__func__);//TODO
  gen_rookroll(board);
}

/* Generators list.
 */
 
static struct gendesc {
  double mindiff;
  double maxdiff;
  chess_generate_fn fn;
} generatorv[]={
  //TODO Set difficulty ranges.
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

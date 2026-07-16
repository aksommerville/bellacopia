#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_gold);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//122 Once upon a time there was a witch who wanted to buy things but her purse was too small.
static void gold_122(double elapsed,int strix,int framec) {
  still(0,0,128,80);
}

//123 So she rescued a Princess and the King gave her a larger purse.
static void gold_123(double elapsed,int strix,int framec) {
  still(128,0,128,80);
}

//124 And she did another thing. !!!TODO!!!
static void gold_124(double elapsed,int strix,int framec) {
  still(0,80,128,80);
}

//125 And something else. !!!TODO!!!
static void gold_125(double elapsed,int strix,int framec) {
  still(128,80,128,80);
}

//126 And probably there's going to be four of these. !!!TODO!!!
static void gold_126(double elapsed,int strix,int framec) {
  still(0,160,128,80);
}

//127 Now Dot has an enormous 499-coin purse!
static void gold_127(double elapsed,int strix,int framec) {
  still(128,160,128,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_gold[]={
  {gold_122,122,0},
  {gold_123,123,0},
  {gold_124,124,0},
  {gold_125,125,0},
  {gold_126,126,0},
  {gold_127,127,0},
{0}};

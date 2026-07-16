#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_toad);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//131 Once upon a time there was a thing I haven't got to yet. !!!TODO!!!
static void toad_131(double elapsed,int strix,int framec) {
  still(0,0,128,96);
}

//132 It goes like this... !!!TODO!!!
static void toad_132(double elapsed,int strix,int framec) {
  still(128,0,128,96);
}

//133 It goes like that... !!!TODO!!!
static void toad_133(double elapsed,int strix,int framec) {
  still(0,96,128,96);
}

//134 It goes here when it's ready. !!!TODO!!!
static void toad_134(double elapsed,int strix,int framec) {
  still(128,96,128,96);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_toad[]={
  {toad_131,131,0},
  {toad_132,132,0},
  {toad_133,133,0},
  {toad_134,134,0},
{0}};

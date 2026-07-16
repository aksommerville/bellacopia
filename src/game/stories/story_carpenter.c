#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_shops);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//102 Making things out of wood is productive and rewarding.
static void carpenter_102(double elapsed,int strix,int framec) {
  still(0,0,128,80);
}

//103 A stick can be made into a divining rod, wand, fishpole, or broom.
static void carpenter_103(double elapsed,int strix,int framec) {
  still(128,0,128,80);
}

//104 Or a whole bunch of matches!
static void carpenter_104(double elapsed,int strix,int framec) {
  still(0,80,128,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_carpenter[]={
  {carpenter_102,102,0},
  {carpenter_103,103,0},
  {carpenter_104,104,0},
{0}};

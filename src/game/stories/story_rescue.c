#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_rescue);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//71 Once upon a time there was a princess who loved to play outside.
static void rescue_71(double elapsed,int strix,int framec) {
  still(0,0,128,80);
}

//72 She stepped into a goblin trap!
static void rescue_72(double elapsed,int strix,int framec) {
  still(128,0,128,80);
}

//73 The goblins took the princess into their cave, to lock her up until they're ready to eat her.
static void rescue_73(double elapsed,int strix,int framec) {
  still(0,80,128,80);
}

//74 Soon, the goblins kidnapped a witch too.
static void rescue_74(double elapsed,int strix,int framec) {
  still(128,80,128,80);
}

//75 The witch broke them out of the goblins' cave and escorted the princess back to her castle.
static void rescue_75(double elapsed,int strix,int framec) {
  still(0,160,128,80);
}

//76 And the king warned the princess: "Don't play outside unless you have a witch to protect you!"
static void rescue_76(double elapsed,int strix,int framec) {
  still(128,160,128,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_rescue[]={
  {rescue_71,71,0},
  {rescue_72,72,0},
  {rescue_73,73,0},
  {rescue_74,74,0},
  {rescue_75,75,0},
  {rescue_76,76,0},
{0}};

#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_labyrinth);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//77 Once upon a time there was a witch looking for root devils.
static void labyrinth_77(double elapsed,int strix,int framec) {
  still(0,0,128,80);
}

//78 She couldn't reach the temple's pool because the escalator was broken.
static void labyrinth_78(double elapsed,int strix,int framec) {
  still(128,0,128,80);
}

//79 So instead she walked through the spooky labyrinth below the temple.
static void labyrinth_79(double elapsed,int strix,int framec) {
  still(0,80,128,80);
}

//80 Along the way, she got swallowed by a sea monster!
static void labyrinth_80(double elapsed,int strix,int framec) {
  still(128,80,128,80);
}
static int labyrinth_condition_80() {
  // It's possible to get swallowed and not pick up the power glove, but that should be rare. There's no other flag to key off.
  if (store_get_itemid(NS_itemid_glove)) return 1;
  return 0;
}

//81 She didn't notice the puzzle piece buried near the northeast corner.
static void labyrinth_81(double elapsed,int strix,int framec) {
  still(0,160,128,80);
}
static int labyrinth_condition_81() {
  // Deliver this cheesy hint as part of the cutscene, just to keep things interesting.
  // Hint if the jigpiece in question has not been acquired.
  if (store_get_jigstore(207)) return 0;
  return 1;
}

//82 And finally she found her way out.
static void labyrinth_82(double elapsed,int strix,int framec) {
  still(128,160,128,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_labyrinth[]={
  {labyrinth_77,77,0},
  {labyrinth_78,78,0},
  {labyrinth_79,79,0},
  {labyrinth_80,80,labyrinth_condition_80},
  {labyrinth_81,81,labyrinth_condition_81},
  {labyrinth_82,82,0},
{0}};

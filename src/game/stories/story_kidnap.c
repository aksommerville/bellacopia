#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_kidnap);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}
 
//50 Once upon a time there was a witch who went looking in the mountains for a root devil.
static void kidnap_50(double elapsed,int strix,int framec) {
  still(0,0,128,80);
}

//51 She stepped into a goblin trap!
static void kidnap_51(double elapsed,int strix,int framec) {
  still(128,0,128,80);
}

//52 The goblins took the witch into their cave, to lock her up until they're ready to eat her.
static void kidnap_52(double elapsed,int strix,int framec) {
  still(0,80,128,80);
}

//53 But there was somebody else there already! The goblins had kidnapped a princess earlier.
static void kidnap_53(double elapsed,int strix,int framec) {
  still(128,80,128,80);
}

//54 "\"Can you help me get out of here?\""
static void kidnap_54(double elapsed,int strix,int framec) {
  still(0,160,128,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_kidnap[]={
  {kidnap_50,50},
  {kidnap_51,51},
  {kidnap_52,52},
  {kidnap_53,53},
  {kidnap_54,54},
{0}};

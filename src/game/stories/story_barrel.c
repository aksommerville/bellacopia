#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_barrel);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//83 All throughout history, people have been putting things in barrels.
static void barrel_83(double elapsed,int strix,int framec) {
  still(0,0,256,80);
}

//84 It's good to pay those barrels back for their years of diligent service.
static void barrel_84(double elapsed,int strix,int framec) {
  still(0,80,256,80);
}

//85 So Dot and the knitter gave all the barrels a pretty new hat!
static void barrel_85(double elapsed,int strix,int framec) {
  still(0,160,256,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_barrel[]={
  {barrel_83,83,0},
  {barrel_84,84,0},
  {barrel_85,85,0},
{0}};

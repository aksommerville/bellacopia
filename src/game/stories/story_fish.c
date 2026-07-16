#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_fish);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//105 Of fishe there be three collours.
static void fish_105(double elapsed,int strix,int framec) {
  still(0,0,128,96);
}

//106 Ye greene fishe tasteth of grass and have meagre worth.
static void fish_106(double elapsed,int strix,int framec) {
  still(128,0,128,96);
}

//107 Ye blue fishe tasteth of fishe.
static void fish_107(double elapsed,int strix,int framec) {
  still(0,96,128,96);
}

//108 Ye precious redde fishe tasteth of rubies and have great worth.
static void fish_108(double elapsed,int strix,int framec) {
  still(128,96,128,96);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_fish[]={
  {fish_105,105,0},
  {fish_106,106,0},
  {fish_107,107,0},
  {fish_108,108,0},
{0}};

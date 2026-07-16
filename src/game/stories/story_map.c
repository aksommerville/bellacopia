#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_map);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//96 Once upon a time there was a witch who travelled the world.
static void map_96(double elapsed,int strix,int framec) {
  still(0,0,128,80);
}

//97 She trekked through the northern tundra.
static void map_97(double elapsed,int strix,int framec) {
  still(128,0,128,80);
}

//98 She explored the southern desert.
static void map_98(double elapsed,int strix,int framec) {
  still(0,80,128,80);
}

//99 She sweltered in the western jungle.
static void map_99(double elapsed,int strix,int framec) {
  still(128,80,128,80);
}

//100 And she climbed the eastern mountains.
static void map_100(double elapsed,int strix,int framec) {
  still(0,160,128,80);
}

//101 "\"But mostly the hard part was underground!\""
static void map_101(double elapsed,int strix,int framec) {
  still(128,160,128,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_map[]={
  {map_96,96,0},
  {map_97,97,0},
  {map_98,98,0},
  {map_99,99,0},
  {map_100,100,0},
  {map_101,101,0},
{0}};

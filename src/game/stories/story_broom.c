#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_broom);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//91 Most witches ride brooms.
static void broom_91(double elapsed,int strix,int framec) {
  still(0,0,128,80);
}

//92 Some of them ride fast, and race each other!
static void broom_92(double elapsed,int strix,int framec) {
  still(128,0,128,80);
}

//93 Broom racing is super dangerous because you crash into things a lot.
static void broom_93(double elapsed,int strix,int framec) {
  still(0,80,128,80);
}

//94 So it's a good idea to keep plenty of potion ready!
static void broom_94(double elapsed,int strix,int framec) {
  still(128,80,128,80);
}

//95 This story brought to you by the Potion Syndicate.
static void broom_95(double elapsed,int strix,int framec) {
  still(0,160,128,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_broom[]={
  {broom_91,91,0},
  {broom_92,92,0},
  {broom_93,93,0},
  {broom_94,94,0},
  {broom_95,95,0},
{0}};

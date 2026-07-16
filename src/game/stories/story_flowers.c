#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_flowers);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//109 Once upon a time there was a witch who planted seven flowers.
static void flowers_109(double elapsed,int strix,int framec) {
  still(0,0,128,80);
}

//110 But no matter what she did, the flowers wouldn't bloom.
static void flowers_110(double elapsed,int strix,int framec) {
  still(128,0,128,80);
}

//111 The book says: "Of floweres which bloometh not, ye likeliest cause be Roote-Devils."
static void flowers_111(double elapsed,int strix,int framec) {
  still(0,80,128,80);
}

//112 Of course! Root Devils!
static void flowers_112(double elapsed,int strix,int framec) {
  still(128,80,128,80);
}
static int flowers_condition_112() {
  return !store_get_fld(NS_fld_root_all);
}

//113 So she traced the flowers' roots and sure enough, there was a Root Devil strangling each one.
static void flowers_113(double elapsed,int strix,int framec) {
  still(0,160,128,80);
}
static int flowers_condition_113() {
  return store_get_fld(NS_fld_root_all);
}

//114 And with the Root Devils strangled, now Dot's flowers bloom in glorious color!
static void flowers_114(double elapsed,int strix,int framec) {
  still(128,160,128,80);
}
static int flowers_condition_114() {
  return store_get_fld(NS_fld_root_all);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_flowers[]={
  {flowers_109,109,0},
  {flowers_110,110,0},
  {flowers_111,111,0},
  {flowers_112,112,flowers_condition_112},
  {flowers_113,113,flowers_condition_113},
  {flowers_114,114,flowers_condition_114},
{0}};

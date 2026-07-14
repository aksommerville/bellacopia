#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_war);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//66 Once upon a time there were two armies at war.
static void war_66(double elapsed,int strix,int framec) {
  still(0,0,128,80);
}

//67 The captains sent treaties back and forth, but every letter just made them angrier.
static void war_67(double elapsed,int strix,int framec) {
  still(128,0,128,80);
}

//68 So a clever poet intercepted their letters and rewrote them with beautiful language.
static void war_68(double elapsed,int strix,int framec) {
  still(0,80,128,80);
}

//69 Upon reading the beautified letters, the captains were embarrassed by their bellicosity and resolved to make peace instead.
static void war_69(double elapsed,int strix,int framec) {
  still(128,80,128,80);
}

//70 "\"Better than a thousand hollow verses is one verse that brings peace.\"\n-Buddha"
static void war_70(double elapsed,int strix,int framec) {
  still(0,160,128,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_war[]={
  {war_66,66,0},
  {war_67,67,0},
  {war_68,68,0},
  {war_69,69,0},
  {war_70,70,0},
{0}};

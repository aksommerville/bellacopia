#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_hearts);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//115 Once upon a time there was a witch whose heart was too small to hold all the love she felt.
static void hearts_115(double elapsed,int strix,int framec) {
  still(0,0,80,80);
}

//116 So she bought more heart from a shop.
static void hearts_116(double elapsed,int strix,int framec) {
  still(80,0,80,80);
}

//117 And she found some heart at a temple.
static void hearts_117(double elapsed,int strix,int framec) {
  still(160,0,80,80);
}

//118 And she earned more heart from the teacher.
static void hearts_118(double elapsed,int strix,int framec) {
  still(0,80,80,80);
}

//119 And she dug up some heart from underground.
static void hearts_119(double elapsed,int strix,int framec) {
  still(80,80,80,80);
}

//120 And she did whatever hc5 is going to be. !!!TODO!!!
static void hearts_120(double elapsed,int strix,int framec) {
  still(160,80,80,80);
}

//121 Now Dot has lots and lots of heart!
static void hearts_121(double elapsed,int strix,int framec) {
  still(0,160,80,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_hearts[]={
  {hearts_115,115,0},
  {hearts_116,116,0},
  {hearts_117,117,0},
  {hearts_118,118,0},
  {hearts_119,119,0},
  {hearts_120,120,0},
  {hearts_121,121,0},
{0}};

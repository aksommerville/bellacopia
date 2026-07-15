#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_things);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//86 Once upon a time there was a witch with too many things.
static void things_86(double elapsed,int strix,int framec) {
  still(0,0,128,80);
}

//87 Things made out of wood...
static void things_87(double elapsed,int strix,int framec) {
  still(128,0,128,80);
}

//88 Things made out of glass...
static void things_88(double elapsed,int strix,int framec) {
  still(0,80,128,80);
}

//89 And some things that just don't make any sense.
static void things_89(double elapsed,int strix,int framec) {
  still(128,80,128,80);
}

//90 But it's OK, because she put them all in alphabetical order!
static void things_90(double elapsed,int strix,int framec) {
  still(0,160,128,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_things[]={
  {things_86,86,0},
  {things_87,87,0},
  {things_88,88,0},
  {things_89,89,0},
  {things_90,90,0},
{0}};

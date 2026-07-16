#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_shops);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//128 Potions are tasty and nutritious.
static void potion_128(double elapsed,int strix,int framec) {
  still(128,80,128,80);
}

//129 A good potion is made of carrots, tomatoes, and a bunch of secret ingredients.
static void potion_129(double elapsed,int strix,int framec) {
  still(0,160,128,80);
}

//130 One sip and you're ready to face the world again!
static void potion_130(double elapsed,int strix,int framec) {
  still(128,160,128,80);
}

/* Table of contents.
 */
 
const struct story_step story_stepv_potion[]={
  {potion_128,128,0},
  {potion_129,129,0},
  {potion_130,130,0},
{0}};

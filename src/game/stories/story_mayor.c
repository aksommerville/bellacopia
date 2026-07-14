#include "game/bellacopia.h"

/* Steps.
 */
 
static void still(int srcx,int srcy,int w,int h) {
  int dstx=(FBW>>1)-(w>>1);
  int dsty=(CUTSCENE_DIVIDE_Y>>1)-(h>>1);
  graf_set_image(&g.graf,RID_image_story_mayor);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
}

//55 Once upon a time there was a witch who needed to get past a giant log.
static void mayor_55(double elapsed,int strix,int framec) {
  still(0,0,80,64);
}

//56 Only the public works crew can move it, but they're all working on other things.
static void mayor_56(double elapsed,int strix,int framec) {
  still(80,0,80,64);
}

//57 So Dot decided to run for mayor!
static void mayor_57(double elapsed,int strix,int framec) {
  still(160,0,80,64);
}

//58 She convinced the hospital staff to support her.
static void mayor_58(double elapsed,int strix,int framec) {
  still(0,64,80,64);
}
static int mayor_condition_58() {
  return store_get_fld(NS_fld_endorse_hospital);
}

//59 She won over the local labor union.
static void mayor_59(double elapsed,int strix,int framec) {
  still(80,64,80,64);
}
static int mayor_condition_59() {
  return store_get_fld(NS_fld_endorse_labor);
}

//60 She got the support of the athletes' guild.
static void mayor_60(double elapsed,int strix,int framec) {
  still(160,64,80,64);
}
static int mayor_condition_60() {
  return store_get_fld(NS_fld_endorse_athlete);
}

//61 She made a crooked deal with the casino bosses.
static void mayor_61(double elapsed,int strix,int framec) {
  still(0,128,80,64);
}
static int mayor_condition_61() {
  return store_get_fld(NS_fld_endorse_casino);
}

//62 She made friends at the public sector employees' union.
static void mayor_62(double elapsed,int strix,int framec) {
  still(80,128,80,64);
}
static int mayor_condition_62() {
  return store_get_fld(NS_fld_endorse_public);
}

//63 She won the food service guild's support.
static void mayor_63(double elapsed,int strix,int framec) {
  still(160,128,80,64);
}
static int mayor_condition_63() {
  return store_get_fld(NS_fld_endorse_food);
}

//64 And with all these new friends helping her, Dot won the election!
static void mayor_64(double elapsed,int strix,int framec) {
  still(0,192,80,64);
}

//65 "First order of business: Let's move that log!"
static void mayor_65(double elapsed,int strix,int framec) {
  still(80,192,80,64);
}

/* Table of contents.
 * Most of our steps are conditional: We'll discuss all the endorsements you actually won.
 */
 
const struct story_step story_stepv_mayor[]={
  {mayor_55,55,0},
  {mayor_56,56,0},
  {mayor_57,57,0},
  {mayor_58,58,mayor_condition_58},
  {mayor_59,59,mayor_condition_59},
  {mayor_60,60,mayor_condition_60},
  {mayor_61,61,mayor_condition_61},
  {mayor_62,62,mayor_condition_62},
  {mayor_63,63,mayor_condition_63},
  {mayor_64,64,0},
  {mayor_65,65,0},
{0}};

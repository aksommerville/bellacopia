#include "bellacopia.h"

/* UUU: Flash.
 */
 
void spell_flash() {
  //TODO sound effect
  g.flash=FLASH_TIME;
}

/* DDD: Home.
 */
 
void spell_home() {
  game_warp(RID_map_start,NS_transition_fadeblack);
}

/* Cast spell, main entry point.
 */
 
int game_cast_spell(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  if ((srcc==3)&&!memcmp(src,"UUU",3)) { spell_flash(); return 1; }
  if ((srcc==3)&&!memcmp(src,"DDD",3)) { spell_home(); return 1; }
  
  return 0;
}

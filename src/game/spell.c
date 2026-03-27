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
  g.song_override_outerworld=0;
  game_warp(RID_map_start,NS_transition_fadeblack);
}

/* (dynamic): Translation.
 */
 
void spell_translation() {
  cryptmsg_translate_next();
}

/* Cast spell, main entry point.
 */
 
int game_cast_spell(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return 0;
  
  /* Static spells must not meet the Spell of Translating's criteria: 6 digits with each direction at least once.
   * So if you define a new spell of 6 digits, ensure that it use no more than 3.
   */
  if ((srcc==3)&&!memcmp(src,"UUU",3)) { spell_flash(); return 1; }
  if ((srcc==3)&&!memcmp(src,"DDD",3)) { spell_home(); return 1; }
  
  /* The Spell of Translating doesn't exist until you buy it.
   * Its length is always 6.
   * Don't require the store for this check. So if they haven't bought it yet, no harm and it won't match.
   */
  if (srcc==6) {
    char spell[8];
    int spellc=cryptmsg_get_spell(spell,sizeof(spell),0);
    if ((spellc==srcc)&&!memcmp(spell,src,srcc)) { spell_translation(); return 1; }
  }
  
  return 0;
}

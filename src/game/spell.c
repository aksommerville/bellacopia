#include "game.h"
#include "world/camera.h"

/* UUU: The Spell Of Light
 */
 
static int bm_spell_light() {
  camera_flash();
  return 1;
}

/* DDD: The Spell Of Home
 */
 
static int bm_spell_home() {
  camera_warp_home_soon();
  return 1;
}

/* Invalid spell.
 */
 
static int bm_cast_spell_fail() {
  bm_sound(RID_sound_reject,0.0);
  return 0;
}

/* Cast spell, main entry point.
 */
 
int bm_cast_spell(const char *src,int srcc) {
  if (!src) return bm_cast_spell_fail();
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  //fprintf(stderr,"%s: '%.*s'\n",__func__,srcc,src);
  switch (srcc) {
    case 3: {
        if (!memcmp(src,"UUU",3)) return bm_spell_light();
        if (!memcmp(src,"DDD",3)) return bm_spell_home();
      } break;
  }
  return bm_cast_spell_fail();
}

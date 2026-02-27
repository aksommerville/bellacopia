#ifndef BELLACOPIA_H
#define BELLACOPIA_H

#include "egg/egg.h"
#include "util/stdlib/egg-stdlib.h"
#include "util/res/res.h"
#include "util/text/text.h"
#include "util/graf/graf.h"
#include "util/font/font.h"
#include "shared_symbols.h"
#include "egg_res_toc.h"
#include "map.h"
#include "store.h"
#include "battle/battle.h"
#include "modal/modal.h"
#include "sprite/sprite.h"
#include "camera.h"
#include "game.h"
#include "feet.h"
#include "spawner.h"

#define SOUND_BLACKOUT_LIMIT 16
#define MODAL_LIMIT 8 /* Could do the stack dynamically, but a static stack also serves as a sanity check on its depth. */

/* Can't be more than 256, or the inner vellum size at modal_pause.c:_pause_render(). (288x153)
 * The largest assembled puzzle is 10x10 screens, so 200x120 pixels.
 */
#define JIGSAW_FLDW 236
#define JIGSAW_FLDH 153

#define SPELL_LIMIT 8
#define FLASH_TIME 1.000

/* Globals.
 ****************************************************************/

extern struct g {
  
  void *rom; // verbatim
  int romc;
  struct rom_entry *resv; // only selected types; see res.c
  int resc;
  struct tilesheet {
    int rid;
    uint8_t *physics;
    uint8_t *jigctab;
  } *tilesheetv;
  int tilesheetc,tilesheeta;
  struct mapstore mapstore;
  
  int input[3],pvinput[3];
  
  int song_playing;
  struct sound_blackout {
    double when;
    int rid;
  } sound_blackoutv[SOUND_BLACKOUT_LIMIT];
  int sound_blackoutc;
  
  struct graf graf;
  struct font *font;
  int framec;
  
  struct modal *modalv[MODAL_LIMIT];
  int modalc;
  struct modal *modal_focus; // WEAK, possibly dead. Only modal.c should touch it.
  
  struct store store;
  struct sprites sprites;
  struct camera camera;
  struct feet feet;
  struct spawner spawner;
  double bugspray; // Counts down, not persisted.
  double vanishing; // ''
  double flash; // ''
  int warp_listener;
  int gameover; // Set nonzero to end story-mode play.
  int telescoping; // Telescope in use. Some viewport-sensitive ops get suspended.
} g;

/* Misc global API.
 *********************************************************************/

/* Maps are stored generically, but you probably want mapstore in map.h.
 */
int res_init();
int res_search(int tid,int rid);
int res_get(void *dstpp,int tid,int rid);
int tilesheet_search(int rid);
const uint8_t *tilesheet_get_physics(int rid); // never null
const uint8_t *tilesheet_get_jigctab(int rid); // never null

/* Globally we use songid 1 and 2.
 * Battles are free to use 3 and above.
 */
void bm_song_force(int rid); // If not playing now, cut the current one and start this.
void bm_song_gently(int rid); // For maps, we'll eventually do a crossfade and maybe playhead capture.
void bm_sound_pan(int rid,double pan);
static inline void bm_sound(int rid) { bm_sound_pan(rid,0.0); }

#endif

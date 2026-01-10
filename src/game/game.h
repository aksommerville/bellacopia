#ifndef GAME_H
#define GAME_H

#include "egg/egg.h"
#include "util/stdlib/egg-stdlib.h"
#include "util/res/res.h"
#include "util/graf/graf.h"
#include "util/font/font.h"
#include "util/text/text.h"
#include "shared_symbols.h"
#include "egg_res_toc.h"
#include "modal/modal.h"
#include "battle/battle.h"
#include "store.h"

struct sprite;

#define FBW 320
#define FBH 180

#define SOUND_BLACKOUT_LIMIT 16
#define MODAL_LIMIT 8

#define INVENTORY_SIZE 25 /* Must yield an agreeable rectangle for presentation. 5x5. */

extern struct g {
  void *rom;
  int romc;
  struct rom_entry *resv;
  int resc,resa;
  struct graf graf;
  int pvtexevictc; // Tracks (graf.texevictc) to detect churn.
  int texchurnc; // Consecutive frames with a change to (graf.texevictc), for detecting churn.
  struct font *font;
  int song_playing;
  struct sound_blackout {
    int rid;
    double time;
  } sound_blackoutv[SOUND_BLACKOUT_LIMIT];
  struct modal *modalv[MODAL_LIMIT];
  int modalc;
  int input[3],pvinput[3]; // We track the aggregate and players 1 and 2, at all times.
  int framec;
  
  struct {
    int battletype;
    int playerc;
    int handicap;
    void (*cb)(void *userdata,int outcome);
    void *userdata;
  } deferred_battle;
  
  struct inventory {
    int itemid; // 0 if slot vacant. 0..255
    int limit; // 0 if not counted. 0..16383
    int quantity; // 0..limit. If limit==0, we can abuse these 14 bits for additional state. eg "sword broken"?
  } inventoryv[INVENTORY_SIZE];
  struct inventory equipped; // The equipped item is just another inventory slot; this item is not in your backpack.
  int inventory_dirty;
} g;

/* The song mentioned by bm_song() is exclusive, it stops the old one.
 * Game code may use (songid>1) for additional concurrent songs.
 */
void bm_song(int rid,int repeat);
void bm_sound(int rid,float pan);

int res_init();
int res_search(int tid,int rid);
int res_get(void *dstpp,int tid,int rid);
const uint8_t *res_get_physics_table(int tilesheetid); // => 256 bytes, never null
const uint8_t *res_get_jigctab_table(int tilesheetid); // ''

/* Preferred entry point for battles.
 * We'll validate, push the modal, and return >=0.
 * (handicap) in 0..128..255 = favor hero .. neutral .. favor foe. (with "foe" meaning player 2 in multiplayer mode).
 */
int bm_begin_battle(
  int battletype,int playerc,int handicap,
  void (*cb)(void *userdata,int outcome),
  void *userdata
);
void bm_begin_battle_soon(
  int battletype,int playerc,int handicap,
  void (*cb)(void *userdata,int outcome),
  void *userdata
);

/* verbiage.c
 * These manage the complex language and formatting rules.
 */
int verbiage_begin_battle(char *dst,int dsta,const struct battle_type *type);

int bm_achievements_generate(); // => texid, always substantially smaller than the framebuffer (will fit in a pause-modal page).

/* inventory.c
 * tileid_for_item() returns zero if none, otherwise a tile in RID_image_pause.
 * It takes (quantity) in case we use that as extra state for certain items. Normal quantities are never involved.
 */
void inventory_reset();
int inventory_save_if_dirty();
uint8_t tileid_for_item(int itemid,int quantity); // RID_image_pause
uint8_t hand_tileid_for_item(int itemid,int quantity); // RID_image_hero
struct inventory *inventory_search(int itemid); // Null if we don't have it.
int inventory_acquire(int itemid,int quantity); // Nonzero if accepted. Does all the sound effects, storage, modals, whatever.
void strings_for_item(int *strix_name,int *strix_desc,int itemid); // RID_strings_dialogue

void bm_begin_activity(int activity,struct sprite *subject);

#endif

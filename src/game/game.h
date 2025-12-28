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

#define FBW 320
#define FBH 180

#define SOUND_BLACKOUT_LIMIT 16
#define MODAL_LIMIT 8

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
  
  struct {
    int battletype;
    int playerc;
    int handicap;
    void (*cb)(void *userdata,int outcome);
    void *userdata;
  } deferred_battle;
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

#endif

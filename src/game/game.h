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

#define FBW 320
#define FBH 180

#define SOUND_BLACKOUT_LIMIT 16

extern struct g {
  void *rom;
  int romc;
  struct rom_entry *resv;
  int resc,resa;
  struct graf graf;
  int song_playing;
  struct sound_blackout {
    int rid;
    double time;
  } sound_blackoutv[SOUND_BLACKOUT_LIMIT];
} g;

/* The song mentioned by bm_song() is exclusive, it stops the old one.
 * Game code may use (songid>1) for additional concurrent songs.
 */
void bm_song(int rid,int repeat);
void bm_sound(int rid,float pan);

int res_init();
int res_search(int tid,int rid);
int res_get(void *dstpp,int tid,int rid);

#endif

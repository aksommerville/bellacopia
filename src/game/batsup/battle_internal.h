/* battle_internal.h
 * Alternate header for battles.
 * They don't get globals or shared_symbols by default.
 */
 
#ifndef BATTLE_INTERNAL_H
#define BATTLE_INTERNAL_H

#include "egg/egg.h"
#include "util/stdlib/egg-stdlib.h"
#include "util/text/text.h"
#include "util/graf/graf.h"
#include "util/font/font.h"
#include "util/res/res.h"
#include "egg_res_toc.h"
#include "battle.h"

/* A few things from shared_symbols that we'll duplicate.
 */
#define FBW 320
#define FBH 180
#define NS_sys_tilesize 16
#define NS_sys_mapw 20
#define NS_sys_maph 12
#define NS_face_monster 0 /* "The monster", whatever the game prefers. */
#define NS_face_dot 1
#define NS_face_princess 2
#define NS_face_moonsong 3 /* For open world races only. */

#define PLAYER_PAN 0.400
void bm_sound_pan(int rid,double pan);
int res_search(int tid,int rid);
int res_get(void *dstpp,int tid,int rid);

/* Selections from (struct g) that battles might need.
 */
extern struct graf *g_graf;
extern struct font *g_font;
extern int *g_input;
extern int *g_pvinput;
extern int g_framec;

#endif

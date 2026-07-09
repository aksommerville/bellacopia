/* activity_internal.h
 * "Activity" are business-level events that can be triggered usually by NPC sprites.
 * They are enumerated in shared_symbols.h as NS_activity_*.
 *
 * We have two public entry points declared in game.h:
 *  void game_begin_activity(int activity,int arg,struct sprite *initiator);
 *  int game_activity_sprite_should_abort(int activity,const struct sprite_type *type);
 */
 
#ifndef ACTIVITY_INTERNAL_H
#define ACTIVITY_INTERNAL_H

#include "game/bellacopia.h"

// activity.c
struct modal *begin_dialogue(int arg,struct sprite *initiator); // RID_strings_dialogue

// activity_shops.c
void begin_carpenter(struct sprite *initiator);
void begin_brewer(struct sprite *initiator); // No initiator.
void begin_brewer_single(struct sprite *initiator,int exorbitance); // No initiator.
void begin_potion_book(struct sprite *initiator); // No initiator.
void begin_bloodbank(struct sprite *initiator,int arg);
void begin_fishwife(struct sprite *initiator);
void begin_magneticnorth(struct sprite *initiator);
void begin_thingwalla(struct sprite *initiator);
void begin_fishprocessor(struct sprite *initiator);
void begin_castleshop(struct sprite *initiator);
void begin_templeshop(struct sprite *initiator);
void begin_inconvenience(struct sprite *initiator);
void begin_fish_book();
void begin_crocodile();
void begin_medomat();

// activity_sidequests.c
void begin_tolltroll(struct sprite *initiator,int arg);
void begin_knitter(struct sprite *initiator);
void begin_tree(struct sprite *initiator,int arg);
void begin_generic_tolltroll(struct sprite *initiator,int arg);
void begin_invcritic(struct sprite *initiator);
void begin_bridget(struct sprite *initiator,int arg);
void begin_moonsong(struct sprite *initiator,int arg);
void begin_endrace(int arg);
void begin_hearts_book();
void begin_gold_book();
void begin_zoo_replay(struct sprite *sprite,int fld);
void begin_surveyor_entry(struct sprite *sprite,int fld16);

// activity_advice.c
void begin_crystal();
void begin_cartographer(struct sprite *initiator);

// activity_princess.c
void begin_king(struct sprite *initiator);
void begin_jaildoor();
void begin_kidnap(struct sprite *initiator);
void begin_escape();
void begin_cryptmsg(int arg);
void begin_linguist(struct sprite *initiator);
void begin_exit_cave(struct sprite *hero); // Door activities get the hero as initiator.

// activity_fractia.c
void begin_logproblem1(struct sprite *initiator);
void begin_logproblem2(struct sprite *initiator);
void begin_board_of_elections(struct sprite *initiator);

// activity_cheat.c
void begin_cheat_store(struct sprite *initiator,int arg);
void begin_cheat_giveaway(struct sprite *initiator,int arg);
void begin_cheatstories();

// activity_war.c
void begin_capnred(struct sprite *initiator);
void begin_capnblue(struct sprite *initiator);
void begin_poet(struct sprite *initiator);

// activity_misc.c
void begin_phonograph();
void begin_busstop(int arg);
void begin_pauserace();
void begin_reset_puzzle(struct sprite *initiator,int fldid);

// activity_dialogue.c
void begin_statuemaze_clue(struct sprite *initiator,int arg);

#endif

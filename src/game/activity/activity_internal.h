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
struct modal *begin_dialogue(int arg,struct sprite *initiator);

// activity_shops.c
void begin_carpenter(struct sprite *initiator);
void begin_brewer(struct sprite *initiator);
void begin_bloodbank(struct sprite *initiator,int arg);
void begin_fishwife(struct sprite *initiator);
void begin_magneticnorth(struct sprite *initiator);
void begin_thingwalla(struct sprite *initiator);
void begin_fishprocessor(struct sprite *initiator);
void begin_castleshop(struct sprite *initiator);
void begin_templeshop(struct sprite *initiator);

// activity_sidequests.c
void begin_tolltroll(struct sprite *initiator,int arg);
void begin_knitter(struct sprite *initiator);
void begin_tree(struct sprite *initiator,int arg);
void begin_generic_tolltroll(struct sprite *initiator,int arg);

// activity_princess.c
void begin_king(struct sprite *initiator);
void begin_jaildoor();
void begin_kidnap(struct sprite *initiator);
void begin_escape();
void begin_cryptmsg(int arg);
void begin_linguist(struct sprite *initiator);

// activity_fractia.c
void begin_logproblem1(struct sprite *initiator);
void begin_logproblem2(struct sprite *initiator);
void begin_board_of_elections(struct sprite *initiator);

// activity_cheat.c
void begin_cheat_store(struct sprite *initiator,int arg);
void begin_cheat_giveaway(struct sprite *initiator,int arg);

// activity_war.c
void begin_capnred(struct sprite *initiator);
void begin_capnblue(struct sprite *initiator);
void begin_poet(struct sprite *initiator);

// activity_misc.c
void begin_phonograph();

#endif

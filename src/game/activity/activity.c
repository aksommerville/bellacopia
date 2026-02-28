#include "activity_internal.h"

/* Generic read-only dialogue.
 */
 
struct modal *begin_dialogue(int strix,struct sprite *speaker) {
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=strix,
    .speaker=speaker,
  };
  return modal_spawn(&modal_type_dialogue,&args,sizeof(args));
}

/* Begin activity.
 */
 
void game_begin_activity(int activity,int arg,struct sprite *initiator) {
  //fprintf(stderr,"%s activity=%d arg=%d initiator=%p\n",__func__,activity,arg,initiator);
  switch (activity) {
    case NS_activity_dialogue: begin_dialogue(arg,initiator); break;
    case NS_activity_carpenter: begin_carpenter(initiator); break;
    case NS_activity_brewer: begin_brewer(initiator); break;
    case NS_activity_bloodbank: begin_bloodbank(initiator,arg); break;
    case NS_activity_fishwife: begin_fishwife(initiator); break;
    case NS_activity_tolltroll: begin_tolltroll(initiator,arg); break;
    case NS_activity_wargate: begin_dialogue(19,initiator); break; // It's just dialogue. But the activity causes sprite to abort at spawn.
    case NS_activity_knitter: begin_knitter(initiator); break;
    case NS_activity_magneticnorth: begin_magneticnorth(initiator); break;
    case NS_activity_thingwalla: begin_thingwalla(initiator); break;
    case NS_activity_king: begin_king(initiator); break;
    case NS_activity_fishprocessor: begin_fishprocessor(initiator); break;
    case NS_activity_jaildoor: begin_jaildoor(); break;
    case NS_activity_kidnap: begin_kidnap(initiator); break;
    case NS_activity_escape: begin_escape(); break;
    case NS_activity_cryptmsg: begin_cryptmsg(arg); break;
    case NS_activity_linguist: begin_linguist(initiator); break;
    case NS_activity_logproblem1: begin_logproblem1(initiator); break;
    case NS_activity_logproblem2: begin_logproblem2(initiator); break;
    case NS_activity_board_of_elections: begin_board_of_elections(initiator); break;
    case NS_activity_cheat_store: begin_cheat_store(initiator,arg); break;
    case NS_activity_cheat_giveaway: begin_cheat_giveaway(initiator,arg); break;
    case NS_activity_capnred: begin_capnred(initiator); break;
    case NS_activity_capnblue: begin_capnblue(initiator); break;
    case NS_activity_poet: begin_poet(initiator); break;
    case NS_activity_enter_labyrinth: labyrinth_reset(); break;
    case NS_activity_tree: begin_tree(initiator,arg); break;
    case NS_activity_override_outerworld_song: g.song_override_outerworld=1; break; // These are triggered by doors, so can't use (arg).
    case NS_activity_unoverride_outerworld_song: g.song_override_outerworld=0; break;
    case NS_activity_castleshop: begin_castleshop(initiator); break;
    case NS_activity_templeshop: begin_templeshop(initiator); break;
    case NS_activity_generic_tolltroll: begin_generic_tolltroll(initiator,arg); break;
    default: {
        fprintf(stderr,"Unknown activity %d.\n",activity);
      }
  }
}

/* Abort sprite?
 */
 
int game_activity_sprite_should_abort(int activity,const struct sprite_type *type) {
  switch (activity) {
    case NS_activity_wargate: if (store_get_fld(NS_fld_war_over)) return 1; return 0;
  }
  return 0;
}

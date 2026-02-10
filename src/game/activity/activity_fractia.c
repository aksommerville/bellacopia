/* activity_fractia.c
 * Activities around running for Mayor of Fractia.
 */
 
#include "activity_internal.h"

/* Log Problems, Volumes One Through Two: Bystanders in Fractia that complain about the log.
 */
 
void begin_logproblem1(struct sprite *sprite) {
  if (store_get_fld(NS_fld_mayor)) begin_dialogue(63,sprite);
  else if (store_get_fld(NS_fld_election_start)) begin_dialogue(62,sprite);
  else begin_dialogue(61,sprite);
}

void begin_logproblem2(struct sprite *sprite) {
  if (store_get_fld(NS_fld_mayor)) begin_dialogue(65,sprite);
  else begin_dialogue(64,sprite);
}

/* Board of Elections.
 */

// Callback when asking do you want to run for mayor.
static int cb_board_of_elections_register(int optionid,void *userdata) {
  if (optionid==4) { // "yes"
    bm_sound(RID_sound_uiactivate);
    store_set_fld(NS_fld_election_start,1);
    return 1;
  }
  return 0;
}

// Callback when asking do you want to hold the election.
static int cb_board_of_elections_vote(int optionid,void *userdata) {
  if (optionid==4) { // "yes"
    bm_sound(RID_sound_uiactivate);
    fprintf(stderr,"*** %s:%d:%s:TODO: Begin election battle ***\n",__FILE__,__LINE__,__func__);
    store_set_fld(NS_fld_mayor,1);//XXX
    return 1;
  }
  return 0;
}
 
void begin_board_of_elections(struct sprite *sprite) {
  // Already mayor, no more need for elections.
  if (store_get_fld(NS_fld_mayor)) {
    begin_dialogue(68,sprite);
    return;
  }
  // Propose running for mayor, or holding the election.
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=66,
    .speaker=sprite,
    .cb=cb_board_of_elections_register,
  };
  if (store_get_fld(NS_fld_election_start)) {
    args.strix=67;
    args.cb=cb_board_of_elections_vote;
  }
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,4);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,5);
}

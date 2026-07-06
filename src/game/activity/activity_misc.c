#include "activity_internal.h"
#include "game/race/race.h"

/* Phonograph.
 */
 
static int cb_phonograph(int rid,void *userdata) {
  store_set_fld16(NS_fld16_phonograph,rid);
  if (!g.song_override_outerworld&&map_is_outerworld(g.camera.map)) {
    bm_song_gently(bm_song_for_outerworld());
  }
  return 1;
}
 
void begin_phonograph() {
  struct modal_args_dialogue args={
    .rid=RID_strings_item,
    .strix=85,
    .cb=cb_phonograph,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  struct song_name_and_rid songv[10];
  int songc=bm_get_available_songs(songv,10);
  if (songc<0) songc=0; else if (songc>10) songc=10;
  struct song_name_and_rid *song=songv;
  for (;songc-->0;song++) {
    modal_dialogue_add_option_string_id(modal,RID_strings_item,song->name,song->rid);
  }
  modal_dialogue_set_default(modal,g.song_playing);
}

/* Bus stop.
 * This is just when you look at a fixed bus stop, it doesn't do anything but talk.
 */
 
static const struct busstop_metadata {
  int busstop;
  int strix; // RID_strings_item
} busstop_metadatav[]={
  {NS_busstop_cheapside,     94},
  {NS_busstop_fractia,       95},
  {NS_busstop_temple,        96},
  {NS_busstop_castle,        97},
  {NS_busstop_magneticnorth, 98},
  {NS_busstop_botire,        99},
};
 
void begin_busstop(int busstop) {
  int strix=0;
  const struct busstop_metadata *metadata=busstop_metadatav;
  int i=sizeof(busstop_metadatav)/sizeof(struct busstop_metadata);
  for (;i-->0;metadata++) {
    if (metadata->busstop!=busstop) continue;
    strix=metadata->strix;
    break;
  }
  if (!strix) {
    fprintf(stderr,"%s:%d: No metadata for busstop %d.\n",__FILE__,__LINE__,busstop);
    return;
  }
  struct modal_args_dialogue args={
    .rid=RID_strings_item,
    .strix=strix,
  };
  modal_spawn(&modal_type_dialogue,&args,sizeof(args));
}

int busstop_by_index(int p) {
  if (p<0) return 0;
  int c=sizeof(busstop_metadatav)/sizeof(struct busstop_metadata);
  if (p>=c) return 0;
  return busstop_metadatav[p].busstop;
}

int busstop_name_by_index(int p) {
  if (p<0) return 0;
  int c=sizeof(busstop_metadatav)/sizeof(struct busstop_metadata);
  if (p>=c) return 0;
  return busstop_metadatav[p].strix;
}

/* Pause, while race running.
 * This isn't involved enough to deserve its own modal, like regular pause.
 */
 
static int pauserace_cb(int optionid,void *userdata) {
  if (optionid==141) {
    race_end();
  }
  return 0;
}
 
void begin_pauserace() {
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=139,
    .cb=pauserace_cb,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,140);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,141);
}

/* "Reset puzzle". Creating for diegetic minesweeper, but intending to be usable generically.
 */
 
static int reset_puzzle_cb(int optionid,void *userdata) {
  if (optionid!=4) return 0;
  int fldid=(int)(uintptr_t)userdata;
  store_set_fld(fldid,0);
  store_broadcast('s',NS_signal_reset_puzzle,fldid);
  return 0;
}
 
void begin_reset_puzzle(struct sprite *initiator,int fldid) {
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=151,
    .cb=reset_puzzle_cb,
    .userdata=(void*)(uintptr_t)fldid,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,5);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,4);
}

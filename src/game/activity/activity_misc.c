#include "activity_internal.h"

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

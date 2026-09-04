/* modal_raceconfig.c
 * A menu that configures Broom Race Mode.
 *
 * TODO Choose player count and raceid interactively.
 * For now, I'm more interested in the actual race part, so the config is hard-coded.
 */
 
#include "game/bellacopia.h"

struct modal_raceconfig {
  struct modal hdr;
};

#define MODAL ((struct modal_raceconfig*)modal)

/* Delete.
 */
 
static void _raceconfig_del(struct modal *modal) {
}

/* Init.
 */
 
static int _raceconfig_init(struct modal *modal,const void *args,int argslen) {
  modal->opaque=1;
  modal->interactive=1;
  return 0;
}

/* Dismiss.
 */
 
static void raceconfig_dismiss(struct modal *modal) {
  bm_sound(RID_sound_uicancel);
  modal->defunct=1;
}

/* Activate.
 */
 
static void raceconfig_activate(struct modal *modal) {
  fprintf(stderr,"%d %s\n",(int)egg_time_real(),__func__);
  struct modal_args_broomrace args={
    //.playerc=1,//XXX
    .playerc=2,
    .raceid=RID_race_round_the_meadow,
    //.raceid=RID_race_downstairs_lake,
    //.raceid=RID_race_across_the_tundra,
    //.raceid=RID_race_seaside_circle,
    //.raceid=RID_race_desert_run,
    //.raceid=RID_race_undernorth,
  };
  struct modal *race=modal_spawn(&modal_type_broomrace,&args,sizeof(args));
  if (!race) {
    bm_sound(RID_sound_reject);
    return;
  }
}

/* Update.
 */
 
static void _raceconfig_update(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) raceconfig_dismiss(modal);
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) raceconfig_activate(modal);
}

/* Focus.
 */
 
static void _raceconfig_focus(struct modal *modal,int focus) {
  if (focus) {
    bm_song_force(RID_song_break_soil);
  }
}

/* Render.
 */
 
static void _raceconfig_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x102030ff);
}

/* Type definition.
 */
 
const struct modal_type modal_type_raceconfig={
  .name="raceconfig",
  .objlen=sizeof(struct modal_raceconfig),
  .del=_raceconfig_del,
  .init=_raceconfig_init,
  .update=_raceconfig_update,
  .focus=_raceconfig_focus,
  .render=_raceconfig_render,
};

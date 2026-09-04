/* modal_broomrace.c
 * Head to head or time trial broom race, entered via the main menu.
 * This is not involved in broom races launched in game by talking to Moon Song.
 * (tho of course we'll try to share as much as possible with those).
 */
 
#include "game/bellacopia.h"
#include "game/race/race.h"

struct modal_broomrace {
  struct modal hdr;
  int playerc;
  int raceid;
  int map_listener;
  int cell_listener;
  int store_listener;
};

#define MODAL ((struct modal_broomrace*)modal)

/* Cleanup.
 */
 
static void _broomrace_del(struct modal *modal) {
  camera_unlisten(MODAL->map_listener);
  camera_unlisten(MODAL->cell_listener);
  store_unlisten(MODAL->store_listener);
}

/* Map exposure callbacks.
 */
 
static void broomrace_cb_map_exposure(struct map *map,int focus,void *userdata) {
  struct modal *modal=userdata;
  switch (focus) {
    // If we need them: -1 is losing focus, and 0 is going out of view.
    case 1: game_welcome_map(map); break;
    case 2: game_focus_map(map); break;
  }
}

static void broomrace_cb_cell_exposure(int x,int y,int w,int h,void *userdata) {
  struct modal *modal=userdata;
  //spawner_expose(x,y,w,h); // XXX I think we should not spawn any monsters. But it's available if we want.
}

/* Store change callback.
 */
 
static void broomrace_cb_store(char type,int id,int value,void *userdata) {
  struct modal *modal=userdata;
  // Refresh the overlay any time anything at all changes in the store.
  // That's more than strictly necessary, but it's not worth burning a lot of cycles to figure out which changes matter.
  // (and the truth is, we probably will miss some broadcasts, so it's good to sync up before too much time passes).
  // The point of caching the overlay is just to prevent redrawing 60 times every second. Once per second is perfectly fine.
  //MODAL->overlay.dirty=1; // XXX broomrace probably doesn't even need a store listener
}

/* Init.
 */
 
static int _broomrace_init(struct modal *modal,const void *args,int argslen) {
  if (!args||(argslen!=sizeof(struct modal_args_broomrace))) return -1;
  const struct modal_args_broomrace *ARGS=args;
  modal->opaque=1;
  modal->interactive=1;
  MODAL->playerc=ARGS->playerc;
  MODAL->raceid=ARGS->raceid;
  
  fprintf(stderr,"%s: playerc=%d raceid=%d\n",__func__,MODAL->playerc,MODAL->raceid);
  
  if (game_reset(1)<0) return -1;
  
  /*TODO We're going to need an alternative to camera, for two important reasons:
   * - 1. We have an optional split-screen mode. Rendering should be entirely dynamic, no "current position" state.
   * - 2. We need to skip things like changing music.
   */
  
  if ((MODAL->map_listener=camera_listen_map(broomrace_cb_map_exposure,modal))<0) return -1;//XXX
  if ((MODAL->cell_listener=camera_listen_cell(broomrace_cb_cell_exposure,modal))<0) return -1;//XXX
  if ((MODAL->store_listener=store_listen(0,broomrace_cb_store,modal))<0) return -1;//XXX
  
  int x=0,y=0,mapid=0;
  if ((mapid=race_get_start_position(&x,&y,MODAL->raceid))<1) return -1;
  camera_cut(mapid,x,y,NS_transition_cut);//XXX
  camera_update(0.0); // Must update once to effect the cut. XXX
  if (race_begin(MODAL->raceid,MODAL->playerc)<0) return -1;
  
  return 0;
}

/* Update.
 */
 
static void _broomrace_update(struct modal *modal,double elapsed) {

  //XXX TEMP: AUX2 to quit.
  if ((g.input[0]&EGG_BTN_AUX2)&&!(g.pvinput[0]&EGG_BTN_AUX2)) modal->defunct=1;
  
  int i=GRP(update)->sprc;
  while (i-->0) {
    struct sprite *sprite=GRP(update)->sprv[i];
    if (sprite->defunct) continue;
    if (sprite->type->update) {
      sprite->type->update(sprite,elapsed);
    }
  }
  sprite_group_kill_all(GRP(deathrow));
  
  race_update(elapsed);
  game_update(elapsed);
  camera_update(elapsed);//XXX
}

/* Render.
 */
 
static void _broomrace_render(struct modal *modal) {
  camera_render();//XXX
  race_render_overlay();
}

/* Type definition.
 */
 
const struct modal_type modal_type_broomrace={
  .name="broomrace",
  .objlen=sizeof(struct modal_broomrace),
  .del=_broomrace_del,
  .init=_broomrace_init,
  .update=_broomrace_update,
  .render=_broomrace_render,
};

/* Completion, reported by race.c
 */
 
void modal_broomrace_report_completion(struct modal *modal,const struct race_status *status) {
  if (!modal||(modal->type!=&modal_type_broomrace)) return;
  modal->defunct=1;
  //TODO Report (status) back to our owner, raceconfig.
  if (status) {
    fprintf(stderr,"%s: lap=%f race=%f opponent=%f(%s)\n",__func__,status->laptime,status->racetime,status->opponenttime,status->opponent_finished?"finished":"incomplete");
  } else {
    fprintf(stderr,"%s: Status unavailable\n",__func__);
  }
}

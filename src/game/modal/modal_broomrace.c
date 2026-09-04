/* modal_broomrace.c
 * Head to head or time trial broom race, entered via the main menu.
 * This is not involved in broom races launched in game by talking to Moon Song.
 * (tho of course we'll try to share as much as possible with those).
 */
 
#include "game/bellacopia.h"
#include "game/race/race.h"
#include "game/race/multicamera.h"

struct modal_broomrace {
  struct modal hdr;
  int playerc;
  int raceid;
};

#define MODAL ((struct modal_broomrace*)modal)

/* Cleanup.
 */
 
static void _broomrace_del(struct modal *modal) {
  multicamera_quit();
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
  
  if (game_reset(1)<0) return -1;
  
  int x=0,y=0,mapid=0;
  if ((mapid=race_get_start_position(&x,&y,MODAL->raceid))<1) return -1;
  if (race_begin(MODAL->raceid,MODAL->playerc)<0) return -1;
  if (multicamera_init(MODAL->playerc)<0) return -1;
  multicamera_update(0.0); // Ensure we have sensible camera positions even if the first update gets skipped.
  
  return 0;
}

/* Update.
 */
 
static void _broomrace_update(struct modal *modal,double elapsed) {
  
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
  multicamera_update(elapsed);
}

/* Render.
 */
 
static void _broomrace_render(struct modal *modal) {
  multicamera_render();
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

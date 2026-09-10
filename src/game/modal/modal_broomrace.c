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

/* Expose map.
 */
 
static void broomrace_cb_expose(struct map *map,void *userdata) {
  struct modal *modal=userdata;
  struct map_extras extras={0};
  map_freshen_tiles(map,&extras);
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_sprite: {
          double x=map->lng*NS_sys_mapw+cmd.arg[0]+0.5;
          double y=map->lat*NS_sys_maph+cmd.arg[1]+0.5;
          int rid=(cmd.arg[2]<<8)|cmd.arg[3];
          const uint8_t *arg=cmd.arg+4;
          
          // There's a few sprites that we explicitly ignore.
          if (rid==RID_sprite_hero) continue;
          
          // And if somehow it was already spawned -- unlikely -- ignore it.
          if (find_sprite_by_arg(arg)) continue;
          
          struct sprite *sprite=sprite_spawn(x,y,rid,arg,4,0,0,0);
        } break;
    }
  }
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
  if (multicamera_init(MODAL->playerc,broomrace_cb_expose,modal)<0) return -1;
  multicamera_update(0.0); // Ensure we have sensible camera positions even if the first update gets skipped.
  
  if (MODAL->playerc==2) {
    modal_pickside_require();
  }
  
  return 0;
}

/* Update.
 */
 
static void _broomrace_update(struct modal *modal,double elapsed) {

  if ((g.input[0]&EGG_BTN_AUX1)&&!(g.pvinput[0]&EGG_BTN_AUX1)) {
    game_begin_activity(NS_activity_pauserace,MODAL->playerc,0);
  }
  
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

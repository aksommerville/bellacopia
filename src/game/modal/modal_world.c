#include "game/game.h"
#include "game/world/map.h"
#include "game/world/camera.h"
#include "game/sprite/sprite.h"

struct modal_world {
  struct modal hdr;
};

#define MODAL ((struct modal_world*)modal)

/* Cleanup.
 */
 
static void _world_del(struct modal *modal) {
}

/* Init.
 */
 
static int _world_init(struct modal *modal) {
  modal->opaque=1;
  modal->interactive=1;
  if (camera_reset(RID_map_start)<0) return -1;
  return 0;
}

/* Update.
 */
 
static void _world_update(struct modal *modal,double elapsed) {
  sprites_update(elapsed);
  camera_update(elapsed);
  
  if (g.deferred_battle.battletype) {
    bm_begin_battle(g.deferred_battle.battletype,g.deferred_battle.playerc,g.deferred_battle.handicap,g.deferred_battle.cb,g.deferred_battle.userdata);
    memset(&g.deferred_battle,0,sizeof(g.deferred_battle));
  }
}

/* Render.
 */
 
static void _world_render(struct modal *modal) {
  camera_render();
}

/* Type definition.
 */
 
const struct modal_type modal_type_world={
  .name="world",
  .objlen=sizeof(struct modal_world),
  .del=_world_del,
  .init=_world_init,
  .update=_world_update,
  .render=_world_render,
};

/* Public ctor.
 */
 
struct modal *modal_new_world() {
  struct modal *modal=modal_new(&modal_type_world);
  if (!modal) return 0;
  if (modal_push(modal)<0) {
    modal_del(modal);
    return 0;
  }
  //TODO further init?
  return modal;
}

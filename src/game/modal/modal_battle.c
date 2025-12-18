#include "game/game.h"

//TODO this is a stub until we decide how minigames works.

struct modal_battle {
  struct modal hdr;
};

#define MODAL ((struct modal_battle*)modal)

/* Cleanup.
 */
 
static void _battle_del(struct modal *modal) {
}

/* Init.
 */
 
static int _battle_init(struct modal *modal) {
  modal->opaque=1;
  modal->interactive=1;
  return 0;
}

/* Update.
 */
 
static void _battle_update(struct modal *modal,double elapsed) {
}

/* Render.
 */
 
static void _battle_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x800000ff);
}

/* Type definition.
 */
 
const struct modal_type modal_type_battle={
  .name="battle",
  .objlen=sizeof(struct modal_battle),
  .del=_battle_del,
  .init=_battle_init,
  .update=_battle_update,
  .render=_battle_render,
};

/* Public ctor.
 */
 
struct modal *modal_new_battle() {
  struct modal *modal=modal_new(&modal_type_battle);
  if (!modal) return 0;
  if (modal_push(modal)<0) {
    modal_del(modal);
    return 0;
  }
  //TODO further init?
  return modal;
}

#include "game/game.h"

struct modal_hello {
  struct modal hdr;
};

#define MODAL ((struct modal_hello*)modal)

/* Cleanup.
 */
 
static void _hello_del(struct modal *modal) {
}

/* Init.
 */
 
static int _hello_init(struct modal *modal) {
  modal->opaque=1;
  modal->interactive=1;
  return 0;
}

/* Activate.
 */
 
static void hello_activate(struct modal *modal) {
  struct modal *world=modal_new_world();
  if (!world) {
    egg_terminate(1);
    return;
  }
}

/* Update.
 */
 
static void _hello_update(struct modal *modal,double elapsed) {
//TODO choose story mode or arcade mode
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) hello_activate(modal);
}

/* Render.
 */
 
static void _hello_render(struct modal *modal) {
  graf_set_image(&g.graf,RID_image_title);
  graf_decal(&g.graf,0,0,0,0,FBW,FBH);
}

/* Type definition.
 */
 
const struct modal_type modal_type_hello={
  .name="hello",
  .objlen=sizeof(struct modal_hello),
  .del=_hello_del,
  .init=_hello_init,
  .update=_hello_update,
  .render=_hello_render,
};

/* Public ctor.
 */
 
struct modal *modal_new_hello() {
  struct modal *modal=modal_new(&modal_type_hello);
  if (!modal) return 0;
  if (modal_push(modal)<0) {
    modal_del(modal);
    return 0;
  }
  //TODO further init?
  return modal;
}

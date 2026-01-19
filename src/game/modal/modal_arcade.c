/* modal_arcade.c
 * UI for selecting a battle, player count, and handicap.
 * Then we launch that battle.
 */

#include "game/bellacopia.h"

struct modal_arcade {
  struct modal hdr;
};

#define MODAL ((struct modal_arcade*)modal)

/* Cleanup.
 */
 
static void _arcade_del(struct modal *modal) {
}

/* Init.
 */
 
static int _arcade_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=1;
  modal->interactive=1;
  return 0;
}

/* Focus.
 */
 
static void _arcade_focus(struct modal *modal,int focus) {
}

/* Notify.
 */
 
static void _arcade_notify(struct modal *modal,int k,int v) {
  if (k==EGG_PREF_LANG) {
  }
}

/* Update.
 */
 
static void _arcade_update(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    bm_sound(RID_sound_uicancel);
    modal->defunct=1;
  }
}

/* Render.
 */
 
static void _arcade_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x800000ff);
}

/* Type definition.
 */
 
const struct modal_type modal_type_arcade={
  .name="arcade",
  .objlen=sizeof(struct modal_arcade),
  .del=_arcade_del,
  .init=_arcade_init,
  .focus=_arcade_focus,
  .notify=_arcade_notify,
  .update=_arcade_update,
  .render=_arcade_render,
};

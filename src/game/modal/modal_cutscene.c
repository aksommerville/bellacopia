/* modal_cutscene.c
 * Wraps some non-interactive cutscene.
 */

#include "game/bellacopia.h"

struct modal_cutscene {
  struct modal hdr;
  int strix_title; // strings:stories, but it's really just a loose identifier.
  void (*cb)(void *userdata);
  void *userdata;
  int texid,texw,texh;//XXX
};

#define MODAL ((struct modal_cutscene*)modal)

/* Cleanup.
 */
 
static void _cutscene_del(struct modal *modal) {
  egg_texture_del(MODAL->texid);
}

/* Init.
 */
 
static int _cutscene_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=1;
  modal->interactive=1; // well, we're not really, but we do suppress interaction with other layers.
  
  if (!arg||(argc!=sizeof(struct modal_args_cutscene))) return -1;
  const struct modal_args_cutscene *args=arg;
  MODAL->strix_title=args->strix_title;
  MODAL->cb=args->cb;
  MODAL->userdata=args->userdata;
  
  // XXX TEMP: Generate a texture explaining who we are.
  MODAL->texid=font_render_to_texture(0,g.font,"TODO: modal_cutscene",-1,FBW,FBH,0xffffffff);
  egg_texture_get_size(&MODAL->texw,&MODAL->texh,MODAL->texid);
  
  return 0;
}

/* Focus.
 */
 
static void _cutscene_focus(struct modal *modal,int focus) {
}

/* Notify.
 */
 
static void _cutscene_notify(struct modal *modal,int k,int v) {
  if (k==EGG_PREF_LANG) {
  }
}

/* Update.
 */
 
static void _cutscene_update(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    bm_sound(RID_sound_uicancel);
    modal->defunct=1;
    if (MODAL->cb) {
      MODAL->cb(MODAL->userdata);
      MODAL->cb=0;
    }
  }
}

/* Render.
 */
 
static void _cutscene_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x404040ff);
  graf_set_input(&g.graf,MODAL->texid);
  graf_decal(&g.graf,(FBW>>1)-(MODAL->texw>>1),(FBH>>1)-(MODAL->texh>>1),0,0,MODAL->texw,MODAL->texh);
}

/* Type definition.
 */
 
const struct modal_type modal_type_cutscene={
  .name="cutscene",
  .objlen=sizeof(struct modal_cutscene),
  .del=_cutscene_del,
  .init=_cutscene_init,
  .focus=_cutscene_focus,
  .notify=_cutscene_notify,
  .update=_cutscene_update,
  .render=_cutscene_render,
};

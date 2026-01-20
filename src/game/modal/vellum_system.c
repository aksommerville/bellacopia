#include "game/bellacopia.h"
#include "vellum.h"

struct vellum_system {
  struct vellum hdr;
};

#define VELLUM ((struct vellum_system*)vellum)

/* Delete.
 */
 
static void _system_del(struct vellum *vellum) {
}

/* Focus.
 */
 
static void _system_focus(struct vellum *vellum,int focus) {
  switch (focus) {
    case VELLUM_FOCUS_BEFORE: break;
    case VELLUM_FOCUS_READY: break;
    case VELLUM_FOCUS_END: break;
  }
}

/* Update animation only.
 */
 
static void _system_updatebg(struct vellum *vellum,double elapsed) {
}

/* Update.
 */
 
static void _system_update(struct vellum *vellum,double elapsed) {
  _system_updatebg(vellum,elapsed);
}

/* Language change.
 */
 
static void _system_langchanged(struct vellum *vellum,int lang) {
}

/* Render.
 */
 
static void _system_render(struct vellum *vellum,int x,int y,int w,int h) {
}

/* New.
 */
 
struct vellum *vellum_new_system() {
  struct vellum *vellum=calloc(1,sizeof(struct vellum_system));
  if (!vellum) return 0;
  vellum->lblstrix=16;
  vellum->del=_system_del;
  vellum->focus=_system_focus;
  vellum->updatebg=_system_updatebg;
  vellum->update=_system_update;
  vellum->render=_system_render;
  vellum->langchanged=_system_langchanged;
  
  return vellum;
}

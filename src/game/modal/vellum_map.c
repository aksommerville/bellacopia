#include "game/bellacopia.h"
#include "vellum.h"

struct vellum_map {
  struct vellum hdr;
};

#define VELLUM ((struct vellum_map*)vellum)

/* Delete.
 */
 
static void _map_del(struct vellum *vellum) {
}

/* Focus.
 */
 
static void _map_focus(struct vellum *vellum,int focus) {
  switch (focus) {
    case VELLUM_FOCUS_BEFORE: break;
    case VELLUM_FOCUS_READY: break;
    case VELLUM_FOCUS_END: break;
  }
}

/* Update animation only.
 */
 
static void _map_updatebg(struct vellum *vellum,double elapsed) {
}

/* Update.
 */
 
static void _map_update(struct vellum *vellum,double elapsed) {
  _map_updatebg(vellum,elapsed);
}

/* Language change.
 */
 
static void _map_langchanged(struct vellum *vellum,int lang) {
}

/* Render.
 */
 
static void _map_render(struct vellum *vellum,int x,int y,int w,int h) {
}

/* New.
 */
 
struct vellum *vellum_new_map() {
  struct vellum *vellum=calloc(1,sizeof(struct vellum_map));
  if (!vellum) return 0;
  vellum->lblstrix=14;
  vellum->del=_map_del;
  vellum->focus=_map_focus;
  vellum->updatebg=_map_updatebg;
  vellum->update=_map_update;
  vellum->render=_map_render;
  vellum->langchanged=_map_langchanged;
  
  return vellum;
}

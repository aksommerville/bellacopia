#include "game/bellacopia.h"
#include "vellum.h"

struct vellum_stats {
  struct vellum hdr;
};

#define VELLUM ((struct vellum_stats*)vellum)

/* Delete.
 */
 
static void _stats_del(struct vellum *vellum) {
}

/* Focus.
 */
 
static void _stats_focus(struct vellum *vellum,int focus) {
}

/* Update animation only.
 */
 
static void _stats_updatebg(struct vellum *vellum,double elapsed) {
}

/* Update.
 */
 
static void _stats_update(struct vellum *vellum,double elapsed) {
  _stats_updatebg(vellum,elapsed);
}

/* Language change.
 */
 
static void _stats_langchanged(struct vellum *vellum,int lang) {
}

/* Render.
 */
 
static void _stats_render(struct vellum *vellum,int x,int y,int w,int h) {
}

/* New.
 */
 
struct vellum *vellum_new_stats() {
  struct vellum *vellum=calloc(1,sizeof(struct vellum_stats));
  if (!vellum) return 0;
  vellum->lblstrix=15;
  vellum->del=_stats_del;
  vellum->focus=_stats_focus;
  vellum->updatebg=_stats_updatebg;
  vellum->update=_stats_update;
  vellum->render=_stats_render;
  vellum->langchanged=_stats_langchanged;
  
  return vellum;
}

#include "game/bellacopia.h"
#include "vellum.h"

/* Delete.
 */
 
void vellum_del(struct vellum *vellum) {
  if (!vellum) return;
  if (vellum->del) vellum->del(vellum);
  egg_texture_del(vellum->lbltexid);
  free(vellum);
}

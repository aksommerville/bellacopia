/* vellum.h
 * Single-page controller for the pause modal.
 */
 
#ifndef VELLUM_H
#define VELLUM_H

struct vellum {
  struct modal *parent; // WEAK; always modal_pause.

  // modal_pause preps these. We clean them up.
  int lblstrix,lbltexid,lblw,lblh,lblx,clickw;

  /* Clean up any controller-specific content.
   * Do not free (vellum) itself, or any of the generic content in (struct vellum).
   */
  void (*del)(struct vellum *vellum);
  
  /* Called by the modal at key focus points.
   * See VELLUM_FOCUS_* above.
   */
  void (*focus)(struct vellum *vellum,int focus);
  
  /* Plain (update) while in focus, (updatebg) otherwise.
   * If you animate, it's good to use (updatebg) so you don't go still during the transition.
   */
  void (*update)(struct vellum *vellum,double elapsed);
  void (*updatebg)(struct vellum *vellum,double elapsed);
  
  /* Render in the given box.
   * (w,h) are constant but we're cagey about the exact values.
   * (x) can vary, during the transitions.
   */
  void (*render)(struct vellum *vellum,int x,int y,int w,int h);
  
  /* Called on a change of language, whether focussed or not.
   */
  void (*langchanged)(struct vellum *vellum,int lang);
};

void vellum_del(struct vellum *vellum);

struct vellum *vellum_new_inventory(struct modal *parent);
struct vellum *vellum_new_map(struct modal *parent);
struct vellum *vellum_new_stats(struct modal *parent);
struct vellum *vellum_new_system(struct modal *parent);

#endif

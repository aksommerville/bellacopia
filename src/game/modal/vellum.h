/* vellum.h
 * Single-page controller for the pause modal.
 */
 
#ifndef VELLUM_H
#define VELLUM_H

//#define VELLUM_FOCUS_GONE   0 /* Transition complete, you are no longer in view. */ XXX This is harder than it sounds, for pause to know.
#define VELLUM_FOCUS_BEFORE 1 /* You are about to slide in. */
#define VELLUM_FOCUS_READY  2 /* You are stable in focus. */
#define VELLUM_FOCUS_END    3 /* About to lose focus. */

struct vellum {

  // modal_pause preps these. We clean them up.
  int lblstrix,lbltexid,lblw,lblh;

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

struct vellum *vellum_new_inventory();
struct vellum *vellum_new_map();
struct vellum *vellum_new_stats();
struct vellum *vellum_new_system();

#endif

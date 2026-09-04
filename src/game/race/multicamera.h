/* multicamera.h
 * Replacement for the global "camera", to support 2-player broom races.
 * Unlike regular camera, we'll draw everything on the fly. And we don't need transitions. So there will be less state involved.
 * Also, we don't participate in spawning sprites. But we do render anything you spawn.
 */
 
#ifndef MULTICAMERA_H
#define MULTICAMERA_H

struct multicamera_view {

  /* Bounds in main framebuffer.
   * Should remain constant.
   */
  int dstx,dsty,dstw,dsth;
  
  /* (z) is the plane we're showing.
   * (x,y) is the current top-left corner of our view, in plane pixels.
   */
  int x,y,z;
  const struct plane *plane; // Lazy. Not expected to change, but we will react if it does.
  
  /* Should contain one sprite, and we'll focus on it.
   */
  struct sprite_group group;
};

void multicamera_quit();

/* (viewc) should be 1 or 2. We'll allow up to 4, why not.
 * Sprites must exist before you call. (eg race_begin() first, then multicamera_init()).
 */
int multicamera_init(int viewc);

/* Return the live object corresponding to one view.
 * (p) are sequential from zero.
 */
struct multicamera_view *multicamera_get_view(int p);

/* Freshen all scroll positions.
 * (elapsed) is probably not going to be used.
 */
void multicamera_update(double elapsed);

/* Overwrites entire framebuffer.
 */
void multicamera_render();

#endif

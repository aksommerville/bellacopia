/* camera.h
 * Coordinates rendering for outer world play.
 * Our responsibility is limited to rendering, transitions, selecting the position, and reporting exposure.
 */
 
#ifndef CAMERA_H
#define CAMERA_H

/* How many maps can we keep in scope at a time.
 * The absolute minimum is 4, and if maps were even a pixel smaller than the framebuffer, would need more.
 */
#define CAMERA_SCOPE_LIMIT 4

#define CAMERA_PAN_SPEED 10.0
#define CAMERA_LOCK_DISTANCE 0.450
#define CAMERA_TRANSITION_TIME 0.750

// Global singleton (g.camera).
struct camera {

  uint8_t z; // ID of current plane.
  int rx,ry; // Final render position, in clamped plane pixels. Top left corner of screen.
  double fx,fy; // Focus position, in plane meters, where the center of view should go. Unclamped.
  struct scope {
    struct map *map; // Can be null.
    int texid; // Exactly the size of one map (slightly larger than the framebuffer).
  } scopev[CAMERA_SCOPE_LIMIT]; // When on plane zero, scopev[0] is populated and the others empty.
  struct map *map; // Map with the primary focus.
  int edgel,edger,edget,edgeb; // Column or row in plane meters, just outside the current view. May be OOB.
  int cut; // Private signal to ourselves that the next update should force ideal position immediately.
  int lock; // Private signal that we are locked on the focus and shouldn't pan.
  int mapsdirty; // Rerender all maps at the next render. Required if any cells change (and yes, we have to rerender everything just to replace one cell).
  
  struct map *door_map;
  int door_x,door_y;
  int transition; // NS_transition_*
  double transition_clock; // Counts up.
  double transition_time; // Probably constant, but if needed we can run transitions at arbitrary speed.
  int transition_texid; // If nonzero, it's the same size as framebuffer.
  int fromx,fromy,tox,toy; // Focus points for spotlight transition, in framebuffer pixels.
  int transition_ready;
  
  double darkness; // 0..1 = light..dark
  double lightness;
  double teledx,teledy; // Telescope offset in pixels. We apply it, hero sets it directly.
  
  //TODO weather
  
  int listenerid_next;
  struct map_listener {
    int listenerid;
    void (*cb)(struct map *map,int focus,void *userdata);
    void *userdata;
  } *map_listenerv;
  int map_listenerc,map_listenera;
  struct cell_listener {
    int listenerid;
    void (*cb)(int x,int y,int w,int h,void *userdata);
    void *userdata;
  } *cell_listenerv;
  int cell_listenerc,cell_listenera;
  int listenlock;
};

/* Resetting drops all listeners cold.
 */
void camera_reset();

/* Call me after updating sprites and before rendering.
 */
void camera_update(double elapsed);

/* Overwrites the whole framebuffer.
 * We draw the scene, weather, and transitions.
 * We do not draw any artificial overlay, that's someone else's problem.
 */
void camera_render();
void camera_render_pretransition(int dsttexid);

/* Schedule a transition.
 * We'll fade out and effect the transition when black, it's not immediate.
 * (subcol,subrow) are in meters relative to the map -- not the usual plane meters. It should come right off a door poi.
 * This does not touch the hero or any other sprites. Hero should listen for map exposures and move herself when the new one focusses.
 */
void camera_cut(int mapid,int subcol,int subrow,int transition);

/* Listeners get called as the camera updates, to inform of exposure.
 * There are two granularities: map and cell.
 * Maps get called at 4 inflection points:
 *  - (focus==-1) lose primary status.
 *  - (focus==0) exit view.
 *  - (focus==1) enter view.
 *  - (focus==2) become primary.
 * Cells only report focus. We give a rectangle in plane meters which is currently just outside view.
 * It's safe to unlisten yourself during the callback, but **** you must not add or remove any other listeners ****.
 */
void camera_unlisten(int listenerid);
int camera_listen_map(void (*cb)(struct map *map,int focus,void *userdata),void *userdata);
int camera_listen_cell(void (*cb)(int x,int y,int w,int h,void *userdata),void *userdata);

/* Returns one of:
 *  0: No transition in progress.
 *  1: Transition in progress, end scene is at least partially visible.
 *  2: Transition in progress, end scene is definitely not visible. eg first half of fadeblack or spotlight.
 */
int camera_describe_transition();

#endif

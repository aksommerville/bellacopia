/* camera.h
 * Responsible for keeping up to 4 maps loaded and determining render position.
 */
 
#ifndef CAMERA_H
#define CAMERA_H

struct map;

/* Resetting camera deletes all the sprites immediately.
 * We load the given map from scratch and begin centered on it.
 */
int camera_reset(int mapid);

void camera_update(double elapsed);
void camera_render();

int camera_get_z();
int camera_get_mapid(); // Only valid if (z<0), ie a solo plane.
const struct map *camera_get_map(); // ''
void camera_get_focus(int *x,int *y,int *z); // Focussed map position (world coords).

int camera_for_each_poi(int x,int y,int (*cb)(uint8_t opcode,const uint8_t *arg,void *userdata),void *userdata);

/* Schedule travel thru a door.
 * It doesn't happen immediately, will wait for the end of the update cycle.
 */
void camera_enter_door(int mapid,int col,int row);
void camera_warp_home_soon(); // Moves hero to the starting position.

// Yes yes, that's a thing cameras do!
void camera_flash();

#endif

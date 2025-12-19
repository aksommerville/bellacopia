/* camera.h
 * Responsible for keeping up to 4 maps loaded and determining render position.
 */
 
#ifndef CAMERA_H
#define CAMERA_H

/* Resetting camera deletes all the sprites immediately.
 * We load the given map from scratch and begin centered on it.
 */
int camera_reset(int mapid);

void camera_update(double elapsed);
void camera_render();

int camera_get_z();

int camera_for_each_poi(int x,int y,int (*cb)(uint8_t opcode,const uint8_t *arg,void *userdata),void *userdata);

/* Schedule travel thru a door.
 * It doesn't happen immediately, will wait for the end of the update cycle.
 */
void camera_enter_door(int mapid,int col,int row);

#endif

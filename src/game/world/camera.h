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

#endif

/* race.h
 * Manages the broom races out in the real world between Dot and Moon.
 * Note that there's also a "broomrace" battle, which is a lot like this, but completely separate.
 */
 
#ifndef RACE_H
#define RACE_H

int race_begin(int raceid);
void race_end();

/* Race accessors, valid only when a race is in progress.
 */
int race_get_lapc();
int race_get_checkpointc();
int race_get_checkpoint(double *x,double *y,int p); // Plane meters. Race begins at (p==0).
int race_get_target_time(); // => seconds

#endif

/* race.h
 * Manages the broom races out in the real world between Dot and Moon.
 * Note that there's also a "broomrace" battle, which is a lot like this, but completely separate.
 */
 
#ifndef RACE_H
#define RACE_H

int race_begin(int raceid);
void race_end();
void race_check_completion();
void race_update(double elapsed);
double race_get_countdown();

/* Race accessors, valid only when a race is in progress.
 */
int race_get_lapc();
int race_get_checkpointc();
int race_get_checkpoint(double *x,double *y,int p); // Plane meters. Race begins at (p==0).
int race_get_trackc();
int race_get_track(double *x,double *y,int p);
int race_get_target_time(); // => seconds

struct race_status {
  int lapp,lapc; // (lapp>lapc) if completed; then (laptime) is for the best lap.
  double laptime;
  double racetime;
  double opponenttime; // Only set after complete.
};
void race_get_status(struct race_status *status);

#endif

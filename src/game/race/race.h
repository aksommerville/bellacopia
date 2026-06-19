/* race.h
 * Manages the broom races out in the real world between Dot and Moon.
 * Note that there's also a "broomrace" battle, which is a lot like this, but completely separate.
 */
 
#ifndef RACE_H
#define RACE_H

int race_begin(int raceid);
void race_end();
void race_check_completion(int stop_soon);
void race_update(double elapsed);
double race_get_countdown();
int race_fld_by_id(int raceid);
int race_fld_by_index(int p); // 0..5; this field gets set automatically first time you win it.
int race_fld16_by_index(int p); // fld16 containing 8ms of your best time, or zero if you haven't run it yet.

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
  int opponent_finished;
};
void race_get_status(struct race_status *status);

#endif

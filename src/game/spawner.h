/* spawner.h
 * Responsible for spawning random sprites.
 * We get called by modal_story on cell exposures, and decide from there.
 */
 
#ifndef SPAWNER_H
#define SPAWNER_H

/* Global singleton (g.spawner).
 */
struct spawner {
  int z; // Most recent plane. If it doesn't match camera, wipe everything.
  
  /* Extra bookkeeping, indexed by map id.
   * For each map, we record the sum of rsprite weights. If zero, the map doesn't spawn anything.
   * A period is calculated from the weight sum. Spawn something every time so many candidate cells are received.
   * To decide which rsprite to spawn, step a counter by a prime step and wrap around at weight sum.
   */
  struct spawnmap {
    int wsum; // Sum of weights from rsprite commands.
    int period; // How many candidate cells per spawn.
    int spawn_counter; // Counts down from (period), spawn at zero.
    int step; // Some positive integer coprime to (wsum).
    int choice; // Counts up by (step), wraps around at (wsum). Which rsprite to spawn.
  } *spawnmapv;
  int spawnmapc;
};

void spawner_reset();

void spawner_expose(int x,int y,int w,int h);

#endif

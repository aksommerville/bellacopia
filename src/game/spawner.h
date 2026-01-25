/* spawner.h
 * Responsible for spawning random sprites.
 * We get called by modal_story on cell exposures, and decide from there.
 */
 
#ifndef SPAWNER_H
#define SPAWNER_H

/* An rsprite command's weight is the odds that a given exposed cell will spawn it.
 * Obviously the natural 0..1 range is too hot, so we scale it down by this constant.
 * 1/10 is a heck of a lot, and 1/2550 is very rare. 1/10 sounds like a good max.
 */
#define SPAWN_WEIGHT_MAX 0.100

// Global singleton (g.spawner).
struct spawner {
  struct sprite_group rsprites;
  
  /* We have some extra bookkeeping per map.
   * These are indexed by mapid, so they always match (g.maps.byidv).
   */
  struct spawnmap {
    int wsum; // Sum of weights from rsprite commands. If zero, doesn't participate.
    int pending; // Volatile. Allow so many candidate cells to pass before spawning the next one.
  } *spawnmapv;
  int spawnmapc;
};

void spawner_reset();

void spawner_expose(int x,int y,int w,int h);

#endif

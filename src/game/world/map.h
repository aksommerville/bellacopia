/* map.h
 * Defines the map object and a global registry of them, indexed by position.
 * Map resources all have a 3-dimensional position (the Z index is "plane", distinct sets of maps).
 * Planes should be numbered sequentially as much as possible, and the content of each plane should aim to be rectangular.
 * The map registry fills in as needed to finish those continuities.
 */
 
#ifndef MAP_H
#define MAP_H

struct map {
  uint8_t x,y,z; // Absolute position in the world. Unreliable if (z==0), those are fallback maps.
  int rid;
  int imageid;
  int songid; // <0=unspecified; 0=silent.
  uint8_t v[NS_sys_mapw*NS_sys_maph]; // Tiles, mutable.
  const uint8_t *ro; // Tiles, immutable (points into ROM).
  const void *cmd; // Immutable (points into ROM).
  int cmdc;
};

/* res_init() will call this after populating the TOC.
 * Happens only once per run of the game.
 */
int maps_init();

/* Restore the global registry to its initial state.
 */
void maps_reset();

/* Once initialized, every map in the world is resident with a fixed address.
 * (addresses are volatile *during* initialization, but that's not your problem).
 * Real maps have coords in 0..255, but the coords you ask for may be OOB and we may make something up.
 * We don't guarantee a default; null is possible.
 * Also the returned map may have null (ro,cmd) and zero (rid). But if one of those is present they all are.
 */
struct map *map_by_position(int x,int y,int z);

#endif

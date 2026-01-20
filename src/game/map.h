/* map.h
 * Define live map objects, and a complex store for them.
 * Map resources don't live in the regular resource store.
 * When you res_init(), res.c coordinates with us to build the map store.
 */
 
#ifndef MAP_H
#define MAP_H

/* Map object.
 ******************************************************************************/
 
struct map {
  int rid;
  uint8_t lng,lat,z;
  const uint8_t *cmd; // Points into resource.
  int cmdc;
  const uint8_t *rov; // Points into resource. NS_sys_mapw*NS_sys_maph.
  uint8_t v[NS_sys_mapw*NS_sys_maph]; // Real cells. If they differ from (rov), the difference is auditable somehow, eg CMD_map_switchable.
  const uint8_t *physics; // Owned by res store.
  const uint8_t *jigctab; // ''
  int imageid,songid;
  int parent; // Zero or map rid.
  uint8_t dark,wind;
};

/* Store.
 *********************************************************************************/

// Global singleton (g.mapstore).
struct mapstore {
  struct plane {
    uint8_t z; // Also my index.
    int w,h; // In maps.
    struct map *v; // Packed LRTB. Gaps have (rid==0) but all valid defaults.
  } planev[256];
  int planec;
  struct map **byidv; // Index is rid. May be null. If not null, owned by (planev) or (singv).
  int byidc;
};

/* Between reset and link, our state may be inconsistent.
 * We use welcome to measure planes, but don't actually store anything at that time.
 * So our counts disagree with our allocations, until link.
 */
int mapstore_reset();
int mapstore_welcome(const struct rom_entry *res);
int mapstore_link();

/* May be null and may be a (rid==0) dummy.
 * Dummies are perfectly usable, if you don't want to check.
 * (z==0) is for singletons. Each map is expected to display on its own.
 * But for storage purposes, all those singletons are laid out in a plane just like the continuous planes.
 * Empty planes are possible if the set is sparse.
 */
struct map *map_by_id(int rid);
struct map *map_by_position(int lng,int lat,int z);
struct map *map_by_sprite_position(double x,double y,int z);
struct plane *plane_by_position(int z);

/* Determine where the hero should be, for a new session.
 * This may involve consulting the store.
 * (col,row) are relative to the map, not the plane.
 */
int maps_get_start_position(int *mapid,int *col,int *row);

#endif

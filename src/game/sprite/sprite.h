/* sprite.h
 */
 
#ifndef SPRITE_H
#define SPRITE_H

struct sprite;
struct sprite_type;

/* Sprite instance.
 ****************************************************************************/
 
struct sprite {
  const struct sprite_type *type;
  int defunct;
  int rid;
  const void *serial;
  int serialc;
  const uint8_t *arg; // 4 bytes, always present (canned zeroes if not available). Can use as identity for map-spawned sprites.
  double x,y; // Position in plane meters.
  int z; // Plane ID. Camera should populate before the first update. Beware, it will not be valid at init. (camera_get_z() if you need it).
  int imageid;
  uint8_t tileid,xform;
  int layer;
  int solid; // Participates in physics.
  uint32_t physics; // Bitfields, (1<<NS_physics_*), for the impassable ones.
  double hbl,hbr,hbt,hbb; // Hitbox, relative to (x,y). Negative and positive 1/2 by default.
};

/* If you don't provide (type), you must provide (serial) to read it from, or (rid) to get that from.
 * When maps spawn a sprite, they are expected to provide just (x,y,rid,arg).
 * Returns WEAK on success. To delete it, set (defunct).
 */
struct sprite *sprite_spawn(
  double x,double y,
  int rid,
  const uint8_t *arg, // 4 bytes or null
  const struct sprite_type *type,
  const void *serial,int serialc
);

/* Carefully confirms the sprite exists, without dereferencing it.
 */
int sprite_is_resident(const struct sprite *sprite);

/* Hooks for the world to call in, for the set of current sprites.
 * Updating drops defunct sprites before returning.
 */
void sprites_update(double elapsed);
void sprites_render(int scrollx,int scrolly);

// Deletes all immediately. Be careful not to call during iteration.
void sprites_clear();

// We track the hero sprite via magic.
struct sprite *sprites_get_hero();

struct sprite *sprite_by_arg(const void *arg);

// Get the actual list. Please be careful.
int sprites_get_all(struct sprite ***dstpp);

/* Sprite type.
 ***************************************************************************/
 
struct sprite_type {
  const char *name;
  int objlen;
  void (*del)(struct sprite *sprite);
  int (*init)(struct sprite *sprite);
  void (*update)(struct sprite *sprite,double elapsed);
  
  /* If you don't implement, we use (imageid,tileid,xform).
   * (dstx,dsty) are (sprite->x,y) transformed to framebuffer space.
   */
  void (*render)(struct sprite *sprite,int dstx,int dsty);
  
  /* Called when a motion is completely nixed due to collision between two sprites.
   * Not called for collisions against the map.
   */
  void (*collide)(struct sprite *sprite,struct sprite *other);
};

#define _(tag) extern const struct sprite_type sprite_type_##tag;
FOR_EACH_SPRTYPE
#undef _

const struct sprite_type *sprite_type_by_id(int sprtype);
const struct sprite_type *sprite_type_from_serial(const void *src,int srcc);

void sprite_hero_ackpos(struct sprite *sprite);

/* Physics.
 ***********************************************************************/
 
struct aabb { double l,r,t,b; };

/* If this sprite participates in physics, first confirm the move is legal.
 * Returns 1 if moved at all (possibly less than you asked for), or 0 if completely blocked.
 */
int sprite_move(struct sprite *sprite,double dx,double dy);

/* (d) must be cardinal and (sprite) must be solid.
 * Returns the distance we can travel in that direction before a collision.
 * If <0, we're already in a collision state.
 * Clamps fairly close. Currently 6 meters.
 * If we return <=0.0 and the collision is due to another sprite, we populate (*cause).
 */
double sprite_measure_freedom(const struct sprite *sprite,double dx,double dy,struct sprite **cause);

/* Nonzero if the current position is legal.
 */
int sprite_test_position(const struct sprite *sprite);

#endif

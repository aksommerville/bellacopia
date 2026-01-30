/* sprite.h
 */
 
#ifndef SPRITE_H
#define SPRITE_H

struct sprites;
struct sprite;
struct sprite_type;
struct sprite_group;

/* Generic sprite.
 ********************************************************************/
 
struct sprite {
  const struct sprite_type *type;
  struct sprite_group **grpv;
  int grpc,grpa;
  int refc;
  int defunct; // Suppresses most activity; presumably we are in deathrow already.
  double x,y; // Plane meters.
  uint8_t z; // Which plane. Sprites will only exist on the camera's current plane, repeated here to be certain.
  int layer; // Lower renders first. Hero at 100.
  int imageid;
  uint8_t tileid,xform;
  double hbl,hbr,hbt,hbb; // Hitbox relative to (x,y). (l,t) are usually negative.
  uint32_t physics; // Bits, (1<<NS_physics_*), which ones are impassable.
  int solid,floating; // Group hints. These are managed magically when adding and removing groups. For things we poll frequently.
  int rid;
  const uint8_t *cmd,*arg; // (cmd) is the entire resource or null. (arg) is at least 4 bytes always.
  int cmdc,argc;
};

/* Try not to use del/new/ref, they should only be used internally.
 * To delete a sprite, assign it to the "deathrow" group.
 * To create one, use sprite_spawn().
 * sprite_new does add to the global groups, but also returns a STRONG reference.
 */
void sprite_del(struct sprite *sprite);
struct sprite *sprite_new(
  double x,double y,
  int rid,
  const void *arg,int argc,
  const struct sprite_type *type,
  const void *cmd,int cmdc
);
int sprite_ref(struct sprite *sprite);

/* Spawn a sprite and return a WEAK reference.
 * If you supply (rid), we can find (type,cmd,cmdc).
 * Null (arg) is legal, we'll replace with 4 zeroes.
 * (arg) shorter than 4 is illegal.
 */
struct sprite *sprite_spawn(
  double x,double y,
  int rid,
  const void *arg,int argc,
  const struct sprite_type *type,
  const void *cmd,int cmdc
);

/* A convenience to set (defunct) and add to (deathrow).
 */
void sprite_kill_soon(struct sprite *sprite);

/* Sprite type.
 ***********************************************************************/
 
struct sprite_type {
  const char *name;
  int objlen;
  void (*del)(struct sprite *sprite);
  int (*init)(struct sprite *sprite);
  void (*update)(struct sprite *sprite,double elapsed);
  void (*render)(struct sprite *sprite,int x,int y);
  void (*collide)(struct sprite *sprite,struct sprite *other);
  void (*tread_poi)(struct sprite *sprite,uint8_t opcode,const uint8_t *arg,int argc);
};

#define _(tag) extern const struct sprite_type sprite_type_##tag;
FOR_EACH_sprtype
#undef _

const struct sprite_type *sprite_type_by_id(int sprtype);

void hero_injure(struct sprite *sprite,struct sprite *assailant);
void sprite_hero_unanimate(struct sprite *sprite); // eg when launching a modal, force Dot to a neutral face.
void sprite_hero_unvanish(struct sprite *sprite);

int sprite_toast_set_text(struct sprite *sprite,const char *src,int srcc);
struct sprite *sprite_toast_get_any();

/* Sprite group.
 *******************************************************************/
 
#define SPRITE_GROUP_MODE_ADDR     0 /* Sort by address. Fastest lookups. */
#define SPRITE_GROUP_MODE_EXPLICIT 1 /* Most recent addition at the end. Re-adding shuffles to the end. */
#define SPRITE_GROUP_MODE_SINGLE   2 /* Zero or one member. Adding a second evicts the first. */
#define SPRITE_GROUP_MODE_RENDER   3 /* Tries to preserve render order. Does allow slippage. */
 
struct sprite_group {
  struct sprite **sprv;
  int sprc,spra;
  int mode;
  int refc; // Zero if immortal (all the global groups are).
};

void sprite_group_del(struct sprite_group *group);
struct sprite_group *sprite_group_new();
int sprite_group_ref(struct sprite_group *group);

int sprite_group_has(const struct sprite_group *group,const struct sprite *sprite);
int sprite_group_add(struct sprite_group *group,struct sprite *sprite);
int sprite_group_remove(struct sprite_group *group,struct sprite *sprite);

void sprite_group_clear(struct sprite_group *group);
void sprite_kill(struct sprite *sprite); // Remove all groups, even deathrow and keepalive. Typically deletes the sprite.
void sprite_group_kill_all(struct sprite_group *group);

void sprite_group_sort_partial(struct sprite_group *group);

/* Global context.
 * There's just one of these, (g.sprites).
 *********************************************************************/
 
struct sprites {
  struct sprite_group grpv[32];
};

#define GRP(tag) (g.sprites.grpv+NS_sprgrp_##tag)

void sprites_reset();

/* (arg) can be a default, which could match many sprites.
 * But ones spawned via CMD_map_sprite or CMD_map_rsprite all have a unique (arg) that we can identify by address.
 */
struct sprite *find_sprite_by_arg(const void *arg);

/* Physics.
 *********************************************************************/
 
struct aabb { double l,r,t,b; };

/* Distance available in the given cardinal direction.
 * <=0 if we can't.
 * Call only if participating in physics.
 */
double sprite_measure_freedom(const struct sprite *sprite,double dx,double dy,struct sprite **cause);

/* Nonzero if we move at all, may be less than you ask for.
 */
int sprite_move(struct sprite *sprite,double dx,double dy);

/* Nonzero if (sprite) is clear of all collisions.
 */
int sprite_test_position(const struct sprite *sprite);

#endif

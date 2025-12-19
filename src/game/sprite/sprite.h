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
  uint32_t arg;
  double x,y; // Position in plane meters.
  int imageid;
  uint8_t tileid,xform;
  int layer;
  //TODO physics, etc
};

/* If you don't provide (type), you must provide (serial) to read it from, or (rid) to get that from.
 * When maps spawn a sprite, they are expected to provide just (x,y,rid,arg).
 * Returns WEAK on success. To delete it, set (defunct).
 */
struct sprite *sprite_spawn(
  double x,double y,
  int rid,
  uint32_t arg,
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
};

#define _(tag) extern const struct sprite_type sprite_type_##tag;
FOR_EACH_SPRTYPE
#undef _

const struct sprite_type *sprite_type_by_id(int sprtype);
const struct sprite_type *sprite_type_from_serial(const void *src,int srcc);

#endif

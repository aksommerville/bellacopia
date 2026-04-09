/* batsup_world.h
 * A private system with one map and multiple sprites, for battles.
 * We do use the global map registry, but not global sprites (or the global sprite type), or the camera.
 * Sprites are expected to not be very numerous, and to all be implemented in the same place.
 * So our sprites are quite a bit simpler than the global type.
 */
 
#ifndef BATSUP_WORLD_H
#define BATSUP_WORLD_H

struct battle;

struct batsup_sprite {
  struct batsup_world *world; // WEAK, REQUIRED
  int id; // >0 and unique within the world all time.
  int objlen;
  int defunct;
  double x,y; // In map cells.
  int layer; // Default 100.
  int imageid; // Defaults to the map's image.
  uint8_t tileid;
  uint8_t xform;
  int solid; // Solid sprites are presumed to have a one meter square hitbox, exactly centered.
  void (*update)(struct batsup_sprite *sprite,double elapsed);
  void (*render)(struct batsup_sprite *sprite,int dstx,int dsty);
};

struct batsup_world {
  struct battle *battle; // WEAK
  struct map *map; // STRONG if (ownmap), else WEAK (belongs to store)
  struct batsup_sprite **spritev;
  int spritec,spritea;
  int spriteid_next;
  int sortdir; // Default 1. Set zero to disable sprite sorting.
  int ownmap;
};

/* New with (mapid==0) to generate a map that you can modify.
 */
void batsup_world_del(struct batsup_world *world);
struct batsup_world *batsup_world_new(struct battle *battle,int mapid);

/* Set our map's tilesheet, which will also be the default image for new sprites.
 * This is only valid if (mapid==0) at init, ie we own the map.
 */
int batsup_world_set_image(struct batsup_world *world,int imageid);

void batsup_world_update(struct batsup_world *world,double elapsed);

/* Overwrites entire framebuffer.
 * Draws map and sprites. No overlay or weather.
 */
void batsup_world_render(struct batsup_world *world);

/* Normally you give 0 for (id) and we make one up.
 * You may also supply anything >0, and we'll fail if already in use.
 * There's no structured type for sprites. After spawning, assign all the things yourself.
 * Returns WEAK.
 */
struct batsup_sprite *batsup_sprite_spawn(struct batsup_world *world,int id,int objlen);

struct batsup_sprite *batsup_sprite_by_id(struct batsup_world *world,int id);

/* Adjust (sprite)'s position by (dx,dy).
 * If (sprite->solid), we check against the map and other solid sprites.
 * Only vacant and safe cells are passable, for all sprites.
 * The map's edge is a hard stop too. (NB map's edge, not screen's edge).
 * Returns >0 if it moved at all.
 */
int batsup_sprite_move(struct batsup_sprite *sprite,double dx,double dy);

/* Internal use.
 */
void batsup_sprite_del(struct batsup_sprite *sprite);
struct batsup_sprite *batsup_sprite_new(struct batsup_world *world,int id,int objlen);
int batsup_sprite_rendercmp(const struct batsup_sprite *a,const struct batsup_sprite *b);

#endif

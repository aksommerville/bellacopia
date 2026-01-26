/* game.h
 * High-level actions for story mode.
 */
 
#ifndef GAME_H
#define GAME_H

/* Resets all the loose globals.
 * Store, camera, etc.
 */
int game_reset(int use_save);

/* Does not update the entire game state.
 * Just the disorganized globals.
 * See modal_story.c
 */
void game_update(double elapsed);

/* modal_story should call when a map becomes visible.
 * Spawns static sprites. Maybe other things. Indexing poi?
 */
int game_welcome_map(struct map *map);

/* modal_story should call when a new map acquires the principal focus.
 * Change song, apply weather, etc.
 */
int game_focus_map(struct map *map);

/* Does all the bells and whistles.
 * Returns nonzero if anything collected.
 * If (itemid==NS_itemid_jigpiece), (quantity) must be the mapid.
 */
int game_get_item(int itemid,int quantity);

/* Static metadata per itemid.
 */
struct item_detail {
  uint8_t tileid; // image:pause, the most generic look.
  uint8_t hand_tileid; // item:hero, to overlay on her sprite.
  int strix_name; // strings:item, just the name.
  int strix_help; // strings:item, extra text to display when we get the item.
  int initial_limit; // Zero if quantity not applicable. If negative, it's a fld16 holding the limit.
  int inventoriable; // May add to inventory.
  int fld16; // Zero or NS_fld16_* where to store the quantity.
};
const struct item_detail *item_detail_for_itemid(int itemid);
const struct item_detail *item_detail_for_equipped();

/* Handles all the odd things. Singleton items, non-inventory items, all of it.
 * Also optionally returns the limit, since we're already in there.
 */
int possessed_quantity_for_itemid(int itemid,int *limit);

/* (initiator) is optional. Typically an npc.
 */
void game_begin_activity(int activity,int arg,struct sprite *initiator);

/* Partner to NS_activity_tolltroll, so the sprite can set its appearance.
 * Returns itemid, or zero if the troll has already been paid off.
 * Pass this appearance back as (arg) to game_begin_activity(NS_activity_tolltroll).
 */
int tolltroll_get_appearance();

/* [LRUD]. Returns nonzero if valid, zero if invalid.
 */
int game_cast_spell(const char *src,int srcc);

/* (mapid) must contain a hero spawn point, we fail immediately if not.
 * Does not move immediately. We negatiate a transition with camera, and move the hero when it takes effect.
 */
int game_warp(int mapid);

/* Generate a list of things the compass can target.
 * Never more than (dsta).
 * Returned values are strix in strings:item, hopefully a short name for the target.
 */
int game_list_targets(int *dstv,int dsta);

/* Get the position in plane meters for the given compass target on the given plane.
 * All targets are positionable on all planes. We'll point to a door if the real thing is somewhere else.
 * >=0 on success. Failure is possible if (strix) is unknown or if the map set is defective and we can't find a path.
 * Expensive, please don't spam.
 * (px,py) are the position on plane (z) that you're searching from. Only relevant for plane zero.
 */
int game_get_target_position(int *lng,int *lat,int px,int py,int z,int strix);

/* Main must call just once, after initializing the maps store.
 * Scans all maps and indexes compass targets.
 */
int game_init_targets();

/* Return zero or itemid, something you caught around the given cell.
 */
int game_choose_fish(int x,int y,int z);

void game_hurt_hero();

#endif

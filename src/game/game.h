/* game.h
 * High-level actions for story mode.
 */
 
#ifndef GAME_H
#define GAME_H

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

/* [LRUD]. Returns nonzero if valid, zero if invalid.
 */
int game_cast_spell(const char *src,int srcc);

#endif

/* game.h
 * High-level actions for story mode.
 */
 
#ifndef GAME_H
#define GAME_H

/* modal_story should call when a map becomes visible.
 * Spawns static sprites. Maybe other things. Indexing poi?
 */
int game_welcome_map(struct map *map);

/* modal_story should call when a new map acquires the principal focus.
 * Change song, apply weather, etc.
 */
int game_focus_map(struct map *map);

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

#endif

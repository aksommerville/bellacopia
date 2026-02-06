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

int game_activity_sprite_should_abort(int activity,const struct sprite_type *type);

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
int game_warp(int mapid,int transition);

/* Generate a list of things the compass can target.
 * Never more than (dsta).
 * Returned values are strix in strings:item, hopefully a short name for the target.
 */
int game_list_targets(int *dstv,int dsta,int mode);
#define TARGET_MODE_AVAILABLE 0 /* Dot has it. Should propose as an option in the compass's menu. */
#define TARGET_MODE_ALL       1 /* All known targets. */
#define TARGET_MODE_UPGRADE   2 /* Targets you don't have yet, for the menu at Magnetic North. */

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

/* For upgrading and downgrading the compass.
 * Beyond the built-in targets, we won't report as available until you've enabled it.
 */
void game_disable_all_targets();
void game_enable_target(int strix);

/* Return zero or itemid, something you caught around the given cell.
 */
int game_choose_fish(int x,int y,int z);

/* Handicap for a typical random encounter.
 * Accounts for current state and all.
 */
uint8_t game_get_handicap(int battle);

/* Decide what should be awarded for winning some battle, accounting for current state.
 * (monsterarg) is the 4-byte argument to a monster sprite, or null.
 * Never returns more than (a).
 */
struct prize {
  int itemid,quantity;
};
int game_get_prizes(struct prize *v,int a,int battle,const uint8_t *monsterarg);

void game_hurt_hero();

/* Generate a list of select POI within a given radius of a point on one plane.
 * Never returns more than (dsta). Stops searching when full, so it doesn't necessarily return the closest.
 * Will not return secrets already got.
 */
struct secret {
  struct map *map;
  struct cmdlist_entry cmd;
  double x,y; // Plane meters to center of tile (ie n.5)
  double d2; // (sqrt(d2)) if you need the real distance, I figure we don't always need it.
};
int game_find_secrets(struct secret *dst,int dsta,double x,double y,int z,double radius);

/* Percentage of things done, accounting for everything.
 * Zero only for a fresh session, and 100 only if everything is actually done.
 */
int game_get_completion();

void game_get_sidequests(int *done,int *total);

/* Get an encrypted message (in Old Goblish) for the crypto sidequest.
 * Punctuation is all ASCII G0, and letters are all 0xc1..0xda (ie ASCII uppercase + 0x80).
 * image:fonttiles is equipped to display this.
 */
int cryptmsg_get(char *dst,int dsta,int which);

/* Notify that a seal is being stood on, or exitted.
 */
void cryptmsg_press_seal(int id);
void cryptmsg_release_seal(int id);

void cryptmsg_notify_item(int itemid);

/* If (battle,itemid) is the key to the star door, effect the change and return nonzero.
 * Monster should call whenever the hero loses.
 */
int cryptmsg_check_star_door(int battle,int itemid);

#endif

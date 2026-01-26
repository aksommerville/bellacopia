/* battle.h
 * Generic interface for battles.
 * Usually you'll interact with modal_battle, not directly with this.
 */
 
#ifndef BATTLE_H
#define BATTLE_H

/* Anyone running a battle should assume it is broken if it runs so long.
 * TODO Length guidance. I'm thinking about 10 seconds would be ideal, but play it out some first.
 */
#define BATTLE_UNIVERSAL_TIMEOUT 10.0

struct battle_type {
  const char *name; // For internal diagnostics only.
  int strix_name; // RID_strings_battle, intro skipped if zero.
  int no_article; // We usually prepend an indefinite article (with per-language conditions). Nonzero to suppress.
  int no_contest; // We usually append the word "contest". Nonzero to suppress it. (eg "to a Dance Off" not "to a Dance Off contest")
  int supported_players; // Bits, (1<<NS_players_*). 15 for all 4 modes.
  
  /* You must return a non-null context object at (init), which will be provided to all other calls.
   * If you allocate it on the heap, you must free it at (del).
   * You must call (cb_end) at some point with an outcome of (-1,0,1).
   * After (cb_end) you do continue updating and rendering, but the result is fixed. And owner may overlay final reporting.
   */
  void (*del)(void *ctx);
  void *(*init)(
    uint8_t handicap,
    uint8_t players,
    void (*cb_end)(int outcome,void *userdata),
    void *userdata
  );
  void (*update)(void *ctx,double elapsed);
  void (*render)(void *ctx);
};

const struct battle_type *battle_type_by_id(int battle); // NS_battle_*

#endif

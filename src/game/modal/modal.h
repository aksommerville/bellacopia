/* modal.h
 * Defines the global modals management, and interface for specific types.
 * Main only interacts with modals.
 * Modals should tend to be lean. If there's heavy business work, defer it to something more business-specific.
 */
 
#ifndef MODAL_H
#define MODAL_H

struct modal;
struct modal_type;

/* Generic modal.
 *************************************************************************/
 
struct modal_type {
  const char *name;
  int objlen;
  void (*del)(struct modal *modal);
  
  /* Return <0 or set (modal->defunct) to fail; they are equivalent.
   * Modals may define an arg struct below. Caller provides its size for validation.
   */
  int (*init)(struct modal *modal,const void *arg,int argc);
  
  /* The highest modal with (interactive) set, and any above it, all get updated.
   */
  void (*update)(struct modal *modal,double elapsed);
  
  /* Called for all modals *below* the topmost (interactive).
   * I don't expect most to need it.
   */
  void (*updatebg)(struct modal *modal,double elapsed);
  
  /* The highest modal with (opaque) set, and any above it, all get rendered.
   * You may be asked to render without updating and vice-versa.
   * The first render typically comes before the first update, so be sure you're ready for either after init.
   */
  void (*render)(struct modal *modal);
  
  /* Call when I gain (1) or lose (0) the principal focus, ie I'm the topmost (interactive) modal.
   * New modals will always get a focus callback before their first update, if warranted.
   */
  void (*focus)(struct modal *modal,int focus);
  
  /* egg_client_notify, forwarded to all modals.
   * I think the only interesting key is EGG_PREFS_LANG, but we send them all.
   */
  void (*notify)(struct modal *modal,int k,int v);
};

struct modal {
  const struct modal_type *type;
  int defunct;
  int opaque; // Nothing below me will render, and I promise to clear the whole framebuffer.
  int interactive; // Nothing below me will update.
  int blotter; // Draw a transparent black blotter below me, if no one above is doing it.
};

/* These should only be used by modal.c.
 * To delete a modal, set its (defunct) nonzero.
 * To create one, see modal_spawn() below.
 */
void modal_del(struct modal *modal);
struct modal *modal_new(const struct modal_type *type,const void *arg,int argc);

/* Global modal stack.
 ************************************************************************/

/* Returns WEAK; the new modal is automatically pushed on top of the stack.
 */
struct modal *modal_spawn(
  const struct modal_type *type,
  const void *arg,int argc
);

void modals_update(double elapsed);
void modals_render(); // Always wipes the whole framebuffer.
void modals_drop_defunct();

int modal_is_resident(const struct modal *modal);

/* Specific types.
 *********************************************************************/
 
extern const struct modal_type modal_type_hello; // Default. No args.
extern const struct modal_type modal_type_story; // Entire session of Story Mode.
extern const struct modal_type modal_type_arcade; // Entire session of Arcade Mode.
extern const struct modal_type modal_type_battle; // Story or Arcade Mode.
extern const struct modal_type modal_type_pause; // Story Mode.
extern const struct modal_type modal_type_dialogue; // Any mode.
extern const struct modal_type modal_type_shop; // Story Mode. Basically dialogue, with shopping-specific extras.
extern const struct modal_type modal_type_cryptmsg; // Special type of dialogue in Story Mode.
extern const struct modal_type modal_type_linguist; // ''

struct modal_args_story {
  int use_save; // If zero, we start from the beginning and erase any save.
};

struct modal_args_battle {
  int battle; // NS_battle_*
  int players; // NS_players_*
  uint8_t handicap; // 0..128..255 = easy..balanced..hard ("easy" for the left player)
  void (*cb)(struct modal *modal,int outcome,void *userdata); // -1,0,1 = right wins,tie,left wins. Report consequences back to the modal during this callback.
  void *userdata;
  int left_name; // NS_strings_battle. Omit names to suppress the "Player Wins!" message at the end.
  int right_name; // NS_strings_battle
  int skip_prompt;
  int no_store; // Set nonzero to forbid store access. Otherwise we might dirty a fresh store and wipe the saved game.
};

struct modal_args_dialogue {
  const char *text;
  int textc;
  int rid,strix; // Only used if (text) null.
  const struct text_insertion *insv;
  int insc;
  struct sprite *speaker;
  int speakerx,speakery; // Framebuffer pixels; only used if (speaker) null.
  int (*cb)(int optionid,void *userdata); // Return >0 to acknowledge, suppresses sound effects. We dismiss either way. (optionid) zero if cancelled.
  void *userdata;
};

struct modal_args_shop {
  const char *text;
  int textc;
  int rid,strix; // Only used if (text) null.
  const struct text_insertion *insv;
  int insc;
  struct sprite *speaker;
  int speakerx,speakery; // Framebuffer pixels; only used if (speaker) null.
  int (*cb)(int itemid,int quantity,void *userdata); // Return >0 to acknowledge, suppresses ordinary purchasing reaction. (itemid) zero if cancelled.
  int (*cb_validated)(int itemid,int quantity,int price,void *userdata); // Do the quantity and price validation, and call just before committing. Nonzero to abort purchase. Quantity and price are the real final values.
  void *userdata;
};

struct modal_args_cryptmsg {
  const char *src; // G0 + 0xc1..0xda are Old Goblish.
  int srcc;
};

struct modal_args_linguist {
  struct sprite *speaker;
};

/* Initiators of modal_battle should call this during their callback to have consequences reported to the user.
 * If you win a no-quantity item from battle (are we doing that?), use (d==0).
 * This does not effect any changes, it only talks about them.
 */
void modal_battle_add_consequence(struct modal *modal,int itemid,int d);

void modal_pause_click_tabs(struct modal *modal,int x,int y);

int modal_dialogue_add_option(struct modal *modal,int optionid,const char *src,int srcc);
int modal_dialogue_add_option_string(struct modal *modal,int rid,int strix); // convenience; (strix) is (optionid) too.
void modal_dialogue_set_default(struct modal *modal,int optionid); // Must add the option first.
void modal_dialogue_set_callback(struct modal *modal,int (*cb)(int optionid,void *userdata),void *userdata);
struct modal *modal_dialogue_simple(int rid,int strix); // Convenience for static text.

/* Fails on invalid itemid.
 * (quantity) zero to permit any quantity, in which case (price) is per-unit.
 * With a nonzero (quantity), you can only buy that quantity, and (price) is for the set.
 * eg the carpenter makes a fixed quantity of matches, or if you're selling heart containers, obviously you're selling just one.
 */
int modal_shop_add_item(struct modal *modal,int itemid,int price,int quantity);

#endif

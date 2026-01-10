/* modal.h
 * Defines the generic modal, the set of types, and the stack framework.
 */
 
#ifndef MODAL_H
#define MODAL_H

struct modal;
struct modal_type;
struct battle_type;
struct inventory;

/* Generic modal.
 ******************************************************************************/
 
struct modal {
  const struct modal_type *type;
  int defunct;
  int opaque; // Modals below me don't need to render, if nonzero.
  int interactive; // Modals below me won't update, if nonzero.
};

struct modal_type {
  const char *name;
  int objlen;
  void (*del)(struct modal *modal);
  int (*init)(struct modal *modal);
  
  /* The topmost interactive modal, and all above it, will get (update) each frame if implemented.
   * Below that, down to the topmost opaque modal, will get (updatebg).
   * Modals that don't render don't update at all.
   * We're not sending notifications of focus and blur. TODO That might become necessary?
   */
  void (*update)(struct modal *modal,double elapsed);
  void (*updatebg)(struct modal *modal,double elapsed); // I am visible but not interactive.
  
  void (*render)(struct modal *modal); // REQUIRED
};

/* Only the framework should call these.
 */
void modal_del(struct modal *modal);
struct modal *modal_new(const struct modal_type *type);

/* Modal stack.
 * Probably only main and the framework should touch these.
 *******************************************************************************/
 
struct modal *modal_top_of_type(const struct modal_type *type);
struct modal *modal_bottom_of_type(const struct modal_type *type);
int modal_stack_search(const struct modal *modal); // => index in (g.modalv) or -1. Does not dereference (modal).

// Transfer ownership to or from the stack, and list. Pulling does not delete.
int modal_push(struct modal *modal);
void modal_pull(struct modal *modal);

void modal_update_all(double elapsed);
void modal_render_all();
void modal_drop_defunct();

/* Type registry and ctors.
 *****************************************************************************/
 
extern const struct modal_type modal_type_hello;
extern const struct modal_type modal_type_world; // One must exist always, during a story-mode session.
extern const struct modal_type modal_type_battle; // A minigame, either story or arcade mode.
extern const struct modal_type modal_type_dialogue; // Single chunk of text, with optional enumerated responses.

/* Typed ctors always return WEAK; the new modal was pushed to the global stack.
 */
struct modal *modal_new_hello();
struct modal *modal_new_world();
struct modal *modal_new_battle(const struct battle_type *type,int playerc,int handicap);
struct modal *modal_new_pause();
struct modal *modal_new_dialogue(int rid,int strix); // (rid,strix) as a convenience; (0,0) if you're going to configure.

void modal_battle_set_callback(struct modal *modal,void (*cb)(void *userdata,int outcome),void *userdata);

// All dialogue can be cancelled; we'll hit your callback with (choiceid<0).
int modal_dialogue_set_res(struct modal *modal,int rid,int strix); // Static text.
int modal_dialogue_set_text(struct modal *modal,const char *src,int srcc); // You're responsible for language etc.
int modal_dialogue_set_fmt(struct modal *modal,int rid,int strix,const struct text_insertion *insv,int insc);
void modal_dialogue_set_callback(struct modal *modal,void (*cb)(void *userdata,int choiceid),void *userdata);
int modal_dialogue_add_choice_res(struct modal *modal,int choiceid,int rid,int strix); // => choiceid; we make one up if <=0.
int modal_dialogue_add_choice_text(struct modal *modal,int choiceid,const char *src,int srcc);
int modal_dialogue_set_shop(struct modal *modal,const struct inventory *merchv,int merchc); // Prepares options for you. (choiceid) is (itemid). (merch->limit) is the price.

#endif

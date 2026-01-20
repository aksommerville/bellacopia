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

struct modal_args_story {
  int use_save; // If zero, we start from the beginning and erase any save.
};

void modal_pause_click_tabs(struct modal *modal,int x,int y);

#endif

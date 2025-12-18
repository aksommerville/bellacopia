#include "game/game.h"

/* Delete.
 */
 
void modal_del(struct modal *modal) {
  if (!modal) return;
  if (modal->type->del) modal->type->del(modal);
  free(modal);
}

/* New.
 */
 
struct modal *modal_new(const struct modal_type *type) {
  if (!type) return 0;
  struct modal *modal=calloc(1,type->objlen);
  if (!modal) return 0;
  modal->type=type;
  if (type->init&&(type->init(modal)<0)) {
    modal_del(modal);
    return 0;
  }
  return modal;
}

/* Search stack.
 */

struct modal *modal_top_of_type(const struct modal_type *type) {
  int i=g.modalc;
  struct modal **p=g.modalv+i-1;
  for (;i-->0;p--) {
    struct modal *modal=*p;
    if (modal->type==type) return modal;
  }
  return 0;
}

struct modal *modal_bottom_of_type(const struct modal_type *type) {
  int i=g.modalc;
  struct modal **p=g.modalv;
  for (;i-->0;p++) {
    struct modal *modal=*p;
    if (modal->type==type) return modal;
  }
  return 0;
}

int modal_stack_search(const struct modal *modal) {
  if (!modal) return -1;
  int i=0;
  struct modal **p=g.modalv;
  for (;i<g.modalc;i++,p++) {
    if (*p==modal) return i;
  }
  return -1;
}

/* Push to stack.
 */

int modal_push(struct modal *modal) {
  if (!modal) return -1;
  if (modal->defunct) return -1; // Refuse to push a defunct modal, dump your trash somewhere else.
  if (g.modalc>=MODAL_LIMIT) return -1;
  if (modal_stack_search(modal)>=0) return -1;
  g.modalv[g.modalc++]=modal;
  return 0;
}

void modal_pull(struct modal *modal) {
  int p=modal_stack_search(modal);
  if (p<0) return;
  g.modalc--;
  memmove(g.modalv+p,g.modalv+p+1,sizeof(void*)*(g.modalc-p));
}

/* Update the whole stack.
 */

void modal_update_all(double elapsed) {
  int fg=1; // True until after we process an "interactive" modal.
  int i=g.modalc;
  struct modal **p=g.modalv+i-1;
  for (;i-->0;p--) {
    struct modal *modal=*p;
    if (modal->defunct) continue;
    
    if (fg) {
      if (modal->type->update) modal->type->update(modal,elapsed);
      if (modal->interactive) fg=0;
    } else {
      if (modal->type->updatebg) modal->type->updatebg(modal,elapsed);
    }
    
    // If it's opaque, stop. Invisible modals never update.
    if (modal->opaque) break;
  }
}

/* Render the whole stack.
 * Logically we should check defunct and not render those, but you're not supposed to reach render with defunct modals stacked.
 */
 
void modal_render_all() {

  // Locate the topmost opaque modal, possibly none.
  int opaquep=g.modalc;
  struct modal **p=g.modalv+opaquep-1;
  for (;opaquep-->0;p--) {
    struct modal *modal=*p;
    if (modal->opaque) break;
  }
  
  // If (opaquep) landed below zero, black out the framebuffer and clamp it.
  if (opaquep<0) {
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
    opaquep=0;
  }
  
  // Walk from (opaquep) to the top, rendering each.
  int i=g.modalc-opaquep;
  for (p=g.modalv+opaquep;i-->0;p++) {
    struct modal *modal=*p;
    modal->type->render(modal);
  }
}

/* Remove defunct modals from the stack and delete them.
 */
 
void modal_drop_defunct() {
  int i=g.modalc;
  struct modal **p=g.modalv+i-1;
  for (;i-->0;p--) {
    struct modal *modal=*p;
    if (!modal->defunct) continue;
    g.modalc--;
    memmove(p,p+1,sizeof(void*)*(g.modalc-i));
    modal_del(modal);
  }
}

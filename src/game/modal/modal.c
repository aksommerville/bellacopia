#include "game/bellacopia.h"

/* Delete.
 */
 
void modal_del(struct modal *modal) {
  if (!modal) return;
  if (modal==g.modal_focus) g.modal_focus=0;
  if (modal->type->del) modal->type->del(modal);
  free(modal);
}

/* New.
 */
 
struct modal *modal_new(const struct modal_type *type,const void *arg,int argc) {
  if (!type) return 0;
  struct modal *modal=calloc(1,type->objlen);
  if (!modal) return 0;
  modal->type=type;
  if (type->init&&((type->init(modal,arg,argc)<0)||modal->defunct)) {
    modal_del(modal);
    return 0;
  }
  return modal;
}

/* Spawn.
 */

struct modal *modal_spawn(
  const struct modal_type *type,
  const void *arg,int argc
) {
  if (g.modalc>=MODAL_LIMIT) return 0; // Tempting to try dropping defunct, but this might not be a safe moment for it.
  struct modal *modal=modal_new(type,arg,argc);
  if (!modal) return 0;
  if (g.modalc>=MODAL_LIMIT) { // Allow that the modal's init might have inappropriately pushed another modal.
    modal_del(modal);
    return 0;
  }
  g.modalv[g.modalc++]=modal;
  return modal;
}

/* Drop defunct.
 */
 
void modals_drop_defunct() {
  int i=g.modalc;
  while (i-->0) {
    struct modal *modal=g.modalv[i];
    if (!modal->defunct) continue;
    g.modalc--;
    memmove(g.modalv+i,g.modalv+i+1,sizeof(void*)*(g.modalc-i));
    modal_del(modal);
  }
}

/* Validate alive.
 */
 
int modal_is_resident(const struct modal *modal) {
  if (!modal) return 0;
  struct modal **p=g.modalv;
  int i=g.modalc;
  for (;i-->0;p++) if (*p==modal) return 1;
  return 0;
}

/* Find the focus widget from scratch.
 */
 
static struct modal *modals_get_focus() {
  int i=g.modalc;
  struct modal **p=g.modalv+i-1;
  for (;i-->0;p--) if ((*p)->interactive&&!(*p)->defunct) return *p;
  return 0;
}

/* Update all.
 */

void modals_update(double elapsed) {
  // Check focus.
  struct modal *nfocus=modals_get_focus();
  if (nfocus!=g.modal_focus) {
    if (modal_is_resident(g.modal_focus)) {
      if (g.modal_focus->type->focus) g.modal_focus->type->focus(g.modal_focus,0);
    }
    g.modal_focus=nfocus;
    if (nfocus) {
      if (nfocus->type->focus) nfocus->type->focus(nfocus,1);
    }
  }
  // Work from the top down. After updating something with (interactive) set, we're done.
  int i=g.modalc;
  while (i-->0) {
    struct modal *modal=g.modalv[i];
    if (modal->defunct) continue;
    if (modal->type->update) modal->type->update(modal,elapsed);
    if (modal->interactive) break;
  }
  // Then continue iterating, for background updates.
  while (i-->0) {
    struct modal *modal=g.modalv[i];
    if (modal->defunct) continue;
    if (modal->type->updatebg) modal->type->updatebg(modal,elapsed);
  }
}

/* Render all.
 */
 
void modals_render() {

  // Find the topmost opaque modal.
  int opaquep=-1;
  int i=g.modalc;
  while (i-->0) {
    struct modal *modal=g.modalv[i];
    if (modal->defunct) continue;
    if (modal->opaque) {
      opaquep=i;
      break;
    }
  }
  
  // If nothing is opaque, blackout.
  if (opaquep<0) {
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
    opaquep=0;
  }
  
  // Render from (opaquep) up.
  for (i=opaquep;i<g.modalc;i++) {
    struct modal *modal=g.modalv[i];
    if (modal->defunct) continue;
    if (!modal->type->render) continue; // weird not to have this...
    modal->type->render(modal);
  }
}

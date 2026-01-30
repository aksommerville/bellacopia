#include "game/bellacopia.h"

struct sprite_princess {
  struct sprite hdr;
  double cooldown;
};

#define SPRITE ((struct sprite_princess*)sprite)

/* Cleanup.
 */
 
static void _princess_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _princess_init(struct sprite *sprite) {

  /* Already rescued? I will never exist anymore.
   */
  if (store_get_fld(NS_fld_rescued_princess)) return -1;
  
  /* If there's already another Princess, nix this new one. Lord knows, one is plenty.
   * This can happen when we're following Dot but Dot goes back to the dungeon for some reason.
   * We are in monsterlike group, and that should be a pretty small one.
   */
  struct sprite **otherp=GRP(monsterlike)->sprv;
  int i=GRP(monsterlike)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other==sprite) continue;
    if (other->type==&sprite_type_princess) return -1;
  }
  
  return 0;
}

/* Update.
 */
 
static void _princess_update(struct sprite *sprite,double elapsed) {
  if (SPRITE->cooldown>0.0) {
    SPRITE->cooldown-=elapsed;
  }
  //TODO
}

/* Collide.
 */
 
static int princess_cb_gift(int optionid,void *userdata) {
  game_get_item(NS_itemid_vanishing,1);
  return 1;
}
 
static void _princess_collide(struct sprite *sprite,struct sprite *other) {
  if (other->type==&sprite_type_hero) {
    if (store_get_fld(NS_fld_jailopen)) return; // We should be following. No more conversation.
    
    /* Pre-rescue, we do three static dialogue messages:
     *  39 Take this vanishing cream.
     *  40 Where are you?
     *  41 Escape and then rescue me please.
     * With 39, we also give one unit of vanishing cream. We have an infinite supply.
     */
    struct modal_args_dialogue args={
      .rid=RID_strings_dialogue,
      .speaker=sprite,
      .userdata=sprite,
    };
    if (g.vanishing>0.0) { // "Where are you?"
      args.strix=40;
    } else if (possessed_quantity_for_itemid(NS_itemid_vanishing,0)>0) { // "Go and send help!"
      args.strix=41;
    } else { // "Here's some vanishing cream."
      args.strix=39;
      args.cb=princess_cb_gift;
    }
    struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
    if (!modal) return;
    SPRITE->cooldown=0.250;
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_princess={
  .name="princess",
  .objlen=sizeof(struct sprite_princess),
  .del=_princess_del,
  .init=_princess_init,
  .update=_princess_update,
  .collide=_princess_collide,
};

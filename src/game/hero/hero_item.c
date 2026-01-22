#include "hero_internal.h"

/* Broom.
 */
 
static int broom_begin(struct sprite *sprite) {
  SPRITE->itemid_in_progress=NS_itemid_broom;
  return 1;
}

static void broom_end(struct sprite *sprite) {
  SPRITE->itemid_in_progress=0;
}

static void broom_update(struct sprite *sprite,double elapsed) {
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    broom_end(sprite);
    return;
  }
  //TODO broom
}

/* Divining Rod.
 */
 
static int divining_begin(struct sprite *sprite) {
  //TODO Extra feedback for current state. (just report the same thing we do at render, but more forcefully).
  return 1;
}

/* Match.
 */
 
static int match_begin(struct sprite *sprite) {
  //TODO verify inventory
  //TODO Sound effect, and do whatever we're doing to track the lit state.
  return 1;
}

/* Wand.
 */
 
static int wand_begin(struct sprite *sprite) {
  SPRITE->itemid_in_progress=NS_itemid_wand;
  return 1;
}

static void wand_end(struct sprite *sprite) {
  SPRITE->itemid_in_progress=0;
  //TODO Cast spell.
}

static void wand_update(struct sprite *sprite,double elapsed) {
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    wand_end(sprite);
    return;
  }
  //TODO Check for new strokes.
}

/* Fishpole.
 */
 
static int fishpole_begin(struct sprite *sprite) {
  //TODO verify position
  SPRITE->itemid_in_progress=NS_itemid_fishpole;
  return 1;
}

static void fishpole_end(struct sprite *sprite) {
  SPRITE->itemid_in_progress=0;
}

static void fishpole_update(struct sprite *sprite,double elapsed) {
  //TODO Releasing SOUTH doesn't end the mode, it's on a clock. Fudged out for now.
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    fishpole_end(sprite);
    return;
  }
}

/* Bug Spray.
 */
 
static int bugspray_begin(struct sprite *sprite) {
  if (g.store.invstorev[0].quantity<1) return 0;
  g.store.invstorev[0].quantity--;
  g.store.dirty=1;
  g.bugspray+=5.000;
  bm_sound(RID_sound_bugspray);
  return 1;
}

/* Potion.
 */
 
static int potion_begin(struct sprite *sprite) {
  if (g.store.invstorev[0].quantity<1) return 0;
  // Forbid quaffture if HP full, it must be an accident.
  if (store_get_fld16(NS_fld16_hp)>=store_get_fld16(NS_fld16_hpmax)) return 0;
  SPRITE->itemid_in_progress=NS_itemid_potion;
  SPRITE->potionclock=1.000;
  bm_sound(RID_sound_glug);
  return 1;
}

static void potion_update(struct sprite *sprite,double elapsed) {
  double pvclock=SPRITE->potionclock;
  if ((SPRITE->potionclock-=elapsed)<=0.0) {
    SPRITE->itemid_in_progress=0;
  } else if ((SPRITE->potionclock<0.500)&&(pvclock>=0.500)) {
    bm_sound(RID_sound_glug2);
    // Don't assume that potion is still armed; the user could have paused while animating.
    struct invstore *invstore=store_get_itemid(NS_itemid_potion);
    if (invstore&&(invstore->quantity>0)) {
      invstore->quantity--;
      g.store.dirty=1;
    }
    // If the item disappeared or zeroed out, heal anyway i guess? We made the sound.
    store_set_fld16(NS_fld16_hp,store_get_fld16(NS_fld16_hpmax));
  }
}

/* Hookshot.
 */
 
static int hookshot_begin(struct sprite *sprite) {
  SPRITE->itemid_in_progress=NS_itemid_hookshot;
  bm_sound(RID_sound_hookshot_begin);
  return 1;
}

static void hookshot_update(struct sprite *sprite,double elapsed) {
  //TODO Multiple stages, pretty complicated. For now, just exit the state when button released.
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    SPRITE->itemid_in_progress=0;
  }
}

/* Candy.
 */
 
static int candy_begin(struct sprite *sprite) {
  //TODO Inventory, validate position, sound effect, create sprite.
  return 1;
}

/* Vanishing cream.
 */
 
static int vanishing_begin(struct sprite *sprite) {
  if (g.store.invstorev[0].quantity<1) return 0;
  g.store.invstorev[0].quantity--;
  g.store.dirty=1;
  g.vanishing+=5.000;
  bm_sound(RID_sound_vanishing);
  return 1;
}

/* Bell.
 */
 
static int bell_begin(struct sprite *sprite) {
  bm_sound(RID_sound_bell);
  //TODO Visual feedback.
  //TODO Some kind of global alert. Monsters should notice, maybe other things happen.
  return 1;
}

/* Update items, main entry point.
 */
 
void hero_item_update(struct sprite *sprite,double elapsed) {

  /* If an item is being used, it has full control.
   */
  if (SPRITE->itemid_in_progress) {
    switch (SPRITE->itemid_in_progress) {
      case NS_itemid_broom: broom_update(sprite,elapsed); break;
      case NS_itemid_wand: wand_update(sprite,elapsed); break;
      case NS_itemid_fishpole: fishpole_update(sprite,elapsed); break;
      case NS_itemid_hookshot: hookshot_update(sprite,elapsed); break;
      case NS_itemid_potion: potion_update(sprite,elapsed); break;
      default: fprintf(stderr,"%s:%d:ERROR: Item %d is in progress but has no update handler.\n",__FILE__,__LINE__,SPRITE->itemid_in_progress);
    }
    return;
  }
  
  /* Poll input.
   * Call out when SOUTH is pressed. Handler may set itemid_in_progress.
   */
  if (SPRITE->item_blackout) {
    if (g.input[0]&EGG_BTN_SOUTH) return;
    SPRITE->item_blackout=0;
  }
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    int result=0;
    switch (g.store.invstorev[0].itemid) {
      case NS_itemid_broom: result=broom_begin(sprite); break;
      case NS_itemid_divining: result=divining_begin(sprite); break;
      case NS_itemid_match: result=match_begin(sprite); break;
      case NS_itemid_wand: result=wand_begin(sprite); break;
      case NS_itemid_fishpole: result=fishpole_begin(sprite); break;
      case NS_itemid_bugspray: result=bugspray_begin(sprite); break;
      case NS_itemid_potion: result=potion_begin(sprite); break;
      case NS_itemid_hookshot: result=hookshot_begin(sprite); break;
      case NS_itemid_candy: result=candy_begin(sprite); break;
      case NS_itemid_vanishing: result=vanishing_begin(sprite); break;
      case NS_itemid_bell: result=bell_begin(sprite); break;
    }
    if (!result) {
      bm_sound(RID_sound_reject);
    }
  }
}

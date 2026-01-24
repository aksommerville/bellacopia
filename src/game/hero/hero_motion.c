#include "hero_internal.h"

/* Nonzero if a given item in progress should prevent us from walking.
 */
 
static int item_prevents_motion(uint8_t itemid) {
  switch (itemid) {
    //case NS_itemid_broom: // Broom motion is a whole different thing.
    case NS_itemid_wand:
    case NS_itemid_fishpole:
    case NS_itemid_hookshot:
    case NS_itemid_potion:
      return 1;
  }
  return 0;
}

/* Enter or exit walking state.
 * Caller must check and don't call if redundant.
 */
 
static void hero_begin_walk(struct sprite *sprite) {
  SPRITE->walking=1;
  SPRITE->walkanimclock=0.0;
  SPRITE->walkanimframe=0;
}
 
static void hero_end_walk(struct sprite *sprite) {
  SPRITE->walking=0;
}

/* Poll dpad.
 * Fires state changes as warranted.
 */
 
static void hero_motion_update_input(struct sprite *sprite) {

  /* Is an item suppressing motion? If so, end walking and get out.
   */
  if (SPRITE->itemid_in_progress&&item_prevents_motion(SPRITE->itemid_in_progress)) {
    if (SPRITE->walking) hero_end_walk(sprite);
    return;
  }
  
  /* Poll dpad.
   */
  int nindx=0,nindy=0;
  switch (g.input[0]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: nindx=-1; break;
    case EGG_BTN_RIGHT: nindx=1; break;
  }
  switch (g.input[0]&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: nindy=-1; break;
    case EGG_BTN_DOWN: nindy=1; break;
  }
  
  /* Dpad clear, we are not walking.
   */
  if (!nindx&&!nindy) {
    SPRITE->indx=SPRITE->indy=0;
    if (SPRITE->walking) hero_end_walk(sprite);
    return;
  }
  
  /* Ensure (faced) agrees with (nind).
   * A fresh stroke on the dpad should always change direction.
   * And she must never face a direction not held, unless nothing is held.
   */
  if (nindx&&(nindx!=SPRITE->indx)) {
    SPRITE->facedx=nindx;
    SPRITE->facedy=0;
  } else if (nindy&&(nindy!=SPRITE->indy)) {
    SPRITE->facedx=0;
    SPRITE->facedy=nindy;
  } else if (!nindx) {
    SPRITE->facedx=0;
    SPRITE->facedy=nindy;
  } else if (!nindy) {
    SPRITE->facedx=nindx;
    SPRITE->facedy=0;
  } else if (nindx&&(nindx==-SPRITE->facedx)) { // I don't think these clauses will be necessary. Just for safety.
    SPRITE->facedx=nindx;
  } else if (nindy&&(nindy==-SPRITE->facedy)) {
    SPRITE->facedy=nindy;
  }
  if (SPRITE->facedx) SPRITE->broomdx=SPRITE->facedx;
  
  /* Commit the new state, and begin walking if we aren't yet.
   */
  SPRITE->indx=nindx;
  SPRITE->indy=nindy;
  if (!SPRITE->walking) hero_begin_walk(sprite);
}

/* Off-axis correction, when walking is fully prevented.
 */
 
#define FUDGE_TOLERANCE 0.333
#define FUDGE_SPEED 1.000
 
static void hero_fudge_horz(struct sprite *sprite,double elapsed) {
  double whole,fract;
  fract=modf(sprite->x,&whole);
  double dx=0.5-fract;
  if (dx<-FUDGE_TOLERANCE) return;
  if (dx>FUDGE_TOLERANCE) return;
  double target=whole+0.5;
  if (dx<0.0) {
    sprite_move(sprite,-FUDGE_SPEED*elapsed,0.0);
    if (sprite->x<target) sprite->x=target;
  } else if (dx>0.0) {
    sprite_move(sprite,FUDGE_SPEED*elapsed,0.0);
    if (sprite->x>target) sprite->x=target;
  }
}
 
static void hero_fudge_vert(struct sprite *sprite,double elapsed) {
  double whole,fract;
  fract=modf(sprite->y,&whole);
  double dy=0.5-fract;
  if (dy<-FUDGE_TOLERANCE) return;
  if (dy>FUDGE_TOLERANCE) return;
  double target=whole+0.5;
  if (dy<0.0) {
    sprite_move(sprite,0.0,-FUDGE_SPEED*elapsed);
    if (sprite->y<target) sprite->y=target;
  } else if (dy>0.0) {
    sprite_move(sprite,0.0,FUDGE_SPEED*elapsed);
    if (sprite->y>target) sprite->y=target;
  }
}

/* Update motion, main entry point.
 */
 
void hero_motion_update(struct sprite *sprite,double elapsed) {
  hero_motion_update_input(sprite);
  if (!SPRITE->walking) return;
  
  if ((SPRITE->walkanimclock-=elapsed)<=0.0) {
    SPRITE->walkanimclock+=0.150;
    if (++(SPRITE->walkanimframe)>=4) SPRITE->walkanimframe=0;
  }
  
  double speed=6.0*elapsed;
  if (SPRITE->itemid_in_progress==NS_itemid_broom) {
    speed=12.0*elapsed;
  }
  if (!sprite_move(sprite,SPRITE->indx*speed,SPRITE->indy*speed)) {
    // If she's trying to move cardinally, perform an off-axis correction.
    if (SPRITE->indx&&!SPRITE->indy) hero_fudge_vert(sprite,elapsed);
    else if (!SPRITE->indx&&SPRITE->indy) hero_fudge_horz(sprite,elapsed);
  }
}

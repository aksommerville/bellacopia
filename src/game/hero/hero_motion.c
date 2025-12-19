#include "hero_internal.h"

/* Nonzero if it's OK to change the face direction.
 * Normally yes, of course, but there may be items that suspend changes while in play.
 */
 
static int hero_can_change_direction(struct sprite *sprite) {
  return 1;
}

/* End of a walk motion.
 */
 
static void hero_end_walk(struct sprite *sprite) {
  SPRITE->walking=0;
}

/* Begin a walk motion, and confirm we're allowed to.
 * Returns zero if walk not permitted.
 */
 
static int hero_begin_walk(struct sprite *sprite) {
  SPRITE->walking=1;
  return 1;
}

/* When motion is blocked and we're facing that way, check if there's something pushable to actuate in that direction.
 * (x,y) should be one meter ahead in the direction of travel.
 */
 
static void hero_check_push(struct sprite *sprite,double x,double y,double elapsed) {
  // TODO Wall switches?
  // TODO NPC dialogue.
  // TODO Pushable blocks?
}

/* Update motion.
 * Called every frame.
 */
 
void hero_update_motion(struct sprite *sprite,double elapsed) {

  //TODO Broom riding should be its own whole thing, short-circuit out of here.

  // Check new state of gamepad.
  int indx=0,indy=0;
  switch (g.input[0]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: indx=-1; break;
    case EGG_BTN_RIGHT: indx=1; break;
  }
  switch (g.input[0]&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: indy=-1; break;
    case EGG_BTN_DOWN: indy=1; break;
  }
  
  // If an axis changed to nonzero, face that way.
  if (hero_can_change_direction(sprite)) {
    if (indx&&(indx!=SPRITE->indx)) {
      SPRITE->facedx=indx;
      SPRITE->facedy=0;
    } else if (indy&&(indy!=SPRITE->indy)) {
      SPRITE->facedx=0;
      SPRITE->facedy=indy;
    // Or if one axis is zero and the other not, and we're facing the zero way, change it.
    } else if (indx&&!indy&&SPRITE->facedy) {
      SPRITE->facedx=indx;
      SPRITE->facedy=0;
    } else if (!indx&&indy&&SPRITE->facedx) {
      SPRITE->facedx=0;
      SPRITE->facedy=indy;
    }
  }
  SPRITE->indx=indx;
  SPRITE->indy=indy;
  
  // If both inputs are zero, we're not walking.
  if (!indx&&!indy) {
    if (SPRITE->walking) {
      hero_end_walk(sprite);
    }
    return;
  }
  
  // We are walking, per input state. Check business rules and get out if not.
  if (!SPRITE->walking) {
    if (!hero_begin_walk(sprite)) return;
  }
  
  // Move on each axis independently.
  const double speed=6.0; // m/s
  if (SPRITE->indx) {
    if (sprite_move(sprite,SPRITE->indx*speed*elapsed,0.0)) {
      // ok walking horz
    } else if (SPRITE->facedx==SPRITE->indx) {
      hero_check_push(sprite,sprite->x+SPRITE->indx,sprite->y,elapsed);
    } else {
      // blocked but facing vert -- don't actuate anything.
    }
  }
  if (SPRITE->indy) {
    if (sprite_move(sprite,0.0,SPRITE->indy*speed*elapsed)) {
      // ok walking vert
    } else if (SPRITE->facedy==SPRITE->indy) {
      hero_check_push(sprite,sprite->x,sprite->y+SPRITE->indy,elapsed);
    } else {
      // blocked but facing horz -- don't actuate anything.
    }
  }
}

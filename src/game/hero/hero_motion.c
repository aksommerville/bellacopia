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
  SPRITE->animclock=0.0;
  SPRITE->animframe=0;
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

/* When motion is blocked and the other axis is zero, fudge toward neat intervals on the zero axis.
 */
 
#define FUDGELESS_STRETCH 0.200
#define FUDGE_ALIGN_SPEED 1.000
 
static void hero_nudge_align_horz(struct sprite *sprite,double elapsed) {
  // Get the misalignment. If it's close to zero or one, leave it be. She's close to a half-tile, no sense aligning.
  double whole,fract;
  fract=modf(sprite->x,&whole);
  if (sprite->x<0.0) { whole-=1.0; fract+=1.0; }
  if (fract<=FUDGELESS_STRETCH) return;
  if (fract>=1.0-FUDGELESS_STRETCH) return;
  if (fract<0.5) {
    sprite_move(sprite,FUDGE_ALIGN_SPEED*elapsed,0.0);
    if (sprite->x>whole+0.5) sprite->x=whole+0.5;
  } else if (fract>0.5) {
    sprite_move(sprite,-FUDGE_ALIGN_SPEED*elapsed,0.0);
    if (sprite->x<whole+0.5) sprite->x=whole+0.5;
  }
}

static void hero_nudge_align_vert(struct sprite *sprite,double elapsed) {
  double whole,fract;
  fract=modf(sprite->y,&whole);
  if (sprite->y<0.0) { whole-=1.0; fract+=1.0; }
  if (fract<=FUDGELESS_STRETCH) return;
  if (fract>=1.0-FUDGELESS_STRETCH) return;
  if (fract<0.5) {
    sprite_move(sprite,0.0,FUDGE_ALIGN_SPEED*elapsed);
    if (sprite->y>whole+0.5) sprite->y=whole+0.5;
  } else if (fract>0.5) {
    sprite_move(sprite,0.0,-FUDGE_ALIGN_SPEED*elapsed);
    if (sprite->y<whole+0.5) sprite->y=whole+0.5;
  }
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
  double speed=6.0; // m/s
  if (SPRITE->indx) {
    if (sprite_move(sprite,SPRITE->indx*speed*elapsed,0.0)) {
      // ok walking horz
    } else if (SPRITE->facedx==SPRITE->indx) {
      hero_check_push(sprite,sprite->x+SPRITE->indx,sprite->y,elapsed);
      if (!SPRITE->indy) hero_nudge_align_vert(sprite,elapsed);
    } else {
      // blocked but facing vert -- don't actuate anything.
    }
  }
  if (SPRITE->indy) {
    if (sprite_move(sprite,0.0,SPRITE->indy*speed*elapsed)) {
      // ok walking vert
    } else if (SPRITE->facedy==SPRITE->indy) {
      hero_check_push(sprite,sprite->x,sprite->y+SPRITE->indy,elapsed);
      if (!SPRITE->indx) hero_nudge_align_horz(sprite,elapsed);
    } else {
      // blocked but facing horz -- don't actuate anything.
    }
  }
}

/* Exit quantized cell.
 */
 
static void hero_exit_cell(struct sprite *sprite,int x,int y) {
  //TODO treadles?
}

/* Enter quantized cell.
 */
 
static int hero_enter_cell_cb(uint8_t opcode,const uint8_t *arg,void *userdata) {
  struct sprite *sprite=userdata;
  switch (opcode) {
    case CMD_map_door: {
        int mapid=(arg[2]<<8)|arg[3];
        int dstcol=arg[4];
        int dstrow=arg[5];
        camera_enter_door(mapid,dstcol,dstrow);
      } break;
  }
  return 0;
}
 
static void hero_enter_cell(struct sprite *sprite,int x,int y) {
  camera_for_each_poi(x,y,hero_enter_cell_cb,sprite);
}

/* Check quantized position.
 */
 
void hero_check_qpos(struct sprite *sprite) {
  int qx=(int)sprite->x; if (sprite->x<0.0) qx--;
  int qy=(int)sprite->y; if (sprite->y<0.0) qy--;
  if ((qx==SPRITE->qx)&&(qy==SPRITE->qy)) return;
  if (SPRITE->qx||SPRITE->qy) hero_exit_cell(sprite,SPRITE->qx,SPRITE->qy);
  SPRITE->qx=qx;
  SPRITE->qy=qy;
  hero_enter_cell(sprite,qx,qy);
}

/* Forcibly acknowledge quantized position.
 */
 
void sprite_hero_ackpos(struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return;
  SPRITE->qx=(int)sprite->x; if (sprite->x<0.0) SPRITE->qx--;
  SPRITE->qy=(int)sprite->y; if (sprite->y<0.0) SPRITE->qy--;
}

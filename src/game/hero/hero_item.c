#include "hero_internal.h"

/* Broom.
 */
 
static void _broom_update(struct sprite *sprite,double elapsed) {
  //TODO
}

static int _broom_begin(struct sprite *sprite) {
  fprintf(stderr,"%s\n",__func__);
  return 0;//TODO
}

/* Divining Rod.
 */
 
static void _divining_rod_update(struct sprite *sprite,double elapsed) {
  //TODO
}

static int _divining_rod_begin(struct sprite *sprite) {
  fprintf(stderr,"%s\n",__func__);
  return 0;//TODO
}

/* Hookshot.
 */
 
static void _hookshot_update(struct sprite *sprite,double elapsed) {
  //TODO
}

static int _hookshot_begin(struct sprite *sprite) {
  fprintf(stderr,"%s\n",__func__);
  return 0;//TODO
}

/* Fishpole.
 */
 
static void _fishpole_update(struct sprite *sprite,double elapsed) {
  //TODO
}

static int _fishpole_begin(struct sprite *sprite) {
  fprintf(stderr,"%s\n",__func__);
  return 0;//TODO
}

/* Match.
 */

#define LIGHT_DURATION 8.0 /* s per match. Includes ramp-up time but not ramp-down time. */
#define LIGHT_RADIUS 3.5 /* m */
#define LIGHTS_ON_SPEED 4.0 /* m/s */
#define LIGHTS_OFF_SPEED 1.0 /* m/s */
 
static void _match_update(struct sprite *sprite,double elapsed) {
  // This is called whenever matchclock or light_radius is positive. Not just when matches are armed.
  if (SPRITE->matchclock>0.0) {
    SPRITE->matchclock-=elapsed;
    if ((sprite->light_radius+=LIGHTS_ON_SPEED*elapsed)>LIGHT_RADIUS) sprite->light_radius=LIGHT_RADIUS;
  } else {
    if ((sprite->light_radius-=LIGHTS_OFF_SPEED*elapsed)<0.0) sprite->light_radius=0.0;
  }
}

static int _match_begin(struct sprite *sprite) {
  if (g.equipped.quantity<1) return 0;
  g.equipped.quantity--;
  g.inventory_dirty=1;
  bm_sound(RID_sound_match,0.0);
  SPRITE->matchclock+=LIGHT_DURATION;
  return 1;
}

/* Bugspray.
 */

static int _bugspray_begin(struct sprite *sprite) {
  fprintf(stderr,"%s\n",__func__);
  return 0;//TODO
}

/* Candy.
 */

static int _candy_begin(struct sprite *sprite) {
  if (g.equipped.quantity<1) return 0;
  
  /* Confirm we have reasonable headroom in which to place the candy.
   */
  double x=sprite->x,y=sprite->y;
  if (SPRITE->facedx<0) x-=1.0;
  else if (SPRITE->facedx>0) x+=1.0;
  else if (SPRITE->facedy<0) y-=1.0;
  else y+=1.0;
  uint8_t physics=physics_at_sprite_position(x,y,sprite->z);
  switch (physics) {
    case NS_physics_vacant:
    case NS_physics_safe:
      break;
    default: return 0;
  }
  
  struct sprite *candy=sprite_spawn(x,y,RID_sprite_candy,0,0,0,0);
  if (!candy) return 0;
  g.equipped.quantity--;
  g.inventory_dirty=1;
  bm_sound(RID_sound_deploy,0.0);
  return 1;
}

/* Wand.
 */
 
static void _wand_update(struct sprite *sprite,double elapsed) {
  //TODO
}

static int _wand_begin(struct sprite *sprite) {
  fprintf(stderr,"%s\n",__func__);
  return 0;//TODO
}

/* Magnifier.
 */
 
static void _magnifier_update(struct sprite *sprite,double elapsed) {
  //TODO
}

static int _magnifier_begin(struct sprite *sprite) {
  fprintf(stderr,"%s\n",__func__);
  return 0;//TODO
}

/* Update all item activity.
 */
 
void hero_update_item(struct sprite *sprite,double elapsed) {

  // Updating match is special, it can proceed even when other items are being used.
  if ((SPRITE->matchclock>0.0)||(sprite->light_radius>0.0)) {
    _match_update(sprite,elapsed);
  }

  /* If an item is in progress, it takes full control of the update.
   */
  if (SPRITE->itemid_in_progress) switch (SPRITE->itemid_in_progress) {
    case NS_itemid_broom: _broom_update(sprite,elapsed); return;
    case NS_itemid_divining_rod: _divining_rod_update(sprite,elapsed); return;
    case NS_itemid_hookshot: _hookshot_update(sprite,elapsed); return;
    case NS_itemid_fishpole: _fishpole_update(sprite,elapsed); return;
    case NS_itemid_wand: _wand_update(sprite,elapsed); return;
    case NS_itemid_magnifier: _magnifier_update(sprite,elapsed); return;
  }
  
  /* A new stroke of SOUTH starts item usage.
   * Every item has a "_begin" function which must return nonzero to acknowledge.
   * If not acknowledged, we'll play the generic buzzer.
   * "_begin" may assume that (g.equipped) is being used.
   * It alone is responsible for checking and updating quantity, if applicable.
   * "_begin" should set (SPRITE->itemid_in_progress) if it wants ongoing updates.
   */
  if (SPRITE->item_blackout) {
    if (!(g.input[0]&EGG_BTN_SOUTH)) SPRITE->item_blackout=0;
  } else if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    int ack=0;
    switch (g.equipped.itemid) {
      case NS_itemid_broom: ack=_broom_begin(sprite); break;
      case NS_itemid_divining_rod: ack=_divining_rod_begin(sprite); break;
      case NS_itemid_hookshot: ack=_hookshot_begin(sprite); break;
      case NS_itemid_fishpole: ack=_fishpole_begin(sprite); break;
      case NS_itemid_match: ack=_match_begin(sprite); break;
      case NS_itemid_bugspray: ack=_bugspray_begin(sprite); break;
      case NS_itemid_candy: ack=_candy_begin(sprite); break;
      case NS_itemid_wand: ack=_wand_begin(sprite); break;
      case NS_itemid_magnifier: ack=_magnifier_begin(sprite); break;
    }
    if (!ack) {
      bm_sound(RID_sound_reject,0.0);
    }
  }
}

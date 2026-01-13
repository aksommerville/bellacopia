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

static int _divining_rod_begin(struct sprite *sprite) {
  // We just make the sound. There's a visual indicator too, but it's handled entirely by render.
  if (hero_roots_present(sprite)) {
    bm_sound(RID_sound_affirmative,0.0);
  } else {
    bm_sound(RID_sound_negatory,0.0);
  }
  return 0;
}
 
int hero_roots_present(const struct sprite *sprite) {
  return SPRITE->onroot;
}

/* Hookshot.
 */
 
#define HOOKSTAGE_LEAVE 1
#define HOOKSTAGE_RETURN 2
#define HOOKSTAGE_PULL 3
#define HOOKSTAGE_FETCH 4 /* RETURN, but a signal that we have a pumpkin. */

#define HOOKSHOT_TICK_INTERVAL 0.100 /* s; space between sounds for all stages. */
#define HOOKSHOT_LEAVE_SPEED   12.0 /* m/s */
#define HOOKSHOT_RETURN_SPEED  12.0 /* m/s */
#define HOOKSHOT_PULL_SPEED    12.0 /* m/s */
#define HOOKSHOT_DISTANCE_MAX 8.0 /* m */

static void hookshot_end(struct sprite *sprite) {
  SPRITE->itemid_in_progress=0;
  sprite->physics|=(1<<NS_physics_water)|(1<<NS_physics_hole);
  hero_force_safe(sprite);
  if (SPRITE->hookstage==HOOKSTAGE_FETCH) {
    struct sprite **otherp=0;
    int otheri=sprites_get_all(&otherp);
    for (;otheri-->0;otherp++) if ((*otherp)->grabbable==2) (*otherp)->grabbable=1;
  }
}

static void hookshot_abort(struct sprite *sprite) {
  if ((SPRITE->hookstage==HOOKSTAGE_RETURN)||(SPRITE->hookstage==HOOKSTAGE_FETCH)) {
    // Let it return on its own.
  } else if (SPRITE->hookstage==HOOKSTAGE_PULL) {
    // Pulling, abort immediately on release.
    hookshot_end(sprite);
  } else { // Presumably LEAVE, start the return stage.
    SPRITE->hookstage=HOOKSTAGE_RETURN;
  }
}

// Call during LEAVE stage. If we catch something, do whatever needs done.
static void hookshot_check_grab(struct sprite *sprite) {
  double x=sprite->x,y=sprite->y;
  if (SPRITE->facedx<0) {
    x-=SPRITE->hookdistance;
    y+=0.25;
  } else if (SPRITE->facedx>0) {
    x+=SPRITE->hookdistance;
    y+=0.25;
  } else if (SPRITE->facedy<0) {
    x+=0.25;
    y-=SPRITE->hookdistance;
  } else {
    x-=0.25;
    y+=SPRITE->hookdistance;
  }
  uint8_t physics=physics_at_sprite_position(x,y,sprite->z);
  if (physics==NS_physics_hookable) {
    SPRITE->hookstage=HOOKSTAGE_PULL;
    bm_sound(RID_sound_hookshot_grab,0.0);
    sprite->physics&=~((1<<NS_physics_water)|(1<<NS_physics_hole));
  } else if ((physics==NS_physics_solid)||(physics==NS_physics_cliff)) {
    hookshot_abort(sprite);
    bm_sound(RID_sound_hookshot_reject,0.0);
  } else {
    struct sprite **otherp=0;
    int otheri=sprites_get_all(&otherp);
    for (;otheri-->0;otherp++) {
      struct sprite *other=*otherp;
      if (other->defunct) continue;
      if (!other->solid&&!other->grabbable) continue;
      if (other==sprite) continue;
      if (x<=other->x+other->hbl) continue;
      if (x>=other->x+other->hbr) continue;
      if (y<=other->y+other->hbt) continue;
      if (y>=other->y+other->hbb) continue;
      if (other->grabbable) {
        SPRITE->hookstage=HOOKSTAGE_FETCH;
        bm_sound(RID_sound_hookshot_grab,0.0);
        other->grabbable=2; // We won't record the sprite. Instead we'll search for grabbable==2 each frame.
      } else if (other->solid) {
        hookshot_abort(sprite);
        bm_sound(RID_sound_hookshot_reject,0.0);
      }
      break;
    }
  }
}
 
static void _hookshot_update(struct sprite *sprite,double elapsed) {

  // Releasing SOUTH forces "abort" but that does not necessarily end the activity.
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    hookshot_abort(sprite);
    if (!SPRITE->itemid_in_progress) return;
  }
  
  SPRITE->hookclock+=elapsed;
  if (SPRITE->hookclock>=HOOKSHOT_TICK_INTERVAL) {
    SPRITE->hookclock-=HOOKSHOT_TICK_INTERVAL;
    bm_sound((SPRITE->hookstage==HOOKSTAGE_LEAVE)?RID_sound_hookshot_tick:RID_sound_hookshot_untick,0.0);
  }
  switch (SPRITE->hookstage) {
    case HOOKSTAGE_LEAVE: {
        if ((SPRITE->hookdistance+=elapsed*HOOKSHOT_LEAVE_SPEED)>HOOKSHOT_DISTANCE_MAX) {
          hookshot_abort(sprite);
        } else {
          hookshot_check_grab(sprite);
        }
      } break;
    case HOOKSTAGE_RETURN:
    case HOOKSTAGE_FETCH: {
        if ((SPRITE->hookdistance-=elapsed*HOOKSHOT_RETURN_SPEED)<=0.0) {
          hookshot_end(sprite);
        } else if (SPRITE->hookstage==HOOKSTAGE_FETCH) {
          struct sprite *pumpkin=0;
          struct sprite **otherp=0;
          int otheri=sprites_get_all(&otherp);
          for (;otheri-->0;otherp++) {
            struct sprite *other=*otherp;
            if (other->grabbable!=2) continue;
            if (other->defunct) continue;
            pumpkin=other;
            break;
          }
          if (pumpkin) {
            if (!sprite_move(pumpkin,SPRITE->facedx*-HOOKSHOT_RETURN_SPEED*elapsed,SPRITE->facedy*-HOOKSHOT_RETURN_SPEED*elapsed)) {
              pumpkin->grabbable=1;
              SPRITE->hookstage=HOOKSTAGE_RETURN;
            }
          } else {
            SPRITE->hookstage=HOOKSTAGE_RETURN;
          }
        }
      } break;
    case HOOKSTAGE_PULL: {
        double x0=sprite->x,y0=sprite->y;
        double dx=SPRITE->facedx*HOOKSHOT_PULL_SPEED*elapsed;
        double dy=SPRITE->facedy*HOOKSHOT_PULL_SPEED*elapsed;
        if (!sprite_move(sprite,dx,dy)) {
          hookshot_end(sprite);
        } else {
          // Update distance by the actual distance travelled.
          // It's very unlikely that we'll ever reach zero -- the hook is anchored on something solid.
          // But do check for it, and end the activity when we cross zero.
          double d=(sprite->x-x0)+(sprite->y-y0);
          if (d<0.0) d=-d;
          if ((SPRITE->hookdistance-=d)<=0.0) {
            hookshot_end(sprite);
          }
        }
      } break;
  }
}

static int _hookshot_begin(struct sprite *sprite) {
  hero_end_walk(sprite);
  SPRITE->itemid_in_progress=NS_itemid_hookshot;
  SPRITE->hookclock=0.0;
  SPRITE->hookdistance=0.0;
  SPRITE->hookstage=HOOKSTAGE_LEAVE;
  SPRITE->safex=sprite->x;
  SPRITE->safey=sprite->y;
  bm_sound(RID_sound_hookshot_begin,0.0);
  return 1;
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
  if (g.equipped.quantity<1) return 0;
  g.equipped.quantity--;
  g.inventory_dirty=1;
  bm_sound(RID_sound_bugspray,0.0);
  g.bugspray+=10.0;
  return 1;
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
 
static void wand_end(struct sprite *sprite) {
  SPRITE->itemid_in_progress=0;
  if (SPRITE->spellc) {
    // Convert from our direction codes to the more portable letters: LRUD
    uint8_t *p=SPRITE->spell;
    int i=SPRITE->spellc;
    for (;i-->0;p++) switch (*p) {
      case 0x40: *p='U'; break;
      case 0x10: *p='L'; break;
      case 0x08: *p='R'; break;
      case 0x02: *p='D'; break;
      default: *p='X'; break;
    }
    bm_cast_spell((char*)SPRITE->spell,SPRITE->spellc);
  }
}

static void wand_dir(struct sprite *sprite,uint8_t dir) {
  if (dir==SPRITE->wanddir) return;
  if (SPRITE->wanddir=dir) {
    if (SPRITE->spellc<SPELL_LIMIT) {
      bm_sound(RID_sound_wandstroke,0.0);
      SPRITE->spell[SPRITE->spellc++]=dir;
    } else {
      bm_sound(RID_sound_reject,0.0);
    }
  } else {
    bm_sound(RID_sound_wandunstroke,0.0);
  }
}
 
static void _wand_update(struct sprite *sprite,double elapsed) {
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    wand_end(sprite);
    return;
  }
  switch (g.input[0]&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_LEFT: wand_dir(sprite,0x10); break;
    case EGG_BTN_RIGHT: wand_dir(sprite,0x08); break;
    case EGG_BTN_UP: wand_dir(sprite,0x40); break;
    case EGG_BTN_DOWN: wand_dir(sprite,0x02); break;
    default: wand_dir(sprite,0);
  }
}

static int _wand_begin(struct sprite *sprite) {
  hero_end_walk(sprite);
  SPRITE->itemid_in_progress=NS_itemid_wand;
  SPRITE->wanddir=0;
  SPRITE->facedx=0;
  SPRITE->facedy=1;
  SPRITE->spellc=0;
  return 1;
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

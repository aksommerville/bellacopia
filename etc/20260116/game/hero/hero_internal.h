#ifndef HERO_INTERNAL_H
#define HERO_INTERNAL_H

#include "game/game.h"
#include "game/sprite/sprite.h"
#include "game/world/camera.h"
#include "game/world/map.h"

struct sprite_hero {
  struct sprite hdr;
  int facedx,facedy; // Always cardinal, never zero.
  int indx,indy; // Digested dpad state.
  int walking;
  int qx,qy; // Plane meters, quantized.
  int onroot; // (NS_fld_root1..7) or zero
  double animclock;
  int animframe;
  uint8_t itemid_in_progress; // Zero if none. Not necessarily the equipped item.
  int item_blackout; // If nonzero, wait for SOUTH to release.
  double matchclock;
  uint8_t wanddir;
  uint8_t spell[SPELL_LIMIT]; // (0x40,0x10,0x08,0x02)
  int spellc;
  double hookclock; // For the tick sound only.
  double hookdistance;
  int hookstage;
  double safex,safey; // Set before a risky operation like hookshot, for the position we can return to.
  double broomdir; // Clockwise radians from Up.
  int broom_rotate_blackout;
  double broomspeed;
};

#define SPRITE ((struct sprite_hero*)sprite)

void hero_update_item(struct sprite *sprite,double elapsed);
void hero_update_motion(struct sprite *sprite,double elapsed);
void hero_check_qpos(struct sprite *sprite);
int hero_roots_present(const struct sprite *sprite);

void hero_end_walk(struct sprite *sprite);

/* Fudge my position to escape minor collisions, or jump to (safex,safey) if we don't find a legal position nearby.
 */
void hero_force_safe(struct sprite *sprite);

#endif

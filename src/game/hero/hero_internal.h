#ifndef HERO_INTERNAL_H
#define HERO_INTERNAL_H

#include "game/bellacopia.h"

struct sprite_hero {
  struct sprite hdr;
  int facedx,facedy; // Always a cardinal unit vector.
  int indx,indy; // State of gamepad. -1..1.
  int item_blackout; // Wait for SOUTH to release.
  uint8_t itemid_in_progress;
  int walking;
  double walkanimclock;
  int walkanimframe;
  double potionclock; // Animating potion.
  int qx,qy,root; // Stays fresh passively while divining rod armed. Otherwise (-1,-1,0). Most POI interactions use feet.
  struct divining_alert {
    int x,y; // plane pixels, to the center of the cell.
    uint8_t tileid;
  } divining_alertv[9];
  double divining_alert_clock;
  char wanddir; // 0,'LRUD'
  char spell[SPELL_LIMIT]; // 'LRUD.'
  int spellc;
  double matchclock; // Add to GRP(light) when increasing. Removal is automatic.
  int compassz; // <0 to refresh lazily
  double compassx,compassy; // <0 if not found.
  double compasst; // Angle for the visual indicator. Only relevant if (x,y,z)>=0.
  
  // For door transitions.
  int door_listener; // Nonzero if transition in progress. It's a map listenerid from the camera.
  double door_clock; // If this reaches zero, assume the transition failed and abort.
  double doorx,doory; // Warp me here when the transition takes effect.
  int ignoreqx,ignoreqy; // My quantized position *in just one map* immediately after a door transition. Ignore any POI that arrive here, until we've moved some.
};

#define SPRITE ((struct sprite_hero*)sprite)

void hero_render(struct sprite *sprite,int x,int y);

// hero_motion.c
void hero_motion_update(struct sprite *sprite,double elapsed);

// hero_item.c
void hero_item_update(struct sprite *sprite,double elapsed);

#endif

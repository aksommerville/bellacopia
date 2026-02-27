/* sprite_escalator.c
 * A broken escalator. Occupied multiple rows.
 * When you step on it, it pushes you downward.
 */
 
#include "game/bellacopia.h"

#define SLIPPAGE_RATE_UP   1.000 /* hz, how long to reach maximum slippage after contact. */
#define SLIPPAGE_RATE_DOWN 0.500 /* '', decay after contact loss. */
#define SLIP_SPEED 10.0 /* m/s, top speed. Must be greater than the hero's walk speed. */

struct sprite_escalator {
  struct sprite hdr;
  uint8_t tileid0;
  int h;
  double slippage; // 0..1
  double subanim; // pixels, makes our tile track delivered motion exactly.
  int animframe;
};

#define SPRITE ((struct sprite_escalator*)sprite)

static int _escalator_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->h=sprite->arg[0];
  if ((SPRITE->h<1)||(SPRITE->h>NS_sys_maph)) {
    fprintf(stderr,"%s: Invalid height\n",__func__);
    return -1;
  }
  return 0;
}

static void escalator_move_pumpkins(struct sprite *sprite,double dy) {
  const double taper_distance=1.0;
  double l=sprite->x-0.5;
  double r=sprite->x+0.5;
  double t=sprite->y-0.5;
  double b=sprite->y+SPRITE->h-0.5; // sic '-': we mean (h-1)
  b+=0.5; // Push a little further than the detection bounds, to mitigate jitter.
  struct sprite **otherp=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other->x<=l) continue;
    if (other->x>=r) continue;
    if (other->y<=t) continue;
    if (other->y>=b) continue;
    if (sprite_group_has(GRP(floating),other)) continue;
    double db=b-other->y;
    if (db<taper_distance) {
      sprite_move(other,0.0,(dy*db)/taper_distance);
    } else {
      sprite_move(other,0.0,dy);
    }
  }
}

static int escalator_has_contact(struct sprite *sprite) {
  double l=sprite->x-0.5;
  double r=sprite->x+0.5;
  double t=sprite->y-0.5;
  double b=sprite->y+SPRITE->h-0.5;
  struct sprite **otherp=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other->x<=l) continue;
    if (other->x>=r) continue;
    if (other->y<=t) continue;
    if (other->y>=b) continue;
    if (sprite_group_has(GRP(floating),other)) continue;
    return 1;
  }
  return 0;
}

static void _escalator_update(struct sprite *sprite,double elapsed) {
  if (escalator_has_contact(sprite)) {
    if ((SPRITE->slippage+=SLIPPAGE_RATE_UP*elapsed)>=1.0) SPRITE->slippage=1.0;
  } else {
    if ((SPRITE->slippage-=SLIPPAGE_RATE_DOWN*elapsed)<=0.0) SPRITE->slippage=0.0;
  }
  if (SPRITE->slippage>0.0) {
    double dy=SLIP_SPEED*SPRITE->slippage*elapsed;
    escalator_move_pumpkins(sprite,dy);
    SPRITE->subanim+=dy*NS_sys_tilesize;
    while (SPRITE->subanim>=1.0) {
      SPRITE->animframe++;
      SPRITE->subanim-=1.0;
    }
    SPRITE->animframe&=15;
  }
}

static void _escalator_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  uint8_t tileid=SPRITE->tileid0+SPRITE->animframe;
  int i=SPRITE->h;
  for (;i-->0;y+=NS_sys_tilesize) {
    graf_tile(&g.graf,x,y,tileid,0);
  }
}

const struct sprite_type sprite_type_escalator={
  .name="escalator",
  .objlen=sizeof(struct sprite_escalator),
  .init=_escalator_init,
  .update=_escalator_update,
  .render=_escalator_render,
};

/* sprite_motion_economist.c
 * This is a gag from my old game Economy of Motion: You have to walk thru the scene in under some threshold of keystrokes.
 * Our flag is ON by default and switches OFF when you breach the threshold.
 * So the door's tile should be CLOSED by default and OPEN when the flag is set. (I'm thinking to use the goblins' jail door).
 * Our sprite appears in the scene as the step indicator -- same appearance as sprite_tenkey.
 */
 
#include "game/bellacopia.h"

#define RESET_DISTANCE   8.0
#define RESET_DISTANCE_2 (RESET_DISTANCE*RESET_DISTANCE)

struct sprite_motion_economist {
  struct sprite hdr;
  int fldid;
  int threshold;
  int stepc;
  int inrange;
  uint16_t instate;
  int tripped;
};

#define SPRITE ((struct sprite_motion_economist*)sprite)

static void _motion_economist_del(struct sprite *sprite) {
}

static int _motion_economist_init(struct sprite *sprite) {
  SPRITE->fldid=(sprite->arg[0]<<8)|sprite->arg[1];
  SPRITE->threshold=(sprite->arg[2]<<8)|sprite->arg[3];
  if (!store_get_fld(SPRITE->fldid)) {
    store_set_fld(SPRITE->fldid,1); // Flag is ON by default. Instantiating us implicitly resets everything.
    g.camera.mapsdirty=1;
  }
  return 0;
}

static void _motion_economist_update(struct sprite *sprite,double elapsed) {

  /* Reset if the hero is far away, and track whether in range.
   */
  int ninrange=0;
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    double dx=hero->x-sprite->x;
    double dy=hero->y-sprite->y;
    double d2=dx*dx+dy*dy;
    ninrange=(d2<RESET_DISTANCE_2);
  }
  if (ninrange!=SPRITE->inrange) {
    fprintf(stderr,"motion_economist: hero %s range. fldid=%d\n",ninrange?"IN":"OUT OF",SPRITE->fldid);
    SPRITE->inrange=ninrange;
    SPRITE->stepc=0;
    if (ninrange) { // Newly in range, don't count the keystroke that brought her in.
      SPRITE->instate=g.input[0];
    } else { // Reset to default ie ON, when going out of range.
      if (!store_get_fld(SPRITE->fldid)) {
        store_set_fld(SPRITE->fldid,1);
        g.camera.mapsdirty=1;
        bm_sound(RID_sound_ding);
      }
      SPRITE->instate=0;
      SPRITE->tripped=0;
    }
  }

  /* If in range, track the dpad.
   */
  if (SPRITE->inrange&&!SPRITE->tripped) {
    uint8_t nstate=g.input[0];
    if (nstate!=SPRITE->instate) {
      // Any new dpad bit, tick the counter. Once per frame. So a really careful player could exploit this to get a diagonal for 1 point instead of 2.
      const uint16_t dpad=(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN);
      if ((nstate&dpad)&~(SPRITE->instate&dpad)) {
        SPRITE->stepc++;
        if (SPRITE->stepc>=SPRITE->threshold) {
          fprintf(stderr,"motion_economist: Trip alarm, stepc==threshold==%d\n",SPRITE->threshold);
          store_set_fld(SPRITE->fldid,0);
          SPRITE->tripped=1;
          bm_sound(RID_sound_reject);
          g.camera.mapsdirty=1;
        }
      }
      SPRITE->instate=nstate;
    }
  }
}

static void _motion_economist_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  if (SPRITE->tripped) {
    graf_tile(&g.graf,x,y,sprite->tileid+11,0);
  } else {
    graf_tile(&g.graf,x,y,sprite->tileid,0);
  }
  // Digits are spaced horizontally by 4 pixels, and the most significant is aligned with the base tile.
  if (SPRITE->stepc>=100) graf_tile(&g.graf,x,y,sprite->tileid+1+(SPRITE->stepc/100)%10,0);
  if (SPRITE->stepc>=10) graf_tile(&g.graf,x+4,y,sprite->tileid+1+(SPRITE->stepc/10)%10,0);
  graf_tile(&g.graf,x+8,y,sprite->tileid+1+SPRITE->stepc%10,0);
}

const struct sprite_type sprite_type_motion_economist={
  .name="motion_economist",
  .objlen=sizeof(struct sprite_motion_economist),
  .del=_motion_economist_del,
  .init=_motion_economist_init,
  .update=_motion_economist_update,
  .render=_motion_economist_render,
};

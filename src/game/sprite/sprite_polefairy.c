/* sprite_polefairy.c
 * Summoned by camera when the player has gone too far north or south.
 * We'll fly in from the edge and stop somewhere, then offer to warp back home when confronted.
 */

#include "game/game.h"
#include "sprite.h"
#include "game/world/camera.h"

#define FLY_SPEED 3.0

struct sprite_polefairy {
  struct sprite hdr;
  uint8_t tileid0;
  double animclock;
  int animframe;
  double tx,ty;
  double blackout;
};

#define SPRITE ((struct sprite_polefairy*)sprite)

/* Init.
 */
 
static int _polefairy_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->tx=sprite->x;
  SPRITE->ty=sprite->y;
  return 0;
}

/* Update.
 */
 
static void _polefairy_update(struct sprite *sprite,double elapsed) {

  // Face the hero, even if we're travelling the other way.
  struct sprite *hero=sprites_get_hero();
  if (hero) {
    if (hero->x<sprite->x) sprite->xform=EGG_XFORM_XREV;
    else sprite->xform=0;
  }

  if (SPRITE->blackout>0.0) SPRITE->blackout-=elapsed;

  // Animation, always.
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.125;
    if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    switch (SPRITE->animframe) {
      case 0: sprite->tileid=SPRITE->tileid0+0; break;
      case 1: sprite->tileid=SPRITE->tileid0+1; break;
      case 2: sprite->tileid=SPRITE->tileid0+2; break;
      case 3: sprite->tileid=SPRITE->tileid0+1; break;
    }
  }

  // Motion.
  double dx=SPRITE->tx-sprite->x;
  double dy=SPRITE->ty-sprite->y;
  double d2=dx*dx+dy*dy;
  if (d2>=0.100) {
    double d=sqrt(d2);
    sprite_move(sprite,(dx*FLY_SPEED*elapsed)/d,0.0);
    sprite_move(sprite,0.0,(dy*FLY_SPEED*elapsed)/d);
  }
}

/* Collide.
 */
 
static void polefairy_cb_dialogue(void *userdata,int choiceid) {
  struct sprite *sprite=userdata;
  if (choiceid==2) { // Go home. Anything else is either "Stay" or cancel, noop.
    camera_warp_home_soon();
  }
}
 
static void _polefairy_collide(struct sprite *sprite,struct sprite *other) {
  if (SPRITE->blackout>0.0) return;
  if (other->type==&sprite_type_hero) {
    struct modal *modal=modal_new_dialogue(RID_strings_dialogue,1);
    if (!modal) return;
    modal_dialogue_set_callback(modal,polefairy_cb_dialogue,sprite);
    modal_dialogue_add_choice_res(modal,1,RID_strings_dialogue,3);
    modal_dialogue_add_choice_res(modal,2,RID_strings_dialogue,2);
    // Once spoken to, we remain wherever we are. Important, if we're travelling in a direction that collides with the hero!
    SPRITE->tx=sprite->x;
    SPRITE->ty=sprite->y;
    // And on top of that, prevent retrigger for a very short interval.
    // This can matter, if both sprites acknowledged the collision.
    SPRITE->blackout=0.250;
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_polefairy={
  .name="polefairy",
  .objlen=sizeof(struct sprite_polefairy),
  .init=_polefairy_init,
  .update=_polefairy_update,
  .collide=_polefairy_collide,
};

/* Set target.
 */
 
void sprite_polefairy_set_target(struct sprite *sprite,double x,double y) {
  if (!sprite||(sprite->type!=&sprite_type_polefairy)) return;
  SPRITE->tx=x;
  SPRITE->ty=y;
  if (x<sprite->x) {
    sprite->xform=EGG_XFORM_XREV;
  } else {
    sprite->xform=0;
  }
}

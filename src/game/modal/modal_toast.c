#include "game/bellacopia.h"

#define TRAVEL_TIME 0.500 /* enterclock,exitclock */

struct modal_toast {
  struct modal hdr;
  int fld16;
  double ttl;
  double enterclock;
  double presence; // Computed at each update, per (enterclock,ttl). 0..1=away..ready
};

#define MODAL ((struct modal_toast*)modal)

/* Cleanup.
 */
 
static void _toast_del(struct modal *modal) {
}

/* Init.
 */
 
static int _toast_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=0;
  modal->interactive=0;
  modal->stay_on_top=1;
  MODAL->enterclock=TRAVEL_TIME;
  
  if (arg&&(argc==sizeof(struct modal_args_toast))) {
    const struct modal_args_toast *ARG=arg;
    MODAL->fld16=ARG->fld16;
  }
  
  /* Message-specific setup.
   */
  switch (MODAL->fld16) {
    case NS_fld16_goodluck: {
        MODAL->ttl=3.0;
      } break;
    default: {
        fprintf(stderr,"Unexpected params for modal_toast. fld16=%d\n",MODAL->fld16);
        return -1;
      }
  }
  
  return 0;
}

/* goodluck
 */
 
static void toast_update_goodluck(struct modal *modal,double elapsed) {
}

static void toast_render_goodluck(struct modal *modal) {
  int midy=-16+(int)(MODAL->presence*48.0);
  int totalw=96;
  int xlo=(FBW>>1)-(totalw>>1);
  graf_set_image(&g.graf,RID_image_modalbits);
  
  double t=MODAL->ttl*3.0;
  int cloverx=xlo+16;
  graf_set_filter(&g.graf,1);
  graf_decal_rotate(&g.graf,cloverx,midy,0,0,32,sin(t),cos(t),1.0);
  graf_set_filter(&g.graf,0);
  
  int msgx=xlo+32;
  int msgy=midy-8;
  graf_decal(&g.graf,msgx,msgy,32,0,64,16);
}

/* Update.
 */
 
static void _toast_update(struct modal *modal,double elapsed) {

  // Unceremoniously defunct when ttl expires.
  if ((MODAL->ttl-=elapsed)<=0.0) {
    modal->defunct=1;
    MODAL->presence=0.0;
    return;
  }
  
  // If the enter clock is running, tick it down. Or if (ttl<TRAVEL_TIME), it's a reverse enterclock. Then compute (presence).
  if (MODAL->enterclock>0.0) {
    MODAL->enterclock-=elapsed;
    MODAL->presence=1.0-MODAL->enterclock/TRAVEL_TIME;
  } else if (MODAL->ttl<TRAVEL_TIME) {
    MODAL->presence=MODAL->ttl/TRAVEL_TIME;
  } else {
    MODAL->presence=1.0;
  }
  if (MODAL->presence<0.0) MODAL->presence=0.0;
  else if (MODAL->presence>1.0) MODAL->presence=1.0;
  
  // Update per message.
  switch (MODAL->fld16) {
    case NS_fld16_goodluck: toast_update_goodluck(modal,elapsed); break;
  }
}

/* Render.
 * Just dispatch based on message, and short-circuit if offscreen.
 */
 
static void _toast_render(struct modal *modal) {
  if (MODAL->presence<=0.0) return;
  switch (MODAL->fld16) {
    case NS_fld16_goodluck: toast_render_goodluck(modal); break;
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_toast={
  .name="toast",
  .objlen=sizeof(struct modal_toast),
  .del=_toast_del,
  .init=_toast_init,
  .update=_toast_update,
  .render=_toast_render,
};

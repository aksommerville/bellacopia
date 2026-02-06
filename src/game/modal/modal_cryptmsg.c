#include "game/bellacopia.h"

#define RISE_SPEED 300.0 /* px/s */
#define FALL_SPEED 400.0 /* px/s */
#define MSG_LIMIT 256

struct modal_cryptmsg {
  struct modal hdr;
  double y;
  double dy;
  char msg[256];
  int msgc;
};

#define MODAL ((struct modal_cryptmsg*)modal)

/* Cleanup.
 */
 
static void _cryptmsg_del(struct modal *modal) {
}

/* Init.
 */
 
static int _cryptmsg_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=0;
  modal->interactive=1;
  modal->blotter=1;
  
  MODAL->y=FBH;
  MODAL->dy=-RISE_SPEED;
  
  if (arg&&(argc==sizeof(struct modal_args_cryptmsg))) {
    const struct modal_args_cryptmsg *args=arg;
    if (args->src) {
      if (args->srcc>MSG_LIMIT) MODAL->msgc=MSG_LIMIT;
      else MODAL->msgc=args->srcc;
      memcpy(MODAL->msg,args->src,MODAL->msgc);
    }
  }
  
  return 0;
}

/* Dismiss.
 */
 
static void cryptmsg_dismiss(struct modal *modal) {
  if (MODAL->dy>0.0) return; // Already dismissing.
  bm_sound(RID_sound_uicancel);
  MODAL->dy=FALL_SPEED;
  //modal->blotter=0;
}

/* Update.
 */
 
static void _cryptmsg_update(struct modal *modal,double elapsed) {

  if (MODAL->dy<0.0) {
    if ((MODAL->y+=MODAL->dy*elapsed)<=0.0) {
      MODAL->y=0.0;
      MODAL->dy=0.0;
    }
  } else if (MODAL->dy>0.0) {
    if ((MODAL->y+=MODAL->dy*elapsed)>=FBH) {
      modal->defunct=1;
    }
  }

  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) cryptmsg_dismiss(modal);
  else if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) cryptmsg_dismiss(modal);
}

/* Render.
 */
 
static void _cryptmsg_render(struct modal *modal) {
  int topy=(int)MODAL->y;
  graf_set_image(&g.graf,RID_image_cryptmsg);
  graf_decal(&g.graf,0,topy,0,0,FBW,FBH);
  
  // It seems a bit overkill to break lines on every single render...
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_set_tint(&g.graf,0x7495a4ff);
  const int xa=60;
  const int xz=FBW-xa;
  int y=topy+64,x=xa;
  int msgp=0;
  while (msgp<MODAL->msgc) {
    if ((unsigned char)MODAL->msg[msgp]<=0x20) {
      msgp++;
      x+=8;
      continue;
    }
    const char *word=MODAL->msg+msgp;
    int wordc=0;
    while ((msgp<MODAL->msgc)&&((unsigned char)MODAL->msg[msgp++]>0x20)) wordc++;
    int wordlen=8*wordc;
    if ((x>xa)&&(x+wordlen>xz)) {
      x=xa;
      y+=8;
    }
    for (;wordc-->0;word++,x+=8) {
      graf_tile(&g.graf,x,y,*word,0);
    }
    x+=8;
  }
  graf_set_tint(&g.graf,0);
}

/* Type definition.
 */
 
const struct modal_type modal_type_cryptmsg={
  .name="cryptmsg",
  .objlen=sizeof(struct modal_cryptmsg),
  .del=_cryptmsg_del,
  .init=_cryptmsg_init,
  .update=_cryptmsg_update,
  .render=_cryptmsg_render,
};

#include "game/bellacopia.h"

#define RAISE_RATE 150.00
#define FALL_RATE  250.0
#define ALPHA_RATE 0.500

struct modal_crystal {
  struct modal hdr;
  double lower; // Framebuffer pixels. Zero at rest. Rise from and fall to FBH.
  double dlower;
  int msg_texid;
  int msgx,msgy,msgw,msgh; // Message only appears when fully raised; these don't change.
  double msgalpha;
};

#define MODAL ((struct modal_crystal*)modal)

/* Cleanup.
 */
 
static void _crystal_del(struct modal *modal) {
  egg_texture_del(MODAL->msg_texid);
}

/* Init.
 */
 
static int _crystal_init(struct modal *modal,const void *args,int argslen) {
  modal->opaque=0;
  modal->interactive=1;
  modal->blotter=1;
  
  MODAL->lower=FBH;
  MODAL->dlower=-RAISE_RATE;
  
  char src[256];
  int srcc=game_get_advice(src,sizeof(src));
  if ((srcc<0)||(srcc>sizeof(src))) srcc=0;
  
  MODAL->msg_texid=font_render_to_texture(0,g.font,src,srcc,FBW/3,FBH/3,0x000000ff);
  egg_texture_get_size(&MODAL->msgw,&MODAL->msgh,MODAL->msg_texid);
  MODAL->msgx=(FBW>>1)-(MODAL->msgw>>1);
  MODAL->msgy=(FBH>>1)-(MODAL->msgh>>1);
  MODAL->msgalpha=0.0;
  
  return 0;
}

/* Dismiss.
 */
 
static void crystal_dismiss(struct modal *modal) {
  if (MODAL->dlower>0.0) return;
  bm_sound(RID_sound_uicancel);
  MODAL->dlower=FALL_RATE;
  MODAL->msgalpha=0.0;
}

/* Update.
 */
 
static void _crystal_update(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) crystal_dismiss(modal);
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) crystal_dismiss(modal);
  if (MODAL->dlower<0.0) {
    if ((MODAL->lower+=MODAL->dlower*elapsed)<=0.0) {
      MODAL->dlower=0.0;
    }
  } else if (MODAL->dlower>0.0) {
    if ((MODAL->lower+=MODAL->dlower*elapsed)>=FBH) {
      modal->defunct=1;
    }
  } else if (MODAL->msgalpha<1.0) {
    if ((MODAL->msgalpha+=ALPHA_RATE*elapsed)>1.0) {
      MODAL->msgalpha=1.0;
    }
  }
}

/* Render.
 */
 
static void _crystal_render(struct modal *modal) {

  // Background image can rise and fall. Must be the size of the framebuffer.
  graf_set_image(&g.graf,RID_image_crystal);
  int dsty=(int)MODAL->lower;
  graf_decal(&g.graf,0,dsty,0,0,FBW,FBH);
  
  // The message.
  if (MODAL->msgalpha>0.0) {
    int alpha=(int)(MODAL->msgalpha*255.0);
    if (alpha>0xff) alpha=0xff;
    graf_set_alpha(&g.graf,alpha);
    graf_set_input(&g.graf,MODAL->msg_texid);
    graf_decal(&g.graf,MODAL->msgx,MODAL->msgy,0,0,MODAL->msgw,MODAL->msgh);
    graf_set_alpha(&g.graf,0xff);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_crystal={
  .name="crystal",
  .objlen=sizeof(struct modal_crystal),
  .del=_crystal_del,
  .init=_crystal_init,
  .update=_crystal_update,
  .render=_crystal_render,
};

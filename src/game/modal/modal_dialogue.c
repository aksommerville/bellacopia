#include "game/bellacopia.h"

#define STAGE_HELLO 1
#define STAGE_WAIT 2
#define STAGE_FAREWELL 3

#define MARGINL 4
#define MARGINR 4
#define MARGINT 2
#define MARGINB 1
#define CURSOR_MARGIN 16
#define SPEAKERSPACING 10 /* It's typically the center of a sprite, so at least 8 to clear the sprite. */
#define SUMMON_SPEED 5.000 /* hz */
#define DISMISS_SPEED 5.000 /* hz */

struct modal_dialogue {
  struct modal hdr;
  int stage;
  int x0,y0; // Single point that we voop to and from at summon and dismiss.
  int texid,texw,texh;
  int texx,texy; // Within (box).
  int boxx,boxy,boxw,boxh; // Final bounds when stable. Larger than (texw,texh).
  double presence; // 0=dismissed .. 1=present. Controls box position and blotter.
  struct option {
    int texid;
    int x,y,w,h;
    int optionid;
  } *optionv;
  int optionc,optiona;
  int optionp;
  int boxdirty;
  int (*cb)(int optionid,void *userdata);
  void *userdata;
};

#define MODAL ((struct modal_dialogue*)modal)

/* Cleanup.
 */
 
static void _dialogue_del(struct modal *modal) {
  egg_texture_del(MODAL->texid);
  if (MODAL->optionv) {
    while (MODAL->optionc-->0) egg_texture_del(MODAL->optionv[MODAL->optionc].texid);
    free(MODAL->optionv);
  }
}

/* Set (boxx,boxy,boxw,boxh,texx,texy) close to (x0,y0), respecting established bounds and options.
 */
 
static void dialogue_reposition(struct modal *modal) {
  if (!MODAL->optionc&&(MODAL->texw<1)) return;
  
  MODAL->boxw=MODAL->texw;
  MODAL->boxh=MODAL->texh+MARGINT+MARGINB;
  struct option *option=MODAL->optionv;
  int i=MODAL->optionc;
  for (;i-->0;option++) {
    MODAL->boxh+=option->h;
    int w1=CURSOR_MARGIN+option->w;
    if (w1>MODAL->boxw) MODAL->boxw=w1;
  }
  MODAL->boxw+=MARGINL+MARGINR;
  
  /* Prefer above the focus point.
   * If that breaches the top, use the bottom and then clamp to bottom of framebuffer.
   * Dialogue is usually triggered by hero action, so the speaker is usually near the screen's center.
   * Which means we pretty much always get what we want.
   */
  MODAL->boxy=MODAL->y0-SPEAKERSPACING-MODAL->boxh;
  if (MODAL->boxy<0) {
    MODAL->boxy=MODAL->y0+SPEAKERSPACING;
    if (MODAL->boxy>FBH-MODAL->boxh) MODAL->boxy=FBH-MODAL->boxh;
  }
  
  /* Center on speaker horizontally, then clamp to framebuffer.
   */
  MODAL->boxx=MODAL->x0-(MODAL->boxw>>1);
  if (MODAL->boxx<0) MODAL->boxx=0;
  else if (MODAL->boxx>FBW-MODAL->boxw) MODAL->boxx=FBW-MODAL->boxw;
  
  MODAL->texx=MODAL->boxx+MARGINL;
  MODAL->texy=MODAL->boxy+MARGINT;
  int ox=MODAL->boxx+MARGINL+CURSOR_MARGIN;
  int oy=MODAL->boxy+MARGINT+MODAL->texh;
  for (i=MODAL->optionc,option=MODAL->optionv;i-->0;option++) {
    option->y=oy;
    option->x=ox;
    oy+=option->h;
  }
}

/* Set text.
 */
 
static int dialogue_set_text(struct modal *modal,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (MODAL->texid) egg_texture_del(MODAL->texid);
  MODAL->texid=font_render_to_texture(0,g.font,src,srcc,FBW>>1,FBH>>1,0xffffffff);
  egg_texture_get_size(&MODAL->texw,&MODAL->texh,MODAL->texid);
  dialogue_reposition(modal);
  return 0;
}

/* Prepare origin from a framebuffer point.
 */
 
static void dialogue_origin_fb(struct modal *modal,int x,int y) {
  if (x<0) MODAL->x0=0;
  else if (x>=FBW) MODAL->x0=FBW;
  else MODAL->x0=x;
  if (y<0) MODAL->y0=0;
  else if (y>=FBH) MODAL->y0=FBH;
  else MODAL->y0=y;
  dialogue_reposition(modal);
}

/* Prepare origin from a sprite.
 */
 
static void dialogue_origin_sprite(struct modal *modal,const struct sprite *sprite) {
  if (!sprite) return;
  int x=(int)(sprite->x*NS_sys_tilesize)-g.camera.rx;
  int y=(int)(sprite->y*NS_sys_tilesize)-g.camera.ry;
  dialogue_origin_fb(modal,x,y);
}

/* Init.
 */
 
static int _dialogue_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=0;
  modal->interactive=1;
  modal->blotter=1;
  
  MODAL->x0=FBW>>1;
  MODAL->y0=FBH>>1;
  MODAL->stage=STAGE_HELLO;
  
  if (arg&&(argc==sizeof(struct modal_args_dialogue))) {
    const struct modal_args_dialogue *args=arg;
    const char *text=0;
    int textc=0;
    if (args->text) {
      text=args->text;
      textc=args->textc;
    } else if (args->rid&&args->strix) {
      textc=text_get_string(&text,args->rid,args->strix);
    }
    if (!text) textc=0; else if (textc<0) { textc=0; while (text[textc]) textc++; }
    if (args->insv&&args->insc) {
      char tmp[1024];
      int tmpc=text_format(tmp,sizeof(tmp),text,textc,args->insv,args->insc);
      if ((tmpc<0)||(tmpc>sizeof(tmp))) {
        fprintf(stderr,"WARNING: strings:%d:%d yielded %d bytes. (lang=%d)\n",args->rid,args->strix,tmpc,egg_prefs_get(EGG_PREF_LANG));
        tmpc=0;
      }
      if (dialogue_set_text(modal,tmp,tmpc)<0) return -1;
    } else {
      if (dialogue_set_text(modal,text,textc)<0) return -1;
    }
    if (args->speaker) {
      dialogue_origin_sprite(modal,args->speaker);
    } else if (args->speakerx||args->speakery) {
      dialogue_origin_fb(modal,args->speakerx,args->speakery);
    }
    MODAL->cb=args->cb;
    MODAL->userdata=args->userdata;
  }
  
  return 0;
}

/* Dismiss.
 */
 
static void dialogue_dismiss(struct modal *modal) {
  if (MODAL->stage==STAGE_FAREWELL) return; // yeah yeah yeah, i'm on it
  if (MODAL->cb&&MODAL->cb(0,MODAL->userdata)) ; // ack'd, no sound
  else bm_sound(RID_sound_uicancel);
  MODAL->stage=STAGE_FAREWELL;
}

/* Activate.
 */
 
static void dialogue_activate(struct modal *modal) {
  if (MODAL->stage==STAGE_FAREWELL) return;
  if ((MODAL->optionp>=0)&&(MODAL->optionp<MODAL->optionc)) {
    int optionid=MODAL->optionv[MODAL->optionp].optionid;
    if (MODAL->cb&&MODAL->cb(optionid,MODAL->userdata)) ;
    else bm_sound(RID_sound_uiactivate);
    MODAL->stage=STAGE_FAREWELL;
  } else {
    dialogue_dismiss(modal);
  }
}

/* Move cursor.
 */
 
static void dialogue_move(struct modal *modal,int d) {
  if (MODAL->optionc<2) return;
  MODAL->optionp+=d;
  if (MODAL->optionp<0) MODAL->optionp=MODAL->optionc-1;
  else if (MODAL->optionp>=MODAL->optionc) MODAL->optionp=0;
  bm_sound(RID_sound_uimotion);
}

/* In background, proceed with dismissal.
 * eg a new dialogue modal was summoned just as we exit.
 */
 
static void _dialogue_updatebg(struct modal *modal,double elapsed) {
  if (MODAL->stage==STAGE_FAREWELL) {
    if ((MODAL->presence-=DISMISS_SPEED*elapsed)<=0.0) {
      MODAL->presence=0.0;
      modal->defunct=1;
    }
  }
}

/* Update.
 */
 
static void _dialogue_update(struct modal *modal,double elapsed) {

  switch (MODAL->stage) {
    case STAGE_HELLO: {
        if ((MODAL->presence+=SUMMON_SPEED*elapsed)>=1.0) {
          MODAL->presence=1.0;
          MODAL->stage=STAGE_WAIT;
        }
      } break;
    case STAGE_FAREWELL: {
        if ((MODAL->presence-=DISMISS_SPEED*elapsed)<=0.0) {
          MODAL->presence=0.0;
          modal->defunct=1;
          return;
        }
      } break;
  }
  
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) dialogue_move(modal,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) dialogue_move(modal,1);
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) dialogue_activate(modal);
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) dialogue_dismiss(modal);
}

/* Render.
 */
 
static void _dialogue_render(struct modal *modal) {
  if (MODAL->boxdirty) {
    MODAL->boxdirty=0;
    dialogue_reposition(modal);
  }
  
  int boxx,boxy,boxw,boxh;
  if (MODAL->presence<1.0) {
    double inv=1.0-MODAL->presence;
    boxx=(int)(MODAL->x0*inv+MODAL->boxx*MODAL->presence);
    boxy=(int)(MODAL->y0*inv+MODAL->boxy*MODAL->presence);
    boxw=(int)(MODAL->boxw*MODAL->presence);
    boxh=(int)(MODAL->boxh*MODAL->presence);
  } else {
    boxx=MODAL->boxx;
    boxy=MODAL->boxy;
    boxw=MODAL->boxw;
    boxh=MODAL->boxh;
  }
  graf_fill_rect(&g.graf,boxx,boxy,boxw,boxh,0x000000ff);
  
  if (MODAL->stage==STAGE_WAIT) {
    graf_set_input(&g.graf,MODAL->texid);
    graf_decal(&g.graf,MODAL->texx,MODAL->texy,0,0,MODAL->texw,MODAL->texh);
    struct option *option=MODAL->optionv;
    int i=MODAL->optionc;
    for (;i-->0;option++) {
      graf_set_input(&g.graf,option->texid);
      graf_decal(&g.graf,option->x,option->y,0,0,option->w,option->h);
    }
    if ((MODAL->optionp>=0)&&(MODAL->optionp<MODAL->optionc)) {
      option=MODAL->optionv+MODAL->optionp;
      graf_set_image(&g.graf,RID_image_pause);
      graf_tile(&g.graf,option->x-10,option->y+(option->h>>1),0x25,0);
    }
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_dialogue={
  .name="dialogue",
  .objlen=sizeof(struct modal_dialogue),
  .del=_dialogue_del,
  .init=_dialogue_init,
  .update=_dialogue_update,
  .updatebg=_dialogue_updatebg,
  .render=_dialogue_render,
  // Not doing notify, even though we definitely show text.
  // Trying to retrieve the original text would be complicated.
};

/* Unused optionid.
 */
 
static int dialogue_unused_optionid(const struct modal *modal) {
  int optionid=1;
  for (;;optionid++) { // Can't overflow, because (struct option) is larger than 1 byte.
    const struct option *option=MODAL->optionv;
    int i=MODAL->optionc,inuse=0;
    for (;i-->0;option++) {
      if (option->optionid==optionid) {
        inuse=1;
        break;
      }
    }
    if (!inuse) return optionid;
  }
}

/* Add option.
 */
 
int modal_dialogue_add_option(struct modal *modal,int optionid,const char *src,int srcc) {
  if (!modal||(modal->type!=&modal_type_dialogue)) return -1;
  if (MODAL->optionc>=MODAL->optiona) {
    int na=MODAL->optiona+8;
    if (na>INT_MAX/sizeof(struct option)) return -1;
    void *nv=realloc(MODAL->optionv,sizeof(struct option)*na);
    if (!nv) return -1;
    MODAL->optionv=nv;
    MODAL->optiona=na;
  }
  if (optionid<1) optionid=dialogue_unused_optionid(modal);
  struct option *option=MODAL->optionv+MODAL->optionc++;
  memset(option,0,sizeof(struct option));
  option->optionid=optionid;
  option->texid=font_render_to_texture(0,g.font,src,srcc,FBW,font_get_line_height(g.font),0xffffffff);
  egg_texture_get_size(&option->w,&option->h,option->texid);
  MODAL->boxdirty=1;
  return optionid;
}

int modal_dialogue_add_option_string(struct modal *modal,int rid,int strix) {
  if (strix<1) return -1;
  const char *src=0;
  int srcc=text_get_string(&src,rid,strix);
  return modal_dialogue_add_option(modal,strix,src,srcc);
}

int modal_dialogue_add_option_string_id(struct modal *modal,int rid,int strix,int optionid) {
  if (strix<1) return -1;
  const char *src=0;
  int srcc=text_get_string(&src,rid,strix);
  return modal_dialogue_add_option(modal,optionid,src,srcc);
}

/* Set default option.
 */

void modal_dialogue_set_default(struct modal *modal,int optionid) {
  if (!modal||(modal->type!=&modal_type_dialogue)) return;
  struct option *option=MODAL->optionv;
  int i=0;
  for (;i<MODAL->optionc;i++,option++) {
    if (option->optionid==optionid) {
      MODAL->optionp=i;
      return;
    }
  }
}

/* Set callback.
 */
 
void modal_dialogue_set_callback(struct modal *modal,int (*cb)(int optionid,void *userdata),void *userdata) {
  if (!modal||(modal->type!=&modal_type_dialogue)) return;
  MODAL->cb=cb;
  MODAL->userdata=userdata;
}

/* Convenience.
 */
 
struct modal *modal_dialogue_simple(int rid,int strix) {
  struct modal_args_dialogue args={
    .rid=rid,
    .strix=strix,
  };
  return modal_spawn(&modal_type_dialogue,&args,sizeof(args));
}

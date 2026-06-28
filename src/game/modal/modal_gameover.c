#include "game/bellacopia.h"

#define LABEL_LIMIT 3

struct modal_gameover {
  struct modal hdr;
  struct label {
    int texid;
    int x,y,w,h;
    int strix; // 17,18,19 = msg,continue,mainmenu
    int interactive;
  } labelv[LABEL_LIMIT];
  int labelc;
  int labelp;
  double runtime;
};

#define MODAL ((struct modal_gameover*)modal)

/* Cleanup.
 */
 
static void _gameover_del(struct modal *modal) {
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) {
    egg_texture_del(label->texid);
  }
}

/* Add one label.
 * We don't touch (x,y), they'll be zero.
 */
 
static struct label *gameover_add_label(struct modal *modal,int strix,uint32_t color) {
  if (MODAL->labelc>=LABEL_LIMIT) return 0;
  struct label *label=MODAL->labelv+MODAL->labelc++;
  memset(label,0,sizeof(struct label));
  label->strix=strix;
  const char *text;
  int textc=text_get_string(&text,1,strix);
  label->texid=font_render_to_texture(0,g.font,text,textc,FBW,FBH,color);
  egg_texture_get_size(&label->w,&label->h,label->texid);
  return label;
}

/* Discard existing labels and rebuild from scratch.
 * Init or language change.
 */
 
static int gameover_rebuild_labels(struct modal *modal) {
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
  MODAL->labelc=0;
  
  if (!(label=gameover_add_label(modal,17,0xff0000ff))) return -1; // Prompt.
  if (!(label=gameover_add_label(modal,18,0xffffffff))) return -1; // Continue.
  label->interactive=1;
  if (!(label=gameover_add_label(modal,19,0xffffffff))) return -1; // Main Menu.
  label->interactive=1;
  
  const int prompt_gap=10; // Space between prompt and choices.
  int labelsh=0;
  for (i=MODAL->labelc,label=MODAL->labelv;i-->0;label++) {
    labelsh+=label->h;
    if (i&&!label->interactive) labelsh+=prompt_gap;
  }
  int y=(FBH>>1)-(labelsh>>1);
  for (i=MODAL->labelc,label=MODAL->labelv;i-->0;label++) {
    label->x=FBW>>1;
    label->y=y;
    y+=label->h;
    if (label->interactive) label->x+=20;
    else y+=prompt_gap;
  }
  
  return 0;
}

/* Init.
 */
 
static int _gameover_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=1;
  modal->interactive=1;
  if (gameover_rebuild_labels(modal)<0) return -1;
  // Focus the first interactive option, ie "Continue":
  struct label *label=MODAL->labelv;
  int i=0;
  for (;i<MODAL->labelc;i++,label++) {
    if (label->interactive) {
      MODAL->labelp=i;
      break;
    }
  }
  return 0;
}

/* Focus.
 */
 
static void _gameover_focus(struct modal *modal,int focus) {
  if (focus) {
    bm_song_gently(0);
  }
}

/* Notify.
 */
 
static void _gameover_notify(struct modal *modal,int k,int v) {
  switch (k) {
    case EGG_PREF_LANG: gameover_rebuild_labels(modal); break;
  }
}

/* Move cursor.
 */
 
static void gameover_move(struct modal *modal,int d) {
  bm_sound(RID_sound_uimotion);
  int panic=MODAL->labelc;
  while (panic-->0) {
    MODAL->labelp+=d;
    if (MODAL->labelp<0) MODAL->labelp=MODAL->labelc-1;
    else if (MODAL->labelp>=MODAL->labelc) MODAL->labelp=0;
    if (MODAL->labelv[MODAL->labelp].interactive) break;
  }
}

/* Activate selected thing.
 */
 
static void gameover_activate(struct modal *modal) {
  modal->defunct=1;
  bm_sound(RID_sound_uiactivate);
  if ((MODAL->labelp>=0)&&(MODAL->labelp<MODAL->labelc)) {
    struct label *label=MODAL->labelv+MODAL->labelp;
    switch (label->strix) {
      case 18: { // Continue.
          struct modal_args_story args={
            .use_save=1, // A "New Game" option here is possible, but I don't think it would make sense.
          };
          struct modal *story=modal_spawn(&modal_type_story,&args,sizeof(args));
        } break;
      // 19, Main Menu, we can noop. The main menu will get spawned automatically if nothing else is presented.
    }
  }
}

/* Update.
 */
 
static void _gameover_update(struct modal *modal,double elapsed) {
  MODAL->runtime+=elapsed;
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) gameover_move(modal,-1);
  else if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) gameover_move(modal,1);
  else if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) gameover_activate(modal);
}

/* Render.
 */
 
static void _gameover_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  
  // Initially, show Dot shocked, holding her heart.
  graf_set_image(&g.graf,RID_image_gameover);
  int dotx=(FBW/3)-24;
  int doty=(FBH/2)-24;
  const double shocktime=1.000;
  const double drifttime=3.000;
  if (MODAL->runtime<shocktime) {
    graf_decal(&g.graf,dotx,doty,0,0,48,48);
    
  // After a beat, she hits the floor and her hat drifts down.
  } else {
    graf_decal(&g.graf,dotx,doty,48,0,48,48);
    // Hat starts exactly where Dot was, and descends about 14 pixels.
    const int driftdistance=14;
    double t=(MODAL->runtime-shocktime)/drifttime;
    if (t<1.0) {
      int hatx=dotx;
      hatx+=lround(sin(t*M_PI*2.0)*4.0);
      int haty=doty+(int)(driftdistance*t);
      graf_decal(&g.graf,hatx,haty,96,0,48,48);
    } else {
      graf_decal(&g.graf,dotx,doty+driftdistance,144,0,48,48);
    }
  }
  
  // Labels, both interactive and non-interactive.
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) {
    graf_set_input(&g.graf,label->texid);
    graf_decal(&g.graf,label->x,label->y,0,0,label->w,label->h);
  }
  
  // Cursor.
  if ((MODAL->labelp>=0)&&(MODAL->labelp<MODAL->labelc)) {
    label=MODAL->labelv+MODAL->labelp;
    if (label->interactive) {
      graf_set_image(&g.graf,RID_image_pause);
      graf_tile(&g.graf,label->x-12,label->y+(label->h>>1),0x25,0);
    }
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_gameover={
  .name="gameover",
  .objlen=sizeof(struct modal_gameover),
  .del=_gameover_del,
  .init=_gameover_init,
  .update=_gameover_update,
  .render=_gameover_render,
  .focus=_gameover_focus,
  .notify=_gameover_notify,
};

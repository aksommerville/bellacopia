#include "game/bellacopia.h"

// (optionid) is an index in strings:1
#define OPTIONID_CONTINUE 10
#define OPTIONID_NEWGAME  11
#define OPTIONID_ARCADE   12
#define OPTION_LIMIT 3

struct modal_hello {
  struct modal hdr;
  int titlew,titleh;
  struct option {
    int optionid;
    int enable;
    int x,y,w,h; // (y) is relative; we apply scrolling separately.
    int texid;
  } optionv[OPTION_LIMIT];
  int optionc;
  int optionp;
};

#define MODAL ((struct modal_hello*)modal)

/* Cleanup.
 */
 
static void _hello_del(struct modal *modal) {
  struct option *option=MODAL->optionv;
  int i=MODAL->optionc;
  for (;i-->0;option++) {
    egg_texture_del(option->texid);
  }
}

/* Add option.
 */
 
struct option *hello_add_option(struct modal *modal,int optionid) {
  if (MODAL->optionc>=OPTION_LIMIT) return 0;
  int y=150;
  if (MODAL->optionc>0) {
    struct option *prev=MODAL->optionv+MODAL->optionc-1;
    y=prev->y+prev->h+1;
  }
  struct option *option=MODAL->optionv+MODAL->optionc++;
  option->optionid=optionid;
  const char *text=0;
  int textc=text_get_string(&text,1,optionid);
  option->texid=font_render_to_texture(0,g.font,text,textc,FBW,font_get_line_height(g.font),0xffffffff);
  egg_texture_get_size(&option->w,&option->h,option->texid);
  option->x=(FBW>>1)-(option->w>>1);
  option->y=y;
  option->enable=1;
  return option;
}

/* Init.
 */
 
static int _hello_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=1;
  modal->interactive=1;
  
  int texid=graf_tex(&g.graf,RID_image_title);
  egg_texture_get_size(&MODAL->titlew,&MODAL->titleh,texid);
  
  struct option *option;
  if (option=hello_add_option(modal,OPTIONID_CONTINUE)) {
    char tmp[10];
    if (egg_store_get(tmp,sizeof(tmp),"save",4)>0) {
      option->enable=1;
    } else {
      option->enable=0;
    }
  }
  hello_add_option(modal,OPTIONID_NEWGAME);
  hello_add_option(modal,OPTIONID_ARCADE);
  
  while ((MODAL->optionp<MODAL->optionc)&&!MODAL->optionv[MODAL->optionp].enable) MODAL->optionp++;
  
  return 0;
}

/* Focus.
 */
 
static void _hello_focus(struct modal *modal,int focus) {
  //TODO bm_song_force(RID_song_hello);
}

/* Notify.
 */
 
static void _hello_notify(struct modal *modal,int k,int v) {
  if (k==EGG_PREF_LANG) {
    struct option *option=MODAL->optionv;
    int i=MODAL->optionc;
    for (;i-->0;option++) {
      const char *text=0;
      int textc=text_get_string(&text,1,option->optionid);
      int ntexid=font_render_to_texture(option->texid,g.font,text,textc,FBW,font_get_line_height(g.font),0xffffffff);
      if (ntexid>0) {
        egg_texture_get_size(&option->w,&option->h,option->texid);
        option->x=(FBW>>1)-(option->w>>1);
        // Height can't change; our fonts have strictly the same height always.
      }
    }
  }
}

/* Begin Story Mode.
 */
 
static void hello_begin_story(struct modal *modal,int from_save) {
  
  struct modal_args_story args={
    .use_save=from_save,
  };
  struct modal *story=modal_spawn(&modal_type_story,&args,sizeof(args));
  if (!story) {
    bm_sound(RID_sound_reject);
    return;
  }
  
  bm_sound(RID_sound_uiactivate);
  modal->defunct=1;
}

/* Begin Arcade Mode.
 */
 
static void hello_begin_arcade(struct modal *modal) {
  
  struct modal *arcade=modal_spawn(&modal_type_arcade,0,0);
  if (!arcade) {
    bm_sound(RID_sound_reject);
    return;
  }
  
  bm_sound(RID_sound_uiactivate);
  modal->defunct=1;
}

/* Activate selected option.
 */
 
static void hello_activate(struct modal *modal) {
  if ((MODAL->optionp<0)||(MODAL->optionp>=MODAL->optionc)) return;
  struct option *option=MODAL->optionv+MODAL->optionp;
  if (!option->enable) return;
  switch (option->optionid) {
    case OPTIONID_CONTINUE: hello_begin_story(modal,1); break;
    case OPTIONID_NEWGAME: hello_begin_story(modal,0); break; // TODO "sure you want to erase?" if there's a save.
    case OPTIONID_ARCADE: hello_begin_arcade(modal); break;
  }
}

/* Move cursor.
 */
 
static void hello_move(struct modal *modal,int d) {
  if (MODAL->optionc<1) return;
  int panic=MODAL->optionc;
  while (panic-->0) {
    MODAL->optionp+=d;
    if (MODAL->optionp<0) MODAL->optionp=MODAL->optionc-1;
    else if (MODAL->optionp>=MODAL->optionc) MODAL->optionp=0;
    if (MODAL->optionv[MODAL->optionp].enable) break;
  }
  bm_sound(RID_sound_uimotion);
}

/* Update.
 */
 
static void _hello_update(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) hello_move(modal,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) hello_move(modal,1);
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) hello_activate(modal);
  else if ((g.input[0]&EGG_BTN_AUX1)&&!(g.pvinput[0]&EGG_BTN_AUX1)) hello_activate(modal);
}

/* Render.
 */
 
static void _hello_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x2a1755ff);
  graf_set_image(&g.graf,RID_image_title);
  graf_decal(&g.graf,(FBW>>1)-(MODAL->titlew>>1),0,0,0,MODAL->titlew,MODAL->titleh);
  if ((MODAL->optionp>=0)&&(MODAL->optionp<MODAL->optionc)) {
    struct option *option=MODAL->optionv+MODAL->optionp;
    if (option->enable) {
      graf_fill_rect(&g.graf,option->x-2,option->y-1,option->w+4,option->h+1,0x204060ff);
    }
  }
  struct option *option=MODAL->optionv;
  int i=MODAL->optionc;
  for (;i-->0;option++) {
    graf_set_input(&g.graf,option->texid);
    if (!option->enable) graf_set_alpha(&g.graf,0x80);
    graf_decal(&g.graf,option->x,option->y,0,0,option->w,option->h);
    graf_set_alpha(&g.graf,0xff);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_hello={
  .name="hello",
  .objlen=sizeof(struct modal_hello),
  .del=_hello_del,
  .init=_hello_init,
  .focus=_hello_focus,
  .notify=_hello_notify,
  .update=_hello_update,
  .render=_hello_render,
};

#include "game/bellacopia.h"

// (optionid) is an index in strings:1
#define OPTIONID_CONTINUE 10
#define OPTIONID_NEWGAME  11
#define OPTIONID_ARCADE   12
#define OPTIONID_DEBUG    44
#define OPTIONID_BROOM    45
#define OPTIONID_SETTINGS 46
#define OPTIONID_CREDITS  47
#define OPTIONID_QUIT     48

#define OPTION_COLC 3
#define OPTION_ROWC 3

struct modal_hello {
  struct modal hdr;
  int titlew,titleh;
  
  /* Options are laid out LRTB.
   * (optionp) is linear, mostly for historical reasons.
   */
  struct option {
    int optionid;
    int enable;
    int x,y,w,h; // (y) is relative; we apply scrolling separately.
    int texid;
  } optionv[OPTION_COLC*OPTION_ROWC];
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
  if (MODAL->optionc>=OPTION_COLC*OPTION_ROWC) return 0;
  /*XXX
  int y=150;
  if (MODAL->optionc>0) {
    struct option *prev=MODAL->optionv+MODAL->optionc-1;
    y=prev->y+prev->h+1;
  }
  /**/
  struct option *option=MODAL->optionv+MODAL->optionc++;
  option->optionid=optionid;
  const char *text=0;
  int textc=text_get_string(&text,1,optionid);
  option->texid=font_render_to_texture(0,g.font,text,textc,FBW,font_get_line_height(g.font),0xffffffff);
  egg_texture_get_size(&option->w,&option->h,option->texid);
  /*XXX layout must come after they're all initialized
  option->x=(FBW>>1)-(option->w>>1);
  option->y=y;
  /**/
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
  
  /* Options lay out LRTB.
   * Though the more logical order is columnwise. So this list will read a little weird.
   *   ARCADE    CONTINUE      SETTINGS
   *   BROOM     NEW           CREDITS
   *   DEBUG     ---           QUIT
   */
  struct option *option;
  if (!(option=hello_add_option(modal,OPTIONID_ARCADE))) return -1;
  if (!(option=hello_add_option(modal,OPTIONID_CONTINUE))) return -1;
  char tmp[10];
  if (egg_store_get(tmp,sizeof(tmp),"save",4)>0) {
    option->enable=1;
  } else {
    option->enable=0;
  }
  if (!(option=hello_add_option(modal,OPTIONID_SETTINGS))) return -1;
  if (!(option=hello_add_option(modal,OPTIONID_BROOM))) return -1;
  if (!(option=hello_add_option(modal,OPTIONID_NEWGAME))) return -1;
  if (!(option=hello_add_option(modal,OPTIONID_CREDITS))) return -1;
  if (!(option=hello_add_option(modal,OPTIONID_DEBUG))) return -1;
  if (!(option=hello_add_option(modal,0))) return -1;
  option->enable=0;
  if (!(option=hello_add_option(modal,OPTIONID_QUIT))) return -1;
  if (0) option->enable=0; // TODO Option to disable Quit for kiosks and such?
  
  /* Choose geometry for the options grid and apply it.
   */
  int rowh=MODAL->optionv[0].h; // They'll all be the same height (per font)
  int midxv[OPTION_COLC];
  int i=OPTION_COLC;
  while (i-->0) midxv[i]=(FBW*(i+1))/(OPTION_COLC+1);
  for (i=0,option=MODAL->optionv;i<MODAL->optionc;i++,option++) {
    option->x=midxv[i%OPTION_COLC]-(option->w>>1);
    option->y=150+rowh*(i/OPTION_COLC);
  }
  
  /* Default to CONTINUE if enabled, otherwise NEWGAME.
   */
  if (MODAL->optionv[1].enable) MODAL->optionp=1;
  else MODAL->optionp=4;
  
  return 0;
}

/* Focus.
 */
 
static void _hello_focus(struct modal *modal,int focus) {
  if (focus) {
    bm_song_gently(RID_song_break_soil);
  }
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
        int midx=option->x+(option->w>>1);
        egg_texture_get_size(&option->w,&option->h,option->texid);
        option->x=midx-(option->w>>1);
        // Height can't change; our fonts have strictly the same height always.
      }
    }
  }
}

/* Start the game.
 */
 
static void hello_spawn_story_modal(struct modal *modal,int from_save) {
  struct modal_args_story args={
    .use_save=from_save,
  };
  struct modal *story=modal_spawn(&modal_type_story,&args,sizeof(args));
  if (!story) {
    bm_sound(RID_sound_reject);
    return;
  }
}

static void hello_cb_intro(void *userdata) {
  hello_spawn_story_modal(userdata,0);
}

static void hello_spawn_intro_cutscene(struct modal *modal) {
  
  /* modal_story is going to reset all the globals, including store.
   * But we need the store cleared before that, so the intro cutscene sees a clean state.
   * (story_flowers consults NS_fld_root_all to know whether "and they lived happily ever after" or "ok get to it!").
   */
  store_clear();
  
  struct modal_args_cutscene args={
    .strix_title=13,
    .context=CUTSCENE_CONTEXT_EXPECTEDISH,
    .cb=hello_cb_intro,
    .userdata=modal,
  };
  struct modal *cutscene=modal_spawn(&modal_type_cutscene,&args,sizeof(args));
};

/* Begin Story Mode.
 * We defunct in this case. No sense staying open during the whole campaign.
 * (and also, if they return here, we should default to Continue even if it was New the first time).
 * All other modes leave this Hello open underneath.
 */
 
static void hello_begin_story(struct modal *modal,int from_save) {
  fprintf(stderr,"%d %s %s\n",(int)egg_time_real(),__func__,from_save?"CONTINUE":"NEW");
  if (from_save) {
    hello_spawn_story_modal(modal,from_save);
  } else {
    hello_spawn_intro_cutscene(modal);
  }
  bm_sound(RID_sound_uiactivate);
  modal->defunct=1;
}

/* Begin Arcade Mode.
 * We're modal_type_pvp, not modal_type_arcade. Poor naming of things on my part, early on.
 */
 
static void hello_begin_arcade(struct modal *modal) {
  fprintf(stderr,"%d %s\n",(int)egg_time_real(),__func__);
  struct modal *arcade=modal_spawn(&modal_type_pvp,0,0);
  if (!arcade) {
    bm_sound(RID_sound_reject);
    return;
  }
  bm_sound(RID_sound_uiactivate);
}

/* Begin Debug Mode.
 * This is modal_type_arcade (which is not the billed "Arcade Mode").
 */
 
static void hello_begin_debug(struct modal *modal) {
  fprintf(stderr,"%d %s\n",(int)egg_time_real(),__func__);
  struct modal *arcade=modal_spawn(&modal_type_arcade,0,0);
  if (!arcade) {
    bm_sound(RID_sound_reject);
    return;
  }
  bm_sound(RID_sound_uiactivate);
}

/* Begin Broom Race.
 */
 
static void hello_begin_broom(struct modal *modal) {
  fprintf(stderr,"%d %s\n",(int)egg_time_real(),__func__);
  struct modal *race=modal_spawn(&modal_type_raceconfig,0,0);
  if (!race) {
    bm_sound(RID_sound_reject);
    return;
  }
  bm_sound(RID_sound_uiactivate);
}

/* Launch Settings modal. This does not dismiss us.
 */
 
static void hello_begin_settings(struct modal *modal) {
  fprintf(stderr,"%d %s\n",(int)egg_time_real(),__func__);
  //TODO Settings modal
}

/* Launch Credits modal. This does not dismiss us.
 */
 
static void hello_begin_credits(struct modal *modal) {
  fprintf(stderr,"%d %s\n",(int)egg_time_real(),__func__);
  //TODO Credits modal
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
    case OPTIONID_DEBUG: hello_begin_debug(modal); break;
    case OPTIONID_BROOM: hello_begin_broom(modal); break;
    case OPTIONID_SETTINGS: hello_begin_settings(modal); break;
    case OPTIONID_CREDITS: hello_begin_credits(modal); break;
    case OPTIONID_QUIT: egg_terminate(0); break;
  }
}

/* Move cursor.
 */
 
static void hello_move(struct modal *modal,int dx,int dy) {
  if (MODAL->optionc<1) return;
  int x=MODAL->optionp%OPTION_COLC;
  int y=MODAL->optionp/OPTION_COLC;
  int panic=MODAL->optionc;
  while (panic-->0) {
    x+=dx; if (x<0) x=OPTION_COLC-1; else if (x>=OPTION_COLC) x=0;
    y+=dy; if (y<0) y=OPTION_ROWC-1; else if (y>=OPTION_ROWC) y=0;
    MODAL->optionp=y*OPTION_COLC+x;
    if (MODAL->optionv[MODAL->optionp].enable) break;
  }
  bm_sound(RID_sound_uimotion);
}

/* Update.
 */
 
static void _hello_update(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) hello_move(modal,-1,0);
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) hello_move(modal,1,0);
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) hello_move(modal,0,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) hello_move(modal,0,1);
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

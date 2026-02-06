/* modal_battle.c
 * Generic entry point for all minigames.
 */

#include "game/bellacopia.h"
#include "egg/egg_language_codes.h"

#define CONSEQUENCE_LIMIT 4

#define STAGE_PROMPT 1
#define STAGE_PLAY 2
#define STAGE_REPORT 3

struct modal_battle {
  struct modal hdr;
  int battle; // NS_battle_*
  int players; // NS_players_*
  uint8_t handicap; // 0..128..255 = easy..balanced..hard ("easy" for the left player)
  void (*cb)(struct modal *modal,int outcome,void *userdata);
  void *userdata;
  int left_name;
  int right_name;
  const struct battle_type *type;
  void *ctx;
  int outcome; // Zero until STAGE_REPORT, then (-1,0,1).
  int stage;
  int no_store;
  
  int prompt_texid,prompt_w,prompt_h;
  int report_texid,report_w,report_h;
  
  double timeout;
  double duration;
  
  struct consequence {
    int itemid,d;
  } consequencev[CONSEQUENCE_LIMIT];
  int consequencec;
};

#define MODAL ((struct modal_battle*)modal)

/* Cleanup.
 */
 
static void _battle_del(struct modal *modal) {
  egg_texture_del(MODAL->prompt_texid);
  egg_texture_del(MODAL->report_texid);
  if (MODAL->ctx&&MODAL->type&&MODAL->type->del) MODAL->type->del(MODAL->ctx);
  MODAL->ctx=0;
}

/* Get article for the game's title, in the current language.
 * I have no doubt that when we add a second language, this framework will prove to have been inadequate.
 * Enjoy!
 */
 
static int battle_get_article(void *dstpp,const char *title,int titlec,const struct battle_type *type) {
  if (titlec<1) return 0;
  char firstlower=title[0];
  if ((firstlower>='A')&&(firstlower<='Z')) firstlower+=0x20;
  switch (egg_prefs_get(EGG_PREF_LANG)) {
    case EGG_LANG_en: switch (firstlower) {
        case 'a': case 'e': case 'i': case 'o': case 'u': *(const void**)dstpp="an"; return 2;
        default: *(const void**)dstpp="a"; return 1;
      }
  }
  return 0;
}

/* "Monster challenges you to a such-and-such contest!"
 * Generate the texture (prompt_texid,w,h).
 */
 
static int battle_generate_prompt(struct modal *modal) {
  char msg[1024];
  int msgc=0;
  
  /* Get the title and participant names.
   */
  const char *title=0;
  int titlec=text_get_string(&title,RID_strings_battle,MODAL->type->strix_name);
  const char *lname=0;
  int lnamec=text_get_string(&lname,RID_strings_battle,MODAL->left_name);
  const char *rname=0;
  int rnamec=text_get_string(&rname,RID_strings_battle,MODAL->right_name);
  
  /* Choose the right template.
   */
  int templateid;
  if (MODAL->players==NS_players_man_cpu) { // The usual refrain, possibly omitting "contest".
    if (MODAL->type->no_contest) templateid=2; // "%0 challenges you to %1!"
    else templateid=1; // "%0 challenges you to %1 contest!"
  } else {
    templateid=3; // The more generic one: "%0: %1 vs %2"
  }
  
  /* Now the rough bit: Prepend an article to the title if needed.
   * This is language-specific.
   */
  char title_storage[64];
  if ((MODAL->players==NS_players_man_cpu)&&!MODAL->type->no_article) {
    const char *article=0;
    int articlec=battle_get_article(&article,title,titlec,MODAL->type);
    if (articlec>0) {
      if (articlec+1+titlec>sizeof(title_storage)) return -1;
      memcpy(title_storage,article,articlec);
      title_storage[articlec]=' ';
      memcpy(title_storage+articlec+1,title,titlec);
      title=title_storage;
      titlec=articlec+1+titlec;
    }
  }
  
  /* Get the template, populate insertions, and generate the final text.
   */
  const char *template=0;
  int templatec=text_get_string(&template,RID_strings_battle,templateid);
  struct text_insertion insv[3];
  #define SETINS(ix,tag) { insv[ix].mode='s'; insv[ix].s.v=tag; insv[ix].s.c=tag##c; }
  int insc=0;
  if ((templateid==1)||(templateid==2)) {
    SETINS(0,rname)
    SETINS(1,title)
    insc=2;
  } else {
    SETINS(0,title)
    SETINS(1,lname)
    SETINS(2,rname)
    insc=3;
  }
  #undef SETINS
  msgc=text_format(msg,sizeof(msg),template,templatec,insv,insc);
  if ((msgc<0)||(msgc>sizeof(msg))) msgc=0;

  egg_texture_del(MODAL->prompt_texid);
  MODAL->prompt_texid=font_render_to_texture(0,g.font,msg,msgc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&MODAL->prompt_w,&MODAL->prompt_h,MODAL->prompt_texid);
  return 0;
}

/* Generate final report text.
 * LF between lines.
 */
 
static int battle_generate_report_text(char *dst,int dsta,struct modal *modal) {
  int dstc=0;
  
  // If both names were provided start with the one-line summary, then a blank line.
  if (MODAL->left_name&&MODAL->right_name) {
    struct text_insertion ins={.mode='r',.r={.rid=RID_strings_battle,.strix=(MODAL->outcome<0)?MODAL->right_name:MODAL->left_name}};
    dstc+=text_format_res(dst+dstc,dsta-dstc,RID_strings_battle,MODAL->outcome?7:8,&ins,1);
    if (dstc<dsta) dst[dstc++]=0x0a;
    if (dstc<dsta) dst[dstc++]=0x0a;
  }
  
  // One line per consequence.
  const struct consequence *consequence=MODAL->consequencev;
  int i=MODAL->consequencec;
  for (;i-->0;consequence++) {
    const struct item_detail *detail=item_detail_for_itemid(consequence->itemid);
    if (!detail) continue;
    struct text_insertion insv[2];
    int insc=0,strix_template;
    if (consequence->itemid==NS_itemid_text) {
      dstc+=text_format_res(dst+dstc,dsta-dstc,RID_strings_battle,consequence->d,0,0);
      if (dstc<dsta) dst[dstc++]=0x0a;
      continue;
    } else if (consequence->d<0) {
      strix_template=46;
      insv[insc++]=(struct text_insertion){.mode='i',.i=-consequence->d};
      insv[insc++]=(struct text_insertion){.mode='r',.r={.rid=RID_strings_item,.strix=detail->strix_name}};
    } else if (consequence->d>0) {
      strix_template=45;
      insv[insc++]=(struct text_insertion){.mode='i',.i=consequence->d};
      insv[insc++]=(struct text_insertion){.mode='r',.r={.rid=RID_strings_item,.strix=detail->strix_name}};
    } else {
      strix_template=44;
      insv[insc++]=(struct text_insertion){.mode='r',.r={.rid=RID_strings_item,.strix=detail->strix_name}};
    }
    dstc+=text_format_res(dst+dstc,dsta-dstc,RID_strings_item,strix_template,insv,insc);
    if (dstc<dsta) dst[dstc++]=0x0a;
  }
    
  
  return dstc;
}

/* Generate texture for the final report.
 */
 
static int battle_generate_report(struct modal *modal) {

  // Get the full text, with LF separating lines.
  char msg[1024];
  int msgc=battle_generate_report_text(msg,sizeof(msg),modal);
  if ((msgc<0)||(msgc>sizeof(msg))) msgc=0;
  
  /* Split the text into lines.
   * Do include blanks, but trim whitespace fore and aft.
   * Take the width of each line.
   */
  #define LINE_LIMIT 8
  struct line {
    const char *v;
    int c;
    int w;
  } linev[LINE_LIMIT];
  int linec=0;
  int msgp=0,imgw=0;
  while (msgp<msgc) {
    const char *src=msg+msgp;
    int srcc=0;
    while ((msgp<msgc)&&(msg[msgp++]!=0x0a)) srcc++;
    while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
    while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
    int w=font_measure_string(g.font,src,srcc);
    if (w>imgw) imgw=w;
    struct line *line=linev+linec++;
    line->v=src;
    line->c=srcc;
    line->w=w;
    if (linec>=LINE_LIMIT) break;
  }
  #undef LINE_LIMIT
  
  // No text might be possible. Clear the texture then.
  if ((imgw<1)||(linec<1)) {
    egg_texture_del(MODAL->report_texid);
    MODAL->report_texid=0;
    MODAL->report_w=0;
    MODAL->report_h=0;
    return 0;
  }
  
  /* Allocate a temporary RGBA buffer and render each line into it.
   */
  int lineh=font_get_line_height(g.font);
  int imgh=lineh*linec;
  int imgstride=imgw*4;
  int imglen=imgstride*imgh;
  uint32_t *rgba=calloc(imgstride,imgh);
  if (!rgba) return -1;
  int y=0,linep=0;
  for (;linep<linec;linep++,y+=lineh) {
    int x=(imgw>>1)-(linev[linep].w>>1);
    font_render(rgba+y*imgw+x,imgw-x,imgh-y,imgstride,g.font,linev[linep].v,linev[linep].c,0xffffffff);
  }
  
  // Upload to texture.
  if (!MODAL->report_texid) MODAL->report_texid=egg_texture_new();
  egg_texture_load_raw(MODAL->report_texid,imgw,imgh,imgstride,rgba,imglen);
  free(rgba);
  MODAL->report_w=imgw;
  MODAL->report_h=imgh;
  return 0;
}

/* Begin STAGE_PLAY.
 */
 
static void battle_begin_play(struct modal *modal) {
  MODAL->stage=STAGE_PLAY;
  MODAL->timeout=BATTLE_UNIVERSAL_TIMEOUT;
}

/* End play.
 */
 
static const char *repr_players(int players) { // For private logging only.
  switch (players) {
    case NS_players_cpu_cpu: return "Princess";
    case NS_players_cpu_man: return "cpu-vs-man";
    case NS_players_man_cpu: return "Dot";
    case NS_players_man_man: return "2-player";
  }
  return "?";
}
 
static void battle_cb_end(int outcome,void *userdata) {
  struct modal *modal=userdata;
  if (MODAL->stage!=STAGE_PLAY) {
    fprintf(stderr,"%s:ERROR: Battle '%s' called cb_end more than once.\n",__func__,MODAL->type->name);
    return;
  }
  fprintf(stderr,
    "%s: battle=%d(%s) players=%s handicap=0x%02x outcome=%d duration=%.03fs\n",__func__,
    MODAL->battle,MODAL->type->name,repr_players(MODAL->players),MODAL->handicap,outcome,MODAL->duration
  );
  MODAL->outcome=(outcome<0)?-1:(outcome>0)?1:0;
  MODAL->stage=STAGE_REPORT;
  MODAL->timeout=0.500;
  // Important: Callback to owner, then generate report. We expect to be told about the consequences during this callback.
  if (MODAL->cb) {
    MODAL->cb(modal,MODAL->outcome,MODAL->userdata);
    MODAL->cb=0;
  }
  battle_generate_report(modal);
  // If the report is empty, don't bother showing it, just terminate.
  if ((MODAL->report_w<1)||(MODAL->report_h<1)) {
    modal->defunct=1;
  }
}

/* Init.
 */
 
static int _battle_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=1;
  modal->interactive=1;
  
  if (!arg||(argc!=sizeof(struct modal_args_battle))) return -1;
  const struct modal_args_battle *args=arg;
  MODAL->battle=args->battle;
  MODAL->players=args->players;
  MODAL->handicap=args->handicap;
  MODAL->cb=args->cb;
  MODAL->userdata=args->userdata;
  MODAL->left_name=args->left_name;
  MODAL->right_name=args->right_name;
  MODAL->no_store=args->no_store;
  if (!(MODAL->type=battle_type_by_id(MODAL->battle))) {
    fprintf(stderr,"%s: battle %d not found\n",__func__,MODAL->battle);
    return -1;
  }
  if (!MODAL->type->init||!(MODAL->ctx=MODAL->type->init(MODAL->handicap,MODAL->players,battle_cb_end,modal))) {
    fprintf(stderr,"%s: Failed to initialize battle '%s'\n",__func__,MODAL->type->name);
    return -1;
  }
  
  if (args->skip_prompt) {
    battle_begin_play(modal);
  } else {
    MODAL->stage=STAGE_PROMPT;
    battle_generate_prompt(modal);
  }
  
  return 0;
}

/* Focus.
 */
 
static void _battle_focus(struct modal *modal,int focus) {
}

/* Notify.
 */
 
static void _battle_notify(struct modal *modal,int k,int v) {
  if (k==EGG_PREF_LANG) {
    if (MODAL->stage==STAGE_PROMPT) {
      battle_generate_prompt(modal);
    } else if (MODAL->stage==STAGE_REPORT) {
      battle_generate_report(modal);
    }
  }
}

/* Update, prompt.
 */
 
static void battle_update_prompt(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    battle_begin_play(modal);
  }
}

/* Update, play.
 */
 
static void battle_update_play(struct modal *modal,double elapsed) {
  if (!MODAL->type||!MODAL->type->update) {
    fprintf(stderr,"%s:%d:ERROR: Aborting battle due to no update hook.\n",__FILE__,__LINE__);
    modal->defunct=1;
    return;
  }
  if (!MODAL->type->no_timeout) {
    if ((MODAL->timeout-=elapsed)<=0.0) {
      fprintf(stderr,"%s: Aborting battle '%s' due to %.0f-second timeout.\n",__func__,MODAL->type->name,BATTLE_UNIVERSAL_TIMEOUT);
      battle_cb_end(0,modal);
      return;
    }
  }
  MODAL->duration+=elapsed;
  MODAL->type->update(MODAL->ctx,elapsed);
}

/* Update, report.
 */
 
static void battle_update_report(struct modal *modal,double elapsed) {
  if (MODAL->timeout>0.0) {
    MODAL->timeout-=elapsed;
  } else if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    modal->defunct=1;
    bm_sound(RID_sound_uiactivate);
  }
}

/* Update.
 */
 
static void _battle_update(struct modal *modal,double elapsed) {

  // Tick (battletime).
  if (!MODAL->no_store) {
    double *battletime=store_require_clock(NS_clock_battletime);
    if (battletime) (*battletime)+=elapsed;
  }

  switch (MODAL->stage) {
    case STAGE_PROMPT: battle_update_prompt(modal,elapsed); break;
    case STAGE_PLAY: battle_update_play(modal,elapsed); break;
    case STAGE_REPORT: battle_update_report(modal,elapsed); break;
  }
}

/* Render prompt.
 */
 
static void battle_render_prompt(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  int dstx=(FBW>>1)-(MODAL->prompt_w>>1);
  int dsty=(FBH>>1)-(MODAL->prompt_h>>1);
  graf_set_input(&g.graf,MODAL->prompt_texid);
  graf_decal(&g.graf,dstx,dsty,0,0,MODAL->prompt_w,MODAL->prompt_h);
  //TODO Generic input description. Maybe a picture of a gamepad with buttons blinking?
}

/* Render play.
 */
 
static void battle_render_play(struct modal *modal) {
  if (!MODAL->type||!MODAL->type->render) {
    modal->defunct=1;
    return;
  }
  MODAL->type->render(MODAL->ctx);
}

/* Render report.
 */
 
static void battle_render_report(struct modal *modal) {
  if (!MODAL->type||!MODAL->type->render) {
    modal->defunct=1;
    return;
  }
  MODAL->type->render(MODAL->ctx);
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000c0);
  int dstx=(FBW>>1)-(MODAL->report_w>>1);
  int dsty=(FBH>>1)-(MODAL->report_h>>1);
  graf_set_input(&g.graf,MODAL->report_texid);
  graf_decal(&g.graf,dstx,dsty,0,0,MODAL->report_w,MODAL->report_h);
}

/* Render.
 */
 
static void _battle_render(struct modal *modal) {
  switch (MODAL->stage) {
    case STAGE_PROMPT: battle_render_prompt(modal); break;
    case STAGE_PLAY: battle_render_play(modal); break;
    case STAGE_REPORT: battle_render_report(modal); break;
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_battle={
  .name="battle",
  .objlen=sizeof(struct modal_battle),
  .del=_battle_del,
  .init=_battle_init,
  .focus=_battle_focus,
  .notify=_battle_notify,
  .update=_battle_update,
  .render=_battle_render,
};

/* Add consequence, public.
 */
 
void modal_battle_add_consequence(struct modal *modal,int itemid,int d) {
  if (!modal||(modal->type!=&modal_type_battle)) return;
  if (MODAL->consequencec>=CONSEQUENCE_LIMIT) return;
  struct consequence *consequence=MODAL->consequencev+MODAL->consequencec++;
  consequence->itemid=itemid;
  consequence->d=d;
}

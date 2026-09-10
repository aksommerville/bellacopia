/* modal_credits.c
 * Noninteractive credits roll.
 * Accessible at main menu, and runs automatically when you finish story mode.
 */
 
#include "game/bellacopia.h"

#define STRIX_FIRST 50
#define STRIX_LAST 55
#define STRING_LIMIT 64 /* The input strings can each produce multiple display strings. */

struct modal_credits {
  struct modal hdr;
  int restore_song;
  void (*cb)(void *userdata);
  void *userdata;
  
  /* What's our strategy for displaying the text?
   *  1. Single giant texture, render once with font.
   *  2. Multiple font textures.
   *  3. Record strings and generate fonttile vertices at render.
   *  4. List of fonttile vertices.
   * (3) is least involved, so let's start with that.
   */
   
  struct string {
    const char *src;
    int srcc;
    int y; // Constant vertical position. Does not include transient scroll.
  } stringv[STRING_LIMIT];
  int stringc;
  
  double scroll; // Positive vertical pixels, starts at zero.
  double scroll_end;
};

#define MODAL ((struct modal_credits*)modal)

/* Cleanup.
 */
 
static void _credits_del(struct modal *modal) {
  if (MODAL->restore_song!=RID_song_bloomful_rejoicement) bm_song_force(MODAL->restore_song);
}

/* Rebuild string list.
 */
 
static void credits_rebuild_strings(struct modal *modal) {
  MODAL->stringc=0;
  int rowh=9;
  int space_between=20;
  int y=0;
  int strix=STRIX_FIRST;
  for (;strix<=STRIX_LAST;strix++) {
    const char *src;
    int srcc=text_get_string(&src,1,strix);
    int srcp=0;
    while (srcp<srcc) {
      if ((unsigned char)src[srcp]<=0x20) {
        srcp++;
        continue;
      }
      const char *sub=src+srcp;
      int subc=0;
      while ((srcp<srcc)&&(src[srcp++]!=0x0a)) subc++;
      while (subc&&((unsigned char)sub[subc-1]<=0x20)) subc--;
      if (!subc) continue;
      if (MODAL->stringc>=STRING_LIMIT) {
        fprintf(stderr,"%s:%d: Too many credits strings\n",__FILE__,__LINE__);
        goto _done_collecting_;
      }
      struct string *string=MODAL->stringv+MODAL->stringc++;
      string->src=sub;
      string->srcc=subc;
      string->y=y;
      y+=rowh;
    }
    y+=space_between;
  }
 _done_collecting_:;
  MODAL->scroll_end=y+FBH+10.0;
}

/* Init.
 */
 
static int _credits_init(struct modal *modal,const void *args,int argslen) {
  modal->opaque=1;
  modal->interactive=1;
  MODAL->restore_song=g.song_playing;
  if (args&&(argslen==sizeof(struct modal_args_credits))) {
    const struct modal_args_credits *ARGS=args;
    MODAL->cb=ARGS->cb;
    MODAL->userdata=ARGS->userdata;
  }
  credits_rebuild_strings(modal);
  return 0;
}

/* Focus.
 */
 
static void _credits_focus(struct modal *modal,int focus) {
  if (focus) {
    bm_song_force(RID_song_bloomful_rejoicement);
  } else {
    if (MODAL->restore_song!=RID_song_bloomful_rejoicement) bm_song_force(MODAL->restore_song);
  }
}

/* Notify.
 */
 
static void _credits_notify(struct modal *modal,int k,int v) {
  switch (k) {
    case EGG_PREF_LANG: credits_rebuild_strings(modal); break;
  }
}

/* Cancel.
 */
 
static void credits_cancel(struct modal *modal) {
  modal->defunct=1;
  if (MODAL->cb) {
    MODAL->cb(MODAL->userdata);
    MODAL->cb=0;
  }
}

/* Update.
 */
 
static void _credits_update(struct modal *modal,double elapsed) {
  MODAL->scroll+=12.0*elapsed;
  if (MODAL->scroll>=MODAL->scroll_end) credits_cancel(modal);
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) credits_cancel(modal);
}

/* Render.
 */
 
static void _credits_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  graf_set_image(&g.graf,RID_image_fonttiles);
  int y0=FBH+10-(int)MODAL->scroll;
  const struct string *string=MODAL->stringv;
  int i=MODAL->stringc;
  for (;i-->0;string++) {
    int y=y0+string->y;
    if ((y<-8)||(y>FBH+8)) continue;
    int x=(FBW>>1)-(string->srcc*4);
    const char *src=string->src;
    int srci=string->srcc;
    for (;srci-->0;src++,x+=8) {
      if ((unsigned char)(*src)<=0x20) continue;
      graf_tile(&g.graf,x,y,*src,0);
    }
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_credits={
  .name="credits",
  .objlen=sizeof(struct modal_credits),
  .del=_credits_del,
  .init=_credits_init,
  .focus=_credits_focus,
  .notify=_credits_notify,
  .update=_credits_update,
  .render=_credits_render,
};

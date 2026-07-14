/* modal_cutscene.c
 * Wraps some non-interactive cutscene.
 */

#include "game/bellacopia.h"

#define VTX_LIMIT 256 /* Maximum size of a caption, in glyphs. */
#define TYPEWRITE_TIME 0.080
#define TYPEWRITE_TIME_0 0.250 /* A longer delay before the first glyph of each step. */
#define PRESSA_TIME 0.500

struct modal_cutscene {
  struct modal hdr;
  int strix_title; // strings:stories, but it's really just a loose identifier.
  int context; // CUTSCENE_CONTEXT_*, under what circumstances were we invoked?
  void (*cb)(void *userdata);
  void *userdata;
  const struct story_step *step; // Never null. Advances until (step->render==0).
  struct egg_render_tile vtxv[VTX_LIMIT];
  int vtxc; // Count of valid vertices, ie the whole string.
  int vtxp; // Count to display. 0..vtxc.
  double typewriteclock;
  double stepclock;
  int stepframec;
  int blackout;
  int pressa;
};

#define MODAL ((struct modal_cutscene*)modal)

/* Cleanup.
 */
 
static void _cutscene_del(struct modal *modal) {
}

/* Prepare step, for the currently focussed one.
 */
 
static void cutscene_prepare_step(struct modal *modal) {
  MODAL->typewriteclock=TYPEWRITE_TIME_0;
  MODAL->vtxc=MODAL->vtxp=0;
  MODAL->stepclock=0.0;
  MODAL->stepframec=0;
  MODAL->pressa=0;
  
  const char *src=0;
  int srcc=text_get_string(&src,RID_strings_stories,MODAL->step->strix);
  if (srcc<0) srcc=0;
  int x=10,x0=10,y=CUTSCENE_DIVIDE_Y+10;
  int srcp=0;
  while (srcp<srcc) {
  
    // Newlines and other whitespace.
    if (src[srcp]==0x0a) {
      x=x0;
      y+=8;
      srcp++;
      continue;
    }
    if ((unsigned char)src[srcp]<=0x20) {
      x+=8;
      srcp++;
      continue;
    }
    
    // Measure the next word. If it will breach the margin, either chop it or hop to the next line.
    int wordlen=1;
    while ((srcp+wordlen<srcc)&&((unsigned char)src[srcp+wordlen]>0x20)) wordlen++;
    if (x+wordlen*8>FBW-x0) { // Implicit line break.
      if (x>x0) { // Hop to the next line and try again. (yeah yeah yeah, remeasuring and everything)
        x=x0;
        y+=8;
        continue;
      }
      // Cut it mid-word.
      wordlen=(FBW-(x0<<1))/8;
    }
    
    // Emit vertices for the word and consume it.
    for (;wordlen-->0;srcp++,x+=8) {
      if (MODAL->vtxc<VTX_LIMIT) {
        MODAL->vtxv[MODAL->vtxc++]=(struct egg_render_tile){
          .x=x,
          .y=y,
          .tileid=src[srcp],
          .xform=0,
        };
      }
    }
  }
}

/* Init.
 */
 
static int _cutscene_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=1;
  modal->interactive=1; // well, we're not really, but we do suppress interaction with other layers.
  
  if (!arg||(argc!=sizeof(struct modal_args_cutscene))) return -1;
  const struct modal_args_cutscene *args=arg;
  MODAL->strix_title=args->strix_title;
  MODAL->context=args->context;
  MODAL->cb=args->cb;
  MODAL->userdata=args->userdata;
  
  MODAL->step=story_stepv_by_strix(MODAL->strix_title);
  if (!MODAL->step||!MODAL->step->render) {
    fprintf(stderr,"%s:%d: Story %d has no content.\n",__FILE__,__LINE__,MODAL->strix_title);
    return -1;
  }
  
  MODAL->blackout=1;
  cutscene_prepare_step(modal);
  
  return 0;
}

/* Focus.
 */
 
static void _cutscene_focus(struct modal *modal,int focus) {
}

/* Notify.
 */
 
static void _cutscene_notify(struct modal *modal,int k,int v) {
  if (k==EGG_PREF_LANG) {
  }
}

/* Update.
 */
 
static void _cutscene_update(struct modal *modal,double elapsed) {

  MODAL->stepclock+=elapsed;

  /* West to cancel.
   */
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    bm_sound(RID_sound_uicancel);
    modal->defunct=1;
    if (MODAL->cb) {
      MODAL->cb(MODAL->userdata);
      MODAL->cb=0;
    }
    return;
  }
  
  /* South to skip to the end of a step in progress, or to the next step.
   */
  if (MODAL->blackout) {
    if (!(g.input[0]&EGG_BTN_SOUTH)) MODAL->blackout=0;
  } else if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    if (MODAL->vtxp<MODAL->vtxc) {
      bm_sound(RID_sound_uimotion);
      MODAL->vtxp=MODAL->vtxc;
    } else {
      bm_sound(RID_sound_uiactivate);
      if (MODAL->step[1].render) { // Next step exists.
        MODAL->step++;
        cutscene_prepare_step(modal);
      } else { // Dismiss.
        modal->defunct=1;
        if (MODAL->cb) {
          MODAL->cb(MODAL->userdata);
          MODAL->cb=0;
        }
      }
    }
    return;
  }
  
  /* Advance typewriter.
   * The clock keeps running; it times our "Press A" indicator after the text is paid out.
   */
  if ((MODAL->typewriteclock-=elapsed)<=0.0) {
    if (MODAL->vtxp<MODAL->vtxc) {
      MODAL->typewriteclock+=TYPEWRITE_TIME;
      MODAL->vtxp++;
      bm_sound(RID_sound_tick);
    } else {
      MODAL->typewriteclock+=PRESSA_TIME;
      MODAL->pressa^=1;
    }
  }
}

/* Render.
 */
 
static void _cutscene_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  MODAL->step->render(MODAL->stepclock,MODAL->step->strix,MODAL->stepframec++);
  graf_fill_rect(&g.graf,0,CUTSCENE_DIVIDE_Y,FBW,FBH-CUTSCENE_DIVIDE_Y,0x000000ff);
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_tile_batch(&g.graf,MODAL->vtxv,MODAL->vtxp);
  
  // "Press A" indicator.
  if (MODAL->vtxp>=MODAL->vtxc) {
    graf_set_image(&g.graf,RID_image_pause);
    graf_tile(&g.graf,FBW-9,FBH-9,MODAL->pressa?0x09:0x0a,0);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_cutscene={
  .name="cutscene",
  .objlen=sizeof(struct modal_cutscene),
  .del=_cutscene_del,
  .init=_cutscene_init,
  .focus=_cutscene_focus,
  .notify=_cutscene_notify,
  .update=_cutscene_update,
  .render=_cutscene_render,
};

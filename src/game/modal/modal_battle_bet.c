#include "game/bellacopia.h"

#define WAGER_MIN 1
#define WAGER_MAX 9
#define DIFFICULTY_MIN 0
#define DIFFICULTY_MAX 5
#define BIAS_MIN 0x30
#define BIAS_MAX 0xe0

#define XMARGIN 2
#define YMARGIN 2
#define RIGHTW 100

// Store the inputs globally so they'll be the same by default next time we open.
static int modal_battle_bet_wager=1;
static int modal_battle_bet_difficulty=3;

struct modal_battle_bet {
  struct modal hdr;
  void (*cb)(int wager,int bias,int payout,void *userdata); // wager zero if cancelled.
  void *userdata;
  int payout; // Computed from (wager,difficulty).
  int inp; // (0,1) = (wager,difficulty)
  int lineh; // From font. We need it to position the dynamic text.
  int boxx,boxy,boxw,boxh;
  int texid,texw,texh;
  double animclock;
  int animframe;
};

#define MODAL ((struct modal_battle_bet*)modal)

/* Cleanup.
 */
 
static void _battle_bet_del(struct modal *modal) {
  egg_texture_del(MODAL->texid);
}

/* Recalculate (payout) based on (wager,difficulty).
 * Also sanitized (wager,difficulty), tho callers should not permit them to go invalid either.
 */
 
static void battle_bet_recalc(struct modal *modal) {
  if (modal_battle_bet_wager<WAGER_MIN) modal_battle_bet_wager=WAGER_MIN;
  else if (modal_battle_bet_wager>WAGER_MAX) modal_battle_bet_wager=WAGER_MAX;
  if (modal_battle_bet_difficulty<DIFFICULTY_MIN) modal_battle_bet_difficulty=DIFFICULTY_MIN;
  else if (modal_battle_bet_difficulty>DIFFICULTY_MAX) modal_battle_bet_difficulty=DIFFICULTY_MAX;
  int mlt=modal_battle_bet_difficulty*2;
  if (mlt<1) mlt=1; // Difficulty zero, you just win your money back, like a fool.
  MODAL->payout=modal_battle_bet_wager*mlt;
}

/* The static text labels are all stamped on to one texture.
 */
 
static void battle_bet_rebuild_text(struct modal *modal) {

  /* Acquire text and take measurements.
   */
  MODAL->lineh=font_get_line_height(g.font);
  const char *s1=0,*s2=0,*s3=0;
  int s1c=text_get_string(&s1,RID_strings_dialogue,167); // Wager
  int s2c=text_get_string(&s2,RID_strings_dialogue,168); // Difficulty
  int s3c=text_get_string(&s3,RID_strings_dialogue,169); // Payout
  int w1=font_measure_string(g.font,s1,s1c);
  int w2=font_measure_string(g.font,s2,s2c);
  int w3=font_measure_string(g.font,s3,s3c);
  
  /* Take full measure and allocate buffer.
   */
  MODAL->texw=1;
  if (w1>=MODAL->texw) MODAL->texw=w1;
  if (w2>=MODAL->texw) MODAL->texw=w2;
  if (w3>=MODAL->texw) MODAL->texw=w3;
  MODAL->texh=MODAL->lineh*3;
  uint32_t *rgba=calloc(MODAL->texw*4,MODAL->texh);
  if (!rgba) return;
  
  /* Render the three strings right-aligned.
   */
  font_render(rgba+MODAL->lineh*0*MODAL->texw+(MODAL->texw-w1),w1,MODAL->lineh,MODAL->texw*4,g.font,s1,s1c,0xffffffff);
  font_render(rgba+MODAL->lineh*1*MODAL->texw+(MODAL->texw-w2),w2,MODAL->lineh,MODAL->texw*4,g.font,s2,s2c,0xffffffff);
  font_render(rgba+MODAL->lineh*2*MODAL->texw+(MODAL->texw-w3),w3,MODAL->lineh,MODAL->texw*4,g.font,s3,s3c,0x00ff00ff);
  
  /* Upload to texture.
   */
  if (!MODAL->texid) MODAL->texid=egg_texture_new();
  egg_texture_load_raw(MODAL->texid,MODAL->texw,MODAL->texh,MODAL->texw*4,rgba,MODAL->texw*MODAL->texh*4);
  free(rgba);
  
  /* Resize box to fit.
   */
  MODAL->boxw=MODAL->texw+XMARGIN*3+RIGHTW;
  MODAL->boxh=MODAL->texh+YMARGIN*2;
  MODAL->boxx=(FBW>>1)-(MODAL->boxw>>1);
  MODAL->boxy=(FBH>>1)-(MODAL->boxh>>1);
}

/* Init.
 */
 
static int _battle_bet_init(struct modal *modal,const void *args,int argslen) {
  modal->opaque=0;
  modal->interactive=1;
  modal->blotter=1;
  
  battle_bet_rebuild_text(modal);
  
  if (args&&(argslen==sizeof(struct modal_args_battle_bet))) {
    const struct modal_args_battle_bet *ARGS=args;
    MODAL->cb=ARGS->cb;
    MODAL->userdata=ARGS->userdata;
  }
  
  battle_bet_recalc(modal);
  
  return 0;
}

/* Notify.
 */
 
static void _battle_bet_notify(struct modal *modal,int k,int v) {
  if (k==EGG_PREF_LANG) battle_bet_rebuild_text(modal);
}

/* Dismiss.
 */
 
static void battle_bet_dismiss(struct modal *modal) {
  modal->defunct=1;
  bm_sound(RID_sound_uicancel);
  if (MODAL->cb) {
    MODAL->cb(0,0,0,MODAL->userdata);
    MODAL->cb=0;
  }
}

/* Commit.
 */
 
static void battle_bet_commit(struct modal *modal) {
  
  /* (wager,difficulty,payout) are kept valid at all times.
   * But we do need to check that she has enough on hand to cover the wager.
   */
  int gold=store_get_fld16(NS_fld16_gold);
  if (gold<modal_battle_bet_wager) {
    struct modal_args_dialogue args={
      .rid=RID_strings_dialogue,
      .strix=2,
    };
    modal_spawn(&modal_type_dialogue,&args,sizeof(args));
    return;
  }
  
  /* Looks good. Tell our caller.
   */
  if (MODAL->cb) {
    int bias=BIAS_MIN+(modal_battle_bet_difficulty*(BIAS_MAX-BIAS_MIN+1))/5;
    if (bias<0) bias=0;
    else if (bias>0xff) bias=0xff;
    MODAL->cb(modal_battle_bet_wager,bias,MODAL->payout,MODAL->userdata);
    MODAL->cb=0;
  }
  modal->defunct=1;
}

/* Move cursor.
 */
 
static void battle_bet_move(struct modal *modal,int d) {
  bm_sound(RID_sound_uimotion);
  MODAL->inp=MODAL->inp?0:1;
}

/* Adjust focussed value.
 */
 
static void battle_bet_adjust(struct modal *modal,int d) {
  switch (MODAL->inp) {
    case 0: { // Wager.
        modal_battle_bet_wager+=d;
        if (modal_battle_bet_wager<WAGER_MIN) { modal_battle_bet_wager=WAGER_MIN; return; }
        if (modal_battle_bet_wager>WAGER_MAX) { modal_battle_bet_wager=WAGER_MAX; return; }
      } break;
    case 1: { // Difficulty.
        modal_battle_bet_difficulty+=d;
        if (modal_battle_bet_difficulty<DIFFICULTY_MIN) { modal_battle_bet_difficulty=DIFFICULTY_MIN; return; }
        if (modal_battle_bet_difficulty>DIFFICULTY_MAX) { modal_battle_bet_difficulty=DIFFICULTY_MAX; return; }
      } break;
    default: return;
  }
  bm_sound(RID_sound_uimotion);
  battle_bet_recalc(modal);
}

/* Update.
 */
 
static void _battle_bet_update(struct modal *modal,double elapsed) {

  if ((MODAL->animclock-=elapsed)<=0.0) {
    MODAL->animclock+=0.200;
    MODAL->animframe^=1;
  }

  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    battle_bet_dismiss(modal);
    return;
  }
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    battle_bet_commit(modal);
    return;
  }
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) battle_bet_move(modal,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) battle_bet_move(modal,1);
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) battle_bet_adjust(modal,-1);
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) battle_bet_adjust(modal,1);
}

/* Render.
 */
 
static void _battle_bet_render(struct modal *modal) {

  // Fill our box black.
  graf_fill_rect(&g.graf,MODAL->boxx,MODAL->boxy,MODAL->boxw,MODAL->boxh,0x000000ff);
  
  // Highlight active row.
  graf_fill_rect(&g.graf,
    MODAL->boxx+XMARGIN-1,
    MODAL->boxy+YMARGIN+MODAL->inp*MODAL->lineh-1,
    MODAL->boxw-XMARGIN*2+2,
    MODAL->lineh+1,
    0x001020ff
  );
  
  // Static text labels are a single texture.
  graf_set_input(&g.graf,MODAL->texid);
  graf_decal(&g.graf,MODAL->boxx+XMARGIN,MODAL->boxy+YMARGIN,0,0,MODAL->texw,MODAL->texh);
  
  int vmidx=MODAL->boxx+((XMARGIN*2+MODAL->texw+MODAL->boxw)>>1);
  
  /* Wager and payout are text from the tiles font.
   * Wager is always one digit, and payout can be one or two.
   */
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_tile(&g.graf,vmidx,MODAL->boxy+YMARGIN+(MODAL->lineh>>1),'0'+modal_battle_bet_wager,0);
  if (MODAL->payout>=10) {
    graf_tile(&g.graf,vmidx-4,MODAL->boxy+YMARGIN+MODAL->lineh*2+(MODAL->lineh>>1),'0'+MODAL->payout/10,0);
    graf_tile(&g.graf,vmidx+4,MODAL->boxy+YMARGIN+MODAL->lineh*2+(MODAL->lineh>>1),'0'+MODAL->payout%10,0);
  } else {
    graf_tile(&g.graf,vmidx,MODAL->boxy+YMARGIN+MODAL->lineh*2+(MODAL->lineh>>1),'0'+MODAL->payout,0);
  }
  
  /* Difficulty is tiles from image:pause. ca=ON cb=OFF cc,cd=FIRE
   */
  graf_set_image(&g.graf,RID_image_pause);
  int y=MODAL->boxy+YMARGIN+MODAL->lineh+(MODAL->lineh>>1);
  int x=vmidx-NS_sys_tilesize*2;
  int i=0;
  for (;i<DIFFICULTY_MAX;i++,x+=NS_sys_tilesize) {
    uint8_t tileid=0xcb;
    if (modal_battle_bet_difficulty>=DIFFICULTY_MAX) tileid=0xcc+MODAL->animframe;
    else if (i<modal_battle_bet_difficulty) tileid=0xca;
    graf_tile(&g.graf,x,y,tileid,0);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_battle_bet={
  .name="battle_bet",
  .objlen=sizeof(struct modal_battle_bet),
  .del=_battle_bet_del,
  .init=_battle_bet_init,
  .update=_battle_bet_update,
  .render=_battle_bet_render,
  .notify=_battle_bet_notify,
};

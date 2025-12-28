#include "game/game.h"

#define BATTLE_STAGE_SURPRISE     1 /* Screen flashes or something. */
#define BATTLE_STAGE_INTRO        2 /* Generic introduction. */
#define BATTLE_STAGE_PLAY         3 /* Implementation takes over. */
#define BATTLE_STAGE_FINAL        4 /* Generic denouement. */

#define LABEL_LIMIT 8

struct modal_battle {
  struct modal hdr;
  int playerc;
  int handicap;
  const struct battle_type *type; // REQUIRED
  void *ctx;
  int outcome; // -2,-1,0,1 = in progress, p1 wins, draw, p2 or cpu wins.
  int stage;
  double stageclock; // SURPRISE
  struct label {
    int texid,x,y,w,h;
  } labelv[LABEL_LIMIT];
  int labelc;
  void (*cb)(void *userdata,int outcome);
  void *userdata;
};

#define MODAL ((struct modal_battle*)modal)

/* Cleanup.
 */
 
static void _battle_del(struct modal *modal) {
  if (MODAL->ctx) MODAL->type->del(MODAL->ctx);
  while (MODAL->labelc-->0) egg_texture_del(MODAL->labelv[MODAL->labelc].texid);
}

/* Init. (before type)
 */
 
static int _battle_init(struct modal *modal) {
  modal->opaque=0; // We become opaque after the SURPRISE stage completes.
  modal->interactive=1;
  MODAL->outcome=-2; // Stays -2 until minigame finishes.
  MODAL->stage=BATTLE_STAGE_SURPRISE;
  MODAL->stageclock=1.000;
  return 0;
}

/* Labels.
 */
 
static void battle_drop_labels(struct modal *modal) {
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) label->w=label->h=0;
}

// (x,y) for a new label are undefined.
static struct label *battle_add_label(struct modal *modal,const char *src,int srcc,uint32_t rgba) {
  struct label *label=0;
  struct label *q=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;q++) {
    if (!q->w) {
      label=q;
      break;
    }
  }
  if (!label) {
    if (MODAL->labelc>=LABEL_LIMIT) return 0;
    label=MODAL->labelv+MODAL->labelc++;
    memset(label,0,sizeof(struct label));
  }
  if (label->texid) egg_texture_del(label->texid);
  label->texid=font_render_to_texture(0,g.font,src,srcc,FBW,font_get_line_height(g.font),rgba);
  egg_texture_get_size(&label->w,&label->h,label->texid);
  return label;
}

static void battle_pack_labels(struct modal *modal) {
  int htotal=0,i=MODAL->labelc;
  struct label *label=MODAL->labelv;
  for (;i-->0;label++) {
    label->x=(FBW>>1)-(label->w>>1);
    htotal+=label->h;
  }
  int y=(FBH>>1)-(htotal>>1);
  for (i=MODAL->labelc,label=MODAL->labelv;i-->0;label++) {
    label->y=y;
    y+=label->h;
  }
}

/* Implementation's callback for finished.
 */
 
static void battle_cb_finish(void *userdata,int outcome) {
  struct modal *modal=userdata;
  MODAL->outcome=outcome;
  MODAL->stage=BATTLE_STAGE_FINAL;
  battle_drop_labels(modal);
  char msg[11]="outcome: .";
  if (outcome>0) msg[9]='+'; else if (outcome<0) msg[9]='-'; else msg[9]='?';
  battle_add_label(modal,msg,10,0xffffffff);
  battle_pack_labels(modal);
  //TODO Consequences. Adjust HP, gold, etc.
  //TODO Standard reporting to user.
  if (MODAL->cb) {
    MODAL->cb(MODAL->userdata,outcome);
    MODAL->cb=0;
  }
}

/* Init. (with type and launch params)
 */
 
static int battle_init_configured(struct modal *modal) {

  if (!(MODAL->ctx=MODAL->type->init(MODAL->playerc,MODAL->handicap,battle_cb_finish,modal))) return -1;
  
  /* We start in SURPRISE stage.
   * If it's 2-player, skip that and go right into INTRO.
   * Either way, prep the INTRO labels.
   */
  if (MODAL->playerc>=2) {
    MODAL->stage=BATTLE_STAGE_INTRO;
  }
  
  {
    char msg[256];
    int msgc=verbiage_begin_battle(msg,sizeof(msg),MODAL->type);
    if ((msgc<0)||(msgc>sizeof(msg))) msgc=0;
    struct label *label=battle_add_label(modal,msg,msgc,0xffffffff);
  }
  
  battle_pack_labels(modal);
  
  return 0;
}

/* Update, SURPRISE stage.
 */
 
static void battle_update_SURPRISE(struct modal *modal,double elapsed) {
  if ((MODAL->stageclock-=elapsed)<=0.0) {
    MODAL->stage=BATTLE_STAGE_INTRO;
    modal->opaque=1;
  }
}

/* Update, INTRO stage.
 */
 
static void battle_update_INTRO(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    MODAL->stage=BATTLE_STAGE_PLAY;
  }
}

/* Update, FINAL stage.
 */
 
static void battle_update_FINAL(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    modal->defunct=1;
  }
}

/* Update.
 */
 
static void _battle_update(struct modal *modal,double elapsed) {
  if (!MODAL->ctx) return;
  switch (MODAL->stage) {
    case BATTLE_STAGE_SURPRISE: battle_update_SURPRISE(modal,elapsed); break;
    case BATTLE_STAGE_INTRO: battle_update_INTRO(modal,elapsed); break;
    case BATTLE_STAGE_PLAY: MODAL->type->update(MODAL->ctx,elapsed); break;
    case BATTLE_STAGE_FINAL: battle_update_FINAL(modal,elapsed); break;
  }
}

/* Render, SURPRISE stage.
 * This stage alone is not opaque.
 */
 
static void battle_render_SURPRISE(struct modal *modal) {
  double adjp=MODAL->stageclock*3.0;
  int adjpi=(int)adjp;
  if (adjpi<0) adjpi=0;
  adjp-=adjpi;
  int alpha=(int)(adjp*255.0);
  if (alpha<0) alpha=0; else if (alpha>0xff) alpha=0xff;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0xff000000|alpha);
}

/* Render, INTRO stage.
 */
 
static void battle_render_INTRO(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) {
    graf_set_input(&g.graf,label->texid);
    graf_decal(&g.graf,label->x,label->y,0,0,label->w,label->h);
  }
  //TODO Show a little gamepad with blinking buttons and labels, to describe the battle's input.
}

/* Render, FINAL stage.
 */
 
static void battle_render_FINAL(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x603010ff);
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) {
    graf_set_input(&g.graf,label->texid);
    graf_decal(&g.graf,label->x,label->y,0,0,label->w,label->h);
  }
}

/* Render.
 */
 
static void _battle_render(struct modal *modal) {
  switch (MODAL->stage) {
    case BATTLE_STAGE_SURPRISE: battle_render_SURPRISE(modal); break;
    case BATTLE_STAGE_INTRO: battle_render_INTRO(modal); break;
    case BATTLE_STAGE_PLAY: {
        if (MODAL->ctx) {
          MODAL->type->render(MODAL->ctx);
        } else {
          graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
        }
      } break;
    case BATTLE_STAGE_FINAL: battle_render_FINAL(modal); break;
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_battle={
  .name="battle",
  .objlen=sizeof(struct modal_battle),
  .del=_battle_del,
  .init=_battle_init,
  .update=_battle_update,
  .render=_battle_render,
};

/* Public ctor.
 */
 
struct modal *modal_new_battle(const struct battle_type *type,int playerc,int handicap) {
  if (!type) return 0;
  if ((playerc<1)||(playerc>2)) return 0;
  
  struct modal *modal=modal_new(&modal_type_battle);
  if (!modal) return 0;
  
  MODAL->playerc=playerc;
  MODAL->handicap=handicap;
  MODAL->type=type;
  
  if (battle_init_configured(modal)<0) {
    modal_del(modal);
    return 0;
  }
  
  if (modal_push(modal)<0) {
    modal_del(modal);
    return 0;
  }
  return modal;
}

/* Trivial public accessors.
 */
 
void modal_battle_set_callback(struct modal *modal,void (*cb)(void *userdata,int outcome),void *userdata) {
  if (!modal||(modal->type!=&modal_type_battle)) return;
  MODAL->cb=cb;
  MODAL->userdata=userdata;
}

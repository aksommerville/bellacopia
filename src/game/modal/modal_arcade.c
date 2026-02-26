/* modal_arcade.c
 * UI for selecting a battle, player count, and handicap.
 * Then we launch that battle.
 *
 * XXX The whole presentation is temporary but will be around for a while.
 * Gearing for convenience as I develop the minigames.
 */

#include "game/bellacopia.h"
#include "game/battle/battle.h"

#define PRESELECT_BATTLE NS_battle_counting

#define ROWC 20 /* FBH / font height */
#define KEY_REPEAT_INITIAL 0.250
#define KEY_REPEAT_ONGOING 0.080
#define INPUT_BLACKOUT 0.333 /* Ignore South and West for so long after returning from a game, they might keep tapping. */

#define LABEL_LIMIT 30
#define LABELID_PLAYERS 1
#define LABELID_DIFFICULTY 2
#define LABELID_BIAS 3
#define LABELID_OUTCOME 4
#define LABELID_BATTLE 5 /* +row. Displayed row, not including scroll. */

struct modal_arcade {
  struct modal hdr;
  int battle; // NS_battle_*
  int players; // (0,1,2) = (princess-vs-monster, dot-vs-monster, dot-vs-princess)
  int difficulty; // 0..255
  int bias; // 0..255
  int outcome; // From most recent game, or zero initially.
  int scroll; // Position in battle list of first displayed row.
  int battlec;
  double horzclock,vertclock;
  int horzpv,vertpv;
  int horzc; // handicap adjustment gets wider as it runs
  double input_blackout;
  
  struct label {
    int labelid;
    int x,y,w,h;
    int texid;
  } labelv[LABEL_LIMIT];
  int labelc;
};

#define MODAL ((struct modal_arcade*)modal)

/* Cleanup.
 */
 
static void _arcade_del(struct modal *modal) {
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
}

/* Find label.
 */
 
static struct label *arcade_label_by_id(struct modal *modal,int labelid) {
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) {
    if (label->labelid==labelid) return label;
  }
  return 0;
}

/* Add label.
 */
 
static struct label *arcade_add_label(struct modal *modal,int labelid) {
  if (MODAL->labelc>=LABEL_LIMIT) return 0;
  struct label *label=MODAL->labelv+MODAL->labelc++;
  memset(label,0,sizeof(struct label));
  label->labelid=labelid;
  return label;
}

/* Rewrite label texture.
 */
 
static void arcade_rewrite_label_battle(struct modal *modal,struct label *label,int battlep) {
  // Since this is a temporary dev tool, use the internal names, they're convenient. Strings exist too, for real life.
  const char *src=0;
  int id=battlep+1;
  const struct battle_type *type=battle_type_by_id(id);
  if (type) src=type->name;
  char tmp[64];
  int tmpc=0;
  if (src) {
    tmpc=snprintf(tmp,sizeof(tmp),"%d: %s",id,src);
    if ((tmpc<0)||(tmpc>=sizeof(tmp))) tmpc=0;
  }
  if (tmpc) {
    int ntexid=font_render_to_texture(label->texid,g.font,tmp,tmpc,FBW,font_get_line_height(g.font),0xffffffff);
    if (ntexid<=0) return;
    label->texid=ntexid;
    egg_texture_get_size(&label->w,&label->h,label->texid);
  } else {
    label->w=label->h=0;
  }
}
 
static void arcade_rewrite_label_players(struct modal *modal,struct label *label) {
  const char *src="?";
  switch (MODAL->players) {
    case 0: src="princess"; break;
    case 1: src="dot"; break;
    case 2: src="2-player"; break;
  }
  int ntexid=font_render_to_texture(label->texid,g.font,src,-1,FBW,font_get_line_height(g.font),0xffffffff);
  if (ntexid<=0) return;
  label->texid=ntexid;
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->x=FBW-1-label->w;
  label->y=1;
}
 
static void arcade_rewrite_label_difficulty(struct modal *modal,struct label *label) {
  char src[32];
  int srcc=snprintf(src,sizeof(src),"diff 0x%02x",MODAL->difficulty);
  if ((srcc<0)||(srcc>=sizeof(src))) srcc=0;
  int ntexid=font_render_to_texture(label->texid,g.font,src,srcc,FBW,font_get_line_height(g.font),0xffffffff);
  if (ntexid<=0) return;
  label->texid=ntexid;
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->x=FBW-1-label->w;
  label->y=label->h+1;
}
 
static void arcade_rewrite_label_bias(struct modal *modal,struct label *label) {
  char src[32];
  int srcc=snprintf(src,sizeof(src),"bias 0x%02x",MODAL->bias);
  if ((srcc<0)||(srcc>=sizeof(src))) srcc=0;
  int ntexid=font_render_to_texture(label->texid,g.font,src,srcc,FBW,font_get_line_height(g.font),0xffffffff);
  if (ntexid<=0) return;
  label->texid=ntexid;
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->x=FBW-1-label->w;
  label->y=label->h*2+1;
}
 
static void arcade_rewrite_label_outcome(struct modal *modal,struct label *label) {
  char src[32];
  int srcc=snprintf(src,sizeof(src),"outcome %d",MODAL->outcome);
  if ((srcc<0)||(srcc>=sizeof(src))) srcc=0;
  int ntexid=font_render_to_texture(label->texid,g.font,src,srcc,FBW,font_get_line_height(g.font),0xffffffff);
  if (ntexid<=0) return;
  label->texid=ntexid;
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->x=FBW-1-label->w;
  label->y=label->h*3+1;
}

/* Rebuild all labels.
 */
 
static int arcade_rebuild_labels(struct modal *modal) {
  while (MODAL->labelc>0) {
    MODAL->labelc--;
    egg_texture_del(MODAL->labelv[MODAL->labelc].texid);
  }
  struct label *label;
  int rowh=font_get_line_height(g.font);
  int row=0;
  for (;row<ROWC;row++) {
    if (!(label=arcade_add_label(modal,LABELID_BATTLE+row))) return -1;
    label->x=1;
    label->y=row*rowh;
    arcade_rewrite_label_battle(modal,label,MODAL->scroll+row);
  }
  if (!(label=arcade_add_label(modal,LABELID_PLAYERS))) return -1;
  arcade_rewrite_label_players(modal,label);
  if (!(label=arcade_add_label(modal,LABELID_DIFFICULTY))) return -1;
  arcade_rewrite_label_difficulty(modal,label);
  if (!(label=arcade_add_label(modal,LABELID_BIAS))) return -1;
  arcade_rewrite_label_bias(modal,label);
  if (!(label=arcade_add_label(modal,LABELID_OUTCOME))) return -1;
  arcade_rewrite_label_outcome(modal,label);
  return 0;
}

/* Init.
 */
 
static int _arcade_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=1;
  modal->interactive=1;
  
  MODAL->battle=NS_battle_fishing;
  MODAL->players=1;
  MODAL->difficulty=0x80;
  MODAL->bias=0x80;
  
  // Count the battles. Assume that they are id'd contiguously.
  while (battle_type_by_id(MODAL->battlec+1)) MODAL->battlec++;
  
  // Let us appoint one battle as starting point, for when I'm developing a new battle dozens of spaces into the list.
  if (PRESELECT_BATTLE) {
    MODAL->battle=PRESELECT_BATTLE;
    MODAL->scroll=(PRESELECT_BATTLE/ROWC)*ROWC;
  }
  
  arcade_rebuild_labels(modal);
  
  return 0;
}

/* Focus.
 */
 
static void _arcade_focus(struct modal *modal,int focus) {
}

/* Notify.
 */
 
static void _arcade_notify(struct modal *modal,int k,int v) {
  if (k==EGG_PREF_LANG) {
    // Tempting to arcade_rebuild_labels() but also pointless, we're not using strings yet.
  }
}

/* Begin game.
 */

static void arcade_cb_battle(struct modal *battle_modal,int outcome,void *userdata) {
  struct modal *modal=userdata;
  fprintf(stderr,"%s: outcome=%d\n",__func__,outcome);
  MODAL->outcome=outcome;
  struct label *label=arcade_label_by_id(modal,LABELID_OUTCOME);
  if (label) arcade_rewrite_label_outcome(modal,label);
  MODAL->input_blackout=INPUT_BLACKOUT;
}
 
static void arcade_begin_game(struct modal *modal) {
  fprintf(stderr,"%s battle=%d players=%d\n",__func__,MODAL->battle,MODAL->players);//TODO
  struct modal_args_battle args={
    .battle=MODAL->battle,
    .args={
      .difficulty=MODAL->difficulty,
      .bias=MODAL->bias,
    },
    .cb=arcade_cb_battle,
    .userdata=modal,
    .skip_prompt=1,
    .no_store=1, // Store might not be initialized yet. Ticking its battleclock is first off wrong, and second, would cause the existing save to get clobbered.
  };
  switch (MODAL->players) {
    case 0: { // Princess vs Monster
        args.args.lctl=0;
        args.args.rctl=0;
        args.args.lface=NS_face_princess;
        args.args.rface=NS_face_monster;
      } break;
    case 1: { // Dot vs Monster
        args.args.lctl=1;
        args.args.rctl=0;
        args.args.lface=NS_face_dot;
        args.args.rface=NS_face_monster;
      } break;
    case 2: { // Dot vs Princess
        args.args.lctl=1;
        args.args.rctl=2;
        args.args.lface=NS_face_dot;
        args.args.rface=NS_face_princess;
      } break;
    default: return;
  }
  struct modal *battle_modal=modal_spawn(&modal_type_battle,&args,sizeof(args));
  if (!battle_modal) return;
}

/* Adjust selections.
 */
 
static void arcade_adjust_difficulty(struct modal *modal,int d) {
  if (!d) return;
  int nv=MODAL->difficulty+d;
  if (nv<0) nv=0;
  else if (nv>0xff) nv=0xff;
  if (nv==MODAL->difficulty) return;
  MODAL->difficulty=nv;
  bm_sound(RID_sound_uimotion);
  struct label *label=arcade_label_by_id(modal,LABELID_DIFFICULTY);
  if (label) arcade_rewrite_label_difficulty(modal,label);
}
 
static void arcade_adjust_bias(struct modal *modal,int d) {
  if (!d) return;
  int nv=MODAL->bias+d;
  if (nv<0) nv=0;
  else if (nv>0xff) nv=0xff;
  if (nv==MODAL->bias) return;
  MODAL->bias=nv;
  bm_sound(RID_sound_uimotion);
  struct label *label=arcade_label_by_id(modal,LABELID_BIAS);
  if (label) arcade_rewrite_label_bias(modal,label);
}
 
static void arcade_adjust_battle(struct modal *modal,int d) {
  if (!d) return;
  int nv=MODAL->battle+d;
  if (nv<1) nv=MODAL->battlec;
  else if (nv>MODAL->battlec) nv=1;
  if (nv==MODAL->battle) return;
  MODAL->battle=nv;
  bm_sound(RID_sound_uimotion);
  int relp=MODAL->battle-1-MODAL->scroll;
  int scrolled=0;
  while (relp<0) { relp+=ROWC; MODAL->scroll-=ROWC; scrolled=1; }
  while (relp>=ROWC) { relp-=ROWC; MODAL->scroll+=ROWC; scrolled=1; }
  if (scrolled) arcade_rebuild_labels(modal);
}

static void arcade_adjust_players(struct modal *modal,int d) {
  if (!d) return;
  int nv=MODAL->players+d;
  if (nv<0) nv=2; else if (nv>2) nv=0;
  MODAL->players=nv;
  bm_sound(RID_sound_uimotion);
  struct label *label=arcade_label_by_id(modal,LABELID_PLAYERS);
  if (label) arcade_rewrite_label_players(modal,label);
}

/* Update.
 */
 
static void _arcade_update(struct modal *modal,double elapsed) {

  // Difficulty and bias have a range of 255. Use key-repeat.
  // Hold EAST to adjust difficulty; bias by default.
  int horz=g.input[0]&(EGG_BTN_LEFT|EGG_BTN_RIGHT);
  if (horz==MODAL->horzpv) {
    int d=(horz==EGG_BTN_LEFT)?-1:(horz==EGG_BTN_RIGHT)?1:0;
    if (d) {
      if ((MODAL->horzclock-=elapsed)<=0.0) {
        MODAL->horzclock+=KEY_REPEAT_ONGOING;
        MODAL->horzc++;
        if (g.input[0]&EGG_BTN_EAST) arcade_adjust_difficulty(modal,d*MODAL->horzc/5);
        else arcade_adjust_bias(modal,d*MODAL->horzc/5);
      }
    }
  } else {
    MODAL->horzpv=horz;
    MODAL->horzclock=KEY_REPEAT_INITIAL;
    MODAL->horzc=1;
    if (g.input[0]&EGG_BTN_EAST) arcade_adjust_difficulty(modal,(horz==EGG_BTN_LEFT)?-1:(horz==EGG_BTN_RIGHT)?1:0);
    else arcade_adjust_bias(modal,(horz==EGG_BTN_LEFT)?-1:(horz==EGG_BTN_RIGHT)?1:0);
  }
  
  // Potentially huge list of battles. Use key-repeat.
  int vert=g.input[0]&(EGG_BTN_UP|EGG_BTN_DOWN);
  if (vert==MODAL->vertpv) {
    int d=(vert==EGG_BTN_UP)?-1:(vert==EGG_BTN_DOWN)?1:0;
    if (d) {
      if ((MODAL->vertclock-=elapsed)<=0.0) {
        MODAL->vertclock+=KEY_REPEAT_ONGOING;
        arcade_adjust_battle(modal,d);
      }
    }
  } else {
    MODAL->vertpv=vert;
    MODAL->vertclock=KEY_REPEAT_INITIAL;
    arcade_adjust_battle(modal,(vert==EGG_BTN_UP)?-1:(vert==EGG_BTN_DOWN)?1:0);
  }
  
  // Player selection uses the more familiar impulse-only adjustment.
  if ((g.input[0]&EGG_BTN_L1)&&!(g.pvinput[0]&EGG_BTN_L1)) arcade_adjust_players(modal,-1);
  if ((g.input[0]&EGG_BTN_R1)&&!(g.pvinput[0]&EGG_BTN_R1)) arcade_adjust_players(modal,1);
  
  // Play and cancel only if not blacked out.
  if (MODAL->input_blackout>0.0) {
    MODAL->input_blackout-=elapsed;
  } else {
  
    // SOUTH to begin play.
    if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
      arcade_begin_game(modal);
      return;
    }

    // WEST to cancel.
    if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
      bm_sound(RID_sound_uicancel);
      modal->defunct=1;
      return;
    }
  }
}

/* Render.
 */
 
static void _arcade_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x201030ff);
  
  int battlep=MODAL->battle-1;
  battlep-=MODAL->scroll;
  if ((battlep>=0)&&(battlep<ROWC)) {
    struct label *label=arcade_label_by_id(modal,LABELID_BATTLE+battlep);
    if (label) {
      graf_fill_rect(&g.graf,label->x-1,label->y-1,label->w+2,label->h+1,0x000000ff);
    }
  }
  
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) {
    graf_set_input(&g.graf,label->texid);
    if (label->labelid-LABELID_BATTLE==battlep) {
      graf_set_tint(&g.graf,0xffff00ff);
    }
    graf_decal(&g.graf,label->x,label->y,0,0,label->w,label->h);
    graf_set_tint(&g.graf,0);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_arcade={
  .name="arcade",
  .objlen=sizeof(struct modal_arcade),
  .del=_arcade_del,
  .init=_arcade_init,
  .focus=_arcade_focus,
  .notify=_arcade_notify,
  .update=_arcade_update,
  .render=_arcade_render,
};

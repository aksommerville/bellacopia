#include "game/bellacopia.h"

#define LABEL_LIMIT 7
#define ALPHABET_PRICE 50
#define TRANSLATION_PRICE 100

struct modal_linguist {
  struct modal hdr;
  int x,y,w,h; // For the outer box.
  
  struct label {
    int x,y,w,h; // Framebuffer pixels. The main box won't move.
    int texid;
    int fld; // Zero for read-only, or one of the purchasable things.
    int price;
  } labelv[LABEL_LIMIT];
  int labelc;
  int labelp;
};

#define MODAL ((struct modal_linguist*)modal)

/* Cleanup.
 */
 
static void _linguist_del(struct modal *modal) {
  struct label *label=MODAL->labelv;
  int i=MODAL->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
}

/* Add a blank label.
 * Bounds and texid will be zero; caller must set them after.
 */
 
static struct label *linguist_add_label(struct modal *modal) {
  if (MODAL->labelc>=LABEL_LIMIT) return 0;
  struct label *label=MODAL->labelv+MODAL->labelc++;
  memset(label,0,sizeof(struct label));
  return label;
}

/* Initialize label for a purchaseable item.
 */
 
static void linguist_set_purchase_label(struct modal *modal,struct label *label,int strix,int price,int y) {
  label->price=price;

  // Establish bounds. The size is constant, because we will right-align the price.
  label->x=MODAL->x+25;
  label->w=MODAL->w-25-4;
  label->y=y;
  label->h=font_get_line_height(g.font);

  // Acquire the text.
  const char *name=0;
  int namec=text_get_string(&name,RID_strings_dialogue,strix);
  char pdesc[32];
  int pdescc=snprintf(pdesc,sizeof(pdesc),"%d G",price);
  if ((pdescc<0)||(pdescc>sizeof(pdesc))) pdescc=0;
  
  // Allocate a temporary RGBA buffer.
  uint32_t *rgba=calloc(label->w<<2,label->h);
  if (!rgba) return;
  
  // Render name on the left and price on the right.
  int pricew=font_measure_string(g.font,pdesc,pdescc);
  if (pricew>label->w) pricew=label->w;
  font_render(rgba,label->w,label->h,label->w<<2,g.font,name,namec,0xffffffff);
  font_render(rgba+label->w-pricew,label->w-pricew,label->h,label->w<<2,g.font,pdesc,pdescc,0xffffffff);
  
  // Upload to a new texture.
  label->texid=egg_texture_new();
  egg_texture_load_raw(label->texid,label->w,label->h,label->w<<2,rgba,label->w*label->h*4);
  free(rgba);
}

/* Label for the alphabet key.
 */
 
static void linguist_set_alphabet_label(struct modal *modal,struct label *label) {

  // First acquire the key.
  const char *ptext="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char ctext[26];
  memcpy(ctext,ptext,26);
  cryptmsg_encrypt_in_place(ctext,26);
  
  // Allocate a texture for it.
  const int xbetween=2;
  const int ybetween=2;
  label->w=26*8+25*xbetween;
  label->h=2*8+ybetween;
  label->texid=egg_texture_new();
  egg_texture_load_raw(label->texid,label->w,label->h,label->w<<2,0,0);
  egg_texture_clear(label->texid);
  
  // Render (ptext) on top and (ctext) on bottom.
  graf_set_output(&g.graf,label->texid);
  graf_set_image(&g.graf,RID_image_fonttiles);
  int x=4,i=0;
  for (;i<26;i++,x+=8+xbetween) {
    graf_tile(&g.graf,x,4,ptext[i],0);
    graf_tile(&g.graf,x,12+ybetween,ctext[i],0);
  }
  graf_set_output(&g.graf,1);
  
  // Center horizontally.
  label->x=MODAL->x+(MODAL->w>>1)-(label->w>>1);
}

/* Label for the spell of translating.
 */
 
static void linguist_set_translation_label(struct modal *modal,struct label *label) {
  char spell[8];
  int spellc=cryptmsg_get_spell(spell,sizeof(spell),1);
  if ((spellc<0)||(spellc>sizeof(spell))) spellc=0;
  label->texid=font_render_to_texture(0,g.font,spell,spellc,MODAL->w,MODAL->h,0xffffffff);
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->x=MODAL->x+(MODAL->w>>1)-(label->w>>1);
}

/* Label for a thing unpurchased.
 */
 
static void linguist_set_buyme_label(struct modal *modal,struct label *label) {
  label->texid=font_render_to_texture(0,g.font,"$$$",3,MODAL->w,MODAL->h,0x00ff00ff);
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->x=MODAL->x+(MODAL->w>>1)-(label->w>>1);
}

/* Tear down and rebuild UI from scratch.
 */
 
static void linguist_rebuild_ui(struct modal *modal) {
  struct label *label=MODAL->labelv+MODAL->labelc;
  while (MODAL->labelc>0) {
    MODAL->labelc--;
    label--;
    egg_texture_del(label->texid);
  }
  
  // Constant size for the outer box. We need most of the screen's width.
  MODAL->w=FBW-20;
  MODAL->h=(FBH*3)/4;
  MODAL->x=(FBW>>1)-(MODAL->w>>1);
  MODAL->y=(FBH>>1)-(MODAL->h>>1);
  int y=MODAL->y+4;
  
  // Gather some facts.
  int bought_alphabet=store_get_fld(NS_fld_bought_alphabet);
  int bought_translation=store_get_fld(NS_fld_bought_translation);
  
  // Static prompt, centered near the top.
  if (label=linguist_add_label(modal)) {
    int strix=(bought_alphabet&&bought_translation)?58:57;
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_dialogue,strix);
    label->texid=font_render_to_texture(0,g.font,src,srcc,MODAL->w-8,MODAL->h,0xffffffff);
    egg_texture_get_size(&label->w,&label->h,label->texid);
    label->x=MODAL->x+(MODAL->w>>1)-(label->w>>1);
    label->y=y;
    y+=label->h+4;
  }
  
  // Selectable labels to purchase the things, if we don't have them yet.
  if (!bought_alphabet) {
    if (label=linguist_add_label(modal)) {
      linguist_set_purchase_label(modal,label,59,ALPHABET_PRICE,y);
      label->fld=NS_fld_bought_alphabet;
      y+=label->h;
    }
  }
  if (!bought_translation) {
    if (label=linguist_add_label(modal)) {
      linguist_set_purchase_label(modal,label,60,TRANSLATION_PRICE,y);
      label->fld=NS_fld_bought_translation;
      y+=label->h;
    }
  }
  
  // Static label for the Alphabet Key header. Fixed vertical position.
  if (label=linguist_add_label(modal)) {
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_dialogue,59);
    label->texid=font_render_to_texture(0,g.font,src,srcc,MODAL->w-8,font_get_line_height(g.font),0xffffffff);
    egg_texture_get_size(&label->w,&label->h,label->texid);
    label->x=MODAL->x+(MODAL->w>>1)-(label->w>>1);
    label->y=MODAL->y+55;
    y=label->y+label->h+4;
  }
  
  // Alphabet Key or the "buy me" placeholder.
  if (label=linguist_add_label(modal)) {
    label->y=y;
    if (bought_alphabet) {
      linguist_set_alphabet_label(modal,label);
    } else {
      linguist_set_buyme_label(modal,label);
    }
  }
  
  // Same idea for the Spell of Translating header.
  if (label=linguist_add_label(modal)) {
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_dialogue,60);
    label->texid=font_render_to_texture(0,g.font,src,srcc,MODAL->w-8,font_get_line_height(g.font),0xffffffff);
    egg_texture_get_size(&label->w,&label->h,label->texid);
    label->x=MODAL->x+(MODAL->w>>1)-(label->w>>1);
    label->y=MODAL->y+95;
    y=label->y+label->h+4;
  }
  
  if (label=linguist_add_label(modal)) {
    label->y=y;
    if (bought_translation) {
      linguist_set_translation_label(modal,label);
    } else {
      linguist_set_buyme_label(modal,label);
    }
  }
}

/* Init.
 */
 
static int _linguist_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=0;
  modal->interactive=1;
  modal->blotter=1;
  
  if (arg&&(argc==sizeof(struct modal_args_linguist))) {
    const struct modal_args_linguist *args=arg;
    // hmm i guess we don't really need this
  }
  
  linguist_rebuild_ui(modal);
  
  // Set initial cursor position. OOB is ok; there might not be any selectable labels.
  MODAL->labelp=0;
  while ((MODAL->labelp<MODAL->labelc)&&!MODAL->labelv[MODAL->labelp].fld) MODAL->labelp++;
  
  return 0;
}

/* Dismiss.
 */
 
static void linguist_dismiss(struct modal *modal) {
  bm_sound(RID_sound_uicancel);
  modal->defunct=1;
}

/* Activate selection.
 */
 
static void linguist_activate(struct modal *modal) {
  if ((MODAL->labelp<0)||(MODAL->labelp>=MODAL->labelc)) return;
  int fld=MODAL->labelv[MODAL->labelp].fld;
  if (!fld) return;
  if (store_get_fld(fld)) return; // Label shouldn't be enabled in this case but whatever.
  int price=MODAL->labelv[MODAL->labelp].price;
  int gold=store_get_fld16(NS_fld16_gold);
  if (gold<price) {
    modal_dialogue_simple(RID_strings_dialogue,2); // not enough gold
    return;
  }
  bm_sound(RID_sound_treasure);
  gold-=price;
  store_set_fld16(NS_fld16_gold,gold);
  store_set_fld(fld,1);
  linguist_rebuild_ui(modal);
  MODAL->labelp=0;
  while ((MODAL->labelp<MODAL->labelc)&&!MODAL->labelv[MODAL->labelp].fld) MODAL->labelp++;
}

/* Move cursor.
 */
 
static void linguist_move(struct modal *modal,int d) {
  if (MODAL->labelc<1) return;
  int panic=MODAL->labelc,ok=0;
  while (panic-->0) {
    MODAL->labelp+=d;
    if (MODAL->labelp<0) MODAL->labelp=MODAL->labelc-1;
    else if (MODAL->labelp>=MODAL->labelc) MODAL->labelp=0;
    if (MODAL->labelv[MODAL->labelp].fld) {
      ok=1;
      break;
    }
  }
  if (!ok) {
    MODAL->labelp=-1;
    return;
  }
  bm_sound(RID_sound_uimotion);
}

/* Update.
 */
 
static void _linguist_update(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) linguist_move(modal,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) linguist_move(modal,1);
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) linguist_activate(modal);
  else if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) linguist_dismiss(modal);
}

/* Notification.
 */
 
static void _linguist_notify(struct modal *modal,int k,int v) {
  switch (k) {
    case EGG_PREF_LANG: linguist_rebuild_ui(modal); break;
  }
}

/* Render.
 */
 
static void _linguist_render(struct modal *modal) {
  graf_fill_rect(&g.graf,MODAL->x,MODAL->y,MODAL->w,MODAL->h,0x000000ff);
  struct label *label=MODAL->labelv;
  int i=0;
  for (;i<MODAL->labelc;i++,label++) {
    graf_set_input(&g.graf,label->texid);
    graf_decal(&g.graf,label->x,label->y,0,0,label->w,label->h);
    if (i==MODAL->labelp) {
      graf_set_image(&g.graf,RID_image_pause);
      graf_tile(&g.graf,label->x-10,label->y+(label->h>>1),0x25,0);
    }
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_linguist={
  .name="linguist",
  .objlen=sizeof(struct modal_linguist),
  .del=_linguist_del,
  .init=_linguist_init,
  .update=_linguist_update,
  .render=_linguist_render,
  .notify=_linguist_notify,
};

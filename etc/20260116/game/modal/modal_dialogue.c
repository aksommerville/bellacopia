#include "game/game.h"

#define CHOICE_LIMIT 8
#define CURSOR_MARGIN 20
#define MARGINL 3
#define MARGINR 3
#define MARGINT 3
#define MARGINB 2 /* Font has some baked-in foot room. */
#define FOOT_SPACE 4 /* Below the box. */

struct modal_dialogue {
  // Message and choice positions are relative to the framebuffer, not the box.
  struct modal hdr;
  int texid,texw,texh; // Just the message text.
  int texx,texy;
  struct choice {
    int texid,w,h;
    int x,y;
    int choiceid;
  } choicev[CHOICE_LIMIT];
  int choicec;
  int choicep;
  void (*cb)(void *userdata,int choiceid);
  void *userdata;
  int bounds_dirty; // Recalculate bounds at next update.
  int boxx,boxy,boxw,boxh; // Blackout box behind everything.
  int input_blackout;
};

#define MODAL ((struct modal_dialogue*)modal)

/* Delete.
 */
 
static void _dialogue_del(struct modal *modal) {
  egg_texture_del(MODAL->texid);
  struct choice *choice=MODAL->choicev;
  int i=MODAL->choicec;
  for (;i-->0;choice++) egg_texture_del(choice->texid);
}

/* Init.
 */
 
static int _dialogue_init(struct modal *modal) {
  modal->opaque=0;
  modal->interactive=1;
  MODAL->bounds_dirty=1;
  MODAL->input_blackout=1;
  return 0;
}

/* Recalculate layout.
 */
 
static void dialogue_layout(struct modal *modal) {

  /* Inner bounds is the max of widths and the sum of heights (message and choices).
   * For these purposes, include the left cursor margin in choice widths.
   */
  int inw=MODAL->texw;
  int inh=MODAL->texh;
  struct choice *choice=MODAL->choicev;
  int i=MODAL->choicec;
  for (;i-->0;choice++) {
    int cw=CURSOR_MARGIN+choice->w;
    if (cw>inw) inw=cw;
    inh+=choice->h;
  }
  
  /* Box size is the inner size plus some margins.
   * Then the box is centered horizontally, and a fixed distance from the bottom.
   */
  MODAL->boxw=MARGINL+inw+MARGINR;
  MODAL->boxh=MARGINT+inh+MARGINB;
  MODAL->boxx=(FBW>>1)-(MODAL->boxw>>1);
  MODAL->boxy=FBH-FOOT_SPACE-MODAL->boxh;
  
  /* Message and choice positions are now trivially knowable from the box position.
   */
  MODAL->texx=MODAL->boxx+MARGINL; // Left-justified even if the box is too wide.
  MODAL->texy=MODAL->boxy+MARGINT;
  int y=MODAL->texy+MODAL->texh;
  for (choice=MODAL->choicev,i=MODAL->choicec;i-->0;choice++) {
    choice->x=MODAL->boxx+MARGINL+CURSOR_MARGIN;
    choice->y=y;
    y+=choice->h;
  }
}

/* Move cursor.
 */
 
static void dialogue_move(struct modal *modal,int d) {
  if (MODAL->choicec<1) return;
  bm_sound(RID_sound_uimotion,0.0);
  MODAL->choicep+=d;
  if (MODAL->choicep<0) MODAL->choicep=MODAL->choicec-1;
  else if (MODAL->choicep>=MODAL->choicec) MODAL->choicep=0;
}

/* Activate.
 */
 
static void dialogue_activate(struct modal *modal) {
  if (MODAL->choicec) {
    if ((MODAL->choicep<0)||(MODAL->choicep>=MODAL->choicec)) return;
    bm_sound(RID_sound_uiactivate,0.0);
    modal->defunct=1;
    if (MODAL->cb) {
      MODAL->cb(MODAL->userdata,MODAL->choicev[MODAL->choicep].choiceid);
      MODAL->cb=0;
    }
  } else {
    bm_sound(RID_sound_uiactivate,0.0);
    modal->defunct=1;
    if (MODAL->cb) {
      MODAL->cb(MODAL->userdata,0);
      MODAL->cb=0;
    }
  }
}

/* Cancel.
 */
 
static void dialogue_cancel(struct modal *modal) {
  bm_sound(RID_sound_uicancel,0.0);
  modal->defunct=1;
  if (MODAL->cb) {
    MODAL->cb(MODAL->userdata,-1);
    MODAL->cb=0;
  }
}

/* Update.
 */
 
static void _dialogue_update(struct modal *modal,double elapsed) {
  if (MODAL->bounds_dirty) {
    MODAL->bounds_dirty=0;
    dialogue_layout(modal);
  }
  //TODO arrive and dismiss animation
  
  if (MODAL->input_blackout) {
    if (!(g.input[0]&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_SOUTH|EGG_BTN_WEST))) {
      MODAL->input_blackout=0;
    }
  } else if (g.input[0]!=g.pvinput[0]) {
    if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) dialogue_move(modal,-1);
    else if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) dialogue_move(modal,1);
    if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) dialogue_activate(modal);
    else if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) dialogue_cancel(modal);
  }
}

/* Render.
 */
 
static void _dialogue_render(struct modal *modal) {
  //TODO Animate arrival and dismissal of box, with no content in it.
  graf_fill_rect(&g.graf,MODAL->boxx,MODAL->boxy,MODAL->boxw,MODAL->boxh,0x000000ff);
  graf_set_input(&g.graf,MODAL->texid);
  graf_decal(&g.graf,MODAL->texx,MODAL->texy,0,0,MODAL->texw,MODAL->texh);
  if (MODAL->choicec) {
    struct choice *choice=MODAL->choicev;
    int i=MODAL->choicec;
    for (;i-->0;choice++) {
      graf_set_input(&g.graf,choice->texid);
      graf_decal(&g.graf,choice->x,choice->y,0,0,choice->w,choice->h);
    }
    if ((MODAL->choicep>=0)&&(MODAL->choicep<MODAL->choicec)) {
      graf_set_image(&g.graf,RID_image_pause);
      choice=MODAL->choicev+MODAL->choicep;
      graf_tile(&g.graf,choice->x-10,choice->y+(choice->h>>1),0x25,0);
    }
  } else {
    //TODO blinking indicator at bottom right
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_dialogue={
  .name="dialogue",
  .objlen=sizeof(struct modal_dialogue),
  .del=_dialogue_del,
  .init=_dialogue_init,
  .update=_dialogue_update,
  .render=_dialogue_render,
};

/* Public ctor.
 */
 
struct modal *modal_new_dialogue(int rid,int strix) {
  struct modal *modal=modal_new(&modal_type_dialogue);
  if (!modal) return 0;
  if (rid&&strix) { // Just a convenience, so one-off static text can be a one-liner.
    if (modal_dialogue_set_res(modal,rid,strix)<0) {
      modal_del(modal);
      return 0;
    }
  }
  if (modal_push(modal)<0) {
    modal_del(modal);
    return 0;
  }
  return modal;
}

/* Set message.
 */

int modal_dialogue_set_text(struct modal *modal,const char *src,int srcc) {
  if (!modal||(modal->type!=&modal_type_dialogue)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (MODAL->texid) egg_texture_del(MODAL->texid);
  int wlimit=(FBW*3)/4;
  int hlimit=(FBH*1)/3;
  MODAL->texid=font_render_to_texture(0,g.font,src,srcc,wlimit,hlimit,0xffffffff);
  egg_texture_get_size(&MODAL->texw,&MODAL->texh,MODAL->texid);
  MODAL->bounds_dirty=1;
  return 0;
}
 
int modal_dialogue_set_res(struct modal *modal,int rid,int strix) {
  const char *src=0;
  int srcc=text_get_string(&src,rid,strix);
  return modal_dialogue_set_text(modal,src,srcc);
}

int modal_dialogue_set_fmt(struct modal *modal,int rid,int strix,const struct text_insertion *insv,int insc) {
  char tmp[1024];
  int tmpc=text_format_res(tmp,sizeof(tmp),rid,strix,insv,insc);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) tmpc=0;
  return modal_dialogue_set_text(modal,tmp,tmpc);
}

/* Set callback.
 */
 
void modal_dialogue_set_callback(struct modal *modal,void (*cb)(void *userdata,int choiceid),void *userdata) {
  if (!modal||(modal->type!=&modal_type_dialogue)) return;
  MODAL->cb=cb;
  MODAL->userdata=userdata;
}

/* Add choice.
 */
 
static int dialogue_unused_choiceid(const struct modal *modal) {
  int max=0,i=MODAL->choicec;
  const struct choice *choice=MODAL->choicev;
  for (;i-->0;choice++) {
    if (choice->choiceid>max) max=choice->choiceid;
  }
  if (max==INT_MAX) return 1; // The hell?
  return max+1;
}

int modal_dialogue_add_choice_text(struct modal *modal,int choiceid,const char *src,int srcc) {
  if (!modal||(modal->type!=&modal_type_dialogue)) return -1;
  if (MODAL->choicec>=CHOICE_LIMIT) return -1;
  if (choiceid<1) choiceid=dialogue_unused_choiceid(modal);
  struct choice *choice=MODAL->choicev+MODAL->choicec++;
  if ((choice->texid=font_render_to_texture(0,g.font,src,srcc,FBW,font_get_line_height(g.font),0xffffffff))<1) {
    MODAL->choicec--;
    return -1;
  }
  egg_texture_get_size(&choice->w,&choice->h,choice->texid);
  choice->choiceid=choiceid;
  MODAL->bounds_dirty=1;
  return choiceid;
}
 
int modal_dialogue_add_choice_res(struct modal *modal,int choiceid,int rid,int strix) {
  const char *src=0;
  int srcc=text_get_string(&src,rid,strix);
  return modal_dialogue_add_choice_text(modal,choiceid,src,srcc);
}

/* Add choices in a standard format for shops.
 */

int modal_dialogue_set_shop(struct modal *modal,const struct inventory *merch,int merchc) {
  if (!modal||(modal->type!=&modal_type_dialogue)) return -1;
  for (;merchc-->0;merch++) {
    int strix=100,itemname=0,dummy;
    if (merch->quantity) {
      strix=101;
    }
    strings_for_item(&itemname,&dummy,merch->itemid);
    struct text_insertion insv[]={
      {.mode='i',.i=merch->limit},
      {.mode='r',.r={.rid=RID_strings_dialogue,.strix=itemname}},
      {.mode='i',.i=merch->quantity},
    };
    char msg[64];
    int msgc=text_format_res(msg,sizeof(msg),RID_strings_dialogue,strix,insv,sizeof(insv)/sizeof(insv[0]));
    if ((msgc<0)||(msgc>sizeof(msg))) msgc=0;
    if (modal_dialogue_add_choice_text(modal,merch->itemid,msg,msgc)<0) return -1;
  }
  return 0;
}

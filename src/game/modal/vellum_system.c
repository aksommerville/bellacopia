#include "game/bellacopia.h"
#include "vellum.h"

// labelid are index in strings:1
#define LABEL_LIMIT 2
#define LABELID_MAIN_MENU 23
#define LABELID_QUIT 24

struct vellum_system {
  struct vellum hdr;
  struct label {
    int labelid;
    int x,y,w,h; // (x,y) relative to vellum
    int texid;
  } labelv[LABEL_LIMIT];
  int labelc;
  int labelp;
};

#define VELLUM ((struct vellum_system*)vellum)

/* Delete.
 */
 
static void _system_del(struct vellum *vellum) {
  struct label *label=VELLUM->labelv;
  int i=VELLUM->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
}

/* Add label.
 */
 
static struct label *system_add_label(struct vellum *vellum,int labelid) {
  if (VELLUM->labelc>=LABEL_LIMIT) return 0;
  struct label *label=VELLUM->labelv+VELLUM->labelc++;
  memset(label,0,sizeof(struct label));
  label->labelid=labelid;
  const char *src=0;
  int srcc=text_get_string(&src,1,labelid);
  if (!srcc) { src="?"; srcc=1; }
  label->texid=font_render_to_texture(0,g.font,src,srcc,FBW,font_get_line_height(g.font),0x000000ff);
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->x=(288>>1)-(label->w>>1);
  return label;
}

/* Center labels.
 */
 
static void system_center_labels(struct vellum *vellum) {
  if (VELLUM->labelc<1) return;
  const int innerh=153;
  const int spacing=2;
  int totalh=0;
  struct label *label=VELLUM->labelv;
  int i=VELLUM->labelc;
  for (;i-->0;label++) totalh+=label->h;
  totalh+=spacing*(VELLUM->labelc-1);
  int y=(innerh>>1)-(totalh>>1);
  for (label=VELLUM->labelv,i=VELLUM->labelc;i-->0;label++) {
    label->y=y;
    y+=label->h+spacing;
  }
}

/* Focus.
 */
 
static void _system_focus(struct vellum *vellum,int focus) {
}

/* Return to main menu.
 */
 
static void system_main_menu(struct vellum *vellum) {
  // Defuncting every modal will dismiss us, and cause main to spin up a new Hello.
  struct modal **p=g.modalv;
  int i=g.modalc;
  for (;i-->0;p++) {
    struct modal *modal=*p;
    modal->defunct=1;
  }
}

/* Activate selected label.
 */
 
static void system_activate(struct vellum *vellum) {
  if ((VELLUM->labelp<0)||(VELLUM->labelp>=VELLUM->labelc)) return;
  switch (VELLUM->labelv[VELLUM->labelp].labelid) {
    case LABELID_MAIN_MENU: system_main_menu(vellum); break;
    case LABELID_QUIT: egg_terminate(0); break;
  }
}

/* Move cursor.
 */
 
static void system_move(struct vellum *vellum,int d) {
  if (VELLUM->labelc<1) return;
  bm_sound(RID_sound_uimotion);
  VELLUM->labelp+=d;
  if (VELLUM->labelp<0) VELLUM->labelp=VELLUM->labelc-1;
  else if (VELLUM->labelp>=VELLUM->labelc) VELLUM->labelp=0;
}

/* Update animation only.
 */
 
static void _system_updatebg(struct vellum *vellum,double elapsed) {
}

/* Update.
 */
 
static void _system_update(struct vellum *vellum,double elapsed) {
  _system_updatebg(vellum,elapsed);
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) system_move(vellum,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) system_move(vellum,1);
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) system_activate(vellum);
}

/* Language change.
 */
 
static void _system_langchanged(struct vellum *vellum,int lang) {
}

/* Render.
 */
 
static void _system_render(struct vellum *vellum,int x,int y,int w,int h) {
  if ((VELLUM->labelp>=0)&&(VELLUM->labelp<VELLUM->labelc)) {
    struct label *label=VELLUM->labelv+VELLUM->labelp;
    graf_fill_rect(&g.graf,x+label->x-2,y+label->y-1,label->w+4,label->h+1,0x70a0f0ff);
  }
  struct label *label=VELLUM->labelv;
  int i=VELLUM->labelc;
  for (;i-->0;label++) {
    graf_set_input(&g.graf,label->texid);
    graf_decal(&g.graf,x+label->x,y+label->y,0,0,label->w,label->h);
  }
}

/* New.
 */
 
struct vellum *vellum_new_system(struct modal *parent) {
  struct vellum *vellum=calloc(1,sizeof(struct vellum_system));
  if (!vellum) return 0;
  vellum->parent=parent;
  vellum->lblstrix=16;
  vellum->del=_system_del;
  vellum->focus=_system_focus;
  vellum->updatebg=_system_updatebg;
  vellum->update=_system_update;
  vellum->render=_system_render;
  vellum->langchanged=_system_langchanged;
  
  system_add_label(vellum,LABELID_MAIN_MENU);
  system_add_label(vellum,LABELID_QUIT);
  system_center_labels(vellum);
  
  return vellum;
}

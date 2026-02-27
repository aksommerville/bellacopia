#include "game/bellacopia.h"
#include "vellum.h"

// labelid are index in strings:1
#define LABEL_LIMIT 6
#define LABELID_MAIN_MENU 23
#define LABELID_QUIT 24
#define LABELID_LANG 25
#define LABELID_INPUT 26
#define LABELID_MUSIC 27
#define LABELID_SOUND 28

struct vellum_system {
  struct vellum hdr;
  struct label {
    int labelid;
    int x,y,w,h; // (x,y) relative to vellum
    int texid;
  } labelv[LABEL_LIMIT];
  int labelc;
  int labelp;
  int *langv; // Populated lazily.
  int langc,langa;
};

#define VELLUM ((struct vellum_system*)vellum)

/* Delete.
 */
 
static void _system_del(struct vellum *vellum) {
  struct label *label=VELLUM->labelv;
  int i=VELLUM->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
  if (VELLUM->langv) free(VELLUM->langv);
}

/* Replace label text.
 */
 
static void system_rewrite_label(struct vellum *vellum,struct label *label) {
  
  char tmp[256];
  const char *src=0;
  int srcc=text_get_string(&src,1,label->labelid);
  if (!srcc) { src="?"; srcc=1; }
  switch (label->labelid) {
    case LABELID_LANG: {
        int lang=egg_prefs_get(EGG_PREF_LANG);
        char name[2];
        EGG_STRING_FROM_LANG(name,lang)
        if ((name[0]<'a')||(name[0]>'z')||(name[1]<'a')||(name[1]>'z')) name[0]=name[1]='?';
        if (srcc>sizeof(tmp)-4) srcc=sizeof(tmp)-4;
        memcpy(tmp,src,srcc);
        memcpy(tmp+srcc,": ",2);
        memcpy(tmp+srcc+2,name,2);
        src=tmp;
        srcc+=4;
      } break;
    case LABELID_MUSIC:
    case LABELID_SOUND: {
        int v=egg_prefs_get((label->labelid==LABELID_MUSIC)?EGG_PREF_MUSIC:EGG_PREF_SOUND);
        if (v<0) v=0; else if (v>99) v=99;
        if (srcc>sizeof(tmp)-4) srcc=sizeof(tmp)-4;
        memcpy(tmp,src,srcc);
        tmp[srcc++]=':';
        tmp[srcc++]=' ';
        if (v>=10) tmp[srcc++]='0'+v/10;
        tmp[srcc++]='0'+v%10;
        src=tmp;
      } break;
  }
  
  egg_texture_del(label->texid);
  label->texid=font_render_to_texture(0,g.font,src,srcc,FBW,font_get_line_height(g.font),0x000000ff);
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->x=(288>>1)-(label->w>>1);
}

/* Add label.
 */
 
static struct label *system_add_label(struct vellum *vellum,int labelid) {
  if (VELLUM->labelc>=LABEL_LIMIT) return 0;
  struct label *label=VELLUM->labelv+VELLUM->labelc++;
  memset(label,0,sizeof(struct label));
  label->labelid=labelid;
  system_rewrite_label(vellum,label);
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

/* Languages list.
 */
 
static int system_require_langv_cb(int lang,void *userdata) {
  struct vellum *vellum=userdata;
  if (VELLUM->langc>=VELLUM->langa) {
    int na=VELLUM->langa+8;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(VELLUM->langv,sizeof(int)*na);
    if (!nv) return -1;
    VELLUM->langv=nv;
    VELLUM->langa=na;
  }
  VELLUM->langv[VELLUM->langc++]=lang;
  return 0;
}
 
static int system_require_langv(struct vellum *vellum) {
  if (VELLUM->langc) return 0;
  if (text_for_each_language(system_require_langv_cb,vellum)<0) return -1;
  if (!VELLUM->langc) system_require_langv_cb(egg_prefs_get(EGG_PREF_LANG),vellum);
  return 0;
}

static int system_find_lang(struct vellum *vellum,int lang) {
  int i=VELLUM->langc;
  while (i-->0) if (VELLUM->langv[i]==lang) return i;
  return -1;
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
    case LABELID_INPUT: egg_input_configure(); break;
  }
}

/* Adjust value of selected label.
 */
 
static void system_adjust(struct vellum *vellum,int d) {
  if ((VELLUM->labelp<0)||(VELLUM->labelp>=VELLUM->labelc)) return;
  switch (VELLUM->labelv[VELLUM->labelp].labelid) {
  
    case LABELID_LANG: {
        bm_sound(RID_sound_uimotion);
        system_require_langv(vellum);
        if (VELLUM->langc<1) return;
        int p=system_find_lang(vellum,egg_prefs_get(EGG_PREF_LANG));
        p+=d;
        if (p<0) p=VELLUM->langc-1;
        else if (p>=VELLUM->langc) p=0;
        egg_prefs_set(EGG_PREF_LANG,VELLUM->langv[p]);
        system_rewrite_label(vellum,VELLUM->labelv+VELLUM->labelp);
      } break;
        
    case LABELID_MUSIC:
    case LABELID_SOUND: {
        int k=(VELLUM->labelv[VELLUM->labelp].labelid==LABELID_MUSIC)?EGG_PREF_MUSIC:EGG_PREF_SOUND;
        int v=egg_prefs_get(k);
        v+=d*10;
        if (v<0) v=0; else if (v>99) v=99;
        egg_prefs_set(k,v);
        system_rewrite_label(vellum,VELLUM->labelv+VELLUM->labelp);
        bm_sound(RID_sound_uimotion); // AFTER setting.
      } break;
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
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) system_adjust(vellum,-1);
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) system_adjust(vellum,1);
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) system_activate(vellum);
}

/* Language change.
 */
 
static void _system_langchanged(struct vellum *vellum,int lang) {
  struct label *label=VELLUM->labelv;
  int i=VELLUM->labelc;
  for (;i-->0;label++) system_rewrite_label(vellum,label);
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
  system_add_label(vellum,LABELID_LANG);
  system_add_label(vellum,LABELID_INPUT);
  system_add_label(vellum,LABELID_MUSIC);
  system_add_label(vellum,LABELID_SOUND);
  system_center_labels(vellum);
  
  return vellum;
}

/* modal_pause.c
 * We manage the vella: Sheets of paper each containing some kind of admin content.
 * Each vellum has its own controller, we're agnostic toward them.
 */

#include "game/bellacopia.h"
#include "vellum.h"

#define STAGE_ENTER 1
#define STAGE_RUN 2
#define STAGE_EXIT 3

#define RISE_UP_SPEED 5.0 /* hz */
#define RISE_DOWN_SPEED 5.0 /* hz */

struct modal_pause {
  struct modal hdr;
  struct vellum **vellumv;
  int vellumc,velluma;
  double rise; // 0..1, 1 when we're at the proper elevation.
  double drise; // -1,1 during hello and goodbye animation
  int bgtexid,bgw,bgh; // Image of the full vellum sheet.
};

#define MODAL ((struct modal_pause*)modal)

/* Store the position statically so it persists across pauses.
 * It won't persist across launches of course, and I don't think there's any need to.
 */
static int vellump=0;

/* Cleanup.
 */
 
static void _pause_del(struct modal *modal) {
  if (MODAL->vellumv) {
    while (MODAL->vellumc-->0) vellum_del(MODAL->vellumv[MODAL->vellumc]);
    free(MODAL->vellumv);
  }
  egg_texture_del(MODAL->bgtexid);
}

/* Draw the background texture.
 * It has a low flat top edge. We'll draw tabs directly on top. The tiles are designed such that that's ok.
 */
 
static void pause_draw_bg(struct modal *modal) {
  int colc=FBW/NS_sys_tilesize-1;
  int rowc=FBH/NS_sys_tilesize;
  int fullw=colc*NS_sys_tilesize;
  int fullh=rowc*NS_sys_tilesize;
  MODAL->bgtexid=egg_texture_new();
  if (egg_texture_load_raw(MODAL->bgtexid,fullw,fullh,fullw<<2,0,0)<0) return;
  egg_texture_clear(MODAL->bgtexid);
  graf_set_output(&g.graf,MODAL->bgtexid);
  graf_set_image(&g.graf,RID_image_pause);
  #define CELL(x,y) (x)*NS_sys_tilesize+(NS_sys_tilesize>>1),(y)*NS_sys_tilesize+(NS_sys_tilesize>>1)
  graf_tile(&g.graf,CELL(0,0),0x00,0);
  graf_tile(&g.graf,CELL(colc-1,0),0x02,0);
  graf_tile(&g.graf,CELL(0,rowc-1),0x20,0);
  graf_tile(&g.graf,CELL(colc-1,rowc-1),0x22,0);
  int col=colc-1; while (col-->1) {
    graf_tile(&g.graf,CELL(col,0),0x01,0);
    graf_tile(&g.graf,CELL(col,rowc-1),0x21,0);
    int row=rowc-1; while (row-->1) {
      graf_tile(&g.graf,CELL(col,row),0x11,0);
    }
  }
  int row=rowc-1; while (row-->1) {
    graf_tile(&g.graf,CELL(0,row),0x10,0);
    graf_tile(&g.graf,CELL(colc-1,row),0x12,0);
  }
  #undef CELL
  graf_set_output(&g.graf,1);
  MODAL->bgw=fullw;
  MODAL->bgh=fullh;
}

/* Redraw label for a vellum.
 */
 
static void pause_draw_label(struct modal *modal,struct vellum *vellum) {
  egg_texture_del(vellum->lbltexid);
  const char *src=0;
  int srcc=text_get_string(&src,1,vellum->lblstrix);
  vellum->lbltexid=font_render_to_texture(0,g.font,src,srcc,FBW,FBH,0x000000ff);
  egg_texture_get_size(&vellum->lblw,&vellum->lblh,vellum->lbltexid);
}

/* Add vellum.
 */
 
static struct vellum *pause_add_vellum(struct modal *modal,struct vellum *(*ctor)(struct modal *parent)) {
  if (MODAL->vellumc>=MODAL->velluma) {
    int na=MODAL->velluma+4;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(MODAL->vellumv,sizeof(void*)*na);
    if (!nv) return 0;
    MODAL->vellumv=nv;
    MODAL->velluma=na;
  }
  struct vellum *vellum=ctor(modal);
  if (!vellum) return 0;
  MODAL->vellumv[MODAL->vellumc++]=vellum;
  pause_draw_label(modal,vellum);
  return vellum;
}

/* Call focus on the focussed vellum.
 */
 
static void pause_focus_vellum(struct modal *modal,int focus) {
  if ((vellump<0)||(vellump>=MODAL->vellumc)) return;
  struct vellum *vellum=MODAL->vellumv[vellump];
  if (vellum->focus) vellum->focus(vellum,focus);
}

/* Init.
 */
 
static int _pause_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=0;
  modal->interactive=1;
  modal->blotter=1;
  MODAL->drise=1.0;
  
  pause_draw_bg(modal);
  
  if (!pause_add_vellum(modal,vellum_new_inventory)) return -1;
  if (!pause_add_vellum(modal,vellum_new_map)) return -1;
  if (!pause_add_vellum(modal,vellum_new_stories)) return -1;
  if (!pause_add_vellum(modal,vellum_new_stats)) return -1;
  if (!pause_add_vellum(modal,vellum_new_system)) return -1;
  
  pause_focus_vellum(modal,1);
  
  return 0;
}

/* Focus.
 */
 
static void _pause_focus(struct modal *modal,int focus) {
}

/* Notify.
 */
 
static void _pause_notify(struct modal *modal,int k,int v) {
  if (k==EGG_PREF_LANG) {
    struct vellum **p=MODAL->vellumv;
    int i=MODAL->vellumc;
    for (;i-->0;p++) {
      struct vellum *vellum=*p;
      if (vellum->langchanged) vellum->langchanged(vellum,v);
      pause_draw_label(modal,vellum);
    }
  }
}

/* Change page.
 */
 
static void pause_change_page(struct modal *modal,int d) {
  if (MODAL->vellumc<2) return;
  pause_focus_vellum(modal,0);
  vellump+=d;
  if (vellump<0) vellump=MODAL->vellumc-1;
  else if (vellump>=MODAL->vellumc) vellump=0;
  pause_focus_vellum(modal,1);
  bm_sound(RID_sound_uipage);
}

/* Update.
 */
 
static void _pause_update(struct modal *modal,double elapsed) {

  // Tick pausetime.
  double *pausetime=store_require_clock(NS_clock_pausetime);
  if (pausetime) (*pausetime)+=elapsed;

  // Rising?
  if (MODAL->drise<0.0) {
    if ((MODAL->rise-=RISE_DOWN_SPEED*elapsed)<=0.0) {
      modal->defunct=1;
      return;
    }
  } else if (MODAL->drise>0.0) {
    if ((MODAL->rise+=RISE_UP_SPEED*elapsed)>=1.0) {
      MODAL->drise=0.0;
      MODAL->rise=1.0;
    }
  }

  // AUX1 to exit.
  // Important to use input[1] and not input[0], since we might be in mouse mode.
  if ((g.input[1]&EGG_BTN_AUX1)&&!(g.pvinput[1]&EGG_BTN_AUX1)) {
    if (MODAL->drise>=0.0) {
      MODAL->drise=-1.0;
      pause_focus_vellum(modal,0);
      bm_sound(RID_sound_uicancel);
    }
  }
  
  // L1 and R1 to change pages. What the heck, let's allow L2 and R2 as well.
  if ((g.input[1]&EGG_BTN_L1)&&!(g.pvinput[1]&EGG_BTN_L1)) pause_change_page(modal,-1);
  if ((g.input[1]&EGG_BTN_R1)&&!(g.pvinput[1]&EGG_BTN_R1)) pause_change_page(modal,1);
  if ((g.input[1]&EGG_BTN_L2)&&!(g.pvinput[1]&EGG_BTN_L2)) pause_change_page(modal,-1);
  if ((g.input[1]&EGG_BTN_R2)&&!(g.pvinput[1]&EGG_BTN_R2)) pause_change_page(modal,1);
  
  // Update vella.
  int i=MODAL->vellumc;
  while (i-->0) {
    struct vellum *vellum=MODAL->vellumv[i];
    if (i==vellump) {
      if (vellum->update) vellum->update(vellum,elapsed);
    } else {
      if (vellum->updatebg) vellum->updatebg(vellum,elapsed);
    }
  }
}

/* Render.
 */
 
static void _pause_render(struct modal *modal) {

  int ya=FBH,yz=(FBH>>1)-(MODAL->bgh>>1);
  int bgy;
  if (MODAL->rise>=1.0) bgy=yz;
  else if (MODAL->rise<=0.0) bgy=ya;
  else bgy=(int)(ya*(1.0-MODAL->rise)+yz*MODAL->rise);
  
  int bgx=(FBW>>1)-(MODAL->bgw>>1);
  graf_set_input(&g.graf,MODAL->bgtexid);
  graf_decal(&g.graf,bgx,bgy,0,0,MODAL->bgw,MODAL->bgh);
  
  struct vellum **p=MODAL->vellumv;
  int i=0;
  int tabx=bgx+NS_sys_tilesize;
  int taby=bgy+(NS_sys_tilesize>>1);
  for (;i<MODAL->vellumc;i++,p++) {
    struct vellum *vellum=*p;
    uint8_t tabtileid=0x06;
    if (i==vellump) tabtileid=0x03;
    int tabcolc=(vellum->lblw+8+NS_sys_tilesize-1)/NS_sys_tilesize;
    if (tabcolc<2) tabcolc=2;
    int tx=tabx+(NS_sys_tilesize>>1);
    vellum->lblx=tabx;
    vellum->clickw=tabcolc*NS_sys_tilesize;
    graf_set_image(&g.graf,RID_image_pause);
    graf_tile(&g.graf,tx,taby,tabtileid,0);
    int ti=1; tx+=NS_sys_tilesize;
    for (;ti<tabcolc-1;ti++,tx+=NS_sys_tilesize) {
      graf_tile(&g.graf,tx,taby,tabtileid+1,0);
    }
    graf_tile(&g.graf,tx,taby,tabtileid+2,0);
    graf_set_input(&g.graf,vellum->lbltexid);
    graf_decal(&g.graf,tabx+4,taby-((i==vellump)?3:5),0,0,vellum->lblw,vellum->lblh);
    tabx+=tabcolc*NS_sys_tilesize;
  }
  
  if ((vellump>=0)&&(vellump<MODAL->vellumc)) {
    struct vellum *vellum=MODAL->vellumv[vellump];
    int innerw=288;
    int innerh=153;
    if (vellum->render) vellum->render(vellum,bgx+8,bgy+17,innerw,innerh);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_pause={
  .name="pause",
  .objlen=sizeof(struct modal_pause),
  .del=_pause_del,
  .init=_pause_init,
  .focus=_pause_focus,
  .notify=_pause_notify,
  .update=_pause_update,
  .render=_pause_render,
};

/* Mouse click, from child.
 */

void modal_pause_click_tabs(struct modal *modal,int x,int y) {
  if (!modal||(modal->type!=&modal_type_pause)) return;
  struct vellum **vellum=MODAL->vellumv;
  int i=0,np=-1;
  for (;i<MODAL->vellumc;i++,vellum++) {
    if (x<(*vellum)->lblx) continue;
    if (x>=(*vellum)->lblx+(*vellum)->clickw) continue;
    np=i;
    break;
  }
  if ((np<0)||(np>=MODAL->vellumc)) return;
  if (np==vellump) return;
  bm_sound(RID_sound_uipage);
  pause_focus_vellum(modal,0);
  vellump=np;
  pause_focus_vellum(modal,1);
}

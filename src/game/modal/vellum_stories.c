#include "game/bellacopia.h"
#include "vellum.h"

#define LABEL_LIMIT 4
#define LABELID_TITLE 1
#define LABELID_DESC 2
#define LABELID_SORRY 3

struct vellum_stories {
  struct vellum hdr;
  const struct story *storyv[16];
  int storyc;
  int storyp;
  struct label {
    int texid;
    int x,y,w,h; // (x,y) relative to vellum
    int labelid;
  } labelv[LABEL_LIMIT];
  int labelc;
  int label_positions_dirty; // If nonzero, we'll reposition at the next render (when bounds are known for sure).
};

#define VELLUM ((struct vellum_stories*)vellum)

/* Delete.
 */
 
static void _stories_del(struct vellum *vellum) {
  struct label *label=VELLUM->labelv;
  int i=VELLUM->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
}

/* Add or replace label.
 */
 
static struct label *stories_set_label(struct vellum *vellum,int labelid,int strix) {
  struct label *label=0;
  struct label *q=VELLUM->labelv;
  int i=VELLUM->labelc;
  for (;i-->0;q++) {
    if (q->labelid==labelid) {
      label=q;
      break;
    }
  }
  if (!label) {
    if (VELLUM->labelc>=LABEL_LIMIT) return 0;
    label=VELLUM->labelv+VELLUM->labelc++;
    memset(label,0,sizeof(struct label));
  }
  label->labelid=labelid;
  egg_texture_del(label->texid);
  const char *src=0;
  int srcc=text_get_string(&src,RID_strings_stories,strix);
  label->texid=font_render_to_texture(0,g.font,src,srcc,FBW-80,FBH-100,0x000000ff);
  egg_texture_get_size(&label->w,&label->h,label->texid);
  VELLUM->label_positions_dirty=1;
  return label;
}

static void stories_remove_label(struct vellum *vellum,int labelid) {
  int i=VELLUM->labelc;
  struct label *label=VELLUM->labelv+i-1;
  for (;i-->0;label--) {
    if (label->labelid!=labelid) continue;
    egg_texture_del(label->texid);
    VELLUM->labelc--;
    memmove(label,label+1,sizeof(struct label)*(VELLUM->labelc-i));
    VELLUM->label_positions_dirty=1;
    return;
  }
}

/* Refresh labels.
 */

static void stories_refresh_labels(struct vellum *vellum) {
  if ((VELLUM->storyp>=0)&&(VELLUM->storyp<VELLUM->storyc)) {
    const struct story *story=VELLUM->storyv[VELLUM->storyp];
    stories_set_label(vellum,LABELID_TITLE,story->strix_title);
    stories_set_label(vellum,LABELID_DESC,story->strix_desc);
    stories_remove_label(vellum,LABELID_SORRY);
  } else {
    stories_remove_label(vellum,LABELID_TITLE);
    stories_remove_label(vellum,LABELID_DESC);
    stories_set_label(vellum,LABELID_SORRY,33);
  }
}

/* Focus.
 */
 
static void _stories_focus(struct vellum *vellum,int focus) {
}

/* Activate selection.
 */
 
static void stories_activate(struct vellum *vellum) {
  if ((VELLUM->storyp<0)||(VELLUM->storyp>=VELLUM->storyc)) {
    bm_sound(RID_sound_reject);
    return;
  }
  const struct story *story=VELLUM->storyv[VELLUM->storyp];
  game_tell_story(story);
}

/* Move cursor.
 */
 
static void stories_move_storyp(struct vellum *vellum,int d) {
  if (VELLUM->storyc<1) return;
  VELLUM->storyp+=d;
  if (VELLUM->storyp<0) VELLUM->storyp=VELLUM->storyc-1;
  else if (VELLUM->storyp>=VELLUM->storyc) VELLUM->storyp=0;
  bm_sound(RID_sound_uimotion);
  stories_refresh_labels(vellum);
}
 
static void stories_move(struct vellum *vellum,int dx,int dy) {
  if (dx) stories_move_storyp(vellum,dx);
}

/* Update animation only.
 */
 
static void _stories_updatebg(struct vellum *vellum,double elapsed) {
}

/* Update.
 */
 
static void _stories_update(struct vellum *vellum,double elapsed) {
  _stories_updatebg(vellum,elapsed);
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) stories_move(vellum,-1,0);
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) stories_move(vellum,1,0);
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) stories_move(vellum,0,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) stories_move(vellum,0,1);
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) stories_activate(vellum);
}

/* Language change.
 */
 
static void _stories_langchanged(struct vellum *vellum,int lang) {
}

/* Render.
 */
 
static void _stories_render(struct vellum *vellum,int x,int y,int w,int h) {

  /* Row of books along the top.
   * The selected one is raised a few pixels.
   */
  graf_set_image(&g.graf,RID_image_stories);
  int i=0,tx=x+130,ty=y+28;
  for (;i<VELLUM->storyc;i++,tx+=4) {
    const struct story *story=VELLUM->storyv[i];
    int sy=ty;
    if (i==VELLUM->storyp) sy-=3;
    graf_tile(&g.graf,tx,sy,story->tileid_small,0);
  }
  
  /* Dot's finger under the selected story.
   */
  const struct story *selected=0;
  if ((VELLUM->storyp>=0)&&(VELLUM->storyp<VELLUM->storyc)) selected=VELLUM->storyv[VELLUM->storyp];
  if (selected) {
    graf_tile(&g.graf,x+130+VELLUM->storyp*4-5,ty+NS_sys_tilesize,0x01,0);
  }
  
  /* Cover of the selected story.
   */
  if (selected) {
    uint8_t ti=selected->tileid_large;
    graf_tile(&g.graf,x+90,y+20,ti+0x00,0);
    graf_tile(&g.graf,x+90+NS_sys_tilesize,y+20,ti+0x01,0);
    graf_tile(&g.graf,x+90,y+20+NS_sys_tilesize,ti+0x10,0);
    graf_tile(&g.graf,x+90+NS_sys_tilesize,y+20+NS_sys_tilesize,ti+0x11,0);
    
    // "New" sunburst if not yet told.
    if (!store_get_fld(selected->fld_told)) {
      graf_tile(&g.graf,x+87,y+15,0x03,0);
      graf_tile(&g.graf,x+87-NS_sys_tilesize,y+15,0x02,0);
    }
  }
  
  /* Replace label positions if dirty.
   */
  if (VELLUM->label_positions_dirty) {
    VELLUM->label_positions_dirty=0;
    struct label *label=VELLUM->labelv;
    for (i=VELLUM->labelc;i-->0;label++) {
      switch (label->labelid) {
        case LABELID_TITLE: label->x=20; label->y=70; break;
        case LABELID_DESC: label->x=20; label->y=90; break;
        case LABELID_SORRY: label->x=(w>>1)-(label->w>>1); label->y=(h>>1)-(label->h>>1); break;
      }
    }
  }
  
  /* Labels.
   */
  struct label *label=VELLUM->labelv;
  for (i=VELLUM->labelc;i-->0;label++) {
    graf_set_input(&g.graf,label->texid);
    graf_decal(&g.graf,x+label->x,y+label->y,0,0,label->w,label->h);
  }
}

/* New.
 */
 
struct vellum *vellum_new_stories(struct modal *parent) {
  struct vellum *vellum=calloc(1,sizeof(struct vellum_stories));
  if (!vellum) return 0;
  vellum->parent=parent;
  vellum->lblstrix=25;
  vellum->del=_stories_del;
  vellum->focus=_stories_focus;
  vellum->updatebg=_stories_updatebg;
  vellum->update=_stories_update;
  vellum->render=_stories_render;
  vellum->langchanged=_stories_langchanged;
  
  /* Acquire the stories.
   */
  int p=0;
  for (;;p++) {
    const struct story *story=story_by_index_present(p);
    if (!story) break;
    VELLUM->storyv[VELLUM->storyc++]=story;
    if (VELLUM->storyc>=16) break;
  }
  stories_refresh_labels(vellum);
  
  return vellum;
}

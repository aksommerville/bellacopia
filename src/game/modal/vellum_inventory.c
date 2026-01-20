#include "game/bellacopia.h"
#include "vellum.h"

/* Backpack geometry.
 * BPCOLC,BPROWC are negotiable but their product must be (INVSTORE_SIZE-1) ie 25.
 * (sooooo actually not negotiable. but you could change INVSTORE_SIZE if you really want to).
 */
#define BPCOLC 5
#define BPROWC 5
#define BPSPACING 10

#define LABEL_LIMIT 3
#define LBLID_EQUIPPED_NAME 1
#define LBLID_FOCUS_NAME 2
#define LBLID_FOCUS_DESC 3

struct vellum_inventory {
  struct vellum hdr;
  double canimclock;
  int canimframe;
  struct label {
    int texid;
    int x,y,w,h; // (x,y) relative to our root.
    int lblid;
  } labelv[LABEL_LIMIT];
  int labelc;
};

/* We'll cheat a little and store the backpack cursor position statically.
 * That means it will persist across pauses, and even session restarts, but not across program termination.
 * If there were any chance at all of two vellum_inventories existing concurrently, this would be a problem. (there's not).
 */
static int invcolp=BPCOLC>>1;
static int invrowp=BPROWC>>1;

#define VELLUM ((struct vellum_inventory*)vellum)

/* Delete.
 */
 
static void _inventory_del(struct vellum *vellum) {
  struct label *label=VELLUM->labelv;
  int i=VELLUM->labelc;
  for (;i-->0;label++) {
    egg_texture_del(label->texid);
  }
}

/* Look up text.
 */
 
static int inventory_strix_for_lblid(int lblid) {
  switch (lblid) {
    case LBLID_EQUIPPED_NAME: {
        const struct item_detail *detail=item_detail_for_itemid(g.store.invstorev[0].itemid);
        if (!detail) return 0;
        return detail->strix_name;
      }
    case LBLID_FOCUS_NAME: {
        if ((invcolp<0)||(invrowp<0)||(invcolp>=BPCOLC)||(invrowp>=BPROWC)) return 0;
        const struct invstore *invstore=g.store.invstorev+1+invrowp*BPCOLC+invcolp;
        const struct item_detail *detail=item_detail_for_itemid(invstore->itemid);
        if (!detail) return 0;
        return detail->strix_name;
      }
    case LBLID_FOCUS_DESC: {
        if ((invcolp<0)||(invrowp<0)||(invcolp>=BPCOLC)||(invrowp>=BPROWC)) return 0;
        const struct invstore *invstore=g.store.invstorev+1+invrowp*BPCOLC+invcolp;
        const struct item_detail *detail=item_detail_for_itemid(invstore->itemid);
        if (!detail) return 0;
        return detail->strix_help;
      }
  }
  return 0;
}

/* Labels.
 */
 
static struct label *inventory_add_label(struct vellum *vellum,int lblid) {
  if (VELLUM->labelc>=LABEL_LIMIT) return 0;
  struct label *label=VELLUM->labelv+VELLUM->labelc++;
  memset(label,0,sizeof(struct label));
  label->lblid=lblid;
  return label;
}

static void inventory_rebuild_labels(struct vellum *vellum) {
  struct label *label=VELLUM->labelv;
  int i=VELLUM->labelc;
  for (;i-->0;label++) {
    int strix=inventory_strix_for_lblid(label->lblid);
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_item,strix);
    egg_texture_del(label->texid);
    label->texid=font_render_to_texture(0,g.font,src,srcc,120,FBH,0x000000ff);
    egg_texture_get_size(&label->w,&label->h,label->texid);
  }
}

/* Focus.
 */
 
static void _inventory_focus(struct vellum *vellum,int focus) {
  switch (focus) {
    case VELLUM_FOCUS_BEFORE: break;
    case VELLUM_FOCUS_READY: break;
    case VELLUM_FOCUS_END: break;
  }
}

/* Update animation only.
 */
 
static void _inventory_updatebg(struct vellum *vellum,double elapsed) {
  if ((VELLUM->canimclock-=elapsed)<=0.0) {
    VELLUM->canimclock+=0.150;
    if (++(VELLUM->canimframe)>=4) VELLUM->canimframe=0;
  }
}

/* Move cursor.
 */
 
static void inventory_move(struct vellum *vellum,int dx,int dy) {
  bm_sound(RID_sound_uimotion);
  invcolp+=dx; if (invcolp<0) invcolp=BPCOLC-1; else if (invcolp>=BPCOLC) invcolp=0;
  invrowp+=dy; if (invrowp<0) invrowp=BPROWC-1; else if (invrowp>=BPROWC) invrowp=0;
  inventory_rebuild_labels(vellum);
  //TODO The cursor ought to also participate in modal_pause's paging. How should that work?
}

/* Swap equipped item with the focussed cell.
 */
 
static void inventory_swap(struct vellum *vellum) {
  if ((invcolp<0)||(invrowp<0)||(invcolp>=BPCOLC)||(invrowp>=BPROWC)) return;
  struct invstore *a=g.store.invstorev;
  struct invstore *b=g.store.invstorev+1+invrowp*BPCOLC+invcolp;
  struct invstore tmp=*a;
  *a=*b;
  *b=tmp;
  g.store.dirty=1;
  bm_sound(RID_sound_uiactivate);
  inventory_rebuild_labels(vellum);
}

/* Update.
 */
 
static void _inventory_update(struct vellum *vellum,double elapsed) {
  _inventory_updatebg(vellum,elapsed);
  
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) inventory_move(vellum,-1,0);
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) inventory_move(vellum,1,0);
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) inventory_move(vellum,0,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) inventory_move(vellum,0,1);
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) inventory_swap(vellum);
}

/* Language change.
 */
 
static void _inventory_langchanged(struct vellum *vellum,int lang) {
  inventory_rebuild_labels(vellum);
}

/* Render quantity for an item icon at (x,y).
 */
 
static void inventory_render_quantity(struct vellum *vellum,int x,int y,int c,int limit) {
  graf_set_image(&g.graf,RID_image_pause);
  uint32_t rgba;
  if (c<=0) { rgba=0xb0b0b0ff; c=0; }
  else if (c<limit) rgba=0xf08040ff;
  else rgba=0x00ff00ff;
  y+=NS_sys_tilesize>>1; // Ones digit centers on the icon's SE corner.
  x+=NS_sys_tilesize>>1;
  for (;;) {
    uint8_t tileid=0x40+c%10;
    c/=10;
    graf_fancy(&g.graf,x,y,tileid,0,0,NS_sys_tilesize,0,rgba);
    if (!c) break;
    x-=4;
  }
}

/* Render backpack.
 */
 
static void inventory_render_backpack(struct vellum *vellum,int outerx,int outery,int outerw,int outerh) {
  graf_fill_rect(&g.graf,outerx,outery,outerw,outerh,0x00000020);
  graf_set_image(&g.graf,RID_image_pause);
  
  // Cursor.
  if ((invcolp>=0)&&(invcolp<BPCOLC)&&(invrowp>=0)&&(invrowp<BPROWC)) {
    // Tile's natural orientation is for the SW corner.
    uint8_t tileid=0x18+VELLUM->canimframe;
    int cx=outerx+BPSPACING+invcolp*(NS_sys_tilesize+BPSPACING); // Center of NW quadrant.
    int cy=outery+BPSPACING+invrowp*(NS_sys_tilesize+BPSPACING);
    graf_tile(&g.graf,cx,cy,tileid,EGG_XFORM_SWAP|EGG_XFORM_YREV);
    graf_tile(&g.graf,cx+NS_sys_tilesize,cy,tileid,EGG_XFORM_XREV|EGG_XFORM_YREV);
    graf_tile(&g.graf,cx,cy+NS_sys_tilesize,tileid,0);
    graf_tile(&g.graf,cx+NS_sys_tilesize,cy+NS_sys_tilesize,tileid,EGG_XFORM_SWAP|EGG_XFORM_XREV);
  }
  
  // Icons.
  const struct invstore *invstore=g.store.invstorev+1; // Skip the equipped item.
  int dsty=outery+BPSPACING+(NS_sys_tilesize>>1);
  int yi=BPROWC;
  for (;yi-->0;dsty+=NS_sys_tilesize+BPSPACING) {
    int dstx=outerx+BPSPACING+(NS_sys_tilesize>>1);
    int xi=BPCOLC;
    for (;xi-->0;dstx+=NS_sys_tilesize+BPSPACING,invstore++) {
      if (!invstore->itemid) continue;
      const struct item_detail *detail=item_detail_for_itemid(invstore->itemid);
      if (!detail) continue;
      graf_tile(&g.graf,dstx,dsty,detail->tileid,0);
    }
  }
  
  // Quantities.
  for (dsty=outery+BPSPACING+(NS_sys_tilesize>>1),invstore=g.store.invstorev+1,yi=BPROWC;yi-->0;dsty+=NS_sys_tilesize+BPSPACING) {
    int dstx=outerx+BPSPACING+(NS_sys_tilesize>>1);
    int xi=BPCOLC;
    for (;xi-->0;dstx+=NS_sys_tilesize+BPSPACING,invstore++) {
      if (!invstore->itemid) continue;
      if (!invstore->limit) continue;
      inventory_render_quantity(vellum,dstx,dsty,invstore->quantity,invstore->limit);
    }
  }
}

/* Big cartoon hand with the equipped item on top.
 */
 
static void inventory_render_hand(struct vellum *vellum,int x,int y) {
  graf_set_image(&g.graf,RID_image_pause);
  graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y+(NS_sys_tilesize>>1),0x13,0);
  graf_tile(&g.graf,x+(NS_sys_tilesize>>1)+NS_sys_tilesize,y+(NS_sys_tilesize>>1),0x14,0);
  graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y+(NS_sys_tilesize>>1)+NS_sys_tilesize,0x23,0);
  graf_tile(&g.graf,x+(NS_sys_tilesize>>1)+NS_sys_tilesize,y+(NS_sys_tilesize>>1)+NS_sys_tilesize,0x24,0);
  const struct invstore *invstore=g.store.invstorev;
  if (invstore->itemid) {
    const struct item_detail *detail=item_detail_for_itemid(invstore->itemid);
    if (detail) {
      graf_tile(&g.graf,x+NS_sys_tilesize,y+NS_sys_tilesize,detail->tileid,0);
      if (invstore->limit) {
        inventory_render_quantity(vellum,x+NS_sys_tilesize,y+NS_sys_tilesize,invstore->quantity,invstore->limit);
      }
    }
  }
}

/* Render.
 */
 
static void _inventory_render(struct vellum *vellum,int x,int y,int w,int h) {
  
  /* 5x5 grid of the backpack, vertically centered toward the left.
   */
  const int bpw=BPCOLC*NS_sys_tilesize+(BPCOLC+1)*BPSPACING;
  const int bph=BPROWC*NS_sys_tilesize+(BPROWC+1)*BPSPACING;
  int bpy=(h>>1)-(bph>>1);
  int bpx=x+bpy;
  bpy+=y;
  inventory_render_backpack(vellum,bpx,bpy,bpw,bph);
  
  /* One more inventory slot showing the equipped item.
   * Right of the backpack and alignedish to its top.
   */
  inventory_render_hand(vellum,bpx+bpw+10,bpy);
  
  /* Name and description of focussed item, in the lower right.
   */
  struct label *label=VELLUM->labelv;
  int i=VELLUM->labelc;
  for (;i-->0;label++) {
    graf_set_input(&g.graf,label->texid);
    graf_decal(&g.graf,x+label->x,y+label->y,0,0,label->w,label->h);
  }
  
  //TODO If there's space, we could also show the non-inventory things. Gold, fish...
}

/* New.
 */
 
struct vellum *vellum_new_inventory() {
  struct vellum *vellum=calloc(1,sizeof(struct vellum_inventory));
  if (!vellum) return 0;
  vellum->lblstrix=13;
  vellum->del=_inventory_del;
  vellum->focus=_inventory_focus;
  vellum->updatebg=_inventory_updatebg;
  vellum->update=_inventory_update;
  vellum->render=_inventory_render;
  vellum->langchanged=_inventory_langchanged;
  
  struct label *label;
  if (label=inventory_add_label(vellum,LBLID_EQUIPPED_NAME)) {
    label->x=196;
    label->y=18;
  }
  if (label=inventory_add_label(vellum,LBLID_FOCUS_NAME)) {
    label->x=150;
    label->y=102;
  }
  if (label=inventory_add_label(vellum,LBLID_FOCUS_DESC)) {
    label->x=150;
    label->y=117;
  }
  inventory_rebuild_labels(vellum);
  
  return vellum;
}

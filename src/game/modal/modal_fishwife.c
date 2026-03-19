#include "game/bellacopia.h"

#define ROW_LIMIT 3
#define ROWH 12
#define LIMIT_LIMIT 10
#define FISH_X_SPACING 8

struct modal_fishwife {
  struct modal hdr;
  int boxx,boxy,boxw,boxh;
  int promptx,prompty,promptw,prompth;
  int prompttexid;
  
  struct row {
    uint8_t tileid; // RID_image_pause
    int q,limit;
    int price;
    int itemid;
    int fld16;
  } rowv[ROW_LIMIT];
  int rowc;
  int rowp;
  
  int total;
};

#define MODAL ((struct modal_fishwife*)modal)

/* Cleanup.
 */
 
static void _fishwife_del(struct modal *modal) {
  egg_texture_del(MODAL->prompttexid);
}

/* Generate prompt texture.
 */
 
static void fishwife_generate_prompt(struct modal *modal,int rid,int strix) {
  const char *src=0;
  int srcc=text_get_string(&src,rid,strix);
  if (MODAL->prompttexid) egg_texture_del(MODAL->prompttexid);
  MODAL->prompttexid=font_render_to_texture(0,g.font,src,srcc,FBW>>1,FBH>>1,0xffffffff);
  egg_texture_get_size(&MODAL->promptw,&MODAL->prompth,MODAL->prompttexid);
}

/* Add a fish row if we have at least one.
 */
 
static void fishwife_add_row_if_present(struct modal *modal,int itemid,int price) {
  if (MODAL->rowc>=ROW_LIMIT) return;
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail) return;
  int q=store_get_fld16(detail->fld16);
  if (!q) return;
  if (q>LIMIT_LIMIT) q=LIMIT_LIMIT;
  struct row *row=MODAL->rowv+MODAL->rowc++;
  row->tileid=detail->tileid;
  row->q=q;
  row->limit=q;
  row->price=price;
  row->itemid=itemid;
  row->fld16=detail->fld16;
}

/* Recalculate total.
 */
 
static void fishwife_recalc(struct modal *modal) {
  MODAL->total=0;
  struct row *row=MODAL->rowv;
  int i=MODAL->rowc;
  for (;i-->0;row++) MODAL->total+=row->q*row->price;
  // Clamp at 9999, mostly for display purposes. It will never be that high.
  // Don't clamp at the purse size -- that's the player's problem.
  if (MODAL->total<0) MODAL->total=0;
  if (MODAL->total>9999) MODAL->total=9999;
}

/* Init.
 */
 
static int _fishwife_init(struct modal *modal,const void *args,int argslen) {
  modal->opaque=0;
  modal->interactive=1;
  modal->blotter=1;
  
  if (args&&(argslen==sizeof(struct modal_args_fishwife))) {
    const struct modal_args_fishwife *ARGS=args;
    fishwife_generate_prompt(modal,ARGS->rid,ARGS->strix);
  }
  
  /* Create rows only for those colors of fish that we have.
   */
  fishwife_add_row_if_present(modal,NS_itemid_greenfish,1);
  fishwife_add_row_if_present(modal,NS_itemid_bluefish,5);
  fishwife_add_row_if_present(modal,NS_itemid_redfish,20);
  
  /* And the final layout.
   */
  MODAL->boxw=MODAL->promptw+8;
  int minw=32+LIMIT_LIMIT*FISH_X_SPACING;
  if (MODAL->boxw<minw) MODAL->boxw=minw;
  MODAL->boxh=4+MODAL->prompth+ROWH*MODAL->rowc+15;
  MODAL->boxx=(FBW>>1)-(MODAL->boxw>>1);
  MODAL->boxy=(FBH>>1)-(MODAL->boxh>>1);
  MODAL->promptx=(FBW>>1)-(MODAL->promptw>>1);
  MODAL->prompty=MODAL->boxy+4;
  
  fishwife_recalc(modal);
  
  return 0;
}

/* Notify.
 */
 
static void _fishwife_notify(struct modal *modal,int k,int v) {
}

/* Dismiss.
 */
 
static void fishwife_dismiss(struct modal *modal) {
  modal->defunct=1;
  bm_sound(RID_sound_uicancel);
}

/* Commit.
 */
 
static void fishwife_commit(struct modal *modal) {
  fishwife_recalc(modal); // Just to be sure.
  if (MODAL->total) {
    struct row *row=MODAL->rowv;
    int i=MODAL->rowc;
    for (;i-->0;row++) {
      if (!row->q) continue;
      int q=store_get_fld16(row->fld16);
      q-=row->q;
      if (q<0) q=0;
      store_set_fld16(row->fld16,q);
    }
    game_get_item(NS_itemid_gold,MODAL->total);
  }
  modal->defunct=1;
}

/* Move cursor.
 */
 
static void fishwife_move(struct modal *modal,int d) {
  if (MODAL->rowc<1) return;
  bm_sound(RID_sound_uimotion);
  MODAL->rowp+=d;
  if (MODAL->rowp<0) MODAL->rowp=MODAL->rowc-1;
  else if (MODAL->rowp>=MODAL->rowc) MODAL->rowp=0;
}

/* Adjust focussed value.
 */
 
static void fishwife_adjust(struct modal *modal,int d) {
  if ((MODAL->rowp<0)||(MODAL->rowp>=MODAL->rowc)) return;
  struct row *row=MODAL->rowv+MODAL->rowp;
  row->q+=d;
  if (row->q<0) {
    row->q=0;
    bm_sound(RID_sound_reject);
    return;
  } else if (row->q>row->limit) {
    row->q=row->limit;
    bm_sound(RID_sound_reject);
    return;
  }
  bm_sound(RID_sound_uimotion);
  fishwife_recalc(modal);
}

/* Update.
 */
 
static void _fishwife_update(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    fishwife_dismiss(modal);
    return;
  }
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    fishwife_commit(modal);
    return;
  }
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) fishwife_move(modal,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) fishwife_move(modal,1);
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) fishwife_adjust(modal,-1);
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) fishwife_adjust(modal,1);
}

/* Render.
 */
 
static void _fishwife_render(struct modal *modal) {

  // Box and prompt.
  graf_fill_rect(&g.graf,MODAL->boxx,MODAL->boxy,MODAL->boxw,MODAL->boxh,0x000000ff);
  graf_set_input(&g.graf,MODAL->prompttexid);
  graf_decal(&g.graf,MODAL->promptx,MODAL->prompty,0,0,MODAL->promptw,MODAL->prompth);
  graf_set_image(&g.graf,RID_image_pause);
  
  // Row highlight.
  int y=MODAL->prompty+MODAL->prompth;
  if ((MODAL->rowp>=0)&&(MODAL->rowp<MODAL->rowc)) {
    graf_tile(&g.graf,MODAL->boxx+9,y+MODAL->rowp*ROWH+8,0x25,0);
  }
  
  // Rows content.
  y+=8;
  struct row *row=MODAL->rowv;
  int i=MODAL->rowc;
  for (;i-->0;row++,y+=ROWH) {
    int x=MODAL->boxx+24;
    int xi=row->q;
    for (;xi-->0;x+=FISH_X_SPACING) {
      graf_tile(&g.graf,x,y,row->tileid,0);
    }
  }
  
  // Bottom line.
  // Current constants (limit 10; prices 1,5,20) yield a max of 260. We allow up to 9999.
  graf_set_image(&g.graf,RID_image_fonttiles);
  int x=MODAL->boxx+13;
  graf_tile(&g.graf,x,y,'=',0); x+=8;
  if (MODAL->total>=1000) { graf_tile(&g.graf,x,y,'0'+(MODAL->total/1000)%10,0); x+=8; }
  if (MODAL->total>=100) { graf_tile(&g.graf,x,y,'0'+(MODAL->total/100)%10,0); x+=8; }
  if (MODAL->total>=10) { graf_tile(&g.graf,x,y,'0'+(MODAL->total/10)%10,0); x+=8; }
  graf_tile(&g.graf,x,y,'0'+MODAL->total%10,0); x+=8;
}

/* Type definition.
 */
 
const struct modal_type modal_type_fishwife={
  .name="fishwife",
  .objlen=sizeof(struct modal_fishwife),
  .del=_fishwife_del,
  .init=_fishwife_init,
  .update=_fishwife_update,
  .render=_fishwife_render,
  .notify=_fishwife_notify,
};

#include "game/bellacopia.h"

#define ROW_LIMIT 4
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
    int q; // Negative if Dot's buying, positive if Dot's selling.
    int limit; // Positive only. How many can Dot sell?
    int buy_price;
    int sell_price;
    int itemid;
    int fld16;
    int selectable;
  } rowv[ROW_LIMIT];
  int rowc;
  int rowp;
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

/* Add a fish row.
 */
 
static void fishwife_add_row(struct modal *modal,int itemid,int buy_price,int sell_price) {
  if (MODAL->rowc>=ROW_LIMIT) return;
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail) return;
  int q=store_get_fld16(detail->fld16);
  if (q>LIMIT_LIMIT) q=LIMIT_LIMIT;
  struct row *row=MODAL->rowv+MODAL->rowc++;
  row->tileid=detail->tileid;
  row->q=0;
  row->limit=q;
  row->buy_price=buy_price;
  row->sell_price=sell_price;
  row->itemid=itemid;
  row->fld16=detail->fld16;
  row->selectable=(itemid!=NS_itemid_gold);
}

/* Recalculate total.
 */
 
static void fishwife_recalc(struct modal *modal) {
  if (MODAL->rowc<1) return;
  struct row *goldrow=MODAL->rowv;
  if (goldrow->itemid!=NS_itemid_gold) return; // oops?
  goldrow->q=0;
  struct row *row=MODAL->rowv+1;
  int i=MODAL->rowc-1;
  for (;i-->0;row++) {
    if (row->q<0) { // Dot's buying.
      goldrow->q+=row->buy_price*-row->q;
    } else if (row->q>0) { // Dot's selling.
      goldrow->q-=row->sell_price*row->q;
    }
  }
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
  
  /* Create a row for each fish color.
   */
  fishwife_add_row(modal,NS_itemid_gold,0,0);
  fishwife_add_row(modal,NS_itemid_greenfish,2,1);
  fishwife_add_row(modal,NS_itemid_bluefish,10,5);
  fishwife_add_row(modal,NS_itemid_redfish,30,20);
  MODAL->rowp=1;
  
  /* And the final layout.
   */
  MODAL->boxw=MODAL->promptw+8;
  int minw=32+LIMIT_LIMIT*FISH_X_SPACING;
  if (MODAL->boxw<minw) MODAL->boxw=minw;
  MODAL->boxh=4+MODAL->prompth+ROWH*MODAL->rowc+4;
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
  if (MODAL->rowc>=1) {
    /* First validate each row.
     * Negative quantities are being acquired by Dot, positive are being given away.
     * Both directions have relevant limits.
     */
    struct row *row=MODAL->rowv;
    int i=MODAL->rowc;
    for (;i-->0;row++) {
      if (row->q<0) { // Will acquiring it exceed my limit?
        int have,limit;
        have=possessed_quantity_for_itemid(row->itemid,&limit);
        if (have>limit+row->q) { // Can't carry this many. (this means you can't overflow your purse either, when selling)
          bm_sound(RID_sound_reject);
          return;
        }
      } else if (row->q>0) { // Do I actually have this many? Should have been enforced previously, but let's be sure.
        int have=possessed_quantity_for_itemid(row->itemid,0);
        if (row->q>have) { // Can't sell things we don't have, or buy things with money we don't have.
          bm_sound(RID_sound_reject);
          return;
        }
      }
    }
    /* Everything is legal, so commit it.
     */
    for (row=MODAL->rowv,i=MODAL->rowc;i-->0;row++) {
      if (row->q<0) {
        game_get_item(row->itemid,-row->q);
      } else if (row->q>0) {
        int have=store_get_fld16(row->fld16);
        have-=row->q;
        store_set_fld16(row->fld16,have);
      }
    }
  }
  modal->defunct=1;
}

/* Move cursor.
 */
 
static void fishwife_move(struct modal *modal,int d) {
  if (MODAL->rowc<1) return;
  bm_sound(RID_sound_uimotion);
  int panic=MODAL->rowc;
  while (panic-->0) {
    MODAL->rowp+=d;
    if (MODAL->rowp<0) MODAL->rowp=MODAL->rowc-1;
    else if (MODAL->rowp>=MODAL->rowc) MODAL->rowp=0;
    if (MODAL->rowv[MODAL->rowp].selectable) return;
  }
}

/* Adjust focussed value.
 */
 
static void fishwife_adjust(struct modal *modal,int d) {
  if ((MODAL->rowp<0)||(MODAL->rowp>=MODAL->rowc)) return;
  struct row *row=MODAL->rowv+MODAL->rowp;
  if (!row->selectable) return;
  row->q+=d;
  // You can't offer to sell fish you don't have. But the buy quantity is unlimited.
  // (we'll check that you can afford it at checkout, not here).
  // Well actually, let's at least clamp them to 2 digits.
  if (row->q<-99) {
    row->q=-99;
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
  
  // Row highlight.
  int y=MODAL->prompty+MODAL->prompth;
  if ((MODAL->rowp>=0)&&(MODAL->rowp<MODAL->rowc)) {
    graf_fill_rect(&g.graf,MODAL->boxx+20,y+MODAL->rowp*ROWH+4,MODAL->boxw-40,9,0x204060ff);
  }
  
  // Rows content.
  graf_set_image(&g.graf,RID_image_pause);
  y+=8;
  int need_text=0;
  int midx=MODAL->boxx+(MODAL->boxw>>1);
  struct row *row=MODAL->rowv;
  int i=MODAL->rowc;
  for (;i-->0;row++,y+=ROWH) {
    graf_tile(&g.graf,midx,y,row->tileid,0);
    if (row->q<0) { // Dot's buying this. Put a leftward arrow on the right.
      graf_tile(&g.graf,midx+16,y,0x1d,EGG_XFORM_XREV);
      need_text=1;
    } else if (row->q>0) { // Dot's selling this. Put a rightward arrow on the left.
      graf_tile(&g.graf,midx-16,y,0x1d,0);
      need_text=1;
    }
  }
  
  // Row text.
  if (need_text) {
    graf_set_image(&g.graf,RID_image_fonttiles);
    for (row=MODAL->rowv,i=MODAL->rowc,y=MODAL->prompty+MODAL->prompth+9;i-->0;row++,y+=ROWH) {
      int n=0,x=midx;
      if (row->q<0) { // Dot's buying this. Put the positive quantity on the left.
        n=-row->q;
        x-=18;
      } else if (row->q>0) { // Dot's selling this. Put the positive quantity on the right.
        n=row->q;
        x+=18;
      }
      if (n>=1000) {
        graf_tile(&g.graf,x+8,y,'-',0);
        graf_tile(&g.graf,x,y,'-',0);
        graf_tile(&g.graf,x-8,y,'-',0);
      } else if (n>=100) {
        graf_tile(&g.graf,x+8,y,'0'+n%10,0);
        graf_tile(&g.graf,x,y,'0'+(n/10)%10,0);
        graf_tile(&g.graf,x-8,y,'0'+n/100,0);
      } else if (n>=10) {
        graf_tile(&g.graf,x+4,y,'0'+n%10,0);
        graf_tile(&g.graf,x-4,y,'0'+n/10,0);
      } else if (n>0) {
        graf_tile(&g.graf,x,y,'0'+n,0);
      }
    }
  }
  
  // Dot and the Fishwife.
  int icony=MODAL->prompty+MODAL->prompth;
  icony+=(MODAL->boxy+MODAL->boxh-icony)>>1;
  graf_set_image(&g.graf,RID_image_hero);
  graf_tile(&g.graf,MODAL->boxx+10,icony,0x12,EGG_XFORM_XREV);
  graf_set_image(&g.graf,RID_image_meadow_sprites);
  graf_tile(&g.graf,MODAL->boxx+MODAL->boxw-10,icony,0x26,EGG_XFORM_XREV);
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

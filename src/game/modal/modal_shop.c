/* modal_shop.c
 * Basically the same thing as modal_dialogue, with some extras for buying things.
 */

#include "game/bellacopia.h"

#define STAGE_HELLO 1
#define STAGE_WAIT 2
#define STAGE_FAREWELL 3

#define MARGINL 4
#define MARGINR 4
#define MARGINT 2
#define MARGINB 1
#define CURSOR_MARGIN 16
#define SPEAKERSPACING 10 /* It's typically the center of a sprite, so at least 8 to clear the sprite. */
#define SUMMON_SPEED 5.000 /* hz */
#define DISMISS_SPEED 5.000 /* hz */

struct modal_shop {
  struct modal hdr;
  int stage;
  int x0,y0; // Single point that we voop to and from at summon and dismiss.
  int texid,texw,texh;
  int texx,texy; // Within (box).
  int boxx,boxy,boxw,boxh; // Final bounds when stable. Larger than (texw,texh).
  double presence; // 0=dismissed .. 1=present. Controls box position and blotter.
  struct option {
    int texid;
    int x,y,w,h;
    int itemid;
    int quantity; // Defaults to 1, or 0 if not applicable.
    int force_quantity; // If nonzero, it's a quantity-bearing item but we only sell so many, and (price) is for the set.
    int limit;
    int price;
    int desc; // texid
    int descw,desch;
  } *optionv;
  int optionc,optiona;
  int optionp;
  int boxdirty;
  int (*cb)(int itemid,int quantity,void *userdata);
  int (*cb_validated)(int itemid,int quantity,int price,void *userdata);
  void *userdata;
};

#define MODAL ((struct modal_shop*)modal)

/* Cleanup.
 */
 
static void option_cleanup(struct option *option) {
  egg_texture_del(option->texid);
  egg_texture_del(option->desc);
}
 
static void _shop_del(struct modal *modal) {
  egg_texture_del(MODAL->texid);
  if (MODAL->optionv) {
    while (MODAL->optionc-->0) option_cleanup(MODAL->optionv+MODAL->optionc);
    free(MODAL->optionv);
  }
}

/* Set (boxx,boxy,boxw,boxh,texx,texy) close to (x0,y0), respecting established bounds and options.
 */
 
static void shop_reposition(struct modal *modal) {
  if (!MODAL->optionc&&(MODAL->texw<1)) return;
  
  int extraw=32; // Quantity and price. They right-align, but item lines must account for the extra width.
  MODAL->boxw=MODAL->texw;
  MODAL->boxh=MODAL->texh+MARGINT+MARGINB;
  struct option *option=MODAL->optionv;
  int i=MODAL->optionc;
  for (;i-->0;option++) { // First, do we have any quantity-bearing options?
    if (option->quantity&&!option->force_quantity) {
      extraw+=32;
      break;
    }
  }
  for (option=MODAL->optionv,i=MODAL->optionc;i-->0;option++) {
    MODAL->boxh+=option->h;
    int w1=CURSOR_MARGIN+option->w+extraw;
    if (w1>MODAL->boxw) MODAL->boxw=w1;
  }
  MODAL->boxw+=MARGINL+MARGINR;
  
  /* Go a tasteful margin above the focus point, then clamp to the top of the framebuffer.
   * Unlike regular dialogue, we won't go below the focus. We have an auxiliary panel on the bottom of the screen.
   */
  MODAL->boxy=MODAL->y0-SPEAKERSPACING-MODAL->boxh;
  if (MODAL->boxy<0) MODAL->boxy=0;
  
  /* Center on speaker horizontally, then clamp to framebuffer.
   */
  MODAL->boxx=MODAL->x0-(MODAL->boxw>>1);
  if (MODAL->boxx<0) MODAL->boxx=0;
  else if (MODAL->boxx>FBW-MODAL->boxw) MODAL->boxx=FBW-MODAL->boxw;
  
  MODAL->texx=MODAL->boxx+MARGINL;
  MODAL->texy=MODAL->boxy+MARGINT;
  int ox=MODAL->boxx+MARGINL+CURSOR_MARGIN;
  int oy=MODAL->boxy+MARGINT+MODAL->texh;
  for (i=MODAL->optionc,option=MODAL->optionv;i-->0;option++) {
    option->y=oy;
    option->x=ox;
    oy+=option->h;
  }
}

/* Set text.
 */
 
static int shop_set_text(struct modal *modal,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (MODAL->texid) egg_texture_del(MODAL->texid);
  MODAL->texid=font_render_to_texture(0,g.font,src,srcc,FBW>>1,FBH>>1,0xffffffff);
  egg_texture_get_size(&MODAL->texw,&MODAL->texh,MODAL->texid);
  shop_reposition(modal);
  return 0;
}

/* Prepare origin from a framebuffer point.
 */
 
static void shop_origin_fb(struct modal *modal,int x,int y) {
  if (x<0) MODAL->x0=0;
  else if (x>=FBW) MODAL->x0=FBW;
  else MODAL->x0=x;
  if (y<0) MODAL->y0=0;
  else if (y>=FBH) MODAL->y0=FBH;
  else MODAL->y0=y;
  shop_reposition(modal);
}

/* Prepare origin from a sprite.
 */
 
static void shop_origin_sprite(struct modal *modal,const struct sprite *sprite) {
  if (!sprite) return;
  int x=(int)(sprite->x*NS_sys_tilesize)-g.camera.rx;
  int y=(int)(sprite->y*NS_sys_tilesize)-g.camera.ry;
  shop_origin_fb(modal,x,y);
}

/* Init.
 */
 
static int _shop_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=0;
  modal->interactive=1;
  
  MODAL->x0=FBW>>1;
  MODAL->y0=FBH>>1;
  MODAL->stage=STAGE_HELLO;
  
  if (arg&&(argc==sizeof(struct modal_args_shop))) {
    const struct modal_args_shop *args=arg;
    if (args->text) {
      if (shop_set_text(modal,args->text,args->textc)<0) return -1;
    } else if (args->rid&&args->strix) {
      const char *text=0;
      int textc=text_get_string(&text,args->rid,args->strix);
      if (textc>0) {
        if (args->insv&&args->insc) {
          char tmp[1024];
          int tmpc=text_format(tmp,sizeof(tmp),text,textc,args->insv,args->insc);
          if ((tmpc<0)||(tmpc>sizeof(tmp))) {
            fprintf(stderr,"WARNING: strings:%d:%d yielded %d bytes. (lang=%d)\n",args->rid,args->strix,tmpc,egg_prefs_get(EGG_PREF_LANG));
            tmpc=0;
          }
          if (shop_set_text(modal,tmp,tmpc)<0) return -1;
        } else {
          if (shop_set_text(modal,text,textc)<0) return -1;
        }
      }
    }
    if (args->speaker) {
      shop_origin_sprite(modal,args->speaker);
    } else if (args->speakerx||args->speakery) {
      shop_origin_fb(modal,args->speakerx,args->speakery);
    }
    MODAL->cb=args->cb;
    MODAL->cb_validated=args->cb_validated;
    MODAL->userdata=args->userdata;
  }
  
  return 0;
}

/* Dismiss.
 */
 
static void shop_dismiss(struct modal *modal) {
  if (MODAL->stage==STAGE_FAREWELL) return; // yeah yeah yeah, i'm on it
  if (MODAL->cb&&MODAL->cb(0,0,MODAL->userdata)) ; // ack'd, no sound
  else bm_sound(RID_sound_uicancel);
  MODAL->stage=STAGE_FAREWELL;
}

/* Try to purchase item.
 * This is only called if the custom callback fails to acknowledge.
 * Dismiss if accepted.
 */
 
static void shop_purchase(struct modal *modal,const struct option *option) {
  const struct item_detail *detail=item_detail_for_itemid(option->itemid);
  if (!detail) { // Shouldn't be possible; we check on the way in.
    shop_dismiss(modal);
    return;
  }
  
  // Reject if we are already full. If it exceeds the limit, reduce quantity.
  int quantity=option->quantity;
  if (quantity) {
    int limit=0;
    int havec=possessed_quantity_for_itemid(option->itemid,&limit);
    if (havec>=limit) {
      struct modal_args_dialogue args={.rid=RID_strings_dialogue,.strix=3};
      modal_spawn(&modal_type_dialogue,&args,sizeof(args));
      return;
    }
    if (havec>limit-quantity) quantity=limit-havec; // Clamp to limit.
  } else {
    if (possessed_quantity_for_itemid(option->itemid,0)) {
      struct modal_args_dialogue args={.rid=RID_strings_dialogue,.strix=3};
      modal_spawn(&modal_type_dialogue,&args,sizeof(args));
      return;
    }
  }
  
  // Adjust price per quantity and reject if too poor.
  // If we clamped, the price does adjust for custom quantities but does not for bundles. I think that's what one would expect.
  int price=option->price;
  if (quantity&&!option->force_quantity) {
    price*=quantity;
  }
  
  // Validate gold.
  int gold=store_get_fld16(NS_fld16_gold);
  if (gold<price) {
    struct modal_args_dialogue args={.rid=RID_strings_dialogue,.strix=2};
    modal_spawn(&modal_type_dialogue,&args,sizeof(args));
    return;
  }
  
  // Ready to purchase. Give the owner a chance to abort.
  if (MODAL->cb_validated&&MODAL->cb_validated(option->itemid,quantity,price,MODAL->userdata)) {
    return;
  }
  if (detail->inventoriable) {
    if (!store_add_itemid(option->itemid,quantity)) return;
  } else {
    fprintf(stderr,"%s:%d: Purchase non-inventory item %d\n",__FILE__,__LINE__,option->itemid);//TODO should have some helper here, like possessed_quantity_for_itemid()
  }
  store_set_fld16(NS_fld16_gold,gold-price);
  
  // Dismiss.
  bm_sound(RID_sound_uiactivate);
  MODAL->stage=STAGE_FAREWELL;
}

/* Activate.
 */
 
static void shop_activate(struct modal *modal) {
  if (MODAL->stage==STAGE_FAREWELL) return;
  if (MODAL->presence<1.0) return; // No buying until you've seen the labels!
  if ((MODAL->optionp>=0)&&(MODAL->optionp<MODAL->optionc)) {
    const struct option *option=MODAL->optionv+MODAL->optionp;
    if (MODAL->cb&&MODAL->cb(option->itemid,option->quantity,MODAL->userdata)) {
      MODAL->stage=STAGE_FAREWELL;
    } else {
      shop_purchase(modal,option);
    }
  } else {
    shop_dismiss(modal);
  }
}

/* Adjust quantity.
 */
 
static void shop_adjust(struct modal *modal,int d) {
  if (MODAL->stage!=STAGE_WAIT) return;
  if ((MODAL->optionp<0)||(MODAL->optionp>=MODAL->optionc)) return;
  struct option *option=MODAL->optionv+MODAL->optionp;
  if (!option->quantity||option->force_quantity) { // Not adjustable.
    bm_sound(RID_sound_reject);
    return;
  }
  int nq=option->quantity+d;
  if ((nq<1)||(nq>option->limit)) {
    bm_sound(RID_sound_reject);
    return;
  }
  option->quantity=nq;
  bm_sound(RID_sound_uimotion);
}

/* Move cursor.
 */
 
static void shop_move(struct modal *modal,int d) {
  if (MODAL->optionc<2) return;
  MODAL->optionp+=d;
  if (MODAL->optionp<0) MODAL->optionp=MODAL->optionc-1;
  else if (MODAL->optionp>=MODAL->optionc) MODAL->optionp=0;
  bm_sound(RID_sound_uimotion);
}

/* In background, proceed with dismissal.
 * eg a new dialogue modal was summoned just as we exit.
 */
 
static void _shop_updatebg(struct modal *modal,double elapsed) {
  if (MODAL->stage==STAGE_FAREWELL) {
    if ((MODAL->presence-=DISMISS_SPEED*elapsed)<=0.0) {
      MODAL->presence=0.0;
      modal->defunct=1;
    }
  }
}

/* Update.
 */
 
static void _shop_update(struct modal *modal,double elapsed) {

  switch (MODAL->stage) {
    case STAGE_HELLO: {
        if ((MODAL->presence+=SUMMON_SPEED*elapsed)>=1.0) {
          MODAL->presence=1.0;
          MODAL->stage=STAGE_WAIT;
        }
      } break;
    case STAGE_FAREWELL: {
        if ((MODAL->presence-=DISMISS_SPEED*elapsed)<=0.0) {
          MODAL->presence=0.0;
          modal->defunct=1;
          return;
        }
      } break;
  }
  
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) shop_adjust(modal,-1);
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) shop_adjust(modal,1);
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) shop_move(modal,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) shop_move(modal,1);
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) shop_activate(modal);
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) shop_dismiss(modal);
}

/* Unsigned decimal integer from 5x7-pixel tiles.
 * (x,y) is the center of the least-significant digit.
 */
 
static void shop_small_decuint(struct modal *modal,int x,int y,int v,uint32_t color) {
  if (v<0) v=0;
  graf_set_image(&g.graf,RID_image_pause);
  for (;;x-=4) {
    int digit=v%10;
    v/=10;
    graf_fancy(&g.graf,x,y,0x40+digit,0,0,NS_sys_tilesize,0,color);
    if (!v) break;
  }
}

/* Quantity and price. Caller should prepare image:fonttiles.
 */
 
static void shop_render_quantity(struct modal *modal,int y,int quantity) {
  if (quantity<0) quantity=0;
  int x=MODAL->boxx+MODAL->boxw-10-48;
  graf_tile(&g.graf,x,y,'x',0);
  int digitc=1,limit=10;
  while (quantity>=limit) { digitc++; if (limit>INT_MAX/10) break; limit*=10; }
  x+=8*digitc;
  for (;digitc-->0;x-=8,quantity/=10) graf_tile(&g.graf,x,y,0x30+quantity%10,0);
}

static void shop_render_price(struct modal *modal,int y,int price) {
  if (price<0) price=0;
  int x=MODAL->boxx+MODAL->boxw-10; // right align
  for (;;x-=8) {
    graf_tile(&g.graf,x,y,0x30+price%10,0);
    price/=10;
    if (!price) break;
  }
}

/* Render.
 */
 
static void _shop_render(struct modal *modal) {
  if (MODAL->boxdirty) {
    MODAL->boxdirty=0;
    shop_reposition(modal);
  }

  int alpha=(int)(MODAL->presence*128.0);
  if (alpha>0) {
    if (alpha>0xff) alpha=0xff;
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0x00000000|alpha);
  }
  
  // Dialogue box.
  int boxx,boxy,boxw,boxh;
  if (MODAL->presence<1.0) {
    double inv=1.0-MODAL->presence;
    boxx=(int)(MODAL->x0*inv+MODAL->boxx*MODAL->presence);
    boxy=(int)(MODAL->y0*inv+MODAL->boxy*MODAL->presence);
    boxw=(int)(MODAL->boxw*MODAL->presence);
    boxh=(int)(MODAL->boxh*MODAL->presence);
  } else {
    boxx=MODAL->boxx;
    boxy=MODAL->boxy;
    boxw=MODAL->boxw;
    boxh=MODAL->boxh;
  }
  graf_fill_rect(&g.graf,boxx,boxy,boxw,boxh,0x000000ff);
  
  // Auxiliary box.
  const int auxx=FBW/6;
  const int auxw=(FBW*2)/3;
  const int auxh=43;
  int auxy;
  if (MODAL->presence<1.0) {
    auxy=FBH-(int)(MODAL->presence*auxh);
  } else {
    auxy=FBH-auxh;
  }
  graf_fill_rect(&g.graf,auxx,auxy,auxw,auxh,0x270d47ff);
  
  // Dialogue box content. Blank while animating.
  if (MODAL->stage==STAGE_WAIT) {
    // Static prompt:
    graf_set_input(&g.graf,MODAL->texid);
    graf_decal(&g.graf,MODAL->texx,MODAL->texy,0,0,MODAL->texw,MODAL->texh);
    // Item names;
    struct option *option=MODAL->optionv;
    int i=MODAL->optionc;
    for (;i-->0;option++) {
      graf_set_input(&g.graf,option->texid);
      graf_decal(&g.graf,option->x,option->y,0,0,option->w,option->h);
    }
    // Another pass over the options to render quantity and price (tiles, from one image).
    graf_set_image(&g.graf,RID_image_fonttiles);
    for (option=MODAL->optionv,i=MODAL->optionc;i-->0;option++) {
      int y=option->y+(option->h>>1),price=option->price;
      if (option->quantity&&!option->force_quantity) { // If (force_quantity), it's baked into the name.
        shop_render_quantity(modal,y,option->quantity);
        price*=option->quantity;
      }
      shop_render_price(modal,y,price);
    }
    // Dot's hand:
    if ((MODAL->optionp>=0)&&(MODAL->optionp<MODAL->optionc)) {
      option=MODAL->optionv+MODAL->optionp;
      graf_set_image(&g.graf,RID_image_pause);
      graf_tile(&g.graf,option->x-10,option->y+(option->h>>1),0x25,0);
    }
  }
  
  // Auxiliary box content, always.
  graf_set_image(&g.graf,RID_image_pause);
  graf_tile(&g.graf,auxx+3+(NS_sys_tilesize>>1),auxy+3+(NS_sys_tilesize>>1),0x17,0); // Item blotter.
  graf_tile(&g.graf,auxx+auxw-7,auxy+6,0x29,0); // Cash on hand.
  graf_tile(&g.graf,auxx+auxw-7,auxy+15,0x2d,0); // Quantity on hand.
  if ((MODAL->optionp>=0)&&(MODAL->optionp<MODAL->optionc)) {
    const struct option *option=MODAL->optionv+MODAL->optionp;
    const struct item_detail *detail=item_detail_for_itemid(option->itemid);
    const struct invstore *invstore=store_get_itemid(option->itemid);
    if (detail) {
      graf_tile(&g.graf,auxx+3+(NS_sys_tilesize>>1),auxy+3+(NS_sys_tilesize>>1),detail->tileid,0); // Item icon.
    }
    const uint32_t maxcolor=0x00ff00ff;
    const uint32_t somecolor=0xf08040ff;
    const uint32_t nonecolor=0xb0b0b0ff;
    if (invstore) {
      if (invstore->limit==0) shop_small_decuint(modal,auxx+auxw-15,auxy+15,1,maxcolor); // Not quantity-bearing, so we have 1.
      else if (invstore->quantity>=invstore->limit) shop_small_decuint(modal,auxx+auxw-15,auxy+15,invstore->quantity,maxcolor);
      else if (invstore->quantity) shop_small_decuint(modal,auxx+auxw-15,auxy+15,invstore->quantity,somecolor);
      else shop_small_decuint(modal,auxx+auxw-15,auxy+15,invstore->quantity,nonecolor);
    } else {
      shop_small_decuint(modal,auxx+auxw-15,auxy+15,0,nonecolor);
    }
    int price=option->price;
    if (option->quantity&&!option->force_quantity) price*=option->quantity;
    int gold=store_get_fld16(NS_fld16_gold);
    if (gold>=price) shop_small_decuint(modal,auxx+auxw-15,auxy+6,gold,maxcolor);
    else shop_small_decuint(modal,auxx+auxw-15,auxy+6,gold,nonecolor);
    graf_set_input(&g.graf,option->texid);
    graf_decal(&g.graf,auxx+6+NS_sys_tilesize,auxy+7,0,0,option->w,option->h);
    graf_set_input(&g.graf,option->desc);
    graf_decal(&g.graf,auxx+3,auxy+6+NS_sys_tilesize,0,0,option->descw,option->desch);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_shop={
  .name="shop",
  .objlen=sizeof(struct modal_shop),
  .del=_shop_del,
  .init=_shop_init,
  .update=_shop_update,
  .updatebg=_shop_updatebg,
  .render=_shop_render,
  // Not doing notify, even though we definitely show text.
  // Trying to retrieve the original text would be complicated.
};

/* Add item.
 */
 
int modal_shop_add_item(struct modal *modal,int itemid,int price,int quantity) {
  if (!modal||(modal->type!=&modal_type_shop)) return -1;
  
  // Get some facts about the item.
  if ((itemid<1)||(itemid>0xff)) return -1;
  if (price<0) return -1;
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail) return -1;
  if (!detail->strix_name) return -1;
  const char *name=0;
  int namec=text_get_string(&name,RID_strings_item,detail->strix_name);
  
  /* If it's a quantity-bearing item, and caller gave an explicit quantity, bake that into the diplay name.
   * Not quantity-bearing, clear (quantity), it won't be used.
   */
  char tmp_quantity[256];
  if (detail->initial_limit&&quantity) {
    int tmpc=snprintf(tmp_quantity,sizeof(tmp_quantity),"%.*s x%d",namec,name,quantity);
    if ((tmpc<0)||(tmpc>=sizeof(tmp_quantity))) return -1;
    name=tmp_quantity;
    namec=tmpc;
  } else {
    quantity=0;
  }
  
  /* Add to my list.
   */
  if (MODAL->optionc>=MODAL->optiona) {
    int na=MODAL->optiona+8;
    if (na>INT_MAX/sizeof(struct option)) return -1;
    void *nv=realloc(MODAL->optionv,sizeof(struct option)*na);
    if (!nv) return -1;
    MODAL->optionv=nv;
    MODAL->optiona=na;
  }
  struct option *option=MODAL->optionv+MODAL->optionc++;
  memset(option,0,sizeof(struct option));
  option->itemid=itemid;
  option->price=price;
  if (quantity) {
    option->quantity=option->force_quantity=quantity;
  } else if (detail->initial_limit) {
    option->quantity=1;
    struct invstore *invstore=store_get_itemid(itemid);
    if (invstore) {
      option->limit=invstore->limit-invstore->quantity;
      if (option->limit<1) option->limit=1;
    } else {
      option->limit=detail->initial_limit;
    }
  }
  
  /* Generate textures.
   */
  option->texid=font_render_to_texture(0,g.font,name,namec,FBW,font_get_line_height(g.font),0xffffffff);
  egg_texture_get_size(&option->w,&option->h,option->texid);
  if (detail->strix_help) {
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_item,detail->strix_help);
    if (srcc>0) {
      option->desc=font_render_to_texture(0,g.font,src,srcc,(FBW*2)/3-6,font_get_line_height(g.font)*2,0xffffffff);
      egg_texture_get_size(&option->descw,&option->desch,option->desc);
    }
  }

  MODAL->boxdirty=1;
  return 0;
}

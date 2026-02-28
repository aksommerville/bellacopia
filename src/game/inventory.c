#include "bellacopia.h"

/* Items metadata.
 */
 
static const struct item_detail item_detailv[]={
  // Indexed by itemid. Slot zero will never be returned.
  [NS_itemid_stick]={
    .tileid=0x39,
    .hand_tileid=0x79,
    .strix_name=1,
    .strix_help=2,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_broom]={
    .tileid=0x30,
    .hand_tileid=0x70,
    .strix_name=3,
    .strix_help=4,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_divining]={
    .tileid=0x31,
    .hand_tileid=0x71,
    .strix_name=5,
    .strix_help=6,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_match]={
    .tileid=0x34,
    .hand_tileid=0x74,
    .strix_name=7,
    .strix_help=8,
    .initial_limit=99,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_wand]={
    .tileid=0x37,
    .hand_tileid=0x77,
    .strix_name=9,
    .strix_help=10,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_fishpole]={
    .tileid=0x33,
    .hand_tileid=0x73,
    .strix_name=11,
    .strix_help=12,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_bugspray]={
    .tileid=0x35,
    .hand_tileid=0x75,
    .strix_name=13,
    .strix_help=14,
    .initial_limit=10,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_potion]={
    .tileid=0x3c,
    .hand_tileid=0x7a,
    .strix_name=15,
    .strix_help=16,
    .initial_limit=3,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_hookshot]={
    .tileid=0x32,
    .hand_tileid=0x72,
    .strix_name=17,
    .strix_help=18,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_candy]={
    .tileid=0x36,
    .hand_tileid=0x76,
    .strix_name=19,
    .strix_help=20,
    .initial_limit=10,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_magnifier]={
    .tileid=0x38,
    .hand_tileid=0x78,
    .strix_name=21,
    .strix_help=22,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_vanishing]={
    .tileid=0x3b,
    .hand_tileid=0x7b,
    .strix_name=23,
    .strix_help=24,
    .initial_limit=10,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_compass]={
    .tileid=0x3a,
    .hand_tileid=0x7c,
    .strix_name=25,
    .strix_help=26,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_gold]={
    .tileid=0x29,
    .hand_tileid=0,
    .strix_name=27,
    .strix_help=0,
    .initial_limit=-NS_fld16_goldmax,
    .inventoriable=0,
    .fld16=NS_fld16_gold,
  },
  [NS_itemid_greenfish]={
    .tileid=0x2a,
    .hand_tileid=0,
    .strix_name=28,
    .strix_help=0,
    .initial_limit=999,
    .inventoriable=0,
    .fld16=NS_fld16_greenfish,
  },
  [NS_itemid_bluefish]={
    .tileid=0x2b,
    .hand_tileid=0,
    .strix_name=29,
    .strix_help=0,
    .initial_limit=999,
    .inventoriable=0,
    .fld16=NS_fld16_bluefish,
  },
  [NS_itemid_redfish]={
    .tileid=0x2c,
    .hand_tileid=0,
    .strix_name=30,
    .strix_help=0,
    .initial_limit=999,
    .inventoriable=0,
    .fld16=NS_fld16_redfish,
  },
  [NS_itemid_heart]={
    .tileid=0x28,
    .hand_tileid=0,
    .strix_name=31,
    .strix_help=0,
    .initial_limit=-NS_fld16_hpmax,
    .inventoriable=0,
    .fld16=NS_fld16_hp,
  },
  [NS_itemid_jigpiece]={
    .tileid=0x2e,
    .hand_tileid=0,
    .strix_name=32,
    .strix_help=33,
    .initial_limit=0,
    .inventoriable=0,
    .fld16=0,
  },
  [NS_itemid_bell]={
    .tileid=0x3d,
    .hand_tileid=0x7d,
    .strix_name=34,
    .strix_help=35,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_heartcontainer]={
    .tileid=0x2f,
    .hand_tileid=0,
    .strix_name=36,
    .strix_help=37,
    .initial_limit=12,
    .inventoriable=0,
    .fld16=NS_fld16_hpmax,
  },
  [NS_itemid_barrelhat]={
    .tileid=0x3e,
    .hand_tileid=0x8d,
    .strix_name=50,
    .strix_help=51,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_telescope]={
    .tileid=0x3f,
    .hand_tileid=0x8e,
    .strix_name=52,
    .strix_help=53,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_shovel]={
    .tileid=0x4d,
    .hand_tileid=0x8f,
    .strix_name=54,
    .strix_help=55,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_pepper]={
    .tileid=0x4e,
    .hand_tileid=0x93,
    .strix_name=56,
    .strix_help=57,
    .initial_limit=10,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_text]={
    .tileid=0,
    .hand_tileid=0,
    .strix_name=0,
    .strix_help=0,
    .initial_limit=0,
    .inventoriable=0,
    .fld16=0,
  },
  [NS_itemid_letter1]={
    .tileid=0x4f,
    .hand_tileid=0x94,
    .strix_name=58,
    .strix_help=62,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_letter2]={
    .tileid=0x5f,
    .hand_tileid=0x95,
    .strix_name=59,
    .strix_help=63,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_letter3]={
    .tileid=0x4f,
    .hand_tileid=0x94,
    .strix_name=60,
    .strix_help=64,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_letter4]={
    .tileid=0x5f,
    .hand_tileid=0x95,
    .strix_name=61,
    .strix_help=65,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_bomb]={
    .tileid=0x6f,
    .hand_tileid=0x96,
    .strix_name=67,
    .strix_help=68,
    .initial_limit=10,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_stopwatch]={
    .tileid=0x7f,
    .hand_tileid=0x97,
    .strix_name=69,
    .strix_help=70,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_busstop]={
    .tileid=0x8f,
    .hand_tileid=0x98,
    .strix_name=71,
    .strix_help=72,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_snowglobe]={
    .tileid=0x9f,
    .hand_tileid=0x99,
    .strix_name=73,
    .strix_help=74,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_tapemeasure]={
    .tileid=0x9e,
    .hand_tileid=0x9a,
    .strix_name=75,
    .strix_help=76,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_phonograph]={
    .tileid=0x9d,
    .hand_tileid=0x9b,
    .strix_name=77,
    .strix_help=78,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_crystal]={
    .tileid=0xaf,
    .hand_tileid=0x9c,
    .strix_name=79,
    .strix_help=80,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_glove]={
    .tileid=0xae,
    .hand_tileid=0x9d,
    .strix_name=81,
    .strix_help=82,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_marionette]={
    .tileid=0xad,
    .hand_tileid=0x9e,
    .strix_name=83,
    .strix_help=84,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
};

/* Get item reporting.
 */
 
// Caller must sprite_toast_set_text() after.
static struct sprite *game_start_toast() {
  if (sprite_toast_get_any()) return 0;
  double x,y;
  if (GRP(hero)->sprc>0) {
    struct sprite *hero=GRP(hero)->sprv[0];
    x=hero->x;
    y=hero->y-0.5;
  } else {
    x=(double)(g.camera.rx+(FBW>>1))/(double)NS_sys_tilesize;
    y=(double)(g.camera.ry+(FBH>>1))/(double)NS_sys_tilesize;
  }
  return sprite_spawn(x,y,0,0,0,&sprite_type_toast,0,0);
}
 
static void game_report_item_quantity_add(int itemid,int quantity) {
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail||!detail->strix_name) return;
  struct sprite *sprite=game_start_toast();
  if (!sprite) return;
  struct text_insertion insv[]={
    {.mode='i',.i=quantity},
    {.mode='r',.r={.rid=RID_strings_item,.strix=detail->strix_name}},
  };
  char text[64];
  int textc=text_format_res(text,sizeof(text),RID_strings_item,48,insv,sizeof(insv)/sizeof(insv[0]));
  if ((textc<0)||(textc>sizeof(text))) return;
  sprite_toast_set_text(sprite,text,textc);
}

static void game_report_item_full(int itemid) {
  // We call this both for counted slot at limit, and for new item but no slots available.
  // It's static text "Pack full!", but we could insert the item's name if we want in the future.
  struct sprite *sprite=game_start_toast();
  if (!sprite) return;
  const char *text=0;
  int textc=text_get_string(&text,RID_strings_item,49);
  sprite_toast_set_text(sprite,text,textc);
}
 
static void game_report_item_acquire(int itemid,int quantity) {
  // (quantity) will be zero for singletons.
  
  /* Most items will never leave your inventory.
   * Those that do, eg giving your compass to a toll troll, it's fine to repeat the fanfare when you get another.
   * But stick is different: It enters and leaves your inventory frequently.
   * So we dedicate a field to indicate whether stick has been got before, and only show this dialogue the first time.
   * After the first time, getting another stick will be passive like getting gold or hearts.
   */
  if (itemid==NS_itemid_stick) {
    if (store_get_fld(NS_fld_had_stick)) {
      game_report_item_quantity_add(NS_itemid_stick,1);
      return;
    }
    store_set_fld(NS_fld_had_stick,1);
  }
  
  // If the item doesn't exist or has no name, noop.
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail) return;
  if (!detail->strix_name) return;
  
  // Message begins "You found Thing!". No quantity even if relevant.
  char msg[256];
  struct text_insertion ins={.mode='r',.r={.rid=RID_strings_item,.strix=detail->strix_name}};
  int msgc=text_format_res(msg,sizeof(msg),RID_strings_item,47,&ins,1);
  if ((msgc<0)||(msgc>sizeof(msg))) msgc=0;
  
  // If help text is available, append a newline then that. It's static text.
  if (detail->strix_help) {
    const char *help=0;
    int helpc=text_get_string(&help,RID_strings_item,detail->strix_help);
    if ((helpc>0)&&(msgc+1+helpc<=sizeof(msg))) {
      msg[msgc++]='\n';
      memcpy(msg+msgc,help,helpc);
      msgc+=helpc;
    }
  }
  
  struct modal_args_dialogue args={
    .text=msg,
    .textc=msgc,
  };
  modal_spawn(&modal_type_dialogue,&args,sizeof(args));
}

/* Get item.
 */
 
int game_get_item(int itemid,int quantity) {
  if ((itemid<1)||(itemid>0xff)) return 0; // Valid itemid are 8 bits and not zero.
  
  /* If we already have it in inventory, either add quantity or do nothing.
   */
  struct invstore *invstore=store_get_itemid(itemid);
  if (invstore) {
    if (!invstore->limit) return 0; // Singleton item and we already have it. Try to avoid this situation in design.
    if (invstore->quantity>=invstore->limit) { // We're already full.
      game_report_item_full(itemid);
      return 0;
    }
    if (quantity<1) return 0; // Counted item but caller didn't supply a quantity.
    int nq=invstore->quantity+quantity;
    if (nq>invstore->limit) nq=invstore->limit; // Taking fewer than supplied due to inventory limit. Do report the attempted count tho.
    invstore->quantity=nq;
    g.store.dirty=1;
    bm_sound(RID_sound_collect);
    game_report_item_quantity_add(itemid,quantity);
    store_broadcast('i',itemid,0);
    return 1;
  }
  
  /* We don't have this item in inventory.
   * Get its details.
   */
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail) { // Undefined item. Must be an error somewhere.
    fprintf(stderr,"%s:%d:ERROR: itemid %d not defined in item_detailv\n",__FILE__,__LINE__,itemid);
    return 0;
  }
  
  /* If it's inventoriable, store it in a fresh inventory slot.
   */
  if (detail->inventoriable) {
    if (detail->initial_limit>1) {
      if (quantity<1) {
        fprintf(stderr,"%s:%d:ERROR: itemid %d expects a quantity but none was provided.\n",__FILE__,__LINE__,itemid);
        return 0;
      }
    } else {
      if (quantity) {
        fprintf(stderr,"%s:%d:ERROR: Quantity %d unnecessarily provided for singleton itemid %d.\n",__FILE__,__LINE__,quantity,itemid);
        return 0;
      }
    }
    if (!(invstore=store_add_itemid(itemid,quantity))) { // Presumably inventory full.
      game_report_item_full(itemid);
      return 0;
    }
    if (detail->initial_limit>0) invstore->limit=detail->initial_limit;
    bm_sound(RID_sound_treasure);
    game_report_item_acquire(itemid,quantity);
    return 1;
  }
  
  /* If it's backed by fld16, we can update generically.
   */
  if (detail->fld16) {
    if (quantity<1) {
      fprintf(stderr,"%s:%d:ERROR: itemid %d expects a quantity but none was provided.\n",__FILE__,__LINE__,itemid);
      return 0;
    }
    int have=store_get_fld16(detail->fld16);
    int limit=0xffff;
    if (detail->initial_limit<0) {
      limit=store_get_fld16(-detail->initial_limit);
      if (!limit) fprintf(stderr,"%s:%d:WARNING: Initial limit for itemid %d expected in fld16 %d, but got zero.\n",__FILE__,__LINE__,itemid,-detail->initial_limit);
    }
    if (have>=limit) { // Already full.
      game_report_item_full(itemid);
      return 0;
    }
    int nq=have+quantity;
    if (nq>limit) nq=limit;
    store_set_fld16(detail->fld16,nq);
    bm_sound(RID_sound_collect);
    if (itemid==NS_itemid_heartcontainer) { // New heart containers are full, ie increment HP.
      store_set_fld16(NS_fld16_hp,store_get_fld16(NS_fld16_hp)+1);
    }
    game_report_item_quantity_add(itemid,quantity);
    return 1;
  }
  
  /* Jigpiece kind of does its own thing.
   */
  if (itemid==NS_itemid_jigpiece) {
    if (!quantity) {
      fprintf(stderr,"%s:%d:ERROR: Getting jigpiece but quantity (ie mapid) zero.\n",__FILE__,__LINE__);
      return 0;
    }
    if (store_get_jigstore(quantity)) return 0; // Already have it.
    if (!store_add_jigstore(quantity)) return 0; // Shouldn't fail.
    bm_sound(RID_sound_collect);
    return 1;
  }
  
  fprintf(stderr,"%s: Unknown itemid %d\n",__func__,itemid);
  return 0;
}
 
const struct item_detail *item_detail_for_itemid(int itemid) {
  if (itemid<=0) return 0;
  int c=sizeof(item_detailv)/sizeof(struct item_detail);
  if (itemid>=c) return 0;
  return item_detailv+itemid;
}

const struct item_detail *item_detail_for_equipped() {
  return item_detail_for_itemid(g.store.invstorev[0].itemid);
}

/* Quantity in store of item or item-like thing.
 */

int possessed_quantity_for_itemid(int itemid,int *limit) {
  if (limit) *limit=0;
  
  // Jigpiece is a little special.
  if (itemid==NS_itemid_jigpiece) {
    if (limit) *limit=INT_MAX;//TODO Can we get a correct count of possible jigpieces? It's complicated.
    return g.store.jigstorec;
  }
  
  // Everything else should be generic.
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail) return 0;
  
  // The easy usual case, use inventory.
  if (detail->inventoriable) {
    const struct invstore *invstore=store_get_itemid(itemid);
    if (!invstore) {
      if (limit) *limit=detail->initial_limit?detail->initial_limit:1;
      return 0;
    }
    if (invstore->limit) {
      if (limit) *limit=invstore->limit;
      return invstore->quantity;
    }
    if (limit) *limit=1;
    return 1; // Singleton in inventory, quantity is one.
  }
  
  // If (fld16) is set, that's the quantity.
  if (detail->fld16) {
    if (limit) {
      if (detail->initial_limit<0) *limit=store_get_fld16(-detail->initial_limit);
      else *limit=0xffff;
    }
    return store_get_fld16(detail->fld16);
  }
  
  // There will probably be plain (fld) items in the future. Maybe? Well there aren't yet.
  return 0;
}

/* activity_cheat.c
 * These are reachable in the Cave of Cheating, which will not exist in the real game.
 * Remove or neutralize these for production builds.
 */
 
#include "activity_internal.h"

/* Cheat store, multiple options.
 */
 
void begin_cheat_store(struct sprite *initiator,int arg) {
  struct modal_args_shop args={
    .text="Welcome to Cheat Shop!",
    .textc=-1,
    .speaker=initiator,
  };
  struct modal *modal=modal_spawn(&modal_type_shop,&args,sizeof(args));
  if (!modal) return;
  switch (arg) {
    case 1: {
        modal_shop_add_item(modal,NS_itemid_broom,0,1);
        modal_shop_add_item(modal,NS_itemid_divining,0,1);
        modal_shop_add_item(modal,NS_itemid_wand,0,1);
        modal_shop_add_item(modal,NS_itemid_fishpole,0,1);
        modal_shop_add_item(modal,NS_itemid_hookshot,0,1);
        modal_shop_add_item(modal,NS_itemid_magnifier,0,1);
        modal_shop_add_item(modal,NS_itemid_compass,0,1);
        modal_shop_add_item(modal,NS_itemid_bell,0,1);
        modal_shop_add_item(modal,NS_itemid_telescope,0,1);
        modal_shop_add_item(modal,NS_itemid_shovel,0,1);
      } break;
    case 2: {
        modal_shop_add_item(modal,NS_itemid_match,0,100);
        modal_shop_add_item(modal,NS_itemid_bugspray,0,10);
        modal_shop_add_item(modal,NS_itemid_potion,0,3);
        modal_shop_add_item(modal,NS_itemid_candy,0,10);
        modal_shop_add_item(modal,NS_itemid_vanishing,0,10);
        modal_shop_add_item(modal,NS_itemid_gold,0,100);
        modal_shop_add_item(modal,NS_itemid_greenfish,0,10);
        modal_shop_add_item(modal,NS_itemid_bluefish,0,10);
        modal_shop_add_item(modal,NS_itemid_redfish,0,10);
        modal_shop_add_item(modal,NS_itemid_heart,0,10);
        modal_shop_add_item(modal,NS_itemid_pepper,0,10);
      } break;
    case 3: {
        modal_shop_add_item(modal,NS_itemid_stick,0,1);
        modal_shop_add_item(modal,NS_itemid_heartcontainer,0,1);
        modal_shop_add_item(modal,NS_itemid_barrelhat,0,1);
      } break;
  }
}

/* Prompt to give away the equipped item.
 */
 
static int giveaway_cb(int optionid,void *userdata) {
  if (optionid==4) {
    struct invstore *inv=g.store.invstorev+0;
    inv->itemid=0;
    inv->quantity=0;
    inv->limit=0;
  }
  return 0;
}
 
void begin_cheat_giveaway(struct sprite *initiator,int arg) {
  if (!g.store.invstorev[0].itemid) {
    struct modal_args_dialogue args={
      .text="Talk to me with an item equipped and I can delete it.",
      .textc=-1,
      .speaker=initiator,
    };
    modal_spawn(&modal_type_dialogue,&args,sizeof(args));
    return;
  }
  const struct item_detail *detail=item_detail_for_itemid(g.store.invstorev[0].itemid);
  struct text_insertion ins;
  if (detail&&detail->strix_name) {
    ins.mode='r';
    ins.r.rid=RID_strings_item;
    ins.r.strix=detail->strix_name;
  } else {
    ins.mode='s';
    ins.s.v="thing";
    ins.s.c=5;
  }
  struct modal_args_dialogue args={
    .text="Delete your %0?",
    .textc=-1,
    .insv=&ins,
    .insc=1,
    .speaker=initiator,
    .cb=giveaway_cb,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,4);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,5);
}

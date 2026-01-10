#include "game.h"

/* Cheapside carpenter.
 * If you have a stick, he offers to make something more useful out of it.
 */
 
static void bm_activity_carpenter_cb(void *userdata,int itemid) {
  
  // Validate (itemid) and gold.
  int quantity=0,price=0;
  switch (itemid) {
    case NS_itemid_broom: price=100; break;
    case NS_itemid_divining_rod: price=3; break;
    case NS_itemid_fishpole: price=25; break;
    case NS_itemid_match: quantity=5; price=1; break;
    case NS_itemid_wand: price=25; break;
    default: return;
  }
  int gold=store_get(NS_fld_gold,10);
  if (gold<price) {
    bm_sound(RID_sound_reject,0.0);
    modal_new_dialogue(RID_strings_dialogue,105);
    return;
  }
  
  struct inventory *inventory=inventory_search(NS_itemid_stick);
  if (inventory) {
    inventory->itemid=inventory->limit=inventory->quantity=0;
  }
  if (inventory_acquire(itemid,quantity)) {
    store_set(NS_fld_gold,10,gold-price);
  }
}
 
static void bm_activity_carpenter(struct sprite *subject) {
  struct inventory *inventory=inventory_search(NS_itemid_stick);
  if (!inventory) {
    modal_new_dialogue(RID_strings_dialogue,102); // "stick required"
    return;
  }
  struct inventory optionv[8];
  int optionc=0;
  
  struct inventory *match=inventory_search(NS_itemid_match);
  if (!match||(match->quantity<match->limit)) optionv[optionc++]=(struct inventory){.itemid=NS_itemid_match,.limit=1,.quantity=5};
  if (!inventory_search(NS_itemid_divining_rod)) optionv[optionc++]=(struct inventory){.itemid=NS_itemid_divining_rod,.limit=3};
  if (!inventory_search(NS_itemid_fishpole)) optionv[optionc++]=(struct inventory){.itemid=NS_itemid_fishpole,.limit=25};
  if (!inventory_search(NS_itemid_wand)) optionv[optionc++]=(struct inventory){.itemid=NS_itemid_wand,.limit=25};
  if (!inventory_search(NS_itemid_broom)) optionv[optionc++]=(struct inventory){.itemid=NS_itemid_broom,.limit=100};
  
  if (!optionc) {
    modal_new_dialogue(RID_strings_dialogue,103); // "you already have everything"
    return;
  }
  struct modal *modal=modal_new_dialogue(RID_strings_dialogue,104);
  if (!modal) return;
  modal_dialogue_set_callback(modal,bm_activity_carpenter_cb,0);
  modal_dialogue_set_shop(modal,optionv,optionc);
}

/* Nurse in Cheapside.
 */
 
static void bm_activity_cheapside_clinic_cb(void *userdata,int choice) {
  switch (choice) {
    case 1: { // Sell heart
        int hp=store_get(NS_fld_hp,4);
        if (hp<1) return;
        int gold=store_get(NS_fld_gold,10);
        if ((gold+=1)>999) gold=999;
        store_set(NS_fld_gold,10,gold);
        store_set(NS_fld_hp,4,hp-1);
      } break;
    case 2: { // Buy heart
        int hp=store_get(NS_fld_hp,4);
        int hpmax=store_get(NS_fld_hpmax,4);
        if (hp>=hpmax) return;
        int gold=store_get(NS_fld_gold,10);
        if (gold<2) {
          modal_new_dialogue(RID_strings_dialogue,105);
          return;
        }
        store_set(NS_fld_hp,4,hp+1);
        store_set(NS_fld_gold,10,gold-2);
      } break;
    case 3: { // Cheat -- TODO remove before production
        store_set(NS_fld_hp,4,store_get(NS_fld_hpmax,4));
      } break;
  }
}
 
static void bm_activity_cheapside_clinic(struct sprite *subject) {
  int hp=store_get(NS_fld_hp,4);
  int hpmax=store_get(NS_fld_hpmax,4);
  struct modal *modal=modal_new_dialogue(RID_strings_dialogue,106);
  if (!modal) return;
  modal_dialogue_set_callback(modal,bm_activity_cheapside_clinic_cb,0);
  if (hp>1) {
    modal_dialogue_add_choice_res(modal,1,RID_strings_dialogue,107); // Sell
  }
  if (hp<hpmax) {
    modal_dialogue_add_choice_res(modal,2,RID_strings_dialogue,108); // Buy
    modal_dialogue_add_choice_res(modal,3,RID_strings_dialogue,109); // Cheat
  }
}

/* Begin activity, general entry point.
 */
 
void bm_begin_activity(int activity,struct sprite *subject) {
  switch (activity) {
    case 0: break;
    case NS_activity_carpenter: bm_activity_carpenter(subject); break;
    case NS_activity_cheapside_clinic: bm_activity_cheapside_clinic(subject); break;
    default: fprintf(stderr,"%s:%d: Activity %d not defined.\n",__FILE__,__LINE__,activity);
  }
}

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
  //TODO if (gold<price)...
  
  struct inventory *inventory=inventory_search(NS_itemid_stick);
  if (inventory) {
    inventory->itemid=inventory->limit=inventory->quantity=0;
  }
  if (inventory_acquire(itemid,quantity)) {
    //TODO gold-=price
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

/* Begin activity, general entry point.
 */
 
void bm_begin_activity(int activity,struct sprite *subject) {
  switch (activity) {
    case 0: break;
    case NS_activity_carpenter: bm_activity_carpenter(subject); break;
    default: fprintf(stderr,"%s:%d: Activity %d not defined.\n",__FILE__,__LINE__,activity);
  }
}

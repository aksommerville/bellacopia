#include "bellacopia.h"

/* Generic read-only dialogue.
 */
 
static void begin_dialogue(int strix,struct sprite *speaker) {
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=strix,
    .speaker=speaker,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
}

/* Carpenter.
 */
 
static int cb_carpenter(int itemid,int quantity,int price,void *userdata) {
  struct invstore *invstore=store_get_itemid(NS_itemid_stick);
  if (!invstore) return -1; // Where'd the stick go?
  invstore->itemid=invstore->quantity=invstore->limit=0;
  return 0; // Proceed with purchase. And if the stick was equipped, its replacement should go in the same slot, which is nice.
}
 
static void begin_carpenter(struct sprite *sprite) {
  struct invstore *invstore=store_get_itemid(NS_itemid_stick);
  if (!invstore) { // "Bring a stick!"
    begin_dialogue(10,sprite);
    return;
  }
  struct modal_args_shop args={
    .rid=RID_strings_dialogue,
    .strix=11,
    .speaker=sprite,
    .cb_validated=cb_carpenter,
  };
  struct modal *modal=modal_spawn(&modal_type_shop,&args,sizeof(args));
  if (!modal) return;
  modal_shop_add_item(modal,NS_itemid_match,1,5);
  modal_shop_add_item(modal,NS_itemid_divining,3,0);
  modal_shop_add_item(modal,NS_itemid_wand,10,0);
  modal_shop_add_item(modal,NS_itemid_fishpole,20,0);
  modal_shop_add_item(modal,NS_itemid_broom,100,0);
}

/* Brewer.
 */
 
static void begin_brewer(struct sprite *sprite) {
  begin_dialogue(13,sprite);//TODO
}

/* Blood bank.
 */
 
static void begin_bloodbank(struct sprite *sprite) {
  begin_dialogue(12,sprite);//TODO
}

/* Fishwife.
 */
 
static void begin_fishwife(struct sprite *sprite) {
  begin_dialogue(14,sprite);//TODO
}

/* Begin activity.
 */
 
void game_begin_activity(int activity,int arg,struct sprite *initiator) {
  switch (activity) {
    case NS_activity_dialogue: begin_dialogue(arg,initiator); break;
    case NS_activity_carpenter: begin_carpenter(initiator); break;
    case NS_activity_brewer: begin_brewer(initiator); break;
    case NS_activity_bloodbank: begin_bloodbank(initiator); break;
    case NS_activity_fishwife: begin_fishwife(initiator); break;
    default: {
        fprintf(stderr,"Unknown activity %d.\n",activity);
      }
  }
}

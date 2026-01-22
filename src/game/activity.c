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
 
static int cb_brewer_fill(int option,void *userdata) {
  if (option!=4) return 0;
  int gold=store_get_fld16(NS_fld16_gold);
  if (gold<10) {
    begin_dialogue(2,0);
    return 0;
  }
  struct invstore *invstore=store_get_itemid(NS_itemid_potion);
  if (!invstore) return 0;
  invstore->quantity=invstore->limit;
  gold-=10;
  store_set_fld16(NS_fld16_gold,gold);
  bm_sound(RID_sound_collect);
  return 1;
}

static int cb_brewer_join(int option,void *userdata) {
  if (option!=4) return 0;
  int gold=store_get_fld16(NS_fld16_gold);
  if (gold<10) {
    begin_dialogue(2,0);
    return 0;
  }
  struct invstore *invstore=store_add_itemid(NS_itemid_potion,3);
  if (!invstore) return 0;
  gold-=10;
  store_set_fld16(NS_fld16_gold,gold);
  bm_sound(RID_sound_collect);
  return 1;
}
 
static void begin_brewer(struct sprite *sprite) {
  struct invstore *invstore=store_get_itemid(NS_itemid_potion);
  
  if (!invstore) { // Join the Frequent Shopper Rewards Club?
    struct modal_args_dialogue args={
      .rid=RID_strings_dialogue,
      .strix=14,
      .speaker=sprite,
      .cb=cb_brewer_join,
    };
    struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
    if (!modal) return;
    modal_dialogue_add_option_string(modal,RID_strings_dialogue,4);
    modal_dialogue_add_option_string(modal,RID_strings_dialogue,5);
    return;
  }
  
  if (invstore->quantity>=invstore->limit) { // Bottle full!
    begin_dialogue(15,sprite);
    return;
  }
  
  // Top er off for ya?
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=16,
    .speaker=sprite,
    .cb=cb_brewer_fill,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,4);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,5);
}

/* Blood bank.
 */
 
static int cb_bloodbank(int option,void *userdata) {
  if (option==4) { // yes
    int price=(int)(uintptr_t)userdata;
    if (price<0) return 0;
    int hp=store_get_fld16(NS_fld16_hp);
    if (hp<2) return 0;
    hp--;
    int gold=store_get_fld16(NS_fld16_gold);
    int goldmax=store_get_fld16(NS_fld16_goldmax);
    if ((gold+=price)>goldmax) gold=goldmax;
    store_set_fld16(NS_fld16_hp,hp);
    store_set_fld16(NS_fld16_gold,gold);
    bm_sound(RID_sound_collect);
    return 1;
  }
  return 0;
}
 
static void begin_bloodbank(struct sprite *sprite,int price) {
  int hp=store_get_fld16(NS_fld16_hp);
  if (hp<=1) {
    begin_dialogue(13,sprite); // "don't give me your last heart, you goof"
    return;
  }
  struct text_insertion insv={.mode='i',.i=price};
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=12,
    .speaker=sprite,
    .insv=&insv,
    .insc=1,
    .cb=cb_bloodbank,
    .userdata=(void*)(uintptr_t)price,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,4); // yes
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,5); // no
}

/* Fishwife.
 */
 
static int fishwife_gather_price(int *fishc) {
  int greenc=store_get_fld16(NS_fld16_greenfish);
  int bluec=store_get_fld16(NS_fld16_bluefish);
  int redc=store_get_fld16(NS_fld16_redfish);
  if (fishc) *fishc=greenc+bluec+redc;
  return greenc+bluec*5+redc*20;
}
 
static int cb_fishwife(int option,void *userdata) {
  if (option!=4) return 0;
  int price=fishwife_gather_price(0);
  int gold=store_get_fld16(NS_fld16_gold);
  int goldmax=store_get_fld16(NS_fld16_goldmax);
  if ((gold+=price)>=goldmax) gold=goldmax;
  store_set_fld16(NS_fld16_gold,gold);
  store_set_fld16(NS_fld16_greenfish,0);
  store_set_fld16(NS_fld16_bluefish,0);
  store_set_fld16(NS_fld16_redfish,0);
  bm_sound(RID_sound_collect);
  return 1;
}
 
static void begin_fishwife(struct sprite *sprite) {
  int fishc=0;
  int price=fishwife_gather_price(&fishc);
  if (fishc<1) { // "Bring me fish"
    begin_dialogue(17,sprite);
    return;
  }
  struct text_insertion insv[]={
    {.mode='i',.i=fishc},
    {.mode='i',.i=price},
  };
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=18,
    .speaker=sprite,
    .insv=insv,
    .insc=2,
    .cb=cb_fishwife,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,4);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,5);
}

/* Begin activity.
 */
 
void game_begin_activity(int activity,int arg,struct sprite *initiator) {
  switch (activity) {
    case NS_activity_dialogue: begin_dialogue(arg,initiator); break;
    case NS_activity_carpenter: begin_carpenter(initiator); break;
    case NS_activity_brewer: begin_brewer(initiator); break;
    case NS_activity_bloodbank: begin_bloodbank(initiator,arg); break;
    case NS_activity_fishwife: begin_fishwife(initiator); break;
    default: {
        fprintf(stderr,"Unknown activity %d.\n",activity);
      }
  }
}

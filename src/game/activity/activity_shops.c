/* activity_shops.c
 * Shops and minor dialogue for all around the world.
 */
 
#include "activity_internal.h"

/* Carpenter.
 */
 
static int cb_carpenter(int itemid,int quantity,int price,void *userdata) {
  struct invstore *invstore=store_get_itemid(NS_itemid_stick);
  if (!invstore) return -1; // Where'd the stick go?
  invstore->itemid=invstore->quantity=invstore->limit=0;
  store_broadcast('i',NS_itemid_stick,0);
  return 0; // Proceed with purchase. And if the stick was equipped, its replacement should go in the same slot, which is nice.
}
 
void begin_carpenter(struct sprite *sprite) {
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
  // Matches are always available, even if you can't hold any more. Ten for penny, what a deal!
  modal_shop_add_item(modal,NS_itemid_match,1,10);
  // The other things are singletons. Only show if we don't have it yet.
  if (!store_get_itemid(NS_itemid_divining)) modal_shop_add_item(modal,NS_itemid_divining,3,0);
  if (!store_get_itemid(NS_itemid_wand)) modal_shop_add_item(modal,NS_itemid_wand,10,0);
  if (!store_get_itemid(NS_itemid_fishpole)) modal_shop_add_item(modal,NS_itemid_fishpole,20,0);
  if (!store_get_itemid(NS_itemid_broom)) modal_shop_add_item(modal,NS_itemid_broom,100,0);
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
  store_broadcast('i',NS_itemid_potion,0);
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
 
void begin_brewer(struct sprite *sprite) {
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
 
void begin_bloodbank(struct sprite *sprite,int price) {
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
 
void begin_fishwife(struct sprite *sprite) {
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

/* Compass technician at Magnetic North.
 * These are generic. To add an upgradable target, see `upgradev` in `targets.c`.
 */
 
static int magneticnorth_cb_giveaway(int optionid,void *userdata) {
  game_get_item(NS_itemid_compass,0);
  game_disable_all_targets(); // In case this is your second time getting a compass.
  return 1;
}

static int magneticnorth_cb_upgrade(int optionid,void *userdata) {
  struct sprite *sprite=userdata;
  if (optionid<1) return 0;
  int gold=store_get_fld16(NS_fld16_gold);
  if (gold<10) {
    begin_dialogue(2,sprite);
    bm_sound(RID_sound_reject);
    return 1;
  }
  gold-=10;
  store_set_fld16(NS_fld16_gold,gold);
  game_enable_target(optionid);
  bm_sound(RID_sound_collect);
  return 1;
}
 
void begin_magneticnorth(struct sprite *sprite) {
  struct invstore *invstore=store_get_itemid(NS_itemid_compass);
  if (invstore) { // Offer upgrades...
  
    // Get all known targets.
    int targetv[16];
    int targetc=game_list_targets(targetv,sizeof(targetv)/sizeof(targetv[0]),TARGET_MODE_UPGRADE);
    if (targetc<1) { // Fully upgraded.
      begin_dialogue(28,sprite);
    } else { // Offer upgrades...
      // We're using regular dialogue for this, not shop. Shop can only sell things with an itemid.
      struct modal_args_dialogue args={
        .rid=RID_strings_dialogue,
        .strix=29,
        .speaker=sprite,
        .cb=magneticnorth_cb_upgrade,
        .userdata=sprite,
      };
      struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
      if (!modal) return;
      int i=0; for (;i<targetc;i++) {
        modal_dialogue_add_option_string(modal,RID_strings_item,targetv[i]);
      }
    }
    
  } else { // Give away the compass...
    struct modal *dialogue=begin_dialogue(27,sprite);
    if (!dialogue) return;
    modal_dialogue_set_callback(dialogue,magneticnorth_cb_giveaway,0);
  }
}

/* Thingwalla: The "things" shop in Fractia.
 */
 
void begin_thingwalla(struct sprite *sprite) {
  struct modal_args_shop args={
    .rid=RID_strings_dialogue,
    .strix=30,
    .speaker=sprite,
  };
  struct modal *modal=modal_spawn(&modal_type_shop,&args,sizeof(args));
  if (!modal) return;
  modal_shop_add_item(modal,NS_itemid_candy,1,0);
  modal_shop_add_item(modal,NS_itemid_bugspray,1,0);
  modal_shop_add_item(modal,NS_itemid_match,2,0);
  modal_shop_add_item(modal,NS_itemid_vanishing,6,0);
}

/* Fish Processor: Offers to change your fish into items.
 * greenfish => candy x10
 * bluefish => bugspray x10
 * redfish => potion x3
 */
 
static int fishprocessor_cb(int optionid,void *userdata) {
  if (!optionid) return 0;
  int gold=store_get_fld16(NS_fld16_gold);
  if (gold<2) {
    bm_sound(RID_sound_reject);
    begin_dialogue(2,userdata);
    return 1;
  }
  gold-=2;
  store_set_fld16(NS_fld16_gold,gold);
  switch (optionid) {
  
    case NS_itemid_candy: {
        int fishc=store_get_fld16(NS_fld16_greenfish);
        if (fishc<1) return 0;
        fishc--;
        store_set_fld16(NS_fld16_greenfish,fishc);
        game_get_item(NS_itemid_candy,10);
      } return 1;
      
    case NS_itemid_bugspray: {
        int fishc=store_get_fld16(NS_fld16_bluefish);
        if (fishc<1) return 0;
        fishc--;
        store_set_fld16(NS_fld16_bluefish,fishc);
        game_get_item(NS_itemid_bugspray,10);
      } return 1;
      
    case NS_itemid_potion: {
        int fishc=store_get_fld16(NS_fld16_redfish);
        if (fishc<1) return 0;
        fishc--;
        store_set_fld16(NS_fld16_redfish,fishc);
        game_get_item(NS_itemid_potion,3);
      } return 1;
  }
  return 0;
}
 
void begin_fishprocessor(struct sprite *sprite) {

  // Only propose options where they have one fish and room for at least one output.
  int greenc=store_get_fld16(NS_fld16_greenfish);
  int bluec=store_get_fld16(NS_fld16_bluefish);
  int redc=store_get_fld16(NS_fld16_redfish);
  int gold=store_get_fld16(NS_fld16_gold);
  
  // No options, show a different message.
  if (!greenc&&!bluec&&!redc) {
    begin_dialogue(34,sprite);
    return;
  }
  
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=35,
    .speaker=sprite,
    .cb=fishprocessor_cb,
    .userdata=sprite,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  #define ADD(color,tag,strix,qty) if (color##c) { \
    char tmp[64]; \
    struct text_insertion insv[]={ \
      {.mode='i',.i=1}, /* how many fish, always 1 */ \
      {.mode='i',.i=qty}, \
    }; \
    int tmpc=text_format_res(tmp,sizeof(tmp),RID_strings_dialogue,strix,insv,sizeof(insv)/sizeof(insv[0])); \
    if ((tmpc<0)||(tmpc>sizeof(tmp))) tmpc=0; \
    modal_dialogue_add_option(modal,NS_itemid_##tag,tmp,tmpc); \
  }
  ADD(green,candy,36,10)
  ADD(blue,bugspray,37,10)
  ADD(red,potion,38,3)
  #undef ADD
}

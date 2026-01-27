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
  // Matches are always available, even if you can't hold any more.
  modal_shop_add_item(modal,NS_itemid_match,1,5);
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

/* Toll Troll.
 */
 
int tolltroll_get_appearance() {

  // Already paid?
  if (store_get_fld(NS_fld_toll_paid)) return 0;
  
  // We want the thing they don't have. Check all three.
  if (!store_get_itemid(NS_itemid_stick)) return NS_itemid_stick;
  if (!store_get_itemid(NS_itemid_compass)) return NS_itemid_compass;
  if (!store_get_itemid(NS_itemid_candy)) return NS_itemid_candy;
  
  // If they have all three things, prefer one that we've asked for already.
  if (store_get_fld(NS_fld_toll_stick_requested)) return NS_itemid_stick;
  if (store_get_fld(NS_fld_toll_compass_requested)) return NS_itemid_compass;
  if (store_get_fld(NS_fld_toll_candy_requested)) return NS_itemid_candy;
  
  // All three things are equally available and desirable, so ask for my very favorite: the stick.
  return NS_itemid_stick;
}

static void begin_tolltroll(struct sprite *sprite,int appearance) {
  if (!appearance) return;

  // Have the item I'm looking for? Great, we're done.
  struct invstore *invstore=store_get_itemid(appearance);
  if (invstore&&(invstore->quantity||!invstore->limit)) {
    const struct item_detail *detail=item_detail_for_itemid(appearance);
    if (!detail) return;
    if (invstore->limit) {
      invstore->quantity--;
    } else {
      invstore->itemid=invstore->limit=invstore->quantity=0;
    }
    struct text_insertion ins={.mode='r',.r={.rid=RID_strings_item,.strix=detail->strix_name}};
    struct modal_args_dialogue args={
      .rid=RID_strings_dialogue,
      .strix=21,
      .speaker=sprite,
      .insv=&ins,
      .insc=1,
    };
    modal_spawn(&modal_type_dialogue,&args,sizeof(args));
    store_set_fld(NS_fld_toll_paid,1);
    return;
  }
  
  /* If one of the "requested" flags is set, and we have that item, it's a different message.
   * But it's the same message every time, we just insert the two items' names.
   * Good to check compass before stick. The usual order is Stick, Compass, Candy. When you get the Candy message, he should mention Compass.
   */
  int offered=0;
  if (store_get_fld(NS_fld_toll_candy_requested)&&possessed_quantity_for_itemid(NS_itemid_candy,0)) {
    offered=NS_itemid_candy;
  } else if (store_get_fld(NS_fld_toll_compass_requested)&&store_get_itemid(NS_itemid_compass)) {
    offered=NS_itemid_compass;
  } else if (store_get_fld(NS_fld_toll_stick_requested)&&store_get_itemid(NS_itemid_stick)) {
    offered=NS_itemid_stick;
  }
  if (offered) {
    const struct item_detail *detail_offered=item_detail_for_itemid(offered);
    const struct item_detail *detail_wanted=item_detail_for_itemid(appearance);
    if (!detail_offered||!detail_wanted) return;
    struct text_insertion insv[]={
      {.mode='r',.r={.rid=RID_strings_item,.strix=detail_offered->strix_name}},
      {.mode='r',.r={.rid=RID_strings_item,.strix=detail_wanted->strix_name}},
    };
    struct modal_args_dialogue args={
      .rid=RID_strings_dialogue,
      .strix=22,
      .speaker=sprite,
      .insv=insv,
      .insc=2,
    };
    modal_spawn(&modal_type_dialogue,&args,sizeof(args));
    switch (appearance) {
      case NS_itemid_stick: store_set_fld(NS_fld_toll_stick_requested,1); break;
      case NS_itemid_compass: store_set_fld(NS_fld_toll_compass_requested,1); break;
      case NS_itemid_candy: store_set_fld(NS_fld_toll_candy_requested,1); break;
    }
    return;
  }
  
  /* First time or fallback, use the more generic message 23.
   */
  const struct item_detail *detail=item_detail_for_itemid(appearance);
  if (!detail) return;
  struct text_insertion ins={.mode='r',.r={.rid=RID_strings_item,.strix=detail->strix_name}};
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=23,
    .speaker=sprite,
    .insv=&ins,
    .insc=1,
  };
  modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  switch (appearance) {
    case NS_itemid_stick: store_set_fld(NS_fld_toll_stick_requested,1); break;
    case NS_itemid_compass: store_set_fld(NS_fld_toll_compass_requested,1); break;
    case NS_itemid_candy: store_set_fld(NS_fld_toll_candy_requested,1); break;
  }
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
    case NS_activity_tolltroll: begin_tolltroll(initiator,arg); break;
    case NS_activity_wargate: begin_dialogue(19,initiator); break; // It's just dialogue. But the activity causes sprite to abort at spawn.
    default: {
        fprintf(stderr,"Unknown activity %d.\n",activity);
      }
  }
}

/* Abort sprite?
 */
 
int game_activity_sprite_should_abort(int activity,const struct sprite_type *type) {
  switch (activity) {
    case NS_activity_wargate: if (store_get_fld(NS_fld_war_over)) return 1; return 0;
  }
  return 0;
}

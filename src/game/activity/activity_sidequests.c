/* activity_sidequests.c
 * Side quests that aren't involved enough to warrant their own file.
 */
 
#include "activity_internal.h"

/* Toll Troll (the main quest one).
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

void begin_tolltroll(struct sprite *sprite,int appearance) {
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
    store_broadcast('i',appearance,0);
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

/* Toll Troll, generic.
 */
 
static int cb_generic_tolltroll(int optionid,void *userdata) {
  if (optionid==97) {
    int arg=(int)(uintptr_t)userdata;
    int price=(arg>>12)&0xfff;
    int fld=arg&0xfff;
    int gold=store_get_fld16(NS_fld16_gold);
    if (gold<price) {
      begin_dialogue(2,0);
      bm_sound(RID_sound_reject);
      return 1;
    }
    gold-=price;
    store_set_fld16(NS_fld16_gold,gold);
    store_set_fld(fld,1);
  }
  return 0;
}
 
void begin_generic_tolltroll(struct sprite *sprite,int arg) {
  int price=(arg>>12)&0xfff;
  int fld=arg&0xfff;
  fprintf(stderr,"%s price=%d fld=%d\n",__func__,price,fld);
  if (store_get_fld(fld)) return;
  struct text_insertion ins={.mode='i',.i=price};
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=96,
    .speaker=sprite,
    .insv=&ins,
    .insc=1,
    .cb=cb_generic_tolltroll,
    .userdata=(void*)(uintptr_t)arg,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,97);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,98);
}

/* Knitter: Give you a barrel hat if you don't have one.
 */
 
static int all_barrels_hatted() {
  if (!store_get_fld(NS_fld_barrelhat1)) return 0;
  if (!store_get_fld(NS_fld_barrelhat2)) return 0;
  if (!store_get_fld(NS_fld_barrelhat3)) return 0;
  if (!store_get_fld(NS_fld_barrelhat4)) return 0;
  if (!store_get_fld(NS_fld_barrelhat5)) return 0;
  if (!store_get_fld(NS_fld_barrelhat6)) return 0;
  if (!store_get_fld(NS_fld_barrelhat7)) return 0;
  if (!store_get_fld(NS_fld_barrelhat8)) return 0;
  if (!store_get_fld(NS_fld_barrelhat9)) return 0;
  return 1;
}
 
static int knitter_cb(int optionid,void *userdata) {
  //TODO incremental prizes?
  game_get_item(NS_itemid_barrelhat,0);
  return 1;
}

static int knitter_cb_final(int optionid,void *userdata) {
  game_get_item(NS_itemid_bell,0);
  return 1;
}
 
void begin_knitter(struct sprite *sprite) {
  if (store_get_itemid(NS_itemid_barrelhat)) {
    begin_dialogue(25,sprite);
  } else if (all_barrels_hatted()) {
    if (store_get_itemid(NS_itemid_bell)) {
      begin_dialogue(31,sprite); // "Thanks again!"
    } else {
      struct modal *dialogue=begin_dialogue(26,sprite); // "Woo hoo, here have a bell."
      if (!dialogue) return;
      modal_dialogue_set_callback(dialogue,knitter_cb_final,0);
    }
  } else {
    struct modal *dialogue=begin_dialogue(24,sprite);
    if (!dialogue) return;
    modal_dialogue_set_callback(dialogue,knitter_cb,0);
  }
}

/* Tree: Either complain about being bored, or thank Dot for telling us a story.
 */
 
void begin_tree(struct sprite *initiator,int arg) {
  if (store_get_fld(arg)) {
    begin_dialogue(92,initiator);
  } else {
    begin_dialogue(91,initiator);
  }
}

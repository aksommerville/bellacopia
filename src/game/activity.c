#include "bellacopia.h"

/* Generic read-only dialogue.
 */
 
static struct modal *begin_dialogue(int strix,struct sprite *speaker) {
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=strix,
    .speaker=speaker,
  };
  return modal_spawn(&modal_type_dialogue,&args,sizeof(args));
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
 
static void begin_knitter(struct sprite *sprite) {
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
 
static void begin_magneticnorth(struct sprite *sprite) {
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
 
static void begin_thingwalla(struct sprite *sprite) {
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

/* King: Behooves you to rescue the Princess, or recompenses you for doing so.
 */
 
static int king_cb_win(int optionid,void *userdata) {
  store_set_fld(NS_fld_rescued_princess,1);
  int goldmax=store_get_fld16(NS_fld16_goldmax);
  goldmax+=100;
  store_set_fld16(NS_fld16_goldmax,goldmax);
  store_set_fld16(NS_fld16_gold,goldmax); // Also fill er up, as long as we're in there.
  bm_sound(RID_sound_treasure);
  return 1;
}

// null and (*near) nonzero if the princess exists but out of range.
static struct sprite *find_princess(struct sprite *king,int *near) {
  struct sprite **spritep=GRP(monsterlike)->sprv;
  int i=GRP(monsterlike)->sprc;
  for (;i-->0;spritep++) {
    struct sprite *sprite=*spritep;
    if (sprite->defunct) continue;
    if (sprite->type!=&sprite_type_princess) continue;
    *near=1;
    double dx=sprite->x-king->x;
    double dy=sprite->y-king->y;
    double d2=dx*dx+dy*dy;
    double tolerance=5.0;
    tolerance*=tolerance;
    if (d2>tolerance) return 0;
    return sprite;
  }
  return 0;
}
 
static void begin_king(struct sprite *sprite) {
  if (store_get_fld(NS_fld_rescued_princess)) {
    begin_dialogue(32,sprite); // "Thanks again!"
  } else {
    int near=0;
    struct sprite *princess=find_princess(sprite,&near);
    if (princess) {
      sprite_kill_soon(princess);
      struct modal *modal=begin_dialogue(20,sprite); // "Hey thanks!"
      if (!modal) return;
      modal_dialogue_set_callback(modal,king_cb_win,sprite);
    } else if (near) {
      begin_dialogue(42,sprite); // "Where is she?"
    } else {
      begin_dialogue(33,sprite); // "Oh no Princess is kidnapped!"
    }
  }
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
 
static void begin_fishprocessor(struct sprite *sprite) {

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

/* Jaildoor: Unlock if we have the key.
 */
 
static void begin_jaildoor() {
  if (store_get_fld(NS_fld_jailopen)) return;
  if (store_get_fld(NS_fld_jailkey)) {
    store_set_fld(NS_fld_jailopen,1);
    g.camera.mapsdirty=1;
    bm_sound(RID_sound_presto);
  }
}

/* Kidnap: The big cutscene where Dot gets kidnapped by goblins.
 */
 
static int kidnap_cb(int optionid,void *userdata) {
  game_warp(RID_map_jail,NS_transition_fadeblack);
  return 0;
}
 
static void begin_kidnap(struct sprite *sprite) {
  fprintf(stderr,"TODO %s:%d:%s\n",__FILE__,__LINE__,__func__);
  struct modal_args_dialogue args={
    .text="TODO: Cutscene of Dot falling into a trap, then the goblins drag her off.",
    .textc=-1,
    .cb=kidnap_cb,
    .userdata=sprite,
  };
  modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  store_set_fld(NS_fld_kidnapped,1);
}

/* Escape: Nothing noticeable to the user, but as you leave the cave we set this flag.
 * It's so we know whether to spawn you next in jail or at home.
 * A crafty user might try to get out of jail by quitting and restarting the game.
 */
 
static void begin_escape() {
  store_set_fld(NS_fld_escaped,1);
}

/* Cryptmsg: One of the seven obelisks giving encrypted clues in the goblins' cave.
 */
 
static void begin_cryptmsg(int which) {
  
  /* If we're in a dark room -- they all are -- require a nearby light source.
   * Camera figures out its lights on the fly and doesn't leave any useful residue for us.
   * Cryptmsg 3, north center of Skull Lake, is 103 m**2, and to my eye should be just barely outside.
   * Perfect! Let's call the threshold 100.
   */
  if (g.camera.darkness>0.0) {
    int lit=0;
    if (GRP(hero)->sprc>=1) {
      struct sprite *hero=GRP(hero)->sprv[0];
      struct sprite *nearlight=0;
      double neard2=999.999;
      struct sprite **qp=GRP(light)->sprv;
      int i=GRP(light)->sprc;
      for (;i-->0;qp++) {
        struct sprite *q=*qp;
        if (q->defunct) continue;
        double dx=q->x-hero->x;
        double dy=q->y-hero->y;
        double d2=dx*dx+dy*dy;
        if (d2<neard2) {
          nearlight=q;
          neard2=d2;
          if (d2<1.0) break;
        }
      }
      if (neard2<100.0) lit=1;
    }
    if (!lit) {
      begin_dialogue(45,0); // "Too dark"
      return;
    }
  }
  
  char msg[256];
  int msgc=cryptmsg_get(msg,sizeof(msg),which);
  if ((msgc<1)||(msgc>sizeof(msg))) return;
  
  struct modal_args_cryptmsg args={
    .src=msg,
    .srcc=msgc,
  };
  struct modal *modal=modal_spawn(&modal_type_cryptmsg,&args,sizeof(args));
}

/* Linguist: Offer help with translating Old Goblish, or show the things already purchased.
 * There's a special modal for the whole interaction; all we do is spawn it.
 */
 
static void begin_linguist(struct sprite *sprite) {
  struct modal_args_linguist args={
    .speaker=sprite,
  };
  struct modal *modal=modal_spawn(&modal_type_linguist,&args,sizeof(args));
}

/* Log Problems, Volumes One Through Two: Bystanders in Fractia that complain about the log.
 */
 
static void begin_logproblem1(struct sprite *sprite) {
  if (store_get_fld(NS_fld_mayor)) begin_dialogue(63,sprite);
  else if (store_get_fld(NS_fld_election_start)) begin_dialogue(62,sprite);
  else begin_dialogue(61,sprite);
}

static void begin_logproblem2(struct sprite *sprite) {
  if (store_get_fld(NS_fld_mayor)) begin_dialogue(65,sprite);
  else begin_dialogue(64,sprite);
}

/* Board of Elections.
 */

// Callback when asking do you want to run for mayor.
static int cb_board_of_elections_register(int optionid,void *userdata) {
  if (optionid==4) { // "yes"
    bm_sound(RID_sound_uiactivate);
    store_set_fld(NS_fld_election_start,1);
    return 1;
  }
  return 0;
}

// Callback when asking do you want to hold the election.
static int cb_board_of_elections_vote(int optionid,void *userdata) {
  if (optionid==4) { // "yes"
    bm_sound(RID_sound_uiactivate);
    fprintf(stderr,"*** %s:%d:%s:TODO: Begin election battle ***\n",__FILE__,__LINE__,__func__);
    store_set_fld(NS_fld_mayor,1);//XXX
    return 1;
  }
  return 0;
}
 
static void begin_board_of_elections(struct sprite *sprite) {
  // Already mayor, no more need for elections.
  if (store_get_fld(NS_fld_mayor)) {
    begin_dialogue(68,sprite);
    return;
  }
  // Propose running for mayor, or holding the election.
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=66,
    .speaker=sprite,
    .cb=cb_board_of_elections_register,
  };
  if (store_get_fld(NS_fld_election_start)) {
    args.strix=67;
    args.cb=cb_board_of_elections_vote;
  }
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
    case NS_activity_tolltroll: begin_tolltroll(initiator,arg); break;
    case NS_activity_wargate: begin_dialogue(19,initiator); break; // It's just dialogue. But the activity causes sprite to abort at spawn.
    case NS_activity_knitter: begin_knitter(initiator); break;
    case NS_activity_magneticnorth: begin_magneticnorth(initiator); break;
    case NS_activity_thingwalla: begin_thingwalla(initiator); break;
    case NS_activity_king: begin_king(initiator); break;
    case NS_activity_fishprocessor: begin_fishprocessor(initiator); break;
    case NS_activity_jaildoor: begin_jaildoor(); break;
    case NS_activity_kidnap: begin_kidnap(initiator); break;
    case NS_activity_escape: begin_escape(); break;
    case NS_activity_cryptmsg: begin_cryptmsg(arg); break;
    case NS_activity_linguist: begin_linguist(initiator); break;
    case NS_activity_logproblem1: begin_logproblem1(initiator); break;
    case NS_activity_logproblem2: begin_logproblem2(initiator); break;
    case NS_activity_board_of_elections: begin_board_of_elections(initiator); break;
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

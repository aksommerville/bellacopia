/* activity_sidequests.c
 * Side quests that aren't involved enough to warrant their own file.
 */
 
#include "activity_internal.h"
#include "game/race/race.h"

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
 
static int any_barrel_hatted() {
  if (store_get_fld(NS_fld_barrelhat1)) return 1;
  if (store_get_fld(NS_fld_barrelhat2)) return 1;
  if (store_get_fld(NS_fld_barrelhat3)) return 1;
  if (store_get_fld(NS_fld_barrelhat4)) return 1;
  if (store_get_fld(NS_fld_barrelhat5)) return 1;
  if (store_get_fld(NS_fld_barrelhat6)) return 1;
  if (store_get_fld(NS_fld_barrelhat7)) return 1;
  if (store_get_fld(NS_fld_barrelhat8)) return 1;
  if (store_get_fld(NS_fld_barrelhat9)) return 1;
  return 0;
}
 
static int knitter_cb(int optionid,void *userdata) {
  game_get_item(NS_itemid_barrelhat,0);
  if (any_barrel_hatted()) {
    game_get_item(NS_itemid_gold,10);
  }
  return 1;
}

static int knitter_cb_finaller(int optionid,void *userdata) {
  struct modal_args_cutscene args={
    .strix_title=7,
    .context=CUTSCENE_CONTEXT_EXPECTEDISH,
  };
  struct modal *modal=modal_spawn(&modal_type_cutscene,&args,sizeof(args));
  return 1;
}

static int knitter_cb_final(int optionid,void *userdata) {
  game_get_item(NS_itemid_bell,0);
  struct modal *dialogue=modal_get_topmost(&modal_type_dialogue);
  if (dialogue) {
    modal_dialogue_set_callback(dialogue,knitter_cb_finaller,0);
  } else {
    knitter_cb_finaller(0,0);
  }
  return 1;
}
 
void begin_knitter(struct sprite *sprite) {
  if (store_get_itemid(NS_itemid_barrelhat)) {
    begin_dialogue(25,sprite);
  } else if (all_barrels_hatted()) {
    if (store_get_itemid(NS_itemid_bell)) {
      begin_dialogue(31,sprite); // "Thanks again!"
    } else {
      store_set_fld(NS_fld_barrelhat_all,1);
      struct modal *dialogue=begin_dialogue(26,sprite); // "Woo hoo, here have a bell."
      if (!dialogue) return;
      modal_dialogue_set_callback(dialogue,knitter_cb_final,0);
    }
  } else {
    int strix=any_barrel_hatted()?99:24;
    struct modal *dialogue=begin_dialogue(strix,sprite); // "Take hat, give to barrel."
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
  
    // If at least so many stories have been told, and we don't have any untold ones, offer a hint about the ones outstanding.
    const int told_min=8; // Must tell so many on your own before we give hints.
    int hintv[16]; // strix in strings:stories, ready to deliver
    int toldc=0,untoldc=0,hintc=0;
    const struct story *story;
    int p=0;
    for (;;p++) {
      if (!(story=story_by_index(p))) break;
      if (store_get_fld(story->fld_present)) {
        if (store_get_fld(story->fld_told)) {
          toldc++;
        } else {
          untoldc++;
          break; // No sense proceeding; any untold nixes the feature.
        }
      } else {
        if ((hintc<16)&&(story->strix_title>=1)&&(story->strix_title<=16)) {
          hintv[hintc++]=story->strix_title+33;
        }
      }
    }
    fprintf(stderr,"%s(%d) toldc=%d untoldc=%d hintc=%d\n",__func__,arg,toldc,untoldc,hintc);
    if (!untoldc&&hintc&&(toldc>=told_min)) {
      int hint=hintv[arg%hintc]; // arg, NS_fld_tree*, are sequential. So for a given state, each outstanding tree should give a unique hint.
      struct modal_args_dialogue args={
        .rid=RID_strings_stories,
        .strix=hint,
        .speaker=initiator,
      };
      modal_spawn(&modal_type_dialogue,&args,sizeof(args));
      return;
    }
  
    begin_dialogue(91,initiator);
  }
}

/* invcritic: Judges your inventory.
 */
 
static int cb_invcritic(int optionid,void *userdata) {
  game_get_item(NS_itemid_heartcontainer,1);
  store_set_fld(NS_fld_hc3,1);
  struct modal_args_cutscene args={
    .strix_title=8,
    .context=CUTSCENE_CONTEXT_EXPECTEDISH,
  };
  struct modal *modal=modal_spawn(&modal_type_cutscene,&args,sizeof(args));
  return 1;
}

static int compare_item_names(const char *a,int ac,const char *b,int bc) {
  for (;;) {
    if (ac<1) {
      if (bc<1) return 0;
      return -1;
    }
    if (bc<1) return 1;
    char cha=*a;
    char chb=*b;
    if ((cha>='a')&&(cha<='z')) ;
    else if ((cha>='A')&&(cha<='Z')) cha+=0x20;
    else { a++; ac--; continue; } // Skip space and punctuation.
    if ((chb>='a')&&(chb<='z')) ;
    else if ((chb>='A')&&(chb<='Z')) chb+=0x20;
    else { b++; bc--; continue; } // Skip space and punctuation.
    a++; ac--;
    b++; bc--;
    if (cha<chb) return -1;
    if (cha>chb) return 1;
  }
}
 
void begin_invcritic(struct sprite *initiator) {
  if (store_get_fld(NS_fld_hc3)) {
    begin_dialogue(104,initiator); // "That was fun."
    return;
  }
  
  /* Collect names of all items.
   * Bail out if any slot is empty and say "come back with a full backpack".
   * Beware: g.store.invstorev[0], the equipped item, is first in the store but last by our reckoning.
   */
  struct criticizable_item {
    const char *name;
    int namec;
  } itemv[INVSTORE_SIZE];
  int itemc=0;
  const struct invstore *invstore=g.store.invstorev+1;
  int i=INVSTORE_SIZE-1;
  for (;i-->0;invstore++) {
    if (!invstore->itemid) {
      begin_dialogue(101,initiator);
      return;
    }
    const struct item_detail *detail=item_detail_for_itemid(invstore->itemid);
    if (!detail) return;
    struct criticizable_item *item=itemv+itemc++;
    item->namec=text_get_string(&item->name,RID_strings_item,detail->strix_name);
  }
  //...and then the equipped item...
  invstore=g.store.invstorev;
  if (!invstore->itemid) {
    begin_dialogue(101,initiator);
    return;
  }
  const struct item_detail *detail=item_detail_for_itemid(invstore->itemid);
  if (!detail) return;
  struct criticizable_item *item=itemv+itemc++;
  item->namec=text_get_string(&item->name,RID_strings_item,detail->strix_name);
  
  /* Backpack is full. Check for alphabetical order.
   */
  for (item=itemv+1,i=itemc-1;i-->0;item++) {
    int cmp=compare_item_names(item[-1].name,item[-1].namec,item[0].name,item[0].namec);
    if (cmp>0) {
      begin_dialogue(102,initiator); // "not in order!"
      return;
    }
  }
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=103,
    .speaker=initiator,
    .cb=cb_invcritic,
  };
  modal_spawn(&modal_type_dialogue,&args,sizeof(args));
}

/* Bridget: Build a bridge.
 *  1: forest-botire: Stick.
 *  2: forest-desert: Match.
 *  3: forest-mountains: Candy
 *  4: fractia-mountains: Pepper
 *  5: desert: Gold.
 *  6: botire-southjungle: Green Fish.
 *  7: botire-northjungle: Telescope
 */
 
static const struct bridgetdata {
  int fldstart;
  int fld16q;
  int flddone;
  int itemid;
  int quantity;
} bridgetdatav[]={
  {NS_fld_bridge1start,NS_fld16_bridge1q,NS_fld_bridge1done,NS_itemid_stick,      6},
  {NS_fld_bridge2start,NS_fld16_bridge2q,NS_fld_bridge2done,NS_itemid_match,     25},
  {NS_fld_bridge3start,NS_fld16_bridge3q,NS_fld_bridge3done,NS_itemid_candy,     12},
  {NS_fld_bridge4start,NS_fld16_bridge4q,NS_fld_bridge4done,NS_itemid_pepper,    12},
  {NS_fld_bridge5start,NS_fld16_bridge5q,NS_fld_bridge5done,NS_itemid_gold,     150},
  {NS_fld_bridge6start,NS_fld16_bridge6q,NS_fld_bridge6done,NS_itemid_greenfish, 15},
  {NS_fld_bridge7start,NS_fld16_bridge7q,NS_fld_bridge7done,NS_itemid_telescope,  3},
};

// There won't be more than one of our dialogues at a time, so we can cut corners by using a global context.
static struct bridget_context {
  const struct bridgetdata *bridgetdata;
  int quantity;
} bridgetctx={0};

static int bridget_cb(int optionid,void *userdata) {
  if ((optionid==4)&&bridgetctx.bridgetdata&&bridgetctx.quantity) {
    if (game_lose_item(bridgetctx.bridgetdata->itemid,bridgetctx.quantity)>=0) {
      store_set_fld(NS_fld_minimalist_disqualify,1); // Minimalist: Might have just spent stick, fish, or gold.
      int nq=store_get_fld16(bridgetctx.bridgetdata->fld16q)-bridgetctx.quantity;
      if (nq<0) nq=0;
      store_set_fld16(bridgetctx.bridgetdata->fld16q,nq);
      if (!nq) {
        store_set_fld(bridgetctx.bridgetdata->flddone,1);
        g.camera.mapsdirty=1;
      }
    }
  }
  return 0;
}
 
void begin_bridget(struct sprite *initiator,int arg) {

  // Find the metadata.
  const struct bridgetdata *bridgetdata=0;
  const struct bridgetdata *q=bridgetdatav;
  int i=sizeof(bridgetdatav)/sizeof(bridgetdatav[0]);
  for (;i-->0;q++) {
    if (q->fldstart==arg) {
      bridgetdata=q;
      break;
    }
  }
  if (!bridgetdata) {
    fprintf(stderr,"%s:%d: fld:%d is not a 'bridgeNstart' flag\n",__FILE__,__LINE__,arg);
    return;
  }
  
  // If my bridge is already built, just congratulate ourselves statically.
  if (store_get_fld(bridgetdata->flddone)) {
    begin_dialogue(118,initiator);
    return;
  }
  
  // If the quest hasn't started yet, initialize its counter and record the fact.
  int first_time=0;
  if (!store_get_fld(bridgetdata->fldstart)) {
    first_time=1;
    store_set_fld(bridgetdata->fldstart,1);
    store_set_fld16(bridgetdata->fld16q,bridgetdata->quantity);
  }
  
  // How many of this item are we asking for? Max of quantity needed and quantity the hero possesses.
  int needed=store_get_fld16(bridgetdata->fld16q);
  int available=possessed_quantity_for_itemid(bridgetdata->itemid,0);
  int quantity=(needed<available)?needed:available;
  
  // Prepare the item's name for display.
  // (start_of_sentence) is always zero for the English strings. I expect that will remain true across languages but who knows.
  char plural[32];
  int pluralc=item_name_contextualize(plural,sizeof(plural),bridgetdata->itemid,needed,0);
  if ((pluralc<0)||(pluralc>sizeof(plural))) pluralc=0;
  
  // Choose the string and prepare format args.
  int strix;
  struct text_insertion insv[3]={0};
  if (first_time) {
    if (quantity) {
      strix=120;
      insv[0]=(struct text_insertion){.mode='i',.i=needed};
      insv[1]=(struct text_insertion){.mode='s',.s={.v=plural,.c=pluralc}};
      insv[2]=(struct text_insertion){.mode='i',.i=quantity};
    } else {
      strix=119;
      insv[0]=(struct text_insertion){.mode='i',.i=needed};
      insv[1]=(struct text_insertion){.mode='s',.s={.v=plural,.c=pluralc}};
    }
  } else {
    if (quantity) {
      strix=122;
      insv[0]=(struct text_insertion){.mode='i',.i=needed};
      insv[1]=(struct text_insertion){.mode='s',.s={.v=plural,.c=pluralc}};
      insv[2]=(struct text_insertion){.mode='i',.i=quantity};
    } else {
      strix=121;
      insv[0]=(struct text_insertion){.mode='i',.i=needed};
      insv[1]=(struct text_insertion){.mode='s',.s={.v=plural,.c=pluralc}};
    }
  }

  // Make a dialogue box.
  bridgetctx.bridgetdata=bridgetdata;
  bridgetctx.quantity=quantity;
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=strix,
    .insv=insv,
    .insc=3,
    .speaker=initiator,
    .cb=bridget_cb,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  if (quantity) {
    modal_dialogue_add_option_string(modal,RID_strings_dialogue,4);
    modal_dialogue_add_option_string(modal,RID_strings_dialogue,5);
  }
}

/* Moon Song.
 */
 
static int moonsong_cb(int optionid,void *userdata) {
  if (optionid!=4) return 0;
  int raceid=(int)(uintptr_t)userdata;
  race_begin(raceid);
  return 0;
}
 
void begin_moonsong(struct sprite *initiator,int arg) {
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=(g.store.invstorev[0].itemid==NS_itemid_broom)?137:138,
    .speaker=initiator,
    .cb=moonsong_cb,
    .userdata=(void*)(uintptr_t)arg,
  };
  if (!store_get_fld(NS_fld_moonsong_intro)) {
    store_set_fld(NS_fld_moonsong_intro,1);
    args.strix=142;
  }
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,4);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,5);
}

void begin_endrace(int arg) {
  g.camera.cut=1; // If we don't cut, camera lingers at the finish line while we speak, then pans after. It's awkward.
  camera_update(0.016);
  int outcome=0,new_high_score=0,first_time=0;
  switch (arg&3) {
    case 1: outcome=1; break;
    case 2: outcome=-1; break;
  }
  if (arg&4) new_high_score=1;
  if (arg&8) first_time=1;
  
  /* Abort if outcome undeclared.
   * If Dot won, check whether this is the final win.
   * races.c manages the per-race flags, but NS_fld_broom_races_complete is our responsibility.
   */
  if (!outcome) return;
  if (outcome>0) {
    if (!store_get_fld(NS_fld_broom_races_complete)) {
      int p=0,alldone=1;
      for (;;p++) {
        int fld=race_fld_by_index(p);
        if (fld<=0) break;
        if (!store_get_fld(fld)) {
          alldone=0;
          break;
        }
      }
      if (alldone) {
        store_set_fld(NS_fld_broom_races_complete,1);
        const struct story *story=story_by_fld_present(NS_fld_broom_races_complete);
        if (story) {
          struct modal_args_cutscene args={
            .strix_title=story->strix_title,
            .context=CUTSCENE_CONTEXT_EXPECTEDISH,
          };
          struct modal *modal=modal_spawn(&modal_type_cutscene,&args,sizeof(args));
          return;
        }
      }
    }
  }
  
  /* Normal reporting, four possible messages.
   */
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
  };
  if (outcome>0) {
    if (g.stopwatch) args.strix=147; // Cheater!
    else if (new_high_score&&!first_time) args.strix=145; // Win and high score.
    else args.strix=143; // Generic win.
  } else {
    if (new_high_score&&!first_time) args.strix=146; // Lose and high score.
    else args.strix=144; // Generic lose.
  }
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
}

/* Run the cutscenes for hearts book or gold book.
 */
 
void begin_hearts_book() {
  struct modal_args_cutscene args={
    .strix_title=14,
    .context=CUTSCENE_CONTEXT_INTERRUPT,
  };
  struct modal *modal=modal_spawn(&modal_type_cutscene,&args,sizeof(args));
}
 
void begin_gold_book() {
  /* Runs when the last purse upgrade is collected.
   * At least one purse upgrade, rescuing the princess, also has its own cutscene, which should take precedence.
   * So check the modal stack for other cutscenes and do nothing if there's already one running.
   */
  if (modal_get_topmost(&modal_type_cutscene)) return;
  struct modal_args_cutscene args={
    .strix_title=15,
    .context=CUTSCENE_CONTEXT_INTERRUPT,
  };
  struct modal *modal=modal_spawn(&modal_type_cutscene,&args,sizeof(args));
}

/* Outcome of zookeeper battles.
 */
 
static void cb_zoo_replay_battle(struct modal *modal,int outcome,void *userdata) {
  struct battle *battle=modal_battle_get_battle(modal);
  if (!battle) return;
  if (outcome>0) {
    struct prize prizev[8];
    int prizec=game_get_prizes(prizev,8,battle->type->id,(const uint8_t*)"\0\0\0\0");
    if (battle->type->get_prizes) {
      prizec+=battle->type->get_prizes(prizev+prizec,8-prizec,battle);
    }
    struct prize *prize=prizev;
    for (;prizec-->0;prize++) {
      game_get_item(prize->itemid,prize->quantity);
      modal_battle_add_consequence(modal,prize->itemid,prize->quantity);
    }
  } else if (outcome<0) {
    // Don't actually hurt the hero until cb_final. Report it first. If it's her last heart, game_hurt_hero would trigger the gameover modal.
    modal_battle_add_consequence(modal,NS_itemid_heart,-1);
  }
}

static void cb_zoo_replay_final(struct modal *modal,int outcome,void *userdata) {
  if (outcome<0) {
    game_hurt_hero();
  }
}

/* Zookeeper offers a contest against one of his captured animals.
 */
 
static int cb_zoo_replay(int optionid,void *userdata) {
  if (optionid<=1) return 0; // Cancelled, or 1 for "No". (sprite:1 is the hero, can't be an animal).
  
  /* Read the sprite resource to get its name and battleid.
   */
  int name_strix=0;
  int battleid=0;
  const void *spriteres;
  int spriteresc=res_get(&spriteres,EGG_TID_sprite,optionid);
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,spriteres,spriteresc)<0) return 0;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_sprite_monster) {
      battleid=(cmd.arg[0]<<8)|cmd.arg[1];
      name_strix=(cmd.arg[4]<<8)|cmd.arg[5];
      break;
    }
  }
  if (!battleid) return 0;
  
  /* Enter battle.
   * Do the same things sprite_monster would do.
   */
  const struct battle_type *type=battle_type_by_id(battleid);
  if (!type) return 0;
  struct modal_args_battle args={
    .battle=battleid,
    .args={
      .difficulty=0x80,
      .bias=0x80,
      .rctl=0,
      .rface=NS_face_monster,
      .lctl=1,
      .lface=NS_face_dot,
    },
    .right_name=name_strix,
    .cb=cb_zoo_replay_battle,
    .cb_final=cb_zoo_replay_final,
  };
  struct modal *modal=modal_spawn(&modal_type_battle,&args,sizeof(args));
  if (!modal) return 0;
  
  return 0;
}
 
void begin_zoo_replay(struct sprite *sprite,int fldid) {
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=117,
    .speaker=sprite,
    .cb=cb_zoo_replay,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string_id(modal,RID_strings_dialogue,5,1);
  int c=zoo_get_count(fldid);
  int i=0;
  for (;i<c;i++) {
    int spriteid=zoo_get_spriteid(fldid+i);
    if (spriteid<1) continue;
    
    // Get the monster's name. Kind of painful...
    int strix=0;
    const void *spriteres;
    int spriteresc=res_get(&spriteres,EGG_TID_sprite,spriteid);
    struct cmdlist_reader reader;
    if (sprite_reader_init(&reader,spriteres,spriteresc)<0) continue;
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      if (cmd.opcode==CMD_sprite_monster) {
        strix=(cmd.arg[4]<<8)|cmd.arg[5];
        break;
      }
    }
    if (!strix) continue;
    
    modal_dialogue_add_option_string_id(modal,RID_strings_battle,strix,spriteid);
  }
}

/* Distance entry for the surveyor contest.
 */
 
static void cb_surveyor_entry(int value,void *userdata) {
  int fld16=(int)(uintptr_t)userdata;
  if (value<0) value=0; else if (value>999) value=999;
  store_set_fld16(fld16+3,value);
  surveyor_check();
}
 
void begin_surveyor_entry(struct sprite *sprite,int fld16) {
  surveyor_require();
  struct modal_args_tenkey args={
    .value=store_get_fld16(fld16+3),
    .cb=cb_surveyor_entry,
    .userdata=(void*)(uintptr_t)fld16,
  };
  struct modal *modal=modal_spawn(&modal_type_tenkey,&args,sizeof(args));
  if (!modal) return;
}

/* Exit door from the labyrinth.
 */

void begin_escape_labyrinth() {
  if (store_get_fld(NS_fld_escaped_labyrinth)) return; // Already done it, all good.
  store_set_fld(NS_fld_escaped_labyrinth,1);
  struct modal_args_cutscene args={
    .strix_title=5,
    .context=CUTSCENE_CONTEXT_INTERRUPT,
  };
  struct modal *modal=modal_spawn(&modal_type_cutscene,&args,sizeof(args));
  if (!modal) return;
}

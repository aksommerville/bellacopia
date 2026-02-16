#include "activity_internal.h"

/* Red Captain.
 */
 
static int cb_capnred_rcv(int optionid,void *userdata) {
  struct invstore *letter=userdata;
  if (letter->itemid==NS_itemid_letter1) { // Unimproved!
    letter->itemid=0;
    begin_dialogue(80,0);
    store_broadcast('i',NS_itemid_letter1,0);
    return 0;
  }
  if (letter->itemid==NS_itemid_letter2) { // Improved.
    letter->itemid=NS_itemid_letter3;
    begin_dialogue(81,0);
    bm_sound(RID_sound_treasure);
    store_broadcast('i',NS_itemid_letter3,0);
    return 1;
  }
  return 0;
}
 
void begin_capnred(struct sprite *sprite) {
  // Done?
  if (store_get_fld(NS_fld_war_over)) {
    begin_dialogue(83,sprite);
    return;
  }
  // Carrying one of my letters?
  if (store_get_itemid(NS_itemid_letter3)||store_get_itemid(NS_itemid_letter4)) {
    begin_dialogue(82,sprite);
    return;
  }
  // Bringing a Blue letter to me?
  struct invstore *letter=store_get_itemid(NS_itemid_letter1);
  if (!letter) letter=store_get_itemid(NS_itemid_letter2);
  if (letter) {
    struct modal *modal=begin_dialogue(75,sprite);
    if (!modal) return;
    modal_dialogue_set_callback(modal,cb_capnred_rcv,letter);
    return;
  }
  // The quest starts with Capn Blue, what do you want with me?
  begin_dialogue(79,sprite);
}

/* Blue Captain.
 */
 
static int cb_capnblue_rcv(int optionid,void *userdata) {
  struct invstore *letter=userdata;
  if (letter->itemid==NS_itemid_letter3) { // Unimproved!
    letter->itemid=0;
    begin_dialogue(76,0);
    store_broadcast('i',NS_itemid_letter3,0);
    return 0;
  }
  if (letter->itemid==NS_itemid_letter4) { // Improved.
    letter->itemid=0;
    begin_dialogue(77,0);
    store_set_fld(NS_fld_war_over,1);
    bm_sound(RID_sound_secret);
    store_broadcast('i',NS_itemid_letter4,0);
    return 1;
  }
  return 0;
}

static int cb_capnblue_start(int optionid,void *userdata) {
  game_get_item(NS_itemid_letter1,0);
  return 1;
}
 
void begin_capnblue(struct sprite *sprite) {
  // Done?
  if (store_get_fld(NS_fld_war_over)) {
    begin_dialogue(78,sprite);
    return;
  }
  // Carrying one of my letters?
  if (store_get_itemid(NS_itemid_letter1)||store_get_itemid(NS_itemid_letter2)) {
    begin_dialogue(74,sprite);
    return;
  }
  // Bringing a Red letter to me?
  struct invstore *letter=store_get_itemid(NS_itemid_letter3);
  if (!letter) letter=store_get_itemid(NS_itemid_letter4);
  if (letter) {
    struct modal *modal=begin_dialogue(75,sprite);
    if (!modal) return;
    modal_dialogue_set_callback(modal,cb_capnblue_rcv,letter);
    return;
  }
  // Give Dot the first letter.
  struct modal *modal=begin_dialogue(73,sprite);
  if (!modal) return;
  modal_dialogue_set_callback(modal,cb_capnblue_start,0);
}

/* Poet.
 */
 
static int cb_poet_improved(int optionid,void *userdata) {
  struct invstore *invstore=userdata;
  if (invstore->itemid==NS_itemid_letter1) invstore->itemid=NS_itemid_letter2;
  else if (invstore->itemid==NS_itemid_letter3) invstore->itemid=NS_itemid_letter4;
    store_broadcast('i',invstore->itemid,0);
  bm_sound(RID_sound_treasure);
  return 1;
}
 
void begin_poet(struct sprite *sprite) {
  // Done?
  if (store_get_fld(NS_fld_war_over)) {
    begin_dialogue(72,sprite);
    return;
  }
  // Got an improved letter?
  if (store_get_itemid(NS_itemid_letter2)||store_get_itemid(NS_itemid_letter4)) {
    begin_dialogue(71,sprite);
    return;
  }
  // Got a letter we can improve?
  struct invstore *letter=store_get_itemid(NS_itemid_letter1);
  if (!letter) letter=store_get_itemid(NS_itemid_letter3);
  if (letter) {
    struct modal *modal=begin_dialogue(70,sprite);
    if (!modal) return;
    modal_dialogue_set_callback(modal,cb_poet_improved,letter);
    return;
  }
  // Say anything.
  begin_dialogue(69,sprite);
}

#include "game/bellacopia.h"

/* Nonzero if any field in the given inclusive range is unset.
 */
 
static int any_fld_unset(int a,int z) {
  for (;a<=z;a++) {
    if (!store_get_fld(a)) return 1;
  }
  return 0;
}
 
static int any_fld_set(int a,int z) {
  for (;a<=z;a++) {
    if (store_get_fld(a)) return 1;
  }
  return 0;
}

/* General context-sensitive advice, for crystal ball.
 */

// RID_strings_advice. Never fails to return something sensible.
static int game_get_advice_strix() {

  /* Early on, prefer to point her toward the Root Devils.
   * Other advice is interleaved here, as the preferred path warrants.
   */
  if (!store_get_fld(NS_fld_root1)) return 2; // Meadow.
  if (!store_get_itemid(NS_itemid_divining)) return 3; // Get the divining rod.
  if (!store_get_fld(NS_fld_war_over)) return 4; // End the war.
  if (!store_get_fld(NS_fld_root5)) return 5; // You ended the war but forgot the root devil!
  // Should we guide her toward endorsements, or is that obvious enough?
  if (!store_get_fld(NS_fld_mayor)) return 6; // Run for mayor.
  if (!store_get_fld(NS_fld_root2)) return 7; // You moved the log but forgot the root devil!
  if (!store_get_fld(NS_fld_root3)) return 8; // Get the south-jungle root devil.
  if (!store_get_fld(NS_fld_root7)) return 9; // Get the east-desert root devil.
  if (store_get_fld(NS_fld_kidnapped)&&!store_get_fld(NS_fld_escaped)) return 10; // Escape from the goblins!
  if (!store_get_fld(NS_fld_root6)) return 11; // Root devil by the goblins' cave.
  if (!store_get_fld(NS_fld_root4)) { // Temple root devil.
    int statuemaze=store_get_fld16(NS_fld16_statuemaze_seed);
    int labyrinth=store_get_fld16(NS_fld16_labyrinth_seed);
    if (!statuemaze) return 12; // Statue maze not visited. Presumably they haven't entered the temple at all.
    if (!labyrinth) return 13; // Labyrinth not visited. Presumably they're stuck on the statue maze.
    return 12; // She's been into the labyrinth. Probably failed to complete it. Go back to the generic "there's a root devil in the temple."
  }
  
  /* All the root devils are strangled, so now we're in the weird position of suggesting a next step when there's really no linear order.
   * (not that there was a linear order to the root devils either).
   * This is going to be opinionated and imprecise any way we slice it.
   * I guess interleave the various kinds of target. So we'll suggest a heart container, then a zookeeper, then a bridge, keep it fresh.
   * Don't advise on buried treasure or jigpieces. There are other ways to hint toward those.
   */
  int have_broom=(store_get_itemid(NS_itemid_broom)?1:0);
  if (!store_get_fld(NS_fld_hc1)) return 14; // Castleshop heart container.
  if (!store_get_fld(NS_fld_rescued_princess)) return 15; // Rescue the Princess!
  if (have_broom&&!store_get_fld(NS_fld_race1win)) return 69; // Cheapside race.
  if (!store_get_fld(NS_fld_barrelhat1)||!store_get_fld(NS_fld_barrelhat2)||!store_get_fld(NS_fld_barrelhat4)) return 16; // Botire barrel hats.
  if (have_broom&&!store_get_fld(NS_fld_race2win)) return 70; // Downstairs lake race.
  if (!store_get_fld(NS_fld_potion_book)) return 17; // Buy the potions book.
  if (have_broom&&!store_get_fld(NS_fld_race3win)) return 71; // Tundra race.
  if (!store_get_fld(NS_fld_bridge1done)) return 18; // Stick bridge by forest.
  if (have_broom&&!store_get_fld(NS_fld_race4win)) return 72; // Botire race.
  if (any_fld_unset(NS_fld_zoo1_0,NS_fld_zoo1_3)) return 19; // Meadow zookeeper.
  if (have_broom&&!store_get_fld(NS_fld_race5win)) return 73; // Desert race.
  if (!store_get_itemid(NS_itemid_fishpole)) return 38; // Recommend fishpole before recommending anything underwater!
  if (!store_get_fld(NS_fld_hc2)) return 20; // Temple pool heart container.
  if (have_broom&&!store_get_fld(NS_fld_race6win)) return 74; // North underground race.
  if (!store_get_fld(NS_fld_barrelhat3)||!store_get_fld(NS_fld_barrelhat7)) return 21; // Tundra barrels. They're not close to each other but whatever.
  if (!store_get_fld(NS_fld_bridge6done)||!store_get_fld(NS_fld_bridge7done)) return 22; // Telescope and Green Fish bridges off Botire.
  if (!store_get_fld(NS_fld_hc4)) return 23; // South jungle heart container.
  if (!store_get_fld(NS_fld_barrelhat5)) return 24; // Cheapside barrel hat.
  if (!store_get_fld(NS_fld_bridge3done)||!store_get_fld(NS_fld_bridge4done)) return 25; // Two bridges off the east river (candy and pepper).
  if (!store_get_fld(NS_fld_barrelhat6)||!store_get_fld(NS_fld_barrelhat8)) return 26; // Fractia barrels.
  if (!store_get_fld(NS_fld_bridge5done)) return 27; // Gold bridge.
  if (!store_get_fld(NS_fld_barrelhat9)) return 28; // Castle barrel.
  if (!store_get_fld(NS_fld_bridge2done)) return 29; // Match bridge.

  /* Goblins' cave.
   */
  if (!store_get_fld(NS_fld_stardoor)) {
    if (!store_get_fld(NS_fld_started_crypto)) return 68; // Seek the goblins' treasure!
    int gotbone=store_get_fld(NS_fld_bonedoor);
    int gotleaf=store_get_fld(NS_fld_leafdoor);
    if (!gotbone&&!gotleaf) {
      if (!store_get_fld(NS_fld_bought_alphabet)&&!store_get_fld(NS_fld_bought_translation)) return 30; // Ask linguist.
    }
    if (!gotbone||!gotleaf) return 31; // Open the first two doors.
    return 32; // Stuck after the first two doors.
  }
  
  /* Any item missing, say something about it.
   * Only when all of the final items are acquired, tell them about the inventory critic.
   * Some of these messages will be unlikely or impossible, due to other things we've advised. (eg you've built the match bridge, so you do have match already).
   */
  {
    int gotall=1; // Only proceed with the per-item checks if there's an open slot.
    const struct invstore *invstore=g.store.invstorev;
    int i=26;
    for (;i-->0;invstore++) {
      if (!invstore->itemid) {
        gotall=0;
        break;
      }
    }
    if (!gotall) {
      const struct itemtag {
        int itemid;
        int strix;
      } itemtagv[26]={
        {NS_itemid_stick        ,33},
        {NS_itemid_broom        ,34},
        {NS_itemid_divining     ,3}, // We advised for this already; listing again for the sake of completeness.
        {NS_itemid_match        ,36},
        {NS_itemid_wand         ,37},
        {NS_itemid_fishpole     ,38},
        {NS_itemid_bugspray     ,39},
        {NS_itemid_potion       ,40},
        {NS_itemid_hookshot     ,41},
        {NS_itemid_candy        ,42},
        {NS_itemid_magnifier    ,43},
        {NS_itemid_vanishing    ,44},
        {NS_itemid_compass      ,45},
        {NS_itemid_bell         ,46},
        {NS_itemid_telescope    ,47},
        {NS_itemid_shovel       ,48},
        {NS_itemid_pepper       ,49},
        {NS_itemid_bomb         ,50},
        {NS_itemid_stopwatch    ,51},
        {NS_itemid_busstop      ,52},
        {NS_itemid_snowglobe    ,53},
        {NS_itemid_tapemeasure  ,54},
        {NS_itemid_phonograph   ,55},
        {NS_itemid_crystal      ,56},
        {NS_itemid_glove        ,57},
        {NS_itemid_marionette   ,58},
      };
      const struct itemtag *itemtag=itemtagv;
      for (i=26;i-->0;itemtag++) {
        if (store_get_itemid(itemtag->itemid)) continue;
        return itemtag->strix;
      }
    }
  }
  if (!store_get_fld(NS_fld_hc3)) return 59; // Inventory critic.
  
  /* If any story is got but untold, hint at one of the unsatisfied trees.
   */
  {
    int i=0,have_story=0;
    for (;;i++) {
      const struct story *story=story_by_index(i);
      if (!story) break;
      if (store_get_fld(story->fld_told)) continue;
      if (store_get_fld(story->fld_present)) {
        have_story=1;
        break;
      }
    }
    if (have_story) {
      if (any_fld_unset(NS_fld_tree1,NS_fld_tree7)) return 60; // Forest.
      if (any_fld_unset(NS_fld_tree13,NS_fld_tree16)) return 61; // Tundra.
      if (any_fld_unset(NS_fld_tree8,NS_fld_tree9)) return 62; // Battlefield.
      if (!store_get_fld(NS_fld_tree10)) return 63; // Jungle.
      if (!store_get_fld(NS_fld_tree11)) return 64; // Desert.
      if (!store_get_fld(NS_fld_tree12)) return 65; // Mountains.
    }
  }
  
  /* If the maps are incomplete, give a generic complaint.
   * We don't guide to specific jigpieces; the whole idea of the jigsaw puzzle map is it guides you to missing pieces implicitly.
   */
  if (!jigstore_is_complete()) return 66;
  
  /* If the cartographer has further advice to give, recommend asking him.
   */
  if (cartographer_has_advice()) return 67;
  
  return 1; // "You did everything!"
}

int game_get_crystal_ball_advice(char *dst,int dsta) {
  if (!dst||(dsta<0)) dsta=0;
  int strix=game_get_advice_strix();
  return text_format_res(dst,dsta,RID_strings_advice,strix,0,0);
}

/* Advice from the Princess.
 */
 
static int game_get_gossip_strix() {

  /* Same idea as game_get_advice_strix(), but different content and not as exhaustive.
   */
  if (!store_get_fld(NS_fld_mayor)) return 110; // You should run for mayor.
  if (!store_get_fld(NS_fld_root2)) return 111; // You cleared the log, but didn't strangle the root devil behind it!
  if (!store_get_fld(NS_fld_root6)) return 112; // Root devil by the goblins' cave.
  if (!store_get_fld(NS_fld_hc1)) return 113; // Buy the heart container, right outside.
  if (!store_get_fld(NS_fld_war_over)) return 114; // End the war.
  if (!store_get_fld(NS_fld_barrelhat_all)) {
    if (any_fld_set(NS_fld_barrelhat1,NS_fld_barrelhat9)) return 115; // Continue hatting barrels.
    return 116; // Start hatting barrels.
  }
  if (!store_get_fld(NS_fld_purse2)) return 117; // Grandpa's puzzle, Fractia.
  if (any_fld_unset(NS_fld_bridge1done,NS_fld_bridge7done)) return 118; // Bridget needs your help.
  if (!store_get_fld(NS_fld_stardoor)) return 119; // Go back to the cave and steal their secret treasure.
  if (!store_get_fld(NS_fld_minesweep1)||!store_get_fld(NS_fld_minesweep2)) return 120; // Sweep the mines!
  if (!store_get_fld(NS_fld_surveyor_complete)) {
    if (!store_get_itemid(NS_itemid_tapemeasure)) return 121; // Seek the treasure (tape measure)
    return 122; // Reunite the rabbits.
  }
  if (!store_get_fld(NS_fld_purse1)) return 123; // There's more stuff in the temple. (implicitly covers seamonster, powerglove, seasonblocks, sphinx)
  
  /* Any story trees outstanding?
   */
  {
    if (!store_get_fld(NS_fld_story4)) return 124; // Tell a tree about our adventure!
    int i=0,have_story=0;
    for (;;i++) {
      const struct story *story=story_by_index(i);
      if (!story) break;
      if (store_get_fld(story->fld_told)) continue;
      if (store_get_fld(story->fld_present)) {
        have_story=1;
        break;
      }
    }
    if (have_story) return 125; // You should tell a story to a tree.
  }
  
  /* She won't tell you where to look for buried treasure, but she will tell you who to ask.
   * These are very optional. So exhaust all other advice first.
   */
  if (any_fld_unset(NS_fld_bt1,NS_fld_bt4)||!store_get_fld(NS_fld_bt5)||any_fld_unset(NS_fld_bt6,NS_fld_bt16)) {
    if (!store_get_fld(NS_fld_target_gold)) return 126; // Upgrade your compass to find buried treasure.
    return 127; // There's buried treasure somewhere!
  }

  /* When there's nothing else to say, spout some random blather.
   * She never shuts up.
   */
  return 100+rand()%4;
}
 
int game_get_gossip(char *dst,int dsta) {
  if (!dst||(dsta<0)) dsta=0;
  int strix=game_get_gossip_strix();
  return text_format_res(dst,dsta,RID_strings_advice,strix,0,0);
}

#include "bellacopia.h"

/* Field lists.
 */
 
static const int fldv_flowers[]={
  NS_fld_root1,NS_fld_root2,NS_fld_root3,NS_fld_root4,NS_fld_root5,NS_fld_root6,NS_fld_root7,
};
static const int fldv_side_quest[]={
  //NS_fld_toll_paid, // This gates root3 but it's very easy to get around, shouldn't call it mandatory.
  NS_fld_mayor,
  NS_fld_war_over,
  NS_fld_rescued_princess,
};
static const int bucketfldv_side_quest[]={
  NS_fld_barrelhat1,NS_fld_barrelhat2,NS_fld_barrelhat3,NS_fld_barrelhat4,NS_fld_barrelhat5,NS_fld_barrelhat6,NS_fld_barrelhat7,NS_fld_barrelhat8,NS_fld_barrelhat9,0,
};
static const int fldv_heart_container[]={
  NS_fld_hc1,NS_fld_hc2,NS_fld_hc3,NS_fld_hc4,
  //NS_fld_hc5,//TODO not placed yet
};
static const int fldv_buried_treasure[]={
  NS_fld_bt1,NS_fld_bt2,NS_fld_bt3,NS_fld_bt4,
  // bt5 is a jigpiece, doesn't count.
  NS_fld_bt6,NS_fld_bt7,NS_fld_bt8,NS_fld_bt9,NS_fld_bt10,
  NS_fld_bt11,NS_fld_bt12,NS_fld_bt13,NS_fld_bt14,NS_fld_bt15,
};
static const int fldv_story_tree[]={
  NS_fld_tree1,NS_fld_tree2,NS_fld_tree3,NS_fld_tree4,NS_fld_tree5,NS_fld_tree6,NS_fld_tree7,NS_fld_tree8,
  NS_fld_tree9,NS_fld_tree10,NS_fld_tree11,NS_fld_tree12,NS_fld_tree13,NS_fld_tree14,NS_fld_tree15,NS_fld_tree16,
};
static const int fldv_zookeeper[]={ // Zookeeper bits count individually, tho we could aggregate if we feel like it.
  NS_fld_zoo1_0,NS_fld_zoo1_1,NS_fld_zoo1_2,NS_fld_zoo1_3,
  NS_fld_zoo2_0,NS_fld_zoo2_1,NS_fld_zoo2_2,NS_fld_zoo2_3,
  NS_fld_zoo3_0,NS_fld_zoo3_1,NS_fld_zoo3_2,NS_fld_zoo3_3,
  NS_fld_zoo4_0,NS_fld_zoo4_1,NS_fld_zoo4_2,NS_fld_zoo4_3,
  NS_fld_zoo5_0,NS_fld_zoo5_1,NS_fld_zoo5_2,NS_fld_zoo5_3,
  NS_fld_zoo6_0,NS_fld_zoo6_1,NS_fld_zoo6_2,NS_fld_zoo6_3,
  NS_fld_zoo7_0,NS_fld_zoo7_1,NS_fld_zoo7_2,NS_fld_zoo7_3,
  NS_fld_zoo8_0,NS_fld_zoo8_1,NS_fld_zoo8_2,NS_fld_zoo8_3,
  NS_fld_zoo9_0,NS_fld_zoo9_1,NS_fld_zoo9_2,NS_fld_zoo9_3,
  NS_fld_zoo10_0,NS_fld_zoo10_1,NS_fld_zoo10_2,NS_fld_zoo10_3,
  NS_fld_zoo11_0,NS_fld_zoo11_1,NS_fld_zoo11_2,NS_fld_zoo11_3,
  NS_fld_zoo12_0,NS_fld_zoo12_1,NS_fld_zoo12_2,NS_fld_zoo12_3,
  NS_fld_zoo13_0,NS_fld_zoo13_1,NS_fld_zoo13_2,NS_fld_zoo13_3,
  NS_fld_zoo14_0,NS_fld_zoo14_1,NS_fld_zoo14_2,NS_fld_zoo14_3,
  NS_fld_zoo15_0,NS_fld_zoo15_1,NS_fld_zoo15_2,NS_fld_zoo15_3,
  NS_fld_zoo16_0,NS_fld_zoo16_1,NS_fld_zoo16_2,NS_fld_zoo16_3,
  NS_fld_zoo17_0,NS_fld_zoo17_1,NS_fld_zoo17_2,NS_fld_zoo17_3,
};
static const int fldv_bridge[]={
  NS_fld_bridge1done,NS_fld_bridge2done,NS_fld_bridge3done,NS_fld_bridge4done,NS_fld_bridge5done,NS_fld_bridge6done,NS_fld_bridge7done,
};
static const int fldv_broomrace[]={
  NS_fld_race1win,NS_fld_race2win,NS_fld_race3win,NS_fld_race4win,NS_fld_race5win,NS_fld_race6win,
};

/* Digest field lists.
 */
 
static int fldv_count(int *a,const int *v,int c) {
  if (a) (*a)+=c;
  int result=0;
  for (;c-->0;v++) if (store_get_fld(*v)) result++;
  return result;
}
static int bucketfldv_count(int *a,const int *v,int c) { // Buckets delimited by zero.
  int result=0;
  while (c>0) {
    int result1=1;
    while ((c>0)&&*v) {
      if (!store_get_fld(*v)) result1=0;
      v++; c--;
    }
    v++; c--;
    if (result1) result++;
    if (a) (*a)++;
  }
  return result;
}
#define FLDV_COUNT(a,name) fldv_count(a,name,sizeof(name)/sizeof(int))
#define BUCKETFLDV_COUNT(a,name) bucketfldv_count(a,name,sizeof(name)/sizeof(int))

/* Count flowers.
 */
 
int bm_count_flowers() {
  return FLDV_COUNT(0,fldv_flowers);
}

/* Get the simple digest of completion.
 */
 
int game_get_completion() {
  int n,d=0;
  
  // Any flowers unset, the main quest is incomplete so our answer is 0.
  n=FLDV_COUNT(&d,fldv_flowers); if (n<d) return 0;
  
  // Check all the other completable things. 1 if anything missing. Start with cheaper tests.
  d=0; n=FLDV_COUNT(&d,fldv_side_quest); if (n<d) return 1;
  d=0; n=BUCKETFLDV_COUNT(&d,bucketfldv_side_quest); if (n<d) return 1;
  d=0; n=FLDV_COUNT(&d,fldv_heart_container); if (n<d) return 1;
  d=0; n=FLDV_COUNT(&d,fldv_buried_treasure); if (n<d) return 1;
  d=0; n=FLDV_COUNT(&d,fldv_story_tree); if (n<d) return 1;
  d=0; n=FLDV_COUNT(&d,fldv_zookeeper); if (n<d) return 1;
  d=0; n=FLDV_COUNT(&d,fldv_bridge); if (n<d) return 1;
  d=0; n=FLDV_COUNT(&d,fldv_broomrace); if (n<d) return 1;
  
  // Any empty inventory slot, we're incomplete.
  const struct invstore *invstore=g.store.invstorev;
  for (n=INVSTORE_SIZE;n-->0;invstore++) {
    if (!invstore->itemid) return 1;
  }
  // With inventory full, check if any is intermediate. Separate from the first pass because this lookup has some cost.
  for (n=INVSTORE_SIZE,invstore=g.store.invstorev;n-->0;invstore++) {
    const struct item_detail *detail=item_detail_for_itemid(invstore->itemid);
    if (!detail||detail->intermediate) return 1;
  }
  
  // Reading jigstore is potentially expensive. We do cache the result, but it clears a lot.
  if (!jigstore_is_complete()) return 1;
  
  // OK, it's fully complete!
  return 2;
}

/* Test for minimalist completion.
 */
 
int game_is_minimalist_pending() {
  int n,d;
  
  // There's a special flag to disqualify the minimalist prize. In normal play, this should get set pretty fast.
  if (store_get_fld(NS_fld_minimalist_disqualify)) return 0;
  
  // Check all the other completable things. Anything at all* present, our answer is 0.
  // [*] Except some things.
  const struct invstore *invstore=g.store.invstorev;
  for (n=INVSTORE_SIZE;n-->0;invstore++) {
    if (!invstore->itemid) continue;
    if (invstore->itemid==NS_itemid_stick) continue; // You're allowed to have a stick, since they're hard to avoid. You're not allowed to use it.
    return 0; // Any other item is a violation.
  }
  if (FLDV_COUNT(0,fldv_side_quest)) return 0;
  if (BUCKETFLDV_COUNT(0,bucketfldv_side_quest)) return 0;
  if (FLDV_COUNT(0,fldv_heart_container)) return 0;
  if (FLDV_COUNT(0,fldv_buried_treasure)) return 0;
  if (FLDV_COUNT(0,fldv_story_tree)) return 0;
  if (FLDV_COUNT(0,fldv_zookeeper)) return 0;
  if (FLDV_COUNT(0,fldv_bridge)) return 0;
  if (FLDV_COUNT(0,fldv_broomrace)) return 0; // uhhhh we already know you don't have a broom, so this would be weird if set
  
  // You're allowed to pick up jigpieces. You're *not* allowed to touch them; that should set minimalist_disqualify.
  //if (jigstore_has_anything()) return 0;
  
  // OK, looking good!
  return 1;
}

int game_is_minimalist_complete() {
  int n,d;
  
  // Any flowers unset, the main quest is incomplete so our answer is 0.
  n=FLDV_COUNT(&d,fldv_flowers); if (n<d) return 0;
  
  // Beyond that, confirm the same things as "pending".
  return game_is_minimalist_pending();
}

/* Measure completion.
 */
 
int game_get_completables(struct completable *dst,int dsta) {
  int dstc=0;
  
  // Flowers.
  if (dstc<dsta) {
    struct completable *comp=dst+dstc;
    comp->strix=30;
    comp->denom=0;
    comp->numer=FLDV_COUNT(&comp->denom,fldv_flowers);
  }
  dstc++;
  
  // Side Quests.
  if (dstc<dsta) {
    struct completable *comp=dst+dstc;
    comp->strix=33;
    comp->denom=0;
    comp->numer=FLDV_COUNT(&comp->denom,fldv_side_quest);
    comp->numer+=BUCKETFLDV_COUNT(&comp->denom,bucketfldv_side_quest);
  }
  dstc++;
  
  // Items.
  if (dstc<dsta) {
    struct completable *comp=dst+dstc;
    comp->strix=34;
    comp->numer=0;
    comp->denom=INVSTORE_SIZE;
    const struct invstore *invstore=g.store.invstorev;
    int i=INVSTORE_SIZE;
    for (;i-->0;invstore++) {
      if (invstore->itemid) {
        const struct item_detail *detail=item_detail_for_itemid(invstore->itemid);
        if (detail&&detail->intermediate) continue; // Certain odd items can be in inventory but don't count toward completion.
        // Don't bother checking quantity. We only care whether the slot is apportioned, it's ok if you've run out.
        comp->numer++;
      }
    }
  }
  dstc++;
  
  // Maps.
  if (dstc<dsta) {
    struct completable *comp=dst+dstc;
    comp->strix=35;
    struct jigstore_progress progress;
    jigstore_progress_tabulate(&progress);
    comp->numer=progress.piecec_got;
    comp->denom=progress.piecec_total;
    comp->numer+=(progress.finished?1:0);
    comp->denom+=1;
  }
  dstc++;
  
  // Heart Containers.
  if (dstc<dsta) {
    struct completable *comp=dst+dstc;
    comp->strix=36;
    comp->denom=0;
    comp->numer=FLDV_COUNT(&comp->denom,fldv_heart_container);
  }
  dstc++;
  
  // Buried Treasure.
  if (dstc<dsta) {
    struct completable *comp=dst+dstc;
    comp->strix=37;
    comp->denom=0;
    comp->numer=FLDV_COUNT(&comp->denom,fldv_buried_treasure);
  }
  dstc++;
  
  // Tree Stories.
  if (dstc<dsta) {
    struct completable *comp=dst+dstc;
    comp->strix=38;
    comp->denom=0;
    comp->numer=FLDV_COUNT(&comp->denom,fldv_story_tree);
  }
  dstc++;
  
  // Zookeepers.
  if (dstc<dsta) {
    struct completable *comp=dst+dstc;
    comp->strix=41;
    comp->denom=0;
    comp->numer=FLDV_COUNT(&comp->denom,fldv_zookeeper);
  }
  dstc++;
  
  // Bridges.
  if (dstc<dsta) {
    struct completable *comp=dst+dstc;
    comp->strix=42;
    comp->denom=0;
    comp->numer=FLDV_COUNT(&comp->denom,fldv_bridge);
  }
  dstc++;
  
  // Broom races.
  if (dstc<dsta) {
    struct completable *comp=dst+dstc;
    comp->strix=43;
    comp->denom=0;
    comp->numer=FLDV_COUNT(&comp->denom,fldv_broomrace);
  }
  dstc++;
  
  return dstc;
}

/* Tally completables.
 */
 
int completables_total(struct completable *dst,const struct completable *src,int srcc) {
  int numer=0;
  int denom=0;
  for (;srcc-->0;src++) {
    numer+=src->numer;
    denom+=src->denom;
  }
  if (dst) {
    dst->numer=numer;
    dst->denom=denom;
  }
  int pct=0;
  if (denom>0) {
    if (numer<=0) {
      pct=0;
    } else if (numer>=denom) {
      pct=100;
    } else {
      pct=(numer*100)/denom;
      if (pct<1) pct=1;
      else if (pct>99) pct=99;
    }
  }
  return pct;
}

/* Outer world song, depending on context.
 * We'll change the song each time you bloom a new flower.
 */
 
int bm_song_for_outerworld() {
  // If Dot has the Phonograph and has used it, it overrides the default.
  int phonograph=store_get_fld16(NS_fld16_phonograph);
  if (phonograph) return phonograph;
  // Otherwise, it depends how many Root Devils have been strangled. Order doesn't matter.
  int flowerc=bm_count_flowers();
  switch (flowerc) {
    // Ordered roughly by intensity, but the last one is special.
    case 0: return RID_song_pretty_pretty_pickle;
    case 1: return RID_song_ladder_to_nowhere;
    case 2: return RID_song_barrel_of_salt;
    case 3: return RID_song_tripping_over_my_tongue;
    case 4: return RID_song_dust_mote;
    case 5: return RID_song_feet_to_the_fire;
    case 6: return RID_song_crawling_chaos;
    case 7: return RID_song_smoke_and_mirrors; // Calm, long, and high-quality: What plays forever after you've won.
  }
  // And our universal fallback (won't happen) is Barrel of Salt.
  return RID_song_barrel_of_salt;
}

/* List available outerworld songs.
 */
 
int bm_get_available_songs(struct song_name_and_rid *dstv,int dsta) {
  int dstc=0;
  int flowerc=bm_count_flowers();
  #define AVAILABLE(strix,tag) { \
    if (dstc<dsta) dstv[dstc]=(struct song_name_and_rid){strix,RID_song_##tag}; \
    dstc++; \
  }
  AVAILABLE(86,pretty_pretty_pickle)
  if (flowerc>=1) AVAILABLE(92,ladder_to_nowhere)
  if (flowerc>=2) AVAILABLE(87,barrel_of_salt)
  if (flowerc>=3) AVAILABLE(89,tripping_over_my_tongue)
  if (flowerc>=4) AVAILABLE(90,dust_mote)
  if (flowerc>=5) AVAILABLE(88,feet_to_the_fire)
  if (flowerc>=6) AVAILABLE(93,crawling_chaos)
  if (flowerc>=7) AVAILABLE(91,smoke_and_mirrors)
  #undef AVAILABLE
  return dstc;
}

/* Test some specific completion things dependent on our metadata here.
 */
 
int last_heart_container_was_collected() {
  if (store_get_fld(NS_fld_hearts_book)) return 0; // Already done, will always be zero now.
  int a=0;
  int c=FLDV_COUNT(&a,fldv_heart_container);
  if (c>=a) return 1;
  return 0;
}

int last_gold_upgrade_was_collected() {
  if (store_get_fld(NS_fld_gold_book)) return 0; // Already done, will always be zero now.
  // We'll drive this one just by the scalar purse size, as opposed to the flags associated with those purse upgrades.
  int goldmax=store_get_fld16(NS_fld16_goldmax);
  if (goldmax>=199) return 1; // Got all the purse upgrades. Keep this up to date.
  return 0;
}

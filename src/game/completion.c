#include "bellacopia.h"

/* Field lists.
 */
 
static const int fldv_flowers[]={
  NS_fld_root1,NS_fld_root2,NS_fld_root3,NS_fld_root4,NS_fld_root5,NS_fld_root6,NS_fld_root7,
};
static const int fldv_side_quest[]={
  NS_fld_toll_paid,
  NS_fld_mayor,
  NS_fld_war_over,
  NS_fld_rescued_princess,
};
static const int bucketfldv_side_quest[]={
  NS_fld_barrelhat1,NS_fld_barrelhat2,NS_fld_barrelhat3,NS_fld_barrelhat4,NS_fld_barrelhat5,NS_fld_barrelhat6,NS_fld_barrelhat7,NS_fld_barrelhat8,NS_fld_barrelhat9,0,
};
static const int fldv_heart_container[]={
  NS_fld_hc1,NS_fld_hc2,NS_fld_hc3,
  //NS_fld_hc4,NS_fld_hc5,//TODO not placed yet
};
static const int fldv_buried_treasure[]={
  NS_fld_bt1,NS_fld_bt2,NS_fld_bt3,NS_fld_bt4,
};
static const int fldv_story_tree[]={
  NS_fld_tree1,NS_fld_tree2,NS_fld_tree3,NS_fld_tree4,NS_fld_tree5,NS_fld_tree6,NS_fld_tree7,NS_fld_tree8,
  NS_fld_tree9,NS_fld_tree10,NS_fld_tree11,NS_fld_tree12,NS_fld_tree13,NS_fld_tree14,NS_fld_tree15,NS_fld_tree16,
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
      if (invstore->itemid) comp->numer++; // We only care whether it has been got, not whether there's any quantity.
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

/* Measure completion.XXX
 */
 
int game_get_completion() {
fprintf(stderr,"%s:%d:%s: XXX Don't use this anymore.\n",__FILE__,__LINE__,__func__);
return 0;
/*XXX
  int numer=0,denom=0;
  
  { // Every field in (progress_fldv) must be set.
    const int *fld=progress_fldv;
    int i=sizeof(progress_fldv)/sizeof(int);
    for (;i-->0;fld++) {
      if (store_get_fld(*fld)) numer++;
      denom++;
    }
  }
  
  { // Count inventory. I'll design things such that every slot can be filled exactly (that's not true yet, 2026-01-29)
    // Do not just count inventoriables in item_detailv. That includes transient things like barrelhat and letter.
    const struct invstore *invstore=g.store.invstorev;
    int i=INVSTORE_SIZE;
    for (;i-->0;invstore++) {
      if (invstore->itemid) numer++;
    }
    denom+=INVSTORE_SIZE;
  }
  
  { // Maps are good to go. Store does the work.
    struct jigstore_progress progress;
    jigstore_progress_tabulate(&progress);
    numer+=progress.piecec_got;
    denom+=progress.piecec_total;
    numer+=(progress.finished?1:0);
    denom+=1;
  }
  
  if ((numer<1)||(denom<1)) return 0;
  if (numer>=denom) return 100;
  int pct=(numer*100)/denom;
  if (pct<1) return 1;
  if (pct>99) return 99;
  return pct;
  */
}

void game_get_sidequests(int *done,int *total) {
fprintf(stderr,"%s:%d:%s: XXX Don't use this anymore.\n",__FILE__,__LINE__,__func__);
/*XXX
  *done=*total=0;
  
  // Most side quests are defined by one field.
  const int *fld=sidequest_fldv;
  int i=sizeof(sidequest_fldv)/sizeof(int);
  for (;i-->0;fld++) {
    if (store_get_fld(*fld)) (*done)++;
    (*total)++;
  }
  
  // barrelhat1..barrelhat9 comprise one quest. All must be set.
  (*total)++;
  if (
    store_get_fld(NS_fld_barrelhat1)&&
    store_get_fld(NS_fld_barrelhat2)&&
    store_get_fld(NS_fld_barrelhat3)&&
    store_get_fld(NS_fld_barrelhat4)&&
    store_get_fld(NS_fld_barrelhat5)&&
    store_get_fld(NS_fld_barrelhat6)&&
    store_get_fld(NS_fld_barrelhat7)&&
    store_get_fld(NS_fld_barrelhat8)&&
    store_get_fld(NS_fld_barrelhat9)
  ) (*done)++;
  /**/
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
    case 0: return RID_song_pretty_pretty_pickle;
    case 1: return RID_song_barrel_of_salt;
    case 2: return RID_song_feet_to_the_fire;//XXX not yet
    case 3: return RID_song_tripping_over_my_tongue;
    case 4: return RID_song_barrel_of_salt;//TODO
    case 5: return RID_song_barrel_of_salt;//TODO
    case 6: return RID_song_barrel_of_salt;//TODO
    case 7: return RID_song_barrel_of_salt;//TODO
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
  if (flowerc>=1) AVAILABLE(87,barrel_of_salt)
  if (flowerc>=2) AVAILABLE(88,feet_to_the_fire)
  if (flowerc>=3) AVAILABLE(89,tripping_over_my_tongue)
  if (flowerc>=4) AVAILABLE(90,barrel_of_salt)//TODO
  if (flowerc>=5) AVAILABLE(91,barrel_of_salt)//TODO
  if (flowerc>=6) AVAILABLE(92,barrel_of_salt)//TODO
  if (flowerc>=7) AVAILABLE(93,barrel_of_salt)//TODO
  #undef AVAILABLE
  return dstc;
}

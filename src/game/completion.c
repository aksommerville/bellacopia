#include "bellacopia.h"

/* Measure completion.
 */
 
const int progress_fldv[]={
  NS_fld_root1,NS_fld_root2,NS_fld_root3,NS_fld_root4,NS_fld_root5,NS_fld_root6,NS_fld_root7,
  NS_fld_hc1,
  NS_fld_toll_paid,
  NS_fld_mayor,
  NS_fld_war_over,
  NS_fld_barrelhat1,NS_fld_barrelhat2,NS_fld_barrelhat3,NS_fld_barrelhat4,NS_fld_barrelhat5,NS_fld_barrelhat6,NS_fld_barrelhat7,NS_fld_barrelhat8,NS_fld_barrelhat9,
  // I think don't include compass upgrades. Mostly because you can't purchase them if you accidentally complete the goal first.
  //NS_fld_target_hc,NS_fld_target_rootdevil,NS_fld_target_sidequest,NS_fld_target_gold,
  NS_fld_rescued_princess,
  NS_fld_bt1,NS_fld_bt2,NS_fld_bt3,NS_fld_bt4,
  //TODO Keep me up to date as we add achievements.
};
const int sidequest_fldv[]={
  NS_fld_toll_paid, // Debatable. He's gating a root devil, and player might broom across instead.
  NS_fld_mayor,
  NS_fld_war_over,
  NS_fld_rescued_princess,
  // Anything that isn't a simple field, check manually below.
};
 
int game_get_completion() {
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
}

void game_get_sidequests(int *done,int *total) {
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
}

/* Outer world song, depending on context.
 * We'll change the song each time you bloom a new flower.
 */
 
int bm_song_for_outerworld() {
  int flowerc=0;
  if (store_get_fld(NS_fld_root1)) flowerc++;
  if (store_get_fld(NS_fld_root2)) flowerc++;
  if (store_get_fld(NS_fld_root3)) flowerc++;
  if (store_get_fld(NS_fld_root4)) flowerc++;
  if (store_get_fld(NS_fld_root5)) flowerc++;
  if (store_get_fld(NS_fld_root6)) flowerc++;
  if (store_get_fld(NS_fld_root7)) flowerc++;
  switch (flowerc) {
    case 0: return RID_song_pretty_pretty_pickle;
    case 1: return RID_song_barrel_of_salt;
    case 2: return RID_song_feet_to_the_fire;//XXX not yet
    case 3: return RID_song_barrel_of_salt;//TODO
    case 4: return RID_song_barrel_of_salt;//TODO
    case 5: return RID_song_barrel_of_salt;//TODO
    case 6: return RID_song_barrel_of_salt;//TODO
    case 7: return RID_song_barrel_of_salt;//TODO
    default: return RID_song_barrel_of_salt;
  }
}

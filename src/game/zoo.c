#include "bellacopia.h"

/* Static metadata.
 */
 
struct zoo_resident {
  int fld;
  int spriteid;
};

// Meadow+Forest.
static const struct zoo_resident zoo1v[]={
  {NS_fld_zoo1_0,RID_sprite_raccoon},
  {NS_fld_zoo1_1,RID_sprite_heron},
  {NS_fld_zoo1_2,RID_sprite_pheasant},
  {NS_fld_zoo1_3,RID_sprite_fox},
  //TODO flds not apportioned yet
  //{NS_fld_zoo1_4,RID_sprite_bull},
  //{NS_fld_zoo1_5,RID_sprite_mouse},
  //{NS_fld_zoo1_6,RID_sprite_owl},
0};

// East Desert
static const struct zoo_resident zoo2v[]={
  {NS_fld_zoo2_0,RID_sprite_beetle},
  {NS_fld_zoo2_1,RID_sprite_koala},
  {NS_fld_zoo2_2,RID_sprite_goat},//XXX
  {NS_fld_zoo2_3,RID_sprite_ostrich},
0};

// West Desert
static const struct zoo_resident zoo3v[]={
  {NS_fld_zoo3_0,RID_sprite_leopard},//XXX
  {NS_fld_zoo3_1,RID_sprite_crab},
  {NS_fld_zoo3_2,RID_sprite_goat},//XXX
  {NS_fld_zoo3_3,RID_sprite_soldier},//XXX
0};

// South Jungle
static const struct zoo_resident zoo4v[]={
  {NS_fld_zoo4_0,RID_sprite_leopard},
  {NS_fld_zoo4_1,RID_sprite_koala},
  {NS_fld_zoo4_2,RID_sprite_goat},
  {NS_fld_zoo4_3,RID_sprite_soldier},//XXX
0};

// North Jungle
static const struct zoo_resident zoo5v[]={
  {NS_fld_zoo5_0,RID_sprite_fishycist},//XXX
  {NS_fld_zoo5_1,RID_sprite_koala},//XXX
  {NS_fld_zoo5_2,RID_sprite_leopard},
  {NS_fld_zoo5_3,RID_sprite_elephant},
0};

// Mountains
static const struct zoo_resident zoo6v[]={
  {NS_fld_zoo6_0,RID_sprite_leopard},//XXX
  {NS_fld_zoo6_1,RID_sprite_sparrow},
  {NS_fld_zoo6_2,RID_sprite_goat},
  {NS_fld_zoo6_3,RID_sprite_wolf},
0};

// West Tundra
static const struct zoo_resident zoo7v[]={
  {NS_fld_zoo7_0,RID_sprite_walrus},
  {NS_fld_zoo7_1,RID_sprite_albatross},
  {NS_fld_zoo7_2,RID_sprite_polarbear},
  {NS_fld_zoo7_3,RID_sprite_reindeer},
0};

// East Tundra
static const struct zoo_resident zoo8v[]={
  {NS_fld_zoo8_0,RID_sprite_walrus},
  {NS_fld_zoo8_1,RID_sprite_albatross},
  {NS_fld_zoo8_2,RID_sprite_koala},//XXX
  {NS_fld_zoo8_3,RID_sprite_reindeer},
0};

// Under Fractia
static const struct zoo_resident zoo9v[]={
  {NS_fld_zoo9_0,RID_sprite_spider},
  {NS_fld_zoo9_1,RID_sprite_leopard},//XXX
  {NS_fld_zoo9_2,RID_sprite_bat},
  {NS_fld_zoo9_3,RID_sprite_rat},
0};

// Under North
static const struct zoo_resident zoo10v[]={
  {NS_fld_zoo10_0,RID_sprite_spider},
  {NS_fld_zoo10_1,RID_sprite_leopard},//XXX
  {NS_fld_zoo10_2,RID_sprite_bat},
  {NS_fld_zoo10_3,RID_sprite_walrus},//XXX
0};

// Under West
static const struct zoo_resident zoo11v[]={
  {NS_fld_zoo11_0,RID_sprite_spider},
  {NS_fld_zoo11_1,RID_sprite_bat},
  {NS_fld_zoo11_2,RID_sprite_ogre},//XXX
  {NS_fld_zoo11_3,RID_sprite_witch},//XXX
0};

// Under Botire
static const struct zoo_resident zoo12v[]={
  {NS_fld_zoo12_0,RID_sprite_spider},
  {NS_fld_zoo12_1,RID_sprite_bat},
  {NS_fld_zoo12_2,RID_sprite_medusa},//XXX
  {NS_fld_zoo12_3,RID_sprite_giant},
0};

// Under Home
static const struct zoo_resident zoo13v[]={
  {NS_fld_zoo13_0,RID_sprite_spider},
  {NS_fld_zoo13_1,RID_sprite_bat},
  {NS_fld_zoo13_2,RID_sprite_koala},//XXX
  {NS_fld_zoo13_3,RID_sprite_goat},//XXX
0};

// Under Horizon
static const struct zoo_resident zoo14v[]={
  {NS_fld_zoo14_0,RID_sprite_spider},
  {NS_fld_zoo14_1,RID_sprite_bat},
  {NS_fld_zoo14_2,RID_sprite_nosferatu},
  {NS_fld_zoo14_3,RID_sprite_walrus},//XXX
0};

// Under East
static const struct zoo_resident zoo15v[]={
  {NS_fld_zoo15_0,RID_sprite_spider},
  {NS_fld_zoo15_1,RID_sprite_bat},
  {NS_fld_zoo15_2,RID_sprite_vandal},//XXX
  {NS_fld_zoo15_3,RID_sprite_walrus},//XXX
0};

// Under Southwest
static const struct zoo_resident zoo16v[]={
  {NS_fld_zoo16_0,RID_sprite_spider},
  {NS_fld_zoo16_1,RID_sprite_bat},
  {NS_fld_zoo16_2,RID_sprite_koala},//XXX
  {NS_fld_zoo16_3,RID_sprite_witch},//XXX
0};

// Under Southeast (Cheapside)
static const struct zoo_resident zoo17v[]={
  {NS_fld_zoo17_0,RID_sprite_spider},
  {NS_fld_zoo17_1,RID_sprite_bat},
  {NS_fld_zoo17_2,RID_sprite_zombie},
  {NS_fld_zoo17_3,RID_sprite_walrus},//XXX
0};
 
static const struct zoo_resident *zoov[]={
  zoo1v,zoo2v,zoo3v,zoo4v,zoo5v,zoo6v,zoo7v,zoo8v,zoo9v,
  zoo10v,zoo11v,zoo12v,zoo13v,zoo14v,zoo15v,zoo16v,zoo17v,
};

/* Get a metadata record by its principal ID.
 * Every zoo described above, the first resident's (fld) is also the name for the entire zoo.
 * Returns null or an array of struct zoo_resident, terminated by the first with zero fld.
 */
 
static const struct zoo_resident *zoo_by_first_fld(int fld) {
  const struct zoo_resident **p=zoov;
  int i=sizeof(zoov)/sizeof(void*);
  for (;i-->0;p++) {
    const struct zoo_resident *zoo=*p;
    if (zoo->fld==fld) return zoo;
  }
  return 0;
}

static int zoo_fldid_by_spriteid(const struct zoo_resident *zoo,int spriteid) {
  for (;zoo->fld;zoo++) {
    if (zoo->spriteid==spriteid) return zoo->fld;
  }
  return NS_fld_zero;
}

/* Public metadata accessors.
 */
 
int zoo_get_count(int fld) {
  const struct zoo_resident *zoo=zoo_by_first_fld(fld);
  if (!zoo) return 0;
  int c=0;
  while (zoo[c].fld) c++;
  return c;
}

int zoo_get_spriteid(int fld) {
  const struct zoo_resident **p=zoov;
  int i=sizeof(zoov)/sizeof(void*);
  for (;i-->0;p++) {
    const struct zoo_resident *zoo=*p;
    for (;zoo->fld;zoo++) {
      if (zoo->fld==fld) return zoo->spriteid;
    }
  }
  return 0;
}

/* Lazy cache of fldid indexed by (rspriteid,spriteid).
 * So the spawner can check fast whether a sprite has been zoo'd.
 */
 
static struct zoo_completion {
  int rspriteid;
  int spriteid;
  int fldid;
} *zoo_completionv=0;
static int zoo_completionc=0;
static int zoo_completiona=0;

void zoo_reset() {
  if (zoo_completionv) free(zoo_completionv);
  zoo_completionv=0;
  zoo_completionc=0;
  zoo_completiona=0;
}

static int zoo_completion_search(int rspriteid,int spriteid) {
  int lo=0,hi=zoo_completionc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct zoo_completion *q=zoo_completionv+ck;
         if (rspriteid<q->rspriteid) hi=ck;
    else if (rspriteid>q->rspriteid) lo=ck+1;
    else if (spriteid<q->spriteid) hi=ck;
    else if (spriteid>q->spriteid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct zoo_completion *zoo_completion_insert(int p,int rspriteid,int spriteid,int fldid) {
  if ((p<0)||(p>zoo_completionc)) return 0;
  if (zoo_completionc>=zoo_completiona) {
    int na=zoo_completiona+64;
    if (na>INT_MAX/sizeof(struct zoo_completion)) return 0;
    void *nv=realloc(zoo_completionv,sizeof(struct zoo_completion)*na);
    if (!nv) return 0;
    zoo_completionv=nv;
    zoo_completiona=na;
  }
  struct zoo_completion *comp=zoo_completionv+p;
  memmove(comp+1,comp,sizeof(struct zoo_completion)*(zoo_completionc-p));
  zoo_completionc++;
  comp->rspriteid=rspriteid;
  comp->spriteid=spriteid;
  comp->fldid=fldid;
  return comp;
}

/* Find fldid for a given rspriteid and spriteid (and mapid if we ever need that).
 * This should only be called once per session per spawn point.
 * NS_fld_zero if it doesn't exist, ie always spawn.
 */
 
static int zoo_fldid_for_spawn(int rspriteid,int spriteid,int mapid) {
  /* There is not a complete 1:1 correlation between rsprite and zoo, but it's pretty close.
   * So mostly we're just an rsprite-to-zoo-id mapping.
   */
  switch (rspriteid) {
    // Simple cases:
    case RID_rsprite_meadow:      return zoo_fldid_by_spriteid(zoo1v,spriteid);
    case RID_rsprite_jungle:      return zoo_fldid_by_spriteid(zoo5v,spriteid);
    case RID_rsprite_southjungle: return zoo_fldid_by_spriteid(zoo4v,spriteid);
    case RID_rsprite_mountains:   return zoo_fldid_by_spriteid(zoo6v,spriteid);
    case RID_rsprite_westdesert:  return zoo_fldid_by_spriteid(zoo3v,spriteid);
    case RID_rsprite_eastdesert:  return zoo_fldid_by_spriteid(zoo2v,spriteid);
    case RID_rsprite_easttundra:  return zoo_fldid_by_spriteid(zoo8v,spriteid);
    case RID_rsprite_ufractia:    return zoo_fldid_by_spriteid(zoo9v,spriteid);
    case RID_rsprite_unorth:      return zoo_fldid_by_spriteid(zoo10v,spriteid);
    case RID_rsprite_uwest:       return zoo_fldid_by_spriteid(zoo11v,spriteid);
    case RID_rsprite_ubotire:     return zoo_fldid_by_spriteid(zoo12v,spriteid);
    case RID_rsprite_uhome:       return zoo_fldid_by_spriteid(zoo13v,spriteid);
    case RID_rsprite_uhorizon:    return zoo_fldid_by_spriteid(zoo14v,spriteid);
    case RID_rsprite_ueast:       return zoo_fldid_by_spriteid(zoo15v,spriteid);
    case RID_rsprite_usouthwest:  return zoo_fldid_by_spriteid(zoo16v,spriteid);
    case RID_rsprite_usoutheast:  return zoo_fldid_by_spriteid(zoo17v,spriteid);
    // Odd cases:
    case RID_rsprite_isthmus:    return zoo_fldid_by_spriteid(zoo5v,spriteid); // Isthmus doesn't have a zoo; borrow the jungle's.
    case RID_rsprite_tundra:     return zoo_fldid_by_spriteid(zoo7v,spriteid); // tundra,westtundra: Same zoo.
    case RID_rsprite_westtundra: return zoo_fldid_by_spriteid(zoo7v,spriteid);
    case RID_rsprite_battlefield: // TODO We'll be doing something else for the battlefield, not pinned down yet.
    // Won't have zoos, just listing for documentary purposes:
    case RID_rsprite_goblins:
    case RID_rsprite_labyrinth:
    case RID_rsprite_goblinsback:
      break;
  }
  return NS_fld_zero;
}

/* Check whether a sprite is zoo'd.
 * Zero to proceed with spawning.
 * My hope is that we'll have a simple correlation between rsprite rids and zoos.
 * But we're getting (spriteid) and (mapid) because there are always exceptions.
 */
 
int zoo_should_suppress_monster(int spriteid,int mapid,int rspriteid) {
  // (mapid) is not used, but we have that available for the future we choose.
  
  // Sticks are never subject to zoo removal.
  if (spriteid==RID_sprite_stick) return 0;
  
  // If we have a record of it already, query the store and that's it.
  int p=zoo_completion_search(rspriteid,spriteid);
  if (p>=0) {
    const struct zoo_completion *comp=zoo_completionv+p;
    if (!comp->fldid) return 0; // I expect zeroes to be pretty common; dodge the function call when we can.
    return store_get_fld(comp->fldid);
  }
  
  // Create a record for it, initially with NS_fld_zero (ie always spawn).
  // Even if there's no associated zoo logic for this rsprite, that's a fact we need to record to prevent running the expensive query every time.
  p=-p-1;
  struct zoo_completion *comp=zoo_completion_insert(p,rspriteid,spriteid,NS_fld_zero);
  if (!comp) return 0;
  
  // Find the proper fldid, record that, and query the store.
  comp->fldid=zoo_fldid_for_spawn(rspriteid,spriteid,mapid);
  return store_get_fld(comp->fldid);
}

/* Is the zoo finished?
 */
 
int zoo_is_finished(int fld) {
  const struct zoo_resident *zoo=zoo_by_first_fld(fld);
  if (!zoo) return 1;
  for (;zoo->fld;zoo++) {
    if (!store_get_fld(zoo->fld)) return 0;
  }
  return 1;
}

/* Get text for a zoo's ticker sprite.
 */
 
int zoo_get_ticker_text(char *dst,int dsta,int fld) {
  int dstc=0;
  const struct zoo_resident *zoo=zoo_by_first_fld(fld);
  if (zoo) {
    for (;zoo->fld;zoo++) {
  
      // Skip if we already have it.
      if (store_get_fld(zoo->fld)) continue;
    
      // Get the monster's name. Anything goes wrong here, skip it.
      int strix=0;
      const void *serial=0;
      int serialc=res_get(&serial,EGG_TID_sprite,zoo->spriteid);
      struct cmdlist_reader reader;
      if (sprite_reader_init(&reader,serial,serialc)>=0) {
        struct cmdlist_entry cmd;
        while (cmdlist_reader_next(&cmd,&reader)>0) {
          if (cmd.opcode==CMD_sprite_monster) {
            strix=(cmd.arg[4]<<8)|cmd.arg[5];
            break;
          }
        }
      }
      if (!strix) continue;
    
      // Append "Wanted: " if we haven't yet.
      if (!dstc) {
        const char *pfx=0;
        int pfxc=text_get_string(&pfx,RID_strings_dialogue,114);
        if (dstc<=dsta-pfxc) memcpy(dst+dstc,pfx,pfxc);
        dstc+=pfxc;
        if (dstc<dsta) dst[dstc]=' ';
        dstc++;
      }
    
      // Append monster's name and a space.
      const char *src=0;
      int srcc=text_get_string(&src,RID_strings_battle,strix);
      if (dstc<=dsta-srcc) memcpy(dst+dstc,src,srcc);
      dstc+=srcc;
      if (dstc<dsta) dst[dstc]=' ';
      dstc++;
    }
  }
  
  // If we didn't get any text, it must be that they're all collected.
  if (!dstc) {
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_dialogue,115);
    if (srcc<=dsta) memcpy(dst,src,srcc);
    return srcc;
  }
  return dstc;
}

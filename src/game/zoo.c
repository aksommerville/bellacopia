#include "bellacopia.h"

/* Static metadata for our zoos.
 */
 
int zoo_get_count(int fld) {
  switch (fld) {
    case NS_fld_zoo1_0: return 4;
    case NS_fld_zoo2_0: return 4;
    case NS_fld_zoo3_0: return 4;
    case NS_fld_zoo4_0: return 4;
    case NS_fld_zoo5_0: return 4;
    case NS_fld_zoo6_0: return 4;
    case NS_fld_zoo7_0: return 4;
    case NS_fld_zoo8_0: return 4;
    case NS_fld_zoo9_0: return 4;
    case NS_fld_zoo10_0: return 4;
    case NS_fld_zoo11_0: return 4;
    case NS_fld_zoo12_0: return 4;
    case NS_fld_zoo13_0: return 4;
    case NS_fld_zoo14_0: return 4;
    case NS_fld_zoo15_0: return 4;
    case NS_fld_zoo16_0: return 4;
    case NS_fld_zoo17_0: return 4;
  }
  return 0;
}

int zoo_get_spriteid(int fld) {
  switch (fld) {
    //TODO none of these is final yet
    // zoo1: Forest.
    case NS_fld_zoo1_0: return RID_sprite_raccoon;
    case NS_fld_zoo1_1: return RID_sprite_heron;
    case NS_fld_zoo1_2: return RID_sprite_wolf;
    case NS_fld_zoo1_3: return RID_sprite_fox;
    // zoo2: east desert
    case NS_fld_zoo2_0: return RID_sprite_leopard;
    case NS_fld_zoo2_1: return RID_sprite_koala;
    case NS_fld_zoo2_2: return RID_sprite_goat;
    case NS_fld_zoo2_3: return RID_sprite_soldier;
    // zoo3: west desert
    case NS_fld_zoo3_0: return RID_sprite_leopard;
    case NS_fld_zoo3_1: return RID_sprite_koala;
    case NS_fld_zoo3_2: return RID_sprite_goat;
    case NS_fld_zoo3_3: return RID_sprite_soldier;
    // zoo4: south jungle
    case NS_fld_zoo4_0: return RID_sprite_leopard;
    case NS_fld_zoo4_1: return RID_sprite_koala;
    case NS_fld_zoo4_2: return RID_sprite_goat;
    case NS_fld_zoo4_3: return RID_sprite_soldier;
    // zoo5: north jungle
    case NS_fld_zoo5_0: return RID_sprite_fishycist;
    case NS_fld_zoo5_1: return RID_sprite_koala;
    case NS_fld_zoo5_2: return RID_sprite_leopard;
    case NS_fld_zoo5_3: return RID_sprite_elephant;
    // zoo6: mountains
    case NS_fld_zoo6_0: return RID_sprite_leopard;
    case NS_fld_zoo6_1: return RID_sprite_koala;
    case NS_fld_zoo6_2: return RID_sprite_goat;
    case NS_fld_zoo6_3: return RID_sprite_soldier;
    // zoo7: west tundra
    case NS_fld_zoo7_0: return RID_sprite_walrus;
    case NS_fld_zoo7_1: return RID_sprite_albatross;
    case NS_fld_zoo7_2: return RID_sprite_polarbear;
    case NS_fld_zoo7_3: return RID_sprite_leopard;
    // zoo8: east tundra
    case NS_fld_zoo8_0: return RID_sprite_walrus;
    case NS_fld_zoo8_1: return RID_sprite_albatross;
    case NS_fld_zoo8_2: return RID_sprite_koala;
    case NS_fld_zoo8_3: return RID_sprite_leopard;
    // zoo9: under fractia
    case NS_fld_zoo9_0: return RID_sprite_spider;
    case NS_fld_zoo9_1: return RID_sprite_leopard;
    case NS_fld_zoo9_2: return RID_sprite_fishycist;
    case NS_fld_zoo9_3: return RID_sprite_walrus;
    // zoo10: under north
    case NS_fld_zoo10_0: return RID_sprite_spider;
    case NS_fld_zoo10_1: return RID_sprite_leopard;
    case NS_fld_zoo10_2: return RID_sprite_fishycist;
    case NS_fld_zoo10_3: return RID_sprite_walrus;
    // zoo11: under west
    case NS_fld_zoo11_0: return RID_sprite_spider;
    case NS_fld_zoo11_1: return RID_sprite_sage;
    case NS_fld_zoo11_2: return RID_sprite_ogre;
    case NS_fld_zoo11_3: return RID_sprite_witch;
    // zoo12: under botire
    case NS_fld_zoo12_0: return RID_sprite_spider;
    case NS_fld_zoo12_1: return RID_sprite_nyarlathotep;
    case NS_fld_zoo12_2: return RID_sprite_medusa;
    case NS_fld_zoo12_3: return RID_sprite_vandal;
    // zoo13: under home
    case NS_fld_zoo13_0: return RID_sprite_spider;
    case NS_fld_zoo13_1: return RID_sprite_leopard;
    case NS_fld_zoo13_2: return RID_sprite_koala;
    case NS_fld_zoo13_3: return RID_sprite_goat;
    // zoo14: under horizon
    case NS_fld_zoo14_0: return RID_sprite_spider;
    case NS_fld_zoo14_1: return RID_sprite_leopard;
    case NS_fld_zoo14_2: return RID_sprite_fishycist;
    case NS_fld_zoo14_3: return RID_sprite_walrus;
    // zoo15: under east
    case NS_fld_zoo15_0: return RID_sprite_spider;
    case NS_fld_zoo15_1: return RID_sprite_raccoon;
    case NS_fld_zoo15_2: return RID_sprite_vandal;
    case NS_fld_zoo15_3: return RID_sprite_walrus;
    // zoo16: under southwest
    case NS_fld_zoo16_0: return RID_sprite_spider;
    case NS_fld_zoo16_1: return RID_sprite_goat;
    case NS_fld_zoo16_2: return RID_sprite_koala;
    case NS_fld_zoo16_3: return RID_sprite_witch;
    // zoo17: under southeast (cheapside)
    case NS_fld_zoo17_0: return RID_sprite_spider;
    case NS_fld_zoo17_1: return RID_sprite_unicorn;
    case NS_fld_zoo17_2: return RID_sprite_elephant;
    case NS_fld_zoo17_3: return RID_sprite_walrus;
  }
  return 0;
}

/* Check whether a sprite is zoo'd.
 * Zero to proceed with spawning.
 * My hope is that we'll have a simple correlation between rsprite rids and zoos.
 * But we're getting (spriteid) and (mapid) because there are always exceptions.
 */
 
int zoo_should_suppress_monster(int spriteid,int mapid,int rspriteid) {
  
  // Sticks are never subject to zoo removal.
  if (spriteid==RID_sprite_stick) return 0;
  
  switch (rspriteid) {
    // Simple cases:
    case RID_rsprite_meadow: return zoo_is_finished(NS_fld_zoo1_0);
    case RID_rsprite_jungle: return zoo_is_finished(NS_fld_zoo5_0);
    case RID_rsprite_southjungle: return zoo_is_finished(NS_fld_zoo4_0);
    case RID_rsprite_mountains: return zoo_is_finished(NS_fld_zoo6_0);
    case RID_rsprite_westdesert: return zoo_is_finished(NS_fld_zoo3_0);
    case RID_rsprite_eastdesert: return zoo_is_finished(NS_fld_zoo2_0);
    case RID_rsprite_easttundra: return zoo_is_finished(NS_fld_zoo8_0);
    case RID_rsprite_ufractia: return zoo_is_finished(NS_fld_zoo9_0);
    case RID_rsprite_unorth: return zoo_is_finished(NS_fld_zoo10_0);
    case RID_rsprite_uwest: return zoo_is_finished(NS_fld_zoo11_0);
    case RID_rsprite_ubotire: return zoo_is_finished(NS_fld_zoo12_0);
    case RID_rsprite_uhome: return zoo_is_finished(NS_fld_zoo13_0);
    case RID_rsprite_uhorizon: return zoo_is_finished(NS_fld_zoo14_0);
    case RID_rsprite_ueast: return zoo_is_finished(NS_fld_zoo15_0);
    case RID_rsprite_usouthwest: return zoo_is_finished(NS_fld_zoo16_0);
    case RID_rsprite_usoutheast: return zoo_is_finished(NS_fld_zoo17_0);
    // Odd cases:
    case RID_rsprite_isthmus: return zoo_is_finished(NS_fld_zoo5_0); // Isthmus doesn't have a zoo; borrow the jungle's.
    case RID_rsprite_tundra: return zoo_is_finished(NS_fld_zoo7_0); // tundra,westtundra: Same zoo.
    case RID_rsprite_westtundra: return zoo_is_finished(NS_fld_zoo7_0);
    case RID_rsprite_battlefield: // TODO We'll be doing something else for the battlefield, not pinned down yet.
    // Won't have zoos, just listing for documentary purposes:
    case RID_rsprite_goblins:
    case RID_rsprite_labyrinth:
    case RID_rsprite_goblinsback:
      break;
  }
  return 0;
}

/* Is the zoo finished?
 */
 
int zoo_is_finished(int fld) {
  int c=zoo_get_count(fld);
  while (c-->0) {
    if (!store_get_fld(fld+c)) return 0;
  }
  return 1;
}

/* Get text for a zoo's ticker sprite.
 */
 
int zoo_get_ticker_text(char *dst,int dsta,int fld) {
  int dstc=0;
  int spritec=zoo_get_count(fld);
  if (spritec<1) return 0;
  int i=0;
  for (;i<spritec;i++,fld++) {
  
    // Skip if we already have it.
    if (store_get_fld(fld)) continue;
    
    // Get the monster's name. Anything goes wrong here, skip it.
    int strix=0;
    int spriteid=zoo_get_spriteid(fld);
    const void *serial=0;
    int serialc=res_get(&serial,EGG_TID_sprite,spriteid);
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
  
  // If we didn't get any text, it must be that they're all collected.
  if (!dstc) {
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_dialogue,115);
    if (srcc<=dsta) memcpy(dst,src,srcc);
    return srcc;
  }
  return dstc;
}

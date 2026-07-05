#include "bellacopia.h"

/* Static metadata for our zoos.
 */
 
int zoo_get_count(int fld) {
  switch (fld) {
    case NS_fld_zoo1_0: return 4;
  }
  return 0;
}

int zoo_get_spriteid(int fld) {
  switch (fld) {
    // zoo1: Forest. XXX These are temporary.
    case NS_fld_zoo1_0: return RID_sprite_raccoon;
    case NS_fld_zoo1_1: return RID_sprite_koala;
    case NS_fld_zoo1_2: return RID_sprite_goat;
    case NS_fld_zoo1_3: return RID_sprite_fox;
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
    //TODO:
    case RID_rsprite_battlefield:
    case RID_rsprite_isthmus:
    case RID_rsprite_jungle:
    case RID_rsprite_southjungle:
    case RID_rsprite_tundra:
    case RID_rsprite_mountains:
    case RID_rsprite_westdesert:
    case RID_rsprite_eastdesert:
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

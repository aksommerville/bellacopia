#include "game/bellacopia.h"

/* Stories metadata.
 */
 
static const struct story storyv[16]={
  {
    .tileid_small=0x10,
    .tileid_large=0x20,
    .strix_title=1,
    .strix_desc=17,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story1,
  },
  {
    .tileid_small=0x11,
    .tileid_large=0x22,
    .strix_title=2,
    .strix_desc=18,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story2,
  },
  {
    .tileid_small=0x12,
    .tileid_large=0x24,
    .strix_title=3,
    .strix_desc=19,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story3,
  },
  {
    .tileid_small=0x13,
    .tileid_large=0x26,
    .strix_title=4,
    .strix_desc=20,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story4,
  },
  {
    .tileid_small=0x14,
    .tileid_large=0x28,
    .strix_title=5,
    .strix_desc=21,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story5,
  },
  {
    .tileid_small=0x15,
    .tileid_large=0x2a,
    .strix_title=6,
    .strix_desc=22,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story6,
  },
  {
    .tileid_small=0x16,
    .tileid_large=0x2c,
    .strix_title=7,
    .strix_desc=23,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story7,
  },
  {
    .tileid_small=0x17,
    .tileid_large=0x2e,
    .strix_title=8,
    .strix_desc=24,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story8,
  },
  {
    .tileid_small=0x18,
    .tileid_large=0x40,
    .strix_title=9,
    .strix_desc=25,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story9,
  },
  {
    .tileid_small=0x19,
    .tileid_large=0x42,
    .strix_title=10,
    .strix_desc=26,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story10,
  },
  {
    .tileid_small=0x1a,
    .tileid_large=0x44,
    .strix_title=11,
    .strix_desc=27,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story11,
  },
  {
    .tileid_small=0x1b,
    .tileid_large=0x46,
    .strix_title=12,
    .strix_desc=28,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story12,
  },
  {
    .tileid_small=0x1c,
    .tileid_large=0x48,
    .strix_title=13,
    .strix_desc=29,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story13,
  },
  {
    .tileid_small=0x1d,
    .tileid_large=0x4a,
    .strix_title=14,
    .strix_desc=30,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story14,
  },
  {
    .tileid_small=0x1e,
    .tileid_large=0x4c,
    .strix_title=15,
    .strix_desc=31,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story15,
  },
  {
    .tileid_small=0x1f,
    .tileid_large=0x4e,
    .strix_title=16,
    .strix_desc=32,
    .fld_present=NS_fld_one,//TODO
    .fld_told=NS_fld_story16,
  },
};

/* Public metadata accessors.
 */
 
const struct story *story_by_index(int p) {
  if (p<0) return 0;
  if (p>=16) return 0;
  return storyv+p;
}

const struct story *story_by_index_present(int p) {
  if (p<0) return 0;
  const struct story *story=storyv;
  int i=16;
  for (;i-->0;story++) {
    if (!store_get_fld(story->fld_present)) continue;
    if (!p--) return story;
  }
  return 0;
}

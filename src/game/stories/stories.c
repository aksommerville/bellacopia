#include "game/bellacopia.h"

/* Step lists.
 * These are each implemented in their own file adjacent to this.
 * Oh shoot....
 * ./src/game/stories/stories.c:35:12: error: initializer element is not a compile-time constant
 * I guess we need to patch these in dynamically instead of letting the linker do it.
 */

extern const struct story_step story_stepv_mayor[];
extern const struct story_step story_stepv_war[];
extern const struct story_step story_stepv_kidnap[];
extern const struct story_step story_stepv_rescue[];
extern const struct story_step story_stepv_labyrinth[];
extern const struct story_step story_stepv_toad[];
extern const struct story_step story_stepv_barrel[];
extern const struct story_step story_stepv_things[];
extern const struct story_step story_stepv_broom[];
extern const struct story_step story_stepv_map[];
extern const struct story_step story_stepv_carpenter[];
extern const struct story_step story_stepv_fish[];
extern const struct story_step story_stepv_flowers[];
extern const struct story_step story_stepv_hearts[];
extern const struct story_step story_stepv_gold[];
extern const struct story_step story_stepv_potion[];

/* Stories metadata.
 */
 
static struct story storyv[16]={
  {
    .tileid_small=0x10,
    .tileid_large=0x20,
    .strix_title=1,
    .strix_desc=17,
    .fld_present=NS_fld_mayor,
    .fld_told=NS_fld_story1,
  },
  {
    .tileid_small=0x11,
    .tileid_large=0x22,
    .strix_title=2,
    .strix_desc=18,
    .fld_present=NS_fld_war_over,
    .fld_told=NS_fld_story2,
  },
  {
    .tileid_small=0x12,
    .tileid_large=0x24,
    .strix_title=3,
    .strix_desc=19,
    .fld_present=NS_fld_kidnapped,
    .fld_told=NS_fld_story3,
  },
  {
    .tileid_small=0x13,
    .tileid_large=0x26,
    .strix_title=4,
    .strix_desc=20,
    .fld_present=NS_fld_rescued_princess,
    .fld_told=NS_fld_story4,
  },
  {
    .tileid_small=0x14,
    .tileid_large=0x28,
    .strix_title=5,
    .strix_desc=21,
    .fld_present=NS_fld_root4,
    .fld_told=NS_fld_story5,
  },
  {
    .tileid_small=0x15,
    .tileid_large=0x2a,
    .strix_title=6,
    .strix_desc=22,
    .fld_present=NS_fld_root7,
    .fld_told=NS_fld_story6,
  },
  {
    .tileid_small=0x16,
    .tileid_large=0x2c,
    .strix_title=7,
    .strix_desc=23,
    .fld_present=NS_fld_barrelhat_all,
    .fld_told=NS_fld_story7,
  },
  {
    .tileid_small=0x17,
    .tileid_large=0x2e,
    .strix_title=8,
    .strix_desc=24,
    .fld_present=NS_fld_hc3,
    .fld_told=NS_fld_story8,
  },
  {
    .tileid_small=0x18,
    .tileid_large=0x40,
    .strix_title=9,
    .strix_desc=25,
    .fld_present=NS_fld_broom_races_complete,
    .fld_told=NS_fld_story9,
  },
  {
    .tileid_small=0x19,
    .tileid_large=0x42,
    .strix_title=10,
    .strix_desc=26,
    .fld_present=NS_fld_maps_complete,
    .fld_told=NS_fld_story10,
  },
  {
    .tileid_small=0x1a,
    .tileid_large=0x44,
    .strix_title=11,
    .strix_desc=27,
    .fld_present=NS_fld_carpenter_book,
    .fld_told=NS_fld_story11,
  },
  {
    .tileid_small=0x1b,
    .tileid_large=0x46,
    .strix_title=12,
    .strix_desc=28,
    .fld_present=NS_fld_fish_book,
    .fld_told=NS_fld_story12,
  },
  {
    .tileid_small=0x1c,
    .tileid_large=0x48,
    .strix_title=13,
    .strix_desc=29,
    .fld_present=NS_fld_root_all,
    .fld_told=NS_fld_story13,
  },
  {
    .tileid_small=0x1d,
    .tileid_large=0x4a,
    .strix_title=14,
    .strix_desc=30,
    .fld_present=NS_fld_hearts_book,
    .fld_told=NS_fld_story14,
  },
  {
    .tileid_small=0x1e,
    .tileid_large=0x4c,
    .strix_title=15,
    .strix_desc=31,
    .fld_present=NS_fld_gold_book,
    .fld_told=NS_fld_story15,
  },
  {
    .tileid_small=0x1f,
    .tileid_large=0x4e,
    .strix_title=16,
    .strix_desc=32,
    .fld_present=NS_fld_potion_book,
    .fld_told=NS_fld_story16,
  },
};

/* Pain in the butt filling in (stepv) for all stories because they're external so can't be assigned in the declaration.
 */
 
static void stories_require() {
  if (storyv[0].stepv) return;
  storyv[ 0].stepv=story_stepv_mayor;
  storyv[ 1].stepv=story_stepv_war;
  storyv[ 2].stepv=story_stepv_kidnap;
  storyv[ 3].stepv=story_stepv_rescue;
  storyv[ 4].stepv=story_stepv_labyrinth;
  storyv[ 5].stepv=story_stepv_toad;
  storyv[ 6].stepv=story_stepv_barrel;
  storyv[ 7].stepv=story_stepv_things;
  storyv[ 8].stepv=story_stepv_broom;
  storyv[ 9].stepv=story_stepv_map;
  storyv[10].stepv=story_stepv_carpenter;
  storyv[11].stepv=story_stepv_fish;
  storyv[12].stepv=story_stepv_flowers;
  storyv[13].stepv=story_stepv_hearts;
  storyv[14].stepv=story_stepv_gold;
  storyv[15].stepv=story_stepv_potion;
}

/* Public metadata accessors.
 */
 
const struct story *story_by_index(int p) {
  stories_require();
  if (p<0) return 0;
  if (p>=16) return 0;
  return storyv+p;
}

const struct story *story_by_index_present(int p) {
  stories_require();
  if (p<0) return 0;
  const struct story *story=storyv;
  int i=16;
  for (;i-->0;story++) {
    if (!store_get_fld(story->fld_present)) continue;
    if (!p--) return story;
  }
  return 0;
}

const struct story *story_by_fld_present(int fld_present) {
  stories_require();
  const struct story *story=storyv;
  int i=16;
  for (;i-->0;story++) {
    if (story->fld_present==fld_present) return story;
  }
  return 0;
}

const struct story_step *story_stepv_by_strix(int strix_title) {
  stories_require();
  const struct story *story=storyv;
  int i=16;
  for (;i-->0;story++) {
    if (story->strix_title==strix_title) return story->stepv;
  }
  return 0;
}

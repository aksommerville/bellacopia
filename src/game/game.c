#include "game/bellacopia.h"

/* Map becomes visible.
 */
 
int game_welcome_map(struct map *map) {
  if (!map) return -1;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      // Camera manages (switchable,treadle,stompbox), we don't need to.
      case CMD_map_sprite: {
          double x=map->lng*NS_sys_mapw+cmd.arg[0]+0.5;
          double y=map->lat*NS_sys_maph+cmd.arg[1]+0.5;
          int rid=(cmd.arg[2]<<8)|cmd.arg[3];
          const void *arg=cmd.arg+4;
          if (rid==RID_sprite_hero) { // Hero is special. If we already have one, skip it.
            if (GRP(hero)->sprc) break;
          }
          struct sprite *sprite=sprite_spawn(x,y,rid,arg,4,0,0,0);
          if (!sprite) {
            fprintf(stderr,"map:%d failed to spawn sprite:%d at %f,%f\n",map->rid,rid,x,y);
          }
        } break;
    }
  }
  return 0;
}

/* Map acquires principal focus.
 */

int game_focus_map(struct map *map) {
  if (!map) return -1;
  int jigsaw_mapid=map->rid;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_dark: break;//TODO weather
      case CMD_map_song: bm_song_gently((cmd.arg[0]<<8)|cmd.arg[1]); break;
      case CMD_map_wind: break;//TODO weather
      case CMD_map_parent: jigsaw_mapid=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_map_debugmsg: fprintf(stderr,"map:%d debugmsg='%.*s'\n",map->rid,cmd.argc,(char*)cmd.arg); break;
    }
  }
  //TODO Should (jigsaw_mapid) get tracked somewhere? Or maybe modal_pause should poll for that on its own.
  return 0;
}

/* Items metadata.
 */
 
static const struct item_detail item_detailv[]={
  // Indexed by itemid. Slot zero will never be returned.
  [NS_itemid_stick]={
    .tileid=0x39,
    .hand_tileid=0x79,
    .strix_name=1,
    .strix_help=2,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_broom]={
    .tileid=0x30,
    .hand_tileid=0x70,
    .strix_name=3,
    .strix_help=4,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_divining]={
    .tileid=0x31,
    .hand_tileid=0x71,
    .strix_name=5,
    .strix_help=6,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_match]={
    .tileid=0x34,
    .hand_tileid=0x74,
    .strix_name=7,
    .strix_help=8,
    .initial_limit=10,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_wand]={
    .tileid=0x37,
    .hand_tileid=0x77,
    .strix_name=9,
    .strix_help=10,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_fishpole]={
    .tileid=0x33,
    .hand_tileid=0x73,
    .strix_name=11,
    .strix_help=12,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_bugspray]={
    .tileid=0x35,
    .hand_tileid=0x75,
    .strix_name=13,
    .strix_help=14,
    .initial_limit=10,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_potion]={
    .tileid=0x3c,
    .hand_tileid=0x7a,
    .strix_name=15,
    .strix_help=16,
    .initial_limit=3,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_hookshot]={
    .tileid=0x32,
    .hand_tileid=0x72,
    .strix_name=17,
    .strix_help=18,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_candy]={
    .tileid=0x36,
    .hand_tileid=0x76,
    .strix_name=19,
    .strix_help=20,
    .initial_limit=10,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_magnifier]={
    .tileid=0x38,
    .hand_tileid=0x78,
    .strix_name=21,
    .strix_help=22,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_vanishing]={
    .tileid=0x3b,
    .hand_tileid=0x7b,
    .strix_name=23,
    .strix_help=24,
    .initial_limit=10,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_compass]={
    .tileid=0x3a,
    .hand_tileid=0x7c,
    .strix_name=25,
    .strix_help=26,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_gold]={
    .tileid=0,
    .hand_tileid=0,
    .strix_name=27,
    .strix_help=0,
    .initial_limit=-NS_fld16_goldmax,
    .inventoriable=0,
    .fld16=NS_fld16_gold,
  },
  [NS_itemid_greenfish]={
    .tileid=0,
    .hand_tileid=0,
    .strix_name=28,
    .strix_help=0,
    .initial_limit=999,
    .inventoriable=0,
    .fld16=NS_fld16_greenfish,
  },
  [NS_itemid_bluefish]={
    .tileid=0,
    .hand_tileid=0,
    .strix_name=29,
    .strix_help=0,
    .initial_limit=999,
    .inventoriable=0,
    .fld16=NS_fld16_bluefish,
  },
  [NS_itemid_redfish]={
    .tileid=0,
    .hand_tileid=0,
    .strix_name=30,
    .strix_help=0,
    .initial_limit=999,
    .inventoriable=0,
    .fld16=NS_fld16_redfish,
  },
  [NS_itemid_heart]={
    .tileid=0,
    .hand_tileid=0,
    .strix_name=31,
    .strix_help=0,
    .initial_limit=-NS_fld16_hpmax,
    .inventoriable=0,
    .fld16=NS_fld16_hp,
  },
  [NS_itemid_jigpiece]={
    .tileid=0,
    .hand_tileid=0,
    .strix_name=32,
    .strix_help=33,
    .initial_limit=0,
    .inventoriable=0,
    .fld16=0,
  },
};
 
const struct item_detail *item_detail_for_itemid(int itemid) {
  if (itemid<=0) return 0;
  int c=sizeof(item_detailv)/sizeof(struct item_detail);
  if (itemid>=c) return 0;
  return item_detailv+itemid;
}

const struct item_detail *item_detail_for_equipped() {
  return item_detail_for_itemid(g.store.invstorev[0].itemid);
}

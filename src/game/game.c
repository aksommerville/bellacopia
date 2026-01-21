#include "game/bellacopia.h"

#define SPRITE_CULL_DISTANCE (NS_sys_mapw*2)
#define SPRITE_CULL_DISTANCE2 (SPRITE_CULL_DISTANCE*SPRITE_CULL_DISTANCE)

/* Map becomes visible.
 */
 
int game_welcome_map(struct map *map) {
  if (!map) return -1;
  
  /* It's not relevant to welcoming the new map, really, but take this occasion to cull far-offscreen sprites.
   */
  struct sprite **p=GRP(keepalive)->sprv;
  int i=GRP(keepalive)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue;
    if (sprite->type==&sprite_type_hero) continue; // never her
    if (sprite->z!=g.camera.z) {
      sprite_kill_soon(sprite);
      continue;
    }
    double dx=sprite->x-g.camera.fx;
    double dy=sprite->y-g.camera.fy;
    double d2=dx*dx+dy*dy;
    if (d2<SPRITE_CULL_DISTANCE2) continue;
    sprite_kill_soon(sprite);
  }
  
  // And on with the show...
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
          if (find_sprite_by_arg(arg)) {
            //fprintf(stderr,"map:%d decline to spawn sprite:%d due to already exists\n",map->rid,rid);
            break;
          }
          struct sprite *sprite=sprite_spawn(x,y,rid,arg,4,0,0,0);
          if (!sprite) {
            //fprintf(stderr,"map:%d failed to spawn sprite:%d at %f,%f\n",map->rid,rid,x,y);
            break;
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
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_dark: break;//TODO weather
      case CMD_map_song: bm_song_gently((cmd.arg[0]<<8)|cmd.arg[1]); break;
      case CMD_map_wind: break;//TODO weather
      //case CMD_map_debugmsg: fprintf(stderr,"map:%d debugmsg='%.*s'\n",map->rid,cmd.argc,(char*)cmd.arg); break;
    }
  }
  return 0;
}

/* Get item reporting.
TODO dialogue modals
 */
 
static void game_report_item_quantity_add(int itemid,int quantity) {
  fprintf(stderr,"%s(%d,%d)\n",__func__,itemid,quantity);
}
 
static void game_report_item_acquire(int itemid,int quantity) {
  // (quantity) will be zero for singletons.
  fprintf(stderr,"%s(%d,%d)\n",__func__,itemid,quantity);
}

static void game_report_item_full(int itemid) {
  // We call this both for counted slot at limit, and for new item but no slots available.
  fprintf(stderr,"%s(%d)\n",__func__,itemid);
}

/* Get item.
 */
 
int game_get_item(int itemid,int quantity) {
  if ((itemid<1)||(itemid>0xff)) return 0; // Valid itemid are 8 bits and not zero.
  
  /* If we already have it in inventory, either add quantity or do nothing.
   */
  struct invstore *invstore=store_get_itemid(itemid);
  if (invstore) {
    if (!invstore->limit) return 0; // Singleton item and we already have it. Try to avoid this situation in design.
    if (invstore->quantity>=invstore->limit) { // We're already full.
      game_report_item_full(itemid);
      return 0;
    }
    if (quantity<1) return 0; // Counted item but caller didn't supply a quantity.
    int nq=invstore->quantity+quantity;
    if (nq>invstore->limit) nq=invstore->limit; // Taking fewer than supplied due to inventory limit. Do report the attempted count tho.
    invstore->quantity=nq;
    g.store.dirty=1;
    bm_sound(RID_sound_collect);
    game_report_item_quantity_add(itemid,quantity);
    return 1;
  }
  
  /* We don't have this item in inventory.
   * Get its details.
   */
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail) { // Undefined item. Must be an error somewhere.
    fprintf(stderr,"%s:%d:ERROR: itemid %d not defined in item_detailv\n",__FILE__,__LINE__,itemid);
    return 0;
  }
  
  /* If it's inventoriable, store it in a fresh inventory slot.
   */
  if (detail->inventoriable) {
    if (detail->initial_limit>1) {
      if (quantity<1) {
        fprintf(stderr,"%s:%d:ERROR: itemid %d expects a quantity but none was provided.\n",__FILE__,__LINE__,itemid);
        return 0;
      }
    } else {
      if (quantity) {
        fprintf(stderr,"%s:%d:ERROR: Quantity %d unnecessarily provided for singleton itemid %d.\n",__FILE__,__LINE__,quantity,itemid);
        return 0;
      }
    }
    if (!(invstore=store_add_itemid(itemid,quantity))) { // Presumably inventory full.
      game_report_item_full(itemid);
      return 0;
    }
    if (detail->initial_limit>0) invstore->limit=detail->initial_limit;
    bm_sound(RID_sound_treasure);
    game_report_item_acquire(itemid,quantity);
    return 1;
  }
  
  /* If it's backed by fld16, we can update generically.
   */
  if (detail->fld16) {
    if (quantity<1) {
      fprintf(stderr,"%s:%d:ERROR: itemid %d expects a quantity but none was provided.\n",__FILE__,__LINE__,itemid);
      return 0;
    }
    int have=store_get_fld16(detail->fld16);
    int limit=0xffff;
    if (detail->initial_limit<0) {
      limit=store_get_fld16(-detail->initial_limit);
      if (!limit) fprintf(stderr,"%s:%d:WARNING: Initial limit for itemid %d expected in fld16 %d, but got zero.\n",__FILE__,__LINE__,itemid,-detail->initial_limit);
    }
    if (have>=limit) { // Already full.
      game_report_item_full(itemid);
      return 0;
    }
    int nq=have+quantity;
    if (nq>limit) nq=limit;
    store_set_fld16(detail->fld16,nq);
    bm_sound(RID_sound_collect);
    game_report_item_quantity_add(itemid,quantity);
    return 1;
  }
  
  /* Jigpiece kind of does its own thing.
   */
  if (itemid==NS_itemid_jigpiece) {
    if (!quantity) {
      fprintf(stderr,"%s:%d:ERROR: Getting jigpiece but quantity (ie mapid) zero.\n",__FILE__,__LINE__);
      return 0;
    }
    if (store_get_jigstore(quantity)) return 0; // Already have it.
    if (!store_add_jigstore(quantity)) return 0; // Shouldn't fail.
    bm_sound(RID_sound_collect);
    return 1;
  }
  
  fprintf(stderr,"%s: Unknown itemid %d\n",__func__,itemid);
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
    .tileid=0x29,
    .hand_tileid=0,
    .strix_name=27,
    .strix_help=0,
    .initial_limit=-NS_fld16_goldmax,
    .inventoriable=0,
    .fld16=NS_fld16_gold,
  },
  [NS_itemid_greenfish]={
    .tileid=0x2a,
    .hand_tileid=0,
    .strix_name=28,
    .strix_help=0,
    .initial_limit=999,
    .inventoriable=0,
    .fld16=NS_fld16_greenfish,
  },
  [NS_itemid_bluefish]={
    .tileid=0x2b,
    .hand_tileid=0,
    .strix_name=29,
    .strix_help=0,
    .initial_limit=999,
    .inventoriable=0,
    .fld16=NS_fld16_bluefish,
  },
  [NS_itemid_redfish]={
    .tileid=0x2c,
    .hand_tileid=0,
    .strix_name=30,
    .strix_help=0,
    .initial_limit=999,
    .inventoriable=0,
    .fld16=NS_fld16_redfish,
  },
  [NS_itemid_heart]={
    .tileid=0x28,
    .hand_tileid=0,
    .strix_name=31,
    .strix_help=0,
    .initial_limit=-NS_fld16_hpmax,
    .inventoriable=0,
    .fld16=NS_fld16_hp,
  },
  [NS_itemid_jigpiece]={
    .tileid=0x2e,
    .hand_tileid=0,
    .strix_name=32,
    .strix_help=33,
    .initial_limit=0,
    .inventoriable=0,
    .fld16=0,
  },
  [NS_itemid_bell]={
    .tileid=0x3d,
    .hand_tileid=0x7d,
    .strix_name=34,
    .strix_help=35,
    .initial_limit=0,
    .inventoriable=1,
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

/* Quantity in store of item or item-like thing.
 */

int possessed_quantity_for_itemid(int itemid,int *limit) {
  if (limit) *limit=0;
  
  // Jigpiece is a little special.
  if (itemid==NS_itemid_jigpiece) {
    if (limit) *limit=INT_MAX;//TODO Can we get a correct count of possible jigpieces? It's complicated.
    return g.store.jigstorec;
  }
  
  // Everything else should be generic.
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail) return 0;
  
  // The easy usual case, use inventory.
  if (detail->inventoriable) {
    const struct invstore *invstore=store_get_itemid(itemid);
    if (!invstore) {
      if (limit) *limit=detail->initial_limit?detail->initial_limit:1;
      return 0;
    }
    if (invstore->limit) {
      if (limit) *limit=invstore->limit;
      return invstore->quantity;
    }
    if (limit) *limit=1;
    return 1; // Singleton in inventory, quantity is one.
  }
  
  // If (fld16) is set, that's the quantity.
  if (detail->fld16) {
    if (limit) {
      if (detail->initial_limit<0) *limit=store_get_fld16(-detail->initial_limit);
      else *limit=0xffff;
    }
    return store_get_fld16(detail->fld16);
  }
  
  // There will probably be plain (fld) items in the future. Maybe? Well there aren't yet.
  return 0;
}

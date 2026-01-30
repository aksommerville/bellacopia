#include "game/bellacopia.h"

#define SPRITE_CULL_DISTANCE (NS_sys_mapw*2)
#define SPRITE_CULL_DISTANCE2 (SPRITE_CULL_DISTANCE*SPRITE_CULL_DISTANCE)

/* Reset entire game.
 */
 
int game_reset(int use_save) {
  if (use_save) {
    if (store_load("save",4)<0) return -1;
  } else {
    if (store_clear()<0) return -1;
  }
  sprites_reset();
  camera_reset();
  feet_reset();
  spawner_reset();
  g.bugspray=0.0;
  g.vanishing=0.0;
  g.flash=0.0;
  g.warp_listener=0;
  g.gameover=0;
  return 0;
}

/* Generate update.
 */
 
void game_update(double elapsed) {
  
  // Minor nonpersistent clocks.
  if (g.bugspray>0.0) {
    g.bugspray-=elapsed;
  }
  if (g.vanishing>0.0) {
    g.vanishing-=elapsed;
  }
  if (g.flash>0.0) {
    g.flash-=elapsed;
  }
}

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
 */
 
// Caller must sprite_toast_set_text() after.
static struct sprite *game_start_toast() {
  if (sprite_toast_get_any()) return 0;
  double x,y;
  if (GRP(hero)->sprc>0) {
    struct sprite *hero=GRP(hero)->sprv[0];
    x=hero->x;
    y=hero->y-0.5;
  } else {
    x=(double)(g.camera.rx+(FBW>>1))/(double)NS_sys_tilesize;
    y=(double)(g.camera.ry+(FBH>>1))/(double)NS_sys_tilesize;
  }
  return sprite_spawn(x,y,0,0,0,&sprite_type_toast,0,0);
}
 
static void game_report_item_quantity_add(int itemid,int quantity) {
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail||!detail->strix_name) return;
  struct sprite *sprite=game_start_toast();
  if (!sprite) return;
  struct text_insertion insv[]={
    {.mode='i',.i=quantity},
    {.mode='r',.r={.rid=RID_strings_item,.strix=detail->strix_name}},
  };
  char text[64];
  int textc=text_format_res(text,sizeof(text),RID_strings_item,48,insv,sizeof(insv)/sizeof(insv[0]));
  if ((textc<0)||(textc>sizeof(text))) return;
  sprite_toast_set_text(sprite,text,textc);
}

static void game_report_item_full(int itemid) {
  // We call this both for counted slot at limit, and for new item but no slots available.
  // It's static text "Pack full!", but we could insert the item's name if we want in the future.
  struct sprite *sprite=game_start_toast();
  if (!sprite) return;
  const char *text=0;
  int textc=text_get_string(&text,RID_strings_item,49);
  sprite_toast_set_text(sprite,text,textc);
}
 
static void game_report_item_acquire(int itemid,int quantity) {
  // (quantity) will be zero for singletons.
  
  /* Most items will never leave your inventory.
   * Those that do, eg giving your compass to a toll troll, it's fine to repeat the fanfare when you get another.
   * But stick is different: It enters and leaves your inventory frequently.
   * So we dedicate a field to indicate whether stick has been got before, and only show this dialogue the first time.
   * After the first time, getting another stick will be passive like getting gold or hearts.
   */
  if (itemid==NS_itemid_stick) {
    if (store_get_fld(NS_fld_had_stick)) {
      game_report_item_quantity_add(NS_itemid_stick,1);
      return;
    }
    store_set_fld(NS_fld_had_stick,1);
  }
  
  // If the item doesn't exist or has no name, noop.
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (!detail) return;
  if (!detail->strix_name) return;
  
  // Message begins "You found Thing!". No quantity even if relevant.
  char msg[256];
  struct text_insertion ins={.mode='r',.r={.rid=RID_strings_item,.strix=detail->strix_name}};
  int msgc=text_format_res(msg,sizeof(msg),RID_strings_item,47,&ins,1);
  if ((msgc<0)||(msgc>sizeof(msg))) msgc=0;
  
  // If help text is available, append a newline then that. It's static text.
  if (detail->strix_help) {
    const char *help=0;
    int helpc=text_get_string(&help,RID_strings_item,detail->strix_help);
    if ((helpc>0)&&(msgc+1+helpc<=sizeof(msg))) {
      msg[msgc++]='\n';
      memcpy(msg+msgc,help,helpc);
      msgc+=helpc;
    }
  }
  
  struct modal_args_dialogue args={
    .text=msg,
    .textc=msgc,
  };
  modal_spawn(&modal_type_dialogue,&args,sizeof(args));
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
    if (itemid==NS_itemid_heartcontainer) { // New heart containers are full, ie increment HP.
      store_set_fld16(NS_fld16_hp,store_get_fld16(NS_fld16_hp)+1);
    }
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
  [NS_itemid_heartcontainer]={
    .tileid=0x2f,
    .hand_tileid=0,
    .strix_name=36,
    .strix_help=37,
    .initial_limit=0,
    .inventoriable=0,
    .fld16=NS_fld16_hpmax,
  },
  [NS_itemid_barrelhat]={
    .tileid=0x3e,
    .hand_tileid=0x8d,
    .strix_name=50,
    .strix_help=51,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_telescope]={
    .tileid=0x3f,
    .hand_tileid=0x8e,
    .strix_name=52,
    .strix_help=53,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_shovel]={
    .tileid=0x4d,
    .hand_tileid=0x8f,
    .strix_name=54,
    .strix_help=55,
    .initial_limit=0,
    .inventoriable=1,
    .fld16=0,
  },
  [NS_itemid_pepper]={
    .tileid=0x4e,
    .hand_tileid=0x93,
    .strix_name=56,
    .strix_help=57,
    .initial_limit=10,
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

/* Warp.
 */
 
int game_warp(int mapid) {
  if (g.warp_listener) return -1;

  // Confirm (mapid) exists and has a hero spawn point.
  const struct map *map=map_by_id(mapid);
  if (!map) return -1;
  int col=-1,row=-1;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_sprite) {
      int rid=(cmd.arg[2]<<8)|cmd.arg[3];
      if (rid==RID_sprite_hero) {
        col=cmd.arg[0];
        row=cmd.arg[1];
        break;
      }
    }
  }
  if (col<0) return -1;
  
  camera_cut(mapid,col,row);
  
  // Move hero immediately.
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    hero->z=map->z;
    hero->x=map->lng*NS_sys_mapw+col+0.5;
    hero->y=map->lat*NS_sys_maph+row+0.5;
  }
  return 0;
}

/* Choose a fish to catch.
 */
 
int game_choose_fish(int x,int y,int z) {
  //TODO Vary geographically.
  //TODO Can we do some logic where fish get rarer as you catch more, and you have to wait a while to build them back up?
  switch (rand()%10) {
    case 0: case 1: case 2: return 0;
    case 3: case 4: case 5: case 6: return NS_itemid_greenfish;
    case 7: case 8: return NS_itemid_bluefish;
    case 9: return NS_itemid_redfish;
  }
  return 0;
}

/* Hurt the hero.
 */
 
void game_hurt_hero() {
  int hp=store_get_fld16(NS_fld16_hp);
  if (--hp<=0) {
    store_set_fld16(NS_fld16_hp,0);
    g.gameover=1;
  } else {
    store_set_fld16(NS_fld16_hp,hp);
    bm_sound(RID_sound_ouch);
  }
}

/* List nearby secrets, within one map.
 */
 
static int game_find_secrets_1(struct secret *dst,int dsta,struct map *map,double x,double y,double r2) {
  if (dsta<1) return 0;
  int dstc=0;
  double x0=(double)(map->lng*NS_sys_mapw)+0.5;
  double y0=(double)(map->lat*NS_sys_maph)+0.5;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
    
      case CMD_map_door: {
          //TODO I'm sure we'll soon want buried doors in addition to buried treasure.
        } break;
        
      case CMD_map_buriedtreasure: {
          int fld=(cmd.arg[2]<<8)|cmd.arg[3];
          if (store_get_fld(fld)) break; // Already got; skip it.
          double poix=cmd.arg[0]+x0;
          double poiy=cmd.arg[1]+y0;
          double dx=x-poix;
          double dy=y-poiy;
          double d2=dx*dx+dy*dy;
          if (d2<r2) {
            struct secret *secret=dst+dstc++;
            secret->map=map;
            secret->cmd=cmd;
            secret->x=poix;
            secret->y=poiy;
            secret->d2=d2;
            if (dstc>=dsta) return dstc;
          }
        } break;
    }
  }
  return dstc;
}

/* List nearby secrets.
 */
 
int game_find_secrets(struct secret *dst,int dsta,double x,double y,int z,double radius) {
  if (!dst||(dsta<1)) return 0;
  struct plane *plane=plane_by_position(z);
  if (!plane||(plane->w<1)) return 0;
  int planew=plane->w*NS_sys_mapw;
  int planeh=plane->h*NS_sys_maph;

  /* Calculate bounds as integer plane meters, and clamp to the plane.
   * (xz,yz) are exclusive.
   */
  int xa=(int)(x-radius); if (xa<0) xa=0;
  int xz=(int)(x+radius)+1; if (xz>planew) xz=planew;
  int ya=(int)(y-radius); if (ya<0) ya=0;
  int yz=(int)(y+radius)+1; if (yz>planeh) yz=planeh;
  int w=xz-xa; if (w<1) return 0;
  int h=yz-ya; if (h<1) return 0;
  
  /* Determine map coverage.
   */
  int mx=xa/NS_sys_mapw;
  int mw=xz/NS_sys_mapw-mx+1;
  int my=ya/NS_sys_maph;
  int mh=yz/NS_sys_maph-my+1;
  
  /* Iterate maps.
   */
  int dstc=0;
  double r2=radius*radius;
  struct map *maprow=plane->v+my*plane->w+mx;
  for (;mh-->0;maprow+=plane->w) {
    struct map *map=maprow;
    int xi=mw;
    for (;xi-->0;map++) {
      dstc+=game_find_secrets_1(dst+dstc,dsta-dstc,map,x,y,r2);
      if (dstc>=dsta) return dstc;
    }
  }

  return dstc;
}

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
  NS_fld_toll_paid,
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

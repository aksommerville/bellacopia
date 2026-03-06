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
  g.song_override_outerworld=0;
  return 0;
}

/* Generate update.
 */
 
void game_update(double elapsed) {
  
  // Minor nonpersistent clocks.
  if (g.bugspray>0.0) {
    g.bugspray-=elapsed;
  }
  if (g.flash>0.0) {
    g.flash-=elapsed;
  }
  if (g.vanishing>0.0) {
    g.vanishing-=elapsed;
  }
  if (g.fishclock>0.0) {
    g.fishclock-=elapsed;
  }
  
  // Weather.
  if (g.eqclock>0.0) bm_update_earthquake(elapsed);
}

/* Map becomes visible.
 */
 
int game_welcome_map(struct map *map) {
  if (!map) return -1;
  
  /* It's not relevant to welcoming the new map, really, but take this occasion to cull far-offscreen sprites.
   */
  if (!g.telescoping) {
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
      case CMD_map_dark: break;
      case CMD_map_song: if (!g.telescoping) bm_song_gently((cmd.arg[0]<<8)|cmd.arg[1]); break;
      case CMD_map_wind: break;
      //case CMD_map_debugmsg: fprintf(stderr,"map:%d debugmsg='%.*s'\n",map->rid,cmd.argc,(char*)cmd.arg); break;
    }
  }
  if (!g.song_override_outerworld&&map_is_outerworld(map)) {
    bm_song_gently(bm_song_for_outerworld());
  }
  return 0;
}

/* Warp.
 */
 
int game_warp(int mapid,int transition) {
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
  
  camera_cut(mapid,col,row,transition);
  
  // Move hero immediately.
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    hero->z=map->z;
    hero->x=map->lng*NS_sys_mapw+col+0.5;
    hero->y=map->lat*NS_sys_maph+row+0.5;
  }
  return 0;
}

/* Poke the global fish clock and take random odds against it.
 * Return zero to catch a fish, or nonzero if fished out (or unlucky).
 */
 
#define FISHCLOCK_LIMIT      20.000
#define FISHCLOCK_INCREMENT   8.000 /* Bear in mind, the fishing itself takes 1..10 seconds. */
 
static int game_touch_fishclock() {
  // If we're already over the time limit, return exhausted but don't penalize any further.
  if (g.fishclock>=FISHCLOCK_LIMIT) return 1;
  int threshold=(int)((g.fishclock*65535.0)/FISHCLOCK_LIMIT);
  int choice=rand()&0xffff;
  g.fishclock+=FISHCLOCK_INCREMENT;
  return (choice<threshold)?1:0;
}

/* Return itemid for a given fishodds, applying context.
 */
 
static int game_apply_fishodds(int fishodds) {
  switch (fishodds) {
    case NS_fishodds_never: return 0;
    case NS_fishodds_green: if (game_touch_fishclock()) return 0; return NS_itemid_greenfish;
    case NS_fishodds_blue: if (game_touch_fishclock()) return 0; return NS_itemid_bluefish;
    case NS_fishodds_red: if (game_touch_fishclock()) return 0; return NS_itemid_redfish;
    case NS_fishodds_rich: {
        if (game_touch_fishclock()) return 0;
        switch (rand()%10) {
          case 0: case 1: case 2: return NS_itemid_greenfish;
          case 3: case 4: case 5: return NS_itemid_bluefish;
          case 6: case 7: case 8: case 9: return NS_itemid_redfish;
        }
      } break;
  }
  // NS_fishodds_default or unknown. Also "parent", if it ends up here.
  if (game_touch_fishclock()) return 0;
  switch (rand()%10) {
    case 0: case 1: case 2: case 3: case 4: case 5: return NS_itemid_greenfish;
    case 6: case 7: case 8: return NS_itemid_bluefish;
    case 9: return NS_itemid_redfish;
  }
  return 0;
}

/* Choose a fish to catch.
 */
 
int game_choose_fish(int x,int y,int z) {
  struct map *map=map_by_sprite_position(x,y,z);
  if (!map) return game_apply_fishodds(NS_fishodds_default);
  int col=x-map->lng*NS_sys_mapw;
  int row=y-map->lat*NS_sys_maph;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_fishodds: {
          if (col!=cmd.arg[0]) break;
          if (row!=cmd.arg[1]) break;
          int fishodds=(cmd.arg[2]<<8)|cmd.arg[3];
          int itemid=(cmd.arg[4]<<8)|cmd.arg[5];
          int fld=(cmd.arg[6]<<8)|cmd.arg[7];
          if (fld&&!store_get_fld(fld)) {
            store_set_fld(fld,1);
            return itemid;
          }
          if (fishodds!=NS_fishodds_parent) {
            return game_apply_fishodds(fishodds);
          }
        } break;
    }
  }
  if (map->fishfld&&!store_get_fld(map->fishfld)) {
    store_set_fld(map->fishfld,1);
    return map->fishitemid;
  }
  return game_apply_fishodds(map->fishodds);
}

/* Prize for regular battles.
 */

int game_get_prizes(struct prize *v,int a,int battle,const uint8_t *monsterarg) {
  if (!v||(a<1)) return 0;
  int c=0;
  //TODO Opportunity for refined logic and per-sprite exceptions.
  
  // If our hp is below the max, award a heart instead of a coin, at say 1/3 odds.
  int hp=store_get_fld16(NS_fld16_hp);
  int hpmax=store_get_fld16(NS_fld16_hpmax);
  if (hp<hpmax) {
    if (!(rand()%3)) {
      v[c++]=(struct prize){NS_itemid_heart,1};
    }
  }
  
  // Nothing else? Give a coin, whether we can hold it or not.
  if (!c) {
    v[c++]=(struct prize){NS_itemid_gold,1};
  }
  return c;
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

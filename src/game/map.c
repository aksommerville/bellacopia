#include "bellacopia.h"

static const uint8_t map_default_cells[NS_sys_mapw*NS_sys_maph]={0};

/* Freshen tiles for one map.
 */
 
void map_freshen_tiles(struct map *map,struct map_extras *extras) {
  if (!map) return;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_switchable:
      case CMD_map_stompbox:
      case CMD_map_treadle:
      case CMD_map_switchable2: {
          int d=(cmd.opcode==CMD_map_switchable2)?2:1;
          int x=cmd.arg[0],y=cmd.arg[1];
          if ((x<NS_sys_mapw)&&(y<NS_sys_maph)) {
            int p=y*NS_sys_mapw+x;
            if (store_get_fld((cmd.arg[2]<<8)|cmd.arg[3])) {
              map->v[p]=map->rov[p]+d;
            } else {
              map->v[p]=map->rov[p];
            }
          }
        } break;
      case CMD_map_debugmsg: if (extras) {
          extras->debugmsg=(const char*)cmd.arg;
          extras->debugmsgc=cmd.argc;
        } break;
    }
  }
}

/* Reset store.
 */
 
static void plane_cleanup(struct plane *plane) {
  if (plane->v) free(plane->v);
}
 
int mapstore_reset() {
  while (g.mapstore.planec--) plane_cleanup(g.mapstore.planev+g.mapstore.planec);
  if (g.mapstore.byidv) free(g.mapstore.byidv);
  memset(&g.mapstore,0,sizeof(struct mapstore));
  
  // Might as well populate (plane->z) in advance, they're effectively constant.
  struct plane *plane=g.mapstore.planev;
  int i=0;
  for (;i<0x100;i++,plane++) plane->z=i;
  
  return 0;
}

/* Welcome map resource, during load.
 */
 
int mapstore_welcome(const struct rom_entry *res) {
  uint8_t lng,lat,z,gotpos=0;
  struct map_res rmap={0};
  if (map_res_decode(&rmap,res->v,res->c)<0) return -1;
  struct cmdlist_reader reader={.v=rmap.cmd,.c=rmap.cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_position: gotpos=1; lng=cmd.arg[0]; lat=cmd.arg[1]; z=cmd.arg[2]; break;
    }
  }
  
  if (!gotpos) {
    fprintf(stderr,"map:%d did not declare a position\n",res->rid);
    return -1;
  }
  
  if (z>=g.mapstore.planec) g.mapstore.planec=z+1;
  struct plane *plane=g.mapstore.planev+z;
  if (lng>=plane->w) plane->w=lng+1;
  if (lat>=plane->h) plane->h=lat+1;
  
  if (res->rid>=g.mapstore.byidc) g.mapstore.byidc=res->rid+1;

  return 0;
}

/* Decode map from resource.
 * These go into a temporary object initially.
 * Then we examine the declared position and copy verbatim into the real storage.
 */
 
static int map_decode(struct map *dst,const void *src,int srcc) {

  struct map_res rmap={0};
  if (map_res_decode(&rmap,src,srcc)<0) return -1;
  if ((rmap.w!=NS_sys_mapw)||(rmap.h!=NS_sys_maph)) return -1;
  
  dst->cmd=rmap.cmd;
  dst->cmdc=rmap.cmdc;
  dst->rov=rmap.v;
  memcpy(dst->v,rmap.v,NS_sys_mapw*NS_sys_maph);
  
  struct cmdlist_reader reader={.v=rmap.cmd,.c=rmap.cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_dark: dst->dark=1; break;
      case CMD_map_image: dst->imageid=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_map_song: dst->songid=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_map_wind: dst->wind=cmd.arg[0]; break;
      case CMD_map_parent: dst->parent=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_map_position: dst->lng=cmd.arg[0]; dst->lat=cmd.arg[1]; dst->z=cmd.arg[2]; break;
    }
  }
  
  dst->physics=tilesheet_get_physics(dst->imageid);
  dst->jigctab=tilesheet_get_jigctab(dst->imageid);
  
  return 0;
}

/* Finish load.
 */
 
int mapstore_link() {

  /* Allocate plane innards.
   * During load, we've set (w,h) in each.
   * But we haven't allocated (v) in each yet.
   */
  struct plane *plane=g.mapstore.planev;
  int i=g.mapstore.planec;
  for (;i-->0;plane++) {
    if (plane->v) return -1;
    if ((plane->w<1)||(plane->h<1)) {
      plane->w=plane->h=0;
    } else {
      if (!(plane->v=calloc(plane->w*plane->h,sizeof(struct map)))) return -1;
    }
  }
  
  /* Allocate id list.
   * We collected its length during load.
   */
  if (g.mapstore.byidv) return -1;
  if (g.mapstore.byidc&&!(g.mapstore.byidv=calloc(g.mapstore.byidc,sizeof(void*)))) return -1;
  
  /* Iterate all map resources and assign to the plane and id lists.
   */
  int resp=res_search(EGG_TID_map,0);
  if (resp<0) resp=-resp-1;
  const struct rom_entry *res=g.resv+resp;
  int singp=0;
  for (;(resp<g.resc)&&(res->tid==EGG_TID_map);resp++,res++) {
    if (res->rid>=g.mapstore.byidc) return -1;
    struct map tmp={0};
    if (map_decode(&tmp,res->v,res->c)<0) return -1;
    if (tmp.z>=g.mapstore.planec) return -1;
    struct plane *plane=g.mapstore.planev+tmp.z;
    if ((tmp.lng>=plane->w)||(tmp.lat>=plane->h)) return 0;
    struct map *map=plane->v+tmp.lat*plane->w+tmp.lng;
    if (map->rid) {
      fprintf(stderr,"%s: map:%d and map:%d both claim position %d,%d,%d\n",__func__,map->rid,res->rid,tmp.lng,tmp.lat,tmp.z);
      return -1;
    }
    memcpy(map,&tmp,sizeof(struct map));
    map->rid=res->rid;
    g.mapstore.byidv[res->rid]=map;
  }
  
  /* One more pass over every plane and reassign (lng,lat,z).
   * For real maps this is noop, but if there were any gaps, they will at least have valid coordinates.
   * Also a good time to detect (full).
   */
  for (plane=g.mapstore.planev,i=g.mapstore.planec;i-->0;plane++) {
    if ((plane->w<1)||(plane->h<1)) continue;
    plane->full=1;
    struct map *map=plane->v;
    int lat=0;
    for (;lat<plane->h;lat++) {
      int lng=0;
      for (;lng<plane->w;lng++,map++) {
        if (!map->rid) plane->full=0;
        map->lng=lng;
        map->lat=lat;
        map->z=plane->z;
        if (!map->rov) map->rov=map_default_cells;
        if (!map->physics) map->physics=tilesheet_get_physics(map->imageid);
        if (!map->jigctab) map->jigctab=tilesheet_get_jigctab(map->imageid);
      }
    }
  }
  
  return 0;
}

/* Accessors.
 */

struct map *map_by_id(int rid) {
  if ((rid<0)||(rid>=g.mapstore.byidc)) return 0;
  return g.mapstore.byidv[rid];
}

struct map *map_by_position(int lng,int lat,int z) {
  if ((lng<0)||(lat<0)||(z<0)) return 0;
  if (z>=g.mapstore.planec) return 0;
  struct plane *plane=g.mapstore.planev+z;
  if ((lng>=plane->w)||(lat>=plane->h)) return 0;
  return plane->v+lat*plane->w+lng;
}

struct map *map_by_sprite_position(double x,double y,int z) {
  if ((x<0.0)||(y<0.0)) return 0;
  int lng=((int)x)/NS_sys_mapw;
  int lat=((int)y)/NS_sys_maph;
  return map_by_position(lng,lat,z);
}

struct plane *plane_by_position(int z) {
  if ((z<0)||(z>=g.mapstore.planec)) return 0;
  return g.mapstore.planev+z;
}

/* Start position.
 */
 
static int maps_get_start_mapid() {
  if (store_get_fld(NS_fld_kidnapped)&&!store_get_fld(NS_fld_escaped)) return RID_map_jail;
  return RID_map_start;
}
 
int maps_get_start_position(int *mapid,int *col,int *row) {
  *mapid=maps_get_start_mapid();
  struct map *map=map_by_id(*mapid);
  if (!map) return -1;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_sprite: {
          int spriteid=(cmd.arg[2]<<8)|cmd.arg[3];
          if (spriteid!=RID_sprite_hero) break;
          *mapid=map->rid;
          *col=cmd.arg[0];
          *row=cmd.arg[1];
          return 0;
        }
    }
  }
  return -1;
}

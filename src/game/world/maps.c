#include "game/game.h"
#include "map.h"

/* Global registry.
 */
 
struct plane {
  int x,y,w,h; // In maps.
  uint8_t oobx,ooby;
  struct map *v; // LRTB, size is (w*h) maps.
  int parent; // mapid or zero, also recorded in all of my maps.
};

struct idix {
  int8_t x,y;
  uint8_t z;
};

static struct {
  struct plane planev[256]; // Every addressable plane is pre-allocated.
  int planec; // How many we actually use.
  struct idix *idixv; // Indexed by rid.
  int idixc;
} maps={0};

/* Install one map, during initial scan.
 */
 
static int maps_install(int rid,const void *serial,int serialc) {

  // Split the resource and read some key commands.
  struct map_res rmap;
  if (map_res_decode(&rmap,serial,serialc)<0) return -1;
  if ((rmap.w!=NS_sys_mapw)||(rmap.h!=NS_sys_maph)) {
    fprintf(stderr,"map:%d has size %dx%d, only %dx%d is allowed.\n",rid,rmap.w,rmap.h,NS_sys_mapw,NS_sys_maph);
    return -1;
  }
  int x=0,y=0,z=-1;
  int imageid=0,songid=-1;
  int oobx=0,ooby=0;
  int parent=0;
  struct cmdlist_reader reader={.v=rmap.cmd,.c=rmap.cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_image: imageid=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_map_song: songid=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_map_oob: {
          oobx=cmd.arg[0];
          ooby=cmd.arg[1];
          if (!oobx||!ooby) {
            fprintf(stderr,"map:%d invalid oob (%d,%d)\n",rid,oobx,ooby);
            return -1;
          }
        } break;
      case CMD_map_parent: parent=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_map_position: x=(int8_t)cmd.arg[0]; y=(int8_t)cmd.arg[1]; z=cmd.arg[2]; break;
    }
  }
  
  // Identify the plane, confirm in bounds, and confirm not occupied.
  // Only the double-occupancy error matters here; the others should have been caught earlier.
  if (z<0) return -1;
  if (z>=maps.planec) return -1;
  struct plane *plane=maps.planev+z;
  if ((x<plane->x)||(y<plane->y)||(x>=plane->x+plane->w)||(y>=plane->y+plane->h)) return -1;
  struct map *map=plane->v+(y-plane->y)*plane->w+(x-plane->x);
  if (map->rid) {
    fprintf(stderr,"map:%d and map:%d both claim position (%d,%d,%d)\n",map->rid,rid,x,y,z);
    return -1;
  }
  
  // If the map declared OOB handling strategy, commit to to plane and ensure it agrees.
  if (oobx) {
    if (plane->oobx) {
      if ((oobx!=plane->oobx)||(ooby!=plane->ooby)) {
        fprintf(stderr,
          "Conflicting OOB strategy for plane %d. (%d,%d) vs (%d,%d). map:%d is one, don't know the other.\n",
          z,oobx,ooby,plane->oobx,plane->ooby,rid
        );
        return -1;
      }
    } else {
      plane->oobx=oobx;
      plane->ooby=ooby;
    }
  }
  
  // Parent, same idea as OOB.
  if (parent) {
    if (plane->parent) {
      if (plane->parent!=parent) {
        fprintf(stderr,
          "Conflicting map parents for plane %d. map:%d (from map:%d) vs map:%d.\n",
          z,parent,rid,plane->parent
        );
        return -1;
      }
    } else {
      plane->parent=parent;
    }
  }
  
  // Populate the map.
  map->x=x;
  map->y=y;
  map->z=z;
  map->rid=rid;
  map->imageid=imageid;
  map->songid=songid;
  map->ro=rmap.v;
  map->cmd=rmap.cmd;
  map->cmdc=rmap.cmdc;
  memcpy(map->v,map->ro,sizeof(map->v));
  if ((rid>=0)&&(rid<maps.idixc)) {
    struct idix *idix=maps.idixv+rid;
    idix->x=x;
    idix->y=y;
    idix->z=z;
  }
  map->physics=res_get_physics_table(map->imageid);
  
  return 0;
}

/* Initialize.
 */
 
int maps_init() {
  
  // One pass over the map resources to only establish plane boundaries.
  int ridmax=1;
  int resp=res_search(EGG_TID_map,1);
  if (resp<0) resp=-resp-1;
  struct rom_entry *res=g.resv+resp;
  int i=g.resc-resp;
  for (;i-->0;res++) {
    if (res->tid>EGG_TID_map) break;
    if (res->rid>ridmax) ridmax=res->rid;
    int x,y,z=-1;
    struct map_res rmap;
    if (map_res_decode(&rmap,res->v,res->c)<0) return -1;
    struct cmdlist_reader reader={.v=rmap.cmd,.c=rmap.cmdc};
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      if (cmd.opcode==CMD_map_position) {
        x=(int8_t)cmd.arg[0];
        y=(int8_t)cmd.arg[1];
        z=cmd.arg[2];
        break;
      }
    }
    if (z<0) {
      fprintf(stderr,"map:%d didn't declare its position\n",res->rid);
      return -1;
    }
    if (z>=maps.planec) maps.planec=z+1;
    struct plane *plane=maps.planev+z;
    // First map in the plane resets plane origin. Otherwise grow to fit it.
    if (plane->w) {
      if (x<plane->x) { plane->w+=plane->x-x; plane->x=x; }
      else if (x>=plane->x+plane->w) plane->w=x-plane->x+1;
      if (y<plane->y) { plane->h+=plane->y-y; plane->y=y; }
      else if (y>=plane->y+plane->h) plane->h=y-plane->y+1;
    } else {
      plane->x=x;
      plane->y=y;
      plane->w=1;
      plane->h=1;
    }
  }
  
  // Allocate the id index.
  maps.idixc=ridmax+1;
  if (!(maps.idixv=calloc(maps.idixc,sizeof(struct idix)))) return -1;
  
  // Iterate the planes and allocate each to its minimum size.
  struct plane *plane=maps.planev;
  for (i=maps.planec;i-->0;plane++) {
    if (!plane->w) continue;
    if (!(plane->v=calloc(plane->w*plane->h,sizeof(struct map)))) return -1;
  }
  
  // Iterate maps again, this time installing in their plane.
  for (res=g.resv+resp,i=g.resc-resp;i-->0;res++) {
    if (res->tid>EGG_TID_map) break;
    if (maps_install(res->rid,res->v,res->c)<0) return -1;
  }
  
  // Any plane without a declared OOB strategy, force it to "null". Zero is not a legal oob strategy.
  // Also in this pass, set (parent) in all maps where the plane has one.
  for (i=maps.planec,plane=maps.planev;i-->0;plane++) {
    if (!plane->oobx) {
      plane->oobx=NS_mapoob_null;
      plane->ooby=NS_mapoob_null;
    }
    if (plane->parent) {
      struct map *map=plane->v;
      int mapi=plane->w*plane->h;
      for (;mapi-->0;map++) map->parent=plane->parent;
    }
  }
  
  return 0;
}

/* Reset.
 */
 
void maps_reset() {
  struct plane *plane=maps.planev;
  int i=maps.planec;
  for (;i-->0;plane++) {
    struct map *map=plane->v;
    int mi=plane->w*plane->h;
    for (;mi-->0;map++) {
      if (!map->ro) continue; // Empty slot, skip it. There will be lots of these.
      memcpy(map->v,map->ro,sizeof(map->v));
    }
  }
}

/* Find map, public.
 */
 
struct map *map_by_position(int x,int y,int z) {

  // Unknown or empty plane, return null.
  if ((z<0)||(z>=maps.planec)) return 0;
  struct plane *plane=maps.planev+z;
  if ((plane->w<1)||(plane->h<1)) return 0;
  
  // Offset by plane's origin. After this, (x,y) are normalized.
  x-=plane->x;
  y-=plane->y;
  
  // Apply OOB strategy if needed.
  switch (plane->oobx) {
    case NS_mapoob_loop: while (x<0) x+=plane->w; x%=plane->w; break;
    case NS_mapoob_repeat: if (x<0) x=0; else if (x>=plane->w) x=plane->w-1; break;
    case NS_mapoob_farloop: {
        int twice=plane->w<<1;
        int half=plane->w>>1;
        int adjx=x+half;
        while (adjx<0) adjx+=twice;
        adjx%=twice;
        x=adjx-half;
        if (x<0) x=0; else if (x>=plane->w) x=plane->w-1;
      } break;
    case NS_mapoob_null:
    default: if ((x<0)||(x>=plane->w)) return 0; break;
  }
  switch (plane->ooby) {
    case NS_mapoob_loop: while (y<0) y+=plane->h; y%=plane->h; break;
    case NS_mapoob_repeat: if (y<0) y=0; else if (y>=plane->h) y=plane->h-1; break;
    case NS_mapoob_farloop: {
        int twice=plane->h<<1;
        int half=plane->h>>1;
        int adjy=y+half;
        while (adjy<0) adjy+=twice;
        adjy%=twice;
        y=adjy-half;
        if (y<0) y=0; else if (y>=plane->h) y=plane->h-1;
      } break;
    case NS_mapoob_null:
    default: if ((y<0)||(y>=plane->h)) return 0; break;
  }
  
  return plane->v+y*plane->w+x;
}

struct map *map_by_id(int rid) {
  if ((rid<0)||(rid>=maps.idixc)) return 0;
  const struct idix *idix=maps.idixv+rid;
  return map_by_position(idix->x,idix->y,idix->z);
}

/* Get some plane details.
 */
 
struct map *maps_get_plane(int *x,int *y,int *w,int *h,int *oobx,int *ooby,int z) {
  if ((z<0)||(z>=maps.planec)) {
    *x=*y=*w=*h=*oobx=*ooby=0;
    return 0;
  } else {
    const struct plane *plane=maps.planev+z;
    *x=plane->x;
    *y=plane->y;
    *w=plane->w;
    *h=plane->h;
    *oobx=plane->oobx;
    *ooby=plane->ooby;
    return plane->v;
  }
}

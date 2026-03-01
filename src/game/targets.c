/* targets.c
 * Compass support.
 */

#include "bellacopia.h"

/* Private globals.
 */
 
static struct {
  struct target {
    int compass; // NS_compass_*
    int fld; // If set, the target is hidden.
    int x,y,z; // Location in world. (x,y) are plane meters.
    int p1x,p1y; // Location in plane 1, predetermined as an optimization.
  } *v;
  int c,a;
  
  /* For cross-plane lookups, we also need an index of doors.
   * Every door in the world.
   * (x,y) are in plane meters, not map meters.
   * The list naturally sorts by (srcmapid) but nothing else.
   */
  struct door {
    int srcmapid,srcx,srcy,srcz;
    int dstmapid,dstx,dsty,dstz;
  } *doorv;
  int doorc,doora;
  
} targets={0};

/* Append blank target to the list.
 */
 
static struct target *targets_add() {
  if (targets.c>=targets.a) {
    int na=targets.a+256;
    if (na>INT_MAX/sizeof(struct target)) return 0;
    void *nv=realloc(targets.v,sizeof(struct target)*na);
    if (!nv) return 0;
    targets.v=nv;
    targets.a=na;
  }
  struct target *target=targets.v+targets.c++;
  memset(target,0,sizeof(struct target));
  return target;
}

static struct door *targets_add_door() {
  if (targets.doorc>=targets.doora) {
    int na=targets.doora+128;
    if (na>INT_MAX/sizeof(struct door)) return 0;
    void *nv=realloc(targets.doorv,sizeof(struct door)*na);
    if (!nv) return 0;
    targets.doorv=nv;
    targets.doora=na;
  }
  struct door *door=targets.doorv+targets.doorc++;
  memset(door,0,sizeof(struct door));
  return door;
}

/* Scan a map for targets.
 */
 
static int targets_scan_map(const struct map *map) {
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
    
      case CMD_map_target: {
          int strix=(cmd.arg[2]<<8)|cmd.arg[3];
          struct target *target=targets_add();
          if (!target) return -1;
          target->compass=strix;
          target->fld=0;
          target->x=map->lng*NS_sys_mapw+cmd.arg[0];
          target->y=map->lat*NS_sys_maph+cmd.arg[1];
          target->z=map->z;
        } break;
    
      case CMD_map_sprite: {
          int compass=0,fld=0;
          int rid=(cmd.arg[2]<<8)|cmd.arg[3];
          if (rid==RID_sprite_treasure) {
            int itemid=cmd.arg[4];
            switch (itemid) {
              case NS_itemid_heartcontainer: compass=NS_compass_heartcontainer; break;
              case NS_itemid_gold: compass=NS_compass_gold; break;
            }
            fld=(cmd.arg[6]<<8)|cmd.arg[7];
          } else if (rid==RID_sprite_rootdevil) {
            compass=NS_compass_rootdevil;
            fld=(cmd.arg[4]<<8)|cmd.arg[5];
          }
          if (compass) {
            struct target *target=targets_add();
            if (!target) return -1;
            target->compass=compass;
            target->fld=fld;
            target->z=map->z;
            target->x=map->lng*NS_sys_mapw+cmd.arg[0];
            target->y=map->lat*NS_sys_maph+cmd.arg[1];
          }
        } break;
    
      case CMD_map_compass: {
          struct target *target=targets_add();
          if (!target) return -1;
          target->compass=(cmd.arg[2]<<8)|cmd.arg[3];
          target->fld=(cmd.arg[4]<<8)|cmd.arg[5];
          target->z=map->z;
          target->x=map->lng*NS_sys_mapw+cmd.arg[0];
          target->y=map->lat*NS_sys_maph+cmd.arg[1];
        } break;
        
      case CMD_map_door: {
          struct door *door=targets_add_door();
          if (!door) return -1;
          door->srcmapid=map->rid;
          door->srcx=map->lng*NS_sys_mapw+cmd.arg[0];
          door->srcy=map->lat*NS_sys_maph+cmd.arg[1];
          door->srcz=map->z;
          door->dstmapid=(cmd.arg[2]<<8)|cmd.arg[3];
          const struct map *dstmap=map_by_id(door->dstmapid);
          if (!dstmap) return -1;
          door->dstx=dstmap->lng*NS_sys_mapw+cmd.arg[4];
          door->dsty=dstmap->lat*NS_sys_maph+cmd.arg[5];
          door->dstz=dstmap->z;
        } break;
    }
  }
  return 0;
}

/* Add a target for the center of the named map.
 */
 
static int target_add_map(int compass,int rid) {
  const struct map *map=map_by_id(rid);
  if (!map) return -1;
  struct target *target=targets_add();
  if (!target) return -1;
  target->compass=compass;
  target->z=map->z;
  target->x=map->lng*NS_sys_mapw+(NS_sys_mapw>>1);
  target->y=map->lat*NS_sys_maph+(NS_sys_maph>>1);
  return 0;
}

/* Sort targets.
 */
 
static int targetcmp(const void *A,const void *B) {
  const struct target *a=A,*b=B;
  if (a->compass<b->compass) return -1;//TODO numeric order here is not kosher, need an arbitrarily-specified order
  if (a->compass>b->compass) return 1;
  if (a->fld<b->fld) return -1; // When assigning fields, we can arrange for them to be in chronological order.
  if (a->fld>b->fld) return 1; // Probably would happen by accident anyway.
  return 0;
}

/* Find the position on plane (dstz) amenable to a target whose final position is (srcx,srcy,srcz).
 * (p1x,p1y) if you have them; <0 if not.
 */
 
static uint8_t plane_search_visited[32];
 
static int targets_find_plane_position(int *dstx,int *dsty,int px,int py,int dstz,int srcx,int srcy,int srcz,int p1x,int p1y,uint8_t *visited) {
  if ((dstz&~0xff)||(srcz&~0xff)) return -1; // Balk for invalid plane.

  // On the same nonzero plane? Great, we have the answer.
  if (dstz&&(dstz==srcz)) {
    *dstx=srcx;
    *dsty=srcy;
    return 0;
  }
  
  // If both on plane zero and (px,py) in the same map as (srcx,srcy), we can answer.
  if (!dstz&&!srcz&&(px>=0)&&(py>=0)) {
    int dstcol=px/NS_sys_mapw;
    int srccol=srcx/NS_sys_mapw;
    if (dstcol==srccol) {
      int dstrow=py/NS_sys_maph;
      int srcrow=srcy/NS_sys_maph;
      if (dstrow==srcrow) {
        *dstx=srcx;
        *dsty=srcy;
        return 0;
      }
    }
  }
  
  // If searching from plane one and (p1x,p1y) provided, use those.
  if ((dstz==1)&&(p1x>=0)&&(p1y>=0)) {
    *dstx=p1x;
    *dsty=p1y;
    return 0;
  }
  
  // If we've already visited this plane, stop.
  if (visited&&(visited[srcz>>3]&(1<<(srcz&7)))) return -1;
  
  // Going to need recursion. If they didn't give a visited list, use the static one.
  if (!visited) {
    memset(plane_search_visited,0,sizeof(plane_search_visited));
    visited=plane_search_visited;
  }
  visited[srcz>>3]|=1<<(srcz&7); // Mark the source plane visited.
  
  // Back thru every door from the source plane until we find a match.
  const struct door *door=targets.doorv;
  int i=targets.doorc;
  for (;i-->0;door++) {
    if (door->dstz!=srcz) continue;
    if (!srcz) { // Plane zero: Only proceed if the door's output position matches (srcx,srcy).
      if (door->dstx/NS_sys_mapw!=srcx/NS_sys_mapw) continue;
      if (door->dsty/NS_sys_maph!=srcy/NS_sys_maph) continue;
    }
    if (targets_find_plane_position(dstx,dsty,px,py,dstz,door->srcx,door->srcy,door->srcz,p1x,p1y,visited)>=0) {
      return 0;
    }
  }
  
  return -1;
}

/* Initialize.
 */
 
int game_init_targets() {
  if (targets.c) return 0;
  
  // Home and Magnetic North are just pointers to known maps, easy.
  if (target_add_map(NS_compass_home,RID_map_start)<0) return -1;
  if (target_add_map(NS_compass_north,RID_map_mn_int)<0) return -1;
  
  // Scan the map list for the rest.
  // This also populates (doorv).
  struct map **p=g.mapstore.byidv;
  int i=g.mapstore.byidc;
  for (;i-->0;p++) {
    struct map *map=*p;
    if (!map) continue;
    if (targets_scan_map(map)<0) return -1;
  }
  
  // Sort targets so identical (compass) are adjacent, and otherwise in the recommendation order.
  qsort(targets.v,targets.c,sizeof(struct target),targetcmp);
  
  // Fill in (p1x,p1y) for each unfinished target so we're not asking again and again.
  struct target *target=targets.v;
  for (i=targets.c;i-->0;target++) {
    if (target->z==1) {
      target->p1x=target->x;
      target->p1y=target->y;
    } else if (!store_get_fld(target->fld)) {
      targets_find_plane_position(&target->p1x,&target->p1y,-1,-1,1,target->x,target->y,target->z,-1,-1,0);
    }
  }
  
  return 0;
}

/* Integer in possibly-unsorted list.
 */
 
static int int_present(const int *v,int c,int q) {
  for (;c-->0;v++) if (*v==q) return 1;
  return 0;
}

/* Target enablement.
 * Any strix in RID_strings_item can be used as a target.
 * By default, they are always available.
 * Call out the upgrade-required targets here.
 */
 
static const struct upgrade { int strix,fld; } upgradev[]={
  {NS_compass_heartcontainer,NS_fld_target_hc},
  {NS_compass_rootdevil,NS_fld_target_rootdevil},
  {NS_compass_sidequest,NS_fld_target_sidequest},
  {NS_compass_gold,NS_fld_target_gold},
};
 
static int target_is_enabled(const struct target *target) {
  const struct upgrade *upgrade=upgradev;
  int i=sizeof(upgradev)/sizeof(upgradev[0]);
  for (;i-->0;upgrade++) {
    if (upgrade->strix==target->compass) return store_get_fld(upgrade->fld);
  }
  return 1;
}
 
void game_disable_all_targets() {
  const struct upgrade *upgrade=upgradev;
  int i=sizeof(upgradev)/sizeof(upgradev[0]);
  for (;i-->0;upgrade++) {
    store_set_fld(upgrade->fld,0);
  }
}

void game_enable_target(int strix) {
  const struct upgrade *upgrade=upgradev;
  int i=sizeof(upgradev)/sizeof(upgradev[0]);
  for (;i-->0;upgrade++) {
    if (upgrade->strix==strix) {
      store_set_fld(upgrade->fld,1);
      return;
    }
  }
}

/* List all available targets.
 */
 
int game_list_targets(int *dstv,int dsta,int mode) {
  if (!dstv||(dsta<1)) return 0;
  int dstc=0;
  #define ADD(strix) { \
    dstv[dstc++]=strix; \
    if (dstc>=dsta) return dstc; \
  }
  
  /* A few options are always available.
   */
  if ((mode==TARGET_MODE_AVAILABLE)||(mode==TARGET_MODE_ALL)) {
    ADD(NS_compass_auto)
    ADD(NS_compass_north)
    ADD(NS_compass_home)
  }
  
  /* Run through the list of targets and add any (compass) value we don't have yet.
   */
  const struct target *target=targets.v;
  int i=0;
  while (i<targets.c) {
    #define NEXTCOMPASS { \
      int compass=target->compass; \
      while ((i<targets.c)&&(target->compass==compass)) { \
        i++; \
        target++; \
      } \
    }
    if (target->compass==NS_compass_castle) {
      NEXTCOMPASS
      continue;
    }
    // Don't include the same thing twice, of course.
    if (int_present(dstv,dstc,target->compass)) {
      NEXTCOMPASS
      continue;
    }
    // New mode. Should we list it? (fld) kind of overrides everything.
    int relevant=0;
    if (store_get_fld(target->fld)) {
      relevant=(mode==TARGET_MODE_ALL);
    } else {
      switch (mode) {
        case TARGET_MODE_AVAILABLE: relevant=target_is_enabled(target); break;
        case TARGET_MODE_ALL: relevant=1; break;
        case TARGET_MODE_UPGRADE: relevant=!target_is_enabled(target); break;
      }
    }
    if (relevant) {
      ADD(target->compass)
      NEXTCOMPASS
      continue;
    }
    i++;
    target++;
    #undef NEXTCOMPASS
  }
  
  #undef ADD
  return dstc;
}

/* Let the compass choose a target.
 * Always returns something valid. "Home" if there isn't anything interesting.
 */
 
static int game_autochoose_target(int px,int py,int z) {
  
  const struct target *target=targets.v;
  int i=targets.c;
  for (;i-->0;target++) {
    switch (target->compass) {
      // Don't suggest these.
      case NS_compass_north:
      case NS_compass_home:
      case NS_compass_castle:
        break;
      // First other thing with an unset flag, that's our answer.
      default: if (!store_get_fld(target->fld)) return target->compass;
    }
  }
  
  // If nothing else, Home is always a sensible option.
  return NS_compass_home;
}

/* Get position of target in a given plane.
 */

int game_get_target_position(int *lng,int *lat,int px,int py,int z,int strix) {

  /* "Compass's Choice" resolves immediately to some other valid target.
   */
  if (strix==NS_compass_auto) {
    strix=game_autochoose_target(px,py,z);
  }
  
  
  /* Resolve the target's absolute position, not necessarily on the requested plane.
   */
  const struct target *target=0;
  const struct target *q=targets.v;
  int i=targets.c;
  for (;i-->0;q++) {
    if (q->compass!=strix) continue;
    if (store_get_fld(q->fld)) continue;
    target=q;
    break;
  }
  if (!target) return -1;
  
  /* Find a path from (z) to (target->z) and return the location of the door on (z).
   */
  return targets_find_plane_position(lng,lat,px,py,z,target->x,target->y,target->z,target->p1x,target->p1y,0);
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
  if (mx<0) { mw+=mx; mx=0; }
  if (my<0) { mh+=my; my=0; }
  if (mx>plane->w-mw) mw=plane->w-mx;
  if (my>plane->h-mh) mh=plane->h-my;
  
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

/* General context-sensitive advice.
 */

int game_get_advice(char *dst,int dsta) {
  if (!dst||(dsta<0)) dsta=0;
  //TODO Context-sensitive advice.
  return text_format_res(dst,dsta,RID_strings_advice,1+rand()%3,0,0);
}

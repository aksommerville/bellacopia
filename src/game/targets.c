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
        
      case CMD_map_door:
      case CMD_map_burieddoor: {
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
 
static int ordercmp(int a,int b,const int *order,int c) {
  if (a==b) return 0; // Equal is equal.
  for (;c-->0;order++) { // First we find in (order) comes first.
    if (*order==a) return -1;
    if (*order==b) return 1;
  }
  // Both unlisted. Shouldn't happen but if so, compare them numerically.
  if (a<b) return -1;
  if (a>b) return 1;
  return 0;
}
 
static int targetcmp(const void *A,const void *B) {
  const struct target *a=A,*b=B;
  if (a->compass<b->compass) return -1;
  if (a->compass>b->compass) return 1;
  switch (a->compass) {
    case NS_compass_rootdevil: {
        const int order[]={
          NS_fld_root1, // meadow
          NS_fld_root2, // fractia
          NS_fld_root5, // battlefield
          NS_fld_root6, // goblins
          NS_fld_root7, // desert
          NS_fld_root3, // south jungle
          NS_fld_root4, // temple
        };
        return ordercmp(a->fld,b->fld,order,sizeof(order)/sizeof(int));
      }
    case NS_compass_heartcontainer: {
        const int order[]={
          NS_fld_hc1, // castle shop
          NS_fld_hc2, // temple pool
          NS_fld_hc4, // south jungle
          NS_fld_hc3, // inventory critic -- should be last, very hard to qualify
          //NS_fld_hc5,
        };
        return ordercmp(a->fld,b->fld,order,sizeof(order)/sizeof(int));
      }
  }
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
    if (door->dstz==NS_plane_tunnel1) {
      // Recommend the tunnels only if our final target is down there.
      // Otherwise, suggest a path in the outerworld, even if it's not navigable.
      if (dstz!=NS_plane_tunnel1) continue;
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
  
  //XXX Show me the root devils in order.
  if (0) {
    fprintf(stderr,"%s ready. Root devils in order:\n",__func__);
    for (i=targets.c,target=targets.v;i-->0;target++) {
      if (target->compass==NS_compass_rootdevil) {
        const char *desc="?";
        switch (target->fld) {
          case NS_fld_root1: desc="meadow"; break;
          case NS_fld_root2: desc="fractia"; break;
          case NS_fld_root3: desc="south jungle"; break;
          case NS_fld_root4: desc="temple"; break;
          case NS_fld_root5: desc="battlefield"; break;
          case NS_fld_root6: desc="goblins"; break;
          case NS_fld_root7: desc="desert"; break;
        }
        fprintf(stderr,"  %d (%s)\n",target->fld,desc);
      }
      // Heart containers:
      // 1: Desert
      // 2: Temple Pool
      // 3: Inventory Judge
      // 4: South Jungle
      // 5: 
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

/* Hints override.
 * Similar to target and secret but totally different authority.
 */
 
int game_get_hints_override_position(int *x,int *y,int fld) {
  const struct map *map=g.camera.map;
  if (!map||(map->hints_override!=fld)) return -1;
  if (store_get_fld(fld)) return -1; // Once the key field is set, the override is no longer in play.
  
  /* Search sprites for ones that serve as hint authorities.
   * It's OK to use more than one per map, kind of, if they're distinguished by fldid and somehow know how to not interfere with each other.
   * I don't expect that to happen.
   */
  struct sprite **spritep=GRP(update)->sprv;
  int i=GRP(update)->sprc;
  for (;i-->0;spritep++) {
    struct sprite *sprite=*spritep;
    
    if (sprite->type==&sprite_type_treadlepass) {
      if (sprite_treadlepass_hint(x,y,sprite,fld)>=0) return 0;
      continue;
    }
    
    if (sprite->type==&sprite_type_trickfloor) {
      if (sprite_trickfloor_hint(x,y,sprite,fld)>=0) return 0;
      continue;
    }
    
  }
  return -1;
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
    
      /* buriedtreasure and burieddoor are exactly the same thing, except fldid in a different place.
       */
      case CMD_map_buriedtreasure:
      case CMD_map_burieddoor: {
          int fldid;
          if (cmd.opcode==CMD_map_burieddoor) {
            fldid=(cmd.arg[6]<<8)|cmd.arg[7];
          } else {
            fldid=(cmd.arg[2]<<8)|cmd.arg[3];
          }
          if (store_get_fld(fldid)) break; // Already got; skip it.
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

/* Nonzero if any field in the given inclusive range is unset.
 */
 
static int any_fld_unset(int a,int z) {
  for (;a<=z;a++) {
    if (!store_get_fld(a)) return 1;
  }
  return 0;
}

/* General context-sensitive advice, for crystal ball.
 */

// RID_strings_advice. Never fails to return something sensible.
static int game_get_advice_strix() {

  /* Early on, prefer to point her toward the Root Devils.
   * Other advice is interleaved here, as the preferred path warrants.
   */
  if (!store_get_fld(NS_fld_root1)) return 2; // Meadow.
  if (!store_get_itemid(NS_itemid_divining)) return 3; // Get the divining rod.
  if (!store_get_fld(NS_fld_war_over)) return 4; // End the war.
  if (!store_get_fld(NS_fld_root5)) return 5; // You ended the war but forgot the root devil!
  // Should we guide her toward endorsements, or is that obvious enough?
  if (!store_get_fld(NS_fld_mayor)) return 6; // Run for mayor.
  if (!store_get_fld(NS_fld_root2)) return 7; // You moved the log but forgot the root devil!
  if (!store_get_fld(NS_fld_root3)) return 8; // Get the south-jungle root devil.
  if (!store_get_fld(NS_fld_root7)) return 9; // Get the east-desert root devil.
  if (store_get_fld(NS_fld_kidnapped)&&!store_get_fld(NS_fld_escaped)) return 10; // Escape from the goblins!
  if (!store_get_fld(NS_fld_root6)) return 11; // Root devil by the goblins' cave.
  if (!store_get_fld(NS_fld_root4)) { // Temple root devil.
    int statuemaze=store_get_fld16(NS_fld16_statuemaze_seed);
    int labyrinth=store_get_fld16(NS_fld16_labyrinth_seed);
    if (!statuemaze) return 12; // Statue maze not visited. Presumably they haven't entered the temple at all.
    if (!labyrinth) return 13; // Labyrinth not visited. Presumably they're stuck on the statue maze.
    return 12; // She's been into the labyrinth. Probably failed to complete it. Go back to the generic "there's a root devil in the temple."
  }
  
  /* All the root devils are strangled, so now we're in the weird position of suggesting a next step when there's really no linear order.
   * (not that there was a linear order to the root devils either).
   * This is going to be opinionated and imprecise any way we slice it.
   * I guess interleave the various kinds of target. So we'll suggest a heart container, then a zookeeper, then a bridge, keep it fresh.
   * Don't advise on buried treasure or jigpieces. There are other ways to hint toward those.
   */
  int have_broom=(store_get_itemid(NS_itemid_broom)?1:0);
  if (!store_get_fld(NS_fld_hc1)) return 14; // Castleshop heart container.
  if (!store_get_fld(NS_fld_rescued_princess)) return 15; // Rescue the Princess!
  if (have_broom&&!store_get_fld(NS_fld_race1win)) return 69; // Cheapside race.
  if (!store_get_fld(NS_fld_barrelhat1)||!store_get_fld(NS_fld_barrelhat2)||!store_get_fld(NS_fld_barrelhat4)) return 16; // Botire barrel hats.
  if (have_broom&&!store_get_fld(NS_fld_race2win)) return 70; // Downstairs lake race.
  if (!store_get_fld(NS_fld_potion_book)) return 17; // Buy the potions book.
  if (have_broom&&!store_get_fld(NS_fld_race3win)) return 71; // Tundra race.
  if (!store_get_fld(NS_fld_bridge1done)) return 18; // Stick bridge by forest.
  if (have_broom&&!store_get_fld(NS_fld_race4win)) return 72; // Botire race.
  if (any_fld_unset(NS_fld_zoo1_0,NS_fld_zoo1_3)) return 19; // Meadow zookeeper.
  if (have_broom&&!store_get_fld(NS_fld_race5win)) return 73; // Desert race.
  if (!store_get_itemid(NS_itemid_fishpole)) return 38; // Recommend fishpole before recommending anything underwater!
  if (!store_get_fld(NS_fld_hc2)) return 20; // Temple pool heart container.
  if (have_broom&&!store_get_fld(NS_fld_race6win)) return 74; // North underground race.
  if (!store_get_fld(NS_fld_barrelhat3)||!store_get_fld(NS_fld_barrelhat7)) return 21; // Tundra barrels. They're not close to each other but whatever.
  if (!store_get_fld(NS_fld_bridge6done)||!store_get_fld(NS_fld_bridge7done)) return 22; // Telescope and Green Fish bridges off Botire.
  if (!store_get_fld(NS_fld_hc4)) return 23; // South jungle heart container.
  if (!store_get_fld(NS_fld_barrelhat5)) return 24; // Cheapside barrel hat.
  if (!store_get_fld(NS_fld_bridge3done)||!store_get_fld(NS_fld_bridge4done)) return 25; // Two bridges off the east river (candy and pepper).
  if (!store_get_fld(NS_fld_barrelhat6)||!store_get_fld(NS_fld_barrelhat8)) return 26; // Fractia barrels.
  if (!store_get_fld(NS_fld_bridge5done)) return 27; // Gold bridge.
  if (!store_get_fld(NS_fld_barrelhat9)) return 28; // Castle barrel.
  if (!store_get_fld(NS_fld_bridge2done)) return 29; // Match bridge.

  /* Goblins' cave.
   */
  if (!store_get_fld(NS_fld_stardoor)) {
    if (!store_get_fld(NS_fld_started_crypto)) return 68; // Seek the goblins' treasure!
    int gotbone=store_get_fld(NS_fld_bonedoor);
    int gotleaf=store_get_fld(NS_fld_leafdoor);
    if (!gotbone&&!gotleaf) {
      if (!store_get_fld(NS_fld_bought_alphabet)&&!store_get_fld(NS_fld_bought_translation)) return 30; // Ask linguist.
    }
    if (!gotbone||!gotleaf) return 31; // Open the first two doors.
    return 32; // Stuck after the first two doors.
  }
  
  /* Any item missing, say something about it.
   * Only when all of the final items are acquired, tell them about the inventory critic.
   * Some of these messages will be unlikely or impossible, due to other things we've advised. (eg you've built the match bridge, so you do have match already).
   */
  {
    int gotall=1; // Only proceed with the per-item checks if there's an open slot.
    const struct invstore *invstore=g.store.invstorev;
    int i=26;
    for (;i-->0;invstore++) {
      if (!invstore->itemid) {
        gotall=0;
        break;
      }
    }
    if (!gotall) {
      const struct itemtag {
        int itemid;
        int strix;
      } itemtagv[26]={
        {NS_itemid_stick        ,33},
        {NS_itemid_broom        ,34},
        {NS_itemid_divining     ,3}, // We advised for this already; listing again for the sake of completeness.
        {NS_itemid_match        ,36},
        {NS_itemid_wand         ,37},
        {NS_itemid_fishpole     ,38},
        {NS_itemid_bugspray     ,39},
        {NS_itemid_potion       ,40},
        {NS_itemid_hookshot     ,41},
        {NS_itemid_candy        ,42},
        {NS_itemid_magnifier    ,43},
        {NS_itemid_vanishing    ,44},
        {NS_itemid_compass      ,45},
        {NS_itemid_bell         ,46},
        {NS_itemid_telescope    ,47},
        {NS_itemid_shovel       ,48},
        {NS_itemid_pepper       ,49},
        {NS_itemid_bomb         ,50},
        {NS_itemid_stopwatch    ,51},
        {NS_itemid_busstop      ,52},
        {NS_itemid_snowglobe    ,53},
        {NS_itemid_tapemeasure  ,54},
        {NS_itemid_phonograph   ,55},
        {NS_itemid_crystal      ,56},
        {NS_itemid_glove        ,57},
        {NS_itemid_marionette   ,58},
      };
      const struct itemtag *itemtag=itemtagv;
      for (i=26;i-->0;itemtag++) {
        if (store_get_itemid(itemtag->itemid)) continue;
        return itemtag->strix;
      }
    }
  }
  if (!store_get_fld(NS_fld_hc3)) return 59; // Inventory critic.
  
  /* If any story is got but untold, hint at one of the unsatisfied trees.
   */
  {
    int i=0,have_story=0;
    for (;;i++) {
      const struct story *story=story_by_index(i);
      if (!story) break;
      if (store_get_fld(story->fld_told)) continue;
      if (store_get_fld(story->fld_present)) {
        have_story=1;
        break;
      }
    }
    if (have_story) {
      if (any_fld_unset(NS_fld_tree1,NS_fld_tree7)) return 60; // Forest.
      if (any_fld_unset(NS_fld_tree13,NS_fld_tree16)) return 61; // Tundra.
      if (any_fld_unset(NS_fld_tree8,NS_fld_tree9)) return 62; // Battlefield.
      if (!store_get_fld(NS_fld_tree10)) return 63; // Jungle.
      if (!store_get_fld(NS_fld_tree11)) return 64; // Desert.
      if (!store_get_fld(NS_fld_tree12)) return 65; // Mountains.
    }
  }
  
  /* If the maps are incomplete, give a generic complaint.
   * We don't guide to specific jigpieces; the whole idea of the jigsaw puzzle map is it guides you to missing pieces implicitly.
   */
  if (!jigstore_is_complete()) return 66;
  
  /* If the cartographer has further advice to give, recommend asking him.
   */
  if (cartographer_has_advice()) return 67;
  
  return 1; // "You did everything!"
}

int game_get_advice(char *dst,int dsta) {
  if (!dst||(dsta<0)) dsta=0;
  int strix=game_get_advice_strix();
  return text_format_res(dst,dsta,RID_strings_advice,strix,0,0);
}

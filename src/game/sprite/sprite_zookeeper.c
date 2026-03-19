/* sprite_zookeeper.c
 * Triggers some dialogue explaining the zookeeping challenge.
 * Also responsible for detecting when a target animal has been delivered.
 */
 
#include "game/bellacopia.h"

#define COOLDOWN 0.500

struct sprite_zookeeper {
  struct sprite hdr;
  int fld;
  double cooldown;
  double zl,zr,zt,zb; // Catch zone, in plane meters. Compare to monsters' midpoints.
  int *wantv; // spriteid, for the outstanding ones only
  int wantc,wanta;
};

#define SPRITE ((struct sprite_zookeeper*)sprite)

/* Cleanup.
 */
 
static void _zookeeper_del(struct sprite *sprite) {
  if (SPRITE->wantv) free(SPRITE->wantv);
}

/* List of wanted sprites, outstanding only.
 */
 
static int zookeeper_wantv_search(const struct sprite *sprite,int spriteid) {
  int lo=0,hi=SPRITE->wantc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (spriteid<SPRITE->wantv[ck]) hi=ck;
    else if (spriteid>SPRITE->wantv[ck]) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int zookeeper_wantv_add(struct sprite *sprite,int spriteid) {
  int p=zookeeper_wantv_search(sprite,spriteid);
  if (p>=0) return 0;
  p=-p-1;
  if (SPRITE->wantc>=SPRITE->wanta) {
    int na=SPRITE->wanta+16;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(SPRITE->wantv,sizeof(int)*na);
    if (!nv) return -1;
    SPRITE->wantv=nv;
    SPRITE->wanta=na;
  }
  memmove(SPRITE->wantv+p+1,SPRITE->wantv+p,sizeof(int)*(SPRITE->wantc-p));
  SPRITE->wantc++;
  SPRITE->wantv[p]=spriteid;
  return 1;
}

/* Find my catch zone (zl,zr,zt,zb).
 * There must be a safe tile within my 3x3 box, and it must be part of a contiguous rectangle.
 * The catch zone must be all on the same map.
 */
 
static int box_is_safe(const struct map *map,int x,int y,int w,int h) {
  const uint8_t *row=map->v+y*NS_sys_mapw+x;
  for (;h-->0;row+=NS_sys_mapw) {
    const uint8_t *p=row;
    int xi=w;
    for (;xi-->0;p++) {
      if (map->physics[*p]!=NS_physics_safe) return 0;
    }
  }
  return 1;
}
 
// (qx,qy) in meters. If we succeed, we'll have set the catch zone to a box containing (qx,qy).
static int zookeeper_find_catch_zone_1(struct sprite *sprite,const struct plane *plane,int qx,int qy) {
  // Confirm it's within the plane:
  if ((qx<0)||(qy<0)) return -1;
  int wm=plane->w*NS_sys_mapw;
  int hm=plane->h*NS_sys_maph;
  if ((qx>=wm)||(qy>=hm)) return -1;
  // Confirm the focus cell is safe:
  int lng=qx/NS_sys_mapw;
  int lat=qy/NS_sys_maph;
  const struct map *map=plane->v+lat*plane->w+lng;
  if (!map->physics) return -1;
  int subx=qx-lng*NS_sys_mapw;
  int suby=qy-lat*NS_sys_maph;
  if (map->physics[map->v[suby*NS_sys_mapw+subx]]!=NS_physics_safe) return -1;
  // Expand the box:
  int subw=1;
  int subh=1;
  while (subx&&box_is_safe(map,subx-1,suby,1,1)) { subx--; subw++; }
  while (suby&&box_is_safe(map,subx,suby-1,subw,1)) { suby--; subh++; }
  while ((subx+subw<NS_sys_mapw)&&box_is_safe(map,subx+subw,suby,1,subh)) subw++;
  while ((suby+subh<NS_sys_maph)&&box_is_safe(map,subx,suby+subh,subw,1)) subh++;
  // Choose floating-point inset:
  const double inset=0.400;
  SPRITE->zl=lng*NS_sys_mapw+subx;
  SPRITE->zr=SPRITE->zl+subw;
  SPRITE->zt=lat*NS_sys_maph+suby;
  SPRITE->zb=SPRITE->zt+subh;
  SPRITE->zl+=inset;
  SPRITE->zt+=inset;
  SPRITE->zr-=inset;
  SPRITE->zb-=inset;
  return 0;
}
 
static int zookeeper_find_catch_zone(struct sprite *sprite) {
  const struct plane *plane=plane_by_position(sprite->z);
  if (!plane) return -1;
  int px=(int)sprite->x;
  int py=(int)sprite->y;
  int suby=-2;
  for (;suby<=2;suby++) {
    int subx=-2;
    for (;subx<=2;subx++) {
      if (zookeeper_find_catch_zone_1(sprite,plane,px+subx,py+suby)>=0) return 0;
    }
  }
  return -1;
}

/* Init.
 */
 
static int _zookeeper_init(struct sprite *sprite) {
  SPRITE->fld=(sprite->arg[0]<<8)|sprite->arg[1];
  if (zookeeper_find_catch_zone(sprite)<0) {
    fprintf(stderr,"%s:WARNING: Failed to locate catch zone.\n",__func__);
  }
  
  int fld=SPRITE->fld;
  int i=zoo_get_count(fld);
  for (;i-->0;fld++) {
    if (store_get_fld(fld)) continue;
    int spriteid=zoo_get_spriteid(fld);
    if (zookeeper_wantv_add(sprite,spriteid)<0) return -1;
  }
  
  return 0;
}

/* Determine the field id for this sprite id and set it.
 * Poke the ticker if we find one.
 * Other fanfare?
 * Caller is responsible for the animal's live sprite and our want list.
 */
 
static void zookeeper_acknowledge_capture(struct sprite *sprite,int rid,struct sprite *animal) {
  int fld=SPRITE->fld;
  int i=zoo_get_count(fld);
  int ok=0;
  for (;i-->0;fld++) {
    if (zoo_get_spriteid(fld)==rid) {
      ok=1;
      break;
    }
  }
  if (ok) {
    store_set_fld(fld,1);
  }
  
  struct sprite **otherp=GRP(solid)->sprv;
  i=GRP(solid)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other->type==&sprite_type_ticker) {
      char tmp[1024];
      int tmpc=zoo_get_ticker_text(tmp,sizeof(tmp),SPRITE->fld);
      if ((tmpc<0)||(tmpc>sizeof(tmp))) tmpc=0;
      sprite_ticker_set_text(other,tmp,tmpc);
    }
  }
  
  //TODO sound, fireworks, reward
  game_get_item(NS_itemid_gold,10);
}

/* Update.
 */
 
static void _zookeeper_update(struct sprite *sprite,double elapsed) {
  if (SPRITE->cooldown>0.0) {
    SPRITE->cooldown-=elapsed;
  }
  
  /* Check all monsters in my catch zone.
   */
  struct sprite **otherp=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other->type!=&sprite_type_monster) continue;
    if (other->x<SPRITE->zl) continue;
    if (other->x>SPRITE->zr) continue;
    if (other->y<SPRITE->zt) continue;
    if (other->y>SPRITE->zb) continue;
    int wantp=zookeeper_wantv_search(sprite,other->rid);
    if (wantp<0) continue;
    
    zookeeper_acknowledge_capture(sprite,other->rid,other);
    sprite_kill_soon(other);
    SPRITE->wantc--;
    memmove(SPRITE->wantv+wantp,SPRITE->wantv+wantp+1,sizeof(int)*(SPRITE->wantc-wantp));
    break;
  }
}

/* Have we caught all the animals?
 */
 
static int zookeeper_caught_em_all(struct sprite *sprite) {
  int fld=SPRITE->fld;
  int c=zoo_get_count(fld);
  for (;c-->0;fld++) {
    if (!store_get_fld(fld)) return 0;
  }
  return 1;
}

/* Collide.
 */
 
static void _zookeeper_collide(struct sprite *sprite,struct sprite *other) {
  if (SPRITE->cooldown>0.0) return;
  if (other->type==&sprite_type_hero) {
    if (zookeeper_caught_em_all(sprite)) {
      game_begin_activity(NS_activity_dialogue,117,sprite);
    } else {
      game_begin_activity(NS_activity_dialogue,116,sprite);
    }
    SPRITE->cooldown=COOLDOWN;
    sprite_hero_unanimate(other);
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_zookeeper={
  .name="zookeeper",
  .objlen=sizeof(struct sprite_zookeeper),
  .del=_zookeeper_del,
  .init=_zookeeper_init,
  .update=_zookeeper_update,
  .collide=_zookeeper_collide,
};

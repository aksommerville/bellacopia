/* sprite_zookeeper.c
 * Triggers some dialogue explaining the zookeeping challenge.
 * Also responsible for detecting when a target animal has been delivered.
 * My map must contain one `zookeeper @X,Y,W,H` command to mark the delivery zone.
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

/* Find catch zone.
 * The map must contain a command `zookeeper @X,Y,W,H` marking my carpet.
 * Limit one zookeeper per map, I don't imagine that will be a problem.
 * Errors needn't be fatal. We'll eat logged errors and only return <0 if we failed without logging.
 */
 
static int zookeeper_find_catch_zone(struct sprite *sprite) {
  const double inset=0.400;
  struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  int x0=map->lng*NS_sys_mapw;
  int y0=map->lat*NS_sys_maph;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_zookeeper) {
      int x=cmd.arg[0],y=cmd.arg[1],w=cmd.arg[2],h=cmd.arg[3];
      SPRITE->zl=x0+x+inset;
      SPRITE->zt=y0+y+inset;
      SPRITE->zr=x0+x+w-inset;
      SPRITE->zb=y0+y+h-inset;
      return 0;
    }
  }
  fprintf(stderr,"%s:WARNING: No 'zookeeper' command in map:%d\n",__func__,map->rid);
  return 0;
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
  game_get_item(NS_itemid_gold,5);
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

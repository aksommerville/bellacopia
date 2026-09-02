/* sprite_figureeight.c
 * Not a visible sprite.
 * Measures to the nearest solids left and right.
 * Then tracks the hero -- if she completes a fast figure-eight whose waist is in my crossing zone, summon the "eight" sprite.
 * It's a simple thing to do, and is unlikely to happen by accident.
 */
 
#include "game/bellacopia.h"

#define TRAVEL_LIMIT 6 /* Five crossings completes a figure-eight, but call it six for safety. */
#define RESET_TIME 1.000 /* Maximum time between crossings. Experimentally, it takes roughly 0.500 on a broom, for a near set of posts. */

struct sprite_figureeight {
  struct sprite hdr;
  uint8_t orient; // 0,1 = horz,vert
  double lo,hi; // x or y in plane meters, depending on orient.
  int herotrack; // -1,1. 0 only at the first update (or if the hero mysteriously vanishes)
  double clock; // Counts up always, and resets at each crossing. Gets too high, we reset everything.
  int travelv[TRAVEL_LIMIT]; // -1,0,1. History of crossings, if you read backward from (travelp).
  int travelp;
};

#define SPRITE ((struct sprite_figureeight*)sprite)

/* Test one cell.
 */
 
static int figureeight_is_post(const struct map *map,int x,int y) {
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return 0;
  uint8_t tileid=map->v[y*NS_sys_mapw+x];
  switch (map->physics[tileid]) {
    case NS_physics_solid:
    case NS_physics_grabbable:
    case NS_physics_vanishable:
      return 1;
  }
  return 0;
}

static int figureeight_is_post_sprite(int rid) {
  switch (rid) {
    case RID_sprite_siren:
      return 1;
  }
  return 0;
}

/* Init.
 */
 
static int _figureeight_init(struct sprite *sprite) {
  SPRITE->orient=sprite->arg[0];
  
  /* Locate my posts.
   * They must be on this map, must have physics (solid,grabble,vanishable), and must be aligned with my starting position.
   */
  const struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  int x=(int)sprite->x-map->lng*NS_sys_mapw;
  int y=(int)sprite->y-map->lat*NS_sys_maph;
  if ((x<0)||(x>=NS_sys_mapw)||(y<0)||(y>=NS_sys_maph)) return -1;
  
  /* Actually, the posts can be either a sprite or a solid.
   * For the initial use case, they're sirens in the open sea.
   * To reduce the amount of extra work necessary, sprites usable as figureeight posts will have to be called out by id, right here.
   */
  int sl=-1,sr=NS_sys_mapw,st=-1,sb=NS_sys_maph;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_sprite) {
      int sx=cmd.arg[0],sy=cmd.arg[1];
      if ((SPRITE->orient==0)&&(sy!=y)) continue;
      if ((SPRITE->orient==1)&&(sx!=x)) continue;
      int rid=(cmd.arg[2]<<8)|cmd.arg[3];
      if (figureeight_is_post_sprite(rid)) {
        switch (SPRITE->orient) {
          case 0: {
              if (sx<x) {
                if (sx>sl) sl=sx;
              } else if (sx>x) {
                if (sx<sr) sr=sx;
              }
            } break;
          case 1: {
              if (sy<0) {
                if (sy>st) st=sy;
              } else if (sy>y) {
                if (sy<sb) sb=sy;
              }
            } break;
        }
      }
    }
  }
  
  switch (SPRITE->orient) {
    case 0: { // Horizontal.
        int lo=x,hi=x;
        for (;;lo--) {
          if (lo<0) return -1;
          if (lo==sl) break;
          if (figureeight_is_post(map,lo,y)) break;
        }
        for (;;hi++) {
          if (hi>=NS_sys_mapw) return -1;
          if (hi==sr) break;
          if (figureeight_is_post(map,hi,y)) break;
        }
        if (lo>=hi) return -1;
        SPRITE->lo=map->lng*NS_sys_mapw+lo+0.5;
        SPRITE->hi=map->lng*NS_sys_mapw+hi+0.5;
      } break;
    case 1: { // Vertical.
        int lo=y,hi=y;
        for (;;lo--) {
          if (lo<0) return -1;
          if (lo==st) break;
          if (figureeight_is_post(map,x,lo)) break;
        }
        for (;;hi++) {
          if (hi>=NS_sys_maph) return -1;
          if (hi==sb) break;
          if (figureeight_is_post(map,x,hi)) break;
        }
        if (lo>=hi) return -1;
        SPRITE->lo=map->lat*NS_sys_maph+lo+0.5;
        SPRITE->hi=map->lat*NS_sys_maph+hi+0.5;
      } break;
    default: return -1;
  }
  
  return 0;
}

/* She just completed a figure-eight. Do the thing.
 */
 
static void figureeight_dance_finished(struct sprite *sprite) {
  SPRITE->clock=0.0;
  SPRITE->travelp=0;
  memset(SPRITE->travelv,0,sizeof(SPRITE->travelv));
  
  // If an eightspawn or eight already exists, forget it.
  struct sprite **otherp=GRP(update)->sprv;
  int i=GRP(update)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->rid==RID_sprite_eightspawn) return;
    if (other->rid==RID_sprite_eight) return;
  }
  
  struct sprite *spawn=sprite_spawn(sprite->x,sprite->y,RID_sprite_eightspawn,0,0,0,0,0);
}

/* Update.
 */
 
static void _figureeight_update(struct sprite *sprite,double elapsed) {

  /* Tick the clock, and when it crosses some threshold, forget everything.
   */
  SPRITE->clock+=elapsed;
  if (SPRITE->clock>RESET_TIME) {
    SPRITE->clock=0.0;
    SPRITE->travelp=0;
    memset(SPRITE->travelv,0,sizeof(SPRITE->travelv));
  }

  /* Find the hero. Get out if we can't.
   */
  if (GRP(hero)->sprc<1) {
    SPRITE->herotrack=0;
    return;
  }
  struct sprite *hero=GRP(hero)->sprv[0];
  
  /* Check which side she's currently on.
   */
  int side=0;
  switch (SPRITE->orient) {
    case 0: side=(hero->y<sprite->y)?-1:1; break;
    case 1: side=(hero->x<sprite->x)?-1:1; break;
  }
  if (!side) return;
  
  /* If it's the first update, or side didn't change, get out.
   * This will be the overwhelming majority of updates.
   */
  if (!SPRITE->herotrack) {
    SPRITE->herotrack=side;
    return;
  }
  if (SPRITE->herotrack==side) return;
  SPRITE->herotrack=side;
  
  /* A crossing has occurred.
   * Record it in (travelv) and reset (clock).
   */
  int region=0;
  switch (SPRITE->orient) {
    case 0: if (hero->x<SPRITE->lo) region=-1; else if (hero->x>SPRITE->hi) region=1; break;
    case 1: if (hero->y<SPRITE->lo) region=-1; else if (hero->y>SPRITE->hi) region=1; break;
  }
  SPRITE->travelv[SPRITE->travelp++]=region;
  if (SPRITE->travelp>=TRAVEL_LIMIT) SPRITE->travelp=0;
  SPRITE->clock=0.0;
  
  /* Check whether a figure-eight was just completed.
   */
  int p=SPRITE->travelp,i=5;
  char norm[5];
  while (i-->0) {
    p--;
    if (p<0) p=TRAVEL_LIMIT-1;
    norm[i]=(SPRITE->travelv[p]<0)?'A':(SPRITE->travelv[p]>0)?'Z':'-';
  }
  if (
    !memcmp(norm,"-A-Z-",5)||
    !memcmp(norm,"-Z-A-",5)
  ) figureeight_dance_finished(sprite);
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_figureeight={
  .name="figureeight",
  .objlen=sizeof(struct sprite_figureeight),
  .init=_figureeight_init,
  .update=_figureeight_update,
};

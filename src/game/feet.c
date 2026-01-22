#include "bellacopia.h"

/* POI pressed or released. Do the things.
 */
 
static void foot_press_poi(const struct poi *poi,struct sprite *sprite) {
  switch (poi->opcode) {
    case CMD_map_treadle: {
        int fld=(poi->arg[2]<<8)|poi->arg[3];
        if (!store_get_fld(fld)) {
          bm_sound(RID_sound_treadle);
          store_set_fld(fld,1);
          g.camera.mapsdirty=1;
        }
      } break;
    case CMD_map_stompbox: {
        int fld=(poi->arg[2]<<8)|poi->arg[3];
        if (store_get_fld(fld)) {
          store_set_fld(fld,0);
        } else {
          store_set_fld(fld,1);
        }
        bm_sound(RID_sound_treadle);
        g.camera.mapsdirty=1;
      } break;
    case CMD_map_root: break; // Interesting to the hero, not generically.
    case CMD_map_door: break; // ''
  }
  if (sprite&&sprite->type->tread_poi) sprite->type->tread_poi(sprite,poi->opcode,poi->arg,poi->argc);
}

static void foot_release_poi(const struct poi *poi) {
  switch (poi->opcode) {
    case CMD_map_treadle: {
        int fld=(poi->arg[2]<<8)|poi->arg[3];
        if (store_get_fld(fld)) {
          bm_sound(RID_sound_untreadle);
          store_set_fld(fld,0);
          g.camera.mapsdirty=1;
        }
      } break;
    case CMD_map_stompbox: {
        bm_sound(RID_sound_untreadle);
      } break;
  }
}

static int poi_interesting_opcode(uint8_t opcode) {
  switch (opcode) {
    // All POI commands must have (x,y) in their first two bytes.
    //case CMD_map_switchable: // Probably don't need to track switchable, they're output-only.
    case CMD_map_treadle:
    case CMD_map_stompbox:
    //case CMD_map_root: // Since there aren't release events, this is unwieldly. Have the hero poll for it.
    case CMD_map_door:
      return 1;
  }
  return 0;
}

/* Reset feet.
 */
 
void feet_reset() {
  g.feet.z=0xff;
  g.feet.footc=0;
  g.feet.poic=0;
}

/* Search poi.
 * There may be more than one at a given point, and we might return any of them.
 * Caller should walk down and up if you want to visit all of them.
 */
 
static int feet_poiv_search(int x,int y) {
  int lo=0,hi=g.feet.poic;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct poi *q=g.feet.poiv+ck;
         if (y<q->y) hi=ck;
    else if (y>q->y) lo=ck+1;
    else if (x<q->x) hi=ck;
    else if (x>q->x) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Rebuild poi list.
 */
 
static int feet_rebuild_poiv() {
  g.feet.poic=0;
  struct plane *plane=plane_by_position(g.feet.z);
  if (!plane) return 0;
  struct map *map=plane->v;
  int i=plane->w*plane->h;
  for (;i-->0;map++) {
    struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      if (cmd.argc<2) continue;
      if (poi_interesting_opcode(cmd.opcode)) {
        if (g.feet.poic>=g.feet.poia) {
          int na=g.feet.poia+256;
          if (na>INT_MAX/sizeof(struct poi)) return -1;
          void *nv=realloc(g.feet.poiv,sizeof(struct poi)*na);
          if (!nv) return -1;
          g.feet.poiv=nv;
          g.feet.poia=na;
        }
        int x=map->lng*NS_sys_mapw+cmd.arg[0];
        int y=map->lat*NS_sys_maph+cmd.arg[1];
        int p=feet_poiv_search(x,y);
        if (p<0) p=-p-1;
        struct poi *poi=g.feet.poiv+p;
        memmove(poi+1,poi,sizeof(struct poi)*(g.feet.poic-p));
        poi->x=x;
        poi->y=y;
        poi->opcode=cmd.opcode;
        poi->arg=cmd.arg;
        poi->argc=cmd.argc;
        g.feet.poic++;
      }
    }
  }
  return 0;
}

/* Press a foot.
 */
 
static void foot_press(int x,int y,struct sprite *sprite) {
  int poip=feet_poiv_search(x,y);
  if (poip<0) return;
  while (poip>0) {
    const struct poi *next=g.feet.poiv+poip-1;
    if (next->x!=x) break;
    if (next->y!=y) break;
    poip--;
  }
  const struct poi *poi=g.feet.poiv+poip;
  for (;poip<g.feet.poic;poi++) {
    if (poi->x!=x) break;
    if (poi->y!=y) break;
    foot_press_poi(poi,sprite);
  }
}

/* Release a foot.
 * We don't tell the sprite, because to do that, we'd have to retain the sprites.
 * A bit pricey, and I don't think there will be a need.
 */
 
static void foot_release(int x,int y) {
  int poip=feet_poiv_search(x,y);
  if (poip<0) return;
  while (poip>0) {
    const struct poi *next=g.feet.poiv+poip-1;
    if (next->x!=x) break;
    if (next->y!=y) break;
    poip--;
  }
  const struct poi *poi=g.feet.poiv+poip;
  for (;poip<g.feet.poic;poi++) {
    if (poi->x!=x) break;
    if (poi->y!=y) break;
    foot_release_poi(poi);
  }
}

/* Search feet.
 */
 
static int footv_search(const struct foot *v,int c,int x,int y) {
  int lo=0,hi=c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct foot *q=v+ck;
         if (y<q->y) hi=ck;
    else if (y>q->y) lo=ck+1;
    else if (x<q->x) hi=ck;
    else if (x>q->x) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert in tmpv.
 */
 
static int feet_tmpv_insert(int p,int x,int y,struct sprite *sprite) {
  if ((p<0)||(p>g.feet.tmpc)) return -1;
  if (g.feet.tmpc>=g.feet.tmpa) {
    int na=g.feet.tmpa+32;
    if (na>INT_MAX/sizeof(struct foot)) return -1;
    void *nv=realloc(g.feet.tmpv,sizeof(struct foot)*na);
    if (!nv) return -1;
    g.feet.tmpv=nv;
    g.feet.tmpa=na;
  }
  struct foot *foot=g.feet.tmpv+p;
  memmove(foot+1,foot,sizeof(struct foot)*(g.feet.tmpc-p));
  g.feet.tmpc++;
  foot->x=x;
  foot->y=y;
  foot->sprite=sprite;
  return 0;
}

/* Update feet.
 */
 
void feet_update() {

  /* If the camera's plane changed, drop all feet, then change ours.
   */
  if (g.camera.z!=g.feet.z) {
    const struct foot *foot=g.feet.footv+g.feet.footc;
    while (g.feet.footc>0) {
      g.feet.footc--;
      foot--;
      foot_release(foot->x,foot->y);
    }
    g.feet.z=g.camera.z;
    feet_rebuild_poiv();
  }
  
  /* It is devastatingly unlikely for a plane to have no POI, but if so, awesome, we're off the hook.
   */
  if (!g.feet.poic) return;
  
  /* Build up a list of feet in (g.feet.tmpv) from scratch.
   * This feels a bit heavy but at least it's watertight.
   * We can't miss a press or release, no matter what shenanigans the sprites get up to.
   */
  g.feet.tmpc=0;
  struct plane *plane=plane_by_position(g.feet.z);
  if (plane) {
    int planew=plane->w*NS_sys_mapw;
    int planeh=plane->h*NS_sys_maph;
    struct sprite **p=GRP(solid)->sprv;
    int i=GRP(solid)->sprc;
    for (;i-->0;p++) {
      struct sprite *sprite=*p;
      if (sprite->defunct||sprite->floating) continue;
      if (sprite->z!=g.feet.z) continue;
      int x=(int)sprite->x;
      if ((x<0)||(x>=planew)) continue;
      int y=(int)sprite->y;
      if ((y<0)||(y>=planeh)) continue;
      int p=footv_search(g.feet.tmpv,g.feet.tmpc,x,y);
      if (p>=0) continue; // Cool, already know about it.
      feet_tmpv_insert(-p-1,x,y,sprite);
    }
  }
  
  /* Diff the two foot lists.
   * Trigger press and release as warranted.
   */
  const struct foot *a=g.feet.footv;
  const struct foot *b=g.feet.tmpv;
  int ai=0,bi=0;
  for (;;) {
    if (ai>=g.feet.footc) {
      if (bi>=g.feet.tmpc) break;
      foot_press(b->x,b->y,b->sprite);
      b++;
      bi++;
    } else if (bi>=g.feet.tmpc) {
      foot_release(a->x,a->y);
      a++;
      ai++;
    } else if ((a->x==b->x)&&(a->y==b->y)) {
      a++;
      ai++;
      b++;
      bi++;
    } else if ((a->y<b->y)||((a->y==b->y)&&(a->x<b->x))) {
      foot_release(a->x,a->y);
      a++;
      ai++;
    } else {
      foot_press(b->x,b->y,b->sprite);
      b++;
      bi++;
    }
  }
  
  /* Swap the lists.
   */
  void *v=g.feet.footv;
  int c=g.feet.footc;
  int aa=g.feet.foota;
  g.feet.footv=g.feet.tmpv;
  g.feet.footc=g.feet.tmpc;
  g.feet.foota=g.feet.tmpa;
  g.feet.tmpv=v;
  g.feet.tmpc=c;
  g.feet.tmpa=aa;
}

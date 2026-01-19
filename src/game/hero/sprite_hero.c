#include "hero_internal.h"

/* Cleanup.
 */
 
static void _hero_del(struct sprite *sprite) {
  camera_unlisten(SPRITE->door_listener);
}

/* Init.
 */
 
static int _hero_init(struct sprite *sprite) {
  SPRITE->facedy=1;
  SPRITE->item_blackout=1;
  return 0;
}

/* Update.
 */
 
static void _hero_update(struct sprite *sprite,double elapsed) {

  if (SPRITE->door_listener) {
    if ((SPRITE->door_clock-=elapsed)<=0.0) {
      fprintf(stderr,"%s:%d:PANIC: Assume door transition failed.\n",__FILE__,__LINE__);
      camera_unlisten(SPRITE->door_listener);
      SPRITE->door_listener=0;
    } else {
      return;
    }
  }

  hero_item_update(sprite,elapsed);
  hero_motion_update(sprite,elapsed);
}

/* Injure. Public.
 */
 
void hero_injure(struct sprite *sprite,struct sprite *assailant) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return;
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);
}

/* Collide.
 */
 
static void _hero_collide(struct sprite *sprite,struct sprite *other) {
  if (sprite_group_has(GRP(hazard),other)) {
    hero_injure(sprite,other);
  }
}

/* Callback from camera after a door transition.
 */
 
static void hero_cb_map(struct map *map,int focus,void *userdata) {
  if (focus!=2) return; // Wait for "gained primary focus".
  struct sprite *sprite=userdata;
  camera_unlisten(SPRITE->door_listener);
  SPRITE->door_listener=0;
  sprite->x=SPRITE->doorx;
  sprite->y=SPRITE->doory;
  sprite->z=map->z;
  SPRITE->ignoreqx=(int)sprite->x-map->lng*NS_sys_mapw;
  SPRITE->ignoreqy=(int)sprite->y-map->lat*NS_sys_maph;
}

/* POI.
 * Treadles and such work generically. Others can actuate them, not just Dot, and that's 1000% by design.
 * Doors are only for Dot, and there will probably be others like it in the future.
 */
 
static void _hero_tread_poi(struct sprite *sprite,uint8_t opcode,const uint8_t *arg,int argc) {
  switch (opcode) {
  
    case CMD_map_door: {
        if ((arg[0]==SPRITE->ignoreqx)&&(arg[1]==SPRITE->ignoreqy)) {
          SPRITE->ignoreqx=SPRITE->ignoreqy=-1;
          return;
        }
        SPRITE->ignoreqx=SPRITE->ignoreqy=-1;
        if (!SPRITE->door_listener) {
          SPRITE->door_listener=camera_listen_map(hero_cb_map,sprite);
          SPRITE->door_clock=3.000; // Panic if this expires. Must be longer than camera's door transition.
        }
        int rid=(arg[2]<<8)|arg[3];
        int dstx=arg[4];
        int dsty=arg[5];
        // (6,7) exist but we're not using yet.
        struct map *map=map_by_id(rid);
        if (!map) { // grrr
          fprintf(stderr,"%s:%d: map:%d not found\n",__FILE__,__LINE__,rid);
          camera_unlisten(SPRITE->door_listener);
          SPRITE->door_listener=0;
          return;
        }
        SPRITE->doorx=map->lng*NS_sys_mapw+dstx+0.5;
        SPRITE->doory=map->lat*NS_sys_maph+dsty+0.5;
        camera_cut(rid,dstx,dsty);
      } break;
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_hero={
  .name="hero",
  .objlen=sizeof(struct sprite_hero),
  .del=_hero_del,
  .init=_hero_init,
  .update=_hero_update,
  .render=hero_render,
  .collide=_hero_collide,
  .tread_poi=_hero_tread_poi,
};

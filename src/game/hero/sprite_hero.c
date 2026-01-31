#include "hero_internal.h"

/* Cleanup.
 */
 
static void _hero_del(struct sprite *sprite) {
  camera_unlisten(SPRITE->door_listener);
  if (SPRITE->pumpkin) {
    sprite_group_clear(SPRITE->pumpkin);
    sprite_group_del(SPRITE->pumpkin);
  }
}

/* Init.
 */
 
static int _hero_init(struct sprite *sprite) {
  SPRITE->facedy=1;
  SPRITE->broomdx=1;
  SPRITE->item_blackout=1;
  SPRITE->qx=-1;
  SPRITE->qy=-1;
  SPRITE->compassx=SPRITE->compassy=-1.0;
  SPRITE->compassz=-1;
  return 0;
}

/* Deal damage to the hero.
 * Return nonzero to acknowledge or zero to ignore.
 */
 
static int hero_hurt(struct sprite *sprite,struct sprite *assailant) {
  int hp=store_get_fld16(NS_fld16_hp);
  if (--hp<0) hp=0;
  store_set_fld16(NS_fld16_hp,hp);
  if (!hp) {
    fprintf(stderr,"TODO End game.\n");//TODO Soulballs, sound effect, a little cooldown time.
    g.gameover=1;
    return 1;
  }
  bm_sound(RID_sound_ouch);
  SPRITE->hurt=HERO_HURT_TIME;
  if (assailant) {
    double dx=sprite->x-assailant->x;
    double dy=sprite->y-assailant->y;
    double d2=dx*dx+dy*dy;
    if (d2<0.001) { // Impossibly close.
      SPRITE->hurtdx=0.0;
      SPRITE->hurtdy=0.0;
    } else {
      double distance=sqrt(d2);
      SPRITE->hurtdx=dx/distance;
      SPRITE->hurtdy=dy/distance;
    }
  } else {
    SPRITE->hurtdx=0.0;
    SPRITE->hurtdy=0.0;
  }
  return 1;
}

/* Update hazards, damage counter, etc.
 */
 
static void hero_hazards_update(struct sprite *sprite,double elapsed) {

  // Currently hurt? Rocket away from the collision site.
  if (SPRITE->hurt>0.0) {
    SPRITE->hurt-=elapsed;
    const double speedmin=8.0;
    const double speedmax=24.0;
    double norm=SPRITE->hurt/HERO_HURT_TIME;
    double speed=speedmin*(1.0-norm)+speedmax*norm;
    sprite_move(sprite,SPRITE->hurtdx*speed*elapsed,SPRITE->hurtdy*speed*elapsed);
    
  // If you're in the middle of quaffing a potion, wait until it's done.
  // This might dodge the collision entirely. Once we get missiles happening, review.
  } else if (SPRITE->itemid_in_progress==NS_itemid_potion) {
  
  // Check for new injuries.
  } else {
    double l=sprite->x+sprite->hbl;
    double r=sprite->x+sprite->hbr;
    double t=sprite->y+sprite->hbt;
    double b=sprite->y+sprite->hbb;
    struct sprite **otherp=GRP(hazard)->sprv;
    int i=GRP(hazard)->sprc;
    for (;i-->0;otherp++) {
      struct sprite *other=*otherp;
      if (other->defunct) continue;
      if (other->x+other->hbl>=r) continue;
      if (other->x+other->hbr<=l) continue;
      if (other->y+other->hbt>=b) continue;
      if (other->y+other->hbb<=t) continue;
      if (hero_hurt(sprite,other)) return;
    }
  }
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
  
  // Suspend normal updating during the dark half of a transition.
  switch (camera_describe_transition()) {
    //case 1: return; // Partially visible. Debatable whether to suspend.
    case 2: return; // Invisible. Suspend.
  }
  
  if (SPRITE->divining_alert_clock>0.0) {
    SPRITE->divining_alert_clock-=elapsed;
  }
  if (SPRITE->matchclock>0.0) {
    if ((SPRITE->matchclock-=elapsed)<=0.0) {
      sprite_group_remove(GRP(light),sprite);
    }
  }
  
  // End of vanishment can defer and run arbitrarily long. (why we're doing it here repeatedly, instead of momentarily at game_update)
  if (!(sprite->physics&(1<<NS_physics_vanishable))&&(g.vanishing<=0.0)) {
    hero_unvanish(sprite);
  }

  hero_item_update(sprite,elapsed);
  hero_motion_update(sprite,elapsed);
  hero_hazards_update(sprite,elapsed);
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
  
  if (SPRITE->respawn_princess) {
    struct sprite *princess=sprite_spawn(sprite->x,sprite->y+0.125,RID_sprite_princess,0,0,0,0,0);
    if (princess) {
      sprite_group_remove(GRP(solid),princess);
    }
  }
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
        
        // If there's a Princess and she's close to us, arrange to respawn her on the other side.
        SPRITE->respawn_princess=0;
        struct sprite **otherp=GRP(monsterlike)->sprv;
        int i=GRP(monsterlike)->sprc;
        for (;i-->0;otherp++) {
          struct sprite *other=*otherp;
          if (other->defunct) continue;
          if (other->type!=&sprite_type_princess) continue;
          double dx=other->x-sprite->x;
          double dy=other->y-sprite->y;
          double d2=dx*dx+dy*dy;
          double tolerance=6.0; // Wide. Ensure there aren't any doors near the jail.
          tolerance*=tolerance;
          if (d2<tolerance) {
            SPRITE->respawn_princess=1;
          }
          break;
        }
        
        int transition=NS_transition_spotlight;
        camera_cut(rid,dstx,dsty,transition);
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

/* Drop animation. Presumably we're about to enter a freeze-frame.
 */

void sprite_hero_unanimate(struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return;
  SPRITE->walkanimframe=0;
}

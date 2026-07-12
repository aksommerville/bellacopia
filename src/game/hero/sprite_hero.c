#include "hero_internal.h"

/* Cleanup.
 */
 
static void _hero_del(struct sprite *sprite) {
  camera_unlisten(SPRITE->door_listener);
  store_unlisten(SPRITE->store_listener);
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
  SPRITE->store_listener=store_listen('f',hero_cb_store,sprite);
  SPRITE->shovel_fault_time=-999.999;
  return 0;
}

/* Deal damage to the hero.
 * Return nonzero to acknowledge or zero to ignore.
 */
 
void hero_injure_at(struct sprite *sprite,struct sprite *assailant,double x,double y) {
  if (SPRITE->hurt<=0.0) {
    int hp=store_get_fld16(NS_fld16_hp);
    if (--hp<0) hp=0;
    store_set_fld16(NS_fld16_hp,hp);
    bm_sound(RID_sound_ouch);
    if (!hp) {
      game_begin_gameover();
      return;
    }
    SPRITE->hurt=HERO_HURT_TIME;
  }
  double dx=sprite->x-x;
  double dy=sprite->y-y;
  double d2=dx*dx+dy*dy;
  if (d2<0.001) { // Impossibly close.
    SPRITE->hurtdx=0.0;
    SPRITE->hurtdy=0.0;
  } else {
    double distance=sqrt(d2);
    SPRITE->hurtdx=dx/distance;
    SPRITE->hurtdy=dy/distance;
  }
}
 
static int hero_hurt(struct sprite *sprite,struct sprite *assailant) {
  if (assailant) {
    hero_injure_at(sprite,assailant,assailant->x,assailant->y);
  } else {
    hero_injure_at(sprite,0,sprite->x,sprite->y);
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
  SPRITE->busstop_clock=0.0;
  
  if (SPRITE->respawn_princess) {
    struct sprite *princess=sprite_spawn(sprite->x,sprite->y+0.125,RID_sprite_princess,0,0,0,0,0);
    if (princess) {
      sprite_group_remove(GRP(solid),princess);
    }
  }
}

/* Check triggers, when (qx,qy) changed.
 * There's generic 'feet' for treadles and stompboxes, but we need something else for narrative triggers, since Dot might be flying.
 */
 
static void hero_check_triggers(struct sprite *sprite) {
  struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return;
  int x=SPRITE->qx%NS_sys_mapw;
  int y=SPRITE->qy%NS_sys_maph;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_triggeronce: {
          if (cmd.arg[0]!=x) break;
          if (cmd.arg[1]!=y) break;
          int fld=(cmd.arg[2]<<8)|cmd.arg[3];
          if (store_get_fld(fld)) break;
          int activity=(cmd.arg[4]<<8)|cmd.arg[5];
          int arg=(cmd.arg[6]<<8)|cmd.arg[7];
          store_set_fld(fld,1);
          game_begin_activity(activity,arg,sprite);
        } break;
    }
  }
  
  // Also look at the physics underfoot.
  uint8_t physics=map->physics[map->v[y*NS_sys_mapw+x]];
  if (physics==NS_physics_ice) {
    SPRITE->onice=1;
    if (
      (SPRITE->itemid_in_progress!=NS_itemid_broom)&&
      SPRITE->walking
    ) {
      // Use (faced) instead of (ind) -- We don't want diagonals.
      SPRITE->sliding=1;
      SPRITE->slidedx=SPRITE->facedx;
      SPRITE->slidedy=SPRITE->facedy;
      SPRITE->walking=0;
    }
  } else if (SPRITE->sliding) {
    SPRITE->onice=0;
    SPRITE->sliding=0;
  }
  
  // And freshen (hints_override) while we're here.
  SPRITE->hints_override=map->hints_override;
}

/* Check if we're trapped inside a solid.
 * It can happen with treadle-activated doors, if your bomb explodes early or a monster steps on the switch.
 * Was thinking it would only require the quantized center, but actually you can get trapped by the toes too. D'oh.
 */
 
static void hero_mitigate_map_traps(struct sprite *sprite,double elapsed) {

  /* Since this is an emergency mitigation, it doesn't need to be pretty or responsive.
   * Also, its cost is cheap but not super cheap.
   * Check only at 2 hz, not every frame.
   */
  if (SPRITE->maptrap_clock>0.0) {
    SPRITE->maptrap_clock-=elapsed;
    return;
  }
  SPRITE->maptrap_clock+=0.500;
  
  // No collision here? Great, we're done.
  if (sprite_test_position(sprite)) return;
  
  /* Move her unceremoniously to the first cardinal or diagonal neighbor where no collision reports.
   * Quantized first, then cardinals, then diagonals.
   */
  double pvx=sprite->x,pvy=sprite->y;
  int x0=(int)sprite->x;
  int y0=(int)sprite->y;
  struct neighbor { int dx,dy; } neighborv[]={
    {0,0},
    {0,-1},{0,1},{-1,0},{1,0},
    {-1,-1},{1,-1},{-1,1},{1,1},
  };
  const struct neighbor *n=neighborv;
  int i=sizeof(neighborv)/sizeof(neighborv[0]);
  for (;i-->0;n++) {
    int ckx=x0+n->dx;
    int cky=y0+n->dy;
    sprite->x=ckx+0.5;
    sprite->y=cky+0.5;
    if (sprite_test_position(sprite)) {
      return;
    }
  }
  sprite->x=pvx;
  sprite->y=pvy;
}

/* Update.
 */
 
static void _hero_update(struct sprite *sprite,double elapsed) {
  SPRITE->alwaysclock+=elapsed;
  
  hero_mitigate_map_traps(sprite,elapsed);

  if (SPRITE->busstop_clock>0.0) {
    if ((SPRITE->busstop_clock-=elapsed)<=0.0) {
      if (g.camera.map&&(g.camera.map->rid==SPRITE->busstop_mapid)) {
        sprite->x=g.camera.map->lng*NS_sys_mapw+SPRITE->busstop_col+0.5;
        sprite->y=g.camera.map->lat*NS_sys_maph+SPRITE->busstop_row+0.5;
        SPRITE->ignoreqx=(int)sprite->x-g.camera.map->lng*NS_sys_mapw;
        SPRITE->ignoreqy=(int)sprite->y-g.camera.map->lat*NS_sys_maph;
      } else {
        if (!SPRITE->door_listener) {
          SPRITE->door_listener=camera_listen_map(hero_cb_map,sprite);
          SPRITE->door_clock=3.000; // Panic if this expires. Must be longer than camera's door transition.
        }
        sprite->z=-1; // Poison our plane, so camera doesn't try to outsmart the transition.
        g.song_override_outerworld=0;
        SPRITE->busstop_clock=10.0;
        camera_cut(SPRITE->busstop_mapid,SPRITE->busstop_col,SPRITE->busstop_row,NS_transition_fadeblack);
      }
    }
    return;
  }

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
  
  // Check quantized position.
  SPRITE->pv_hints_override=SPRITE->hints_override;
  SPRITE->qnew=0;
  int qx=(int)sprite->x;
  int qy=(int)sprite->y;
  if ((qx!=SPRITE->qx)||(qy!=SPRITE->qy)) {
    SPRITE->qnew=1;
    SPRITE->qx=qx;
    SPRITE->qy=qy;
    hero_check_triggers(sprite);
  }

  hero_item_update(sprite,elapsed);
  hero_motion_update(sprite,elapsed);
  hero_hazards_update(sprite,elapsed);
}

/* Injure. Public.
 */
 
void hero_injure(struct sprite *sprite,struct sprite *assailant) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return;
  hero_hurt(sprite,assailant);
}

/* Collide.
 */
 
static void _hero_collide(struct sprite *sprite,struct sprite *other) {
  if (sprite_group_has(GRP(hazard),other)) {
    hero_injure(sprite,other);
  }
}

/* POI.
 * Treadles and such work generically. Others can actuate them, not just Dot, and that's 1000% by design.
 * Doors are only for Dot, and there will probably be others like it in the future.
 * Nonstatic because we trigger this artificially for door-reverse mitigation.
 */
 
void _hero_tread_poi(struct sprite *sprite,uint8_t opcode,const uint8_t *arg,int argc) {
  switch (opcode) {
  
    case CMD_map_door:
    case CMD_map_burieddoor: {
        if ((arg[0]==SPRITE->ignoreqx)&&(arg[1]==SPRITE->ignoreqy)) {
          SPRITE->ignoreqx=SPRITE->ignoreqy=-1;
          return;
        }
        
        int fld=0,activity=0;
        if (opcode==CMD_map_burieddoor) fld=(arg[6]<<8)|arg[7];
        else activity=(arg[6]<<8)|arg[7];
        if (fld&&!store_get_fld(fld)) return;
        
        SPRITE->ignoreqx=SPRITE->ignoreqy=-1;
        if (!SPRITE->door_listener) {
          SPRITE->door_listener=camera_listen_map(hero_cb_map,sprite);
          SPRITE->door_clock=3.000; // Panic if this expires. Must be longer than camera's door transition.
        }
        int rid=(arg[2]<<8)|arg[3];
        int dstx=arg[4];
        int dsty=arg[5];
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
        
        // If there's an activity, trigger it now, with me as initiator.
        if (activity) game_begin_activity(activity,0,sprite);
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

double sprite_hero_get_match_time(struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return 0.0;
  return SPRITE->matchclock;
}

/* Warp to a bus stop.
 */
 
static struct map *find_busstop(struct cmdlist_entry *cmd,int busstop) {
  struct plane *plane=g.mapstore.planev;
  int planei=g.mapstore.planec;
  for (;planei-->0;plane++) {
    struct map *map=plane->v;
    int mapi=plane->w*plane->h;
    for (;mapi-->0;map++) {
      struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
      struct cmdlist_entry entry;
      while (cmdlist_reader_next(&entry,&reader)>0) {
        if (entry.opcode==CMD_map_busstop) {
          int q=(entry.arg[2]<<8)|entry.arg[3];
          if (q==busstop) {
            *cmd=entry;
            return map;
          }
        }
      }
    }
  }
  return 0;
}

void sprite_hero_warp_busstop(struct sprite *sprite,int busstop) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return;
  struct cmdlist_entry cmd={0};
  struct map *map=find_busstop(&cmd,busstop);
  if (!map) return;
  
  // If we were kidnapped and not yet escaped, this counts as escaping. (all bus stops are outside the goblins' cave)
  if (!store_get_fld(NS_fld_escaped)&&store_get_fld(NS_fld_kidnapped)) {
    store_set_fld(NS_fld_escaped,1);
  }
  
  SPRITE->busstop_col=cmd.arg[0];
  SPRITE->busstop_row=cmd.arg[1]+1;
  SPRITE->doorx=map->lng*NS_sys_mapw+SPRITE->busstop_col+0.5;
  SPRITE->doory=map->lat*NS_sys_maph+SPRITE->busstop_row+0.5;
  SPRITE->busstop_mapid=map->rid;
  SPRITE->busstop_clock=1.0;
}

/* Trivial accessors.
 */
 
int sprite_hero_is_injured(struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return 0;
  return (SPRITE->hurt>0.0)?1:0;
}

int sprite_hero_is_escorting_princess(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return 0;
  return SPRITE->respawn_princess;
}

uint8_t sprite_hero_get_facedir(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return 0;
  if (SPRITE->facedx<0) return 0x10;
  if (SPRITE->facedy>0) return 0x08;
  if (SPRITE->facedy<0) return 0x40;
  return 0x02;
}

int sprite_hero_get_item_in_play(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return 0;
  return SPRITE->itemid_in_progress;
}

/* Do my feet touch the ground?
 */

int sprite_hero_is_grounded(const struct sprite *sprite) {
  if (!sprite) return 0;
  if (sprite->type==&sprite_type_hero) {
    if (SPRITE->itemid_in_progress==NS_itemid_broom) return 0;
    return 1;
  }
  if (sprite->type==&sprite_type_racer) return 0;
  return 1;
}

/* Is a door transition pending?
 */

int sprite_hero_is_using_door(double *dstx,double *dsty,const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_hero)) return 0;
  if (!SPRITE->door_listener) return 0;
  *dstx=SPRITE->doorx;
  *dsty=SPRITE->doory;
  return 1;
}

/* sprite_princess.c
 * We're the imprisoned Princess, basically an NPC, and also the live one that follows you around.
 * The Princess at home is a regular sprite_type_npc. (and in hindsight, the one in prison should have been too).
 * We are in play both for the escape-from-the-goblins quest and the optional side quests.
 */

#include "game/bellacopia.h"

#define WHACK_TIME       0.500
#define WHACK_SPEED_MAX 10.000
#define WHACK_SPEED_MIN  1.000
#define KIDNAPPER_POLL_TIME 5.000

struct sprite_princess {
  struct sprite hdr;
  double cooldown;
  double animclock;
  int animframe;
  uint8_t tileid0;
  int recheck_solid;
  double armt;
  int targetx,targety,targetz;
  int fldid; // Zero for the rescue sequence, otherwise walk1..walk5.
  int finished; // If (fldid), nonzero after we reach the goal.
  int target_near; // Nonzero if we're pointing to the final destination.
  double kidnap_ttl; // Counts down to the next possible kidnapper spawn.
  double kidnap_hero_x,kidnap_hero_y; // Track hero's position; only spawn kidnappers when she moves.
  
  // Forced motion, tapers down over time. For getting whacked by the stick.
  double whackdx,whackdy;
  double whackttl;
};

#define SPRITE ((struct sprite_princess*)sprite)

/* Cleanup.
 */
 
static void _princess_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int _princess_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->targetz=-1;
  SPRITE->fldid=(sprite->arg[1]<<8)|sprite->arg[2];
  SPRITE->finished=sprite->arg[3];
  SPRITE->kidnap_ttl=KIDNAPPER_POLL_TIME;
  
  /* If I'm in the well, don't spawn. Shouldn't be possible.
   */
  if (store_get_fld(NS_fld_princess_in_well)) return -1;

  /* Already rescued? I will never exist anymore.
   * Unless I'm seq-zero, for the post-rescue side quests.
   */
  int seq=(sprite->argc>=4)?sprite->arg[0]:0;
  if (seq&&store_get_fld(NS_fld_rescued_princess)) return -1;
  
  /* Check my sequence tag. Is this the right spawn point for the narrative context?
   */
  switch (seq) {
    case 0: break; // Always spawn.
    case 1: { // In the jail cell.
        if (store_get_fld(NS_fld_princess_outside)) return -1;
      } break;
    case 2: { // At the cave's entrance.
        if (!store_get_fld(NS_fld_princess_outside)) return -1;
      } break;
    default: {
        fprintf(stderr,"Illegal value %d for princess seq.\n",seq);
        return -1;
      }
  }
  
  /* If there's already another Princess, nix this new one. Lord knows, one is plenty.
   * This can happen when we're following Dot but Dot goes back to the dungeon for some reason.
   * We are in monsterlike group, and that should be a pretty small one.
   * Also, now that there's a front-door spawn point, it comes up a lot.
   */
  struct sprite **otherp=GRP(monsterlike)->sprv;
  int i=GRP(monsterlike)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other==sprite) continue;
    if (other->type==&sprite_type_princess) return -1;
  }
  
  return 0;
}

/* Walk.
 * (dx,dy) account for elapsed time but not for walking speed.
 */
 
static void princess_walk(struct sprite *sprite,double dx,double dy) {
  if (dx<0.0) sprite->xform=0;
  else if (dx>0.0) sprite->xform=EGG_XFORM_XREV;
  const double speed=5.0; // less than dot's, which is 6
  sprite_move(sprite,dx*speed,dy*speed);
}

static int princess_walk_naive(struct sprite *sprite,struct sprite *hero,double elapsed) {
  // Try to walk in the straightest line to Dot.
  // This is actually not bad. I thought we were going to need some complex path-finding... maybe we can do without.
  const double CLOSE_ENOUGH_2=1.250;
  const double TOO_CLOSE_2=1.000; // If we're this close, Dot must be pushing me. Walk the other way.
  const double SPEED=4.000; // Less than Dot (6), and also we don't root-two the diagonals like she does.
  double dx=hero->x-sprite->x;
  double dy=hero->y-sprite->y;
  double d2=dx*dx+dy*dy;
  if (d2<=TOO_CLOSE_2) { // Dot must be pushing us. Walk the other way.
    // Walk cardinally, whichever axis is more significant.
    double adx=dx; if (adx<0.0) adx=-adx;
    double ady=dy; if (ady<0.0) ady=-ady;
    if (adx>ady) {
      dx*=-2.0;
      dy=0.0;
    } else {
      dx=0.0;
      dy*=-2.0;
    }
    d2=dx*dx+dy*dy;
  }
  if (dx<0.0) sprite->xform=0; // We only do left and right faces.
  else if (dx>0.0) sprite->xform=EGG_XFORM_XREV;
  if (d2<CLOSE_ENOUGH_2) return 0;
  double distance=sqrt(d2);
  double ndx=dx/distance;
  double ndy=dy/distance;
  double x0=sprite->x;
  double y0=sprite->y;
  int result=sprite_move(sprite,ndx*elapsed*SPEED,ndy*elapsed*SPEED);
  
  // Check actual motion, to see if we're caught on a corner or something. Happens all the time.
  double postdx=sprite->x-x0;
  double postdy=sprite->y-y0;
  double actual=sqrt(postdx*postdx+postdy*postdy)/elapsed;
  if ((actual>0.0)&&(actual<SPEED*0.250)) {
    if (sprite_move(sprite,((dx<0.0)?-SPEED:SPEED)*elapsed,0.0)) result=1;
    if (sprite_move(sprite,0.0,((dy<0.0)?-SPEED:SPEED)*elapsed)) result=1;
  }
  
  return result;
}

/* When walking the same direction as a monster -- likely -- it often doesn't detect a collision.
 * You end up being tailgated by a monster until the first time you stop or turn.
 * To mitigate, we'll scan for monsters any time we're walking.
 * Anyone super close, call their collision callback.
 */
 
static void princess_check_missed_triggers(struct sprite *sprite) {
  struct sprite **otherp=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other==sprite) continue;
    if (other->type==&sprite_type_hero) continue;
    if (!other->type->collide) continue;
    double dx=other->x-sprite->x;
    double dy=other->y-sprite->y;
    const double radius=1.0; // We could check against hitboxes, that would surely be more correct.
    if ((dx>=-radius)&&(dy>=-radius)&&(dx<=radius)&&(dy<=radius)) {
      other->type->collide(other,sprite);
      return;
    }
  }
}

/* If this cell looks kosher, spawn a kidnapper here and return it.
 */
 
static struct sprite *princess_spawn_kidnapper(struct sprite *sprite,const struct plane *plane,int x,int y) {
  if ((x<0)||(y<0)||(x>=plane->w*NS_sys_mapw)||(y>=plane->h*NS_sys_maph)) return 0;
  const struct map *map=plane->v+(y/NS_sys_maph)*plane->w+(x/NS_sys_mapw);
  int subx=x-map->lng*NS_sys_mapw;
  int suby=y-map->lat*NS_sys_maph;
  if ((subx<0)||(suby<0)||(subx>=NS_sys_mapw)||(suby>=NS_sys_maph)) return 0; // just because i don't trust myself
  switch (map->physics[map->v[suby*NS_sys_mapw+subx]]) {
    case NS_physics_vacant:
      break;
    default: return 0;
  }
  double xlo=x-0.5,xhi=x+1.5;
  double ylo=y-0.5,yhi=y+1.5;
  struct sprite **otherp=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->x<xlo) continue;
    if (other->y>xhi) continue;
    if (other->y<ylo) continue;
    if (other->y>yhi) continue;
    return 0;
  }
  int candidatev[]={ // Monster sprites I haven't placed yet. TODO Decide who actually belongs here. Maybe a specific "kidnapper" monster? Or adjust per map?
    RID_sprite_bull,
    RID_sprite_mouse,
    RID_sprite_owl,
    RID_sprite_geographer,
    RID_sprite_elf,
    RID_sprite_fishycist,
  };
  int candidatec=sizeof(candidatev)/sizeof(int);
  int rid=candidatev[rand()%candidatec];
  return sprite_spawn(x+0.5,y+0.5,rid,0,0,0,0,0);
}

/* Kidnap support.
 */
 
struct princess_kidnap_candidate {
  int x,y; // Plane meters.
  int weight;
};

static int princess_kidnap_candidate_cmp(const void *a,const void *b) {
  const struct princess_kidnap_candidate *A=a,*B=b;
  return B->weight-A->weight;
}

static int princess_valid_kidnapper_cell(const struct plane *plane,int x,int y) {
  if ((x<0)||(y<0)) return 0;
  if (x>=plane->w*NS_sys_mapw) return 0;
  if (y>=plane->h*NS_sys_maph) return 0;
  const struct map *map=plane->v+(y/NS_sys_maph)*plane->w+(x/NS_sys_mapw);
  x-=map->lng*NS_sys_mapw;
  y-=map->lat*NS_sys_maph;
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return 0;
  uint8_t ph=map->physics[map->v[y*NS_sys_mapw+x]];
  if (ph==NS_physics_vacant) return 1;
  return 0;
}

/* Poll for possible creation of a kidnapper.
 * Only relevant to the post-rescue side quests, and only on the way out.
 */
 
static void princess_update_kidnappers(struct sprite *sprite,double elapsed) {

  // Long delay between spawn opportunities.
  if ((SPRITE->kidnap_ttl-=elapsed)>0.0) return;
  SPRITE->kidnap_ttl+=KIDNAPPER_POLL_TIME;
  
  /* Poll hero's position.
   * If she hasn't moved far, skip this cycle.
   * Should become safe when you stop moving.
   */
  if (GRP(hero)->sprc>0) {
    struct sprite *hero=GRP(hero)->sprv[0];
    double dx=hero->x-SPRITE->kidnap_hero_x;
    double dy=hero->y-SPRITE->kidnap_hero_y;
    double d2=dx*dx+dy*dy;
    if (d2<2.0) {
      return;
    }
    SPRITE->kidnap_hero_x=hero->x;
    SPRITE->kidnap_hero_y=hero->y;
  }
  
  /* Only spawn when we're in the outerworld.
   * We specifically do not want to spawn kidnappers in singletons or the Temple. Other places, meh?
   * You are of course free to take the Princess downstairs, but there's never a need.
   */
  const struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map||(map->z!=NS_plane_outerworld)) return;
  const struct plane *plane=plane_by_position(map->z);
  if (!plane) return;
  
  /* If there's too many monsters already, don't make a new one.
   */
  const int enough_monsters=5;
  int monsterc=0;
  struct sprite **otherp=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->type!=&sprite_type_monster) continue;
    monsterc++;
    if (monsterc>=enough_monsters) {
      return;
    }
  }
  
  /* Candidate cells are along the nearest fully-offscreen border.
   * Only record safe in-bounds cells.
   * Weight based on distance to the center, prefer to spawn in the middle of the edge.
   * Weight is slightly randomized, so when there's two cells across from each other -- typical -- their order is random.
   * Don't bother checking solid sprites yet; we'll get to that.
   */
  #define CANDIDATE_LIMIT 70
  struct princess_kidnap_candidate candidatev[CANDIDATE_LIMIT];
  int candidatec=0;
  int xlo=g.camera.rx/NS_sys_tilesize-1;
  int xhi=(g.camera.rx+FBW)/NS_sys_tilesize+1;
  int ylo=g.camera.ry/NS_sys_tilesize-1;
  int yhi=(g.camera.ry+FBH)/NS_sys_tilesize+1;
  int xmid=(xlo+xhi)>>1,ymid=(ylo+yhi)>>1;
  #define WEIGH(n) ({ \
    int _weight=(n); \
    if (_weight<0) _weight=-_weight; \
    _weight=20-_weight; \
    _weight<<=2; \
    _weight+=rand()&3; \
    (_weight); \
  })
  int x=xlo; for (;x<=xhi;x++) {
    if (candidatec>CANDIDATE_LIMIT-2) break;
    if (princess_valid_kidnapper_cell(plane,x,ylo)) candidatev[candidatec++]=(struct princess_kidnap_candidate){x,ylo,WEIGH(x-xmid)};
    if (princess_valid_kidnapper_cell(plane,x,yhi)) candidatev[candidatec++]=(struct princess_kidnap_candidate){x,yhi,WEIGH(x-xmid)};
  }
  int y=ylo; for (;y<=yhi;y++) {
    if (candidatec>CANDIDATE_LIMIT-2) break;
    if (princess_valid_kidnapper_cell(plane,xlo,y)) candidatev[candidatec++]=(struct princess_kidnap_candidate){xlo,y,WEIGH(y-ymid)};
    if (princess_valid_kidnapper_cell(plane,xhi,y)) candidatev[candidatec++]=(struct princess_kidnap_candidate){xhi,y,WEIGH(y-ymid)};
  }
  #undef WEIGH
  #undef CANDIDATE_LIMIT
  qsort(candidatev,candidatec,sizeof(struct princess_kidnap_candidate),princess_kidnap_candidate_cmp);
  const struct princess_kidnap_candidate *candidate=candidatev;
  for (i=candidatec;i-->0;candidate++) {
    struct sprite *kidnapper=princess_spawn_kidnapper(sprite,plane,candidate->x,candidate->y);
    if (kidnapper) {
      sprite_monster_extra_hungry_for_princess(kidnapper);
      return;
    }
  }
}

/* Update.
 */
 
static void _princess_update(struct sprite *sprite,double elapsed) {
  
  // If we're whacked, tick it down and move.
  if (SPRITE->whackttl>0.0) {
    double t=SPRITE->whackttl/WHACK_TIME;
    if (t>0.0) {
      if (t>1.0) t=1.0;
      double speed=WHACK_SPEED_MIN*(1.0-t)+WHACK_SPEED_MAX*t;
      speed*=elapsed;
      sprite_move(sprite,SPRITE->whackdx*speed,0.0);
      sprite_move(sprite,0.0,SPRITE->whackdy*speed);
    }
    SPRITE->whackttl-=elapsed;
    return;
  }

  // If the jail is still locked, we're just an NPC.
  if (SPRITE->cooldown>0.0) {
    SPRITE->cooldown-=elapsed;
  }
  if (!store_get_fld(NS_fld_jailopen)) {
    SPRITE->targetz=-1;
    return;
  }
  
  // Refresh target if necessary.
  if ((SPRITE->targetz!=sprite->z)&&(SPRITE->targetz!=-2)) {
    SPRITE->targetz=sprite->z;
    int compass=SPRITE->fldid;
    if (!compass||SPRITE->finished) compass=NS_compass_castle;
    int err=game_get_target_position(&SPRITE->targetx,&SPRITE->targety,sprite->x,sprite->y,sprite->z,compass);
    if (err<0) {
      SPRITE->targetz=-2; // Error, poison it.
      SPRITE->target_near=0;
    } else {
      SPRITE->target_near=err;
    }
  }
  
  // Check completion of walks.
  if (SPRITE->fldid) {
    princess_update_kidnappers(sprite,elapsed);
    if (SPRITE->finished) {
      // sprite_npc takes care of this case.
    } else if (SPRITE->target_near) {
      // Target is on our plane. If we're within 2 meters of it, declare success and demand to be taken home.
      double dx=SPRITE->targetx-sprite->x;
      double dy=SPRITE->targety-sprite->y;
      double d2=dx*dx+dy*dy;
      if (d2<4.0) {
        SPRITE->finished=1;
        SPRITE->targetz=-1;
        game_begin_activity(NS_activity_dialogue,186,sprite);
      }
    }
  }
  
  // Update arm if pointing.
  if (SPRITE->targetz>=0) {
    double dx=sprite->x-(SPRITE->targetx+0.5);
    double dy=SPRITE->targety+0.5-sprite->y;
    SPRITE->armt=atan2(dx,dy);
  }
  
  // Jail is open, now things get interesting.
  int walking=0;
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    walking=princess_walk_naive(sprite,hero,elapsed);
  }
  
  // Animate if walking, otherwise return to idle face.
  if (walking) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.200;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    }
    princess_check_missed_triggers(sprite);
  } else {
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
  }
  
  // Update face.
  switch (SPRITE->animframe) {
    case 0: sprite->tileid=SPRITE->tileid0; break;
    case 1: sprite->tileid=SPRITE->tileid0+1; break;
    case 2: sprite->tileid=SPRITE->tileid0; break;
    case 3: sprite->tileid=SPRITE->tileid0+2; break;
  }
  
  /* Hero kicks us out of the solid group when we pass thru a door.
   * Periodically check whether I'm missing it, and try to reenable.
   */
  if (--(SPRITE->recheck_solid)<0) {
    SPRITE->recheck_solid=60;
    if (!sprite_group_has(GRP(solid),sprite)) {
      sprite_group_add(GRP(solid),sprite);
      if (sprite_test_position(sprite)) {
        // cool, we're back to normal
      } else {
        sprite_group_remove(GRP(solid),sprite);
      }
    }
  }
}

/* Collide.
 */
 
static int princess_cb_gift(int optionid,void *userdata) {
  game_get_item(NS_itemid_vanishing,1);
  return 1;
}
 
static void _princess_collide(struct sprite *sprite,struct sprite *other) {
  if (other->type==&sprite_type_hero) {
    if (store_get_fld(NS_fld_jailopen)) return; // We should be following. No more conversation.
    
    /* Pre-rescue, we do three static dialogue messages:
     *  39 Take this vanishing cream.
     *  40 Where are you?
     *  41 Escape and then rescue me please.
     * With 39, we also give one unit of vanishing cream. We have an infinite supply.
     */
    struct modal_args_dialogue args={
      .rid=RID_strings_dialogue,
      .speaker=sprite,
      .userdata=sprite,
    };
    if (g.vanishing>0.0) { // "Where are you?"
      args.strix=40;
    } else if (possessed_quantity_for_itemid(NS_itemid_vanishing,0)>0) { // "Go and send help!"
      args.strix=41;
    } else { // "Here's some vanishing cream."
      args.strix=39;
      args.cb=princess_cb_gift;
    }
    struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
    if (!modal) return;
    SPRITE->cooldown=0.250;
  }
}

/* Render.
 */
 
static void _princess_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
  if (SPRITE->targetz>=0) {
    int armx=x;
    if (sprite->xform&EGG_XFORM_XREV) armx-=3; else armx+=3;
    int army=y+2;
    int8_t armrot=(int8_t)((SPRITE->armt*128.0)/M_PI);
    graf_fancy(&g.graf,armx,army,SPRITE->tileid0+3,0,armrot,NS_sys_tilesize,0,0);
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_princess={
  .name="princess",
  .objlen=sizeof(struct sprite_princess),
  .del=_princess_del,
  .init=_princess_init,
  .update=_princess_update,
  .collide=_princess_collide,
  .render=_princess_render,
};

/* Get whacked.
 */
 
int sprite_princess_whack(struct sprite *sprite,double x,double y) {
  if (!sprite||(sprite->type!=&sprite_type_princess)) return 0;
  if (SPRITE->whackttl>0.0) return 0;
  SPRITE->whackttl=WHACK_TIME;
  SPRITE->whackdx=sprite->x-x;
  SPRITE->whackdy=sprite->y-y;
  double dx2=SPRITE->whackdx*SPRITE->whackdx;
  double dy2=SPRITE->whackdy*SPRITE->whackdy;
  double d2=dx2+dy2;
  if (d2<0.001) { // Should usually be about 1; (x,y) is Dot's position. If it comes in crazy low, abort.
    SPRITE->whackttl=0.0;
    return 0;
  }
  double distance=sqrt(d2);
  SPRITE->whackdx/=distance;
  SPRITE->whackdy/=distance;
  return 1;
}

/* Check completion of walks.
 */
 
int sprite_princess_get_target_if_successful(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_princess)) return 0;
  if (!SPRITE->finished) return 0;
  return SPRITE->fldid;
}

#include "game/bellacopia.h"

struct sprite_princess {
  struct sprite hdr;
  double cooldown;
  double animclock;
  int animframe;
  uint8_t tileid0;
  int recheck_solid;
  double armt;
  int targetx,targety,targetz;
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

  /* Already rescued? I will never exist anymore.
   */
  if (store_get_fld(NS_fld_rescued_princess)) return -1;
  
  /* If there's already another Princess, nix this new one. Lord knows, one is plenty.
   * This can happen when we're following Dot but Dot goes back to the dungeon for some reason.
   * We are in monsterlike group, and that should be a pretty small one.
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

/* Update.
 */
 
static void _princess_update(struct sprite *sprite,double elapsed) {

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
    if (game_get_target_position(&SPRITE->targetx,&SPRITE->targety,sprite->x,sprite->y,sprite->z,NS_compass_castle)<0) {
      SPRITE->targetz=-2; // Error, poison it.
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

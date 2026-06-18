#include "game/bellacopia.h"
#include "game/race/race.h"

struct sprite_racer {
  struct sprite hdr;
  int human,face;
  int srcy;
  
  // The man or CPU controller sets these and nothing else.
  int steer; // -1,0,1 = deasil,neutral,clockwise
  int accel; // 0,1. If we had a brake or reverse, it would be -1, but I think we're not doing that.
  
  double facet;
  int frame; // (0,1,2,3)=(normal,rush,turnleft,turnright)
  int elevation; // (0,1)=(lo,hi), are we above water? Controls the shadow depth.
  double steer_speed; // rad/s. Volatile, final answer.
  double top_speed; // m/s. Holds fairly constant, but adjusts per bells.
  double speed; // m/s. Highly volatile, adjusts each frame per input and steering.
  double accel_rate; // XXX m/s**2, must be positive.
  double decel_rate; // m/s**2, must be negative. Deceleration is constant, so it should be substantially less significant than acceleration.
  double steer_penalty; // Multiplier.
  double dx,dy; // m/s inertia.
};

#define SPRITE ((struct sprite_racer*)sprite)

/* Init.
 */
 
static int _racer_init(struct sprite *sprite) {
  SPRITE->human=sprite->arg[0];
  SPRITE->face=sprite->arg[1];
  if ((SPRITE->human<0)||(SPRITE->human>2)) return -1;
  // We allow Princess and the Green Witch, tho I don't plan to use these in outer-world races.
  // Likewise, it wouldn't take much from here to support multiplayer, if we ever want that.
  switch (SPRITE->face) {
    case NS_face_dot: SPRITE->srcy=0; break;
    case NS_face_moonsong: SPRITE->srcy=64; break;
    case NS_face_princess: SPRITE->srcy=128; break;
    case NS_face_monster: SPRITE->srcy=192; break;
    default: SPRITE->srcy=192;
  }
  sprite->physics=(
    (1<<NS_physics_solid)|
    (1<<NS_physics_grabbable)|
    (1<<NS_physics_vanishable)|
  0);
  sprite->hbl=sprite->hbt=-0.400;
  sprite->hbr=sprite->hbb= 0.400;
  
  if (sprite_group_add(GRP(visible),sprite)<0) return -1;
  if (sprite_group_add(GRP(update),sprite)<0) return -1;
  if (sprite_group_add(GRP(solid),sprite)<0) return -1;
  if (SPRITE->human==1) {
    if (sprite_group_add(GRP(hero),sprite)<0) return -1;
  }
  
  SPRITE->steer_speed=5.000;
  SPRITE->top_speed=18.0;
  SPRITE->accel_rate=200.0;
  SPRITE->decel_rate=-8.0;
  SPRITE->steer_penalty=0.750;
  
  if (!SPRITE->human) {
    //TODO Prep CPU racer.
  }
  
  return 0;
}

/* Turn.
 */
 
static void racer_turn(struct sprite *sprite,double elapsed,double d) {
  SPRITE->facet+=d*elapsed*5.000; // TODO Turn speed control.
  if (SPRITE->facet<-M_PI) SPRITE->facet+=M_PI*2.0;
  if (SPRITE->facet>M_PI) SPRITE->facet-=M_PI*2.0;
}

/* Update, human control.
 */
 
static void racer_update_man(struct sprite *sprite,double elapsed,int input,int pvinput) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: SPRITE->steer=-1; break;
    case EGG_BTN_RIGHT: SPRITE->steer=1; break;
    default: SPRITE->steer=0; break;
  }
  if (input&EGG_BTN_SOUTH) {
    SPRITE->accel=1;
  } else {
    SPRITE->accel=0;
  }
  //XXX Press WEST to end the race.
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    race_end();
  }
}

/* Update, CPU control.
 */
 
static void racer_update_cpu(struct sprite *sprite,double elapsed) {
  //TODO
}

/* Update.
 */
 
static void _racer_update(struct sprite *sprite,double elapsed) {

  /* Operating like most battles, we have a man or cpu controller first, just making the decisions.
   */
  if (SPRITE->human) racer_update_man(sprite,elapsed,g.input[SPRITE->human],g.pvinput[SPRITE->human]);
  else racer_update_cpu(sprite,elapsed);
  
  /* Steering.
   */
  if (SPRITE->steer) {
    SPRITE->facet+=SPRITE->steer*SPRITE->steer_speed*elapsed;
    if (SPRITE->facet<-M_PI) SPRITE->facet+=M_PI*2.0;
    if (SPRITE->facet>M_PI) SPRITE->facet-=M_PI*2.0;
  }
  
  /* Drop inertia.
   */
  int motion=0;
  double d2=SPRITE->dx*SPRITE->dx+SPRITE->dy*SPRITE->dy;
  if (d2>0.001) {
    double prespeed=sqrt(d2);
    double decel=SPRITE->decel_rate;
    if (!SPRITE->accel) decel*=4.000; // More deceleration when stopped.
    double postspeed=prespeed+decel*elapsed;
    if (postspeed<=0.0) {
      SPRITE->dx=0.0;
      SPRITE->dy=0.0;
    } else {
      double scale=postspeed/prespeed;
      SPRITE->dx*=scale;
      SPRITE->dy*=scale;
      motion=1;
    }
  }
  
  /* Apply new velocity to inertia.
   */
  if (SPRITE->accel) {
    double target=SPRITE->top_speed;
    if (SPRITE->steer) target*=SPRITE->steer_penalty;
    if (SPRITE->speed<target) {//XXX
      if ((SPRITE->speed+=SPRITE->accel_rate*elapsed)>=target) SPRITE->speed=target;
    } else if (SPRITE->speed>target) {
      if ((SPRITE->speed-=SPRITE->accel_rate*elapsed)<=target) SPRITE->speed=target;
    }
    SPRITE->speed=target;
    SPRITE->dx+=sin(SPRITE->facet)*elapsed*SPRITE->speed;
    SPRITE->dy-=cos(SPRITE->facet)*elapsed*SPRITE->speed;
    motion=1;
    
    // Clamp to the limit.
    d2=SPRITE->dx*SPRITE->dx+SPRITE->dy*SPRITE->dy;
    double limit2=SPRITE->top_speed*SPRITE->top_speed;
    if (d2>limit2) {
      double d=sqrt(d2);
      double scale=SPRITE->top_speed/d;
      SPRITE->dx*=scale;
      SPRITE->dy*=scale;
    }
  }
  
  /* Move per inertia.
   * When motion on one axis is blocked, flip and reduce it.
   * Our collisions are one-dimensional axis-aligned rectangles, not the circles you'd expect.
   */
  if (motion) {
    //fprintf(stderr,"d=%+f,%+f m/s\n",SPRITE->dx,SPRITE->dy);
    const double bounce=-0.500;
    if (!sprite_move(sprite,SPRITE->dx*elapsed,0.0)) {
      SPRITE->dx*=bounce;
    }
    if (!sprite_move(sprite,0.0,SPRITE->dy*elapsed)) {
      SPRITE->dy*=bounce;
    }
  }
  
  /* Decide whether the shadow should be near or far.
   * It's a subtle decorative effect, just there to look cool.
   */
  SPRITE->elevation=0;
  struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (map) {
    int qx=(int)sprite->x%NS_sys_mapw;
    int qy=(int)sprite->y%NS_sys_maph;
    if ((qx>=0)&&(qx<NS_sys_mapw)&&(qy>=0)&&(qy<NS_sys_maph)) {
      uint8_t tileid=map->v[qy*NS_sys_mapw+qx];
      uint8_t physics=map->physics[tileid];
      if ((physics==NS_physics_water)||(physics==NS_physics_hole)) {
        SPRITE->elevation=1;
      }
    }
  }
}

/* Render.
 */
 
static void _racer_render(struct sprite *sprite,int x,int y) {
  int srcx=SPRITE->frame*64;
  double sint=sin(SPRITE->facet);
  double cost=cos(SPRITE->facet);
  int shadowy=y+(SPRITE->elevation?4:3);
  graf_set_image(&g.graf,RID_image_broomrace);
  graf_set_filter(&g.graf,1);
  graf_set_tint(&g.graf,0x000000ff);
  graf_set_alpha(&g.graf,0x80);
  graf_decal_rotate(&g.graf,x,shadowy,srcx,SPRITE->srcy,64,sint,cost,0.333);
  graf_set_tint(&g.graf,0);
  graf_set_alpha(&g.graf,0xff);
  graf_decal_rotate(&g.graf,x,y,srcx,SPRITE->srcy,64,sint,cost,0.333);
  graf_set_filter(&g.graf,0);
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_racer={
  .name="racer",
  .objlen=sizeof(struct sprite_racer),
  .init=_racer_init,
  .update=_racer_update,
  .render=_racer_render,
};

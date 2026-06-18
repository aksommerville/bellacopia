#include "game/bellacopia.h"
#include "game/race/race.h"

#define CHECKPOINT_RADIUS2 4.0

struct sprite_racer {
  struct sprite hdr;
  int human,face;
  int srcy;
  
  // The man or CPU controller sets these and nothing else.
  int steer; // -1,0,1 = deasil,neutral,clockwise
  int accel; // 0,1. If we had a brake or reverse, it would be -1, but I think we're not doing that.
  
  // Extra state for CPU controller.
  double obsclock; // Counts down after obstruction detected.
  double obst; // Aim for this angle to escape obstruction.
  int trackp,trackc;
  double trackax,trackay,trackbx,trackby; // From (a) to (b), current leg of the track. (trackp) refers to (b).
  double samplex,sampley,sampleclock;
  
  double facet;
  int frame; // (0,1,2,3)=(normal,rush,turnleft,turnright)
  int elevation; // (0,1)=(lo,hi), are we above water? Controls the shadow depth.
  double steer_speed; // rad/s. Volatile, final answer.
  double top_speed; // m/s. Holds fairly constant, but adjusts per bells.
  double speed; // m/s. Highly volatile, adjusts each frame per input and steering.
  double decel_rate; // m/s**2, must be negative. Deceleration is constant, so it should be substantially less significant than acceleration.
  double steer_penalty; // Multiplier.
  double dx,dy; // m/s inertia.
  
  // Race state.
  int lapc,checkpointc;
  int lapp; // from one
  int checkpointp; // from zero
  double cpx,cpy;
  double racetime;
  double laptime;
  double laptime_best;
  int finished;
};

#define SPRITE ((struct sprite_racer*)sprite)

/* Init.
 */
 
static int _racer_init(struct sprite *sprite) {
  SPRITE->human=sprite->arg[0];
  SPRITE->face=sprite->arg[1];
  uint8_t orient=sprite->arg[2];
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
  
  switch (orient) {
    case 0x40: SPRITE->facet=0.0; break;
    case 0x10: SPRITE->facet=M_PI*-0.5; break;
    case 0x08: SPRITE->facet=M_PI*0.5; break;
    case 0x02: SPRITE->facet=M_PI; break;
  }
  
  if (sprite_group_add(GRP(visible),sprite)<0) return -1;
  if (sprite_group_add(GRP(update),sprite)<0) return -1;
  if (sprite_group_add(GRP(solid),sprite)<0) return -1;
  if (SPRITE->human==1) {
    if (sprite_group_add(GRP(hero),sprite)<0) return -1;
  }
  
  SPRITE->lapp=1;
  SPRITE->checkpointp=1; // We start on checkpoint zero, so one is the first to target.
  SPRITE->lapc=race_get_lapc();
  SPRITE->checkpointc=race_get_checkpointc();
  if ((SPRITE->lapc<1)||(SPRITE->checkpointc<2)) return -1;
  if (race_get_checkpoint(&SPRITE->cpx,&SPRITE->cpy,SPRITE->checkpointp)<0) return -1;
  SPRITE->steer_speed=5.000;
  SPRITE->top_speed=18.0;
  SPRITE->decel_rate=-8.0;
  SPRITE->steer_penalty=0.750;
  
  if (!SPRITE->human) {
    // CPU players are faster and won't collide with other sprites. But they are also laughably bad at turning. I think it kind of works out.
    SPRITE->steer_speed=10.0;
    SPRITE->top_speed=28.0;
    SPRITE->decel_rate=-12.0;
    SPRITE->steer_penalty=0.900;
    sprite->ignore_other_sprites=1;
    SPRITE->trackc=race_get_trackc();
    if (SPRITE->trackc<2) return -1;
    SPRITE->trackp=1;
    // The initial A point is our position, not necessarily the exact track position.
    SPRITE->trackax=sprite->x;
    SPRITE->trackay=sprite->y;
    if (race_get_track(&SPRITE->trackbx,&SPRITE->trackby,SPRITE->trackp)<0) return -1;
    SPRITE->samplex=sprite->x;
    SPRITE->sampley=sprite->y;
  }
  
  return 0;
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
}

/* Update, CPU control.
 */
 
static void racer_update_cpu(struct sprite *sprite,double elapsed) {

  /* If within a meter of point B, advance to the next.
   */
  double dx=SPRITE->trackbx-sprite->x;
  double dy=SPRITE->trackby-sprite->y;
  double d2=dx*dx+dy*dy;
  if (d2<1.000) {
    if (++(SPRITE->trackp)>=SPRITE->trackc) SPRITE->trackp=0;
    SPRITE->trackax=SPRITE->trackbx;
    SPRITE->trackay=SPRITE->trackby;
    race_get_track(&SPRITE->trackbx,&SPRITE->trackby,SPRITE->trackp);
  }
  
  /* If we're far off the track, our target is the nearest on-track point.
   * Otherwise it's point B.
   */
  double mex=sprite->x-SPRITE->trackax;
  double mey=sprite->y-SPRITE->trackay;
  double bx=SPRITE->trackbx-SPRITE->trackax;
  double by=SPRITE->trackby-SPRITE->trackay;
  double tracklen=sqrt(bx*bx+by*by);
  double rej=(mex*by-bx*mey)/tracklen;
  double proj=(mex*bx+mey*by)/tracklen;
  double targetx,targety;
  if (proj<0.0) {
    targetx=SPRITE->trackax;
    targety=SPRITE->trackay;
  } else if (proj>tracklen) {
    targetx=SPRITE->trackbx;
    targety=SPRITE->trackby;
  } else if ((rej<-1.0)||(rej>1.0)) {
    double nproj=proj/tracklen;
    targetx=SPRITE->trackax+bx*nproj;
    targety=SPRITE->trackay+by*nproj;
  } else {
    targetx=SPRITE->trackbx;
    targety=SPRITE->trackby;
  }
  
  /* Take the angle to our target.
   */
  double targett=atan2(targetx-sprite->x,sprite->y-targety);
  double dt=targett-SPRITE->facet;
  while (dt>M_PI) dt-=M_PI*2.0;
  while (dt<-M_PI) dt+=M_PI*2.0;
  if (dt<-0.010) SPRITE->steer=-1;
  else if (dt>0.010) SPRITE->steer=1;
  else SPRITE->steer=0;
  if ((dt>-1.000)&&(dt<1.000)) SPRITE->accel=1;
  else SPRITE->accel=0;
}

/* Update.
 */
 
static void _racer_update(struct sprite *sprite,double elapsed) {

  /* If finished, we do let normal physics wind down.
   * But the controllers no longer get to play.
   */
  if (SPRITE->finished) {
    SPRITE->steer=0;
    SPRITE->accel=0;
  } else if (race_get_countdown()>0.0) {
    return;
  } else {
    SPRITE->laptime+=elapsed;
    SPRITE->racetime+=elapsed;
    if (SPRITE->human) racer_update_man(sprite,elapsed,g.input[SPRITE->human],g.pvinput[SPRITE->human]);
    else racer_update_cpu(sprite,elapsed);
  }
  
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
  
  /* Checkpoint?
   */
  if (!SPRITE->finished) {
    double dx=SPRITE->cpx-sprite->x;
    double dy=SPRITE->cpy-sprite->y;
    d2=dx*dx+dy*dy;
    if (d2<CHECKPOINT_RADIUS2) {
      if (!SPRITE->checkpointp) { // Reached checkpoint zero -- advance lap or end race.
        if (SPRITE->lapp>=SPRITE->lapc) {
          SPRITE->finished=1;
          race_check_completion(SPRITE->human);
          return;
        }
        if ((SPRITE->lapp==1)||(SPRITE->laptime<SPRITE->laptime_best)) SPRITE->laptime_best=SPRITE->laptime;
        SPRITE->lapp++;
        SPRITE->laptime=0.0;
      }
      if (SPRITE->human) bm_sound(RID_sound_collect);
      if (++(SPRITE->checkpointp)>=SPRITE->checkpointc) {
        SPRITE->checkpointp=0;
      }
      race_get_checkpoint(&SPRITE->cpx,&SPRITE->cpy,SPRITE->checkpointp);
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
  
  /* For humans, an indicator pointing to the next checkpoint.
   */
  if (SPRITE->human&&!SPRITE->finished) {
    double dx=SPRITE->cpx-sprite->x;
    double dy=SPRITE->cpy-sprite->y;
    double d2=dx*dx+dy*dy;
    if (d2>CHECKPOINT_RADIUS2) {
      double distance=sqrt(d2);
      double indradius=40.0;
      int indx=x+dx*indradius/distance;
      int indy=y+dy*indradius/distance;
      int alpha=(g.framec&0x10)?0xc0:0x40;
      int rotation=(int)((atan2(dx,-dy)*128.0)/M_PI);
      graf_set_image(&g.graf,RID_image_pause);
      graf_set_filter(&g.graf,1);
      graf_fancy(&g.graf,indx,indy,0x9b,0,rotation&0xff,NS_sys_tilesize,0,0x00ff0000|alpha);
      graf_set_filter(&g.graf,0);
    }
  }
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

/* Public accessors.
 */
 
int sprite_racer_get_checkpointp(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_racer)) return -1;
  return SPRITE->checkpointp;
}

int sprite_racer_is_finished(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_racer)) return 0;
  return SPRITE->finished;
}

int sprite_racer_is_human(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_racer)) return 0;
  return SPRITE->human;
}

double sprite_racer_get_lap_time(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_racer)) return 0.0;
  if (SPRITE->finished) return SPRITE->laptime_best;
  return SPRITE->laptime;
}

double sprite_racer_get_race_time(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_racer)) return 0.0;
  return SPRITE->racetime;
}

int sprite_racer_get_lapp(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_racer)) return 0;
  if (SPRITE->finished) return SPRITE->lapc+1;
  return SPRITE->lapp;
}

/* battle_mindcontrol.c
 * Alternate A/B in the right rhythm to maintain psychic control. While control established, dpad moves your victim.
 * Push a piece of candy thru the hazards.
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_world.h"

/* Our sprite ids are also arguments to "battlemark" poi in the map.
 */
#define SPRITEID_LMAN 1
#define SPRITEID_RMAN 2
#define SPRITEID_LCAT 3
#define SPRITEID_RCAT 4
#define BATTLEMARK_TREE 5 /* POI must describe a rectangle. */

#define CHARGE_PENALTY      0.250 /* s. Added to charge clock for an out-of-sequence stroke. */
#define CHARGE_INTERVAL_MAX 0.500 /* s. So long between strokes and we lose charge. More rhythm cadence than karate-chop cadence. */
#define CHARGE_MAX          1.250 /* The extent above 1.0 is a buffer against momentary loss. */
#define CPU_STROKE_INTERVAL 0.120 /* s */
#define CAT_SPEED_MIN 3.0 /* m/s */
#define CAT_SPEED_MAX 6.0 /* m/s */
#define CPU_PENALTY 0.900 /* CPU players are nearly perfect. Handicap their cats' speed to make it closer to fair. */
#define REST_TIME_MIN 0.100 /* CPU players stop at each step, for a duration both random and skewed by bias. */
#define REST_TIME_MAX 0.300
#define STEP_LIMIT 64

struct battle_mindcontrol {
  struct battle hdr;
  struct batsup_world *world;
  double treex,treey,treew,treeh;
};

#define BATTLE ((struct battle_mindcontrol*)battle)

struct sprite_man {
  struct batsup_sprite hdr;
  int human; // player id or zero for cpu control
  int btnid_next;
  double chargeclock; // Counts up and zeroes at each valid stroke.
  double charge; // >=1 if effective.
  double catspeed;
  // For CPU player:
  double strokeclock;
  double restclock;
  double restmin,restmax;
  // (stepv) describes a path from the man to the tree.
  // Adjacent steps are axis-aligned and have no intervening obstacles.
  struct step { int8_t x,y; } stepv[STEP_LIMIT];
  int stepc;
  int stepp;
};

struct sprite_cat {
  struct batsup_sprite hdr;
  double animclock;
  int animframe;
  int mindcontrolled;
  int has_apple;
  int walking;
};

/* Receive keystroke for a man.
 */
 
static void sprite_man_stroke(struct batsup_sprite *sprite,int btnid) {
  struct sprite_man *SPRITE=(struct sprite_man*)sprite;
  if (btnid==SPRITE->btnid_next) {
    SPRITE->btnid_next^=(EGG_BTN_SOUTH|EGG_BTN_WEST);
    SPRITE->chargeclock=0.0;
  } else {
    SPRITE->chargeclock+=CHARGE_PENALTY;
  }
}

/* Dpad state for a man.
 * The Man and CPU controllers should call this every frame.
 */
 
static void sprite_man_dpad(struct batsup_sprite *sprite,int dx,int dy,double elapsed) {
  struct sprite_man *SPRITE=(struct sprite_man*)sprite;
  struct batsup_sprite *cat=0;
  switch (sprite->id) {
    case SPRITEID_LMAN: cat=batsup_sprite_by_id(sprite->world,SPRITEID_LCAT); break;
    case SPRITEID_RMAN: cat=batsup_sprite_by_id(sprite->world,SPRITEID_RCAT); break;
  }
  if (!cat) return;
  struct sprite_cat *CAT=(struct sprite_cat*)cat;
  
  if (SPRITE->charge<1.0) {
    CAT->mindcontrolled=0;
    
  } else {
    CAT->mindcontrolled=1;
    if (dx||dy) {
      CAT->walking=1;
      batsup_sprite_move(cat,SPRITE->catspeed*dx*elapsed,SPRITE->catspeed*dy*elapsed);
      if (dx<0) cat->xform=EGG_XFORM_XREV;
      else cat->xform=0;
    } else {
      CAT->walking=0;
    }
  }
}

/* Update charge.
 * May set or unset control.
 */
 
static void sprite_man_update_charge(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_man *SPRITE=(struct sprite_man*)sprite;
  SPRITE->chargeclock+=elapsed;
  if (SPRITE->chargeclock>=CHARGE_INTERVAL_MAX) {
    if ((SPRITE->charge-=elapsed)<=0.0) SPRITE->charge=0.0;
  } else {
    if ((SPRITE->charge+=elapsed)>=CHARGE_MAX) SPRITE->charge=CHARGE_MAX;
  }
}

/* Update human player.
 */
 
static void sprite_update_man(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_man *SPRITE=(struct sprite_man*)sprite;
  sprite_man_update_charge(sprite,elapsed);
  
  if ((g.input[SPRITE->human]&EGG_BTN_SOUTH)&&!(g.pvinput[SPRITE->human]&EGG_BTN_SOUTH)) sprite_man_stroke(sprite,EGG_BTN_SOUTH);
  if ((g.input[SPRITE->human]&EGG_BTN_WEST)&&!(g.pvinput[SPRITE->human]&EGG_BTN_WEST)) sprite_man_stroke(sprite,EGG_BTN_WEST);
  int dx=0,dy=0;
  switch (g.input[SPRITE->human]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: dx=-1; break;
    case EGG_BTN_RIGHT: dx=1; break;
  }
  switch (g.input[SPRITE->human]&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: dy=-1; break;
    case EGG_BTN_DOWN: dy=1; break;
  }
  sprite_man_dpad(sprite,dx,dy,elapsed);
}

/* Update CPU player.
 */
 
static void sprite_update_cpu(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_man *SPRITE=(struct sprite_man*)sprite;
  sprite_man_update_charge(sprite,elapsed);
  
  // CPU's toggling is perfect:
  if ((SPRITE->strokeclock-=elapsed)<=0.0) {
    SPRITE->strokeclock+=CPU_STROKE_INTERVAL;
    sprite_man_stroke(sprite,SPRITE->btnid_next);
  }
  
  // Take a break every step.
  if (SPRITE->restclock>0.0) {
    SPRITE->restclock-=elapsed;
    sprite_man_dpad(sprite,0,0,elapsed);
    return;
  }
  
  /* Find the cat.
   */
  if ((SPRITE->stepc>0)&&(SPRITE->charge>=1.0)) {
    struct batsup_sprite *cat=0;
    if (sprite->id==SPRITEID_LMAN) cat=batsup_sprite_by_id(sprite->world,SPRITEID_LCAT);
    else cat=batsup_sprite_by_id(sprite->world,SPRITEID_RCAT);
    if (cat) {
      struct sprite_cat *CAT=(struct sprite_cat*)cat;
    
      /* If (stepp) out of range, find the step nearest the cat.
       * This means we're newly charged, and the cat could be anywhere.
       */
      if ((SPRITE->stepp<0)||(SPRITE->stepp>=SPRITE->stepc)) {
        SPRITE->stepp=0;
        double bestd2=999999.999;
        struct step *step=SPRITE->stepv;
        int i=SPRITE->stepc;
        for (;i-->0;step++) {
          double stepx=step->x+0.5;
          double stepy=step->y+0.5;
          double dx=stepx-cat->x;
          double dy=stepy-cat->y;
          double d2=dx*dx+dy*dy;
          if (d2<bestd2) {
            SPRITE->stepp=step-SPRITE->stepv;
            bestd2=d2;
            if (d2<1.0) break; // Not going to get closer than this.
          }
        }
      }
      
      /* If we're close to the target step, advance to the next one.
       * Forward if no apple, or backward if it has the apple.
       * And begin a rest.
       */
      double dx=SPRITE->stepv[SPRITE->stepp].x+0.5-cat->x;
      double dy=SPRITE->stepv[SPRITE->stepp].y+0.5-cat->y;
      double closeenough=0.125;
      if ((dx>=-closeenough)&&(dy>=-closeenough)&&(dx<=closeenough)&&(dy<=closeenough)) {
        if (CAT->has_apple) {
          if (SPRITE->stepp>0) SPRITE->stepp--;
        } else {
          if (SPRITE->stepp<SPRITE->stepc-1) SPRITE->stepp++;
        }
        dx=SPRITE->stepv[SPRITE->stepp].x+0.5-cat->x;
        dy=SPRITE->stepv[SPRITE->stepp].y+0.5-cat->y;
        double rest=(rand()&0xffff)/65535.0;
        SPRITE->restclock+=rest*SPRITE->restmin+(1.0-rest)*SPRITE->restmax;
      }
      
      /* If the cat is very close to the target on either axis, force it exactly equal.
       * Mitigates jitter.
       */
      double veryclose=0.040;
      if ((dx>=-veryclose)&&(dx<=veryclose)) {
        dx=0.0;
        cat->x=SPRITE->stepv[SPRITE->stepp].x+0.5;
      }
      if ((dy>=-veryclose)&&(dy<=veryclose)) {
        dy=0.0;
        cat->y=SPRITE->stepv[SPRITE->stepp].y+0.5;
      }
      
      // Dpad according to cat's delta to step.
      int indx=(dx<0.0)?-1:(dx>0.0)?1:0;
      int indy=(dy<0.0)?-1:(dy>0.0)?1:0;
      sprite_man_dpad(sprite,indx,indy,elapsed);
    } else {
      sprite_man_dpad(sprite,0,0,elapsed);
    }
  } else {
    // Not charged, the cat might stray. Forget where it was.
    SPRITE->stepp=-1;
    sprite_man_dpad(sprite,0,0,elapsed);
  }
}

/* Render man.
 * Adjusts tileid per state, plus a charge meter above.
 */
 
static void sprite_render_man(struct batsup_sprite *sprite,int dstx,int dsty) {
  struct sprite_man *SPRITE=(struct sprite_man*)sprite;
  graf_set_image(&g.graf,sprite->imageid);
  
  int metery=dsty-NS_sys_tilesize;
  if (SPRITE->charge>=1.0) {
    int haty=dsty-12;
    if (g.framec&16) haty--;
    graf_tile(&g.graf,dstx,dsty,sprite->tileid+1,sprite->xform);
    graf_tile(&g.graf,dstx,haty,sprite->tileid+2,sprite->xform);
    graf_tile(&g.graf,dstx,metery,(g.framec&8)?0xa7:0xa8,0);
  } else {
    graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
    uint8_t metertile=0xa3+(int)(SPRITE->charge*4.0);
    if (metertile<0xa3) metertile=0xa3;
    else if (metertile>0xa6) metertile=0xa6;
    graf_tile(&g.graf,dstx,metery,metertile,0);
  }
}

/* Update cat.
 */
 
static void sprite_update_cat(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_cat *SPRITE=(struct sprite_cat*)sprite;
  struct battle *battle=sprite->world->battle;
  
  // Animate.
  if (SPRITE->walking) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.250;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
      sprite->tileid=0xb0;
      if (SPRITE->mindcontrolled) sprite->tileid+=3;
      switch (SPRITE->animframe) {
        case 1: sprite->tileid+=1; break;
        case 3: sprite->tileid+=2; break;
      }
    }
  } else {
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
    sprite->tileid=0xb0;
    if (SPRITE->mindcontrolled) sprite->tileid+=3;
  }
  
  // Move.
  if (SPRITE->mindcontrolled) {
    // Motion is effected by our master.
    SPRITE->walking=0;
  } else {
    SPRITE->has_apple=0;
    //TODO Drunk walk?
  }
  
  // Collect or deliver apple.
  if (SPRITE->has_apple) {
    struct batsup_sprite *master=0;
    switch (sprite->id) {
      case SPRITEID_LCAT: master=batsup_sprite_by_id(sprite->world,SPRITEID_LMAN); break;
      case SPRITEID_RCAT: master=batsup_sprite_by_id(sprite->world,SPRITEID_RMAN); break;
    }
    if (master) {
      double dx=sprite->x-master->x;
      double dy=sprite->y-master->y;
      if (dx<0.0) dx=-dx;
      if (dy<0.0) dy=-dy;
      if ((dx<1.250)&&(dy<1.250)) {
        bm_sound(RID_sound_treasure);
        SPRITE->has_apple=0;
        if (sprite->id==SPRITEID_LCAT) battle->outcome=1;
        else battle->outcome=-1;
      }
    }
  } else if (SPRITE->mindcontrolled) {
    if ((sprite->x>=BATTLE->treex)&&(sprite->y>=BATTLE->treey)&&(sprite->x<BATTLE->treex+BATTLE->treew)&&(sprite->y<BATTLE->treey+BATTLE->treeh)) {
      bm_sound(RID_sound_collect);
      SPRITE->has_apple=1;
    }
  }
}

/* Render cat.
 * It's the generic render, plus one overlay tile if we have the apple.
 */
 
static void sprite_render_cat(struct batsup_sprite *sprite,int dstx,int dsty) {
  struct sprite_cat *SPRITE=(struct sprite_cat*)sprite;
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
  if (SPRITE->has_apple) {
    graf_tile(&g.graf,dstx,dsty,0xb6,sprite->xform);
  }
}

/* Delete.
 */
 
static void _mindcontrol_del(struct battle *battle) {
}

/* Hero tiles.
 */
 
static uint8_t mindcontrol_hero_tile(int face) {
  switch (face) {
    case NS_face_dot: return 0x80;
    case NS_face_princess: return 0x90;
    case NS_face_monster: return 0xa0;
  }
  return 0xa0;
}

/* Prioritized list of candidate steps from a given position.
 * (srcx,srcy) is where we're at. Candidates will be one cardinal step from there.
 * (map) is (mapw*maph) with zero passable.
 * (dst) and (src) must be in bounds.
 * Returns 0..4 candidates, with the first being the most preferred.
 */
 
static int mindcontrol_get_path_candidates(struct step *stepv,const uint8_t *map,int srcx,int srcy,int dstx,int dsty) {
  int stepc=0;
  
  /* First list all four steps, in order of preference.
   */
  int dx=dstx-srcx;
  int adx=dx; if (adx<0) adx=-adx;
  int nx=(dx<0)?-1:1;
  int dy=dsty-srcy;
  int ady=dy; if (ady<0) ady=-ady;
  int ny=(dy<0)?-1:1;
  if (!dx&&!dy) return 0; // We're already there; you shouldn't have called.
  if (adx>=ady) { // Horizontal first and last, vertical in between.
    stepv[stepc++]=(struct step){srcx+nx,srcy};
    stepv[stepc++]=(struct step){srcx,srcy+ny};
    stepv[stepc++]=(struct step){srcx,srcy-ny};
    stepv[stepc++]=(struct step){srcx-nx,srcy};
  } else { // Vertical first and last, horizontal in between.
    stepv[stepc++]=(struct step){srcx,srcy+ny};
    stepv[stepc++]=(struct step){srcx+nx,srcy};
    stepv[stepc++]=(struct step){srcx-nx,srcy};
    stepv[stepc++]=(struct step){srcx,srcy-ny};
  }
  
  /* Then filter out steps if OOB or impassable.
   */
  int i=stepc;
  struct step *step=stepv+stepc-1;
  for (;i-->0;step--) {
    if ((step->x<0)||(step->y<0)||(step->x>=NS_sys_mapw)||(step->y>=NS_sys_maph)||map[step->y*NS_sys_mapw+step->x]) {
      stepc--;
      memmove(step,step+1,sizeof(struct step)*(stepc-i));
    }
  }
  return stepc;
}

/* Generate path for a CPU player.
 * Return <0 to fail, and should log the reason.
 */
 
static int mindcontrol_generate_path(struct battle *battle,struct batsup_sprite *sprite) {
  struct sprite_man *SPRITE=(struct sprite_man*)sprite;
  SPRITE->stepp=-1;

  /* Determine the two endpoints.
   */
  if ((BATTLE->treew<0.5)||(BATTLE->treeh<0.5)) {
    fprintf(stderr,"%s: No tree in map:%d\n",__func__,BATTLE->world->map->rid);
    return -1;
  }
  int dstx=(int)(BATTLE->treex+BATTLE->treew*0.5);
  int dsty=(int)(BATTLE->treey+BATTLE->treeh*0.5);
  if ((dstx<0)||(dsty<0)||(dstx>=NS_sys_mapw)||(dsty>=NS_sys_maph)) {
    fprintf(stderr,"%s: Tree (%f,%f,%f,%f) in map:%d out of bounds\n",__func__,BATTLE->treex,BATTLE->treey,BATTLE->treew,BATTLE->treeh,BATTLE->world->map->rid);
    return -1;
  }
  int srcx=(int)sprite->x,srcy=(int)sprite->y;
  if ((srcx<0)||(srcy<0)||(srcx>=NS_sys_mapw)||(srcy>=NS_sys_maph)) {
    fprintf(stderr,"%s: Player (%f,%f) in map:%d out of bounds\n",__func__,sprite->x,sprite->y,BATTLE->world->map->rid);
    return -1;
  }
  
  /* Make a simplified copy of the map's physics.
   * Each cell is 0=available, 1=impassable.
   */
  uint8_t map[NS_sys_mapw*NS_sys_maph];
  uint8_t *dst=map;
  const uint8_t *src=BATTLE->world->map->v;
  int i=NS_sys_mapw*NS_sys_maph;
  for (;i-->0;dst++,src++) {
    switch (BATTLE->world->map->physics[*src]) {
      case NS_physics_vacant: case NS_physics_safe: *dst=0; break;
      default: *dst=1;
    }
  }
  
  /* First step is always (srcx,srcy).
   * Add to the path and mark it impassable.
   */
  SPRITE->stepc=0;
  SPRITE->stepv[SPRITE->stepc++]=(struct step){.x=srcx,.y=srcy};
  map[srcy*NS_sys_mapw+srcx]=2;
  int px=srcx,py=srcy;
  //fprintf(stderr,"%s:%d: Start at %d,%d. Target at %d,%d\n",__FILE__,__LINE__,px,py,dstx,dsty);
  
  /* Determining the rest of the path is complicated.
   * We're not going to find the optimal path, just a valid and unsurprising one.
   */
  int panic=NS_sys_mapw*NS_sys_maph*2;
  while ((px!=dstx)||(py!=dsty)) {
  
    /* Visiting every cell twice suggests that we're stuck in a loop.
     * This should never happen but hey I'm not perfect.
     */
    if (--panic<0) {
      fprintf(stderr,"%s: Unexpected failure to find a path after %d iterations, on map:%d. Aborting.\n",__func__,NS_sys_mapw*NS_sys_maph*2,BATTLE->world->map->rid);
      return -1;
    }
    
    /* First get a prioritized list of candidate moves from (px,py) toward (dstx,dsty).
     * Moves are one cardinal step.
     * If our path is full, it's automatically empty.
     */
    struct step candidatev[4];
    int candidatec=0;
    if (SPRITE->stepc<STEP_LIMIT) {
      candidatec=mindcontrol_get_path_candidates(candidatev,map,px,py,dstx,dsty);
    }
    //fprintf(stderr,"%s:%d: %d candidates at %d,%d\n",__FILE__,__LINE__,candidatec,px,py);
    
    /* If there are no candidates here, mark this cell impassable and pop it from the path.
     * If the path pops to empty, fail.
     * Otherwise try again from the previous step.
     */
    if (!candidatec) {
      map[py*NS_sys_mapw+px]=2;
      SPRITE->stepc--;
      if (SPRITE->stepc<=0) {
        // This is the expected failure case for maps that actually don't have a path between tree and man.
        fprintf(stderr,"%s: Failed to generate path in map:%d\n",__func__,BATTLE->world->map->rid);
        return -1;
      }
      px=SPRITE->stepv[SPRITE->stepc-1].x;
      py=SPRITE->stepv[SPRITE->stepc-1].y;
      //fprintf(stderr,"%s:%d: Back off to %d,%d\n",__FILE__,__LINE__,px,py);
      continue;
    }
    
    /* Push the first candidate.
     */
    SPRITE->stepv[SPRITE->stepc++]=candidatev[0];
    px=candidatev[0].x;
    py=candidatev[0].y;
    map[py*NS_sys_mapw+px]=2;
    //fprintf(stderr,"%s:%d: Advance to %d,%d\n",__FILE__,__LINE__,px,py);
  }
  
  if (0) { // Show me.
    fprintf(stderr,"%s: Generated path of %d steps:\n",__func__,SPRITE->stepc);
    struct step *step=SPRITE->stepv;
    for (i=SPRITE->stepc;i-->0;step++) fprintf(stderr,"  %d,%d\n",step->x,step->y);
  }
  return 0;
}

/* New.
 */
 
static int _mindcontrol_init(struct battle *battle) {
  
  /* Map selection is random.
   * They're all symmetric to the two players.
   * I was thinking of effecting difficulty via the maps, and still might return to that.
   */
  int ridv[]={
    RID_map_mindcontrol,
    RID_map_mindcontrol2,
    RID_map_mindcontrol3,
  };
  int ridc=sizeof(ridv)/sizeof(ridv[0]);
  int rid=ridv[rand()%ridc];
  if (!(BATTLE->world=batsup_world_new(battle,rid))) return -1;
  
  struct cmdlist_reader reader={.v=BATTLE->world->map->cmd,.c=BATTLE->world->map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_battlemark: {
          int arg=(cmd.arg[2]<<8)|cmd.arg[3];
          switch (arg) {
          
            case SPRITEID_LMAN: {
                struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_LMAN,sizeof(struct sprite_man));
                if (!sprite) return -1;
                struct sprite_man *SPRITE=(struct sprite_man*)sprite;
                sprite->render=sprite_render_man;
                if (battle->args.lctl) sprite->update=sprite_update_man;
                else sprite->update=sprite_update_cpu;
                SPRITE->human=battle->args.lctl;
                sprite->x=cmd.arg[0]+0.5;
                sprite->y=cmd.arg[1]+0.5;
                sprite->tileid=mindcontrol_hero_tile(battle->args.lface);
                SPRITE->btnid_next=EGG_BTN_WEST;
                SPRITE->catspeed=(0xff-battle->args.bias)/255.0;
                SPRITE->catspeed=SPRITE->catspeed*CAT_SPEED_MAX+(1.0-SPRITE->catspeed)*CAT_SPEED_MIN;
                SPRITE->restmin=(0xff-battle->args.bias)/255.0;
                SPRITE->restmin=SPRITE->restmin*REST_TIME_MIN+(1.0-SPRITE->restmin)*REST_TIME_MAX;
                SPRITE->restmax=SPRITE->restmin*1.500;
              } break;

            case SPRITEID_RMAN: {
                struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_RMAN,sizeof(struct sprite_man));
                if (!sprite) return -1;
                struct sprite_man *SPRITE=(struct sprite_man*)sprite;
                sprite->render=sprite_render_man;
                if (battle->args.rctl) sprite->update=sprite_update_man;
                else sprite->update=sprite_update_cpu;
                SPRITE->human=battle->args.rctl;
                sprite->x=cmd.arg[0]+0.5;
                sprite->y=cmd.arg[1]+0.5;
                sprite->tileid=mindcontrol_hero_tile(battle->args.rface);
                sprite->xform=EGG_XFORM_XREV;
                SPRITE->btnid_next=EGG_BTN_WEST;
                SPRITE->catspeed=battle->args.bias/255.0;
                SPRITE->catspeed=SPRITE->catspeed*CAT_SPEED_MAX+(1.0-SPRITE->catspeed)*CAT_SPEED_MIN;
                SPRITE->restmin=battle->args.bias/255.0;
                SPRITE->restmin=SPRITE->restmin*REST_TIME_MIN+(1.0-SPRITE->restmin)*REST_TIME_MAX;
                SPRITE->restmax=SPRITE->restmin*1.500;
              } break;

            case SPRITEID_LCAT: {
                struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_LCAT,sizeof(struct sprite_cat));
                if (!sprite) return -1;
                struct sprite_cat *SPRITE=(struct sprite_cat*)sprite;
                sprite->update=sprite_update_cat;
                sprite->render=sprite_render_cat;
                sprite->x=cmd.arg[0]+0.5;
                sprite->y=cmd.arg[1]+0.5;
                sprite->tileid=0xb0;
                sprite->solid=1;
              } break;

            case SPRITEID_RCAT: {
                struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_RCAT,sizeof(struct sprite_cat));
                if (!sprite) return -1;
                struct sprite_cat *SPRITE=(struct sprite_cat*)sprite;
                sprite->update=sprite_update_cat;
                sprite->render=sprite_render_cat;
                sprite->x=cmd.arg[0]+0.5;
                sprite->y=cmd.arg[1]+0.5;
                sprite->tileid=0xb0;
                sprite->xform=EGG_XFORM_XREV;
                sprite->solid=1;
              } break;

            case BATTLEMARK_TREE: {
                if (BATTLE->treew<0.5) {
                  BATTLE->treex=cmd.arg[0];
                  BATTLE->treey=cmd.arg[1];
                  BATTLE->treew=1.0;
                  BATTLE->treeh=1.0;
                } else {
                  double nv=cmd.arg[0];
                  if (nv<BATTLE->treex) {
                    BATTLE->treew+=BATTLE->treex-nv;
                    BATTLE->treex=nv;
                  }
                  nv+=1.0;
                  if (nv>BATTLE->treex+BATTLE->treew) {
                    BATTLE->treew=nv-BATTLE->treex;
                  }
                  nv=cmd.arg[1];
                  if (nv<BATTLE->treey) {
                    BATTLE->treeh+=BATTLE->treey-nv;
                    BATTLE->treey=nv;
                  }
                  nv+=1.0;
                  if (nv>BATTLE->treey+BATTLE->treeh) {
                    BATTLE->treeh=nv-BATTLE->treey;
                  }
                }
              } break;
          }
        } break;
    }
  }
  
  /* Generate paths after everything is settled.
   */
  struct batsup_sprite *sprite;
  if (sprite=batsup_sprite_by_id(BATTLE->world,SPRITEID_LMAN)) {
    if (!((struct sprite_man*)sprite)->human) {
      if (mindcontrol_generate_path(battle,sprite)<0) return -1;
    }
  } else {
    fprintf(stderr,"%s: left player not found on map:%d\n",__func__,BATTLE->world->map->rid);
    return -1;
  }
  if (sprite=batsup_sprite_by_id(BATTLE->world,SPRITEID_RMAN)) {
    if (!((struct sprite_man*)sprite)->human) {
      if (mindcontrol_generate_path(battle,sprite)<0) return -1;
    }
  } else {
    fprintf(stderr,"%s: right player not found on map:%d\n",__func__,BATTLE->world->map->rid);
    return -1;
  }

  return 0;
}

/* Update.
 */
 
static void _mindcontrol_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  batsup_world_update(BATTLE->world,elapsed);
}

/* Render.
 */
 
static void _mindcontrol_render(struct battle *battle) {
  batsup_world_render(BATTLE->world);
}

/* Type definition.
 */
 
const struct battle_type battle_type_mindcontrol={
  .name="mindcontrol",
  .objlen=sizeof(struct battle_mindcontrol),
  .strix_name=177,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_mindcontrol_del,
  .init=_mindcontrol_init,
  .update=_mindcontrol_update,
  .render=_mindcontrol_render,
};

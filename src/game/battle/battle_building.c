/* battle_building.c
 * Mini platformer, carry boxes to build a staircase to ring the bell.
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_world.h"

#define SPRID_LPLAYER 1
#define SPRID_RPLAYER 2
#define SPRID_LBELL 3
#define SPRID_RBELL 4
#define SPRID_BRICK0 100 /* +n */

#define GRAVITY_ACCEL 20.0 /* m/s**2 */
#define GRAVITY_LIMIT 10.0 /* m/s */
#define WALKSPEED_LO 5.0
#define WALKSPEED_HI 7.0

struct battle_building {
  struct battle hdr;
  struct batsup_world *world;
};

#define BATTLE ((struct battle_building*)battle)

/* Brick.
 **********************************************************************************/
 
struct sprite_brick {
  struct batsup_sprite hdr;
  double gravity;
  int carried;
};

static void brick_update(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_brick *SPRITE=(struct sprite_brick*)sprite;
  if (SPRITE->carried) return;
  if ((SPRITE->gravity+=GRAVITY_ACCEL*elapsed)>GRAVITY_LIMIT) SPRITE->gravity=GRAVITY_LIMIT;
  batsup_sprite_move(sprite,0.0,SPRITE->gravity*elapsed);
}
  
static void brick_set_carried(struct batsup_sprite *sprite,int carried) {
  struct sprite_brick *SPRITE=(struct sprite_brick*)sprite;
  SPRITE->gravity=0.0; // Whether starting or stopping, nix gravity.
  if (SPRITE->carried=carried) {
    sprite->solid=0;
  } else {
    sprite->solid=1;
  }
}
 
static struct batsup_sprite *brick_spawn(struct battle *battle,int x,int y,int id) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,id,sizeof(struct sprite_brick));
  if (!sprite) return 0;
  struct sprite_brick *SPRITE=(struct sprite_brick*)sprite;
  sprite->update=brick_update;
  sprite->x=x+0.5;
  sprite->y=y+0.5;
  sprite->solid=1;
  sprite->fullmeter=1;
  switch (rand()%6) {
    case 0: sprite->tileid=0x39; break;
    case 1: sprite->tileid=0x3a; break;
    case 2: sprite->tileid=0x49; break;
    case 3: sprite->tileid=0x4a; break;
    case 4: sprite->tileid=0x59; break;
    case 5: sprite->tileid=0x5a; break;
  }
  return sprite;
}

static struct batsup_sprite *building_find_brick(struct batsup_world *world,double x,double y) {
  struct batsup_sprite **spritep=world->spritev;
  int i=world->spritec;
  for (;i-->0;spritep++) {
    struct batsup_sprite *sprite=*spritep;
    if (sprite->id<SPRID_BRICK0) continue;
    if (!sprite->solid) continue;
    double dx=sprite->x-x;
    if ((dx<=-0.5)||(dx>=0.5)) continue;
    double dy=sprite->y-y;
    if ((dy<=-0.5)||(dy>=0.5)) continue;
    return sprite;
  }
  return 0;
}

/* Bell.
 ************************************************************************************/
 
struct sprite_bell {
  struct batsup_sprite hdr;
  int ringing;
  double animclock;
  int animframe;
};

static void bell_update(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_bell *SPRITE=(struct sprite_bell*)sprite;
  if (SPRITE->ringing) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.250;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
      switch (SPRITE->animframe) {
        case 0: case 2: sprite->tileid=0x3b; break;
        case 1: bm_sound(RID_sound_ding); sprite->tileid=0x4b; break;
        case 3: bm_sound(RID_sound_dong); sprite->tileid=0x5b; break;
      }
    }
  }
}
 
static struct batsup_sprite *bell_spawn(struct battle *battle,int x,int id) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,id,sizeof(struct sprite_bell));
  if (!sprite) return 0;
  sprite->update=bell_update;
  sprite->x=x+0.5;
  sprite->y=1.5;
  sprite->tileid=0x3b;
  return sprite;
}

static void bell_ring(struct batsup_sprite *sprite) {
  struct sprite_bell *SPRITE=(struct sprite_bell*)sprite;
  SPRITE->ringing=1;
}
 
/* Player, both man and cpu.
 *************************************************************************************/
 
struct sprite_player {
  struct batsup_sprite hdr;
// Controller sets these per real input or cpu decisions:
  int indx,indy;
  int injump,inpickup;
// Specifics for human players:
  int inputp;
  int injump_blackout;
// Specifics for cpu players:
// State shared between man and cpu:
  int pvinjump;
  double skill;
  uint8_t tileid0;
  double animclock;
  int animframe;
  double walkspeed;
  double jumpclock; // Counts down while jumping; compute velocity from it.
  int seated;
  double gravity; // Starts at zero.
  int pumpkinid; // Sprite ID of a brick, or zero if none.
};

static double player_jump_dy(const struct batsup_sprite *sprite) {
  const struct sprite_player *SPRITE=(const struct sprite_player*)sprite;
  return -SPRITE->jumpclock*45.0;
}

static int cmpx(const void *a,const void *b) {
  const struct batsup_sprite *A=*(const void**)a;
  const struct batsup_sprite *B=*(const void**)b;
  if (A->x<B->x) return -1;
  if (A->x>B->x) return 1;
  return 0;
}

static int player_has_no_pumpkin_space(struct batsup_sprite *sprite,struct batsup_sprite *ignore) {
  /* Check all possible positions this player sprite can occupy.
   * If we find some position with a full square meter of space directly overhead, great, return zero.
   * If not, we are trapped: Holding a pumpkin and got nowhere to put it. Return nonzero and caller will destroy the pumpkin.
   * This mitigation exists because it is possible to trap yourself by grabbing a pumpkin out of the wall such that others fall to lock you in.
   * I believe it's only possible to be trapped like this if you have less than 3 meters of horizontal freedom.
   */
  double ledge=0.0,redge=NS_sys_mapw;
  double above=sprite->y-0.5;
  double below=sprite->y+0.5;
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other==ignore) continue;
    if (!other->solid) continue;
    if (other->id<SPRID_BRICK0) continue;
    if (other->y<above) continue;
    if (other->y>below) continue;
    if (other->x<sprite->x) {
      if (other->x>ledge) ledge=other->x;
    } else {
      if (other->x<redge) redge=other->x;
    }
  }
  double freedom=redge-ledge;
  freedom-=1.0; // We were measuring from centers.
  if (freedom>3.0) return 0; // Plenty of horizontal freedom, there must be somewhere to put it.
  
  /* Now run thru them again, find those in the row just above me, and sort horizontally.
   * If there's room here, call it free.
   * We'd land here if you're in a small depression, but it has only a small ledge above.
   */
  above-=1.0;
  below-=1.0;
  struct batsup_sprite *upperv[4];
  int upperc=0;
  for (otherp=sprite->world->spritev,i=sprite->world->spritec;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other==ignore) continue;
    if (!other->solid) continue;
    if (other->id<SPRID_BRICK0) continue;
    if (other->y<above) continue;
    if (other->y>below) continue;
    if (other->x<ledge) continue;
    if (other->x>redge) continue;
    upperv[upperc++]=other;
    if (upperc>=4) break;
  }
  if (upperc<1) return 0; // Overhead not blocked.
  qsort(upperv,upperc,sizeof(void*),cmpx);
  if (upperv[0]->x-ledge-1.0>=1.125) return 0;
  if (redge-upperv[upperc-1]->x-1.0>=1.125) return 0;
  for (i=1;i<upperc;i++) {
    struct batsup_sprite *a=upperv[i-1];
    struct batsup_sprite *b=upperv[i];
    double dx=b->x-a->x-1.0;
    if (dx>=1.125) return 0; // There's an ample gap, quit complaining.
  }
  
  return 1;
}

static void player_drop_pumpkin(struct batsup_sprite *sprite) {
  /* The sprites just trade places.
   * But we do still need to check new position, and may reject, because pumpkins are not solid while carried.
   */
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  struct batsup_sprite *pumpkin=batsup_sprite_by_id(sprite->world,SPRITE->pumpkinid);
  if (pumpkin) {
    double ybottom=sprite->y;
    double ytop=pumpkin->y;
    sprite->y=ytop;
    if (!batsup_sprite_move(sprite,0.0,0.0)) {
      /* When dropping is impossible, check whether there is some position where it could be done.
       * If not, kill the pumpkin. We're trapped.
       * With this mitigation, it's probably still possible to get trapped but it's hard to do.
       * If a determined player gets himself trapped, bravo for him, let him wait for timeout.
       */
      sprite->y=ybottom;
      if (player_has_no_pumpkin_space(sprite,pumpkin)) {
        pumpkin->defunct=1;
      } else {
        if (SPRITE->inputp) bm_sound_pan(RID_sound_reject,(sprite->id==SPRID_RPLAYER)?0.250:-0.250);
        return;
      }
    }
    SPRITE->pumpkinid=0;
    bm_sound_pan(RID_sound_bombdrop,(sprite->id==SPRID_RPLAYER)?0.250:-0.250);
    pumpkin->y=ybottom;
    brick_set_carried(pumpkin,0);
  }
}

static void player_pickup_pumpkin(struct batsup_sprite *sprite) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  if (SPRITE->pumpkinid) return; // oy!
  
  /* If he's holding up or down, check above or below.
   * First dead center, then toewise, then heelwise.
   * No vertical modifier, check in front, and if nothing there, below.
   */
  struct batsup_sprite *pumpkin=0;
  double frontx=sprite->x,toex=sprite->x,heelx=sprite->x;
  if (sprite->xform&EGG_XFORM_XREV) {
    frontx-=1.0;
    toex-=0.5;
    heelx+=0.5;
  } else {
    frontx+=1.0;
    toex+=0.5;
    heelx-=0.5;
  }
  if (SPRITE->indy<0) {
    if (!(pumpkin=building_find_brick(sprite->world,sprite->x,sprite->y-1.0))) {
      if (!(pumpkin=building_find_brick(sprite->world,toex,sprite->y-1.0))) {
        pumpkin=building_find_brick(sprite->world,heelx,sprite->y-1.0);
      }
    }
  } else {
    if (!SPRITE->indy) { // Neutral vertical: Try forward first.
      pumpkin=building_find_brick(sprite->world,frontx,sprite->y);
    }
    if (!pumpkin) { // Down, or neutral and nothing there: Check below.
      if (!(pumpkin=building_find_brick(sprite->world,sprite->x,sprite->y+1.0))) {
        if (!(pumpkin=building_find_brick(sprite->world,toex,sprite->y+1.0))) {
          pumpkin=building_find_brick(sprite->world,heelx,sprite->y+1.0);
        }
      }
    }
  }
  if (!pumpkin) {
    bm_sound_pan(RID_sound_reject,(sprite->id==SPRID_RPLAYER)?0.250:-0.250);
    return;
  }
  
  /* A pumpkin candidate exists.
   * Initially we were rejecting if there's no room overhead, but no need for that: Pumpkins are non-solid while carried.
   */
  double x0=pumpkin->x;
  double y0=pumpkin->y;
  pumpkin->x=sprite->x;
  pumpkin->y=sprite->y-1.0;
  
  // Got one!
  SPRITE->pumpkinid=pumpkin->id;
  brick_set_carried(pumpkin,1);
  bm_sound_pan(RID_sound_mount,(sprite->id==SPRID_RPLAYER)?0.250:-0.250);
}

static void player_update_common(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  struct battle *battle=sprite->world->battle;
  
  /* If no bell has been rung, check it.
   * We can only ring our own bell, and only the first to reach it rings.
   * (this means a very slight asymmetry: Left player wins ties, within one frame)
   */
  if (battle->outcome==-2) {
    int bellid=(sprite->id==SPRID_LPLAYER)?SPRID_LBELL:SPRID_RBELL;
    struct batsup_sprite *bell=batsup_sprite_by_id(sprite->world,bellid);
    if (bell) {
      const double radius=0.500;
      double dx=sprite->x-bell->x;
      if ((dx>=-radius)&&(dx<=radius)) {
        double dy=sprite->y-bell->y;
        if ((dy>=-radius)&&(dy<=radius)) {
          battle->outcome=(sprite->id==SPRID_RPLAYER)?-1:1;
          bell_ring(bell);
        }
      }
    }
  }
  
  /* Walk.
   */
  if (SPRITE->indx) {
    batsup_sprite_move(sprite,SPRITE->indx*SPRITE->walkspeed*elapsed,0.0);
    sprite->xform=(SPRITE->indx<0)?EGG_XFORM_XREV:0;
  }
  
  /* Pick up or drop.
   */
  if (SPRITE->inpickup) {
    SPRITE->inpickup=0;
    if (SPRITE->pumpkinid) {
      player_drop_pumpkin(sprite);
    } else {
      player_pickup_pumpkin(sprite);
    }
  }
  
  /* Gravity or jump.
   */
  int apply_gravity=0;
  if (SPRITE->injump) {
    if (SPRITE->seated&&!SPRITE->pvinjump) {
      bm_sound_pan(RID_sound_jump,(sprite->id==SPRID_RPLAYER)?0.250:-0.250);
      SPRITE->jumpclock=0.350; // TODO scale per skill?
      SPRITE->gravity=0.0;
      SPRITE->seated=0;
      batsup_sprite_move(sprite,0.0,player_jump_dy(sprite)*elapsed);
    } else if (SPRITE->jumpclock>0.0) {
      SPRITE->jumpclock-=elapsed;
      batsup_sprite_move(sprite,0.0,player_jump_dy(sprite)*elapsed);
    } else {
      apply_gravity=1;
    }
  } else {
    apply_gravity=1;
  }
  SPRITE->pvinjump=SPRITE->injump;
  if (apply_gravity) {
    if ((SPRITE->gravity+=GRAVITY_ACCEL*elapsed)>GRAVITY_LIMIT) SPRITE->gravity=GRAVITY_LIMIT;
    double dy=SPRITE->gravity*elapsed;
    if (batsup_sprite_move(sprite,0.0,dy)) {
      SPRITE->seated=0;
    } else if (!SPRITE->seated) {
      SPRITE->seated=1;
      SPRITE->gravity=0.0;
    } else {
      SPRITE->gravity=0.0;
    }
  }
  
  /* If we have a pumpkin, move it.
   */
  if (SPRITE->pumpkinid) {
    struct batsup_sprite *pumpkin=batsup_sprite_by_id(sprite->world,SPRITE->pumpkinid);
    if (!pumpkin) { // oops?
      SPRITE->pumpkinid=0;
    } else {
      pumpkin->x=sprite->x;
      pumpkin->y=sprite->y-1.0;
    }
  }
  
  /* Animate and set appropriate face.
   */
  sprite->tileid=SPRITE->tileid0;
  if (SPRITE->pumpkinid) sprite->tileid+=3;
  if (SPRITE->indx) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.150;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    }
    switch (SPRITE->animframe) {
      case 1: sprite->tileid+=1; break;
      case 3: sprite->tileid+=2; break;
    }
  } else {
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
  }
}

/* Nonzero if there's a solid sprite above this one and close.
 */
 
static int near_overhead(struct batsup_sprite *sprite) {
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other->id<SPRID_BRICK0) continue;
    if (!other->solid) continue;
    if (other->y>=sprite->y) continue;
    double dx=other->x-sprite->x;
    if (dx<-1.0) continue;
    if (dx>1.0) continue;
    double dy=other->y-sprite->y;
    if (dy<-1.5) continue; // Far enough up, don't worry about it.
    return 1;
  }
  return 0;
}

/* Update cpu player.
 * This is a hairy job any way you slice it.
 * Let's impose a few constraints to keep it manageable:
 *  - No magic. We set the input state like humans, and don't touch any other state.
 *  - Stack bricks in a staggered pattern directly under the bell. One a bit left, one a bit right, and repeat.
 *  - When we need to fetch a brick, fetch the nearest.
 */
 
static void cpu_update(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  
  /* Record my previous input state in case we need it, and preemptively zero everything.
   * At any point during this update, we can just "DONE" and terminate with null input.
   */
  int pvdx=SPRITE->indx; SPRITE->indx=0;
  int pvdy=SPRITE->indy; SPRITE->indy=0;
  int pvjump=SPRITE->injump; SPRITE->injump=0;
  int pvpickup=SPRITE->inpickup; SPRITE->inpickup=0;
  #define DONE { player_update_common(sprite,elapsed); return; }
  
  /* If the game is over, do nothing, let it stay idle.
   * We do still want gravity to run.
   */
  if (sprite->world->battle->outcome>-2) DONE
  
  /* Collect all the interesting state from world.
   * It's overkill to collect all this from scratch every time, but I think it's OK.
   * And if we cached more aggressively, I bet we would miss cases where it needs to refresh.
   */
  int stacksize=0;
  struct batsup_sprite *stacktop=0;
  struct batsup_sprite *nearest=0;
  double nearestd2;
  struct batsup_sprite *bell=batsup_sprite_by_id(sprite->world,(sprite->id==SPRID_RPLAYER)?SPRID_RBELL:SPRID_LBELL);
  if (!bell) DONE
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other->defunct) continue;
    if (other->id<SPRID_BRICK0) continue;
    if (!other->solid) continue; // Being carried, either by me or by the other guy. Either way, not interesting.
    double dx=other->x-bell->x;
    if ((dx>-1.0)&&(dx<1.0)) { // Under my bell. Must be part of my stack.
      stacksize++;
      if (!stacktop||(other->y<stacktop->y)) stacktop=other;
    } else { // Not under my bell. Record the nearest.
      dx=other->x-sprite->x; // nearest to *me*, that is
      double dy=other->y-sprite->y;
      double d2=dx*dx+dy*dy;
      if (!nearest||(d2<nearestd2)) {
        nearest=other;
        nearestd2=d2;
      }
    }
  }
  
  /* If I have a pumpkin, I need to reach the top of the stack and deposit it there.
   * Deposits must alternate left and right of the bell, just a little, to give us toeholds.
   */
  if (SPRITE->pumpkinid) {
    if (!stacktop||(sprite->y<stacktop->y)) { // Above the stack. Approach target and deposit.
      double targetl,targetr; // Ensure wide enough that we'll land inside it for a frame, even at top speed.
      double radius=0.300*(10-stacksize)/10.0;
      if (stacksize&1) {
        targetr=bell->x-radius;
        targetl=targetr-0.120;
      } else {
        targetl=bell->x+radius;
        targetr=targetl+0.120;
      }
      if (sprite->x<targetl) SPRITE->indx=1;
      else if (sprite->x>targetr) SPRITE->indx=-1;
      else {
        // ok one little cheat: force us to exactly the center of the target.
        sprite->x=(targetl+targetr)*0.5;
        SPRITE->inpickup=1;
      }
      if (SPRITE->seated) SPRITE->injump=0;
      else SPRITE->injump=pvjump;
      DONE
    }
    // Jump constantly, and hold each jump as long as possible.
    if (SPRITE->seated&&!pvjump) SPRITE->injump=1;
    else if (SPRITE->jumpclock>0.0) SPRITE->injump=1;
    /* Climb the tower.
     * If we're falling, move toward the bell always.
     * If we're rising, check for blockages overhead.
     */
    int toward=1;
    if ((SPRITE->gravity<=0.0)&&SPRITE->injump) {
      if (near_overhead(sprite)) toward=0;
    }
    double dx=bell->x-sprite->x;
    if (!toward) dx=-dx;
    if (dx<0.0) SPRITE->indx=-1;
    else if (dx>0.0) SPRITE->indx=1;
    DONE
  }
  
  /* No pumpkin. Walk toward the nearest.
   * If more than a meter horizontally, just approach.
   * Within a meter, check whether we're above it or in line vertically.
   * When it looks grabbable, grab it.
   */
  if (!nearest) DONE
  double dx=nearest->x-sprite->x;
  if (dx<-1.25) { SPRITE->indx=-1; DONE }
  if (dx>1.25) { SPRITE->indx=1; DONE }
  if (sprite->y<nearest->y-0.5) {
    if (dx<-0.125) { SPRITE->indx=-1; DONE }
    if (dx>0.125) { SPRITE->indx=1; DONE }
    SPRITE->indy=1;
    SPRITE->inpickup=1;
    DONE
  }
  SPRITE->inpickup=1;
  
  DONE
  #undef DONE
}

/* Update man player.
 */
 
static void man_update(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  if (sprite->world->battle->outcome>-2) {
    SPRITE->indx=0;
    SPRITE->indy=0;
    SPRITE->injump=0;
    SPRITE->inpickup=0;
  } else {
    switch (g.input[SPRITE->inputp]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
      case EGG_BTN_LEFT: SPRITE->indx=-1; break;
      case EGG_BTN_RIGHT: SPRITE->indx=1; break;
      default: SPRITE->indx=0;
    }
    switch (g.input[SPRITE->inputp]&(EGG_BTN_UP|EGG_BTN_DOWN)) {
      case EGG_BTN_UP: SPRITE->indy=-1; break;
      case EGG_BTN_DOWN: SPRITE->indy=1; break;
      default: SPRITE->indy=0;
    }
    if (SPRITE->injump_blackout) {
      if (!(g.input[SPRITE->inputp]&EGG_BTN_SOUTH)) SPRITE->injump_blackout=0;
    } else if (g.input[SPRITE->inputp]&EGG_BTN_SOUTH) SPRITE->injump=1;
    else SPRITE->injump=0;
    if ((g.input[SPRITE->inputp]&EGG_BTN_WEST)&&!(g.pvinput[SPRITE->inputp]&EGG_BTN_WEST)) SPRITE->inpickup=1;
    else SPRITE->inpickup=0;
  }
  player_update_common(sprite,elapsed);
}

/* Player sprites, top level.
 */
 
static struct batsup_sprite *player_spawn(struct battle *battle,int x,int id,double skill) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,id,sizeof(struct sprite_player));
  if (!sprite) return 0;
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  sprite->x=x+0.5;
  sprite->y=NS_sys_maph-1.5;
  if (x>=NS_sys_mapw>>1) sprite->xform=EGG_XFORM_XREV;
  sprite->solid=1;
  sprite->fullmeter=1;
  SPRITE->seated=1;
  SPRITE->injump_blackout=1;
  SPRITE->skill=skill;
  SPRITE->walkspeed=WALKSPEED_LO*(1.0-skill)+WALKSPEED_HI*skill;
  int face,ctl;
  if (id==SPRID_LPLAYER) {
    face=battle->args.lface;
    ctl=battle->args.lctl;
  } else {
    face=battle->args.rface;
    ctl=battle->args.rctl;
  }
  if (ctl) { // Human
    sprite->update=man_update;
    SPRITE->inputp=ctl;
  } else { // CPU
    sprite->update=cpu_update;
  }
  switch (face) {
    case NS_face_dot: sprite->tileid=0x30; break;
    case NS_face_princess: sprite->tileid=0x40; break;
    default: sprite->tileid=0x50; break;
  }
  SPRITE->tileid0=sprite->tileid;
  return sprite;
}

/* Battle global context.
 *******************************************************************************************/

/* Delete.
 */
 
static void _building_del(struct battle *battle) {
  batsup_world_del(BATTLE->world);
}

/* Generate map and sprites.
 */
 
static int building_generate(struct battle *battle) {

  /* Map is just a border and big blue field.
   */
  {
    uint8_t *dsta,*dstb;
    int i;
    for (
      dsta=BATTLE->world->map->v+1,
      dstb=BATTLE->world->map->v+NS_sys_mapw*(NS_sys_maph-1)+1,
      i=NS_sys_mapw-2;
      i-->0;
      dsta++,dstb++
    ) {
      *dsta=0x37;
      *dstb=0x57;
    }
    for (
      dsta=BATTLE->world->map->v+NS_sys_mapw,
      dstb=BATTLE->world->map->v+NS_sys_mapw*2-1,
      i=NS_sys_maph-2;
      i-->0;
      dsta+=NS_sys_mapw,
      dstb+=NS_sys_mapw
    ) {
      *dsta=0x46;
      *dstb=0x48;
    }
    for (
      dsta=BATTLE->world->map->v+NS_sys_mapw+1,
      i=NS_sys_maph-2;
      i-->0;
      dsta+=NS_sys_mapw
    ) {
      int xi=NS_sys_mapw-2;
      for (dstb=dsta;xi-->0;dstb++) {
        *dstb=0x47;
      }
    }
    BATTLE->world->map->v[0]=0x36;
    BATTLE->world->map->v[NS_sys_mapw-1]=0x38;
    BATTLE->world->map->v[NS_sys_mapw*(NS_sys_maph-1)]=0x56;
    BATTLE->world->map->v[NS_sys_mapw*NS_sys_maph-1]=0x58;
  }
  
  /* Spawn the four key sprites.
   */
  struct batsup_sprite *sprite;
  double lskill,rskill;
  battle_normalize_bias(&lskill,&rskill,battle);
  if (!bell_spawn(battle,3,SPRID_LBELL)) return -1;
  if (!bell_spawn(battle,NS_sys_mapw-4,SPRID_RBELL)) return -1;
  if (!player_spawn(battle,3,SPRID_LPLAYER,lskill)) return -1;
  if (!player_spawn(battle,NS_sys_mapw-4,SPRID_RPLAYER,rskill)) return -1;
  
  /* Spawn a bunch of bricks in the middle.
   */
  int id=SPRID_BRICK0;
  if (!brick_spawn(battle, 6,10,id++)) return -1;
  if (!brick_spawn(battle, 7,10,id++)) return -1;
  if (!brick_spawn(battle, 8,10,id++)) return -1;
  if (!brick_spawn(battle, 9,10,id++)) return -1;
  if (!brick_spawn(battle,10,10,id++)) return -1;
  if (!brick_spawn(battle,11,10,id++)) return -1;
  if (!brick_spawn(battle,12,10,id++)) return -1;
  if (!brick_spawn(battle,13,10,id++)) return -1;
  if (!brick_spawn(battle, 6, 9,id++)) return -1;
  if (!brick_spawn(battle, 7, 9,id++)) return -1;
  if (!brick_spawn(battle, 8, 9,id++)) return -1;
  if (!brick_spawn(battle, 9, 9,id++)) return -1;
  if (!brick_spawn(battle,10, 9,id++)) return -1;
  if (!brick_spawn(battle,11, 9,id++)) return -1;
  if (!brick_spawn(battle,12, 9,id++)) return -1;
  if (!brick_spawn(battle,13, 9,id++)) return -1;
  if (!brick_spawn(battle, 7, 8,id++)) return -1;
  if (!brick_spawn(battle, 8, 8,id++)) return -1;
  if (!brick_spawn(battle, 9, 8,id++)) return -1;
  if (!brick_spawn(battle,10, 8,id++)) return -1;
  if (!brick_spawn(battle,11, 8,id++)) return -1;
  if (!brick_spawn(battle,12, 8,id++)) return -1;
  if (!brick_spawn(battle, 8, 7,id++)) return -1;
  if (!brick_spawn(battle, 9, 7,id++)) return -1;
  if (!brick_spawn(battle,10, 7,id++)) return -1;
  if (!brick_spawn(battle,11, 7,id++)) return -1;

  return 0;
}

/* New.
 */
 
static int _building_init(struct battle *battle) {
  if (!(BATTLE->world=batsup_world_new(battle,0))) return -1;
  batsup_world_set_image(BATTLE->world,RID_image_battle_fractia2);
  if (building_generate(battle)<0) return -1;
  return 0;
}

/* Update.
 */
 
static void _building_update(struct battle *battle,double elapsed) {
  batsup_world_update(BATTLE->world,elapsed);
}

/* Render.
 */
 
static void _building_render(struct battle *battle) {
  batsup_world_render(BATTLE->world);
}

/* Type definition.
 */
 
const struct battle_type battle_type_building={
  .name="building",
  .objlen=sizeof(struct battle_building),
  .id=NS_battle_building,
  .strix_name=170,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_building_del,
  .init=_building_init,
  .update=_building_update,
  .render=_building_render,
};

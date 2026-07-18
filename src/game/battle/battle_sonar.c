/* battle_sonar.c
 */

#include "game/bellacopia.h"

/* Column and row refer to the spaces between walls.
 * Walls are the infinitely thin space between columns and rows, where physics happens.
 * Walls can live at both fenceposts, so their grid is (COLC+1,ROWC+1).
 */
#define COLC 9
#define ROWC 7
#define CELLSIZE 24

#define WALL_LIMIT 128 /* I believe the worst-case scenario approaches 1 wall per cell (picturing 2x2 cells where one of them has four walls and the rest none). */
#define LUM_LIMIT 256
#define SCREECH_LIMIT 256

struct battle_sonar {
  struct battle hdr;
  double goalx,goaly;
  
  /* Walls are constant after construction.
   * They all align to an axis and run as long as possible.
   */
  struct wall {
    int ax,ay,bx,by; // Endpoints in meters. Must align to an axis, ie ax==bx or ay==by.
  } wallv[WALL_LIMIT];
  int wallc;
  
  /* One lum is a tiny short-lived illuminated segment of wall.
   * We'll project them to output space from the start.
   */
  struct lum {
    int ax,ay,bx,by;
    double bright; // 0..1, counts down
    double dbright; // hz, captured from the player. Negative.
  } lumv[LUM_LIMIT];
  int lumc;
  
  /* Paths to the goal, for CPU's use.
   * Each entry is a cell index. So (v/COLC) is the row and (v%COLC) the column.
   * (l) starts at (0,0) and (r) starts at (COLC-1,ROWC-1).
   */
  uint8_t lpath[COLC*ROWC];
  uint8_t rpath[COLC*ROWC];
  int lpathc,rpathc;
  
  /* One screech is a ray emanating from a player.
   * Each player screech creates multiple screech objects, travelling in different directions.
   * At first I thought we'd do a continuous version of this, where the screech starts as a circle, but that way lies madness.
   */
  struct screech {
    double x,y;
    double dx,dy;
    double dbright; // Pass along to luma.
    double size,bright;
  } screechv[SCREECH_LIMIT];
  int screechc;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double walkspeed; // m/s
    double dbright; // hz. How fast do my screech lums fade out. Must be negative.
    double sos; // m/s Speed Of Sound
    double radius; // m
    double radius2;
    int screechc; // How many rays at each screech.
    double screechtime; // s, pause after each screech.
    
    double x,y; // Position in world meters.
    int pvinscreech;
    uint8_t xform;
    double animclock;
    int animframe;
    double screechclock;
    int done;
    
    // Controllers set:
    int indx,indy,inscreech;
    
    // Humans:
    int blackout;
    
    // CPU:
    const uint8_t *path; // lpath or rpath
    int pathc,pathp;
    double targetx,targety;
    double delaymin,delaymax; // How long between screeches.
    double delayclock;
    
  } playerv[2];
};

#define BATTLE ((struct battle_sonar*)battle)

/* Delete.
 */
 
static void _sonar_del(struct battle *battle) {
  egg_play_song(3,0,0,0.0,0.0);
}

/* Init player.
 */
 
static int player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->xform=0;
    player->x=0.5;
    player->y=0.5;
    player->path=BATTLE->lpath;
    player->pathc=BATTLE->lpathc;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->x=COLC-0.5;
    player->y=ROWC-0.5;
    player->path=BATTLE->rpath;
    player->pathc=BATTLE->rpathc;
  }
  
  player->walkspeed=3.0*(1.0-player->skill)+5.0*player->skill;
  player->dbright=-0.500;
  player->sos=12.0;
  player->screechc=30;
  player->screechtime=0.900*(1.0-player->skill)+0.400*player->skill;
  player->radius=9.0/(double)CELLSIZE; // Radius actually larger than the tile, so we don't cover walls.
  player->radius2=player->radius*player->radius;
  
  if (player->human=human) {
    player->blackout=1;
  } else {
    player->walkspeed*=0.500; // Heavy CPU penalty.
    player->delaymin=0.500*(1.0-player->skill)+2.000*player->skill;
    player->delaymax=player->delaymin*2.0;
    player->delayclock=player->delaymin;
    player->targetx=player->x;
    player->targety=player->y;
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0x805f38ff;
        player->tileid=0x50;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x30;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x40;
      } break;
  }
  return 0;
}

/* Add a wall.
 */
 
static struct wall *sonar_add_wall(struct battle *battle,int ax,int ay,int bx,int by) {
  if (BATTLE->wallc>=WALL_LIMIT) return 0;
  struct wall *wall=BATTLE->wallv+BATTLE->wallc++;
  wall->ax=ax;
  wall->ay=ay;
  wall->bx=bx;
  wall->by=by;
  return wall;
}

/* Randomly break down a wall into an untouched neighbor, then do it again and again.
 * Stops when no untouched neighbors exist: Either there aren't any zeroes, or we've painted ourselves into a corner.
 */
 
static void sonar_drunk_walk(struct battle *battle,uint8_t *cellv,int x,int y) {
  int openc=0,x0=x,y0=y;
  for (;;) {
    if ((x<0)||(y<0)||(x>=COLC)||(y>=ROWC)) break;
    uint8_t *cellp=cellv+y*COLC+x;
    // Candidates are those where a wall currently exists, and the cell on the other side is untouched.
    struct candidate {
      int x,y;
      uint8_t srcmask,dstmask;
    } candidatev[4];
    int candidatec=0;
    if ((x>0)&&!(cellp[0]&0x10)&&!cellp[-1]) candidatev[candidatec++]=(struct candidate){x-1,y,0x10,0x08};
    if ((x<COLC-1)&&!(cellp[0]&0x08)&&!cellp[1]) candidatev[candidatec++]=(struct candidate){x+1,y,0x08,0x10};
    if ((y>0)&&!(cellp[0]&0x40)&&!cellp[-COLC]) candidatev[candidatec++]=(struct candidate){x,y-1,0x40,0x02};
    if ((y<ROWC-1)&&!(cellp[0]&0x02)&&!cellp[COLC]) candidatev[candidatec++]=(struct candidate){x,y+1,0x02,0x40};
    if (!candidatec) break; // Can't advance from here.
    const struct candidate *dst=candidatev+(rand()%candidatec);
    cellp[0]|=dst->srcmask;
    cellv[dst->y*COLC+dst->x]|=dst->dstmask;
    x=dst->x;
    y=dst->y;
    openc++;
  }
}

/* Find the optimal path between two points in a grid of cardinal connections.
 */
 
static int sonar_find_path(uint8_t *dst,int dstc,int dsta,struct battle *battle,uint8_t *cellv,int ax,int ay,int bx,int by) {

  // Confirm we have headroom, both points are in bounds, and (a) has not been visited yet.
  if ((dstc>=dsta)||(ax<0)||(ay<0)||(bx<0)||(by<0)||(ax>=COLC)||(ay>=ROWC)||(bx>=COLC)||(by>=ROWC)) return 999;
  int p=ay*COLC+ax;
  int i=dstc;
  while (i-->0) if (dst[i]==p) return 999;
  
  // Push (a) on the list. If (a==b) we're done, report it.
  dst[dstc++]=p;
  if ((ax==bx)&&(ay==by)) return dstc;
  
  /* Recur for each of the four directions if our wall that way is open.
   * There should be exactly one that returns a valid count, not 999.
   * As soon as we find it, return.
   */
  int subc;
  if ((subc=(cellv[p]&0x40)?sonar_find_path(dst,dstc,dsta,battle,cellv,ax,ay-1,bx,by):999)<999) return subc;
  if ((subc=(cellv[p]&0x10)?sonar_find_path(dst,dstc,dsta,battle,cellv,ax-1,ay,bx,by):999)<999) return subc;
  if ((subc=(cellv[p]&0x08)?sonar_find_path(dst,dstc,dsta,battle,cellv,ax+1,ay,bx,by):999)<999) return subc;
  if ((subc=(cellv[p]&0x02)?sonar_find_path(dst,dstc,dsta,battle,cellv,ax,ay+1,bx,by):999)<999) return subc;
  
  return 999;
}

/* Generate a random maze.
 * Populates (wallv).
 * The algorithm seems solid. For an 18x10 grid, usually 20 passes or so, and 80..90 walls.
 */
 
static int sonar_generate_maze(struct battle *battle) {
  int x,y,i;
  uint8_t *cellp;
  BATTLE->wallc=0;
  
  /* First a wall on all four edges, no controversy over that.
   */
  sonar_add_wall(battle,0,0,COLC,0);
  sonar_add_wall(battle,0,ROWC,COLC,ROWC);
  sonar_add_wall(battle,0,0,0,ROWC);
  sonar_add_wall(battle,COLC,0,COLC,ROWC);
  
  /* Then make a grid of interior cells.
   * Each cell is a bitmap of connections: 0x40,0x10,0x08,0x02 = N,W,E,S as usual.
   * Initially zero ie nothing connected.
   */
  uint8_t cellv[COLC*ROWC]={0};
  
  /* Drunk-walk from the center of the grid, breaking down walls to an untouched cell, until we inevitably paint ourselves into a corner.
   */
  sonar_drunk_walk(battle,cellv,COLC>>1,ROWC>>1);
  int opc=1;
  
  /* Now, in a loop, count the nonzero cells.
   * Anything nonzero is connected to the initial drunk-walk.
   * Anything nonzero with a zero neighbor is a candidate for the next drunk-walk.
   * When no zeroes remain, we're done.
   * This algorithm produces a perfect maze: There is exactly one path between any two cells.
   * (Contrast to the Labyrinth, which is deliberately leaky and loopy).
   */
  for (;;) {
    struct candidate { uint8_t x,y; } candidatev[COLC*ROWC]; // Nonzero cells with zero neighbors.
    int candidatec=0;
    int zeroc=0;
    for (cellp=cellv,y=0;y<ROWC;y++) {
      for (x=0;x<COLC;x++,cellp++) {
        if (!*cellp) {
          zeroc++;
        } else {
          if (
            ((x>0)&&!cellp[-1])||
            ((x<COLC-1)&&!cellp[1])||
            ((y>0)&&!cellp[-COLC])||
            ((y<ROWC-1)&&!cellp[COLC])
          ) {
            candidatev[candidatec++]=(struct candidate){x,y};
          }
        }
      }
    }
    if (!zeroc) break; // All cells connected, great!
    if (!candidatec) return -1; // Can't be possible. A zero and a nonzero exists, so there must be a zero with a nonzero neighbor.
    int candidatep=rand()%candidatec;
    sonar_drunk_walk(battle,cellv,candidatev[candidatep].x,candidatev[candidatep].y);
    opc++;
  }
  
  /* With the maze in grid form, find ideal paths for both players.
   * Humans won't use it, but we do need both.
   * If the path lengths disagree with the bias, rotate the grid a half-turn.
   * At 0x80, the typical one-player case, the shorter side goes left.
   */
  BATTLE->lpathc=sonar_find_path(BATTLE->lpath,0,COLC*ROWC,battle,cellv,0,0,COLC>>1,ROWC>>1);
  BATTLE->rpathc=sonar_find_path(BATTLE->rpath,0,COLC*ROWC,battle,cellv,COLC-1,ROWC-1,COLC>>1,ROWC>>1);
  if (BATTLE->lpathc>COLC*ROWC) BATTLE->lpathc=0;
  if (BATTLE->rpathc>COLC*ROWC) BATTLE->rpathc=0;
  if (BATTLE->lpathc&&BATTLE->rpathc&&(BATTLE->lpathc!=BATTLE->rpathc)) {
    int ok;
    if (battle->args.bias<=0x80) ok=(BATTLE->lpathc<BATTLE->rpathc); // Favor left player.
    else ok=(BATTLE->lpathc>BATTLE->rpathc); // Favor right player.
    if (!ok) {
      int ap=0,bp=COLC*ROWC-1;
      for (;ap<bp;ap++,bp--) {
        uint8_t tmp=cellv[ap];
        cellv[ap]=cellv[bp];
        cellv[bp]=tmp;
      }
      // And of course this means our cell values need to be reversed too.
      for (cellp=cellv,i=COLC*ROWC;i-->0;cellp++) {
        *cellp=(
          (((*cellp)&0x40)?0x02:0)|
          (((*cellp)&0x10)?0x08:0)|
          (((*cellp)&0x08)?0x10:0)|
          (((*cellp)&0x02)?0x40:0)
        );
      }
      for (i=(BATTLE->lpathc>BATTLE->rpathc)?BATTLE->lpathc:BATTLE->rpathc;i-->0;) {
        uint8_t tmp=BATTLE->lpath[i];
        BATTLE->lpath[i]=BATTLE->rpath[i];
        BATTLE->rpath[i]=tmp;
        BATTLE->lpath[i]=COLC*ROWC-1-BATTLE->lpath[i];
        BATTLE->rpath[i]=COLC*ROWC-1-BATTLE->rpath[i];
      }
      int tmp=BATTLE->lpathc;
      BATTLE->lpathc=BATTLE->rpathc;
      BATTLE->rpathc=tmp;
    }
  }
  
  /* (cellv) now describes a fully connected grid.
   * Iterate the fenceposts and create walls.
   * Walls should be as long as possible, not single meters every time.
   * Bear in mind we already made the outer walls.
   */
  for (y=1;y<ROWC;y++) {
    cellp=cellv+y*COLC; // Look at the lower cell, at bit 0x40.
    for (x=0;x<COLC;) {
      if ((*cellp)&0x40) { // Connected, no wall. Skip column.
        x++;
        cellp++;
      } else { // Start a wall here.
        int lx=x;
        do {
          x++;
          cellp++;
        } while ((x<COLC)&&(!((*cellp)&0x40)));
        if (!sonar_add_wall(battle,lx,y,x,y)) return -1;
      }
    }
  }
  for (x=1;x<COLC;x++) {
    cellp=cellv+x; // Look at the right cell, at bit 0x10.
    for (y=0;y<ROWC;) {
      if ((*cellp)&0x10) { // Connected, no wall. Skip row.
        y++;
        cellp+=COLC;
      } else { // Start a wall here.
        int ty=y;
        do {
          y++;
          cellp+=COLC;
        } while ((y<ROWC)&&(!((*cellp)&0x10)));
        if (!sonar_add_wall(battle,x,ty,x,y)) return -1;
      }
    }
  }
  
  return 0;
}

/* New.
 */
 
static int _sonar_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  sonar_generate_maze(battle);
  BATTLE->goalx=COLC*0.5;
  BATTLE->goaly=ROWC*0.5;
  if (player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface)<0) return -1;
  if (player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface)<0) return -1;
  egg_play_song(3,RID_song_sonar,1,0.666,0.0);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->inscreech=(input&EGG_BTN_SOUTH)?1:0;
  }
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0; break;
  }
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player->indy=-1; break;
    case EGG_BTN_DOWN: player->indy=1; break;
    default: player->indy=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If we're screeching, just sit tight.
   */
  if (player->screechclock>0.0) {
    player->indx=player->indy=player->inscreech=0;
    return;
  }

  /* Stop to screech now and then.
   * We don't need it. But it is a nice decoration, and also it slows the CPU down.
   */
  if ((player->delayclock-=elapsed)<=0.0) {
    player->inscreech=1;
    double n=((rand()&0xffff)/65535.0);
    player->delayclock=player->delaymin*(1.0-n)+player->delaymax*n;
    return;
  }
  player->inscreech=0;

  /* How far to my target?
   * Engage the dpad.
   */
  const double thresh=0.200;
  double dx=player->targetx-player->x;
  double dy=player->targety-player->y;
  if (dx<-thresh) player->indx=-1; else if (dx>thresh) player->indx=1; else player->indx=0;
  if (dy<-thresh) player->indy=-1; else if (dy>thresh) player->indy=1; else player->indy=0;
  
  /* If we decided on a neutral dpad, look up the next entry in our path.
   */
  if (!player->indx&&!player->indy&&(player->pathp<player->pathc)) {
    int cellp=player->path[player->pathp++];
    player->targetx=(cellp%COLC)+0.5;
    player->targety=(cellp/COLC)+0.5;
  }
}

/* Move player by the minimum amount to escape a collision at (x,y).
 */
 
static void player_rectify_point(struct battle *battle,struct player *player,double x,double y) {
  double dx=player->x-x;
  double dy=player->y-y;
  double d2=dx*dx+dy*dy;
  if (d2>player->radius2) return; // No collision.
  double distance=sqrt(d2);
  if (distance<=0.001) return; // Too close, don't touch it.
  double pen=player->radius-distance;
  if (pen<=-0.0) return; // No collision again. Maybe rounding errors in play?
  double nx=dx/distance;
  double ny=dy/distance;
  player->x+=nx*pen;
  player->y+=ny*pen;
}

/* Check for wall collisions and nudge player away from them.
 * We're playing a little loose: Collision detection is not away of the player's direction of travel.
 * No collisions against the other player; they must be able to walk past each other.
 * Since walls are always axis-aligned, we can cheat it pretty hard.
 */
 
static void player_rectify_position(struct battle *battle,struct player *player) {
  const struct wall *wall=BATTLE->wallv;
  int i=BATTLE->wallc;
  for (;i-->0;wall++) {
    /* The general approach would be to take scalar projection and scalar rejection, yadda yadda.
     * That would of course work here, but since our walls are always axis-aligned, we can make it much simpler.
     * First check rejection. Further than radius, great, no collision.
     * Then if the projection is in 0..1, we have the simple axis-aligned case.
     * If projection is in -radius..0 or 1..1+radius, we have the more interesting point case.
     */
    if (wall->ax==wall->bx) { // Vertical wall.
      double dx=player->x-wall->ax;
      if ((dx>=player->radius)||(dx<=-player->radius)) continue;
      double len=wall->by-wall->ay;
      double proj=player->y-wall->ay;
      if ((proj>=0.0)&&(proj<=len)) { // Square case.
        if (dx>0.0) player->x=wall->ax+player->radius; // Correct to the right.
        else player->x=wall->ax-player->radius; // Correct to the left.
      } else if (proj>-player->radius) { // Point case against A.
        player_rectify_point(battle,player,wall->ax,wall->ay);
      } else if (proj<1.0+player->radius) { // Point case against B.
        player_rectify_point(battle,player,wall->bx,wall->by);
      }
    } else if (wall->ay==wall->by) { // Horizontal wall.
      double dy=player->y-wall->ay;
      if ((dy>=player->radius)||(dy<=-player->radius)) continue;
      double len=wall->bx-wall->ax;
      double proj=player->x-wall->ax;
      if ((proj>=0.0)&&(proj<=len)) { // Square case.
        if (dy>0.0) player->y=wall->ay+player->radius; // Correct downward.
        else player->y=wall->ay-player->radius; // Correct upward.
      } else if (proj>-player->radius) { // Point case against A.
        player_rectify_point(battle,player,wall->ax,wall->ay);
      } else if (proj<1.0+player->radius) { // Point case against B.
        player_rectify_point(battle,player,wall->bx,wall->by);
      }
    }
  }
}

/* Add a new screech.
 */
 
static struct screech *sonar_add_screeches(struct battle *battle,struct player *player) {
  if (player->screechc<1) return 0;
  if (BATTLE->screechc>SCREECH_LIMIT-player->screechc) return 0;
  struct screech *screech=BATTLE->screechv+BATTLE->screechc;
  struct screech *rtn=screech;
  BATTLE->screechc+=player->screechc;
  int i=player->screechc;
  double t=0.0;
  double dt=(M_PI*2.0)/player->screechc;
  for (;i-->0;screech++,t+=dt) {
    screech->x=player->x;
    screech->y=player->y;
    screech->dbright=player->dbright;
    screech->dy=sin(t)*player->sos;
    screech->dx=-cos(t)*player->sos;
    screech->size=0.250;
    screech->bright=0.750;
  }
  return rtn;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* If a screech is paying out, force dpad to neutral.
   */
  if (player->screechclock>0.0) {
    player->screechclock-=elapsed;
    player->indx=player->indy=0;
  }

  /* Screech?
   */
  if (player->inscreech!=player->pvinscreech) {
    if (player->pvinscreech=player->inscreech) {
      if (sonar_add_screeches(battle,player)) {
        player->screechclock=player->screechtime;
        player->indx=player->indy=0;
        uint8_t noteid=0x58+rand()%10;
        egg_song_event_note_once(3,player->who,noteid,0x40,0);
      }
    }
  }
  
  /* Move?
   * Keep the velocities low enough and radii high enough that a player colliding with the wall will always have her center on the correct side.
   * Saves us a ton of touble here; we can just move optimistically, then rectify.
   */
  if (player->indx||player->indy) {
    if (player->indx<0) player->xform=EGG_XFORM_XREV;
    else if (player->indx>0) player->xform=0;
    player->x+=player->indx*player->walkspeed*elapsed;
    player->y+=player->indy*player->walkspeed*elapsed;
    player_rectify_position(battle,player);
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.200;
      if (++(player->animframe)>=4) player->animframe=0;
    }
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
  
  /* Reached goal?
   */
  if (!player->done) {
    double dx=BATTLE->goalx-player->x;
    double dy=BATTLE->goaly-player->y;
    double thresh=0.333;
    if ((dx>-thresh)&&(dx<thresh)&&(dy>-thresh)&&(dy<thresh)) {
      player->done=1;
    }
  }
}

/* Add lum.
 */
 
static struct lum *sonar_add_lum(struct battle *battle,double ax,double ay,double bx,double by,double dbright) {
  if (BATTLE->lumc>=LUM_LIMIT) return 0;
  struct lum *lum=BATTLE->lumv+BATTLE->lumc++;
  lum->bright=0.500;
  lum->dbright=dbright;
  int fldx=(FBW>>1)-((COLC*CELLSIZE)>>1);
  int fldy=(FBH>>1)-((ROWC*CELLSIZE)>>1);
  lum->ax=fldx+(int)(ax*CELLSIZE);
  lum->ay=fldy+(int)(ay*CELLSIZE);
  lum->bx=fldx+(int)(bx*CELLSIZE);
  lum->by=fldy+(int)(by*CELLSIZE);
  return lum;
}

/* Update screech.
 * Return nonzero to proceed or zero if defunct.
 */
 
static int screech_update(struct battle *battle,struct screech *screech,double elapsed) {

  screech->size+=elapsed;
  screech->bright-=2.000*elapsed;
  if (screech->bright<=0.0) return 0;

  /* Travel.
   * If we go an unreasonable distance outside the map, kill it. Shouldn't happen, there should be walls everywhere.
   */
  double pvx=screech->x,pvy=screech->y;
  screech->x+=screech->dx*elapsed;
  screech->y+=screech->dy*elapsed;
  if ((screech->x<-1.0)||(screech->y<-1.0)||(screech->x>COLC+1.0)||(screech->y>ROWC+1.0)) return 0;
  
  /* Check for wall collisions.
   * If we collided, kill me and replace with a lum aligned to the wall.
   * Like player collisions, we'll exploit the fact that walls are always axis-aligned.
   */
  double halflen=screech->size;
  const struct wall *wall=BATTLE->wallv;
  int i=BATTLE->wallc;
  for (;i-->0;wall++) {
    if (wall->ax==wall->bx) { // Vertical wall.
      if ((screech->y<wall->ay)&&(pvy<wall->ay)) continue;
      if ((screech->y>wall->by)&&(pvy>wall->by)) continue;
      if (
        ((screech->x<=wall->ax)&&(pvx>=wall->ax))||
        ((screech->x>=wall->ax)&&(pvx<=wall->ax))
      ) {
        double ay=screech->y-halflen;
        if (ay<wall->ay) ay=wall->ay;
        double by=screech->y+halflen;
        if (by>wall->by) by=wall->by;
        sonar_add_lum(battle,wall->ax,ay,wall->bx,by,screech->dbright);
        return 0;
      }
      
    } else if (wall->ay==wall->by) { // Horizontal wall.
      if ((screech->x<wall->ax)&&(pvx<wall->ax)) continue;
      if ((screech->x>wall->bx)&&(pvx>wall->bx)) continue;
      if (
        ((screech->y<=wall->ay)&&(pvy>=wall->ay))||
        ((screech->y>=wall->ay)&&(pvy<=wall->ay))
      ) {
        double ax=screech->x-halflen;
        if (ax<wall->ax) ax=wall->ax;
        double bx=screech->x+halflen;
        if (bx>wall->bx) bx=wall->bx;
        sonar_add_lum(battle,ax,wall->ay,bx,wall->by,screech->dbright);
        return 0;
      }
    }
  }
  
  return 1;
}

/* Update.
 */
 
static void _sonar_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  // Update players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  // Update screeches.
  struct screech *screech=BATTLE->screechv+BATTLE->screechc-1;
  for (i=BATTLE->screechc;i-->0;screech--) {
    if (!screech_update(battle,screech,elapsed)) {
      BATTLE->screechc--;
      memmove(screech,screech+1,sizeof(struct screech)*(BATTLE->screechc-i));
    }
  }
  
  // Tick down lums and remove when defunct.
  struct lum *lum=BATTLE->lumv+BATTLE->lumc-1;
  for (i=BATTLE->lumc;i-->0;lum--) {
    if ((lum->bright+=lum->dbright*elapsed)<=0.0) {
      BATTLE->lumc--;
      memmove(lum,lum+1,sizeof(struct lum)*(BATTLE->lumc-i));
    }
  }
  
  // First to reach the goal wins. In theory they could do it on the same frame.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->done) {
    if (r->done) battle->outcome=0;
    else battle->outcome=1;
  } else if (r->done) battle->outcome=-1;
}

/* Fill a column with the given color.
 * Caller asserts valid bounds.
 */
 
static inline void sonar_fill_column(uint32_t *v,int stride,int h,uint32_t color) {
  for (;h-->0;v+=stride) *v=color;
}

/* Render.
 */
 
static void _sonar_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  int fldw=COLC*CELLSIZE;
  int fldh=ROWC*CELLSIZE;
  int fldx=(FBW>>1)-(fldw>>1);
  int fldy=(FBH>>1)-(fldh>>1);
  int i;
  
  // Lit walls.
  const struct lum *lum=BATTLE->lumv;
  for (i=BATTLE->lumc;i-->0;lum++) {
    int alpha=(int)(lum->bright*255.0);
    if (alpha<=0) continue;
    if (alpha>0xff) alpha=0xff;
    uint32_t color=0xffffff00|alpha;
    graf_line(&g.graf,lum->ax,lum->ay,color,lum->bx,lum->by,color);
  }
  
  // Goal.
  graf_set_image(&g.graf,RID_image_battle_underground);
  graf_tile(&g.graf,fldx+(int)(BATTLE->goalx*CELLSIZE),fldy+(int)(BATTLE->goaly*CELLSIZE),0x06,0);
  
  // Players.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  uint8_t ltile=l->tileid,rtile=r->tileid;
  if (l->screechclock>0.0) ltile+=3;
  else switch (l->animframe) {
    case 1: ltile+=1; break;
    case 3: ltile+=2; break;
  }
  if (r->screechclock>0.0) rtile+=3;
  else switch (r->animframe) {
    case 1: rtile+=1; break;
    case 3: rtile+=2; break;
  }
  if (l->y<=r->y) {
    graf_tile(&g.graf,(int)(fldx+l->x*CELLSIZE),(int)(fldy+l->y*CELLSIZE),ltile,l->xform);
    graf_tile(&g.graf,(int)(fldx+r->x*CELLSIZE),(int)(fldy+r->y*CELLSIZE),rtile,r->xform);
  } else {
    graf_tile(&g.graf,(int)(fldx+r->x*CELLSIZE),(int)(fldy+r->y*CELLSIZE),rtile,r->xform);
    graf_tile(&g.graf,(int)(fldx+l->x*CELLSIZE),(int)(fldy+l->y*CELLSIZE),ltile,l->xform);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_sonar={
  .name="sonar",
  .objlen=sizeof(struct battle_sonar),
  .id=NS_battle_sonar,
  .strix_name=282,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_horz_a,
  .del=_sonar_del,
  .init=_sonar_init,
  .update=_sonar_update,
  .render=_sonar_render,
};

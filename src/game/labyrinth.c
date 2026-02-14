/* labyrinth.c
 * Logic for generating the labryinth, a random maze.
 * This requires some participation from the data:
 *  - We use all of plane 4 (NS_plane_labyrinth1).
 *  - Plane must be rectangular, and all images must use compatible tilesheets.
 *  - Plane must have a solid border and unbroken passable border just within it. In particular, the inner corners must be vacant.
 *  - Interior is filled with a grid of solid pylons. Any count, size, and spacing is allowed, but all within each column and row must align.
 *  - Top-left corner of top-left pylon must be marked with CMD_map_pylon. This command must only appear once, and must be in map (0,0).
 *  - All other pylons must start with that tile, and align to their column and row.
 *  - Pylons may be omitted to form larger rooms, but not from the left or top sets.
 *  - - Edges around an omitted pylon are presumed passable.
 *  - Space between the pylons of top row and left column must all be wallable (see lab_isw_tileid()).
 */

#include "bellacopia.h"

/* Private globals.
 */
 
struct irange {
  int p,c;
};

struct cell {
  int x,y;
};

#define CELL_HORZ      0x01 /* Has a wall rightward. Always on for the rightmost column. */
#define CELL_VERT      0x02 /* Has a wall downward. Always on for the bottommost column. */
#define CELL_REACHABLE 0x04 /* For private bookkeeping during maze generation. */
 
struct {

  // Private PRNG. The seed gets saved as NS_fld16_labyrinth_seed.
  int seed;
  int prng;
  
  // Plane geometry. This could all be captured at build time but meh. We'll build it up once per run.
  struct plane *plane; // If null, this whole block needs generated.
  int colc,rowc; // Logical maze size in cells, ie regions between pylons.
  struct irange *colv; // Cell column position and size in plane meters.
  struct irange *rowv; // Cell row position and size in plane meters.
  // Size of walls can be inferred from the space between adjacent members of (colv) and (rowv).
  int ox,oy,ow,oh; // Bounds of the inner space in plane meters. The solid border is stripped off, that's all.
  struct cell *omitv; // Cells for which the lower-right pylon is missing. Sorted LRTB. Any cell named here is not allowed to have right or bottom walls.
  int omitc; // There will never be an omit on the right or bottom edges.
  
  // The maze. This part gets rebuilt on each reset. A given seed will always produce the same maze. (assuming geometry hasn't changed).
  uint8_t *cellv; // LRTB, colc*rowc. Values are bitfields, CELL_*
  
} lab={0};

/* Tile helpers.
 *************************************************************************************/
 
static int lab_isw_tileid(uint8_t tileid) {
  return (tileid==0x00)||(tileid==0x01);
}

// Nonzero for solid tiles, given in plane meters. True if OOB.
static int lab_iss_plane(int x,int y) {
  if ((x<0)||(y<0)) return 1;
  int col=x/NS_sys_mapw;
  int row=y/NS_sys_maph;
  if ((col>=NS_sys_mapw)||(row>=NS_sys_maph)) return 1;
  struct map *map=lab.plane->v+row*lab.plane->w+col;
  int subx=x%NS_sys_mapw;
  int suby=y%NS_sys_maph;
  uint8_t physics=map->physics[map->v[suby*NS_sys_mapw+subx]];
  switch (physics) {
    case NS_physics_solid:
    case NS_physics_grabbable:
    case NS_physics_vanishable:
      return 1;
  }
  return 0;
}

static uint8_t lab_plane_tile(int x,int y) {
  if ((x<0)||(y<0)) return 0xff;
  int col=x/NS_sys_mapw;
  int row=y/NS_sys_maph;
  if ((col>=NS_sys_mapw)||(row>=NS_sys_maph)) return 0xff;
  struct map *map=lab.plane->v+row*lab.plane->w+col;
  int subx=x%NS_sys_mapw;
  int suby=y%NS_sys_maph;
  return map->v[suby*NS_sys_mapw+subx];
}

static void lab_draw_wall(struct map *map,int x,int y,int w,int h,int wall) {
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>NS_sys_mapw-w) w=NS_sys_mapw-x;
  if (y>NS_sys_maph-h) h=NS_sys_maph-y;
  if ((w<1)||(h<1)) return;
  uint8_t tileid=wall?0x01:0x00; // For now, just one tile each for empty and wall, regardless of position.
  uint8_t *dstrow=map->v+y*NS_sys_mapw+x;
  int yi=h;
  for (;yi-->0;dstrow+=NS_sys_mapw) {
    uint8_t *dstp=dstrow;
    int xi=w;
    for (;xi-->0;dstp++) {
      *dstp=tileid;
    }
  }
}

/* Pseudorandom integer from our private generator.
 * Always in (0..range-1).
 */
 
static int lab_rand(int range) {
  if (range<2) return 0;
  lab.prng^=lab.prng<<13;
  lab.prng^=lab.prng>>17;
  lab.prng^=lab.prng<<5;
  return (lab.prng&0x7fffffff)%range;
}

/* Are these coords present in (omitv)?
 */
 
static int lab_omitting(int col,int row) {
  int lo=0,hi=lab.omitc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct cell *q=lab.omitv+ck;
         if (row<q->y) hi=ck;
    else if (row>q->y) lo=ck+1;
    else if (col<q->x) hi=ck;
    else if (col>q->x) lo=ck+1;
    else return 1;
  }
  return 0;
}

/* One-time geometry acquisition.
 ************************************************************************************************/
 
static void lab_acquire_geometry() {
  
  /* Get the plane and acquire the outer bounds, ie strip off a solid border.
   */
  if (!(lab.plane=plane_by_position(NS_plane_labyrinth1))) {
    fprintf(stderr,"%s: Plane %d not found.\n",__func__,NS_plane_labyrinth1);
    return;
  }
  if (!lab.plane->w||!lab.plane->h||!lab.plane->full) {
    fprintf(stderr,"%s: Plane %d empty or irregular.\n",__func__,NS_plane_labyrinth1);
    lab.plane=0;
    return;
  }
  lab.ox=0;
  lab.oy=0;
  lab.ow=lab.plane->w*NS_sys_mapw;
  lab.oh=lab.plane->h*NS_sys_maph;
  // Move (ox,oy) diagonally until we strike something vacant.
  while ((lab.ow>0)&&(lab.oh>0)&&lab_iss_plane(lab.ox,lab.oy)) { lab.ox++; lab.ow--; lab.oy++; lab.oh--; }
  // Then draw back each of (ox,oy) cardinally to find the corner.
  while ((lab.ox>0)&&!lab_iss_plane(lab.ox-1,lab.oy)) { lab.ox--; lab.ow++; }
  while ((lab.oy>0)&&!lab_iss_plane(lab.ox,lab.oy-1)) { lab.oy--; lab.oh++; }
  // Then draw the right and bottom edges inward across solids.
  while ((lab.ow>0)&&lab_iss_plane(lab.ox+lab.ow-1,lab.oy)) lab.ow--;
  while ((lab.oh>0)&&lab_iss_plane(lab.ox,lab.oy+lab.oh-1)) lab.oh--;
  if ((lab.ow<1)||(lab.oh<1)) {
    fprintf(stderr,"%s: Failed to detect rectangular border.\n",__func__);
    lab.plane=0;
    return;
  }
  
  /* Locate the first pylon, which must be in the top-left map.
   */
  int pyx=-1,pyy=-1;
  struct map *map=lab.plane->v;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_pylon) {
      pyx=cmd.arg[0];
      pyy=cmd.arg[1];
      break;
    }
  }
  if ((pyx<0)||(pyy<0)||(pyx>=NS_sys_mapw)||(pyy>=NS_sys_maph)) {
    fprintf(stderr,"%s: map:%d must identify the first pylon\n",__func__,map->rid);
    lab.plane=0;
    return;
  }
  uint8_t pytile=map->v[pyy*NS_sys_mapw+pyx];
  
  /* Read from (ox,pyy) rightward, populating (lab.colv).
   */
  lab.colc=0;
  int a=0;
  int stop=lab.ox+lab.ow;
  int planex=lab.ox;
  int subx=lab.ox;
  const uint8_t *p=map->v+pyy*NS_sys_mapw+lab.ox;
  struct irange *current=0; // Null between columns, non-null while measuring one.
  for (;planex<stop;planex++,subx++,p++) {
    if (subx>=NS_sys_mapw) { // Load next map. Doesn't affect column.
      map++;
      subx=0;
      p=map->v+pyy*NS_sys_mapw;
    }
    if (*p==pytile) { // Start of pylon: Terminate the current column.
      current=0;
    } else if (current) { // Grow current column.
      current->c++;
    } else if (lab_isw_tileid(*p)) { // Start a new column here.
      if (lab.colc>=a) {
        int na=a+32;
        if (na>INT_MAX/sizeof(struct irange)) { lab.plane=0; return; }
        void *nv=realloc(lab.colv,sizeof(struct irange)*na);
        if (!nv) { lab.plane=0; return; }
        lab.colv=nv;
        a=na;
      }
      current=lab.colv+lab.colc++;
      current->p=planex;
      current->c=1;
    }
  }
  
  /* Same idea for (rowv).
   */
  map=lab.plane->v;
  lab.rowc=0;
  a=0;
  stop=lab.oy+lab.oh;
  int planey=lab.oy;
  int suby=lab.oy;
  p=map->v+lab.oy*NS_sys_mapw+pyx;
  current=0;
  for (;planey<stop;planey++,suby++,p+=NS_sys_mapw) {
    if (suby>=NS_sys_maph) {
      map+=lab.plane->w;
      suby=0;
      p=map->v+pyx;
    }
    if (*p==pytile) {
      current=0;
    } else if (current) {
      current->c++;
    } else if (lab_isw_tileid(*p)) {
      if (lab.rowc>=a) {
        int na=a+32;
        if (na>INT_MAX/sizeof(struct irange)) { lab.plane=0; return; }
        void *nv=realloc(lab.rowv,sizeof(struct irange)*na);
        if (!nv) { lab.plane=0; return; }
        lab.rowv=nv;
        a=na;
      }
      current=lab.rowv+lab.rowc++;
      current->p=planey;
      current->c=1;
    }
  }
  
  /* Rebuild (omitv) by checking every cell.
   */
  lab.omitc=0;
  a=0;
  int row=0;
  for (;row<lab.rowc;row++) {
    int y=lab.rowv[row].p+lab.rowv[row].c;
    if (y>=lab.oy+lab.oh) break;
    int col=0;
    for (;col<lab.colc;col++) {
      int x=lab.colv[col].p+lab.colv[col].c;
      if (x>=lab.ox+lab.ow) break;
      if (lab_plane_tile(x,y)!=pytile) {
        if (lab.omitc>=a) {
          int na=a+16;
          if (na>INT_MAX/sizeof(struct cell)) { lab.plane=0; return; }
          void *nv=realloc(lab.omitv,sizeof(struct cell)*na);
          if (!nv) { lab.plane=0; return; }
          lab.omitv=nv;
          a=na;
        }
        struct cell *cell=lab.omitv+lab.omitc++;
        cell->x=col;
        cell->y=row;
      }
    }
  }
}

/* Build walls, with geometry and seed established.
 *************************************************************************************************/
 
// Mark this cell reachable and recur into neighbors it can reach.
// (x,y) are OOB-safe.
static void lab_extend_reachability(int x,int y,int p) {
  if ((x<0)||(y<0)||(x>=lab.colc)||(y>=lab.rowc)) return;
  if (p<0) p=y*lab.colc+x;
  uint8_t *cell=lab.cellv+p;
  if ((*cell)&CELL_REACHABLE) return; // Already visited, I guess.
  (*cell)|=CELL_REACHABLE;
  if (!((*cell)&CELL_HORZ)) lab_extend_reachability(x+1,y,p+1);
  if (!((*cell)&CELL_VERT)) lab_extend_reachability(x,y+1,p+lab.colc);
  if ((x>0)&&!(cell[-1]&CELL_HORZ)) lab_extend_reachability(x-1,y,p-1);
  if ((y>0)&&!(cell[-lab.colc]&CELL_VERT)) lab_extend_reachability(x,y-1,p-lab.colc);
}

// Given a newly-reachable cell, knock down one wall into unreachable territory, and recur there.
static void lab_drunk_snake(int x,int y) {
  if ((x<0)||(y<0)||(x>=lab.colc)||(y>=lab.rowc)) return;
  uint8_t *cell=lab.cellv+y*lab.colc+x;
  if (!((*cell)&CELL_REACHABLE)) return;
  struct move {
    int x,y;
  } movev[4];
  int movec=0;
  if ((x>0)&&(cell[-1]&CELL_HORZ)&&!(cell[-1]&CELL_REACHABLE)) movev[movec++]=(struct move){x-1,y};
  if ((x<lab.colc-1)&&(cell[0]&CELL_HORZ)&&!(cell[1]&CELL_REACHABLE)) movev[movec++]=(struct move){x+1,y};
  if ((y>0)&&(cell[-lab.colc]&CELL_VERT)&&!(cell[-lab.colc]&CELL_REACHABLE)) movev[movec++]=(struct move){x,y-1};
  if ((y<lab.rowc-1)&&(cell[0]&CELL_VERT)&&!(cell[lab.colc]&CELL_REACHABLE)) movev[movec++]=(struct move){x,y+1};
  if (movec<1) return;
  int movep=lab_rand(movec);
  struct move *move=movev+movep;
  uint8_t *ncell=lab.cellv+move->y*lab.colc+move->x;
  (*ncell)|=CELL_REACHABLE;
  if (move->x<x) (*ncell)&=~CELL_HORZ;
  else if (move->x>x) (*cell)&=~CELL_HORZ;
  else if (move->y<y) (*ncell)&=~CELL_VERT;
  else (*cell)&=~CELL_VERT;
  lab_drunk_snake(move->x,move->y);
}
 
static void lab_build_walls() {

  // Reset PRNG.
  lab.prng=lab.seed|(lab.seed<<16);
  
  // Initialize (cellv). We start with every possible wall present.
  int cellc=lab.colc*lab.rowc;
  if (cellc<1) return;
  if (!lab.cellv) {
    if (!(lab.cellv=malloc(cellc))) return;
  }
  memset(lab.cellv,CELL_HORZ|CELL_VERT,cellc);
  
  // Knock down walls where a pylon was omitted.
  struct cell *omit=lab.omitv;
  int i=lab.omitc;
  for (;i-->0;omit++) {
    uint8_t *p=lab.cellv+omit->y*lab.colc+omit->x;
    *p=0;
    p[1]&=~CELL_VERT; // Omits can't exist on the positive edges; we don't need to check bounds.
    p[lab.colc]&=~CELL_HORZ;
  }
  
  /* Select a starting point at random, doesn't matter where.
   * We'll mark that cell reachable by fiat.
   * Note that we also must check for walls removed by omit; there may be more initially-reachable cells.
   */
  int startx=lab_rand(lab.colc);
  int starty=lab_rand(lab.rowc);
  int startp=starty*lab.colc+startx;
  lab_extend_reachability(startx,starty,startp);
  
  /* Consider the set of walls between reachable and unreachable cells.
   * If there aren't any, then every cell is reachable and we win.
   * Pick one wall at random and knock it down.
   * Then pick another such wall from that newly-reachable cell, continuing to snake out until there's nowhere to go.
   */
  for (;;) {
    
    // List all candidate walls.
    #define CANDIDATE_LIMIT 256
    struct wall {
      uint8_t x,y,bit;
    } candidatev[CANDIDATE_LIMIT];
    int candidatec=0;
    #define ADDCANDIDATE(cx,cy,mask) { \
      if (candidatec>=CANDIDATE_LIMIT) goto _ready_; \
      candidatev[candidatec++]=(struct wall){cx,cy,mask}; \
    }
    uint8_t *p=lab.cellv;
    int y=0; for (;y<lab.rowc;y++) {
      int x=0; for (;x<lab.colc;x++,p++) {
        if ((*p)&CELL_REACHABLE) {
          if (((*p)&CELL_HORZ)&&(x<lab.colc-1)&&!(p[1]&CELL_REACHABLE)) ADDCANDIDATE(x,y,CELL_HORZ)
          if (((*p)&CELL_VERT)&&(y<lab.rowc-1)&&!(p[lab.colc]&CELL_REACHABLE)) ADDCANDIDATE(x,y,CELL_VERT)
        } else {
          if (((*p)&CELL_HORZ)&&(x<lab.colc-1)&&(p[1]&CELL_REACHABLE)) ADDCANDIDATE(x,y,CELL_HORZ)
          if (((*p)&CELL_VERT)&&(y<lab.rowc-1)&&(p[lab.colc]&CELL_REACHABLE)) ADDCANDIDATE(x,y,CELL_VERT)
        }
      }
    }
    #undef ADDCANDIDATE
    #undef CANDIDATE_LIMIT
   _ready_:;
   
    // No demolition candidates, we must be finished.
    if (!candidatec) break;
    
    // Pick a demolition candidate.
    int candidatep=lab_rand(candidatec);
    struct wall *wall=candidatev+candidatep;
    lab.cellv[wall->y*lab.colc+wall->x]&=~wall->bit;
    
    // Extend reachability to whichever side of the wall was previously unreachable.
    // This is recursive; we might be breaking open a region already connected via omits.
    int fwx=wall->x,fwy=wall->y;
    if (lab.cellv[wall->y*lab.colc+wall->x]&CELL_REACHABLE) {
      switch (wall->bit) {
        case CELL_HORZ: fwx++; break;
        case CELL_VERT: fwy++; break;
      }
    }
    lab_extend_reachability(fwx,fwy,-1);
    
    /* The above is sufficient to generate a valid maze every time.
     * But it will tend to generate hairy mazes: Lots of tiny dead-end paths.
     * To encourage longer paths, now we'll trace a snake randomly from that forward position, until it runs out of moves.
     */
    lab_drunk_snake(fwx,fwy);
  }
}

/* Public entry points.
 *****************************************************************************************************/

/* Force reset.
 */
 
void labyrinth_reset() {
  
  if (!lab.plane) {
    lab_acquire_geometry();
    if (!lab.plane) return;
  }
  
  int panic=100;
  for (;;) {
    lab.seed=rand()&0xffff;
    if (lab.seed) break;
    if (panic--<0) {
      fprintf(stderr,"Panic! Failed to produce a nonzero labyrinth seed after a bunch of tries. Using 1.\n");
      lab.seed=1;
      break;
    }
  }
  store_set_fld16(NS_fld16_labyrinth_seed,lab.seed);
  //fprintf(stderr,"Chose labyrinth seed %d.\n",lab.seed);
  
  lab_build_walls();
}
 
/* Freshen map.
 */
 
void labyrinth_freshen_map(struct map *map) {

  // Confirm this map really is in play.
  if (!map) return;
  if (map->z!=NS_plane_labyrinth1) return;

  // Rebuild the longer-term things if dirty; get out if they fail.
  if (!lab.seed) {
    if (!(lab.seed=store_get_fld16(NS_fld16_labyrinth_seed))) {
      labyrinth_reset();
    } else {
      //fprintf(stderr,"Restore labyrinth seed %d.\n",lab.seed);
      if (!lab.plane) {
        lab_acquire_geometry();
      }
      lab_build_walls();
    }
  }
  if (!lab.plane) return;
  if (!lab.cellv) return;
  
  // Find the map's extent in plane meters.
  int mapx=map->lng*NS_sys_mapw;
  int mapy=map->lat*NS_sys_maph;
  int mapw=NS_sys_mapw;
  int maph=NS_sys_maph;
  
  /* Find the potentially impacted cells.
   * We're interested in cells that overlap us, or the margin after them overlaps us.
   */
  int col0=0;
  while ((col0<lab.colc)&&(lab.colv[col0].p>=mapx+mapw)) col0++;
  if (col0>=lab.colc) return;
  int colc=1;
  while ((col0+colc<lab.colc)&&(lab.colv[col0+colc-1].p+lab.colv[col0+colc-1].c<mapx+mapw)) colc++;
  int row0=0;
  while ((row0<lab.rowc)&&(lab.rowv[row0].p>=mapy+maph)) row0++;
  if (row0>=lab.rowc) return;
  int rowc=1;
  while ((row0+rowc<lab.rowc)&&(lab.rowv[row0+rowc-1].p+lab.rowv[row0+rowc-1].c<mapy+maph)) rowc++;
  
  /* Iterate those cells, skip if in (omitv), find their walls' ranges, and draw those walls where they overlap the map.
   */
  int col=col0+colc;
  while (col-->col0) {
    int rx=lab.colv[col].p+lab.colv[col].c;
    int rw=(col<lab.colc-1)?(lab.colv[col+1].p-rx):(lab.ox+lab.ow-rx);
    int row=row0+rowc;
    while (row-->row0) {
      if (lab_omitting(col,row)) continue;
      int by=lab.rowv[row].p+lab.rowv[row].c;
      int bh=(row<lab.rowc-1)?(lab.rowv[row+1].p-by):(lab.oy+lab.oh-by);
      uint8_t cell=lab.cellv[row*lab.colc+col];
      if ((col<lab.colc-1)&&!lab_omitting(col,row-1)) lab_draw_wall(map,rx-mapx,lab.rowv[row].p-mapy,rw,lab.rowv[row].c,cell&CELL_HORZ);
      if ((row<lab.rowc-1)&&!lab_omitting(col-1,row)) lab_draw_wall(map,lab.colv[col].p-mapx,by-mapy,lab.colv[col].c,bh,cell&CELL_VERT);
    }
  }
}

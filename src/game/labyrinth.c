#include "bellacopia.h"

/* The final disposition is really two grids.
 * One for vertical walls, and one for horizontal walls.
 * Vertical walls appear at map junctions, and also in the middle of non-edge maps.
 * Horizontal walls only appear at junctions.
 */
#define VERTCOLC 9
#define VERTROWC 5
#define HORZCOLC 10
#define HORZROWC 4

/* When generating the maze, we store it as a single grid of cells.
 * Mostly two cells per map, but the left and right edges are just one.
 */
#define CELLCOLC 10
#define CELLROWC 5

/* Private globals.
 */
 
static struct {

  // Also stored as NS_fld16_labyrinth_seed. The entire state can be rebuilt from this.
  int seed;
  uint32_t prng; // The volatile state; initially derived from (seed).
  
  // Final answer, one boolean byte per wall.
  uint8_t vertv[VERTCOLC*VERTROWC];
  uint8_t horzv[HORZCOLC*HORZROWC];
  
  // Intermediate cell construction. (0x40|0x10|0x08|0x02) set if there's a wall. (0x01) if we have a path to Grand Central.
  uint8_t cellv[CELLCOLC*CELLROWC];
  
} lab={0};

/* Pseudorandom integer from our private generator.
 * Always in (0..range-1).
 */
 
static int labyrinth_rand(int range) {
  if (range<2) return 0;
  lab.prng^=lab.prng<<13;
  lab.prng^=lab.prng>>17;
  lab.prng^=lab.prng<<5;
  return (lab.prng&0x7fffffff)%range;
}

/* Generate maze, the interesting part.
 * At entry, (seed) must be set and equal to (prng), and (cellv) must be all walls.
 * When we return, (cellv) will be updated such that every cell is reachable from every other.
 * We don't touch (horz,vertv).
 */
 
static void labyrinth_generate_maze() {

  /* First select one cell at random to serve as Grand Central.
   * This cell alone is "reachable" by fiat, and the maze will grow out of it.
   */
  int gcp=labyrinth_rand(CELLCOLC*CELLROWC);
  lab.cellv[gcp]|=0x01;

  /* Then knock down one wall at a time until every cell is reachable.
   */
  for (;;) {
  
    /* Identify all demolition candidates.
     * A valid candidate is a wall between an accessible and inaccessible cell.
     * In effect, we're spreading outward from the initial Grand Central.
     * The upper candidate limit is surely less than (w*h*4), but easier to spend the extra stack space and not figure it out for real.
     */
    #define CANDIDATE_LIMIT (CELLCOLC*CELLROWC*4)
    struct candidate {
      uint8_t p,x,y; // Position of the accessible cell.
      uint8_t d; // (0x40,0x10,0x08,0x02), wall selection, points to the inaccessible cell.
    } candidatev[CANDIDATE_LIMIT];
    int candidatec=0;
    #define ADDCANDIDATE(bit) { \
      if (candidatec>=CANDIDATE_LIMIT) goto _candidates_ready_; \
      candidatev[candidatec++]=(struct candidate){y*CELLCOLC+x,x,y,bit}; \
    }
    
    const uint8_t *src=lab.cellv;
    int y=0; for (;y<CELLROWC;y++) {
      int x=0; for (;x<CELLCOLC;x++,src++) {
        if (!((*src)&0x01)) continue; // We will only operate from an accessible vantage point.
        // Each accessible cell yields 0..4 candidates. We don't need to check the wall bits, just the accessible bit.
        if ((x>0)&&!(src[-1]&0x01)) ADDCANDIDATE(0x10)
        if ((y>0)&&!(src[-CELLCOLC]&0x01)) ADDCANDIDATE(0x40)
        if ((x<CELLCOLC-1)&&!(src[1]&0x01)) ADDCANDIDATE(0x08)
        if ((y<CELLROWC-1)&&!(src[CELLCOLC]&0x01)) ADDCANDIDATE(0x02)
      }
    }
    #undef ADDCANDIDATE
    #undef CANDIDATE_LIMIT
   _candidates_ready_:;
    
    // If there's no demolition candidates, the maze is ready.
    if (candidatec<1) return;
    
    /* Pick one at random, and eliminate that wall from both the candidate and its neighbor.
     * Then mark the neighbor accessible.
     * It's a consequence of this algorithm, that we don't need to recursively check newly-accessible cells.
     * The neighbor we just connected to, it's not possible for them to be connected to anything else yet.
     */
    int candidatep=labyrinth_rand(candidatec);
    const struct candidate *focus=candidatev+candidatep;
    lab.cellv[focus->p]&=~focus->d;
    uint8_t *neighbor;
    switch (focus->d) {
      case 0x40: neighbor=lab.cellv+focus->p-CELLCOLC; (*neighbor)&=~0x02; (*neighbor)|=0x01; break;
      case 0x10: neighbor=lab.cellv+focus->p-1;        (*neighbor)&=~0x08; (*neighbor)|=0x01; break;
      case 0x08: neighbor=lab.cellv+focus->p+1;        (*neighbor)&=~0x10; (*neighbor)|=0x01; break;
      case 0x02: neighbor=lab.cellv+focus->p+CELLCOLC; (*neighbor)&=~0x40; (*neighbor)|=0x01; break;
    }
  }
}

/* Set (vertv,horzv) empty, and (cellv) all walls.
 */
 
static void labyrinth_clear_grids() {
  memset(lab.vertv,0,sizeof(lab.vertv));
  memset(lab.horzv,0,sizeof(lab.horzv));
  memset(lab.cellv,0x5a,sizeof(lab.cellv));
}

/* With each cell accessible to Grand Central, now write the walls in (vertv,horzv).
 * Those must be zeroed first.
 */
 
static void labyrinth_write_walls() {
  // Each wall is named twice in (cellv). We won't check consistency, we'll just use one of them.
  uint8_t *dst;
  const uint8_t *src;
  int x,y;
  for (dst=lab.vertv,src=lab.cellv,y=VERTROWC;y-->0;) {
    for (x=VERTCOLC;x-->0;dst++,src++) {
      if ((*src)&0x08) *dst=1;
    }
    src++; // Skip the last cell of each cell row.
  }
  for (dst=lab.horzv,src=lab.cellv,y=HORZROWC;y-->0;) {
    for (x=HORZCOLC;x-->0;dst++,src++) {
      if ((*src)&0x02) *dst=1;
    }
  }
}

/* Reset the entire puzzle.
 */
 
void labyrinth_reset() {
  fprintf(stderr,"%s:%d:%s\n",__FILE__,__LINE__,__func__);
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
  lab.prng=lab.seed|(lab.seed<<16);
  store_set_fld16(NS_fld16_labyrinth_seed,lab.seed);
  fprintf(stderr,"Chose labyrinth seed %d.\n",lab.seed);
  labyrinth_clear_grids();
  labyrinth_generate_maze();
  labyrinth_write_walls();
}

/* Primitives for testing and setting walls.
 * For now, every cell that participates must be either 0x00 or 0x01.
 * We could get fancier on that if we felt like it.
 */
 
static int lab_isw_tileid(uint8_t tileid) {
  return ((tileid==0x00)||(tileid==0x01));
}

static int lab_isw_cell(struct map *map,int x,int y) {
  return lab_isw_tileid(map->v[y*NS_sys_mapw+x]);
}
 
static int lab_isw_rect(const struct map *map,int x,int y,int w,int h) {
  const uint8_t *row=map->v+y*NS_sys_mapw+x;
  for (;h-->0;row+=NS_sys_mapw) {
    const uint8_t *p=row;
    int xi=w;
    for (;xi-->0;p++) if (!lab_isw_tileid(*p)) return 0;
  }
  return 1;
}

static void labyrinth_set_wall(struct map *map,int x,int y,int w,int h,int wall) {
  uint8_t tileid=wall?0x01:0x00;
  uint8_t *row=map->v+y*NS_sys_mapw+x;
  for (;h-->0;row+=NS_sys_mapw) memset(row,tileid,w);
}

/* Rewrite one horizontal segment.
 * (xgross) in 0..1, (ygross) in 0..1, (wall) boolean.
 */
 
static void labyrinth_set_horz(struct map *map,int xgross,int ygross,int wall) {
  // Determine horizontal position by reading along the top edge until we find a 0x00 or 0x01.
  int x,w,y,h=1;
  if (ygross) y=NS_sys_maph-1; else y=0;
  const uint8_t *row=map->v+y*NS_sys_mapw;
  if (xgross) { // Read right-to-left.
    x=NS_sys_mapw;
    while ((x>0)&&!lab_isw_tileid(row[x-1])) x--;
    w=0;
    while ((x>0)&&lab_isw_tileid(row[x-1])) { x--; w++; }
  } else { // Read left-to-right.
    x=0;
    while ((x<NS_sys_mapw)&&!lab_isw_tileid(row[x])) x++;
    w=0;
    while ((x+w<NS_sys_mapw)&&lab_isw_tileid(row[x+w])) w++;
  }
  // Horz range must not touch the edge.
  if ((w<1)||(x<=0)||(x+w>=NS_sys_mapw)) {
    fprintf(stderr,"%s map:%d xgross=%d ygross=%d, invalid horz range %d,%d\n",__func__,map->rid,xgross,ygross,x,w);
    return;
  }
  // Extend vertically while rows have the same [nonwallable,wallable...,nonwallable] disposition.
  if (ygross) { // Extend upward.
    while ((y>0)&&lab_isw_rect(map,x,y-1,w,1)&&!lab_isw_cell(map,x-1,y-1)&&!lab_isw_cell(map,x+w,y-1)) { y--; h++; }
  } else { // Extend downward.
    while ((y+h<NS_sys_maph)&&lab_isw_rect(map,x,y+h,w,1)&&!lab_isw_cell(map,x-1,y+h)&&!lab_isw_cell(map,x+w,y+h)) h++;
  }
  labyrinth_set_wall(map,x,y,w,h,wall);
}

/* Rewrite one vertical segment.
 * (xgross) in 0..2, (wall) boolean.
 */
 
static void labyrinth_set_vert(struct map *map,int xgross,int wall) {
  // Top row of the map establishes our horizontal bounds.
  // Each contiguous run of non-wallables is a gross range candidate.
  // If latitude is zero, ie the top edge of the plane, use the bottom row instead.
  int x=0,w=0;
  int qx=0,qgross=0;
  const uint8_t *p=map->v;
  if (!map->lat) p+=NS_sys_mapw*(NS_sys_maph-1);
  for (;;) {
    if (qx>=NS_sys_mapw) {
      fprintf(stderr,"%s map:%d xgross %d not found\n",__func__,map->rid,xgross);
      return;
    }
    if (lab_isw_tileid(*p)) { qx++; p++; continue; }
    p++;
    x=qx++;
    w=1;
    while ((qx<NS_sys_mapw)&&!lab_isw_tileid(*p)) { qx++; p++; w++; }
    if (qgross==xgross) break;
    qgross++;
  }
  // Vertical walls are pretty much the whole map. Start from full and trim edges where non-wallable.
  int y=0,h=NS_sys_maph;
  while (h&&!lab_isw_rect(map,x,y,w,1)) { y++; h--; }
  while (h&&!lab_isw_rect(map,x,y+h-1,w,1)) h--;
  if (!h) {
    fprintf(stderr,"%s map:%d failed to locate column [%s:%d]\n",__func__,map->rid,__FILE__,__LINE__);
    return;
  }
  labyrinth_set_wall(map,x,y,w,h,wall);
}

/* Update one map.
 */

void labyrinth_freshen_map(struct map *map) {
  
  // Confirm it's actually within the labyrinth.
  if (!map) return;
  if (map->z!=NS_plane_labyrinth1) return;
  int col=(map->lng<<1)-1; // Each map is two columns. The first can be (-1), meaning right column only, the left edge of the plane.
  int row=map->lat; // Row is much simpler; one per map row.
  if ((col<-1)||(row<0)||(col>=CELLCOLC)||(row>=CELLROWC)) return;
  
  if (!lab.seed) labyrinth_reset();
  fprintf(stderr,"%s:%d:%s map:%d at (%d,%d) in maze\n",__FILE__,__LINE__,__func__,map->rid,col,row);
  // There's 1 or 3 vertical walls and 1, 2, or 4 horizontals. Everything depends on whether (colc) and (colc+1) are in range.
  /* Interior maps have 7 possible walls:
   *    X..a..X..b..X
   *    c.....d.....e
   *    X..f..X..g..X
   * Horizontal edge maps have no (d), and also missing one of (a,c,f) or (b,e,g).
   * Vertical edge maps omit either (a,b) or (f,g).
   */
  if (col>0) { // (c) is in play and (d,a,f) might be.
    labyrinth_set_vert(map,0,lab.vertv[row*VERTCOLC+col-1]); // c
    if (col+1<CELLCOLC) labyrinth_set_vert(map,1,lab.vertv[row*VERTCOLC+col]); // d
    if (row>0) labyrinth_set_horz(map,0,0,lab.horzv[(row-1)*HORZCOLC+col]); // a
    if (row<HORZROWC) labyrinth_set_horz(map,0,1,lab.horzv[row*HORZCOLC+col]); // f
  }
  if (col+1<CELLCOLC) { // (e) is play and (b,g) might be.
    labyrinth_set_vert(map,(col>0)?2:1,lab.vertv[row*VERTCOLC+col+1]); // e
    if (row>0) labyrinth_set_horz(map,1,0,lab.horzv[(row-1)*HORZCOLC+col+1]); // b
    if (row<HORZROWC) labyrinth_set_horz(map,1,1,lab.horzv[row*HORZCOLC+col+1]); // g
  }
}

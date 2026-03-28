#include "bellacopia.h"

#define SPRC_ACCEL_THRESH 4 /* If below this, fudge maps to make a spawn more likely. */
#define SPRC_DECEL_THRESH 6 /* If over this, fudge maps to make less likely. */
#define SPRC_HARD_LIMIT   8 /* Above this, just stop spawning. */
#define KILL_DISTANCE         200 /* So far offscreen, we drop at the next opportunity no matter what. */
#define QUESTIONABLE_DISTANCE 100 /* Drop if our sprite count is too high. */
#define QUESTIONABLE_SPRC       5

static uint16_t primev[]={
1,2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,
73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,
179,181,191,193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,
283,293,307,311,313,317,331,337,347,349,353,359,367,373,379,383,389,397,401,409,
419,421,431,433,439,443,449,457,461,463,467,479,487,491,499,503,509,521,523,541,
547,557,563,569,571,577,587,593,599,601,607,613,617,619,631,641,643,647,653,659,
661,673,677,683,691,701,709,719,727,733,739,743,751,757,761,769,773,787,797,809,
811,821,823,827,829,839,853,857,859,863,877,881,883,887,907,911,919,929,937,941,
947,953,967,971,977,983,991,997,1009,1013,1019,1021,1031,1033,1039,1049,1051,1061,1063,1069,
1087,1091,1093,1097,1103,1109,1117,1123,1129,1151,1153,1163,1171,1181,1187,1193,1201,1213,1217,1223,
1229,1231,1237,1249,1259,
};

/* Reset one spawnmap.
 */
 
static void spawnmap_reset(struct spawnmap *spawnmap,struct map *map) {

  // First zero everything. If it stays zeroes, it's noop.
  // (map) may be null, in fact the first one will always be.
  memset(spawnmap,0,sizeof(struct spawnmap));
  if (!map) return;
  
  // Add up rsprite weights. Get out if zero.
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd={0};
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_rsprite) {
      uint8_t weight=cmd.arg[2];
      if (!weight) {
        fprintf(stderr,"map:%d contains an rsprite (sprite:%d) with weight zero\n",map->rid,(cmd.arg[0]<<8)|cmd.arg[1]);
      } else {
        spawnmap->wsum+=weight;
      }
    }
  }
  if (!spawnmap->wsum) return;
  
  /* We're deliberately cagey about the exact meaning of "weight".
   * But we need to settle on something concrete here.
   * Let's say a weight of 255, the most that one command could use, will yield a period of 10.
   */
  const int scale=10;
  spawnmap->period=(255*scale)/spawnmap->wsum;
  if (spawnmap->period<1) {
    fprintf(stderr,"map:%d: wsum %d exceeds expected limit of %d\n",map->rid,spawnmap->wsum,255*scale);
    spawnmap->period=1;
  }
  spawnmap->spawn_counter=spawnmap->period;
  
  /* (step) must be coprime to (wsum), and smaller.
   * Start from a random position in our list of primes, and chop in half until we're under (wsum).
   * A (wsum) of 1 is legal and there's nothing lower; in that case, use 1 for (step).
   */
  int primec=sizeof(primev)/sizeof(primev[0]);
  int primep=(rand()&0x7fffffff)%primec;
  while (primep&&(primev[primep]>=spawnmap->wsum)) primep>>=1;
  spawnmap->step=primev[primep];
  
  /* Initialize (choice) randomly.
   */
  spawnmap->choice=rand()%spawnmap->step;
}

/* Reset.
 */
 
void spawner_reset() {
  g.spawner.z=-1;
  g.spawner.spawnmapc=0;
  sprite_group_clear(&g.spawner.group);
  g.spawner.discard=0;
  
  /* Rebuild (g.spawner.spawnmapv) from (g.mapstore.byidv).
   */
  if (g.spawner.spawnmapv) free(g.spawner.spawnmapv);
  if (!(g.spawner.spawnmapv=malloc(sizeof(struct spawnmap)*g.mapstore.byidc))) return;
  struct spawnmap *spawnmap=g.spawner.spawnmapv;
  struct map **mapp=g.mapstore.byidv;
  for (;g.spawner.spawnmapc<g.mapstore.byidc;g.spawner.spawnmapc++,spawnmap++,mapp++) {
    spawnmap_reset(spawnmap,*mapp);
  }
}

/* Count living rsprites with a given (arg).
 * Return nonzero if there's >=(limit).
 */
 
static int spawnmap_over_limit(const void *arg,int limit) {
  struct sprite **p=g.spawner.group.sprv;
  int i=g.spawner.group.sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue;
    if (sprite->arg!=arg) continue;
    if (--limit<=0) return 1;
  }
  return 0;
}

/* Expose one cell.
 * Caller ensures the tile is agreeable.
 * We'll check for other sprites.
 * Returns nonzero if we spawn something.
 * Does not touch (spawn_counter).
 */
 
static int spawner_expose_cell(struct spawnmap *spawnmap,const struct map *map,double x,double y) {
  
  /* Reject if there's a solid sprite within 1 m cardinally.
   * Don't bother with the hitboxes, keep it as neat as possible.
   */
  double limitl=x-1.0;
  double limitr=x+1.0;
  double limitt=y-1.0;
  double limitb=y+1.0;
  struct sprite **otherp=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->x<limitl) continue;
    if (other->x>limitr) continue;
    if (other->y<limitt) continue;
    if (other->y>limitb) continue;
    return 0; // Occupado!
  }
  
  /* Which sprite do we want?
   * Reread (map.cmd) and add up the rsprite weights. When that counter crosses (choice), that's the one.
   * This should never fail to find one, but if it does, drop (choice) to zero and report a failure.
   */
  const uint8_t *arg=0;
  int acc=0;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_rsprite) {
      acc+=cmd.arg[2];
      if (acc>spawnmap->choice) {
        arg=cmd.arg;
        break;
      }
    }
  }
  if (!arg) {
    spawnmap->choice=0;
    return 0;
  }
  
  /* Step (choice).
   */
  spawnmap->choice+=spawnmap->step;
  if (spawnmap->choice>=spawnmap->wsum) spawnmap->choice-=spawnmap->wsum;
  
  /* If we have too many of these afield already, report a failure.
   * Caller will try again soon.
   */
  int rid=(arg[0]<<8)|arg[1];
  int limit=arg[3]; if (!limit) limit=1;
  const uint8_t *sarg=arg+4;
  if (spawnmap_over_limit(sarg,limit)) return 0;
  
  /* If we're over the deceleration threshold, (discard) must be zero, and we'll set it positive.
   * When discarding a spawn this way, report a success so the caller resets counters.
   */
  int over=g.spawner.group.sprc-SPRC_DECEL_THRESH;
  if (over>0) {
    if (g.spawner.discard>0) {
      g.spawner.discard--;
      return 1;
    }
    g.spawner.discard=over;
  } else {
    g.spawner.discard=0;
  }
  
  /* Try to spawn it.
   */
  struct sprite *sprite=sprite_spawn(x,y,rid,sarg,4,0,0,0);
  if (!sprite) return 0;
  sprite_group_add(&g.spawner.group,sprite);
  return 1;
}

/* Expose cells for one map.
 */
 
static void spawner_expose_map(struct spawnmap *spawnmap,const struct map *map,int x,int y,int w,int h) {

  // If we're over the hard limit, forget it. And keep all the counters right where they are.
  if (g.spawner.group.sprc>=SPRC_HARD_LIMIT) return;

  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>NS_sys_mapw-w) w=NS_sys_mapw-x;
  if (y>NS_sys_maph-h) h=NS_sys_maph-y;
  if ((w<1)||(h<1)) return;
  
  // Visit each cell. Only operate on vacant ones.
  const uint8_t *cellrow=map->v+y*NS_sys_mapw+x;
  int yi=h;
  double yf=map->lat*NS_sys_maph+y+0.5;
  for (;yi-->0;cellrow+=NS_sys_mapw,yf+=1.0) {
    const uint8_t *cellp=cellrow;
    int xi=w;
    double xf=map->lng*NS_sys_mapw+x+0.5;
    for (;xi-->0;cellp++,xf+=1.0) {
      uint8_t physics=map->physics[*cellp];
      if (physics!=NS_physics_vacant) continue;
      if (spawnmap->spawn_counter>0) {
        int extra=SPRC_ACCEL_THRESH-g.spawner.group.sprc;
        if (extra>0) spawnmap->spawn_counter-=extra;
        spawnmap->spawn_counter--;
      } else if (spawner_expose_cell(spawnmap,map,xf,yf)) {
        spawnmap->spawn_counter=spawnmap->period;
      }
    }
  }
}

/* Distance to the nearest viewport edge, or zero if onscreen.
 */
 
static int spawner_distance_to_viewport(const struct sprite *sprite) {
  int x=(int)(sprite->x*NS_sys_tilesize);
  int y=(int)(sprite->y*NS_sys_tilesize);
  int dx,dy;
  if ((dx=g.camera.rx-x)<-FBW) dx=-dx-FBW; else if (dx<0) dx=0;
  if ((dy=g.camera.ry-y)<-FBH) dy=-dy-FBH; else if (dy<0) dy=0;
  return (dx>dy)?dx:dy;
}

/* Expose cells, main entry point.
 */

void spawner_expose(int x,int y,int w,int h) {

  // Kill any sprites too far offscreen.
  struct sprite **spritep=g.spawner.group.sprv;
  int i=g.spawner.group.sprc;
  for (;i-->0;spritep++) {
    struct sprite *sprite=*spritep;
    if (sprite->defunct) continue;
    int distance=spawner_distance_to_viewport(sprite);
    if (distance>=KILL_DISTANCE) {
      sprite_kill_soon(sprite);
    } else if ((distance>=QUESTIONABLE_DISTANCE)&&(g.spawner.group.sprc>=QUESTIONABLE_SPRC)) {
      sprite_kill_soon(sprite);
    }
  }
  
  // Drop all defunct sprites.
  for (spritep=g.spawner.group.sprv+g.spawner.group.sprc-1,i=g.spawner.group.sprc;i-->0;spritep--) {
    struct sprite *sprite=*spritep;
    if (sprite->defunct) sprite_kill(sprite);
  }
  
  // Over the hard limit, no need to do anything else.
  if (g.spawner.group.sprc>=SPRC_HARD_LIMIT) return;
  
  // Acquire the plane and clip to it. Fully OOB is entirely possible.
  const struct plane *plane=plane_by_position(g.camera.z);
  if (!plane) return;
  int planew=plane->w*NS_sys_mapw;
  int planeh=plane->h*NS_sys_maph;
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>planew-w) w=planew-x;
  if (y>planeh-h) h=planeh-y;
  if ((w<1)||(h<1)) return;
  
  // Operate piecemeal on the covered maps. There's usually at least two at a time.
  int cola=x/NS_sys_mapw;
  int colz=(x+w-1)/NS_sys_mapw;
  int rowa=y/NS_sys_maph;
  int rowz=(y+h-1)/NS_sys_maph;
  int row=rowa; for (;row<=rowz;row++) {
    const struct map *map=plane->v+row*plane->w+cola;
    int col=cola; for (;col<=colz;col++,map++) {
      if ((map->rid<1)||(map->rid>=g.spawner.spawnmapc)) continue;
      struct spawnmap *spawnmap=g.spawner.spawnmapv+map->rid;
      if (!spawnmap->wsum) continue;
      int subx=x-map->lng*NS_sys_mapw;
      int suby=y-map->lat*NS_sys_maph;
      spawner_expose_map(spawnmap,map,subx,suby,w,h);
    }
  }
}

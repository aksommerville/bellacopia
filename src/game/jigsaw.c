#include "game/game.h"
#include "game/jigsaw.h"
#include "game/world/camera.h"
#include "game/world/map.h"
#include "game/sprite/sprite.h"

#define NUB_SIZE 3
#define JTILESIZE 32 /* Must be at least (max(NS_sys_mapw,NS_sys_maph)+NUB_SIZE*2), ie 26. */
#define WLIMIT 12 /* We can afford 14. But 12 seems like plenty. */
#define HLIMIT 12 /* Would need some painful UI redesign to increase. */
#define X0 ((JTILESIZE>>1)-(NS_sys_mapw>>1)) /* Start position within a tile. Nubs go outside this. */
#define Y0 ((JTILESIZE>>1)-(NS_sys_maph>>1))
#define CHEER_TIME 30 /* Brief highlight after making a connection. */

// Assume little-endian, so they look ABGR. Need to rephrase these if we ever target a big-endian host, which I really don't expect ever.
#define COLOR_SOLID  0xff008000
#define COLOR_WATER  0xffff0000
#define COLOR_VACANT 0xffc0e0f0

/* Draw one map into an RGBA buffer.
 * Output has a constant stride (JTILESIZE*16*4), and dimensions (JTILESIZE,JTILESIZE).
 */
 
static void jigsaw_draw_map(uint32_t *dst,const struct map *map) {
  int stride=JTILESIZE<<4; // in pixels
  dst+=Y0*stride+X0;
  const uint8_t *src=map->v;
  int yi=NS_sys_maph;
  for (;yi-->0;dst+=stride) {
    uint32_t *dstp=dst;
    int xi=NS_sys_mapw;
    for (;xi-->0;dstp++,src++) {
      switch (map->physics[*src]) {
        case NS_physics_solid:
        case NS_physics_cliff:
        case NS_physics_hookable:
            *dstp=COLOR_SOLID;
            break;
        case NS_physics_water:
        case NS_physics_hole:
            *dstp=COLOR_WATER;
            break;
        case NS_physics_vacant:
        case NS_physics_safe:
        default:
            *dstp=COLOR_VACANT;
            break;
      }
    }
  }
}

/* Yoink a nub from cell (x,y) and paste it onto the neighbor at (-1,0) or (0,-1).
 * Or vice-versa.
 * But you always must provide the higher coordinates, the Right or Bottom of the pair.
 */
 
static int jigsaw_not_actually_random(int x,int y,const struct map *a,const struct map *b) {
  return a->rid*b->rid+y*12+x;
}

static void jigsaw_yoink_bits(
  uint32_t *dst,int dstx,int dsty,
  uint32_t *src,int srcx,int srcy,
  int w,int h
) {
  const int imgw=JTILESIZE<<4;
  dst+=dsty*imgw+dstx;
  src+=srcy*imgw+srcx;
  int yi=h;
  for (;yi-->0;dst+=imgw,src+=imgw) {
    uint32_t *dstp=dst;
    uint32_t *srcp=src;
    int xi=w;
    for (;xi-->0;dstp++,srcp++) {
      *dstp=*srcp;
      *srcp=0;
    }
  }
}
 
static void jigsaw_nubbinize_horz(uint32_t *img,struct jigsaw *jigsaw,int x,int y) {
  const int imgw=JTILESIZE<<4;
  struct map *mapr=jigsaw->mapv+y*jigsaw->mapstride+x;
  struct map *mapl=mapr-1;
  uint32_t *imgr=img+(y*imgw*JTILESIZE)+x*JTILESIZE;
  uint32_t *imgl=imgr-JTILESIZE;
  int seed=jigsaw_not_actually_random(x,y,mapl,mapr);
  int yrange=NS_sys_maph-(NUB_SIZE*3);
  if (yrange<1) yrange=1;
  int offy=NUB_SIZE+seed%yrange;
  seed/=yrange;
  if (seed&1) {
    jigsaw_yoink_bits(imgl,X0+NS_sys_mapw,Y0+offy,imgr,X0,Y0+offy,NUB_SIZE,NUB_SIZE);
  } else {
    jigsaw_yoink_bits(imgr,X0-NUB_SIZE,Y0+offy,imgl,X0+NS_sys_mapw-NUB_SIZE,Y0+offy,NUB_SIZE,NUB_SIZE);
  }
}

static void jigsaw_nubbinize_vert(uint32_t *img,struct jigsaw *jigsaw,int x,int y) {
  const int imgw=JTILESIZE<<4;
  struct map *mapb=jigsaw->mapv+y*jigsaw->mapstride+x;
  struct map *mapt=mapb-jigsaw->mapstride;
  uint32_t *imgb=img+(y*imgw*JTILESIZE)+x*JTILESIZE;
  uint32_t *imgt=imgb-JTILESIZE*imgw;
  int seed=jigsaw_not_actually_random(x,y,mapt,mapb);
  int xrange=NS_sys_mapw-(NUB_SIZE*3);
  if (xrange<1) xrange=1;
  int offx=NUB_SIZE+seed%xrange;
  seed/=xrange;
  if (seed&1) {
    jigsaw_yoink_bits(imgt,X0+offx,Y0+NS_sys_maph,imgb,X0+offx,Y0,NUB_SIZE,NUB_SIZE);
  } else {
    jigsaw_yoink_bits(imgb,X0+offx,Y0-NUB_SIZE,imgt,X0+offx,Y0+NS_sys_maph-NUB_SIZE,NUB_SIZE,NUB_SIZE);
  }
}

/* Wherever an opaque pixel has cardinal transparent neighbors, either lighten or darken it.
 * Transparent pixels must be natural zeroes.
 */
 
static uint32_t jigsaw_lighten(uint32_t abgr) {
  int b=(abgr>>16)&0xff;
  int g=(abgr>>8)&0xff;
  int r=abgr&0xff;
  b+=(0xff-b)>>2;
  g+=(0xff-g)>>2;
  r+=(0xff-r)>>2;
  return 0xff000000|(b<<16)|(g<<8)|r;
}

static uint32_t jigsaw_darken(uint32_t abgr) {
  int b=(abgr>>16)&0xff;
  int g=(abgr>>8)&0xff;
  int r=abgr&0xff;
  b-=b>>2;
  g-=g>>2;
  r-=r>>2;
  return 0xff000000|(b<<16)|(g<<8)|r;
}
 
static void jigsaw_shade_edges(uint32_t *img,int w,int h,int stridewords) {
  // Arrange not to visit the very edges. We shouldn't need to, and this lets us read all neighbors without a bounds check.
  img+=stridewords+1;
  w-=2;
  h-=2;
  for (;h-->0;img+=stridewords) {
    uint32_t *p=img;
    int xi=w;
    for (;xi-->0;p++) {
      if (!*p) continue;
      if (!p[-1]||!p[-stridewords]) *p=jigsaw_lighten(*p);
      else if (!p[1]||!p[stridewords]) *p=jigsaw_darken(*p);
    }
  }
}

/* More signal processing to generate the outlines image.
 */
 
static void jigsaw_trace_edges(uint32_t *img,int w,int h,int stridewords) {
  // I think* we won't need to touch the very edges, so do the same trick as jigsaw_shade_edges, to avoid bounds checking.
  // [*] As in, I'm pretty sure that 28 is less than 32.
  img+=stridewords+1;
  w-=2;
  h-=2;
  for (;h-->0;img+=stridewords) {
    uint32_t *p=img;
    int xi=w;
    for (;xi-->0;p++) {
      if (*p) continue;
      if (
        ((p[-1]!=0)&&(p[-1]!=0xffffffff))||
        ((p[1]!=0)&&(p[1]!=0xffffffff))||
        ((p[-stridewords]!=0)&&(p[-stridewords]!=0xffffffff))||
        ((p[stridewords]!=0)&&(p[stridewords]!=0xffffffff))
      ) *p=0xffffffff;
    }
  }
}

static void jigsaw_eliminate_colors(uint32_t *img,int w,int h,int stridewords) {
  for (;h-->0;img+=stridewords) {
    uint32_t *p=img;
    int xi=w;
    for (;xi-->0;p++) {
      if (*p!=0xffffffff) *p=0;
    }
  }
}

/* Initialize a new jigsaw, private.
 * (mapv,mapstride,colc,rowc) must have been set previously.
 * All tiles get generated, whether we need them or not.
 */
 
static int jigsaw_generate_puzzle(struct jigsaw *jigsaw) {
  if (!jigsaw->mapv||(jigsaw->colc<1)||(jigsaw->rowc<1)) return -1;
  
  // Allocate a tilesheet buffer, initially zeroed.
  int imgw=JTILESIZE<<4,imgh=JTILESIZE<<4;
  uint32_t *img=calloc(imgw<<2,imgh);
  if (!img) return -1;
  
  // Draw each map, each centered in one tile.
  struct map *maprow=jigsaw->mapv;
  uint32_t *dstrow=img;
  int yi=jigsaw->rowc;
  for (;yi-->0;maprow+=jigsaw->mapstride,dstrow+=imgw*JTILESIZE) {
    struct map *mapp=maprow;
    uint32_t *dstp=dstrow;
    int xi=jigsaw->colc;
    for (;xi-->0;mapp++,dstp+=JTILESIZE) {
      jigsaw_draw_map(dstp,mapp);
    }
  }
  
  /* Pull random scoops out of each edge and paste them on the neighbor, to produce the nubs.
   * Actually not random. Use some obscure function on the mapids or something, to ensure we get determinstic but unpredictable results.
   * It's important that we make the same nubbinning decisions every time, for a given map.
   */
  int x,y;
  for (x=1;x<jigsaw->colc;x++) {
    for (y=0;y<jigsaw->rowc;y++) {
      jigsaw_nubbinize_horz(img,jigsaw,x,y);
    }
  }
  for (y=1;y<jigsaw->rowc;y++) {
    for (x=0;x<jigsaw->colc;x++) {
      jigsaw_nubbinize_vert(img,jigsaw,x,y);
    }
  }
  
  // Apply shading.
  jigsaw_shade_edges(img,jigsaw->colc*JTILESIZE,jigsaw->rowc*JTILESIZE,imgw);
  
  // Upload to a new texture.
  jigsaw->texid=egg_texture_new();
  if (egg_texture_load_raw(jigsaw->texid,imgw,imgh,imgw<<2,img,imgw*imgh*4)<0) {
    free(img);
    return -1;
  }
  
  // Generate an outline texture too, using the same buffer.
  jigsaw->texid_outline=egg_texture_new();
  jigsaw_trace_edges(img,jigsaw->colc*JTILESIZE,jigsaw->rowc*JTILESIZE,imgw);
  jigsaw_eliminate_colors(img,jigsaw->colc*JTILESIZE,jigsaw->rowc*JTILESIZE,imgw);
  if (egg_texture_load_raw(jigsaw->texid_outline,imgw,imgh,imgw<<2,img,imgw*imgh*4)<0) {
    free(img);
    return -1;
  }
  
  free(img);
  return 0;
}

/* Get coords for the "current map".
 * This is going to be complicated: There will be one-off planes where we want to show the outer world instead.
 * On success we populate: mapv,mapstride,colc,rowc,fx,fy
 * We don't care at this point, whether the pieces have been found yet.
 */
 
static int jigsaw_acquire_current_position(struct jigsaw *jigsaw) {

  // Get the plane where the camera is currently focussed.
  int z=camera_get_z();
  int x=0,y=0,w=0,h=0,oobx=0,ooby=0;
  struct map *mapv=maps_get_plane(&x,&y,&w,&h,&oobx,&ooby,z);
  if (!mapv||(w<1)||(h<1)) {
    fprintf(stderr,"%s: Plane %d not found!\n",__func__,z);
    return -1;
  }
  int fx=x,fy=y;
  
  // If that plane has a parent, get the parent instead. Note that this is not in a loop. One level of redirection only.
  if (mapv->parent) {
    struct map *map=map_by_id(mapv->parent);
    if (!map) {
      fprintf(stderr,"map:%d not found, declared as parent of map:%d (plane %d)\n",mapv->parent,mapv->rid,z);
      return -1;
    }
    z=map->z;
    if (!(mapv=maps_get_plane(&x,&y,&w,&h,&oobx,&ooby,z))||(w<1)||(h<1)) {
      fprintf(stderr,"%s: Plane %d not found!\n",__func__,z);
      return -1;
    }
    fx=map->x;
    fy=map->y;
    
  // If the first plane did not have a parent, we should find the hero on it.
  } else {
    struct sprite *hero=sprites_get_hero();
    if (hero) {
      fx=(int)hero->x/NS_sys_mapw; if (hero->x<0.0) fx--;
      fy=(int)hero->y/NS_sys_maph; if (hero->y<0.0) fy--;
    } else {
      fx=x-1;
      fy=y-1;
    }
  }
  
  // Trim a one-map border for OOB modes "repeat" and "farloop". Use the whole axis for "null" or "loop".
  int stride=w;
  switch (oobx) {
    case NS_mapoob_repeat:
    case NS_mapoob_farloop: if (w>2) {
        x+=1;
        w-=2;
        mapv+=1;
      } break;
  }
  switch (ooby) {
    case NS_mapoob_repeat:
    case NS_mapoob_farloop: if (h>2) {
        y+=1;
        h-=2;
        mapv+=stride;
      } break;
  }
  if ((w<1)||(h<1)||(w>WLIMIT)||(h>HLIMIT)) {
    fprintf(stderr,"%s: Plane %d invalid size %dx%d.\n",__func__,z,w,h);
    return -1;
  }
  
  // OK, let's keep it.
  jigsaw->mapv=mapv;
  jigsaw->mapstride=stride;
  jigsaw->colc=w;
  jigsaw->rowc=h;
  jigsaw->fx=fx-x;
  jigsaw->fy=fy-y;
  
  return 0;
}

/* Tell the store about a changed jigpiece.
 */
 
static void jigsaw_dirty(struct jigsaw *jigsaw,struct jigpiece *jigpiece) {
  int col=jigpiece->tileid&15;
  int row=jigpiece->tileid>>4;
  if ((col<0)||(row<0)||(col>=jigsaw->colc)||(row>=jigsaw->rowc)) return;
  struct map *map=jigsaw->mapv+row*jigsaw->mapstride+col;
  store_jigsaw_set(map->rid,jigpiece->x,jigpiece->y,jigpiece->xform);
}

/* Cluster and jigpiece primitives.
 */
 
static struct jigpiece *jigsaw_jigpiece_by_tileid(struct jigsaw *jigsaw,uint8_t tileid) {
  struct jigpiece *jigpiece=jigsaw->jigpiecev;
  int i=jigsaw->jigpiecec;
  for (;i-->0;jigpiece++) {
    if (jigpiece->tileid==tileid) return jigpiece;
  }
  return 0;
}

static void jigsaw_neighbor_coords(int *x,int *y,int x0,int y0,int xform,int dx,int dy) {
  dx*=NS_sys_mapw;
  dy*=NS_sys_maph;
  if (xform&EGG_XFORM_XREV) dx=-dx;
  if (xform&EGG_XFORM_YREV) dy=-dy;
  if (xform&EGG_XFORM_SWAP) {
    int tmp=dx;
    dx=dy;
    dy=tmp;
  }
  *x=x0+dx;
  *y=y0+dy;
}

static uint8_t jigsaw_unused_clusterid(const struct jigsaw *jigsaw) {
  uint8_t bits[32]={0};
  bits[0]|=1;
  const struct jigpiece *jigpiece=jigsaw->jigpiecev;
  int i=jigsaw->jigpiecec;
  for (;i-->0;jigpiece++) {
    bits[jigpiece->clusterid>>3]|=1<<(jigpiece->clusterid&7);
  }
  int major=0;
  for (;major<32;major++) {
    uint8_t subbits=bits[major];
    if (subbits==0xff) continue;
    int minor=0;
    uint8_t submask=1;
    for (;;minor++,submask<<=1) {
      if (!(subbits&submask)) return (major<<3)|minor;
    }
  }
  return 0;
}

static void jigsaw_rename_cluster(struct jigsaw *jigsaw,uint8_t from,uint8_t to) {
  struct jigpiece *jigpiece=jigsaw->jigpiecev;
  int i=jigsaw->jigpiecec;
  for (;i-->0;jigpiece++) {
    if (jigpiece->clusterid==from) jigpiece->clusterid=to;
  }
}

static void jigsaw_move_cluster(struct jigsaw *jigsaw,uint8_t clusterid,int dx,int dy) {
  struct jigpiece *jigpiece=jigsaw->jigpiecev;
  int i=jigsaw->jigpiecec;
  for (;i-->0;jigpiece++) {
    if (jigpiece->clusterid==clusterid) {
      jigpiece->x+=dx;
      jigpiece->y+=dy;
      jigsaw_dirty(jigsaw,jigpiece);
    }
  }
}

static struct jigpiece *jigsaw_find_hover(const struct jigsaw *jigsaw,int x,int y) {
  // Search backward. Their natural order is render order.
  int i=jigsaw->jigpiecec;
  struct jigpiece *jigpiece=jigsaw->jigpiecev+i-1;
  for (;i-->0;jigpiece--) {
    int px=jigpiece->x;
    int py=jigpiece->y;
    int pw,ph;
    if (jigpiece->xform&EGG_XFORM_SWAP) {
      pw=NS_sys_maph;
      ph=NS_sys_mapw;
    } else {
      pw=NS_sys_mapw;
      ph=NS_sys_maph;
    }
    px-=pw>>1;
    py-=ph>>1;
    if (x<px) continue;
    if (y<py) continue;
    if (x>=px+pw) continue;
    if (y>=py+ph) continue;
    return jigpiece;
  }
  return 0;
}

static int jigsaw_clockwise(int xform) {
  switch (xform) {
    case 0: return EGG_XFORM_SWAP|EGG_XFORM_YREV;
    case EGG_XFORM_SWAP|EGG_XFORM_YREV: return EGG_XFORM_XREV|EGG_XFORM_YREV;
    case EGG_XFORM_XREV|EGG_XFORM_YREV: return EGG_XFORM_SWAP|EGG_XFORM_XREV;
    case EGG_XFORM_SWAP|EGG_XFORM_XREV: return 0;
  }
  return xform;
}

/* This replaces all (clusterid) by searching for neighbors.
 * Only neighbors in exactly the right position will join.
 */
 
static void jigsaw_detect_clusters_1(struct jigsaw *jigsaw,struct jigpiece *jigpiece) {
  if (jigpiece->clusterid) return; // Been here already.
  jigpiece->clusterid=0xff; // Mark visited temporarily. We'll update if a neighbor is detected.
  int col=jigpiece->tileid&0x0f;
  int row=jigpiece->tileid>>4;
  struct jigpiece *l=col?jigsaw_jigpiece_by_tileid(jigsaw,jigpiece->tileid-1):0;
  struct jigpiece *t=row?jigsaw_jigpiece_by_tileid(jigsaw,jigpiece->tileid-0x10):0;
  // No need to bounds-check r or b; we can't go above 12.
  struct jigpiece *r=jigsaw_jigpiece_by_tileid(jigsaw,jigpiece->tileid+1);
  struct jigpiece *b=jigsaw_jigpiece_by_tileid(jigsaw,jigpiece->tileid+0x10);
  // Drop any neighbor whose xform doesn't match, or which has an invalid clusterid. This is easy to check.
  if (l&&((l->clusterid==0xff)||(l->xform!=jigpiece->xform))) l=0;
  if (r&&((r->clusterid==0xff)||(r->xform!=jigpiece->xform))) r=0;
  if (t&&((t->clusterid==0xff)||(t->xform!=jigpiece->xform))) t=0;
  if (b&&((b->clusterid==0xff)||(b->xform!=jigpiece->xform))) b=0;
  // And if we're already neighborless, done.
  if (!l&&!r&&!t&&!b) return;
  // Then the painful bit, transform coordinates and check for exact matches.
  if (l) {
    int qx,qy;
    jigsaw_neighbor_coords(&qx,&qy,jigpiece->x,jigpiece->y,jigpiece->xform,-1,0);
    if ((l->x!=qx)||(l->y!=qy)) l=0;
  }
  if (r) {
    int qx,qy;
    jigsaw_neighbor_coords(&qx,&qy,jigpiece->x,jigpiece->y,jigpiece->xform,1,0);
    if ((r->x!=qx)||(r->y!=qy)) r=0;
  }
  if (t) {
    int qx,qy;
    jigsaw_neighbor_coords(&qx,&qy,jigpiece->x,jigpiece->y,jigpiece->xform,0,-1);
    if ((t->x!=qx)||(t->y!=qy)) t=0;
  }
  if (b) {
    int qx,qy;
    jigsaw_neighbor_coords(&qx,&qy,jigpiece->x,jigpiece->y,jigpiece->xform,0,1);
    if ((b->x!=qx)||(b->y!=qy)) b=0;
  }
  if (!l&&!r&&!t&&!b) return;
  // OK, the remaining neighbors are real, we need a clusterid. If any neighbor already has one, rename all of it.
  jigpiece->clusterid=jigsaw_unused_clusterid(jigsaw);
  if (l) {
    if (l->clusterid&&(l->clusterid!=0xff)) jigsaw_rename_cluster(jigsaw,l->clusterid,jigpiece->clusterid);
    else l->clusterid=jigpiece->clusterid;
  }
  if (r) {
    if (r->clusterid&&(r->clusterid!=0xff)) jigsaw_rename_cluster(jigsaw,r->clusterid,jigpiece->clusterid);
    else r->clusterid=jigpiece->clusterid;
  }
  if (t) {
    if (t->clusterid&&(t->clusterid!=0xff)) jigsaw_rename_cluster(jigsaw,t->clusterid,jigpiece->clusterid);
    else t->clusterid=jigpiece->clusterid;
  }
  if (b) {
    if (b->clusterid&&(b->clusterid!=0xff)) jigsaw_rename_cluster(jigsaw,b->clusterid,jigpiece->clusterid);
    else b->clusterid=jigpiece->clusterid;
  }
}
 
static void jigsaw_detect_clusters(struct jigsaw *jigsaw) {
  struct jigpiece *jigpiece;
  int i;
  for (jigpiece=jigsaw->jigpiecev,i=jigsaw->jigpiecec;i-->0;jigpiece++) jigpiece->clusterid=0;
  for (jigpiece=jigsaw->jigpiecev,i=jigsaw->jigpiecec;i-->0;jigpiece++) {
    if (jigpiece->clusterid) continue; // Already visited, no worries.
    jigsaw_detect_clusters_1(jigsaw,jigpiece);
  }
  for (jigpiece=jigsaw->jigpiecev,i=jigsaw->jigpiecec;i-->0;jigpiece++) if (jigpiece->clusterid==0xff) jigpiece->clusterid=0;
}

/* Generate pieces, with the maps and images already established.
 * (tho images aren't actually needed for this).
 */
 
static int jigsaw_generate_pieces(struct jigsaw *jigsaw) {

  // Allocate space for enough pieces to fill the grid. We won't necessarily use all of it.
  int piecea=jigsaw->colc*jigsaw->rowc;
  if (piecea<1) return -1;
  jigsaw->jigpiecec=0;
  if (jigsaw->jigpiecev) free(jigsaw->jigpiecev);
  if (!(jigsaw->jigpiecev=calloc(sizeof(struct jigpiece),piecea))) return -1;
  
  /* Fully-exposed and scattered map.
   * Enable this for testing or whatever, if you need to disregard the persistent state.
   * It's completely functional, but you get a new shuffle every time, which is extremely unhelpful in real life.
   */
  #if 0
    int row=0;
    for (;row<jigsaw->rowc;row++) {
      int col=0;
      for (;col<jigsaw->colc;col++) {
        struct jigpiece *jigpiece=jigsaw->jigpiecev+jigsaw->jigpiecec++;
        jigpiece->x=rand()%200; // aaaand of course, we don't know our bounds yet.
        jigpiece->y=rand()%100;
        jigpiece->tileid=(row<<4)|col;
        switch (rand()&3) {
          case 0: jigpiece->xform=0; break;
          case 1: jigpiece->xform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
          case 2: jigpiece->xform=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
          case 3: jigpiece->xform=EGG_XFORM_SWAP|EGG_XFORM_XREV; break;
        }
      }
    }
  
  /* The real thing: Read jigsaw state from the store.
   */
  #else
    struct map *mrow=jigsaw->mapv;
    int row=0;
    for (;row<jigsaw->rowc;row++,mrow+=jigsaw->mapstride) {
      struct map *map=mrow;
      int col=0;
      for (;col<jigsaw->colc;col++,map++) {
        int x=0,y=0;
        uint8_t xform=0;
        if (store_jigsaw_get(&x,&y,&xform,map->rid)<0) continue; // No such map. Maybe the plane is non-rectangular. (that's illegal but hey).
        if (xform==0xff) continue; // Piece has not been discovered yet. For our purposes then, it just doesn't exist.
        struct jigpiece *jigpiece=jigsaw->jigpiecev+jigsaw->jigpiecec++;
        jigpiece->x=x;
        jigpiece->y=y;
        jigpiece->tileid=(row<<4)|col;
        jigpiece->xform=xform;
      }
    }
  #endif
  
  // With the pieces made, determine which are already connected.
  jigsaw_detect_clusters(jigsaw);
   
  return 0;
}

/* Move a piece to the top.
 */
 
static struct jigpiece *jigsaw_to_top(struct jigsaw *jigsaw,struct jigpiece *jigpiece) {
  int p=jigpiece-jigsaw->jigpiecev;
  if ((p<0)||(p>=jigsaw->jigpiecec)) return jigpiece; // Invalid.
  struct jigpiece *npiece=jigsaw->jigpiecev+jigsaw->jigpiecec-1;
  if (jigpiece==npiece) return jigpiece; // Already there.
  struct jigpiece tmp=*jigpiece;
  memmove(jigpiece,jigpiece+1,sizeof(struct jigpiece)*(jigsaw->jigpiecec-p-1));
  *npiece=tmp;
  return npiece;
}

/* Scan for possible new connections to the given piece.
 * May move pieces around and alter clusterid. Will not change transforms or order.
 * Returns nonzero if anything connected.
 */
 
static int jigsaw_check_connections(struct jigsaw *jigsaw,struct jigpiece *jigpiece) {

  // First find the 4 candidate pieces by tileid.
  int col=jigpiece->tileid&15;
  int row=jigpiece->tileid>>4;
  struct jigpiece *l=col?jigsaw_jigpiece_by_tileid(jigsaw,jigpiece->tileid-1):0;
  struct jigpiece *t=row?jigsaw_jigpiece_by_tileid(jigsaw,jigpiece->tileid-0x10):0;
  struct jigpiece *r=jigsaw_jigpiece_by_tileid(jigsaw,jigpiece->tileid+1);
  struct jigpiece *b=jigsaw_jigpiece_by_tileid(jigsaw,jigpiece->tileid+0x10);
  
  // Eliminate if xforms mismatch.
  if (l&&(l->xform!=jigpiece->xform)) l=0;
  if (t&&(t->xform!=jigpiece->xform)) t=0;
  if (r&&(r->xform!=jigpiece->xform)) r=0;
  if (b&&(b->xform!=jigpiece->xform)) b=0;
  
  // Eliminate if we have the same nonzero clusterid. We're already connected.
  if (jigpiece->clusterid) {
    if (l&&(l->clusterid==jigpiece->clusterid)) l=0;
    if (r&&(r->clusterid==jigpiece->clusterid)) r=0;
    if (t&&(t->clusterid==jigpiece->clusterid)) t=0;
    if (b&&(b->clusterid==jigpiece->clusterid)) b=0;
  }
  if (!l&&!r&&!t&&!b) return 0;
  
  // Then the tricky bit. Find the ideal position for each neighbor, and if it's within some tolerance of that position by Manhattan distance, connect.
  int result=0;
  const int tolerance=7; // Arbitrary.
  #define CK(name,kdx,kdy) if (name) { \
    int qx,qy; \
    jigsaw_neighbor_coords(&qx,&qy,jigpiece->x,jigpiece->y,jigpiece->xform,kdx,kdy); \
    int dx=name->x-qx; \
    int dy=name->y-qy; \
    int d=((dx<0)?-dx:dx)+((dy<0)?-dy:dy); \
    if (d<=tolerance) { \
      result=1; \
      if (dx||dy) { \
        if (jigpiece->clusterid) { \
          jigsaw_move_cluster(jigsaw,jigpiece->clusterid,dx,dy); \
        } else { \
          jigpiece->clusterid=jigsaw_unused_clusterid(jigsaw); \
          jigpiece->x+=dx; \
          jigpiece->y+=dy; \
          jigsaw_dirty(jigsaw,jigpiece); \
        } \
      } else if (!jigpiece->clusterid) { \
        jigpiece->clusterid=jigsaw_unused_clusterid(jigsaw); \
      } \
      if (name->clusterid) jigsaw_rename_cluster(jigsaw,name->clusterid,jigpiece->clusterid); \
      else name->clusterid=jigpiece->clusterid; \
    } \
  }
  CK(l,-1,0)
  CK(r,1,0)
  CK(t,0,-1)
  CK(b,0,1)
  #undef CK
  
  if (result) {
    jigsaw->cheerclock=CHEER_TIME;
    jigsaw->cheercluster=jigpiece->clusterid;
    return 1;
  }
  return 0;
}

/* Check for connections, visiting every member of this piece's cluster if needed.
 */
 
static int jigsaw_check_connections_all(struct jigsaw *jigsaw,struct jigpiece *jigpiece) {
  if (jigpiece->clusterid) {
    int result=0;
    uint8_t clusterid=jigpiece->clusterid;
    int i=jigsaw->jigpiecec;
    for (jigpiece=jigsaw->jigpiecev;i-->0;jigpiece++) {
      if (jigpiece->clusterid!=clusterid) continue;
      if (jigsaw_check_connections(jigsaw,jigpiece)) result=1;
    }
    return result;
  } else {
    return jigsaw_check_connections(jigsaw,jigpiece);
  }
}

/* Public API.
 **************************************************************************************************************/

/* Cleanup.
 */
 
void jigsaw_cleanup(struct jigsaw *jigsaw) {
  if (jigsaw->texid) {
    egg_texture_del(jigsaw->texid);
    jigsaw->texid=0;
  }
  if (jigsaw->texid_outline) {
    egg_texture_del(jigsaw->texid_outline);
    jigsaw->texid_outline=0;
  }
  if (jigsaw->jigpiecev) {
    free(jigsaw->jigpiecev);
    jigsaw->jigpiecev=0;
    jigsaw->jigpiecec=0;
  }
  jigsaw->hover=0;
  jigsaw->grab=0;
  jigsaw->colc=0;
  jigsaw->rowc=0;
}

/* Initialize.
 * We must noop on reinitialization.
 */
 
int jigsaw_require(struct jigsaw *jigsaw) {
  if (!jigsaw->texid) {
    if (jigsaw_acquire_current_position(jigsaw)<0) return -1;
    if (jigsaw_generate_puzzle(jigsaw)<0) return -1;
    if (jigsaw_generate_pieces(jigsaw)<0) return -1;
  }
  return 0;
}

/* Set bounds in framebuffer.
 */

void jigsaw_set_bounds(struct jigsaw *jigsaw,int x,int y,int w,int h) {
  if ((x==jigsaw->ox)&&(y==jigsaw->oy)&&(w==jigsaw->ow)&&(h==jigsaw->oh)) return;
  
  jigsaw->ox=x;
  jigsaw->oy=y;
  jigsaw->ow=w;
  jigsaw->oh=h;
  
  struct jigpiece *jigpiece=jigsaw->jigpiecev;
  int i=jigsaw->jigpiecec;
  for (;i-->0;jigpiece++) {
    if (jigpiece->x>=w) jigpiece->x=w-1;
    if (jigpiece->y>=h) jigpiece->y=h-1;
  }
}

/* Render.
 */
 
void jigsaw_render(struct jigsaw *jigsaw) {

  /* Get out fast if our outer bounds are empty or offscreen.
   * In the pause modal, we will be called often when entirely offscreen, when the map page is showing just its edge.
   */
  if ((jigsaw->ow<1)||(jigsaw->oh<1)) return;
  if (jigsaw->ox>=FBW) return;
  if (jigsaw->oy>=FBH) return;
  if (jigsaw->ox<=-jigsaw->ow) return;
  if (jigsaw->oy<=-jigsaw->oh) return;
  
  if (jigsaw->cheerclock>0) jigsaw->cheerclock--;
  
  // Draw the pieces.
  graf_set_input(&g.graf,jigsaw->texid);
  struct jigpiece *jigpiece=jigsaw->jigpiecev;
  int i=jigsaw->jigpiecec;
  for (;i-->0;jigpiece++) {
    if (jigsaw->cheerclock&&(jigpiece->clusterid==jigsaw->cheercluster)) graf_set_tint(&g.graf,0x00ff0080);
    graf_tile(&g.graf,jigsaw->ox+jigpiece->x,jigsaw->oy+jigpiece->y,jigpiece->tileid,jigpiece->xform);
    if (jigsaw->cheerclock&&(jigpiece->clusterid==jigsaw->cheercluster)) graf_set_tint(&g.graf,0);
  }
  
  // Outline the grab or hover piece. This outline intentionally renders above any pieces occluding the focussed one.
  if (jigsaw->grab) {
    graf_set_input(&g.graf,jigsaw->texid_outline);
    graf_set_tint(&g.graf,0x00ffffff);
    graf_tile(&g.graf,jigsaw->ox+jigsaw->grab->x,jigsaw->oy+jigsaw->grab->y,jigsaw->grab->tileid,jigsaw->grab->xform);
    graf_set_tint(&g.graf,0);
  } else if (jigsaw->hover) {
    graf_set_input(&g.graf,jigsaw->texid_outline);
    graf_set_tint(&g.graf,0x0080ffff);
    graf_tile(&g.graf,jigsaw->ox+jigsaw->hover->x,jigsaw->oy+jigsaw->hover->y,jigsaw->hover->tileid,jigsaw->hover->xform);
    graf_set_tint(&g.graf,0);
  }
  
  //TODO Other highlights? This definitely will come up eventually, the whole point of a map is we can show "treasure here!" indicators on it.
}

/* Test grab.
 */

int jigsaw_is_grabbed(const struct jigsaw *jigsaw) {
  if (jigsaw->grab) return 1;
  return 0;
}

/* Grab.
 */

void jigsaw_grab(struct jigsaw *jigsaw) {
  if (jigsaw->grab) return;
  if (!jigsaw->hover) return;
  jigsaw->grab=jigsaw_to_top(jigsaw,jigsaw->hover);
  jigsaw->hover=0;
  bm_sound(RID_sound_jigsaw_grab,0.0);
}

/* Release.
 */
 
void jigsaw_release(struct jigsaw *jigsaw) {
  if (!jigsaw->grab) return;
  jigsaw->hover=jigsaw->grab;
  jigsaw->grab=0;
  if (jigsaw_check_connections_all(jigsaw,jigsaw->hover)) {
    bm_sound(RID_sound_jigsaw_connect,0.0);
  } else {
    bm_sound(RID_sound_jigsaw_drop,0.0);
  }
}

/* Rotate.
 */
 
void jigsaw_rotate(struct jigsaw *jigsaw) {
  struct jigpiece *jigpiece=0;
  if (jigsaw->grab) jigpiece=jigsaw->grab;
  else if (jigsaw->hover) jigpiece=jigsaw->hover;
  else return;
  jigpiece->xform=jigsaw_clockwise(jigpiece->xform);
  jigsaw_dirty(jigsaw,jigpiece);
  if (jigpiece->clusterid) {
    int col=jigpiece->tileid&15;
    int row=jigpiece->tileid>>4;
    struct jigpiece *other=jigsaw->jigpiecev;
    int i=jigsaw->jigpiecec;
    for (;i-->0;other++) {
      if (other->clusterid!=jigpiece->clusterid) continue;
      if (other==jigpiece) continue;
      other->xform=jigpiece->xform;
      // It's a little simpler to recalculate the transformed coords from scratch, based on tileids.
      int ocol=other->tileid&15;
      int orow=other->tileid>>4;
      jigsaw_neighbor_coords(&other->x,&other->y,jigpiece->x,jigpiece->y,jigpiece->xform,ocol-col,orow-row);
      jigsaw_dirty(jigsaw,other);
    }
  }
  bm_sound(RID_sound_jigsaw_rotate,0.0);
}

/* Motion.
 */
 
void jigsaw_motion(struct jigsaw *jigsaw,int x,int y) {
  int dx=x-jigsaw->mx;
  int dy=y-jigsaw->my;
  if (!dx&&!dy) return;
  jigsaw->mx=x;
  jigsaw->my=y;
  if (jigsaw->grab) {
    if (jigsaw->grab->clusterid) {
      struct jigpiece *jigpiece=jigsaw->jigpiecev;
      int i=jigsaw->jigpiecec;
      for (;i-->0;jigpiece++) {
        if (jigpiece->clusterid!=jigsaw->grab->clusterid) continue;
        jigpiece->x+=dx;
        jigpiece->y+=dy;
        jigsaw_dirty(jigsaw,jigpiece);
      }
    } else {
      jigsaw->grab->x+=dx;
      jigsaw->grab->y+=dy;
      jigsaw_dirty(jigsaw,jigsaw->grab);
    }
  } else {
    jigsaw->hover=jigsaw_find_hover(jigsaw,x-jigsaw->ox,y-jigsaw->oy);
  }
}

#include "game/bellacopia.h"
#include "jigsaw.h"

#define NUB_SIZE 3
#define JTILESIZE 32 /* Must be at least (max(NS_sys_mapw,NS_sys_maph)+NUB_SIZE*2), ie 26. */
#define WLIMIT 12 /* We can afford 14. But 12 seems like plenty. */
#define HLIMIT 12 /* Would need some painful UI redesign to increase. */
#define X0 ((JTILESIZE>>1)-(NS_sys_mapw>>1)) /* Start position within a tile. Nubs go outside this. */
#define Y0 ((JTILESIZE>>1)-(NS_sys_maph>>1))
#define CHEER_TIME 20 /* Brief highlight after making a connection. */

/* 32-bit pixel from jigctab's rgb332.
 * If we're going to support big-endian hosts, need to manage that here.
 */
 
static uint32_t jigsaw_pixel_from_rgb332(uint8_t src) {
  uint8_t r=src&0xe0; r|=r>>3; r|=r>>6;
  uint8_t g=src&0x1c; g|=g<<3; g|=g>>6;
  uint8_t b=src&0x03; b|=b<<2; b|=b<<4;
  return r|(g<<8)|(b<<16)|0xff000000;
}

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
      *dstp=jigsaw_pixel_from_rgb332(map->jigctab[*src]);
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
  struct map *mapr=jigsaw->plane->v+y*jigsaw->plane->w+x;
  struct map *mapl=mapr-1;
  if (!mapr->rid||!mapl->rid) return;
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
  struct map *mapb=jigsaw->plane->v+y*jigsaw->plane->w+x;
  struct map *mapt=mapb-jigsaw->plane->w;
  if (!mapb->rid||!mapt->rid) return;
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
  
  // Allocate a tilesheet buffer, initially zeroed.
  int imgw=JTILESIZE<<4,imgh=JTILESIZE<<4;
  uint32_t *img=calloc(imgw<<2,imgh);
  if (!img) return -1;
  
  // Draw each map, each centered in one tile.
  struct map *maprow=jigsaw->plane->v;
  uint32_t *dstrow=img;
  int yi=jigsaw->plane->h;
  for (;yi-->0;maprow+=jigsaw->plane->w,dstrow+=imgw*JTILESIZE) {
    struct map *mapp=maprow;
    uint32_t *dstp=dstrow;
    int xi=jigsaw->plane->w;
    for (;xi-->0;mapp++,dstp+=JTILESIZE) {
      map_freshen_tiles(mapp,0);
      jigsaw_draw_map(dstp,mapp);
    }
  }
  
  /* Pull random scoops out of each edge and paste them on the neighbor, to produce the nubs.
   * Actually not random. Use some obscure function on the mapids or something, to ensure we get determinstic but unpredictable results.
   * It's important that we make the same nubbinning decisions every time, for a given map.
   */
  int x,y;
  for (x=1;x<jigsaw->plane->w;x++) {
    for (y=0;y<jigsaw->plane->h;y++) {
      jigsaw_nubbinize_horz(img,jigsaw,x,y);
    }
  }
  for (y=1;y<jigsaw->plane->h;y++) {
    for (x=0;x<jigsaw->plane->w;x++) {
      jigsaw_nubbinize_vert(img,jigsaw,x,y);
    }
  }
  
  // Apply shading.
  jigsaw_shade_edges(img,jigsaw->plane->w*JTILESIZE,jigsaw->plane->h*JTILESIZE,imgw);
  
  // Upload to a new texture.
  jigsaw->texid=egg_texture_new();
  if (egg_texture_load_raw(jigsaw->texid,imgw,imgh,imgw<<2,img,imgw*imgh*4)<0) {
    free(img);
    return -1;
  }
  
  // Generate an outline texture too, using the same buffer.
  jigsaw->texid_outline=egg_texture_new();
  jigsaw_trace_edges(img,jigsaw->plane->w*JTILESIZE,jigsaw->plane->h*JTILESIZE,imgw);
  jigsaw_eliminate_colors(img,jigsaw->plane->w*JTILESIZE,jigsaw->plane->h*JTILESIZE,imgw);
  if (egg_texture_load_raw(jigsaw->texid_outline,imgw,imgh,imgw<<2,img,imgw*imgh*4)<0) {
    free(img);
    return -1;
  }
  
  free(img);
  return 0;
}

/* Get coords for the "current map".
 * If the camera's focus is not on the plane we were provided, no worries, we just won't focus a piece.
 * Populates (fx,fy).
 */
 
static int jigsaw_acquire_current_position(struct jigsaw *jigsaw) {
  jigsaw->fx=-1;
  jigsaw->fy=-1;
  struct map *map=g.camera.map;
  if (map&&map->parent) map=map_by_id(map->parent);
  if (!map) return 0;
  if (map->z!=jigsaw->plane->z) return 0;
  jigsaw->fx=map->lng;
  jigsaw->fy=map->lat;
  return 0;
}

/* Tell the store about a changed jigpiece.
 */
 
static void jigsaw_dirty(struct jigsaw *jigsaw,struct jigpiece *jigpiece) {
  int col=jigpiece->tileid&15;
  int row=jigpiece->tileid>>4;
  if ((col<0)||(row<0)||(col>=jigsaw->plane->w)||(row>=jigsaw->plane->h)) return;
  struct map *map=jigsaw->plane->v+row*jigsaw->plane->w+col;
  struct jigstore *jigstore=store_get_jigstore(map->rid);
  if (!jigstore) return;
  jigstore->x=jigpiece->x;
  jigstore->y=jigpiece->y;
  jigstore->xform=jigpiece->xform;
  g.store.dirty=1;
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
  // OK, the remaining neighbors are real, we need a clusterid.
  if (!jigpiece->clusterid) {
    jigpiece->clusterid=jigsaw_unused_clusterid(jigsaw);
  }
  // Neighbors already marked as this cluster are ok, drop them.
  if (l&&(l->clusterid==jigpiece->clusterid)) l=0;
  if (r&&(r->clusterid==jigpiece->clusterid)) r=0;
  if (t&&(t->clusterid==jigpiece->clusterid)) t=0;
  if (b&&(b->clusterid==jigpiece->clusterid)) b=0;
  // If a neighbor already has a clusterid, rename that whole cluster. Otherwise, assign it simply to ours.
  if (l) {
    if (l->clusterid) jigsaw_rename_cluster(jigsaw,l->clusterid,jigpiece->clusterid);
    else l->clusterid=jigpiece->clusterid;
  }
  if (r) {
    if (r->clusterid) jigsaw_rename_cluster(jigsaw,r->clusterid,jigpiece->clusterid);
    else r->clusterid=jigpiece->clusterid;
  }
  if (t) {
    if (t->clusterid) jigsaw_rename_cluster(jigsaw,t->clusterid,jigpiece->clusterid);
    else t->clusterid=jigpiece->clusterid;
  }
  if (b) {
    if (b->clusterid) jigsaw_rename_cluster(jigsaw,b->clusterid,jigpiece->clusterid);
    else b->clusterid=jigpiece->clusterid;
  }
}
 
static void jigsaw_detect_clusters(struct jigsaw *jigsaw) {
  struct jigpiece *jigpiece;
  int i;
  for (jigpiece=jigsaw->jigpiecev,i=jigsaw->jigpiecec;i-->0;jigpiece++) jigpiece->clusterid=0;
  for (jigpiece=jigsaw->jigpiecev,i=jigsaw->jigpiecec;i-->0;jigpiece++) {
    jigsaw_detect_clusters_1(jigsaw,jigpiece);
  }
}

/* Tileid in RID_image_pause if this map deserves a blinking indicator.
 * Zero means none; that's not a tile you'd want.
 * Each jigpiece can have no more than one indicator.
 * We're not choosing the hero indicator here; it's more convenient to do that once at the end.
 */
 
static uint8_t jigsaw_choose_indicator(const struct map *map) {
  //TODO indicators?
  return 0;
}

/* Generate pieces, with the maps and images already established.
 * (tho images aren't actually needed for this).
 */
 
static int jigsaw_generate_pieces(struct jigsaw *jigsaw) {

  // Allocate space for enough pieces to fill the grid. We won't necessarily use all of it.
  int piecea=jigsaw->plane->w*jigsaw->plane->h;
  if (piecea<1) return -1;
  jigsaw->jigpiecec=0;
  if (jigsaw->jigpiecev) free(jigsaw->jigpiecev);
  if (!(jigsaw->jigpiecev=calloc(sizeof(struct jigpiece),piecea))) return -1;
  
  /* Generate a piece for each appropriate map.
   */
  jigsaw->total=0;
  struct map *mrow=jigsaw->plane->v;
  int row=0;
  for (;row<jigsaw->plane->h;row++,mrow+=jigsaw->plane->w) {
    struct map *map=mrow;
    int col=0;
    for (;col<jigsaw->plane->w;col++,map++) {
      int x=0,y=0;
      uint8_t xform=0;
      if (!map->rid) continue;
      jigsaw->total++;
      struct jigstore *jigstore=store_get_jigstore(map->rid);
      if (!jigstore) continue;
      x=jigstore->x;
      y=jigstore->y;
      xform=jigstore->xform;
      if (xform==0xff) continue; // Piece has not been discovered yet. For our purposes then, it just doesn't exist.
      struct jigpiece *jigpiece=jigsaw->jigpiecev+jigsaw->jigpiecec++;
      jigpiece->x=x;
      jigpiece->y=y;
      jigpiece->tileid=(row<<4)|col;
      jigpiece->xform=xform;
      jigpiece->indicator=jigsaw_choose_indicator(map);
    }
  }
  
  /* If we have the currently-focussed piece, show the hero indicator on it.
   */
  if ((jigsaw->fx>=0)&&(jigsaw->fy>=0)&&(jigsaw->fx<jigsaw->plane->w)&&(jigsaw->fy<jigsaw->plane->h)) {
    uint8_t tileid=(jigsaw->fy<<4)|jigsaw->fx;
    struct jigpiece *jigpiece=jigsaw_jigpiece_by_tileid(jigsaw,tileid);
    if (jigpiece) {
      jigpiece->indicator=0x26;
    }
  }
  
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

/* If any piece is out of bounds, move its entire cluster to get in bounds.
 * This should be called after each move.
 */
 
static void jigsaw_force_legal_positions(struct jigsaw *jigsaw) {
  struct jigpiece *jigpiece=jigsaw->jigpiecev;
  int i=jigsaw->jigpiecec;
  for (;i-->0;jigpiece++) {
    int dx=0,dy=0;
    if (jigpiece->x<0) dx=-jigpiece->x;
    else if (jigpiece->x>jigsaw->ow) dx=jigsaw->ow-jigpiece->x;
    if (jigpiece->y<0) dy=-jigpiece->y;
    else if (jigpiece->y>jigsaw->oh) dy=jigsaw->oh-jigpiece->y;
    if (!dx&&!dy) continue;
    if (jigpiece->clusterid) {
      struct jigpiece *other=jigsaw->jigpiecev;
      int oi=jigsaw->jigpiecec;
      for (;oi-->0;other++) {
        if (jigpiece->clusterid!=other->clusterid) continue;
        other->x+=dx;
        other->y+=dy;
        jigsaw_dirty(jigsaw,other);
      }
    } else {
      jigpiece->x+=dx;
      jigpiece->y+=dy;
      jigsaw_dirty(jigsaw,jigpiece);
    }
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
  jigsaw->plane=0;
}

/* Initialize.
 * We must noop on reinitialization.
 */
 
int jigsaw_require(struct jigsaw *jigsaw,int z) {
  if (!(jigsaw->plane=plane_by_position(z))) {
    fprintf(stderr,"%s: Plane %d not found!\n",__func__,z);
    return -1;
  }
  if (!jigsaw->plane->v||(jigsaw->plane->w<1)||(jigsaw->plane->h<1)) {
    fprintf(stderr,"%s: Invalid plane %d.\n",__func__,z);
    return -1;
  }
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
  if (w>256) {
    x+=(w-256)>>1;
    w=256;
  }
  if (h>256) {
    y+=(h-256)>>1;
    h=256;
  }
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
  jigsaw->blinkclock++;
  int blink=(jigsaw->blinkclock&32);
  
  // Draw the pieces.
  struct jigpiece *jigpiece=jigsaw->jigpiecev;
  int i=jigsaw->jigpiecec;
  for (;i-->0;jigpiece++) {
    graf_set_input(&g.graf,jigsaw->texid);
    if (jigsaw->cheerclock&&(jigpiece->clusterid==jigsaw->cheercluster)) graf_set_tint(&g.graf,0x00ff0080);
    graf_tile(&g.graf,jigsaw->ox+jigpiece->x,jigsaw->oy+jigpiece->y,jigpiece->tileid,jigpiece->xform);
    if (jigsaw->cheerclock&&(jigpiece->clusterid==jigsaw->cheercluster)) graf_set_tint(&g.graf,0);
    if (blink&&jigpiece->indicator) {
      graf_set_image(&g.graf,RID_image_pause);
      graf_tile(&g.graf,jigsaw->ox+jigpiece->x,jigsaw->oy+jigpiece->y,jigpiece->indicator,jigpiece->xform);
    }
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
  bm_sound(RID_sound_jigsaw_grab);
}

/* Release.
 */
 
void jigsaw_release(struct jigsaw *jigsaw) {
  if (!jigsaw->grab) return;
  jigsaw->hover=jigsaw->grab;
  jigsaw->grab=0;
  if (jigsaw_check_connections_all(jigsaw,jigsaw->hover)) {
    bm_sound(RID_sound_jigsaw_connect);
  } else {
    bm_sound(RID_sound_jigsaw_drop);
  }
  jigsaw_force_legal_positions(jigsaw);
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
  bm_sound(RID_sound_jigsaw_rotate);
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

/* Test plane completeness.
 */
 
static int jigsaw_plane_is_complete_inner(struct jigsaw *jigsaw,int z) {
  if (!(jigsaw->plane=plane_by_position(z))) return 0;
  if (!jigsaw->plane->v||(jigsaw->plane->w<1)||(jigsaw->plane->h<1)) return 0;
  if (jigsaw_generate_pieces(jigsaw)<0) return 0;
  if (jigsaw->jigpiecec<1) return 0;
  if (jigsaw->jigpiecec<jigsaw->total) return 0;
  // More than one clusterid, or a zero, means it's incomplete.
  const struct jigpiece *jigpiece=jigsaw->jigpiecev;
  int i=jigsaw->jigpiecec;
  for (;i-->0;jigpiece++) {
    if (jigpiece->xform) return 0;
    if (!jigpiece->clusterid) return 0;
    if (jigpiece->clusterid!=jigsaw->jigpiecev[0].clusterid) return 0;
  }
  return 1;
}
 
int jigsaw_plane_is_complete(int z) {
  struct jigsaw jigsaw={0};
  int result=jigsaw_plane_is_complete_inner(&jigsaw,z);
  jigsaw_cleanup(&jigsaw);
  return result;
}

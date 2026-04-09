#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include "opt/image/image.h"

/* Return a pixel similar to the input but at least some distance away.
 */
 
static uint32_t close_but_not_too_close(uint32_t src) {
  const int dlo= 8;
  const int dhi=16;
  int d=dlo+(rand()%(dhi-dlo+1));
  if (rand()&1) d=-d;
  int shift=0;
  for (;shift<24;shift+=8) { // sic <. Don't operate on the MSB, which is alpha because we're little-endian.
    int v=(src>>shift)&0xff;
    v+=d;
    if (v<0) v=0;
    else if (v>0xff) v=0xff;
    src=(src&~(0xff<<shift))|(v<<shift);
  }
  return src;
}

/* Fill rectangle.
 * Note that (pixel) is in the natural byte order.
 * I'm going to assume that the running host is always little-endian.
 * (img) stride must be (imgw*4).
 */
 
static void fill_rect(uint32_t *img,int imgw,int imgh,int x,int y,int w,int h,uint32_t pixel) {
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>imgw-w) w=imgw-x;
  if (y>imgh-h) h=imgh-y;
  if ((w<1)||(h<1)) return;
  img+=y*imgw+x;
  for (;h-->0;img+=imgw) {
    uint32_t *p=img;
    int xi=w;
    for (;xi-->0;p++) *p=pixel;
  }
}

/* Zero if there's at least one pixel in this rect with nonzero alpha.
 */
 
static int rect_is_transparent(const uint32_t *img,int imgw,int imgh,int x,int y,int w,int h) {
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>imgw-w) w=imgw-x;
  if (y>imgh-h) h=imgh-y;
  if ((w<1)||(h<1)) return 0;
  uint32_t alpha=0xff000000;
  img+=y*imgw+x;
  for (;h-->0;img+=imgw) {
    const uint32_t *p=img;
    int xi=w;
    for (;xi-->0;p++) {
      if ((*p)&alpha) return 0;
    }
  }
  return 1;
}

/* Bring in the edges of a bounding box to eliminate transparent edges.
 * NB We expect (l,r,t,b) not (x,y,w,h).
 */
 
static void inset_box(const uint32_t *img,int imgw,int imgh,int *l,int *r,int *t,int *b) {
  if (*l<0) *l=0;
  if (*t<0) *t=0;
  if (*r>imgw) *r=imgw;
  if (*b>imgh) *b=imgh;
  while (*t<*b) {
    int progress=0;
    if (rect_is_transparent(img,imgw,imgh,*l,*t,(*r)-(*l),1)) { progress=1; (*t)++; if (*t>=*b) break; }
    if (rect_is_transparent(img,imgh,imgh,*l,(*b)-1,(*r)-(*l),1)) { progress=1; (*b)--; }
    if (!progress) break;
  }
  if (*t>=*b) return;
  while (*l<*r) {
    int progress=0;
    if (rect_is_transparent(img,imgw,imgh,*l,*t,1,(*b)-(*t))) { progress=1; (*l)++; if (*l>=*r) break; }
    if (rect_is_transparent(img,imgw,imgh,(*r)-1,*t,1,(*b)-(*t))) { progress=1; (*r)--; }
    if (!progress) break;
  }
}

/* Copy a portion of the image onto itself, replacing all opaque pixels with the given pixel.
 * Intermediate alpha are treated as opaque. We could smarten up about that, but my images won't have any.
 * We crop both sides.
 * If the two rects overlap, behavior is undefined.
 */
 
static void blit_self_recolor(
  uint32_t *img,int imgw,int imgh,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  uint32_t pixel
) {
  if (dstx<0) { srcx-=dstx; w+=dstx; dstx=0; }
  if (dsty<0) { srcy-=dsty; h+=dsty; dsty=0; }
  if (srcx<0) { dstx-=srcx; w+=srcx; srcx=0; }
  if (srcy<0) { dsty-=srcy; h+=srcy; srcy=0; }
  if (dstx>imgw-w) w=imgw-dstx;
  if (dsty>imgh-h) h=imgh-dsty;
  if (srcx>imgw-w) w=imgw-srcx;
  if (srcy>imgh-h) h=imgh-srcy;
  if ((w<1)||(h<1)) return;
  const uint32_t alpha=0xff000000;
  uint32_t *dstrow=img+dsty*imgw+dstx;
  const uint32_t *srcrow=img+srcy*imgw+srcx;
  int yi=h;
  for (;yi-->0;dstrow+=imgw,srcrow+=imgw) {
    uint32_t *dstp=dstrow;
    const uint32_t *srcp=srcrow;
    int xi=w;
    for (;xi-->0;dstp++,srcp++) {
      if ((*srcp)&alpha) *dstp=pixel;
    }
  }
}

/* Animated grass for the Lawnmowing Contest.
 * XXX This is written and works, but now I'm thinking that random is not the way to go about it.
 */

static int lawnmowing_generate_animated_grass(uint32_t *pixels,int w,int h,const char *path) {
  if ((w!=256)||(h!=256)) {
    fprintf(stderr,"%s: Expected 256x256, found %dx%d\n",path,w,h);
    return -2;
  }
  const int tilesize=16;
  
  // The first pixel is our background color.
  uint32_t bgcolor=pixels[0];
  
  /* Rows 1,2,3,4 contain 8 tiles each, each representing one blade of grass.
   * They are each a small sub-image. Determine the bounds of each.
   * Each row has a source box, same across all its frames.
   * We don't need any per-frame bookkeeping.
   */
  struct face {
    int l,r,t,b; // Maximum bounds across all frames, within the tile.
    int x,y,w,h; // The same, rephrased.
    uint8_t tileid; // +0..7
  } facev[4]={0};
  int shortest=tilesize; // Height of the shortest face.
  struct face *face=facev;
  uint8_t tileid=0x18;
  int i=4,srcy=tilesize;
  for (;i-->0;face++,tileid+=0x10,srcy+=tilesize) {
    face->tileid=tileid;
    // Face starts with invalid inside-out bounds.
    face->l=tilesize;
    face->r=0;
    face->t=tilesize;
    face->b=0;
    int frame=0,srcx=tilesize*8;
    for (;frame<8;frame++,srcx+=tilesize) {
      int l=srcx,t=srcy;
      int r=l+tilesize,b=t+tilesize;
      inset_box(pixels,w,h,&l,&r,&t,&b);
      l-=srcx;
      r-=srcx;
      t-=srcy;
      b-=srcy;
      if (l<face->l) face->l=l;
      if (r>face->r) face->r=r;
      if (t<face->t) face->t=t;
      if (b>face->b) face->b=b;
    }
    face->x=face->l;
    face->y=face->t;
    face->w=face->r-face->l;
    face->h=face->b-face->t;
    if ((face->w<1)||(face->h<1)) {
      fprintf(stderr,"%s: Failed to measure face bounds for the row starting at tile 0x%02x\n",path,tileid);
      return -2;
    }
    if (face->h<shortest) shortest=face->h;
  }
  
  /* We're going to replace tiles 0x08..0x0f.
   * Start them with the background color.
   */
  fill_rect(pixels,w,h,tilesize*8,0,tilesize*8,tilesize,bgcolor);
  
  /* Iterate sub-rows down to the shortest available, and put a fixed count of faces on each row.
   */
  const int face_count_per_row=2;
  int stopy=tilesize-shortest;
  int suby=0;
  for (;suby<=stopy;suby++) {
    for (i=face_count_per_row;i-->0;) {
      int panic=100;
      for (;;) {
        face=facev+(rand()&3);
        if (face->h<=tilesize-suby) break;
        if (panic--<=0) {
          fprintf(stderr,"%s: Might have messed up shortest-face detection. Failed to locate a face within %d pixels.\n",path,tilesize-suby);
          return -2;
        }
      }
      int subx=0;
      #if 0 /* Keep strictly in bounds. I'm not crazy about this, it's too tiley. */
        if (face->w<tilesize) subx=rand()%(tilesize-face->w+1);
      #else /* Allow horizontal joining. This will make unpleasant missing-neighbor artifacts. */
        subx=rand()%tilesize;
      #endif
      int dstx=tilesize*8+subx;
      int dsty=suby;
      int srcx=(face->tileid&15)*tilesize+face->x;
      srcy=(face->tileid>>4)*tilesize+face->y;
      uint32_t color=close_but_not_too_close(bgcolor);
      blit_self_recolor(pixels,w,h,dstx,dsty,srcx,srcy,tilesize*8,face->h,color);
      // If we're off the right edge, draw a second copy straddling the left edge.
      if (subx>tilesize-face->w) {
        blit_self_recolor(pixels,w,h,dstx-tilesize,dsty,srcx,srcy,tilesize*8,face->h,color);
      }
    }
  }
  
  return 0;
}

/* Select operation. ie a function implemented above.
 */

#define OPERATION lawnmowing_generate_animated_grass

static const char *operation_name(void *op) {
  #define _(name) if (op==name) return #name;
  _(lawnmowing_generate_animated_grass)
  #undef _
  return "?";
}

/* Main.
 */
 
int main(int argc,char **argv) {
  if (argc!=2) {
    fprintf(stderr,"Usage: %s PNG_FILE\nFile is rewritten in place.\nSelected operation: %s\n",argv[0],operation_name(OPERATION));
    return 1;
  }
  const char *path=argv[1];
  srand(time(0));
  
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",path);
    return 1;
  }
  
  int w=0,h=0;
  if ((image_measure(&w,&h,src,srcc)<0)||(w<1)||(h<1)||(w>4096)||(h>4096)) {
    fprintf(stderr,"%s: Failed to decode image header, or unreasonable size (%d,%d)\n",path,w,h);
    return 1;
  }
  
  uint32_t *pixels=malloc(w*h*4);
  if (!pixels) return 1;
  if (image_decode(pixels,w*h*4,src,srcc)<0) {
    fprintf(stderr,"%s: Failed to decode %dx%d image from %d bytes serial.\n",path,w,h,srcc);
    return 1;
  }
  
  int err=OPERATION(pixels,w,h,path);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error during main operation '%s'.\n",path,operation_name(OPERATION));
    return 1;
  }
  
  struct sr_encoder dst={0};
  if (image_encode(&dst,pixels,w*h*4,w,h)<0) {
    fprintf(stderr,"%s: Failed to reencode image after processing.\n",path);
    return 1;
  }
  
  if (file_write(path,dst.v,dst.c)<0) {
    fprintf(stderr,"%s: Failed to rewrite file, %d bytes.\n",path,dst.c);
    return 1;
  }
  fprintf(stderr,
    "%s: Rewrote %dx%d image file with operation '%s'. %d bytes in, %d bytes out.\n",
    path,w,h,operation_name(OPERATION),srcc,dst.c
  );
  
  return 0;
}

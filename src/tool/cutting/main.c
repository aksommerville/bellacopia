#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include "opt/image/image.h"

/* Globals.
 */
 
static const char *exename="cutting";
static const char *srcpath=0;
static int imgw=0,imgh=0;
static uint32_t *img=0;

// Buffer size in points. 256 is way more than we could ever need; ok to fail if it breaches.
#define BUFFER_SIZE 256
struct ipt { uint8_t x,y; };
static struct ipt tmp[BUFFER_SIZE];
static struct ipt iptv[BUFFER_SIZE];

/* Find white pixels in this box and emit the path, as C code for pasting into battle_cutting.c.
 * (w,h) should be a square, a factor of 128. That way, we can draw the images as a full line and let scaling up produce the dotted effect.
 */
 
static int extract_points(const char *name,int x,int y,int w,int h) {

  /* Validate dimensions.
   */
  if ((w!=h)||(w<1)||(w>128)||(128%w)||(x<0)||(y<0)||(x>128-w)||(y>128-h)) {
    fprintf(stderr,"%s(%s): Invalid dimensions %dx%d\n",__func__,name,w,h);
    return -1;
  }
  int scale=128/w;
  int offset=scale>>1;
  
  /* Collect all white pixels into (tmp).
   * Important to use opaque white as the marker color, since it's byte-order-independant.
   */
  int tmpc=0;
  const uint32_t *src=img+imgw*y+x;
  int suby=0;
  for (;suby<h;suby++,src+=imgw) {
    const uint32_t *srcp=src;
    int subx=0;
    for (;subx<w;subx++,srcp++) {
      if (*srcp==0xffffffff) {
        if (tmpc>=BUFFER_SIZE) {
          fprintf(stderr,"%s: Breached BUFFER_SIZE\n",name);
          return -1;
        }
        tmp[tmpc++]=(struct ipt){subx,suby};
      }
    }
  }
  if (tmpc<1) {
    fprintf(stderr,"%s: No white pixels.\n",name);
    return -1;
  }
  
  /* Start at the bottommost pixel, and rightmost of those if more than one.
   */
  int tmpp=0;
  int i=0;
  struct ipt *q=tmp;
  for (;i<tmpc;i++,q++) {
    if (q->y>tmp[tmpp].y) tmpp=i;
    else if ((q->y==tmp[tmpp].y)&&(q->x>tmp[tmpp].x)) tmpp=i;
  }
  int tmpx=tmp[tmpp].x;
  int tmpy=tmp[tmpp].y;
  int x0=tmpx;
  int y0=tmpy;
  
  /* Begin emitting text.
   */
  fprintf(stdout,"static const struct ipt pattern_%s[]={\n",name);
  
  /* Consume point and find the next one, until we run out.
   * Next point is the nearest, and we don't care which way ties break.
   * Does need to be true nearest tho -- Manhattan distance won't cut it.
   */
  int linelen=0; // We'll be a good citizen and indent lines, and keep them to a reasonable length.
  #define CONSUMEPT { \
    if (!linelen) { \
      linelen=2; \
      fprintf(stdout,"  "); \
    } else if (linelen>100) { \
      linelen=2; \
      fprintf(stdout,"\n  "); \
    } \
    linelen+=fprintf(stdout,"{%d,%d},",tmpx*scale+offset,tmpy*scale+offset); \
    tmpc--; \
    memmove(tmp+tmpp,tmp+tmpp+1,sizeof(struct ipt)*(tmpc-tmpp)); \
  }
  for (;;) {
    CONSUMEPT
    if (tmpc<1) break;
    tmpp=0;
    int dbest=999999;
    for (i=0,q=tmp;i<tmpc;i++,q++) {
      int dx=q->x-tmpx;
      int dy=q->y-tmpy;
      int d=dx*dx+dy*dy;
      if (d<dbest) {
        tmpp=i;
        dbest=d;
      }
    }
    tmpx=tmp[tmpp].x;
    tmpy=tmp[tmpp].y;
  }
  
  /* Repeat the first point, terminate the text, and yep, that's it.
   */
  fprintf(stdout,"{%d,%d},\n{0,0}};\n",x0*scale+offset,y0*scale+offset);
  return 0;
}

/* Alpha mode.
 * GIMP 3 isn't storing chroma from transparent pixels even when I explicitly fucking tell it to.
 * We need that for images that get rotated scaled up with linear filtering, if their edge isn't black. Doesn't come up a lot in Bellacopia but it does come up.
 *
 * Oh for fuck's sake.
 * It's Egg dropping the alpha, isn't it?
 */
 
static int alpha_mode() {
  fprintf(stderr,"%s %dx%d %s. 0x%08x\n",__func__,imgw,imgh,srcpath,img[68*imgw+180]);
  int changec=0;
  uint32_t *p=img;
  int y=0; for (;y<imgh;y++) {
    int x=0; for (;x<imgw;x++,p++) {
      uint32_t pixel=*p;
      if (pixel&0xff000000) continue; // Not transparent, carry on.
      uint32_t opn=0; // Opaque neighbor or zero. "Opaque" meaning any alpha at all.
      #define CK(dx,dy) { \
        int subx=x+(dx),suby=y+(dy); \
        if ((subx>=0)&&(suby>=0)&&(subx<imgw)&&(suby<imgh)) { \
          uint32_t qpx=p[(dy)*imgw+(dx)]; \
          if (qpx&0xff000000) { \
            if (opn&&(qpx!=opn)) continue; /* Multiple opaque neighbors of different colors. Don't touch it. */ \
            opn=qpx; \
          } \
        } \
      }
      CK( 0,-1)
      CK(-1, 0)
      CK( 1, 0)
      CK( 0, 1)
      if (!opn) {
        CK(-1,-1)
        CK( 1,-1)
        CK(-1, 1)
        CK( 1, 1)
      }
      #undef CK
      if (!opn) continue; // No opaque neighbor. Leave it be.
      uint32_t rplc=opn&0x00ffffff;
      if (*p==rplc) continue; // Already the right value.
      *p=rplc;
      changec++;
    }
  }
  if (changec) {
    struct sr_encoder encoder={0};
    if (image_encode(&encoder,img,imgw*imgh*4,imgw,imgh)<0) {
      fprintf(stderr,"%s: Failed to reencode image\n",srcpath);
      sr_encoder_cleanup(&encoder);
      return -1;
    }
    if (file_write(srcpath,encoder.v,encoder.c)<0) {
      fprintf(stderr,"%s: Failed to rewrite file, %d bytes\n",srcpath,encoder.c);
      sr_encoder_cleanup(&encoder);
      return -1;
    }
    fprintf(stderr,"%s: Rewrote file with %d changed pixels, %d bytes.\n",srcpath,changec,encoder.c);
    sr_encoder_cleanup(&encoder);
  } else {
    fprintf(stderr,"%s: No change\n",srcpath);
  }
  return 0;
}

/* --help
 */
 
static void print_usage() {
  fprintf(stderr,
    "Usage: %s SRCPATH\n"
    "  --alpha      Alpha mode: Overwrite image in place, replacing transparent pixels with their opaque neighbor.\n"
  ,exename);
}

/* Main.
 */
 
int main(int argc,char **argv) {

  /* Read argv.
   */
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) exename=argv[0];
  int alphamode=0;
  int argi=1;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    if (arg[0]!='-') { // Positional.
      if (srcpath) goto _unexpected_arg_;
      srcpath=arg;
      continue;
    }
    if (!arg[1]) goto _unexpected_arg_; // Single dash alone.
    const char *k=0,*v=0;
    int kc=0,vc=0;
    if (arg[1]!='-') { // Short option.
      k=arg+1;
      kc=1;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&(argv[argi][0]!='-')) v=argv[argi++];
    } else if (!arg[2]) { // Double dash alone.
      goto _unexpected_arg_;
    } else { // Long option.
      k=arg+2;
      kc=0;
      while (k[kc]&&(k[kc]!='=')) kc++;
      if (k[kc]=='=') v=k+kc+1;
      else if ((argi<argc)&&(argv[argi][0]!='-')) v=argv[argi++];
    }
    if (v) while (v[vc]) vc++;
    
    if ((kc==4)&&!memcmp(k,"help",4)) {
      print_usage();
      return 0;
    }
    if ((kc==5)&&!memcmp(k,"alpha",5)) {
      alphamode=1;
      continue;
    }
    //TODO options?
    
   _unexpected_arg_:;
    fprintf(stderr,"%s: Unexpected argument '%s'\n",exename,arg);
    return 1;
  }
  if (!srcpath) {
    print_usage();
    return 1;
  }
  
  /* Acquire the reference image.
   */
  void *serial=0;
  int serialc=file_read(&serial,srcpath);
  if (serialc<0) {
    fprintf(stderr,"%s: Failed to read file\n",srcpath);
    return 1;
  }
  if (image_measure(&imgw,&imgh,serial,serialc)<0) {
    fprintf(stderr,"%s: Failed to measure image\n",srcpath);
    return 1;
  }
  int imglen=imgw*imgh*4;
  if (imglen<4) return 1;
  if (!(img=calloc(1,imglen))) return 1;
  if (image_decode(img,imglen,serial,serialc)<0) {
    fprintf(stderr,"%s: Failed to decode image\n",srcpath);
    return 1;
  }
  fprintf(stderr,"%s: Got reference image, %dx%d pixels.\n",srcpath,imgw,imgh);
  
  /* "alpha mode" shoe-horned in here just to avoid writing another tool.
   */
  if (alphamode) {
    if (alpha_mode()<0) return 1;
    return 0;
  }

  /* Slice and analyze.
   */
  if (extract_points("heart",0,0,16,16)<0) return 1;
  if (extract_points("diamond",16,0,16,16)<0) return 1;
  if (extract_points("star",32,0,16,16)<0) return 1;
  if (extract_points("circle",48,0,16,16)<0) return 1;
  if (extract_points("hat",64,0,16,16)<0) return 1;
  if (extract_points("seashell",80,0,16,16)<0) return 1;
  if (extract_points("claw",96,0,16,16)<0) return 1;
  if (extract_points("beet",112,0,16,16)<0) return 1;
  
  return 0;
}

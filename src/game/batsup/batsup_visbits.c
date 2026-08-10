#include "game/bellacopia.h"
#include "batsup_visbits.h"

/* Hourglass.
 */
 
void batsup_render_hourglass(int midx,int midy,double v,double range) {
  double n=v/range;
  if (n<0.0) n=0.0;
  else if (n>1.0) n=1.0;
  int frame=(int)(n*35.0);
  if (frame<0) frame=0;
  else if (frame>34) frame=34;
  frame=34-frame;
  graf_set_image(&g.graf,RID_image_visbits);
  graf_tile(&g.graf,midx,midy,frame,0);
  if (v<2.000) {
    int warnframe=(g.framec%10>=5);
    graf_tile(&g.graf,midx,midy,warnframe?36:35,0);
  }
}

/* Fancy decal.
 * Same idea as graf_decal_rotate(), but we've added non-square sources and an axiswise transform.
 * Those two things make it a bit more complicated.
 */
 
void batsup_render_decal(int dstx,int dsty,int srcl,int srct,int w,int h,uint8_t xform,double t,double scale) {
  
  /* Effect XREV and YREV upon the source coordinates.
   * But that won't fly for SWAP.
   */
  int srcr=srcl+w;
  int srcb=srct+h;
  #define SWAP(a,b) { \
    int tmp=a; \
    a=b; \
    b=tmp; \
  }
  if (xform&EGG_XFORM_XREV) SWAP(srcl,srcr)
  if (xform&EGG_XFORM_YREV) SWAP(srct,srcb)
  #undef SWAP
  
  /* Select initial output coordinates taking (dst) as the origin.
   * Apply SWAP and (scale) here.
   */
  double nwx,nwy,nex,ney,swx,swy,sex,sey;
  if (xform&EGG_XFORM_SWAP) {
    nwx=nex=h*-0.5*scale;
    swx=sex=h* 0.5*scale;
    nwy=swy=w*-0.5*scale;
    ney=sey=w* 0.5*scale;
  } else {
    nwx=swx=w*-0.5*scale;
    nex=sex=w* 0.5*scale;
    nwy=ney=h*-0.5*scale;
    swy=sey=h* 0.5*scale;
  }
  
  /* Apply rotation in-place on those output coordinates.
   */
  double sint=sin(t);
  double cost=cos(t);
  #define AFFIFY(pfx) { \
    double _x=pfx##x*cost-pfx##y*sint; \
    double _y=pfx##x*sint+pfx##y*cost; \
    pfx##x=_x; \
    pfx##y=_y; \
  }
  AFFIFY(nw)
  AFFIFY(ne)
  AFFIFY(sw)
  AFFIFY(se)
  #undef AFFIFY
  
  graf_triangle_strip_tex_begin(&g.graf,
    dstx+lround(nwx),dsty+lround(nwy),srcl,srct,
    dstx+lround(nex),dsty+lround(ney),srcr,srct,
    dstx+lround(swx),dsty+lround(swy),srcl,srcb
  );
  graf_triangle_strip_tex_more(&g.graf,
    dstx+lround(sex),dsty+lround(sey),srcr,srcb
  );
}

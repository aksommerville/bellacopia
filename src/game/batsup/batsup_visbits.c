#include "egg_res_toc.h"
#include "game/bellacopia.h"
#include "game/text.h"
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

/* Monkish text.
 */
 
static const struct monkish_adjustments {
  int8_t l,r; // Advance by (l), render the tile, then advance by (r).
} monkish_adjustments[64]={
  {2,2},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},
  {8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{8,8},{0,0},{0,0},{0,0},{0,0},{0,0},
  {0,0},{3,4},{3,4},{3,4},{3,4},{3,4},{3,4},{3,4},{3,4},{1,2},{3,4},{3,4},{2,3},{3,4},{3,4},{3,4},
  {3,4},{3,4},{3,4},{3,4},{3,4},{3,4},{3,4},{3,4},{3,4},{3,4},{3,4},{0,0},{0,0},{0,0},{0,0},{0,0},
};
 
int monkish_render(int dstx,int dsty,const char *src,int srcc,int measure_only) {
  if (!measure_only) graf_set_image(&g.graf,RID_image_monkish);
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  dsty-=4;
  int dstx0=dstx;
  int srcp=0,pvch=0;
  while (srcp<srcc) {
    int ch=src[srcp++];
    
    // If it's a digit, evaluate, then turn into text.
    // Also dash followed by a digit.
    int isnumber=0,v=0;
    if ((ch>='0')&&(ch<='9')) {
      isnumber=1;
      v=ch-'0';
      while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
        v*=10;
        v+=src[srcp++]-'0';
      }
    } else if ((ch=='-')&&(srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      isnumber=1;
      while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
        v*=10;
        v-=src[srcp++]-'0';
      }
    }
    if (isnumber) {
      char tmp[64];
      int tmpc=int_as_words(tmp,sizeof(tmp),v);
      if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
        dstx+=monkish_render(dstx,dsty+4,tmp,tmpc,measure_only); // +4 to undo the vertical correction we applied up top
      }
      continue;
    }
    
    // Find the adjustments. Everything that isn't in the letters' rows measures like '@', which is how monks spell space.
    const struct monkish_adjustments *adj;
    if ((ch>=0x40)&&(ch<0x80)) adj=monkish_adjustments+ch-0x40;
    else adj=monkish_adjustments;
    
    //TODO Special kerning case for lower J.
    //...oh but now that i think about it, Latin doesn't use J so who cares. Revisit if we ever use this for English.
    
    dstx+=adj->l;
    if (!measure_only) graf_tile(&g.graf,dstx,dsty,ch,0);
    dstx+=adj->r;
  }
  return dstx-dstx0;
}

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

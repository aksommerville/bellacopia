/* batsup_visbits.h
 * Little render things that might be shared across battles.
 */
 
#ifndef BATSUP_VISBITS_H
#define BATSUP_VISBITS_H

/* One tile, centered at (midx,midy).
 * Provide (v) and (range) separate instead of normalizing, so we can do a warning highlight in absolute time.
 */
void batsup_render_hourglass(int midx,int midy,double v,double range);

/* graf doesn't have a helper for scaled rotated decals that also have an axiswise transform.
 * But it's not complicated.
 * And as long as we're in there, we also support non-square source images.
 * (dstx,dsty) is the center of output.
 * Load the desired image in (g.graf) as usual.
 */
void batsup_render_decal(int dstx,int dsty,int srcx,int srcy,int w,int h,uint8_t xform,double t,double scale);

#endif

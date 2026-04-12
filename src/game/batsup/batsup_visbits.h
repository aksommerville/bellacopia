/* batsup_visbits.h
 * Little render things that might be shared across battles.
 */
 
#ifndef BATSUP_VISBITS_H
#define BATSUP_VISBITS_H

/* One tile, centered at (midx,midy).
 * Provide (v) and (range) separate instead of normalizing, so we can do a warning highlight in absolute time.
 */
void batsup_render_hourglass(int midx,int midy,double v,double range);

#endif

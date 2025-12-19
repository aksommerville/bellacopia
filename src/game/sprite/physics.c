#include "game/game.h"
#include "game/world/map.h"
#include "sprite.h"

#define SMIDGEON 0.001

/* Measure freedom in a cardinal direction.
 * <=0 if we can't move that way.
 * Otherwise returns something positive, zero thru the magnitude of the request.
 * We don't look for things beyond the requested distance.
 * If you ask for more than 2 meters, we'll clamp at 2. (alternative would be possible missed collisions).
 */
 
static double sprite_measure_freedom(const struct sprite *sprite,double dx,double dy) {

  /* Take the union of old and new positions as an aabb.
   * And as long as we're in there, validate cardinality, and express motion polar.
   * And enforce the universal speed limit.
   */
  #define SPEEDLIMIT 2.0
  double mag,nx=0.0,ny=0.0;
  struct aabb cover={sprite->x+sprite->hbl,sprite->x+sprite->hbr,sprite->y+sprite->hbt,sprite->y+sprite->hbb};
  if (dx<0.0) {
    if ((dy<0.0)||(dy>0.0)) return 0.0;
    if (dx<-SPEEDLIMIT) dx=-SPEEDLIMIT;
    mag=-dx;
    nx=-1.0;
    cover.l+=dx;
  } else if (dx>0.0) {
    if ((dy<0.0)||(dy>0.0)) return 0.0;
    if (dx>SPEEDLIMIT) dx=SPEEDLIMIT;
    mag=dx;
    nx=1.0;
    cover.r+=dx;
  } else if (dy<0.0) {
    if (dy<-SPEEDLIMIT) dy=-SPEEDLIMIT;
    mag=-dy;
    ny=-1.0;
    cover.t+=dy;
  } else if (dy>0.0) {
    if (dy>SPEEDLIMIT) dy=SPEEDLIMIT;
    mag=dy;
    ny=1.0;
    cover.b+=dy;
  } else return 0.0;
  #undef SPEEDLIMIT
  
  /* Collect all the things that overlap that coverage box.
   * Each cell of the grid is a distinct thing.
   * If we cross some arbitrary limit, stop looking. Owing to SPEEDLIMIT, that is extremely unlikely.
   */
  #define COLL_LIMIT 16
  struct aabb collv[COLL_LIMIT];
  int collc=0;
  
  /* First grid collisions, if applicable.
   * We check just one map per request, the one the sprite is currently centered on.
   * Since sprites should be <=1m wide and map borders should match each other, that should be OK.
   */
  if (sprite->physics) {
    // Express the coverage bounds in integer plane meters.
    int cola=(int)cover.l; if (cover.l<0.0) cola--;
    int colz=(int)(cover.r-SMIDGEON); if (cover.r<0.0) colz--;
    int rowa=(int)cover.t; if (cover.t<0.0) rowa--;
    int rowz=(int)(cover.b-SMIDGEON); if (cover.b<0.0) rowz--;
    // Then turn those into the map coverage range.
    int mxa=cola/NS_sys_mapw; if (cola<0) mxa--;
    int mxz=colz/NS_sys_mapw; if (colz<0) mxz--;
    int mya=rowa/NS_sys_maph; if (rowa<0) mya--;
    int myz=rowz/NS_sys_maph; if (rowz<0) myz--;
    // Iterate covered maps. If no map exists there, assume it's passable.
    int my=mya; for (;my<=myz;my++) {
      int mx=mxa; for (;mx<=mxz;mx++) {
        const struct map *map=map_by_position(mx,my,sprite->z);
        if (!map) continue;
        int mx0=mx*NS_sys_mapw;
        int my0=my*NS_sys_maph;
        int mcola=cola-mx0; if (mcola<0) mcola=0;
        int mcolz=colz-mx0; if (mcolz>=NS_sys_mapw) mcolz=NS_sys_mapw-1;
        int mrowa=rowa-my0; if (mrowa<0) mrowa=0;
        int mrowz=rowz-my0; if (mrowz>=NS_sys_maph) mrowz=NS_sys_maph-1;
        const uint8_t *srcrow=map->v+mrowa*NS_sys_mapw+mcola;
        int mrow=mrowa; for (;mrow<=mrowz;mrow++,srcrow+=NS_sys_mapw) {
          const uint8_t *srcp=srcrow;
          int mcol=mcola; for (;mcol<=mcolz;mcol++,srcp++) {
            uint8_t ph=map->physics[*srcp];
            if (sprite->physics&(1<<ph)) {
              struct aabb *coll=collv+collc++;
              coll->l=mx0+mcol;
              coll->r=mx0+mcol+1.0;
              coll->t=my0+mrow;
              coll->b=my0+mrow+1.0;
              if (collc>=COLL_LIMIT) goto _ready_;
            }
          }
        }
      }
    }
  }
  
  /* Check other solid sprites.
   */
  {
    struct sprite **otherp;
    int otherc=sprites_get_all(&otherp);
    for (;otherc-->0;otherp++) {
      struct sprite *other=*otherp;
      if (!other->solid) continue;
      if (other->defunct) continue;
      if (other==sprite) continue;
      double ol=other->x+other->hbl; if (ol>=cover.r) continue;
      double or=other->x+other->hbr; if (or<=cover.l) continue;
      double ot=other->y+other->hbt; if (ot>=cover.b) continue;
      double ob=other->y+other->hbb; if (ob<=cover.t) continue;
      collv[collc++]=(struct aabb){ol,or,ot,ob};
      if (collc>=COLL_LIMIT) goto _ready_;
    }
  }
  
  /* With the collisions gathered, identify the nearest in the direction of travel.
   */
  #undef COLL_LIMIT
 _ready_:;
  if (!collc) return mag;
  struct aabb *coll=collv;
  struct aabb *nearest=coll;
  int i=collc;
  if (dx<0.0) {
    for (;i-->0;coll++) if (coll->r>nearest->r) nearest=coll;
    return sprite->x+sprite->hbl-nearest->r-SMIDGEON;
  } else if (dx>0.0) {
    for (;i-->0;coll++) if (coll->l<nearest->l) nearest=coll;
    return nearest->l-(sprite->x+sprite->hbr)-SMIDGEON;
  } else if (dy<0.0) {
    for (;i-->0;coll++) if (coll->b>nearest->b) nearest=coll;
    return sprite->y+sprite->hbt-nearest->b-SMIDGEON;
  } else {
    for (;i-->0;coll++) if (coll->t<nearest->t) nearest=coll;
    return nearest->t-(sprite->y+sprite->hbb)-SMIDGEON;
  }
  
  return mag;
}

/* Move sprite.
 */
 
int sprite_move(struct sprite *sprite,double dx,double dy) {

  // Not participating? Cool, just do it.
  if (!sprite->solid) {
    sprite->x+=dx;
    sprite->y+=dy;
    return 1;
  }
  
  /* Confirm we've got just one axis of motion, and note that as a scalar.
   * If both axes are nonzero, recur for each.
   */
  #define SPLIT { \
    int xok=sprite_move(sprite,dx,0.0); \
    int yok=sprite_move(sprite,0.0,dy); \
    return xok||yok; \
  }
  uint8_t dir;
  if (dx<0.0) {
    if ((dy<0.0)||(dy>0.0)) SPLIT
    dir=NS_dir_w;
  } else if (dx>0.0) {
    if ((dy<0.0)||(dy>0.0)) SPLIT
    dir=NS_dir_e;
  } else if (dy<0.0) {
    dir=NS_dir_n;
  } else if (dy>0.0) {
    dir=NS_dir_s;
  } else return 0;
  #undef SPLIT
  
  /* Measure freedom in the requested direction.
   * If greater than zero, commit it and we're done.
   */
  double freedom=sprite_measure_freedom(sprite,dx,dy);
  if (freedom>0.0) {
         if (dx<0.0) sprite->x-=freedom;
    else if (dx>0.0) sprite->x+=freedom;
    else if (dy<0.0) sprite->y-=freedom;
    else if (dy>0.0) sprite->y+=freedom;
    return 1;
  }
  
  // Was thinking about some generic off-axis correction here, but maybe we should leave that to the controllers.
  
  return 0;
}

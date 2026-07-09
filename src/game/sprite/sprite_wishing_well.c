/* sprite_wishing_well.c
 * We're not the well itself, just a decoration of the tossed item sinking into it.
 */
 
#include "game/bellacopia.h"

struct sprite_wishing_well {
  struct sprite hdr;
  int itemid;
  int vish; // Starts at NS_sys_tilesize and dwindles to zero. Also serves as a TTL.
  double animclock;
};

#define SPRITE ((struct sprite_wishing_well*)sprite)

static int _wishing_well_init(struct sprite *sprite) {
  SPRITE->itemid=(sprite->arg[0]<<8)|sprite->arg[1];
  const struct item_detail *detail=item_detail_for_itemid(SPRITE->itemid);
  if (!detail) return -1;
  sprite->imageid=RID_image_pause;
  sprite->tileid=detail->tileid;
  sprite_group_add(GRP(visible),sprite);
  sprite_group_add(GRP(update),sprite);
  return 0;
}

static void _wishing_well_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.100;
    if (--(SPRITE->vish)<=0) sprite_kill_soon(sprite);
  }
}

static void _wishing_well_render(struct sprite *sprite,int x,int y) {
  if (SPRITE->vish<1) return;
  graf_set_image(&g.graf,sprite->imageid);
  int w=NS_sys_tilesize,h=NS_sys_tilesize-SPRITE->vish;
  int srcx=(sprite->tileid&0x0f)*NS_sys_tilesize;
  int srcy=(sprite->tileid>>4)*NS_sys_tilesize;
  int dstx=x-(NS_sys_tilesize>>1);
  int dsty=y-(NS_sys_tilesize>>1)+(NS_sys_tilesize-SPRITE->vish);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,w,h);
  if (SPRITE->vish&1) {
    graf_fill_rect(&g.graf,dstx-1,dsty+h,NS_sys_tilesize+2,1,0x0000ffff);
  } else {
    graf_fill_rect(&g.graf,dstx,dsty+h,NS_sys_tilesize,1,0x0000ffff);
  }
}

const struct sprite_type sprite_type_wishing_well={
  .name="wishing_well",
  .objlen=sizeof(struct sprite_wishing_well),
  .init=_wishing_well_init,
  .update=_wishing_well_update,
  .render=_wishing_well_render,
};

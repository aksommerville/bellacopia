/* sprite_soulballs.c
 * Manages a set of flashing balls that spew out of a dead thing.
 */
 
#include "game/bellacopia.h"

struct sprite_soulballs {
  struct sprite hdr;
  int ballc;
  double r;
  double dr;
  double ttl;
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_soulballs*)sprite)

static int _soulballs_init(struct sprite *sprite) {
  SPRITE->ballc=sprite->arg[0];
  if (SPRITE->ballc<3) SPRITE->ballc=3;
  else if (SPRITE->ballc>16) SPRITE->ballc=16; // Technical limit 255, but please be reasonable.
  SPRITE->dr=10.0; // m/s
  SPRITE->ttl=2.0; // s
  sprite->layer=200;
  sprite->imageid=RID_image_hero;
  sprite->tileid=0xa6; // Four tiles pingpong.
  sprite_group_add(GRP(visible),sprite);
  sprite_group_add(GRP(update),sprite);
  return 0;
}

static void _soulballs_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->ttl-=elapsed)<=0.0) {
    sprite_kill_soon(sprite);
    return;
  }
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.120;
    if (++(SPRITE->animframe)>=6) SPRITE->animframe=0;
  }
  SPRITE->r+=SPRITE->dr*elapsed;
}

static void _soulballs_render(struct sprite *sprite,int x,int y) {
  if (SPRITE->ballc<1) return;
  double midx=x,midy=y;
  double t=0.0;
  double dt=(M_PI*2.0)/SPRITE->ballc;
  int i=SPRITE->ballc;
  int frame=SPRITE->animframe;
  graf_set_image(&g.graf,sprite->imageid);
  for (;i-->0;t+=dt,frame++) {
    int dstx=(int)(midx+SPRITE->r*sin(t)*NS_sys_tilesize);
    int dsty=(int)(midy-SPRITE->r*cos(t)*NS_sys_tilesize);
    if (frame>=6) frame=0;
    uint8_t tileid=sprite->tileid;
    if (frame==5) tileid+=1;
    else if (frame==4) tileid+=2;
    else tileid+=frame;
    graf_tile(&g.graf,dstx,dsty,tileid,0);
  }
}

const struct sprite_type sprite_type_soulballs={
  .name="soulballs",
  .objlen=sizeof(struct sprite_soulballs),
  .init=_soulballs_init,
  .update=_soulballs_update,
  .render=_soulballs_render,
};

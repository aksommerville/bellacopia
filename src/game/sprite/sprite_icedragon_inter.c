/* sprite_icedragon_inter.c
 * Decoration that flies from a defeated icedragon to the next instance.
 * Instantiate without a resource.
 * Put me directly on the defeated icedragon, and I'll find the next one or self-abort.
 * Set NS_fld16_iceseq before spawning me.
 */
 
#include "game/bellacopia.h"

struct sprite_icedragon_inter {
  struct sprite hdr;
  double dx,dy;
  double ttl;
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_icedragon_inter*)sprite)

static int _icedragon_inter_init(struct sprite *sprite) {
  sprite->imageid=RID_image_icepalace_sprites;
  sprite->tileid=0x03;
  sprite->layer=110;
  SPRITE->ttl=5.000; // No matter what else, if so much time elapses, we're done.
  
  /* Which spawn point is next?
   * Abort if it's not in 1..6. We could just let it ride, but we know there are no spawn points outside that range.
   */
  int seq=store_get_fld16(NS_fld16_iceseq)+1;
  if ((seq<1)||(seq>6)) return -1;
  
  /* Find that spawn point on my plane.
   */
  struct plane *plane=plane_by_position(sprite->z);
  if (!plane) return -1;
  double dstx=0.0,dsty=0.0; // >0 if we find it
  int mapi=plane->w*plane->h;
  struct map *map=plane->v;
  for (;mapi-->0;map++) {
    struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      if (cmd.opcode!=CMD_map_sprite) continue;
      int spriteid=(cmd.arg[2]<<8)|cmd.arg[3];
      if (spriteid!=RID_sprite_icedragon) continue;
      int spawnseq=cmd.arg[4];
      if (spawnseq!=seq) continue;
      dstx=map->lng*NS_sys_mapw+cmd.arg[0]+0.5;
      dsty=map->lat*NS_sys_maph+cmd.arg[1]+0.5;
      mapi=0;
      break;
    }
  }
  if (dstx<=0.0) return -1;
  
  /* Prepare travel vector.
   */
  const double speed=9.0; // m/s, good if it's a little faster than Dot.
  SPRITE->dx=dstx-sprite->x;
  SPRITE->dy=dsty-sprite->y;
  double d2=SPRITE->dx*SPRITE->dx+SPRITE->dy*SPRITE->dy;
  if (d2<1.0) return -1;
  double distance=sqrt(d2);
  SPRITE->dx*=speed/distance;
  SPRITE->dy*=speed/distance;
  
  sprite_group_add(GRP(visible),sprite);
  sprite_group_add(GRP(update),sprite);
  return 0;
}

static void _icedragon_inter_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->ttl-=elapsed)<=0.0) {
    sprite_kill_soon(sprite);
  }
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.200;
    if (++(SPRITE->animframe)>=2) SPRITE->animframe=0;
  }
  sprite->x+=SPRITE->dx*elapsed;
  sprite->y+=SPRITE->dy*elapsed;
}

static void _icedragon_inter_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  uint8_t tileid=sprite->tileid;
  if (SPRITE->animframe) tileid+=0x10;
  if (SPRITE->dx>=0.0) {
    graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y,tileid,0);
    graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y,tileid+1,0);
  } else {
    graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y,tileid+1,EGG_XFORM_XREV);
    graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y,tileid,EGG_XFORM_XREV);
  }
}

const struct sprite_type sprite_type_icedragon_inter={
  .name="icedragon_inter",
  .objlen=sizeof(struct sprite_icedragon_inter),
  .init=_icedragon_inter_init,
  .update=_icedragon_inter_update,
  .render=_icedragon_inter_render,
};

/* sprite_motionsensor.c
 * Tracks movement of hero in our line of sight and sets a flag when too much motion.
 */
 
#include "game/bellacopia.h"

#define UP_RATE 5.000
#define DOWN_RATE 1.000

struct sprite_motionsensor {
  struct sprite hdr;
  int fldid;
  double beaml,beamr,beamt,beamb; // Detection zone bounds in plane meters.
  int present; // Nonzero if something is present in my beam field.
  int triggered; // Nonzero if our spook limit was breached, ie field was set.
  double spook; // 0..1, how much motion. We trigger at 1.
  double herox,heroy; // Recent hero position.
};

#define SPRITE ((struct sprite_motionsensor*)sprite)

/* Cleanup.
 */
 
static void _motionsensor_del(struct sprite *sprite) {
}

/* Init.
 */
 
static int motionsensor_can_beam_cell(struct sprite *sprite,const struct map *map,int x,int y) {
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return 0;
  uint8_t ph=map->physics[map->v[y*NS_sys_mapw+x]];
  switch (ph) {
    case NS_physics_solid:
    case NS_physics_grabbable:
      return 0;
  }
  return 1;
}
 
static int _motionsensor_init(struct sprite *sprite) {
  SPRITE->fldid=(sprite->arg[0]<<8)|sprite->arg[1];
  
  /* Determine detection zone.
   * We always point leftward, and extend to the first solid in that direction.
   * Left wall must be on the same map, or we'll stop at the edge.
   * We look for other sprites' centers in the beam box, not intersection.
   */
  SPRITE->beamt=sprite->y-0.5;
  SPRITE->beamb=sprite->y+0.5;
  SPRITE->beamr=sprite->x-0.5;
  SPRITE->beaml=SPRITE->beamr;
  struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  int qx=(int)sprite->x-1-map->lng*NS_sys_mapw;
  int qy=(int)sprite->y-map->lat*NS_sys_maph;
  if ((qy<0)||(qy>=NS_sys_maph)) return -1;
  if ((qx<0)||(qx>=NS_sys_mapw)) return -1;
  while ((qx>=0)&&motionsensor_can_beam_cell(sprite,map,qx,qy)) {
    qx--;
    SPRITE->beaml-=1.0;
  }
  
  /* If our field is set, force it off.
   */
  if (store_get_fld(SPRITE->fldid)) {
    store_set_fld(SPRITE->fldid,0);
    g.camera.mapsdirty=1;
  }
  
  return 0;
}

/* State changes.
 */
 
static void motionsensor_off(struct sprite *sprite) {
  // Don't unset the flag. Once tripped, you have to manually unlock it (or leave and cause it to respawn).
  SPRITE->triggered=0;
}

static void motionsensor_on(struct sprite *sprite) {
  if (!store_get_fld(SPRITE->fldid)) {
    bm_sound(RID_sound_negatory);
    store_set_fld(SPRITE->fldid,1);
    g.camera.mapsdirty=1;
  }
  SPRITE->triggered=1;
}

/* Update.
 */
 
static void _motionsensor_update(struct sprite *sprite,double elapsed) {
  
  /* Is the hero in our line of sight and moving?
   */
  int motion=0;
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    double dx=hero->x-SPRITE->herox;
    double dy=hero->y-SPRITE->heroy;
    const double small=0.050;
    if ((dx>small)||(dy>small)||(dx<-small)||(dy<-small)) {
      SPRITE->herox=hero->x;
      SPRITE->heroy=hero->y;
      if (g.vanishing<=0.0) { // We are optical so vanishing cream is definitely in play.
        if ((hero->x>SPRITE->beaml)&&(hero->x<SPRITE->beamr)&&(hero->y>SPRITE->beamt)&&(hero->y<SPRITE->beamb)) {
          motion=1;
        }
      }
    }
  }
  
  /* Spook meter goes up or down according to (motion).
   * If it breaches zero or one, change state.
   */
  if (motion) SPRITE->spook+=UP_RATE*elapsed;
  else SPRITE->spook-=DOWN_RATE*elapsed;
  if (SPRITE->spook>1.0) {
    SPRITE->spook=1.0;
    if (!SPRITE->triggered) {
      motionsensor_on(sprite);
    }
  } else if (SPRITE->spook<0.0) {
    SPRITE->spook=0.0;
    if (SPRITE->triggered) {
      motionsensor_off(sprite);
    }
  }
}

/* Render.
 */
 
static void _motionsensor_render(struct sprite *sprite,int x,int y) {

  // Static base tiles.
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
  graf_tile(&g.graf,x+NS_sys_tilesize,y,sprite->tileid+1,sprite->xform);
  
  // Spook meter.
  if (SPRITE->triggered) {
    graf_tile(&g.graf,x+NS_sys_tilesize,y,sprite->tileid+3,sprite->xform);
  } else {
    int spookw=(int)(SPRITE->spook*12.0);
    if (spookw>0) {
      if (spookw>12) spookw=12;
      int srcx=NS_sys_tilesize*2+2;
      int srcy=NS_sys_tilesize*8+10;
      graf_decal(&g.graf,x+10,y+2,srcx,srcy,spookw,4);
    }
  }
  
  // Laser.
  int lw=(int)((SPRITE->beamr-SPRITE->beaml)*NS_sys_tilesize);
  int lx=x-(NS_sys_tilesize>>1)-lw;
  int ly=y-1;
  int lh=2;
  graf_fill_rect(&g.graf,lx,ly,lw,lh,0xff000080);
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_motionsensor={
  .name="motionsensor",
  .objlen=sizeof(struct sprite_motionsensor),
  .del=_motionsensor_del,
  .init=_motionsensor_init,
  .update=_motionsensor_update,
  .render=_motionsensor_render,
};

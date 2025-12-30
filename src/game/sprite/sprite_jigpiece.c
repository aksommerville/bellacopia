#include "game/game.h"
#include "game/world/camera.h"
#include "game/world/map.h"
#include "sprite.h"

#define RADIUS 0.750

struct sprite_jigpiece {
  struct sprite hdr;
  int mapid;
};

#define SPRITE ((struct sprite_jigpiece*)sprite)

static int _jigpiece_init(struct sprite *sprite) {
  
  // Determine which map we belong to. Abort if undeterminable or already collected.
  int z=camera_get_z(); // Not (sprite->z), that hasn't been established yet. But we know what it's going to be.
  int px=0,py=0,pw=0,ph=0,oobx=0,ooby=0;
  struct map *mapv=maps_get_plane(&px,&py,&pw,&ph,&oobx,&ooby,z);
  if (!mapv||(pw<1)||(ph<1)) {
    fprintf(stderr,"%s: z=%d, failed to acquire plane.\n",__func__,z);
    return -1;
  }
  int pcellx=(int)sprite->x;
  int pcelly=(int)sprite->y;
  int fx=pcellx/NS_sys_mapw; if (sprite->x<0.0) fx--;
  int fy=pcelly/NS_sys_maph; if (sprite->y<0.0) fy--;
  if ((fx<px)||(fy<py)||(fx>=px+pw)||(fy>=py+ph)) {
    fprintf(stderr,"%s: (%f,%f) appears to be outside plane %d (%d,%d,%d,%d).\n",__func__,sprite->x,sprite->y,z,px,py,pw,ph);
    return -1;
  }
  int nx=fx-px;
  int ny=fy-py;
  struct map *map=mapv+ny*pw+nx;
  if (map->parent) {
    if (!(map=map_by_id(map->parent))) {
      fprintf(stderr,"%s: Parent map not found!\n",__func__); // Oops, can't report the id now, whatever.
      return -1;
    }
  }
  int jx=0,jy=0;
  uint8_t jxform=0;
  if (!map||!map->rid||(store_jigsaw_get(&jx,&jy,&jxform,map->rid)<0)) {
    fprintf(stderr,"%s: Failed to acquire map or jigsaw record.\n",__func__);
    return -1;
  }
  if (jxform!=0xff) return -1; // Already got it. This is perfectly normal.
  SPRITE->mapid=map->rid;
  
  return 0;
}

static void _jigpiece_update(struct sprite *sprite,double elapsed) {
  struct sprite *hero=sprites_get_hero();
  if (hero) {
    double dx=sprite->x-hero->x;
    if ((dx>=-RADIUS)&&(dx<RADIUS)) {
      double dy=sprite->y-hero->y;
      if ((dy>=-RADIUS)&&(dy<=RADIUS)) {
        int x=rand()%200;//TODO
        int y=rand()%100;//TODO
        uint8_t xform;
        switch (rand()&3) {
          case 0: xform=0; break;
          case 1: xform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
          case 2: xform=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
          case 3: xform=EGG_XFORM_XREV|EGG_XFORM_SWAP; break;
        }
        store_jigsaw_set(SPRITE->mapid,x,y,xform);
        bm_sound(RID_sound_treasure,0.0);
        sprite->defunct=1;
      }
    }
  }
}

const struct sprite_type sprite_type_jigpiece={
  .name="jigpiece",
  .objlen=sizeof(struct sprite_jigpiece),
  .init=_jigpiece_init,
  .update=_jigpiece_update,
};

/* sprite_sphinx.c
 * Controller for a puzzle where you must spell "SPHINX" with moveable alphabet blocks.
 * We spawn the blocks.
 */
 
#include "game/bellacopia.h"

struct sprite_sphinx {
  struct sprite hdr;
  int eqtrack;
  uint8_t tileid0;
};

#define SPRITE ((struct sprite_sphinx*)sprite)

static void _sphinx_del(struct sprite *sprite) {
}

/* Init.
 */

static int _sphinx_init(struct sprite *sprite) {

  // If I've already been out-riddled, abort. No need to exist.
  if (store_get_fld(NS_fld_sphinx)) return -1;
  
  /* Scan map for the allowable spawn region.
   * It's a rectangle, whose bounds align with my initial position.
   * Do not leave the one map.
   */
  int x=(int)sprite->x;
  int y=(int)sprite->y;
  int w=1,h=1;
  const struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  x-=map->lng*NS_sys_mapw;
  y-=map->lat*NS_sys_maph;
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return -1;
  int x0=x;
  uint8_t tileid=map->v[y*NS_sys_mapw+x];
  while ((x>0)&&(map->v[y*NS_sys_mapw+x-1]==tileid)) { x--; w++; }
  while ((x+w<NS_sys_mapw)&&(map->v[y*NS_sys_mapw+x+w]==tileid)) w++;
  while ((y>0)&&(map->v[(y-1)*NS_sys_mapw+x0]==tileid)) { y--; h++; }
  while ((y+h<NS_sys_maph)&&(map->v[(y+h)*NS_sys_mapw+x0]==tileid)) h++;
  
  /* Pick six cells from among those.
   * Eliminate any whose tileid doesn't match our starting point.
   * Blocks should not start cardinally adjacent to each other, so remove neighbors as we go too.
   */
  uint16_t available[NS_sys_mapw*NS_sys_maph];
  int availablec=0;
  const uint8_t *mrow=map->v+y*NS_sys_mapw+x;
  int yi=0; for (;yi<h;yi++,mrow+=NS_sys_mapw) {
    const uint8_t *mp=mrow;
    int xi=0; for (;xi<w;xi++,mp++) {
      if (*mp!=tileid) continue;
      available[availablec++]=(y+yi)*NS_sys_mapw+x+xi;
    }
  }
  int i=6;
  while (i-->0) {
    if (availablec<1) {
      fprintf(stderr,"%s: blocks region %d,%d,%d,%d too small to hold six blocks\n",__func__,x,y,w,h);
      return -1;
    }
    int avp=rand()%availablec;
    int v=available[avp];
    availablec--;
    memmove(available+avp,available+avp+1,sizeof(uint16_t)*(availablec-avp));
    double bx=map->lng*NS_sys_mapw+v%NS_sys_mapw+0.5;
    double by=map->lat*NS_sys_maph+v/NS_sys_mapw+0.5;
    struct sprite *block=sprite_spawn(bx,by,RID_sprite_sphinxblock,0,0,0,0,0);
    if (!block) return -1;
    SPRITE->tileid0=block->tileid;
    block->tileid+=i;
  }
  
  return 0;
}

/* Update.
 */

static void _sphinx_update(struct sprite *sprite,double elapsed) {
  // As an optimization, we don't scan every frame. Only on the frame after an earthquake ends.
  if (g.eqclock>0.0) {
    SPRITE->eqtrack=1;
  } else if (SPRITE->eqtrack) {
    SPRITE->eqtrack=0;
    double xv[6]={0};
    double yall=-1.0;
    int match=1;
    struct sprite **blockp=GRP(hookpull)->sprv;
    int i=GRP(hookpull)->sprc;
    for (;i-->0;blockp++) {
      struct sprite *block=*blockp;
      if (block->rid!=RID_sprite_sphinxblock) continue;
      int seq=block->tileid-SPRITE->tileid0;
      if ((seq<0)||(seq>=6)) continue;
      xv[seq]=block->x;
      if (yall<0.0) {
        yall=block->y;
      } else if ((block->y<yall-0.250)||(block->y>yall+0.250)) {
        match=0;
        break;
      }
    }
    if (match) {
      for (i=1;i<6;i++) {
        double dx=xv[i]-xv[i-1];
        if ((dx<0.800)||(dx>1.200)) {
          match=0;
          break;
        }
      }
    }
    if (match) {
      bm_sound(RID_sound_secret);
      store_set_fld(NS_fld_sphinx,1);
      g.camera.mapsdirty=1;
      sprite_kill_soon(sprite);
    }
  }
}

/* Type definition.
 */

static void _sphinx_render(struct sprite *sprite,int x,int y) {
}

const struct sprite_type sprite_type_sphinx={
  .name="sphinx",
  .objlen=sizeof(struct sprite_sphinx),
  .del=_sphinx_del,
  .init=_sphinx_init,
  .update=_sphinx_update,
  .render=_sphinx_render,
};

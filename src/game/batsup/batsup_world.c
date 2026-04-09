#include "game/bellacopia.h"
#include "batsup_world.h"

/* Delete.
 */
 
void batsup_world_del(struct batsup_world *world) {
  if (!world) return;
  if (world->spritev) {
    while (world->spritec-->0) batsup_sprite_del(world->spritev[world->spritec]);
    free(world->spritev);
  }
  if (world->ownmap&&world->map) {
    free(world->map);
  }
  free(world);
}

/* New.
 */
 
struct batsup_world *batsup_world_new(struct battle *battle,int mapid) {
  struct batsup_world *world=calloc(1,sizeof(struct batsup_world));
  if (!world) return 0;
  
  world->battle=battle;
  world->sortdir=1;
  world->spriteid_next=1;
  
  /* If a nonzero (mapid) provided, it must name a map in the store.
   */
  if (mapid) {
    if (!(world->map=map_by_id(mapid))) {
      fprintf(stderr,"%s:%d: map:%d not found\n",__FILE__,__LINE__,mapid);
      batsup_world_del(world);
      return 0;
    }
    
  /* (mapid) zero means generate one.
   */
  } else {
    world->ownmap=1;
    if (!(world->map=calloc(1,sizeof(struct map)))) {
      batsup_world_del(world);
      return 0;
    }
    world->map->rov=world->map->v;
    world->map->physics=tilesheet_get_physics(0); // Returns an all-zeroes default.
    world->map->jigctab=tilesheet_get_jigctab(0); // Can't imagine anyone will need this, but let's be safe.
  }
  
  return world;
}

/* Set image for custom map.
 */
 
int batsup_world_set_image(struct batsup_world *world,int imageid) {
  if (!world) return -1;
  if (!world->ownmap) return -1;
  world->map->imageid=imageid;
  world->map->physics=tilesheet_get_physics(imageid);
  world->map->jigctab=tilesheet_get_jigctab(imageid);
  return 0;
}

/* Update.
 */

void batsup_world_update(struct batsup_world *world,double elapsed) {
  struct batsup_sprite **spritep=world->spritev;
  int i=world->spritec;
  for (;i-->0;spritep++) {
    struct batsup_sprite *sprite=*spritep;
    if (sprite->defunct) continue;
    if (!sprite->update) continue;
    sprite->update(sprite,elapsed);
  }
  for (i=world->spritec,spritep=world->spritev+world->spritec-1;i-->0;spritep--) {
    struct batsup_sprite *sprite=*spritep;
    if (!sprite->defunct) continue;
    world->spritec--;
    memmove(spritep,spritep+1,sizeof(void*)*(world->spritec-i));
  }
}

/* Render.
 */
 
#define ORX ((FBW>>1)-((NS_sys_tilesize*NS_sys_mapw)>>1))
#define ORY ((FBH>>1)-((NS_sys_tilesize*NS_sys_maph)>>1))

void batsup_world_render(struct batsup_world *world) {

  // Map. Draw every tile every time. There's no scrolling.
  int dsty=ORY+(NS_sys_tilesize>>1);
  int dstx0=ORX+(NS_sys_tilesize>>1),dstx;
  const uint8_t *cellp=world->map->v;
  graf_set_image(&g.graf,world->map->imageid);
  int yi=NS_sys_maph; for (;yi-->0;dsty+=NS_sys_tilesize) {
    int xi=NS_sys_mapw; for (dstx=dstx0;xi-->0;dstx+=NS_sys_tilesize,cellp++) {
      graf_tile(&g.graf,dstx,dsty,*cellp,0);
    }
  }
  
  // Sort sprites. One pass of a cocktail sort.
  if ((world->spritec>=2)&&((world->sortdir==1)||(world->sortdir==-1))) {
    int first,last,i;
    if (world->sortdir==1) {
      first=0;
      last=world->spritec-2;
    } else {
      first=world->spritec-1;
      last=1;
    }
    for (i=first;i!=last;i+=world->sortdir) {
      struct batsup_sprite *a=world->spritev[i];
      struct batsup_sprite *b=world->spritev[i+world->sortdir];
      int cmp=batsup_sprite_rendercmp(a,b);
      if (cmp==world->sortdir) {
        world->spritev[i]=b;
        world->spritev[i+world->sortdir]=a;
      }
    }
    if (world->sortdir==1) {
      world->sortdir=-1;
    } else {
      world->sortdir=1;
    }
  }
  
  // Sprites.
  struct batsup_sprite **spritep=world->spritev;
  int i=world->spritec;
  for (;i-->0;spritep++) {
    struct batsup_sprite *sprite=*spritep;
    dstx=ORX+(int)(sprite->x*NS_sys_tilesize);
    dsty=ORY+(int)(sprite->y*NS_sys_tilesize);
    if (sprite->render) {
      sprite->render(sprite,dstx,dsty);
    } else {
      graf_set_image(&g.graf,sprite->imageid);
      graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
    }
  }
}

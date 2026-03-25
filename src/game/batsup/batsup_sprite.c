#include "game/bellacopia.h"
#include "batsup_world.h"

/* Delete.
 */
 
void batsup_sprite_del(struct batsup_sprite *sprite) {
  if (!sprite) return;
  free(sprite);
}

/* New.
 */
 
struct batsup_sprite *batsup_sprite_new(struct batsup_world *world,int id,int objlen) {
  if (!world) return 0;
  if (objlen<=0) objlen=sizeof(struct batsup_sprite);
  else if (objlen<sizeof(struct batsup_sprite)) return 0;
  struct batsup_sprite *sprite=calloc(1,objlen);
  if (!sprite) return 0;
  sprite->world=world;
  sprite->id=id;
  sprite->objlen=objlen;
  sprite->x=NS_sys_mapw*0.5;
  sprite->y=NS_sys_maph*0.5;
  sprite->layer=100;
  sprite->imageid=world->map->imageid;
  return sprite;
}

/* Spawn.
 */
 
struct batsup_sprite *batsup_sprite_spawn(struct batsup_world *world,int id,int objlen) {
  if (!world) return 0;
  if (id<0) return 0;
  if (id>0) {
    if (batsup_sprite_by_id(world,id)) return 0;
  } else {
    for (;;) {
      if (world->spriteid_next>=INT_MAX) return 0;
      id=world->spriteid_next++;
      if (!batsup_sprite_by_id(world,id)) break;
    }
  }
  if (world->spritec>=world->spritea) {
    int na=world->spritea+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(world->spritev,sizeof(void*)*na);
    if (!nv) return 0;
    world->spritev=nv;
    world->spritea=na;
  }
  struct batsup_sprite *sprite=batsup_sprite_new(world,id,objlen);
  if (!sprite) return 0;
  world->spritev[world->spritec++]=sprite;
  return sprite;
}

/* Get by id.
 */

struct batsup_sprite *batsup_sprite_by_id(struct batsup_world *world,int id) {
  if (id<1) return 0;
  struct batsup_sprite **p=world->spritev;
  int i=world->spritec;
  for (;i-->0;p++) if ((*p)->id==id) return *p;
  return 0;
}

/* Move with physics.
 */

int batsup_sprite_move(struct batsup_sprite *sprite,double dx,double dy) {

  // Invalid or not solid, easy.
  if (!sprite||sprite->defunct) return 0;
  if (!sprite->solid) {
    sprite->x+=dx;
    sprite->y+=dy;
    return 1;
  }
  
  // If they're both nonzero, reenter per axis. And if both zero, get out.
  if ((dx<-0.0)||(dx>0.0)) {
    if ((dy<-0.0)||(dy>0.0)) {
      int xok=batsup_sprite_move(sprite,dx,0.0);
      int yok=batsup_sprite_move(sprite,0.0,dy);
      return (xok||yok)?1:0;
    }
  } else {
    if ((dy>=-0.0)&&(dy<=0.0)) return 0;
  }
  
  // Capture the desired position, both the center and the hitbox bounds.
  #define RADIUS 0.450
  #define SMIDGE 0.001
  double nx=sprite->x+dx,ny=sprite->y+dy,nl,nr,nt,nb;
  #define REBOX {\
    nl=nx-RADIUS; \
    nr=nx+RADIUS; \
    nt=ny-RADIUS; \
    nb=ny+RADIUS; \
  }
  REBOX
  
  /* Call me for every physical box we might encounter.
   * If it collides with the target box, I'll update the target and abort if we end up noop or backward.
   */
  #define CHECK(_ol,_or,_ot,_ob) { \
    double ol=(_ol),or=(_or),ot=(_ot),ob=(_ob); \
    if ((nl>=or)||(nr<=ol)||(nt>=ob)||(nb<=ot)) { \
      /* No collision. */ \
    } else if (dx<-0.0) { \
      nx=or+RADIUS; \
      if (nx>=sprite->x) return 0; \
      REBOX \
    } else if (dx>0.0) { \
      nx=ol-RADIUS; \
      if (nx<=sprite->x) return 0; \
      REBOX \
    } else if (dy<-0.0) { \
      ny=ob+RADIUS; \
      if (ny>=sprite->y) return 0; \
      REBOX \
    } else { \
      ny=ot-RADIUS; \
      if (ny<=sprite->y) return 0; \
      REBOX \
    } \
  }
  
  /* Check map edges.
   */
  CHECK(-100.0,0.0,-100.0,100.0)
  CHECK(NS_sys_mapw,100.0,-100.0,100.0)
  CHECK(-100.0,100.0,-100.0,0.0)
  CHECK(-100.0,100.0,NS_sys_maph,100.0)
  
  /* Check map.
   */
  int cola=(int)(nl+SMIDGE); if (cola<0) cola=0;
  int colz=(int)(nr-SMIDGE); if (colz>=NS_sys_mapw) colz=NS_sys_mapw-1;
  int rowa=(int)(nt+SMIDGE); if (rowa<0) rowa=0;
  int rowz=(int)(nb-SMIDGE); if (rowz>=NS_sys_maph) rowz=NS_sys_maph-1;
  const uint8_t *maprow=sprite->world->map->v+rowa*NS_sys_mapw+cola;
  int row=rowa;
  double mapy=rowa;
  for (;row<=rowz;row++,mapy+=1.0,maprow+=NS_sys_mapw) {
    const uint8_t *mapp=maprow;
    int col=cola;
    double mapx=cola;
    for (;col<=colz;col++,mapx+=1.0,mapp++) {
      uint8_t physics=sprite->world->map->physics[*mapp];
      if ((physics!=NS_physics_vacant)&&(physics!=NS_physics_safe)) {
        CHECK(mapx,mapx+1.0,mapy,mapy+1.0)
      }
    }
  }
  
  /* Check other solid sprites.
   */
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (!other->solid) continue;
    if (other->defunct) continue;
    if (other==sprite) continue;
    CHECK(other->x-RADIUS,other->x+RADIUS,other->y-RADIUS,other->y+RADIUS)
  }
  
  // OK, commit it.
  #undef RADIUS
  #undef SMIDGE
  #undef REBOX
  sprite->x=nx;
  sprite->y=ny;
  return 1;
}

/* Compare for render.
 */
 
int batsup_sprite_rendercmp(const struct batsup_sprite *a,const struct batsup_sprite *b) {
  if (!a||!b) return 0;
  if (a->layer<b->layer) return -1;
  if (a->layer>b->layer) return 1;
  if (a->y<b->y) return -1;
  if (a->y>b->y) return 1;
  return 0;
}

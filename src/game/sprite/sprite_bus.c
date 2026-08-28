/* sprtype_bus.c
 * Spawn at the hero's position. We'll knock it a bit vertically to land somewhere appropriate.
 */
 
#include "game/bellacopia.h"

#define BUS_TTL 5.0

struct sprite_bus {
  struct sprite hdr;
  double stopx;
  int stopped;
  double alpha; // 0..1
  int departing;
  double ttl;
  double cooldown;
};

#define SPRITE ((struct sprite_bus*)sprite)

/* Choose a starting position.
 * Hero spawns us at her exact position; obviously we don't want to land there.
 * The ideal is either directly above or below her.
 * But fuzz it out a little if those fail. Don't give up too easy.
 * If we succeed, (sprite->x,y) are overwritten.
 */
 
static int bus_try_landing_zone(struct sprite *sprite,double x,double y) {
  sprite->x=x;
  sprite->y=y;
  return sprite_test_position(sprite);
}
 
static int bus_choose_landing_zone(struct sprite *sprite) {
  double x0=sprite->x;
  double y0=sprite->y;
  const double yideal=1.600;
  const double xideal=2.600;
  
  // Most preferred: Directly above or below.
  if (bus_try_landing_zone(sprite,x0,y0+yideal)) return 0;
  if (bus_try_landing_zone(sprite,x0,y0-yideal)) return 0;
  
  // Not great but hey: Directly left or right.
  if (bus_try_landing_zone(sprite,x0-xideal,y0)) return 0;
  if (bus_try_landing_zone(sprite,x0+xideal,y0)) return 0;
  
  // Try some diagonals at a slightly higher radius.
  const double ywide=1.800;
  const double xwide=2.800;
  const int stepc=7;
  int step=1;
  for (;step<stepc;step++) {
    double t=(step*M_PI*0.5)/stepc;
    double dx=cos(t)*xwide;
    double dy=sin(t)*ywide;
    if (bus_try_landing_zone(sprite,x0+dx,y0+dy)) return 0;
    if (bus_try_landing_zone(sprite,x0-dx,y0+dy)) return 0;
    if (bus_try_landing_zone(sprite,x0+dx,y0-dy)) return 0;
    if (bus_try_landing_zone(sprite,x0-dx,y0-dy)) return 0;
  }
  
  // OK I give up.
  return -1;
}

/* Init.
 */

static int _bus_init(struct sprite *sprite) {

  // If a bus already exists, reject. We're not made of busses.
  struct sprite **otherp=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other==sprite) continue;
    if (other->defunct) continue;
    if (other->type==&sprite_type_bus) return -1;
  }

  // If we can't find a good position, abort.
  if (bus_choose_landing_zone(sprite)<0) return -1;
  
  SPRITE->stopx=sprite->x;
  sprite->x+=10.0;
  SPRITE->alpha=0.0;
  SPRITE->ttl=BUS_TTL;
  return 0;
}

/* Update.
 */

static void _bus_update(struct sprite *sprite,double elapsed) {

  if (SPRITE->cooldown>0.0) SPRITE->cooldown-=elapsed;

  // If the TTL expires, start departing, ready or not.
  if ((SPRITE->ttl-=elapsed)<=0.0) {
    SPRITE->departing=1;
  }

  // Approaching the stop?
  if (!SPRITE->stopped) {
    if ((sprite->x-=15.0*elapsed)<=SPRITE->stopx) {
      sprite->x=SPRITE->stopx;
      SPRITE->stopped=1;
      SPRITE->alpha=1.0;
    }
    if ((SPRITE->alpha+=2.000*elapsed)>=1.0) SPRITE->alpha=1.0;
    return;
  }

  // Departing?
  if (SPRITE->departing) {
    sprite->x-=15.0*elapsed;
    if ((SPRITE->alpha-=2.000*elapsed)<=0.0) {
      sprite_kill_soon(sprite);
    }
  }
}

/* Render.
 */

static void _bus_render(struct sprite *sprite,int x,int y) {
  const int ht=NS_sys_tilesize>>1;
  uint8_t tileid=sprite->tileid;
  if (SPRITE->stopped&&!SPRITE->departing) tileid+=3;
  graf_set_image(&g.graf,sprite->imageid);
  int alpha=(int)(SPRITE->alpha*255.0);
  if (alpha<0) return;
  if (alpha<0xff) graf_set_alpha(&g.graf,alpha);
  graf_tile(&g.graf,x-NS_sys_tilesize,y-ht,tileid+0x00,0);
  graf_tile(&g.graf,x                ,y-ht,tileid+0x01,0);
  graf_tile(&g.graf,x+NS_sys_tilesize,y-ht,tileid+0x02,0);
  graf_tile(&g.graf,x-NS_sys_tilesize,y+ht,tileid+0x10,0);
  graf_tile(&g.graf,x                ,y+ht,tileid+0x11,0);
  graf_tile(&g.graf,x+NS_sys_tilesize,y+ht,tileid+0x12,0);
  graf_set_alpha(&g.graf,0xff);
}

/* Type definition.
 */

static int bus_cb_dialogue(int busstop,void *userdata) {
  struct sprite *sprite=userdata;
  if (busstop<=0) return 0;
  if (GRP(hero)->sprc<1) return 0;
  struct sprite *hero=GRP(hero)->sprv[0];
  sprite_hero_warp_busstop(hero,busstop);
  SPRITE->departing=1;
  return 1;
}

struct trial { double x,y,d; };

static int trialcmp(const void *a,const void *b) {
  const struct trial *A=a,*B=b;
  if (A->d<B->d) return -1;
  if (A->d>B->d) return 1;
  return 0;
}

static int bus_is_lawsuit_situation(struct sprite *sprite,struct sprite *hero) {
  if (sprite_test_position(hero)) return 0;
  double x0=hero->x;
  double y0=hero->y;
  double bl=sprite->x+sprite->hbl;
  double br=sprite->x+sprite->hbr;
  double bt=sprite->y+sprite->hbt;
  double bb=sprite->y+sprite->hbb;
  double dl=hero->x-bl;
  double dr=br-hero->x;
  double dt=hero->y-bt;
  double db=bb-hero->y;
  struct trial trialv[4]={
    {bl-hero->hbr,y0,dl},
    {br-hero->hbl,y0,dr},
    {x0,bt-hero->hbb,dt},
    {x0,bb-hero->hbt,dt},
  };
  qsort(trialv,4,sizeof(struct trial),trialcmp);
  struct trial *trial=trialv;
  int i=4;
  for (;i-->0;trial++) {
    hero->x=trial->x;
    hero->y=trial->y;
    if (sprite_test_position(hero)) return 1;
  }
  hero->x=x0;
  hero->y=y0;
  return 1;
}

static void _bus_collide(struct sprite *sprite,struct sprite *hero) {
  if (SPRITE->departing||!SPRITE->stopped) return;
  if (!hero||(hero->type!=&sprite_type_hero)) return;
  if (SPRITE->cooldown>0.0) return;
  SPRITE->cooldown=0.250;
  
  // It's pretty easy to get run over by the bus. Bus moves without physics, but is solid. (must be so; we don't want to catch on every blockage in the run-up).
  // So if hero is in a state of collision, try to force her out instead of triggering the modal.
  // Worst case scenario if this doesn't work, she can wait for us to depart.
  if (bus_is_lawsuit_situation(sprite,hero)) return;
  
  struct modal_args_dialogue args={
    .rid=RID_strings_item,
    .strix=100,
    .cb=bus_cb_dialogue,
    .userdata=sprite,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  int p=0;
  for (;;p++) {
    int busstop=busstop_by_index(p);
    if (busstop<=0) break;
    int strix=busstop_name_by_index(p);
    modal_dialogue_add_option_string_id(modal,RID_strings_item,strix,busstop);
  }
}

const struct sprite_type sprite_type_bus={
  .name="bus",
  .objlen=sizeof(struct sprite_bus),
  .init=_bus_init,
  .update=_bus_update,
  .render=_bus_render,
  .collide=_bus_collide,
};

/* sprite_landmine.c
 * For exploding.
 */
 
#include "game/bellacopia.h"

struct sprite_landmine {
  struct sprite hdr;
  uint16_t fldid;
  uint8_t tileid0;
  double animinterval;
  double animclock;
  int animframe;
  double exploded; // Counts down to deletion.
  struct pumpkin {
    struct sprite *pumpkin; // WEAK; validate before use.
    double dx,dy;
  } *pumpkinv;
  int pumpkinc,pumpkina;
};

#define SPRITE ((struct sprite_landmine*)sprite)

/* Cleanup.
 */
 
static void _landmine_del(struct sprite *sprite) {
  if (SPRITE->pumpkinv) free(SPRITE->pumpkinv);
}

/* Init.
 */
 
static int _landmine_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->fldid=(sprite->arg[0]<<8)|sprite->arg[1];
  if (store_get_fld(SPRITE->fldid)) return -1;
  
  // Randomize animation so they don't appear in sync.
  SPRITE->animinterval=0.200+((rand()&0xffff)*0.100)/65535.0;
  SPRITE->animclock=((rand()&0xffff)*SPRITE->animinterval)/65535.0;
  SPRITE->animframe=rand()&1;
  
  return 0;
}

/* Register a pumpkin.
 */

static void landmine_record_pumpkin(struct sprite *sprite,struct sprite *pumpkin,double dx,double dy) {
  if (SPRITE->pumpkinc>=SPRITE->pumpkina) {
    int na=SPRITE->pumpkina+8;
    if (na>INT_MAX/sizeof(struct pumpkin)) return;
    void *nv=realloc(SPRITE->pumpkinv,sizeof(struct pumpkin)*na);
    if (!nv) return;
    SPRITE->pumpkinv=nv;
    SPRITE->pumpkina=na;
  }
  struct pumpkin *dst=SPRITE->pumpkinv+SPRITE->pumpkinc++;
  dst->pumpkin=pumpkin;
  dst->dx=dx;
  dst->dy=dy;
}

/* Explode.
 */
 
void sprite_landmine_explode(struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_landmine)) return;
  
  bm_sound(RID_sound_bombblow);
  store_set_fld(SPRITE->fldid,1);
  SPRITE->exploded=1.000;
  sprite->layer=150; // Now we're the flames decoration; render on top of the rest.
  
  /* Any moveable sprite in my blast radius gets recorded for further velocity adjustment.
   * This is copied from sprite_bomb.c.
   */
  const double HURT_RADIUS=4.000;
  const double HURT_RADIUS_2=HURT_RADIUS*HURT_RADIUS;
  const double MAX_EJECT_VELOCITY=15.0;
  struct sprite **otherp=GRP(moveable)->sprv;
  int otheri=GRP(moveable)->sprc;
  for (;otheri-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other==sprite) continue;
    if (other->type==&sprite_type_bomb) continue; // Don't eject bombs, it looks weird.
    double dx=other->x-sprite->x;
    double dy=other->y-sprite->y;
    double d2=dx*dx+dy*dy;
    if (d2>=HURT_RADIUS_2) continue;
    if (d2<0.001) { // Improbably small distance. Make something up.
      dx=0.0;
      dy=1.0;
      d2=1.0;
    }
    double distance=sqrt(d2);
    
    if ((other->type==&sprite_type_hero)&&(distance<2.0)) {
      hero_injure(other,sprite);
    }
    
    double velocity=(1.0-distance/HURT_RADIUS)*MAX_EJECT_VELOCITY;
    dx=(dx*velocity)/distance;
    dy=(dy*velocity)/distance;
    landmine_record_pumpkin(sprite,other,dx,dy);
  }
}

/* Pumpkin deceleration.
 */

static void reduce_delta(struct pumpkin *pumpkin,double elapsed) {
  const double rate=20.0;
  if (pumpkin->dx<0.0) {
    if ((pumpkin->dx+=rate*elapsed)>0.0) pumpkin->dx=0.0;
  } else {
    if ((pumpkin->dx-=rate*elapsed)<0.0) pumpkin->dx=0.0;
  }
  if (pumpkin->dy<0.0) {
    if ((pumpkin->dy+=rate*elapsed)>0.0) pumpkin->dy=0.0;
  } else {
    if ((pumpkin->dy-=rate*elapsed)<0.0) pumpkin->dy=0.0;
  }
}

/* Update.
 */
 
static void _landmine_update(struct sprite *sprite,double elapsed) {

  // If we've exploded already, count down and die.
  // Also move all the pumpkins.
  if (SPRITE->exploded>0.0) {
    if ((SPRITE->exploded-=elapsed)<=0.0) sprite_kill_soon(sprite);
    struct pumpkin *pumpkin=SPRITE->pumpkinv+SPRITE->pumpkinc-1;
    int i=SPRITE->pumpkinc;
    for (;i-->0;pumpkin--) {
      if (!sprite_is_alive(pumpkin->pumpkin)) {
        SPRITE->pumpkinc--;
        memmove(pumpkin,pumpkin+1,sizeof(struct pumpkin)*(SPRITE->pumpkinc-i));
      } else {
        sprite_move(pumpkin->pumpkin,pumpkin->dx*elapsed,pumpkin->dy*elapsed);
        reduce_delta(pumpkin,elapsed);
      }
    }
    return;
  }

  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=SPRITE->animinterval;
    if (++(SPRITE->animframe)>=2) SPRITE->animframe=0;
    sprite->tileid=SPRITE->tileid0+SPRITE->animframe;
  }
  
  struct sprite **otherp=GRP(solid)->sprv;
  int otheri=GRP(solid)->sprc;
  for (;otheri-->0;otherp++) {
    struct sprite *other=*otherp;
    if (!sprite_hero_is_grounded(other)) continue;
    if (other->type==&sprite_type_bomb) continue; // Placing a bomb on a landmine shouldn't blow either. Let the fuse burn down.
    double dx=other->x-sprite->x;
    double dy=other->y-sprite->y;
    double d2=dx*dx+dy*dy;
    if (d2>0.300) continue;
    sprite_landmine_explode(sprite);
    return;
  }
}

/* Render.
 */
 
static void _landmine_render(struct sprite *sprite,int dstx,int dsty) {
  
  /* If we've exploded, we do something different.
   */
  if (SPRITE->exploded>0.0) {
    int w=NS_sys_tilesize*2;
    int srcxlo=160,srcylo=160,srcxhi=192,srcyhi=160;
    double lot=SPRITE->exploded*3.0;
    double hit=SPRITE->exploded*-5.0;
    double p=(SPRITE->exploded+1.000)/1.000;
    int loalpha=(int)(p*255.0); if (loalpha>0xff) loalpha=0xff;
    int hialpha=0xff;
    if (p<0.250) {
      hialpha=(int)(p*4*255.0); if (hialpha>0xff) hialpha=0xff;
    }
    graf_set_image(&g.graf,RID_image_hero);
    graf_set_alpha(&g.graf,loalpha);
    graf_decal_rotate(&g.graf,dstx,dsty,srcxlo,srcylo,w,sin(lot),cos(lot),1.0);
    graf_set_alpha(&g.graf,hialpha);
    graf_decal_rotate(&g.graf,dstx,dsty,srcxhi,srcyhi,w,sin(hit),cos(hit),1.0);
    graf_set_alpha(&g.graf,0xff);
    return;
  }
  
  // Not exploded yet, it's the default.
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_landmine={
  .name="landmine",
  .objlen=sizeof(struct sprite_landmine),
  .del=_landmine_del,
  .init=_landmine_init,
  .update=_landmine_update,
  .render=_landmine_render,
};

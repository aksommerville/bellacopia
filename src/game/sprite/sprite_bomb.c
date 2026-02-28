/* sprite_bomb.c
 */
 
#include "game/bellacopia.h"

#define FUSETIME 2.000
#define SMOKETIME 1.000
#define HURT_RADIUS 4.000
#define HURT_RADIUS_2 (HURT_RADIUS*HURT_RADIUS)
#define MAX_EJECT_VELOCITY 20.0

struct sprite_bomb {
  struct sprite hdr;
  uint8_t tileid0;
  double fuse;
  struct pumpkin {
    struct sprite *pumpkin; // WEAK; validate before use.
    double dx,dy;
  } *pumpkinv;
  int pumpkinc,pumpkina;
};

#define SPRITE ((struct sprite_bomb*)sprite)

static void _bomb_del(struct sprite *sprite) {
  if (SPRITE->pumpkinv) free(SPRITE->pumpkinv);
}

static int _bomb_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->fuse=FUSETIME;
  return 0;
}

static void bomb_record_pumpkin(struct sprite *sprite,struct sprite *pumpkin,double dx,double dy) {
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

static void bomb_blow(struct sprite *sprite) {
  bm_sound(RID_sound_bombblow);
  sprite->layer=150;
  sprite_group_remove(GRP(solid),sprite);
  
  if (GRP(hero)->sprc>=0) {
    sprite_hero_unbury_treasure(GRP(hero)->sprv[0],(int)sprite->x,(int)sprite->y);
  }
  
  /* Anything in my blast radius gets recorded for further velocity adjustment.
   */
  struct sprite **otherp=GRP(moveable)->sprv;
  int otheri=GRP(moveable)->sprc;
  for (;otheri-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other==sprite) continue;
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
    bomb_record_pumpkin(sprite,other,dx,dy);
  }
}

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

static void _bomb_update(struct sprite *sprite,double elapsed) {
  if (SPRITE->fuse>0.0) {
    if ((SPRITE->fuse-=elapsed)<=0.0) {
      bomb_blow(sprite);
    }
  } else {
    if ((SPRITE->fuse-=elapsed)<=-SMOKETIME) {
      sprite_kill_soon(sprite);
    }
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
  }
}

static void _bomb_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  if (SPRITE->fuse>0.0) {
    uint8_t bodytile=sprite->tileid;
    uint8_t fusetile=sprite->tileid;
    if (((int)(SPRITE->fuse*6.0))%6>=3) bodytile+=1;
    int fuselen=(int)((SPRITE->fuse*4.0)/FUSETIME);
    if (fuselen<0) fuselen=0; else if (fuselen>3) fuselen=3;
    int fuseframe=((int)(SPRITE->fuse*10.0))&1;
    fusetile+=8-fuselen*2+fuseframe;
    graf_tile(&g.graf,x,y,bodytile,sprite->xform);
    graf_tile(&g.graf,x+8,y-8,fusetile,sprite->xform);
  } else {
    int w=NS_sys_tilesize*2;
    int srcxlo=((sprite->tileid&15)+10)*NS_sys_tilesize;
    int srcylo=((sprite->tileid>>4)-1)*NS_sys_tilesize;
    int srcxhi=srcxlo+w;
    int srcyhi=srcylo;
    double lot=SPRITE->fuse*3.0;
    double hit=SPRITE->fuse*-5.0;
    double p=(SPRITE->fuse+SMOKETIME)/SMOKETIME;
    int loalpha=(int)(p*255.0); if (loalpha>0xff) loalpha=0xff;
    int hialpha=0xff;
    if (p<0.250) {
      hialpha=(int)(p*4*255.0); if (hialpha>0xff) hialpha=0xff;
    }
    graf_set_alpha(&g.graf,loalpha);
    graf_decal_rotate(&g.graf,x,y,srcxlo,srcylo,w,sin(lot),cos(lot),1.0);
    graf_set_alpha(&g.graf,hialpha);
    graf_decal_rotate(&g.graf,x,y,srcxhi,srcyhi,w,sin(hit),cos(hit),1.0);
    graf_set_alpha(&g.graf,0xff);
  }
}

const struct sprite_type sprite_type_bomb={
  .name="bomb",
  .objlen=sizeof(struct sprite_bomb),
  .del=_bomb_del,
  .init=_bomb_init,
  .update=_bomb_update,
  .render=_bomb_render,
};

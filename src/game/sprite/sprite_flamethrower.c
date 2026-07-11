/* sprite_flamethrower.c
 * Creates an axis-aligned flame, either on a timer or in response to a store field.
 */
 
#include "game/bellacopia.h"

struct sprite_flamethrower {
  struct sprite hdr;
  int fldid; // If zero, use (period).
  int offvalue;
  int length;
  int dx,dy; // cardinal unit
  double period; // If zero (and fldid zero), always on.
  double halfperiod;
  double clock;
  int state;
  int store_listener;
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_flamethrower*)sprite)

/* Cleanup.
 */
 
static void _flamethrower_del(struct sprite *sprite) {
  store_unlisten(SPRITE->store_listener);
}

/* Store callback.
 */
 
static void flamethrower_cb_store(char type,int id,int value,void *userdata) {
  struct sprite *sprite=userdata;
  if ((type=='f')&&(id==SPRITE->fldid)) {
    if (value==SPRITE->offvalue) {
      SPRITE->state=0;
    } else {
      SPRITE->state=1;
    }
  }
}

/* Init.
 */
 
static int _flamethrower_init(struct sprite *sprite) {
  SPRITE->fldid=(sprite->arg[0]<<8)|sprite->arg[1];
  if (SPRITE->fldid) { // Controlled by store.
    SPRITE->offvalue=sprite->arg[2];
    if (store_get_fld(SPRITE->fldid)==SPRITE->offvalue) {
      SPRITE->state=0;
    } else {
      SPRITE->state=1;
    }
    SPRITE->store_listener=store_listen('f',flamethrower_cb_store,sprite);
  } else if (sprite->arg[2]) { // Timed.
    SPRITE->period=0.500+((sprite->arg[2]>>4)*3.000)/16.0;
    SPRITE->clock=((sprite->arg[2]&15)*SPRITE->period)/16.0;
    SPRITE->halfperiod=SPRITE->period*0.5;
    SPRITE->state=(SPRITE->clock>=SPRITE->halfperiod)?1:0;
  } else { // Always on.
    SPRITE->period=0.0;
    SPRITE->state=1;
  }
  SPRITE->length=(sprite->arg[3]>>4)+1;
  switch (sprite->arg[3]&3) {
    case 0: SPRITE->dx=-1; sprite->xform=EGG_XFORM_XREV; break;
    case 1: SPRITE->dx=1; sprite->xform=0; break;
    case 2: SPRITE->dy=-1; sprite->xform=EGG_XFORM_SWAP|EGG_XFORM_XREV; break;
    case 3: SPRITE->dy=1; sprite->xform=EGG_XFORM_SWAP; break;
  }
  return 0;
}

/* Deal damage to a colliding sprite, if it does that.
 * The flamethrower itself will land here, watch out.
 */
 
static void flamethrower_hurt(struct sprite *sprite,struct sprite *victim) {

  if (victim->type==&sprite_type_flamethrower) return;
  
  /* For ones we do hurt, if possible, report a collision perpendicular to the flame.
   * Don't let the collision point coincide with the victim, cheat it out if necessary to force movement.
   */
  double x=sprite->x,y=sprite->y;
  if (SPRITE->dx) {
    x=victim->x;
    if (sprite->y<victim->y) y-=0.125; else y+=0.125;
  } else {
    y=victim->y;
    if (sprite->x<victim->x) x-=0.125; else x+=0.125;
  }
  
  if (victim->type==&sprite_type_hero) {
    hero_injure_at(victim,sprite,x,y);
    return;
  }
  
  if (victim->type==&sprite_type_monster) {
    sprite_monster_shock(victim,x,y);
    return;
  }
  
  if (victim->type==&sprite_type_princess) {
    sprite_princess_whack(victim,x,y);
    return;
  }
}

/* Get my hurtbox.
 */
 
static void flamethrower_hurtbox(double *l,double *r,double *t,double *b,const struct sprite *sprite) {
  const double barrel_radius=0.333;
  
  if (SPRITE->dx<0) {
    *t=sprite->y-barrel_radius;
    *b=sprite->y+barrel_radius;
    *r=sprite->x;
    *l=sprite->x-SPRITE->length-barrel_radius;
  
  } else if (SPRITE->dx>0) {
    *t=sprite->y-barrel_radius;
    *b=sprite->y+barrel_radius;
    *l=sprite->x;
    *r=sprite->x+SPRITE->length+barrel_radius;
  
  } else if (SPRITE->dy<0) {
    *l=sprite->x-barrel_radius;
    *r=sprite->x+barrel_radius;
    *t=sprite->y-SPRITE->length-barrel_radius;
    *b=sprite->y;
  
  } else {
    *l=sprite->x-barrel_radius;
    *r=sprite->x+barrel_radius;
    *t=sprite->y;
    *b=sprite->y+SPRITE->length+barrel_radius;
  }
}

/* Update.
 */
 
static void _flamethrower_update(struct sprite *sprite,double elapsed) {

  /* If we use a timer, tick it.
   */
  if (SPRITE->fldid) {
    // No update necessary; store callback does it.
  } else if (SPRITE->period>0.0) {
    if ((SPRITE->clock-=elapsed)<=0.0) {
      SPRITE->clock+=SPRITE->period;
    }
    if (SPRITE->clock>=SPRITE->halfperiod) SPRITE->state=1;
    else SPRITE->state=0;
  }
  
  /* Tick the animation always, why not.
   */
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.125;
    if (++(SPRITE->animframe)>=2) SPRITE->animframe=0;
  }
  
  /* If burning, check for victims.
   * We don't have a "fragile" group, so I'm using moveable, and checking known types.
   */
  if (SPRITE->state) {
    double hl,hr,ht,hb;
    flamethrower_hurtbox(&hl,&hr,&ht,&hb,sprite);
    struct sprite **otherp=GRP(moveable)->sprv;
    int i=GRP(moveable)->sprc;
    for (;i-->0;otherp++) {
      struct sprite *other=*otherp;
      if (other->x+other->hbr<hl) continue;
      if (other->x+other->hbl>hr) continue;
      if (other->y+other->hbb<ht) continue;
      if (other->y+other->hbt>hb) continue;
      flamethrower_hurt(sprite,other);
    }
  }
}

/* Render.
 */
 
static void _flamethrower_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  
  // When not burninating, it's just the base tile.
  if (!SPRITE->state) {
    graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);

  // A bit more action while burninating.
  } else {
    uint8_t tileid_base=sprite->tileid+0x10;
    uint8_t tileid_barrel=sprite->tileid+1;
    uint8_t tileid_tip=sprite->tileid+2;
    if (SPRITE->animframe) {
      tileid_barrel+=0x10;
      tileid_tip+=0x10;
    }
    graf_tile(&g.graf,x,y,tileid_base,sprite->xform);
    int fx=x+SPRITE->dx*NS_sys_tilesize;
    int fy=y+SPRITE->dy*NS_sys_tilesize;
    int i=SPRITE->length-1;
    for (;i-->0;fx+=SPRITE->dx*NS_sys_tilesize,fy+=SPRITE->dy*NS_sys_tilesize) {
      graf_tile(&g.graf,fx,fy,tileid_barrel,sprite->xform);
    }
    graf_tile(&g.graf,fx,fy,tileid_tip,sprite->xform);
  }
  
  // Then a 10x2px phase indicator on top of the base, for timed flamethrowers.
  if (!SPRITE->fldid&&(SPRITE->period>0.0)) {
    const int len=10;
    const int off=(NS_sys_tilesize>>1)-2;
    double t=SPRITE->clock/SPRITE->period+0.5;
    if (t>=1.0) t-=1.0;
    int thumbp=len-(t*len*2.0);
    int boxx,boxy,boxw,boxh,barx,bary,barw,barh;
    if (SPRITE->dx) {
      boxy=bary=y-1;
      boxh=barh=2;
      boxw=barw=len;
      if (SPRITE->dx>0) {
        boxx=x-off;
        barx=boxx+thumbp;
      } else {
        boxx=x+off-len;
        barx=boxx-thumbp;
      }
    } else {
      boxx=barx=x-1;
      boxw=barw=2;
      boxh=barh=len;
      if (SPRITE->dy>0) {
        boxy=y-off;
        bary=boxy+thumbp;
      } else {
        boxy=y+off-len;
        bary=boxy-thumbp;
      }
    }
    if (barx<boxx) { barw-=boxx-barx; barx=boxx; }
    else if (barx+barw>boxx+boxw) barw=boxx+boxw-barx;
    if (bary<boxy) { barh-=boxy-bary; bary=boxy; }
    else if (bary+barh>boxy+boxh) barh=boxy+boxh-bary;
    graf_fill_rect(&g.graf,boxx,boxy,boxw,boxh,0x001808ff);
    graf_fill_rect(&g.graf,barx,bary,barw,barh,0xff1040ff);
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_flamethrower={
  .name="flamethrower",
  .objlen=sizeof(struct sprite_flamethrower),
  .del=_flamethrower_del,
  .init=_flamethrower_init,
  .update=_flamethrower_update,
  .render=_flamethrower_render,
};

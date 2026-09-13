/* sprite_rhinoceros.c
 * When hero enters my line of scent, I charge horizontally and hurt her.
 * Must be placed with a solid cell immediately to my left or two meters to my right.
 */
 
#include "game/bellacopia.h"

struct sprite_rhinoceros {
  struct sprite hdr;
  int orient; // -1=leftward, 1=rightward
  double x0,y0;
  double sniffclock; // Counts down from when hero first enters line of scent.
  int charge;
  double animclock;
  int animframe; // Only relevant during charge.
};

#define SPRITE ((struct sprite_rhinoceros*)sprite)

/* Cell is a blockage?
 */
 
static int rhinoceros_is_blockage(const struct map *map,int x,int y) {
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return 0;
  switch (map->physics[map->v[y*NS_sys_mapw+x]]) {
    case NS_physics_solid:
    case NS_physics_water:
    case NS_physics_hole:
    case NS_physics_grabbable:
    case NS_physics_vanishable:
      return 1;
  }
  return 0;
}

/* Init.
 */
 
static int _rhinoceros_init(struct sprite *sprite) {
  SPRITE->x0=sprite->x;
  SPRITE->y0=sprite->y;
  
  /* Orientation is determined by examining the map.
   */
  const struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  int subx=(int)sprite->x-map->lng*NS_sys_mapw;
  int suby=(int)sprite->y-map->lat*NS_sys_maph;
  if (rhinoceros_is_blockage(map,subx-1,suby)) SPRITE->orient=1;
  else if (rhinoceros_is_blockage(map,subx+2,suby)) SPRITE->orient=-1;
  else {
    fprintf(stderr,"rhinoceros requires a solid cell at either (x-1,y) or (x+2,y) on the same map\n");
    return -1;
  }
  
  return 0;
}

/* Nonzero if there's a hero in our line of scent.
 */
 
static int rhinoceros_hero_present(struct sprite *sprite) {
  if (g.bugspray>0.0) return 0;
  // On the fence about this... Should vanishing cream and flash hide me too? They affect vision, not scent.
  //if (g.vanishing>0.0) return 0;
  //if (g.flash>0.0) return 0;
  if (GRP(hero)->sprc<1) return 0;
  struct sprite *hero=GRP(hero)->sprv[0];
  double ylo=sprite->y-0.500;
  double yhi=sprite->y+0.500;
  double xlo=sprite->x;
  double xhi=sprite->x;
  if (SPRITE->orient>0) {
    xhi=999.999;
  } else {
    xlo=0.0;
  }
  if (hero->y<ylo) return 0;
  if (hero->y>yhi) return 0;
  if (hero->x<xlo) return 0;
  if (hero->x>xhi) return 0;
  return 1;
}

/* Check for the hero, and if we still smell her, start charging.
 */
 
static void rhinoceros_maybe_charge(struct sprite *sprite) {
  if (rhinoceros_hero_present(sprite)) {
    SPRITE->charge=1;
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
  }
}

/* Buttnip: If we're standing still and the hero runs into us, NS_sprgrp_hazard takes care of it.
 * But in the likely event that we do the running-into, we need to deliver the damage to the hero manually.
 */
 
static void rhinoceros_check_buttnip(struct sprite *sprite) {
  SPRITE->charge=0;
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    if (!sprite_hero_is_injured(hero)) {
      double x=sprite->x;
      if (SPRITE->orient>0) x+=2.0; else x-=1.0;
      double dx=hero->x-x;
      double dy=hero->y-sprite->y;
      if (dx*dx+dy*dy<0.500) {
        hero_injure(hero,sprite);
      }
    }
  }
}

/* Update.
 */
 
static void _rhinoceros_update(struct sprite *sprite,double elapsed) {

  /* If (sniffclock) is set, tick it down, and when it expires, consider charging.
   */
  if (SPRITE->sniffclock>0.0) {
    if ((SPRITE->sniffclock-=elapsed)<=0.0) {
      rhinoceros_maybe_charge(sprite);
    } else {
      return;
    }
  }
  
  /* If I'm charging, continue until we hit something.
   */
  int atrest=0;
  if (SPRITE->charge) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.150;
      if (++(SPRITE->animframe)>=2) SPRITE->animframe=0;
    }
    double dx=10.0*elapsed; // m/s
    if (SPRITE->orient<0) dx=-dx;
    double x0=sprite->x;
    if (!sprite_move(sprite,dx,0.0)) {
      rhinoceros_check_buttnip(sprite);
    } else {
      double actual=sprite->x-x0;
      if ((dx>0.0)&&(actual<dx*0.85)) {
        rhinoceros_check_buttnip(sprite);
      } else if ((dx<0.0)&&(actual>dx*0.85)) {
        rhinoceros_check_buttnip(sprite);
      }
    }
    
  /* Or if not charging, slide back to home.
   */
  } else {
    const double speed=5.0;
    if (SPRITE->orient<0) {
      sprite_move(sprite,speed*elapsed,0.0);
      if (sprite->x>=SPRITE->x0) {
        sprite->x=SPRITE->x0;
        atrest=1;
      }
    } else {
      sprite_move(sprite,-speed*elapsed,0.0);
      if (sprite->x<=SPRITE->x0) {
        sprite->x=SPRITE->x0;
        atrest=1;
      }
    }
  }
  
  /* And if we're at home, sniff for the hero.
   */
  if (atrest) {
    if (rhinoceros_hero_present(sprite)) {
      SPRITE->sniffclock=0.500;
    }
  }
}

/* Render.
 */
 
static void _rhinoceros_render(struct sprite *sprite,int dstx,int dsty) {
  int headx,buttx;
  uint8_t xform;
  if (SPRITE->orient>0) {
    buttx=dstx;
    headx=buttx+NS_sys_tilesize;
    xform=0;
  } else {
    headx=dstx;
    buttx=headx+NS_sys_tilesize;
    xform=EGG_XFORM_XREV;
  }
  uint8_t tileid=sprite->tileid;
  if (SPRITE->sniffclock>0.0) {
         if (SPRITE->sniffclock>=0.350) tileid+=2;
    else if (SPRITE->sniffclock>=0.200) ;
    else if (SPRITE->sniffclock>=0.100) tileid+=2;
  } else if (SPRITE->charge) {
    tileid+=4+SPRITE->animframe*2;
  }
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,buttx,dsty,tileid,xform);
  graf_tile(&g.graf,headx,dsty,tileid+1,xform);
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_rhinoceros={
  .name="rhinoceros",
  .objlen=sizeof(struct sprite_rhinoceros),
  .init=_rhinoceros_init,
  .update=_rhinoceros_update,
  .render=_rhinoceros_render,
};

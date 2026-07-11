/* battle_distancing.c
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_world.h"

#define SPRITEID_L 1
#define SPRITEID_R 2
#define SPRITEID_IDLER0 100
#define SPRITEID_WALKER0 200

#define HELLO_TIME 1.000
#define IDLERC 10
#define WALKERC 15
#define WIN_THRESHOLD 30.0 /* Technically the unit is meter-seconds. Don't worry about it. */

struct battle_distancing {
  struct battle hdr;
  struct batsup_world *world;
  double helloclock; // Counts down during initial fade-the-npcs-in animation.
};

#define BATTLE ((struct battle_distancing*)battle)

/* Delete.
 */
 
static void _distancing_del(struct battle *battle) {
  batsup_world_del(BATTLE->world);
}

/* Player sprite.
 */
 
struct sprite_player {
  struct batsup_sprite hdr;
  uint32_t color;
  double skill;
  int human;
  double animclock;
  int animframe;
  double speed;
  double distance; // Distance to nearest other sprite, at the last update.
  double nearnx,nearny; // Unit vector pointing toward that nearest other sprite.
  int dispdist; // Distance jitters too hard. Sample at a more legible rate, and convert to cm while we're at it.
  double dispdistclock;
  double score; // Running integral of (distance).
  double scorefudge; // Constant multiplier, to bias more explicitly than just walk speed.
  // CPU only:
  double cpusyncclock;
  double cpudx,cpudy;
};

/* Update CPU player.
 * Motion only, and we're allowed to go OOB.
 */
 
static void player_update_cpu(struct battle *battle,struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(void*)sprite;
  
  /* First try the most obvious thing: Walk directly away from the nearest other. Easy because we already have that info.
   * OK! This naive approach actually works pretty well. And way less involved than anything else I can think of.
   * Only thing is it is very jittery. So we'll also employ an anti-jitter clock. Hold (dx,dy) for some minimum interval.
   */
  if ((SPRITE->cpusyncclock-=elapsed)<=0.0) {
    SPRITE->cpusyncclock+=0.250;
    SPRITE->cpudx=-SPRITE->nearnx*SPRITE->speed;
    SPRITE->cpudy=-SPRITE->nearny*SPRITE->speed;
    if (SPRITE->cpudx>0.0) sprite->xform=0; else sprite->xform=EGG_XFORM_XREV;
  }
  sprite->x+=SPRITE->cpudx*elapsed;
  sprite->y+=SPRITE->cpudy*elapsed;
}

/* Update player.
 */
 
static void _player_update(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(void*)sprite;
  struct battle *battle=sprite->world->battle;
  
  SPRITE->score+=SPRITE->distance*elapsed*SPRITE->scorefudge;
  
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.200;
    if (++(SPRITE->animframe)>=2) SPRITE->animframe=0;
  }
  
  if (SPRITE->human) {
    switch (g.input[SPRITE->human]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
      case EGG_BTN_LEFT: sprite->x-=SPRITE->speed*elapsed; sprite->xform=EGG_XFORM_XREV; break;
      case EGG_BTN_RIGHT: sprite->x+=SPRITE->speed*elapsed; sprite->xform=0; break;
    }
    switch (g.input[SPRITE->human]&(EGG_BTN_UP|EGG_BTN_DOWN)) {
      case EGG_BTN_UP: sprite->y-=SPRITE->speed*elapsed; break;
      case EGG_BTN_DOWN: sprite->y+=SPRITE->speed*elapsed; break;
    }
    
  } else {
    player_update_cpu(battle,sprite,elapsed);
  }
  
  if (sprite->x<0.5) sprite->x=0.5; else if (sprite->x>NS_sys_mapw-0.5) sprite->x=NS_sys_mapw-0.5;
  if (sprite->y<0.5) sprite->y=0.5; else if (sprite->y>NS_sys_maph-0.5) sprite->y=NS_sys_maph-0.5;
  
  /* Now the main event: Find the nearest other sprite and capture distance to it.
   */
  struct batsup_sprite *nearest=0;
  double neard2=999.999;
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other==sprite) continue;
    double dx=other->x-sprite->x;
    double dy=other->y-sprite->y;
    double d2=dx*dx+dy*dy;
    if (!nearest||(d2<neard2)) {
      nearest=other;
      neard2=d2;
    }
  }
  SPRITE->distance=sqrt(neard2);
  if (SPRITE->distance>0.010) {
    SPRITE->nearnx=(nearest->x-sprite->x)/SPRITE->distance;
    SPRITE->nearny=(nearest->y-sprite->y)/SPRITE->distance;
  }
  
  /* If we only looked at other sprites, the best play would almost always be to tuck yourself in a corner.
   * That's boring!
   * So we'll consider walls human too.
   * ....ha ha perfect, this is exactly the pandemonium I was going for.
   */
  double walld;
  if ((walld=sprite->x)<SPRITE->distance) { SPRITE->distance=walld; SPRITE->nearnx=-1.0; SPRITE->nearny=0.0; }
  if ((walld=sprite->y)<SPRITE->distance) { SPRITE->distance=walld; SPRITE->nearnx=0.0; SPRITE->nearny=-1.0; }
  if ((walld=NS_sys_mapw-sprite->x)<SPRITE->distance) { SPRITE->distance=walld; SPRITE->nearnx=1.0; SPRITE->nearny=0.0; }
  if ((walld=NS_sys_maph-sprite->y)<SPRITE->distance) { SPRITE->distance=walld; SPRITE->nearnx=0.0; SPRITE->nearny=1.0; }
  
  if ((SPRITE->dispdistclock-=elapsed)<=0.0) {
    SPRITE->dispdistclock+=0.250;
    SPRITE->dispdist=(int)(SPRITE->distance*100.0);
    if (SPRITE->dispdist<0) SPRITE->dispdist=0;
    else if (SPRITE->dispdist>999) SPRITE->dispdist=999;
  }
}

/* Render player.
 */
 
static void _player_render(struct batsup_sprite *sprite,int x,int y) {
  struct sprite_player *SPRITE=(void*)sprite;
  struct battle *battle=sprite->world->battle;
  uint8_t tileid=sprite->tileid+SPRITE->animframe;
  graf_tile(&g.graf,x,y,tileid,sprite->xform);
}

/* Init player.
 */

static void player_init(struct battle *battle,struct batsup_sprite *sprite,double skill,uint8_t ctl,uint8_t face) {
  struct sprite_player *SPRITE=(void*)sprite;
  
  sprite->update=_player_update;
  sprite->render=_player_render;
  SPRITE->skill=skill;
  if ((ctl>=0)&&(ctl<=2)) SPRITE->human=ctl;
  SPRITE->speed=4.000*(1.0-SPRITE->skill)+8.000*SPRITE->skill;
  SPRITE->scorefudge=0.800*(1.0-SPRITE->skill)+1.200*SPRITE->skill;
  SPRITE->nearnx=0.0;
  SPRITE->nearny=1.0;
  SPRITE->dispdist=0;
  SPRITE->dispdistclock=0.0;
  
  if (sprite->id==SPRITEID_L) {
    sprite->x=NS_sys_mapw*0.333;
  } else {
    sprite->x=NS_sys_mapw*0.666;
    sprite->xform=EGG_XFORM_XREV;
  }
  sprite->y=NS_sys_maph*0.500;
  
  switch (face) {
    case NS_face_monster: {
        SPRITE->color=0x6f747aff;
        sprite->tileid=0x4c;
      } break;
    case NS_face_dot: {
        SPRITE->color=0x612aa5ff; // lighter than usual (0x411775ff) to read nice on black
        sprite->tileid=0x56;
      } break;
    case NS_face_princess: {
        SPRITE->color=0x0d3ac1ff;
        sprite->tileid=0x58;
      } break;
  }
}

/* NPC (idler or walker).
 */
 
struct sprite_npc {
  struct batsup_sprite hdr;
  int walking;
  // Remainder only relevant if (walking):
  double dx,dy;
  double animclock;
  int animframe;
  uint8_t tileid0;
};

static const uint8_t tileidv_walker[]={0x00,0x02,0x04,0x06,0x11,0x34,0x36,0x38,0x3a,0x3c,0x3e,0x46,0x48,0x4a};
static const uint8_t tileidv_idler[]={0x09,0x0a,0x0b,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,0x33};

static void _npc_update(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_npc *SPRITE=(void*)sprite;
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.200;
    if (++(SPRITE->animframe)>=2) SPRITE->animframe=0;
  }
  sprite->x+=SPRITE->dx*elapsed;
  sprite->y+=SPRITE->dy*elapsed;
  if (((sprite->x<0.5)&&(SPRITE->dx<0.0))||((sprite->x>NS_sys_mapw-0.5)&&(SPRITE->dx>0.0))) {
    SPRITE->dx=-SPRITE->dx;
    sprite->xform=(SPRITE->dx>0.0)?0:EGG_XFORM_XREV;
  }
  if (((sprite->y<0.5)&&(SPRITE->dy<0.0))||((sprite->y>NS_sys_maph-0.5)&&(SPRITE->dy>0.0))) {
    SPRITE->dy=-SPRITE->dy;
  }
}

static void _npc_render(struct batsup_sprite *sprite,int x,int y) {
  struct battle *battle=sprite->world->battle;
  struct sprite_npc *SPRITE=(void*)sprite;
  if (BATTLE->helloclock>0.0) {
    int alpha=255-(BATTLE->helloclock*255)/HELLO_TIME;
    if (alpha<=0) return;
    if (alpha>0xff) alpha=0xff;
    graf_set_alpha(&g.graf,alpha);
  }
  graf_tile(&g.graf,x,y,sprite->tileid+SPRITE->animframe,sprite->xform);
  graf_set_alpha(&g.graf,0xff);
}

static void npc_init(struct battle *battle,struct batsup_sprite *sprite,int walking) {
  struct sprite_npc *SPRITE=(void*)sprite;
  const uint8_t *tileidv;
  int tileidc;
  
  if (SPRITE->walking=walking) {
    tileidv=tileidv_walker;
    tileidc=sizeof(tileidv_walker);
    const double speedlo=0.250;
    const double speedhi=3.000;
    SPRITE->dx=speedlo+((rand()&0xffff)*(speedhi-speedlo))/65535.0;
    SPRITE->dy=speedlo+((rand()&0xffff)*(speedhi-speedlo))/65535.0;
    switch (rand()&3) {
      case 1: SPRITE->dx*=-1.0; break;
      case 2: SPRITE->dy*=-1.0; break;
      case 3: SPRITE->dx*=-1.0; SPRITE->dy*=-1.0; break;
    }
    
  } else {
    tileidv=tileidv_idler;
    tileidc=sizeof(tileidv_idler);
  }

  int tileidp=rand()%tileidc;
  sprite->tileid=tileidv[tileidp];
  SPRITE->tileid0=sprite->tileid;
  if (walking) sprite->update=_npc_update; // idlers don't need an update at all.
  sprite->render=_npc_render;
  
  int panic=40; while (panic-->0) { // If we don't find a reasonable location soon enough, just let it roll, it will be somewhere.
    sprite->x=(rand()%FBW)/(double)NS_sys_tilesize;
    sprite->y=(rand()%FBH)/(double)NS_sys_tilesize;
    int ok=1,i=BATTLE->world->spritec;
    struct batsup_sprite **otherp=BATTLE->world->spritev;
    for (;i-->0;otherp++) {
      struct batsup_sprite *other=*otherp;
      if (other==sprite) continue;
      double dx=other->x-sprite->x;
      double dy=other->y-sprite->y;
      double d2=dx*dx+dy*dy;
      if (d2<1.0) {
        ok=0;
        break;
      }
    }
    if (ok) break;
  }
  if (SPRITE->walking) sprite->xform=(SPRITE->dx<0.0)?EGG_XFORM_XREV:0;
  else if (sprite->x>NS_sys_mapw*0.5) sprite->xform=EGG_XFORM_XREV;
}

/* New.
 */
 
static int _distancing_init(struct battle *battle) {
  double lskill,rskill;
  battle_normalize_bias(&lskill,&rskill,battle);
  if (!(BATTLE->world=batsup_world_new(battle,0))) return -1;
  if (batsup_world_set_image(BATTLE->world,RID_image_meadow_sprites)<0) return -1;
  memset(BATTLE->world->map->v,0x4e,NS_sys_mapw*NS_sys_maph);
  struct batsup_sprite *sprite;
  int i;
  
  if (!(sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_L,sizeof(struct sprite_player)))) return -1;
  player_init(battle,sprite,lskill,battle->args.lctl,battle->args.lface);
  
  if (!(sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_R,sizeof(struct sprite_player)))) return -1;
  player_init(battle,sprite,rskill,battle->args.rctl,battle->args.rface);
  
  for (i=0;i<IDLERC;i++) {
    if (!(sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_IDLER0+i,sizeof(struct sprite_npc)))) return -1;
    npc_init(battle,sprite,0);
  }
  
  for (i=0;i<WALKERC;i++) {
    if (!(sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_WALKER0+i,sizeof(struct sprite_npc)))) return -1;
    npc_init(battle,sprite,1);
  }
  
  BATTLE->helloclock=HELLO_TIME;
  return 0;
}

/* Update.
 */
 
static void _distancing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if (BATTLE->helloclock>0.0) {
    BATTLE->helloclock-=elapsed;
    return;
  }
  
  batsup_world_update(BATTLE->world,elapsed);
  
  /* Game ends when one player exceeds WIN_THRESHOLD.
   * Scores are constantly increasing, so I don't think timeout is possible.
   * ...experimentally, if neither player moves, it usually ends in about 20 s.
   * If they both breach it on the same frame, is one higher? Ties are astronomically unlikely, but possible.
   * If either sprite disappears, declare a tie immediately. Won't happen.
   */
  struct batsup_sprite *l=batsup_sprite_by_id(BATTLE->world,SPRITEID_L);
  struct batsup_sprite *r=batsup_sprite_by_id(BATTLE->world,SPRITEID_R);
  if (!l||!r) { battle->outcome=0; return; }
  struct sprite_player *L=(void*)l;
  struct sprite_player *R=(void*)r;
  if ((L->score>WIN_THRESHOLD)||(R->score>WIN_THRESHOLD)) {
    if (L->score>R->score) battle->outcome=1;
    else if (L->score<R->score) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render.
 */
 
static void distancing_render_distance(struct battle *battle,int spriteid) {
  struct batsup_sprite *sprite=batsup_sprite_by_id(BATTLE->world,spriteid);
  if (!sprite) return;
  struct sprite_player *SPRITE=(void*)sprite;
  int x,y;
  batsup_sprite_render_position(&x,&y,sprite);
  const double radius=10.0;
  x+=lround(SPRITE->nearnx*radius);
  y+=lround(SPRITE->nearny*radius);
  graf_set_image(&g.graf,RID_image_tinyfonttiles);
  if (SPRITE->dispdist>=100) graf_tile(&g.graf,x-6,y,'0'+(SPRITE->dispdist/100)%10,0);
  if (SPRITE->dispdist>=10) graf_tile(&g.graf,x,y,'0'+(SPRITE->dispdist/10)%10,0);
  graf_tile(&g.graf,x+6,y,'0'+SPRITE->dispdist%10,0);
}
 
static void _distancing_render(struct battle *battle) {

  // Background and sprites.
  batsup_world_render(BATTLE->world);
  
  /* Draw a distance indicator on top of the players.
   */
  if (battle->outcome==-2) {
    distancing_render_distance(battle,SPRITEID_L);
    distancing_render_distance(battle,SPRITEID_R);
  }
  
  /* Scoreboard on top.
   */
  struct batsup_sprite *l=batsup_sprite_by_id(BATTLE->world,SPRITEID_L);
  struct batsup_sprite *r=batsup_sprite_by_id(BATTLE->world,SPRITEID_R);
  if (l&&r) {
    struct sprite_player *L=(void*)l;
    struct sprite_player *R=(void*)r;
    int barw=(FBW>>1)-5;
    int lhw=(int)((L->score*barw)/WIN_THRESHOLD);
    if (lhw<0) lhw=0; else if (lhw>barw) lhw=barw;
    int rhw=(int)((R->score*barw)/WIN_THRESHOLD);
    if (rhw<0) rhw=0; else if (rhw>barw) rhw=barw;
    int lbarx=(FBW>>1)-2-barw;
    int rbarx=(FBW>>1)+2;
    graf_fill_rect(&g.graf,lbarx-1,2,barw+2,4,0x000000ff);
    graf_fill_rect(&g.graf,rbarx-1,2,barw+2,4,0x000000ff);
    graf_fill_rect(&g.graf,lbarx,3,lhw,2,L->color);
    graf_fill_rect(&g.graf,rbarx+barw-rhw,3,rhw,2,R->color);
    //fprintf(stderr,"%10.0f %10.0f\n",L->score,R->score);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_distancing={
  .name="distancing",
  .objlen=sizeof(struct battle_distancing),
  .id=NS_battle_distancing,
  .strix_name=276,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_dpad,
  .del=_distancing_del,
  .init=_distancing_init,
  .update=_distancing_update,
  .render=_distancing_render,
};

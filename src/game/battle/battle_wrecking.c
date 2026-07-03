/* battle_wrecking.c
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_world.h"

#define SPRITEID_L 1
#define SPRITEID_R 2
#define SPRITEID_WRECKABLE 100 /* +0..6 */

struct battle_wrecking {
  struct battle hdr;
  struct batsup_world *world;
  int dirty; // Set nonzero when wrecking something; we'll check for completion at the end of the frame.
};

#define BATTLE ((struct battle_wrecking*)battle)

/* Delete.
 */
 
static void _wrecking_del(struct battle *battle) {
  batsup_world_del(BATTLE->world);
}

/* Wreckable sprite.
 */
 
struct batsup_sprite_wreckable {
  struct batsup_sprite hdr;
  int srcx,srcy;
  int wrecked;
};

static void _wreckable_render(struct batsup_sprite *sprite,int dstx,int dsty) {
  struct batsup_sprite_wreckable *SPRITE=(void*)sprite;
  int srcx=SPRITE->srcx;
  int srcy=SPRITE->srcy;
  if (SPRITE->wrecked) srcx+=NS_sys_tilesize*2;
  dstx-=NS_sys_tilesize;
  dsty-=NS_sys_tilesize;
  graf_set_image(&g.graf,RID_image_battle_war);
  graf_decal(&g.graf,dstx,dsty,srcx,srcy,NS_sys_tilesize*2,NS_sys_tilesize*2);
}

static int wrecking_add_wreckable(struct battle *battle,int seq,int face,double x,double y) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_WRECKABLE+seq,sizeof(struct batsup_sprite_wreckable));
  if (!sprite) return -1;
  struct batsup_sprite_wreckable *SPRITE=(void*)sprite;
  sprite->x=x+0.5;
  sprite->y=y;
  sprite->render=_wreckable_render;
  SPRITE->srcx=(face&3)*NS_sys_tilesize*4;
  SPRITE->srcy=(((face&4)>>1)+11)*NS_sys_tilesize;
  return 0;
}

static int wrecking_init_wreckables(struct battle *battle) {

  /* There are 8 appearances, and we'll pick 7 of them randomly.
   * Choice and order doesn't mean anything, just shake it up to stay lively.
   * But always 7 different things.
   */
  int available[8]={0,1,2,3,4,5,6,7};
  int facev[7];
  int i=0; for (;i<7;i++) {
    int avp=rand()%(8-i);
    facev[i]=available[avp];
    memmove(available+avp,available+avp+1,sizeof(int)*(8-avp-1));
  }
  
  /* Put them in 7 preordained locations.
   */
  if (wrecking_add_wreckable(battle,0,facev[0], 2.0, 3.0)<0) return -1;
  if (wrecking_add_wreckable(battle,1,facev[1],17.0, 3.0)<0) return -1;
  if (wrecking_add_wreckable(battle,2,facev[2], 9.5, 4.0)<0) return -1;
  if (wrecking_add_wreckable(battle,3,facev[3], 1.0, 7.0)<0) return -1;
  if (wrecking_add_wreckable(battle,4,facev[4],18.0, 7.0)<0) return -1;
  if (wrecking_add_wreckable(battle,5,facev[5], 6.0,10.0)<0) return -1;
  if (wrecking_add_wreckable(battle,6,facev[6],13.0,10.0)<0) return -1;
  
  return 0;
}

/* Player sprite.
 */
 
struct batsup_sprite_hero {
  struct batsup_sprite hdr;
  int who,human;
  uint32_t color;
  double skill;
  double animclock;
  int animframe;
  int indx,injump,inswing; // Man or CPU controller sets only these.
  int jumppoison; // For regulating one jump per stroke.
  int jumping;
  int pvswing,extended;
  double walkspeed; // Const. m/s
  double jumpinitial; // Const. m/s
  double jumpdecel; // Const. m/s**2 positive
  double jumppower; // Volatile. m/s (NB positive: must flip sign when applying)
  double gravitylimit; // Const. m/s
  double gravityaccel; // Const. m/s**2
  double gravitythresh; // Const. m/s. Something I haven't done before; don't even attempt gravity if below this.
  double gravity; // Volatile. m/s
  double hammert; // radians faceward; 0 is up, -pi/4 at rest
  double swingspeed; // Const. rad/sec positive
  double releasespeed; // Const. rad/sec positive
  int score;
  
  // For CPU:
  double targetx,jumptargetx;
  int targetxdir,jumptargetxdir;
  double stuckclock,stuckx;
  double hello;
};

/* Human player.
 */
 
static void hero_update_man(struct batsup_sprite *sprite,double elapsed) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  int input=g.input[SPRITE->human];
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: SPRITE->indx=-1; break;
    case EGG_BTN_RIGHT: SPRITE->indx=1; break;
    default: SPRITE->indx=0; break;
  }
  SPRITE->injump=(input&EGG_BTN_SOUTH)?1:0;
  SPRITE->inswing=(input&EGG_BTN_WEST)?1:0;
}

/* Nonzero if there's a walkable path between these two sprites.
 */
 
static int wrecking_is_same_platform(struct batsup_sprite *sprite,struct batsup_sprite *other) {
  int sy=(int)(sprite->y+1.0);
  int oy=(int)(other->y+1.5); // (other) should be a wreckable, which has radius 1, not 1/2.
  if ((sy<0)||(sy>=NS_sys_maph)||(oy<0)||(oy>=NS_sys_maph)) return 0;
  if (sy!=oy) return 0;
  int sx=(int)sprite->x;
  int ox=(int)other->x;
  if ((sx<0)||(sx>=NS_sys_mapw)||(ox<0)||(ox>=NS_sys_mapw)) return 0;
  for (;;) {
    uint8_t tileid=sprite->world->map->v[sy*NS_sys_mapw+sx];
    uint8_t physics=sprite->world->map->physics[tileid];
    if (physics==NS_physics_vacant) return 0; // The only physicses we'll see are vacant and solid.
    // We're only checking the ground, not horizontal obstructions, because there won't be any.
    if (sx<ox) sx++;
    else if (sx>ox) sx--;
    else return 1;
  }
}

/* Plan a jump for a CPU player.
 * This means setting (targetx,targetxdir,jumptargetx,jumptargetxdir).
 * The location you specify must be one jump away.
 */
 
static void cpu_plan_jump(struct batsup_sprite *sprite,double dstx,double dsty) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  
  // Find the platform below (dstx,dsty).
  int px=(int)dstx;
  int py=(int)dsty;
  if ((px<0)||(px>=NS_sys_mapw)) return;
  if (py<0) return;
  for (;;py++) {
    if (py>=NS_sys_maph) return;
    if (sprite->world->map->physics[sprite->world->map->v[py*NS_sys_mapw+px]]==NS_physics_solid) break;
  }
  
  // Find its nearest edge, not counting map edges.
  const uint8_t *row=sprite->world->map->v+py*NS_sys_mapw;
  int dx=1;
  for (;;dx++) {
    int lx=px-dx,rx=px+dx;
    if ((lx<0)&&(rx>=NS_sys_mapw)) return; // Not a platform!
    if ((lx>=0)&&(sprite->world->map->physics[row[lx]]==NS_physics_vacant)) {
      SPRITE->targetx=lx-1.0; // With extra clearance, because we'll be moving horizontally during the jump.
      break;
    }
    if ((rx<NS_sys_mapw)&&(sprite->world->map->physics[row[rx]]==NS_physics_vacant)) {
      SPRITE->targetx=rx+2.0;
      break;
    }
  }
  if (SPRITE->targetx<0.5) SPRITE->targetx=0.5;
  else if (SPRITE->targetx>NS_sys_mapw-0.5) SPRITE->targetx=NS_sys_mapw-0.5;
  
  // Find the platform below me. If (targetx) is beyond its edge, target the edge instead.
  int sx=(int)sprite->x;
  int sy=(int)(sprite->y+1.0);
  if ((sx<0)||(sx>=NS_sys_mapw)||(sy<0)||(sy>=NS_sys_maph)) return;
  row=sprite->world->map->v+sy*NS_sys_mapw;
  int tx=(int)SPRITE->targetx;
  while (sx!=tx) {
    int nextx=sx;
    if (tx<sx) nextx--; else nextx++;
    if (sprite->world->map->physics[row[nextx]]==NS_physics_vacant) {
      SPRITE->targetx=sx+0.5;
      break;
    }
    sx=nextx;
  }
  
  SPRITE->targetxdir=(SPRITE->targetx>sprite->x)?1:-1;
  SPRITE->jumptargetx=dstx;
  SPRITE->jumptargetxdir=(SPRITE->jumptargetx>SPRITE->targetx)?1:-1;
}

/* Plan to walk off the current platform, for a CPU player.
 */
 
static void cpu_plan_fall(struct batsup_sprite *sprite) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  int x=(int)sprite->x;
  int y=(int)(sprite->y+1.0);
  if ((x<0)||(x>=NS_sys_mapw)||(y<0)||(y>=NS_sys_maph)) return;
  const uint8_t *row=sprite->world->map->v+y*NS_sys_mapw;
  int dx=1;
  for (;;dx++) {
    int lx=x-dx,rx=x+dx;
    if ((lx<0)&&(rx>=NS_sys_mapw)) return; // oy, you're supposed to be on a platform!
    if ((lx>=0)&&(sprite->world->map->physics[row[lx]]==NS_physics_vacant)) {
      SPRITE->targetx=lx;
      SPRITE->targetxdir=-1;
      return;
    }
    if ((rx<NS_sys_mapw)&&(sprite->world->map->physics[row[rx]]==NS_physics_vacant)) {
      SPRITE->targetx=rx+1.0;
      SPRITE->targetxdir=1;
      return;
    }
  }
}

/* Check for situations where the CPU gets stuck, usually by two CPU players walking into each other.
 * We'll poll for it at random intervals.
 * When we detect stickage, jump.
 */
 
static void cpu_check_stuck(struct batsup_sprite *sprite,double elapsed) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  if ((SPRITE->stuckclock-=elapsed)>0.0) return;
  SPRITE->stuckclock=0.200+(((rand()&0xffff)*0.500)/65535.0);
  double dx=sprite->x-SPRITE->stuckx;
  if ((dx>-0.100)&&(dx<0.100)) {
    SPRITE->injump=1;
  }
  SPRITE->stuckx=sprite->x;
}

/* Call each frame while walking.
 * If we walk past an unwrecked wreckable, don't stand on ceremony, wreck it.
 */
 
static void cpu_check_opportunistic_wreckage(struct batsup_sprite *sprite) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other->defunct||(other->id<SPRITEID_WRECKABLE)) continue;
    if (((struct batsup_sprite_wreckable*)other)->wrecked) continue;
    double dy=other->y-sprite->y;
    if ((dy<-1.0)||(dy>1.0)) continue;
    double dx=other->x-sprite->x;
    if (sprite->xform) {
      if ((dx<-1.25)||(dx>=0.0)) continue;
    } else {
      if ((dx>1.25)||(dx<=0.0)) continue;
    }
    SPRITE->inswing=1;
    SPRITE->targetxdir=0;
    SPRITE->jumptargetxdir=0;
    SPRITE->indx=0;
    SPRITE->injump=0;
    return;
  }
}

/* CPU player.
 */
 
static void hero_update_cpu(struct batsup_sprite *sprite,double elapsed) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  
  /* Right after startup, take a few deep breaths.
   */
  if (SPRITE->hello>0.0) {
    SPRITE->hello-=elapsed;
    return;
  }
  
  /* If we have a jump target and no walk target, walk toward the jump target.
   */
  if (SPRITE->jumptargetxdir&&!SPRITE->targetxdir) {
    if (
      ((SPRITE->jumptargetxdir<0)&&(sprite->x<=SPRITE->jumptargetx))||
      ((SPRITE->jumptargetxdir>0)&&(sprite->x>=SPRITE->jumptargetx))
    ) {
      SPRITE->jumptargetxdir=0;
      SPRITE->indx=0;
    } else {
      SPRITE->indx=SPRITE->jumptargetxdir;
      cpu_check_stuck(sprite,elapsed);
      cpu_check_opportunistic_wreckage(sprite);
    }
    if (SPRITE->gravity<SPRITE->gravitythresh) {
      SPRITE->injump=1;
    } else {
      SPRITE->injump=0;
      SPRITE->targetxdir=SPRITE->jumptargetxdir;
      SPRITE->targetx=SPRITE->jumptargetx;
      SPRITE->jumptargetxdir=0;
    }
    SPRITE->inswing=0;
    return;
  }
  
  /* If I'm jumping, wait for it to wind down.
   */
  if (SPRITE->jumping) return;
  
  /* If I'm swinging, wait for it to become extended, then let go.
   * And if the hammer is coming back, wait for it.
   */
  if (SPRITE->inswing) {
    if (SPRITE->extended) {
      SPRITE->inswing=0;
    }
    SPRITE->indx=0;
    SPRITE->injump=0;
    return;
  }
  if (SPRITE->hammert>M_PI*0.200) {
    SPRITE->indx=0;
    SPRITE->inswing=0;
    SPRITE->injump=0;
    return;
  }
  
  /* If I have a walking target assigned, walk toward it.
   */
  if (SPRITE->targetxdir) {
    if (
      ((SPRITE->targetxdir<0)&&(sprite->x<=SPRITE->targetx))||
      ((SPRITE->targetxdir>0)&&(sprite->x>=SPRITE->targetx))
    ) {
      SPRITE->targetxdir=0;
      if (!SPRITE->jumptargetxdir) SPRITE->indx=0;
      SPRITE->inswing=0;
      SPRITE->injump=0;
    } else {
      SPRITE->indx=SPRITE->targetxdir;
      SPRITE->inswing=0;
      SPRITE->injump=0;
      cpu_check_stuck(sprite,elapsed);
      cpu_check_opportunistic_wreckage(sprite);
    }
    return;
  }
  
  /* If I'm falling, let it go, don't do anything else.
   */
  if (SPRITE->gravity>SPRITE->gravitythresh) {
    SPRITE->indx=0;
    SPRITE->injump=0;
    SPRITE->inswing=0;
    return;
  }
  
  /* Identify all the unwrecked wreckables.
   * If there's none, chill, we're done. Shouldn't happen.
   */
  struct batsup_sprite *wreckablev[8];
  int wreckablec=0;
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other->defunct||(other->id<SPRITEID_WRECKABLE)) continue;
    struct batsup_sprite_wreckable *OTHER=(void*)other;
    if (OTHER->wrecked) continue;
    wreckablev[wreckablec++]=other;
    if (wreckablec>=8) break;
  }
  if (!wreckablec) {
    SPRITE->indx=0;
    SPRITE->inswing=0;
    SPRITE->injump=0;
    return;
  }
  
  /* Find the nearest wreckable at my vertical level, with a contiguous floor to it.
   * If it's close enough, wreck it. Otherwise arrange to walk to it.
   */
  struct batsup_sprite *nearest=0;
  double neardx=999.999;
  for (i=wreckablec;i-->0;) {
    if (!wrecking_is_same_platform(sprite,wreckablev[i])) continue;
    double dx=wreckablev[i]->x-sprite->x;
    if (dx<0.0) dx=-dx;
    if (!nearest||(dx<neardx)) {
      nearest=wreckablev[i];
      neardx=dx;
    }
  }
  if (nearest) {
    double dx=nearest->x-sprite->x;
    if (
      ((dx<0.0)&&(dx>-1.0)&&sprite->xform)||
      ((dx>0.0)&&(dx<1.0)&&!sprite->xform)
    ) {
      SPRITE->indx=0;
      SPRITE->inswing=1;
      SPRITE->injump=0;
      return;
    }
    if (nearest->x<sprite->x) {
      SPRITE->targetx=nearest->x+0.5;
    } else {
      SPRITE->targetx=nearest->x-0.5;
    }
    SPRITE->targetxdir=(SPRITE->targetx>sprite->x)?1:-1;
    SPRITE->indx=SPRITE->targetxdir;
    SPRITE->injump=0;
    SPRITE->inswing=0;
    return;
  }
  
  /* If there's a wreckable less than 4 m above me, we should be able to jump to its platform.
   * Also note whether there's one further up than that.
   */
  int has_higher=0;
  for (i=wreckablec;i-->0;) {
    double dy=wreckablev[i]->y-sprite->y;
    if (dy>0.0) continue;
    if (dy<=-4.0) {
      if (wreckablev[i]->x<8.0) has_higher=-1;
      else has_higher=1;
      continue;
    }
    cpu_plan_jump(sprite,wreckablev[i]->x,wreckablev[i]->y);
    return;
  }
  
  /* Something more than 4 m above me?
   * We want to reach the middle platform first.
   */
  if (has_higher) {
    if (sprite->y>7.0) {
      cpu_plan_jump(sprite,10.0,4.0);
    } else if (has_higher<0) {
      cpu_plan_jump(sprite,3.5,3.0);
    } else {
      cpu_plan_jump(sprite,16.5,3.0);
    }
    return;
  }
  
  /* Let's get back to the ground floor to reassess.
   */
  if (sprite->y>9.0) { // Already there. Huh.
    SPRITE->indx=0;
    SPRITE->inswing=0;
    SPRITE->injump=0;
    return;
  }
  cpu_plan_fall(sprite);
}

/* Gravity.
 * This is not called during a jump's Up phase.
 */
 
static void hero_update_gravity(struct batsup_sprite *sprite,double elapsed) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  if ((SPRITE->gravity+=SPRITE->gravityaccel*elapsed)>=SPRITE->gravitylimit) {
    SPRITE->gravity=SPRITE->gravitylimit;
  }
  if (SPRITE->gravity<=SPRITE->gravitythresh) return;
  if (batsup_sprite_move(sprite,0.0,SPRITE->gravity*elapsed)) {
    SPRITE->jumppower=0.0;
  } else {
    SPRITE->jumppower=SPRITE->jumpinitial;
    SPRITE->gravity=0.0;
  }
}

/* Hammer just reached full extension.
 * Look for anything we can wreck, and effect all consequences.
 */
 
static void hero_check_wreckage(struct batsup_sprite *sprite) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other->defunct) continue;
    if (other->id<SPRITEID_WRECKABLE) continue;
    double dy=other->y-sprite->y;
    if (dy<-1.0) continue;
    if (dy>1.0) continue;
    double dx=other->x-sprite->x;
    if (dx<0.0) {
      if (!sprite->xform) continue;
      if (dx<-1.5) continue;
    } else {
      if (sprite->xform) continue;
      if (dx>1.5) continue;
    }
    struct batsup_sprite_wreckable *OTHER=(void*)other;
    if (OTHER->wrecked) continue;
    OTHER->wrecked=1;
    bm_sound_pan(RID_sound_wreck,SPRITE->who?PLAYER_PAN:-PLAYER_PAN);
    SPRITE->score++;
    struct battle *battle=sprite->world->battle;
    BATTLE->dirty=1;
    return;
  }
}

/* Update player: Dispatch control, and common features.
 */

static void _hero_update(struct batsup_sprite *sprite,double elapsed) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  
  // Manual input or AI.
  if (SPRITE->human) hero_update_man(sprite,elapsed);
  else hero_update_cpu(sprite,elapsed);
  
  // Animation.
  if (SPRITE->indx) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.180;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    }
    if (SPRITE->indx<0) sprite->xform=EGG_XFORM_XREV;
    else sprite->xform=0;
  } else {
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
  }
  
  // Horizontal motion.
  if (SPRITE->indx) {
    batsup_sprite_move(sprite,SPRITE->walkspeed*elapsed*SPRITE->indx,0.0);
  }
  
  // Gravity or jump.
  if (SPRITE->jumppoison) {
    if (!SPRITE->injump) SPRITE->jumppoison=0;
    hero_update_gravity(sprite,elapsed);
  } else if (SPRITE->jumping&&!SPRITE->injump) {
    SPRITE->jumping=0;
    SPRITE->jumppoison=1;
  } else if (SPRITE->jumping) {
    SPRITE->gravity=0.0;
    batsup_sprite_move(sprite,0.0,-SPRITE->jumppower*elapsed);
    if ((SPRITE->jumppower-=SPRITE->jumpdecel*elapsed)<=0.0) {
      SPRITE->jumping=0;
      SPRITE->jumppoison=1;
    }
  } else if (SPRITE->injump&&!SPRITE->jumppoison&&(SPRITE->jumppower>0.0)) {
    bm_sound_pan(RID_sound_jump,SPRITE->who?PLAYER_PAN:-PLAYER_PAN);
    SPRITE->jumping=1;
    SPRITE->gravity=0.0;
  } else {
    hero_update_gravity(sprite,elapsed);
  }
  
  // Hammer.
  if (SPRITE->inswing) {
    if (!SPRITE->pvswing) {
      SPRITE->pvswing=1;
      bm_sound_pan(RID_sound_swing_racket,SPRITE->who?PLAYER_PAN:-PLAYER_PAN);
    }
    if ((SPRITE->hammert+=SPRITE->swingspeed*elapsed)>=M_PI*0.5) {
      SPRITE->hammert=M_PI*0.5;
      if (!SPRITE->extended) {
        SPRITE->extended=1;
        hero_check_wreckage(sprite);
      }
    }
  } else {
    SPRITE->extended=0;
    SPRITE->pvswing=0;
    if ((SPRITE->hammert-=SPRITE->releasespeed*elapsed)<=M_PI*-0.250) {
      SPRITE->hammert=M_PI*-0.250;
    }
  }
}

/* Render player.
 */

static void _hero_render(struct batsup_sprite *sprite,int dstx,int dsty) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  graf_set_image(&g.graf,RID_image_battle_war);
  
  // Main tile could be regular tile, but using fancy so it can share a batch hopefully.
  uint8_t tileid=sprite->tileid;
  switch (SPRITE->animframe) {
    case 1: tileid+=1; break;
    case 3: tileid+=2; break;
  }
  graf_fancy(&g.graf,dstx,dsty,tileid,sprite->xform,0,NS_sys_tilesize,0,0x808080ff);
  
  // Hammer.
  double hamx=dstx,hamy=dsty;
  double hamt=SPRITE->hammert;
  if (sprite->xform&EGG_XFORM_XREV) {
    hamt=-hamt;
  }
  const double offset=9.0;
  hamx+=sin(hamt)*offset;
  hamy-=cos(hamt)*offset;
  int hamix=lround(hamx);
  int hamiy=lround(hamy);
  uint8_t hamrot=(int8_t)((hamt*128.0)/M_PI);
  graf_fancy(&g.graf,hamix,hamiy,0xf0,0,hamrot,NS_sys_tilesize,0,0x808080ff);
}

/* Initialize both player sprites.
 */
 
static void wrecking_init_player1(struct battle *battle,struct batsup_sprite *sprite,uint8_t ctl,uint8_t face,double skill) {
  struct batsup_sprite_hero *SPRITE=(void*)sprite;
  SPRITE->human=ctl;
  SPRITE->skill=skill;
  SPRITE->walkspeed=5.5*(1.0-skill)+7.0*skill; // With the low at 5.0, CPU can't reach the topmost platforms. 5.5 is ok.
  if (!ctl) SPRITE->walkspeed*=0.800; // CPU penalty.
  SPRITE->jumpinitial=25.0;
  SPRITE->jumpdecel=90.0;
  SPRITE->gravitylimit=12.0;
  SPRITE->gravityaccel=35.0;
  SPRITE->gravitythresh=2.0;
  SPRITE->swingspeed=50.0;
  SPRITE->releasespeed=10.0;
  SPRITE->jumppoison=1; // Must release SOUTH before you're allowed to jump.
  SPRITE->hammert=M_PI*-0.250;
  SPRITE->hello=0.500+(1.0-skill)*1.500;
  sprite->solid=1;
  switch (face) {
    case NS_face_dot: {
        sprite->tileid=0x80;
        SPRITE->color=0x411775ff;
      } break;
    case NS_face_princess: {
        sprite->tileid=0x90;
        SPRITE->color=0x0d3ac1ff;
      } break;
    case NS_face_monster: default: {
        sprite->tileid=0xa0;
        SPRITE->color=0xdf6a12ff;
      } break;
  }
  sprite->update=_hero_update;
  sprite->render=_hero_render;
}
 
static int wrecking_init_players(struct battle *battle,struct batsup_sprite *l,struct batsup_sprite *r) {
  struct batsup_sprite_hero *L=(void*)l,*R=(void*)r;
  L->who=0;
  R->who=1;
  
  /* Pick a random starting point for each player.
   */
  uint8_t optionv[]={ // col0,row0,col1,row1,...
    1,10,
    18,10,
    4,7,
    15,7,
    7,4,
    12,4,
    3,3,
    16,3,
  };
  int optionc=sizeof(optionv)>>1;
  int optionp=(rand()%optionc)<<1;
  l->x=optionv[optionp]+0.5;
  l->y=optionv[optionp+1]+0.5;
  optionc--;
  memmove(optionv+optionp,optionv+optionp+2,(optionc<<1)-optionp);
  optionp=(rand()%optionc)<<1;
  r->x=optionv[optionp]+0.5;
  r->y=optionv[optionp+1]+0.5;
  if (l->x>10.0) l->xform=EGG_XFORM_XREV;
  if (r->x>10.0) r->xform=EGG_XFORM_XREV;
  
  wrecking_init_player1(battle,l,battle->args.lctl,battle->args.lface,1.0-battle->args.bias/255.0);
  wrecking_init_player1(battle,r,battle->args.rctl,battle->args.rface,battle->args.bias/255.0);
  return 0;
}

/* Initialize map.
 */
 
static void wrecking_platform(struct battle *battle,int x,int y,int w) {
  if ((y<0)||(y>=NS_sys_maph)) return;
  if (x<0) { w+=x; x=0; }
  if (x>NS_sys_mapw-w) w=NS_sys_mapw-x;
  if (w<1) return;
  uint8_t *p=BATTLE->world->map->v+y*NS_sys_mapw+x;
  // First, eagerly fill with the middle tile.
  memset(p,0x03,w);
  // If it doesn't touch the edge, cap with 0x02 or 0x04 instead.
  if (x>0) p[0]=0x02;
  if (x+w<NS_sys_mapw) p[w-1]=0x04;
}
 
static int wrecking_init_map(struct battle *battle) {
  if (batsup_world_set_image(BATTLE->world,RID_image_battle_war)<0) return -1;
  wrecking_platform(battle, 0,11,20);
  wrecking_platform(battle, 0, 8, 5);
  wrecking_platform(battle,15, 8, 5);
  wrecking_platform(battle, 7, 5, 6);
  wrecking_platform(battle, 1, 4, 3);
  wrecking_platform(battle,16, 4, 3);
  return 0;
}

/* New.
 */
 
static int _wrecking_init(struct battle *battle) {
  if (!(BATTLE->world=batsup_world_new(battle,0))) return -1;
  if (wrecking_init_map(battle)<0) return -1;
  
  struct batsup_sprite *l,*r;
  if (!(l=batsup_sprite_spawn(BATTLE->world,SPRITEID_L,sizeof(struct batsup_sprite_hero)))) return -1;
  if (!(r=batsup_sprite_spawn(BATTLE->world,SPRITEID_R,sizeof(struct batsup_sprite_hero)))) return -1;
  if (wrecking_init_players(battle,l,r)<0) return -1;
  
  if (wrecking_init_wreckables(battle)<0) return -1;
  
  return 0;
}

/* Trivial hooks to batsup_world.
 */
 
static void _wrecking_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  batsup_world_update(BATTLE->world,elapsed);
  
  if (BATTLE->dirty) {
    BATTLE->dirty=0;
    int done=1;
    struct batsup_sprite **p=BATTLE->world->spritev;
    int i=BATTLE->world->spritec;
    for (;i-->0;p++) {
      struct batsup_sprite *sprite=*p;
      if (sprite->defunct||(sprite->id<SPRITEID_WRECKABLE)) continue;
      struct batsup_sprite_wreckable *SPRITE=(void*)sprite;
      if (!SPRITE->wrecked) { // Game ends when all seven things are wrecked (even if one player has four already, keep going)
        done=0;
        break;
      }
    }
    if (done) {
      struct batsup_sprite_hero *l=(void*)batsup_sprite_by_id(BATTLE->world,SPRITEID_L);
      struct batsup_sprite_hero *r=(void*)batsup_sprite_by_id(BATTLE->world,SPRITEID_R);
      if (!l||!r) battle->outcome=0;
      else if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render.
 */
 
static void _wrecking_render(struct battle *battle) {
  batsup_world_render(BATTLE->world);
  
  // Score on top. Don't bother implementing these as sprites.
  struct batsup_sprite_hero *l=(struct batsup_sprite_hero*)batsup_sprite_by_id(BATTLE->world,SPRITEID_L);
  struct batsup_sprite_hero *r=(struct batsup_sprite_hero*)batsup_sprite_by_id(BATTLE->world,SPRITEID_R);
  int lscore=0,rscore=0;
  if (l) lscore=l->score;
  if (r) rscore=r->score;
  int x=(FBW>>1)-NS_sys_tilesize*3;
  int y=20;
  int i=0;
  for (;i<7;i++,x+=NS_sys_tilesize) {
    uint32_t color=0x80808080;
    if (i<lscore) color=l->color;
    else if (i>6-rscore) color=r->color;
    graf_fancy(&g.graf,x,y,0xf1,0,0,NS_sys_tilesize,0,color);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_wrecking={
  .name="wrecking",
  .objlen=sizeof(struct battle_wrecking),
  .id=NS_battle_wrecking,
  .strix_name=261,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_wrecking_del,
  .init=_wrecking_init,
  .update=_wrecking_update,
  .render=_wrecking_render,
};

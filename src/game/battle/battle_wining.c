/* battle_wining.c
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_world.h"

#define SPRITEID_L 1
#define SPRITEID_R 2
#define SPRITEID_LBOTTLE 3
#define SPRITEID_RBOTTLE 4
#define SPRITEID_GRAPE 100 /* +whatever */

/* In theory a bottle's (fill) runs 0..1, corresponding to the full height of the decal.
 * But the actual wine pixels don't cover the full vertical extent, so the game ends a little short of 1.0.
 */
#define FILL_FINISH 0.900

struct battle_wining {
  struct battle hdr;
  struct batsup_world *world;
};

#define BATTLE ((struct battle_wining*)battle)

/* If a grape is here, squash it and return count.
 * We do sounds, scoring, and fireworks too.
 */
static int wining_squash_grape(struct battle *battle,double x,double y,int spriteid_player);
static void _grape_update(struct batsup_sprite *sprite,double elapsed);

/* Delete.
 */
 
static void _wining_del(struct battle *battle) {
  batsup_world_del(BATTLE->world);
}

/* Player sprite.
 */
 
struct sprite_player {
  struct batsup_sprite hdr;
  int ctl;
  int face;
  double skill;
  int walking;
  int animframe;
  double animclock;
  int blackout;
  int indx,injump;
  double walkspeed;
  double fallstarty; // Vertical position where we started falling.
  int jumping; // True while ascending.
  int jumpok; // True while touching floor.
  double jumppower; // Positive m/s, volatile.
  double jumppower_initial; // Positive m/s, constant.
  double jumpdecay; // Positive m/s**2, constant.
  double gravity; // Positive m/s, volatile.
  double gravity_rate; // Positive m/s**2, constant.
  double gravity_limit; // Positive m/s, constant.
  double squash_distance; // m, constant.
  double grape_value; // 0..1 per grape, constant.
};

/* Gather input for human player.
 */
 
static void player_update_man(struct batsup_sprite *sprite,double elapsed,int input,int pvinput) {
  struct sprite_player *SPRITE=(void*)sprite;
  if (SPRITE->blackout) {
    if (!(input&EGG_BTN_SOUTH)) SPRITE->blackout=0;
  } else {
    switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
      case EGG_BTN_LEFT: SPRITE->indx=-1; break;
      case EGG_BTN_RIGHT: SPRITE->indx=1; break;
      default: SPRITE->indx=0; break;
    }
    SPRITE->injump=(input&EGG_BTN_SOUTH)?1:0;
  }
}

/* CPU player controller.
 * Our algorithm puts the CPU at a natural disadvantage in two important ways:
 *  1. He doesn't prioritize multi-squash piles.
 *  2. He always jumps to the fullest extent.
 * At balanced difficulty, the score will always be close but it's actually pretty hard for a human to lose.
 */
 
static void player_update_cpu(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(void*)sprite;
  struct battle *battle=sprite->world->battle;
  
  /* Never stop jumping.
   * Only release the jump button when falling.
   */
  if (SPRITE->jumping||SPRITE->jumpok) {
    SPRITE->injump=1;
  } else {
    SPRITE->injump=0;
  }
  
  /* Walk horizontally to the nearest grape.
   */
  double horizon=sprite->y-0.250; // Don't target grapes above me (get above them by jumping first).
  double targetx=sprite->x;
  struct batsup_sprite *nearest=0;
  double neardist=999.999; // horizontal distance only.
  struct batsup_sprite **otherp=BATTLE->world->spritev;
  int otheri=BATTLE->world->spritec;
  for (;otheri-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other->update!=_grape_update) continue;
    if (other->y<horizon) continue;
    double dx=other->x-sprite->x;
    double adx=(dx<0.0)?-dx:dx;
    if (!nearest||(adx<neardist)) {
      nearest=other;
      neardist=adx;
      targetx=other->x;
    }
  }
  // My target must be within the pit. And fuzz this out a little, because we're actually going to aim a little off the target.
  if (targetx<4.75) targetx=4.75;
  else if (targetx>15.25) targetx=15.25;
  double dx=targetx-sprite->x;
  if (dx>0.125) SPRITE->indx=1;
  else if (dx<-0.125) SPRITE->indx=-1;
  else SPRITE->indx=0;
}

/* Update player.
 */
 
static void player_update(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(void*)sprite;
  struct battle *battle=sprite->world->battle;
  
  // Gather input or make decisions for CPU players.
  if (battle->outcome>-2) {
    SPRITE->indx=0;
    SPRITE->injump=0;
  } else if (SPRITE->ctl) {
    player_update_man(sprite,elapsed,g.input[SPRITE->ctl],g.pvinput[SPRITE->ctl]);
  } else {
    player_update_cpu(sprite,elapsed);
  }
  
  // Walk if requested.
  if (SPRITE->indx) {
    if (!SPRITE->walking) {
      SPRITE->walking=1;
    }
    sprite->xform=(SPRITE->indx>0)?0:EGG_XFORM_XREV;
    batsup_sprite_move(sprite,SPRITE->walkspeed*elapsed*SPRITE->indx,0.0);
  } else if (SPRITE->walking) {
    SPRITE->walking=0;
  }
  
  // Animate walking.
  if (SPRITE->walking) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.200;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    }
  } else {
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
  }
  
  // Pay out running jump. And poll for termination.
  if (SPRITE->jumping) {
    SPRITE->jumppower-=SPRITE->jumpdecay*elapsed;
    if (!SPRITE->injump) {
      SPRITE->jumping=0;
    } else if (SPRITE->jumppower<=0.0) {
      SPRITE->jumping=0;
    } else {
      // It is possible to get stuck inside a grape, in fact it happens all the time.
      // So when moving upward, ignore collisions.
      // This means you can get your head stuck in the other player's feet. Not sure how likely that will be.
      //batsup_sprite_move(sprite,0.0,-SPRITE->jumppower*elapsed);
      sprite->y-=SPRITE->jumppower*elapsed;
      SPRITE->fallstarty=sprite->y;
    }
    
  // Is a jump starting?
  } else if (SPRITE->jumpok&&SPRITE->injump) {
    bm_sound_pan(RID_sound_jump,(sprite->id==SPRITEID_R)?PLAYER_PAN:-PLAYER_PAN);
    SPRITE->jumpok=0;
    SPRITE->jumping=1;
    SPRITE->jumppower=SPRITE->jumppower_initial;
    
  // Not jumping, so apply gravity.
  } else {
    SPRITE->gravity+=SPRITE->gravity_rate*elapsed;
    if (SPRITE->gravity>=SPRITE->gravity_limit) {
      SPRITE->gravity=SPRITE->gravity_limit;
    }
   _fall_again_:;
    double y0=sprite->y;
    double yexpect=SPRITE->gravity*elapsed;
    if (!batsup_sprite_move(sprite,0.0,yexpect)) {
     _floor_:;
      double fall=sprite->y-SPRITE->fallstarty;
      if (fall>0.0) {
        if (fall>=SPRITE->squash_distance) {
          if (wining_squash_grape(battle,sprite->x,sprite->y+0.5,sprite->id)) {
            goto _fall_again_;
          }
        }
      }
      SPRITE->gravity=0.0;
      if (!SPRITE->injump) SPRITE->jumpok=1;
      SPRITE->fallstarty=sprite->y;
    } else {
      // Move might have reported a change but just a hair. If it's substantially less than (yexpect), treat it as the floor.
      // Comes up when you're standing on a grape that's sliding.
      double yactual=sprite->y-y0;
      if (yactual<yexpect*0.5) {
        goto _floor_;
      }
    }
  }
}

/* Render player.
 */
 
static void player_render(struct batsup_sprite *sprite,int dstx,int dsty) {
  struct sprite_player *SPRITE=(void*)sprite;
  struct battle *battle=sprite->world->battle;
  graf_set_image(&g.graf,RID_image_battle_forest2);
  uint8_t tileid;
  switch (SPRITE->face) {
    case NS_face_dot: tileid=0xd4; break;
    case NS_face_princess: tileid=0xe4; break;
    case NS_face_monster: tileid=0xf4; break;
  }
  if (SPRITE->walking) switch (SPRITE->animframe) {
    case 1: tileid+=1; break;
    case 3: tileid+=2; break;
  }
  graf_tile(&g.graf,dstx,dsty,tileid,sprite->xform);
}

/* Spawn player.
 */
 
static struct batsup_sprite *wining_spawn_player(struct battle *battle,int id,int ctl,int face,double skill) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,id,sizeof(struct sprite_player));
  if (!sprite) return 0;
  struct sprite_player *SPRITE=(void*)sprite;
  sprite->solid=1;
  sprite->update=player_update;
  sprite->render=player_render;
  SPRITE->ctl=ctl;
  SPRITE->face=face;
  SPRITE->skill=skill;
  SPRITE->blackout=1;
  SPRITE->walkspeed=3.750*(1.0-skill)+8.000*skill;
  SPRITE->jumppower_initial=16.0;
  SPRITE->jumpdecay=30.0;
  SPRITE->gravity_rate=20.0;
  SPRITE->gravity_limit=15.0;
  SPRITE->squash_distance=2.000*(1.0-skill)+1.000*skill;
  SPRITE->grape_value=0.020*(1.0-skill)+0.040*skill; // At top difficulty, a human can win, but only with extensive blocking.
  sprite->y=5.5;
  SPRITE->fallstarty=sprite->y;
  if (id==SPRITEID_L) {
    sprite->x=3.0;
  } else {
    sprite->x=17.0;
    sprite->xform=EGG_XFORM_XREV;
  }
  return sprite;
}

/* Bottle sprite.
 */
 
struct sprite_bottle {
  struct batsup_sprite hdr;
  double fill; // 0..1
  double target; // 0..1 but may go over
};

static void _bottle_update(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_bottle *SPRITE=(void*)sprite;
  struct battle *battle=sprite->world->battle;
  if (SPRITE->fill<SPRITE->target) {
    if (SPRITE->target>1.0) SPRITE->target=1.0;
    SPRITE->fill+=0.500*elapsed;
    if (SPRITE->fill>SPRITE->target) SPRITE->fill=SPRITE->target;
  }
}

static void _bottle_render(struct batsup_sprite *sprite,int dstx,int dsty) {
  struct sprite_bottle *SPRITE=(void*)sprite;
  struct battle *battle=sprite->world->battle;
  // (dstx,dsty) is my top left corner.
  graf_set_image(&g.graf,RID_image_battle_forest2);
  graf_decal(&g.graf,dstx,dsty,0,144,32,96);
  int fillh=(int)(SPRITE->fill*96.0);
  if (fillh>0) {
    if (fillh>96) fillh=96;
    graf_decal(&g.graf,dstx,dsty+96-fillh,32,144+96-fillh,32,fillh);
  }
}

/* Spawn bottle.
 */
 
static struct batsup_sprite *wining_spawn_bottle(struct battle *battle,int id) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,id,sizeof(struct sprite_bottle));
  if (!sprite) return 0;
  sprite->update=_bottle_update;
  sprite->render=_bottle_render;
  sprite->y=0.0;
  switch (id) {
    case SPRITEID_LBOTTLE: {
        sprite->x=0.0;
      } break;
    case SPRITEID_RBOTTLE: {
        sprite->x=18.0;
      } break;
  }
  return sprite;
}

/* Juice sprite: Decoration that just flies up and disappears.
 */
 
struct sprite_juice {
  struct batsup_sprite hdr;
  double dx,dy;
  double clock;
};

static void _juice_update(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_juice *SPRITE=(void*)sprite;
  if ((SPRITE->clock+=elapsed)>=1.0) {
    sprite->defunct=1;
  }
  SPRITE->dy+=10.0*elapsed;
  sprite->x+=SPRITE->dx*elapsed;
  sprite->y+=SPRITE->dy*elapsed;
}

/* Grape sprite.
 */
 
struct sprite_grape {
  struct batsup_sprite hdr;
  double pvx,pvy;
};

/* Update grape.
 * Grapes are "solid" for batsup purposes, which means players treat them as squares.
 * But for our own purposes, we do the physics and we treat them as circles.
 * During the update cycle, set (pvx,pvy) and then optimistically apply gravity.
 * I don't expect grapes to fall far, so let's try constant gravity for starters.
 */
 
static void _grape_update(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_grape *SPRITE=(void*)sprite;
  struct battle *battle=sprite->world->battle;
  SPRITE->pvx=sprite->x;
  SPRITE->pvy=sprite->y;
  sprite->y+=5.0*elapsed;
}

/* After the batsup update, we do another pass checking for grape collisions.
 */
 
static void wining_update_physics(struct battle *battle,double elapsed) {
  struct batsup_sprite **ap=BATTLE->world->spritev;
  int ai=0;
  for (;ai<BATTLE->world->spritec;ai++,ap++) {
    struct batsup_sprite *a=*ap;
    
    /* Use (update) rather than (id) for the is-a-grape test.
     * Squashed grapes turn into gore and retain their original id.
     * But when they squash, we nix their (update) hook.
     */
    if (a->update!=_grape_update) continue;
    
    /* Check all other grapes.
     * Also check players, we must not bump into them either.
     */
    struct batsup_sprite **bp=BATTLE->world->spritev;
    int bi=0;
    for (;bi<ai;bi++,bp++) {
      struct batsup_sprite *b=*bp;
      
      if (b->id<=SPRITEID_R) {
        double dx=b->x-a->x;
        double dy=b->y-a->y;
        const double pdist=0.900;
        if ((dx>-pdist)&&(dx<pdist)&&(dy>-pdist)&&(dy<pdist)) {
          // Colliding with player.
          double adx=(dx<0.0)?-dx:dx;
          double ady=(dy<0.0)?-dy:dy;
          if ((adx<ady)||(dy>-0.125)) {
            if (dx<0.0) a->x=b->x+pdist;
            else a->x=b->x-pdist;
          } else {
            b->y=a->y-pdist;
            //if (dy<0.0) a->y=b->y+pdist;
            //else a->y=b->y-pdist;
          }
        }
        continue;
      }
      
      if (b->update!=_grape_update) continue;
      double dx=b->x-a->x;
      double dy=b->y-a->y;
      double d2=dx*dx+dy*dy;
      if (d2>=1.0) continue; // No collision.
      if (d2<0.010) continue; // Panic, impossibly close.
      double distance=sqrt(d2);
      double pen=1.0-distance;
      if (pen<=0.0) continue; // No collision. Maybe possible due to rounding.
      double nx=dx/distance; // Normal pointing toward (b).
      double ny=dy/distance;
      b->x+=nx*pen*0.5;
      b->y+=ny*pen*0.5;
      a->x-=nx*pen*0.5;
      a->y-=ny*pen*0.5;
    }
    
    /* Grapes can't leave the pit.
     * So apply a hard limit left, right, and bottom.
     */
    if (a->x<4.5) a->x=4.5;
    else if (a->x>15.5) a->x=15.5;
    if (a->y>9.5) a->y=9.5;
  }
  
  /* One more thing.
   * Because players turn physics off when moving upward, there can be teleporter-mishap situations.
   * These come up pretty often in cpu-vs-cpu, because they are so symmetric.
   * ...well. Saw it once at least. Ten subsequent tries and nothing.
   * But you can see the problem reliably in one-player mode too, just try to touch the hippopotamus.
   * Check the players against each other, and force them apart.
   */
  struct batsup_sprite *l=batsup_sprite_by_id(BATTLE->world,SPRITEID_L);
  struct batsup_sprite *r=batsup_sprite_by_id(BATTLE->world,SPRITEID_R);
  if (l&&r) {
    // Also, as long as we're here, don't let them breach the ceiling. batsup allows ceiling breach by default.
    if (l->y<0.5) l->y=0.5;
    if (r->y<0.5) r->y=0.5;
    const double tooclose=0.900;
    double dx=r->x-l->x;
    if ((dx>-tooclose)&&(dx<tooclose)) {
      double dy=r->y-l->y;
      if ((dy>-tooclose)&&(dy<tooclose)) {
        double adx=(dx<0.0)?-dx:dx;
        double ady=(dy<0.0)?-dy:dy;
        if (ady<adx) {
          double nudge=tooclose-adx;
          if (dx>0.0) {
            l->x-=nudge*0.5;
            r->x+=nudge*0.5;
          } else {
            l->x+=nudge*0.5;
            r->x-=nudge*0.5;
          }
        } else {
          double nudge=tooclose-ady;
          if (dy>0.0) {
            l->x-=nudge*0.5;
            r->x+=nudge*0.5;
          } else {
            l->x+=nudge*0.5;
            r->x-=nudge*0.5;
          }
        }
      }
    }
  }
}

/* Find a grape near (x,y) and squash it.
 */

static int wining_squash_grape(struct battle *battle,double x,double y,int spriteid_player) {
  int result=0;
  //struct batsup_sprite **spritep=BATTLE->world->spritev; // Don't use a pointer! We might add sprites along the way.
  int i=BATTLE->world->spritec;
  for (;i-->0;) {
    struct batsup_sprite *sprite=BATTLE->world->spritev[i];
    if (sprite->update!=_grape_update) continue;
    double dx=sprite->x-x;
    if ((dx<=-1.0)||(dx>=1.0)) continue;
    double dy=sprite->y-0.75-y;
    if ((dy<-0.5)||(dy>0.5)) continue;
    
    int juici=3;
    while (juici-->0) {
      struct batsup_sprite *juice=batsup_sprite_spawn(BATTLE->world,0,sizeof(struct sprite_juice));
      if (juice) {
        struct sprite_juice *JUICE=(void*)juice;
        JUICE->dx=((rand()&0xffff)*3.0)/65535.0-1.5;
        JUICE->dy=-5.0;
        juice->update=_juice_update;
        juice->x=sprite->x;
        juice->y=sprite->y;
        juice->tileid=0xf0+(rand()&3);
        juice->xform=rand()&7;
        juice->layer=200;
      }
    }
    
    sprite->update=0;
    sprite->tileid=0xc6+(rand()%3);
    sprite->y=10.350;
    sprite->solid=0;
    result++;
  }
  if (result) {
    bm_sound_pan(RID_sound_collect,(spriteid_player==SPRITEID_R)?PLAYER_PAN:-PLAYER_PAN);
    struct batsup_sprite *player=batsup_sprite_by_id(BATTLE->world,spriteid_player);
    struct batsup_sprite *bottle=batsup_sprite_by_id(BATTLE->world,(spriteid_player==SPRITEID_L)?SPRITEID_LBOTTLE:SPRITEID_RBOTTLE);
    if (player&&bottle) {
      struct sprite_player *PLAYER=(void*)player;
      struct sprite_bottle *BOTTLE=(void*)bottle;
      BOTTLE->target+=PLAYER->grape_value*result;
    }
  }
  return result;
}

/* Spawn grape.
 */
 
static struct batsup_sprite *wining_spawn_grape(struct battle *battle,int id,double x,double y) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,id,sizeof(struct sprite_grape));
  if (!sprite) return 0;
  sprite->update=_grape_update;
  sprite->x=x;
  sprite->y=y;
  sprite->tileid=0xc4;
  sprite->layer=101; // Important that grapes be after players in the list.
  sprite->solid=1;
  return sprite;
}

/* New.
 */
 
static int _wining_init(struct battle *battle) {
  double lskill,rskill;
  battle_normalize_bias(&lskill,&rskill,battle);
  if (!(BATTLE->world=batsup_world_new(battle,0))) return -1;
  if (batsup_world_set_image(BATTLE->world,RID_image_battle_forest2)<0) return -1;
  
  /* Generate the map.
   */
  memset(BATTLE->world->map->v,0xb8,NS_sys_mapw*NS_sys_maph);
  uint8_t *row=BATTLE->world->map->v+6*NS_sys_mapw;
  row[0]=row[1]=row[2]=0x95;
  row[3]=0x96;
  row[16]=0x94;
  row[17]=row[18]=row[19]=0x95;
  row+=NS_sys_mapw;
  row[0]=row[1]=row[2]=0xa5;
  row[3]=0xa6;
  row[16]=0xa4;
  row[17]=row[18]=row[19]=0xa5;
  row+=NS_sys_mapw;
  memcpy(row,row-NS_sys_mapw,NS_sys_mapw);
  row+=NS_sys_mapw;
  memcpy(row,row-NS_sys_mapw,NS_sys_mapw);
  row+=NS_sys_mapw;
  row[0]=row[1]=row[2]=0xa5;
  row[3]=0xa7;
  memset(row+4,0x95,12);
  row[16]=0xa8;
  row[17]=row[18]=row[19]=0xa5;
  row+=NS_sys_mapw;
  memset(row,0xa5,NS_sys_mapw);
  
  /* Generate player and bottle sprites.
   */
  if (!wining_spawn_player(battle,SPRITEID_L,battle->args.lctl,battle->args.lface,lskill)) return -1;
  if (!wining_spawn_player(battle,SPRITEID_R,battle->args.rctl,battle->args.rface,rskill)) return -1;
  if (!wining_spawn_bottle(battle,SPRITEID_LBOTTLE)) return -1;
  if (!wining_spawn_bottle(battle,SPRITEID_RBOTTLE)) return -1;
  
  /* Generate grapes.
   */
  int id=SPRITEID_GRAPE;
  double y=9.5;
  double x;
  for (x=4.5;x<16.0;x+=1.0) {
    if (!wining_spawn_grape(battle,id++,x,y)) return -1;
  }
  y-=1.0;//M_SQRT1_2;
  for (x=5.0;x<15.5;x+=1.0) {
    if (!wining_spawn_grape(battle,id++,x,y)) return -1;
  }
  y-=1.0;//M_SQRT1_2;
  for (x=5.5;x<15.0;x+=1.0) {
    if (!wining_spawn_grape(battle,id++,x,y)) return -1;
  }
  
  return 0;
}

/* Update.
 */
 
static void _wining_update(struct battle *battle,double elapsed) {
  batsup_world_update(BATTLE->world,elapsed);
  wining_update_physics(battle,elapsed);
  
  /* Game is over when all the grapes are squashed or a bottle is full.
   * It won't happen, but if we fail to find either bottle, declare a tie.
   */
  if (battle->outcome==-2) {
    struct batsup_sprite *lbottle=batsup_sprite_by_id(BATTLE->world,SPRITEID_LBOTTLE);
    struct batsup_sprite *rbottle=batsup_sprite_by_id(BATTLE->world,SPRITEID_RBOTTLE);
    struct sprite_bottle *LBOTTLE=(void*)lbottle;
    struct sprite_bottle *RBOTTLE=(void*)rbottle;
    if (!lbottle||!rbottle) {
      battle->outcome=0;
    } else if (LBOTTLE->fill>=FILL_FINISH) {
      if (RBOTTLE->fill>=FILL_FINISH) battle->outcome=0;
      else battle->outcome=1;
    } else if (RBOTTLE->fill>=FILL_FINISH) {
      battle->outcome=-1;
    } else {
      int have_grape=0;
      struct batsup_sprite **p=BATTLE->world->spritev;
      int i=BATTLE->world->spritec;
      for (;i-->0;p++) {
        struct batsup_sprite *sprite=*p;
        if (sprite->update==_grape_update) {
          have_grape=1;
          break;
        }
      }
      if (!have_grape) {
        if (LBOTTLE->fill>RBOTTLE->fill) battle->outcome=1;
        else if (LBOTTLE->fill<RBOTTLE->fill) battle->outcome=-1;
        else battle->outcome=0;
      }
    }
  }
}

/* Render.
 */
 
static void _wining_render(struct battle *battle) {
  batsup_world_render(BATTLE->world);
}

/* Type definition.
 */
 
const struct battle_type battle_type_wining={
  .name="wining",
  .objlen=sizeof(struct battle_wining),
  .id=NS_battle_wining,
  .strix_name=338,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz_a,
  .imageid_default=0,
  .del=_wining_del,
  .init=_wining_init,
  .update=_wining_update,
  .render=_wining_render,
};

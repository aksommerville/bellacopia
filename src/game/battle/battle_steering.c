/* battle_steering.c
 */

#include "game/bellacopia.h"

#define TLIMIT 1.0 /* 0..pi/2 */
#define SPRITE_LIMIT 32
#define TURBO_TIME 1.500
#define BONK_TIME 1.000

struct battle_steering {
  struct battle hdr;
  double len; // Road length in pixels.
  
  /* "sprite" is any moveable feature that isn't one of the players.
   */
  struct steering_sprite {
    double x,y;
    uint8_t tileid;
    uint8_t xform;
    int defunct;
    void (*update)(struct battle *battle,struct steering_sprite *sprite,double elapsed);
    double animclock;
    double bonkclock;
    double speed;
  } spritev[SPRITE_LIMIT];
  int spritec;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double x,y; // Position in world pixels.
    double t;
    double speed; // px/s
    double steerrate; // rad/s
    double bonkpeak; // px/s, must be substantially higher than (speed)
    
    int insteer;
    int done;
    double turboclock;
    double bonkclock;
    int collision;
    double collisionclock;
    
  } playerv[2];
};

#define BATTLE ((struct battle_steering*)battle)

/* Delete.
 */
 
static void _steering_del(struct battle *battle) {
}

/* Speed boost.
 */
 
static void speedboost_update(struct battle *battle,struct steering_sprite *sprite,double elapsed) {
  const double thresh=12.0;
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->turboclock>TURBO_TIME*0.750) continue; // Don't immediately retrigger. But don't require it to wind all the way down either.
    double dy=player->y-sprite->y;
    if ((dy<-thresh)||(dy>thresh)) continue;
    double dx=player->x-sprite->x;
    if ((dx<-thresh)||(dx>thresh)) continue;
    player->turboclock=TURBO_TIME;
    bm_sound_pan(RID_sound_turbo,player->who?PLAYER_PAN:-PLAYER_PAN);
  }
}

/* Duck.
 */
 
static void duck_update(struct battle *battle,struct steering_sprite *sprite,double elapsed) {

  if (sprite->bonkclock>0.0) {
    sprite->bonkclock-=elapsed;
    if (sprite->bonkclock<=0.0) {
      sprite->tileid=0xb9;
    }
    return;
  }
  
  if ((sprite->animclock-=elapsed)<=0.0) {
    sprite->animclock+=0.250;
    if (sprite->tileid==0xb9) sprite->tileid=0xba;
    else sprite->tileid=0xb9;
  }
  
  if (sprite->xform) {
    sprite->x-=sprite->speed*elapsed;
    if (sprite->x<20.0) sprite->xform=0;
  } else {
    sprite->x+=sprite->speed*elapsed;
    if (sprite->x>130.0) sprite->xform=EGG_XFORM_XREV;
  }
  
  const double thresh=12.0;
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->bonkclock>0.0) continue; // Wait for their last bounce to finish.
    double dy=player->y-sprite->y;
    if ((dy<0.0)||(dy>thresh)) continue; // 0.0 instead of (-thresh): Let them clip us on the backside.
    double dx=player->x-sprite->x;
    if ((dx<-thresh)||(dx>thresh)) continue;
    bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->bonkclock=BONK_TIME;
    sprite->bonkclock=1.000;
    sprite->tileid=0xbb;
    break;
  }
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  int fldw=(FBW-11)>>1;
  player->x=fldw*0.5;
  player->y=BATTLE->len;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x-=NS_sys_tilesize*2;
  } else { // Right.
    player->who=1;
    player->x+=NS_sys_tilesize*2;
  }
  player->speed=90.0*(1.0-player->skill)+110.0*player->skill;
  player->bonkpeak=250.0*(1.0-player->skill)+200.0*player->skill;
  player->steerrate=4.0*(1.0-player->skill)+5.0*player->skill;
  if (player->human=human) { // Human.
  } else { // CPU.
    player->speed*=0.900; // cpu penalty
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xd59f3dff;
        player->tileid=0x49;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x09;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x29;
      } break;
  }
}

/* Generate all the sprites.
 */
 
static struct steering_sprite *steering_spawn_sprite(struct battle *battle) {
  if (BATTLE->spritec>=SPRITE_LIMIT) return 0;
  struct steering_sprite *sprite=BATTLE->spritev+BATTLE->spritec++;
  return sprite;
}
 
static int steering_generate_sprites(struct battle *battle) {
  int i;
  struct steering_sprite *sprite;
  const double xlo=30.0;
  const double xhi=120.0;
  const double ylo=40.0;
  double yhi=BATTLE->len-80.0;
  const int duckc=16;
  const int turboc=4;
  const int blankc=20;
  const int slotc=duckc+turboc+blankc;
  #define R(lo,hi) ({ \
    double _n=(rand()&0xffff)/65535.0; \
    ((lo)*(1.0-_n)+(hi)*_n); \
  })
  
  /* Put the things in slots spaced evenly along the vertical axis.
   * There's a substantial quantity of blank slots too, so it doesn't look too uniform.
   */
  double slotspacing=(yhi-ylo)/(double)slotc;
  char slotv[40]={0}; // "dt\0" = duck,turbo,blank
  int candidatev[40];
  if (sizeof(slotv)!=slotc) return -1; // Can't use "slotc" as the actual array length without a gnu extension :(
  int candidatec=slotc;
  for (i=candidatec;i-->0;) candidatev[i]=i;
  for (i=duckc;i-->0;) {
    int candidatep=rand()%candidatec;
    slotv[candidatev[candidatep]]='d';
    candidatec--;
    memmove(candidatev+candidatep,candidatev+candidatep+1,sizeof(int)*(candidatec-candidatep));
  }
  for (i=turboc;i-->0;) {
    int candidatep=rand()%candidatec;
    slotv[candidatev[candidatep]]='t';
    candidatec--;
    memmove(candidatev+candidatep,candidatev+candidatep+1,sizeof(int)*(candidatec-candidatep));
  }
  
  /* Turbos first.
   * Sprites are in render order.
   */
  for (i=slotc;i-->0;) {
    if (slotv[i]!='t') continue;
    if (!(sprite=steering_spawn_sprite(battle))) return -1;
    sprite->x=R(xlo,xhi);
    sprite->y=ylo+i*slotspacing;
    sprite->tileid=0xb8;
    sprite->xform=0;
    sprite->update=speedboost_update;
  }
  
  /* Ducks.
   */
  for (i=slotc;i-->0;) {
    if (slotv[i]!='d') continue;
    if (!(sprite=steering_spawn_sprite(battle))) return -1;
    sprite->x=R(xlo,xhi);
    sprite->y=ylo+i*slotspacing;
    sprite->tileid=0xb9;
    sprite->xform=(rand()&1)?EGG_XFORM_XREV:0;
    sprite->update=duck_update;
    sprite->animclock=((rand()&0xffff)*0.200)/65535.0;
    sprite->speed=10.0+((rand()&0xffff)*20.0)/65535.0;
  }
  
  #undef R
  return 0;
}

/* New.
 */
 
static int _steering_init(struct battle *battle) {
  BATTLE->len=1600.0;
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  if (steering_generate_sprites(battle)<0) return -1;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->insteer=-1; break;
    case EGG_BTN_RIGHT: player->insteer=1; break;
    default: player->insteer=0; break;
  }
}

/* Update CPU player.
 * This CPU player is pretty good. He hardly ever hits a duck, but does tend to over-steer.
 * With balanced parameters and not much practice, I beat him most of the time but not all.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Find the next sprite.
   * It's overkill, but whatever, find it from scratch every time.
   * If sufficiently far away, ignore it.
   */
  struct steering_sprite *next=0;
  struct steering_sprite *q=BATTLE->spritev;
  int i=BATTLE->spritec;
  for (;i-->0;q++) {
    if (q->y>player->y) continue;
    if (!next||(q->y>next->y)) next=q;
  }
  if (next&&(player->y-next->y>60.0)) next=0;
  
  /* Steer according to (next):
   *  - none: Dead center. Steer a little to get off the edges if need be.
   *  - duck: Away.
   *  - turbo: Toward.
   */
  double ttarget=0.0;
  if (!next) {
    if (player->x<40.0) ttarget=TLIMIT*0.5;
    else if (player->x>110.0) ttarget=TLIMIT*-0.5;

  } else if (next->update==duck_update) {
    double dx=next->x-player->x;
    if ((dx>-30.0)&&(dx<30.0)) { // Close enough to care about.
      if ((dx<-25.0)||(dx>25.0)) { // Maintain angle near the edges, just to reduce jitter.
        ttarget=player->t;
      } else if (next->x<30.0) { // Duck near left edge -- steer right regardless of relative positions.
        ttarget=TLIMIT;
      } else if (next->x>120.0) { // '' right edge.
        ttarget=-TLIMIT;
      } else if (dx<0.0) {
        ttarget=TLIMIT;
      } else {
        ttarget=-TLIMIT;
      }
    } else {
      ttarget=0.0;
    }

  } else if (next->update==speedboost_update) {
    double dx=next->x-player->x;
    if (dx<-20.0) ttarget=-TLIMIT;
    else if (dx>20.0) ttarget=TLIMIT;
    else ttarget=0.0;
  } 
  
  if (ttarget<player->t) {
    player->insteer=-1;
  } else if (ttarget>player->t) {
    player->insteer=1;
  } else {
    player->insteer=0;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* If there was a collision, it plays out in the horizontal only.
   */
  if (player->collision) {
    if ((player->collisionclock-=elapsed)<=0.0) {
      player->collision=0;
    } else {
      player->x+=player->collision*player->collisionclock*100.0*elapsed;
    }
  }

  /* Steer.
   */
  if (player->insteer) {
    player->t+=player->insteer*player->steerrate*elapsed;
    if (player->t<-TLIMIT) player->t=-TLIMIT;
    else if (player->t>TLIMIT) player->t=TLIMIT;
  }
  
  /* Turbo?
   */
  double turbo=1.000;
  if (player->turboclock>0.0) {
    turbo=1.000+player->turboclock/TURBO_TIME;
    player->turboclock-=elapsed;
  }
  
  /* Regular motion.
   */
  player->x+=sin(player->t)*player->speed*turbo*elapsed;
  player->y-=cos(player->t)*player->speed*turbo*elapsed;
  if (player->x<20.0) player->x=20.0;
  else if (player->x>130.0) player->x=130.0;
  if (player->y<=0.0) player->done=1;
  
  /* Bonk?
   */
  if (player->bonkclock>0.0) {
    double vel=(player->bonkclock*player->bonkpeak)/BONK_TIME;
    player->y+=vel*elapsed;
    player->bonkclock-=elapsed;
  }
}

/* Update.
 */
 
static void _steering_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Update sprites.
   */
  struct steering_sprite *sprite=BATTLE->spritev+BATTLE->spritec-1;
  for (i=BATTLE->spritec;i-->0;sprite--) {
    if (sprite->update) sprite->update(battle,sprite,elapsed);
    if (sprite->defunct) {
      BATTLE->spritec--;
      memmove(sprite,sprite+1,sizeof(struct steering_sprite)*(BATTLE->spritec-i));
    }
  }
  
  /* Check for player-on-player collisions.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (!l->collision) {
    double dy=r->y-l->y;
    if ((dy>=-12.0)&&(dy<12.0)) {
      double dx=r->x-l->x;
      if ((dx>=-12.0)&&(dx<12.0)) {
        bm_sound(RID_sound_ouch);
        l->collisionclock=1.000;
        r->collisionclock=1.000;
        if (dx>0.0) {
          l->collision=-1;
          r->collision=1;
        } else {
          l->collision=1;
          r->collision=-1;
        }
      }
    }
  }
  
  /* First across the finish line wins. Ties are narrowly possible.
   */
  if (l->done) {
    if (r->done) battle->outcome=0;
    else battle->outcome=1;
  } else if (r->done) battle->outcome=-1;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player,int scenex,int sceney,int scenew,int sceneh) {
  int scroll=player->y-sceneh+20;

  // Fill with the ground color, and darker stripes to make motion clear.
  graf_fill_rect(&g.graf,scenex,sceney,scenew,sceneh,battle->ctab[BATTLE_COLOR_GROUND]);
  const int stripew=40;
  int stripey=-scroll%(stripew*2);
  if (stripey>0) stripey-=stripew*2;
  for (;stripey<sceneh;stripey+=stripew*2) {
    graf_fill_rect(&g.graf,scenex,sceney+stripey,scenew,stripew,0x00000040);
  }
  
  // The road, 8x2 tiles repeating.
  graf_set_image(&g.graf,RID_image_battle_forest);
  int roadsrcx=NS_sys_tilesize*8;
  int roadsrcy=NS_sys_tilesize*9;
  int roadw=NS_sys_tilesize*8;
  int roadh=NS_sys_tilesize*2;
  int roaddstx=scenex+(scenew>>1)-(roadw>>1);
  int roady=-scroll%roadh;
  if (roady>0) roady-=roadh;
  for (;roady<sceneh;roady+=roadh) {
    graf_decal(&g.graf,roaddstx,roady,roadsrcx,roadsrcy,roadw,roadh);
  }
  
  // Finish line.
  int finy=-scroll;
  if ((finy>-8)&&(finy<sceneh+8)) {
    int finx=roaddstx+NS_sys_tilesize+(NS_sys_tilesize>>1);
    int i=6; for (;i-->0;finx+=NS_sys_tilesize) {
      graf_tile(&g.graf,finx,finy,0xbc,0);
    }
  }
  
  // Sprites.
  struct steering_sprite *sprite=BATTLE->spritev;
  int i=BATTLE->spritec;
  for (;i-->0;sprite++) {
    graf_tile(&g.graf,scenex+(int)sprite->x,sceney+(int)sprite->y-scroll,sprite->tileid,sprite->xform);
  }
  
  // Player sprite.
  int px=scenex+(int)player->x;
  if ((px>=scenex)&&(px<scenex+scenew)) {
    int py=sceney+(int)player->y-scroll;
    uint8_t rot=(int8_t)((player->t*128.0)/M_PI);
    graf_fancy(&g.graf,px,py,player->tileid,0,rot,NS_sys_tilesize,0,player->color);
  }
  
  // The other guy.
  struct player *other=(player==BATTLE->playerv)?(BATTLE->playerv+1):(BATTLE->playerv+0);
  px=scenex+(int)other->x;
  if ((px>=scenex)&&(px<scenex+scenew)) {
    int py=sceney+(int)other->y-scroll;
    uint8_t rot=(int8_t)((other->t*128.0)/M_PI);
    graf_fancy(&g.graf,px,py,other->tileid,0,rot,NS_sys_tilesize,0,other->color);
  }
}

/* Render one thumb of the middle progress bar.
 */
 
static void steering_render_progress(int x,int y,int w,int h,double t,uint32_t color) {
  int ty=(int)(t*(h-w));
  if (ty<0) ty=0; else if (ty>h-w) ty=h-w;
  ty+=y;
  graf_fill_rect(&g.graf,x,ty,w,w,color);
}

/* Render.
 */
 
static void _steering_render(struct battle *battle) {

  // Two player scenes.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  int midbarw=11;
  int leftw=(FBW-midbarw)>>1;
  int rightw=(FBW-midbarw)-leftw;
  player_render(battle,l,0,0,leftw,FBH);
  player_render(battle,r,leftw+midbarw,0,rightw,FBH);
  
  // Middle bar showing each player's progress.
  graf_fill_rect(&g.graf,leftw,0,midbarw,FBH,0x000000ff);
  int thumbw=(midbarw-3)>>1;
  steering_render_progress(leftw+1,1,thumbw,FBH-2,l->y/BATTLE->len,l->color);
  steering_render_progress(leftw+2+thumbw,1,thumbw,FBH-2,r->y/BATTLE->len,r->color);
}

/* Type definition.
 */
 
const struct battle_type battle_type_steering={
  .name="steering",
  .objlen=sizeof(struct battle_steering),
  .id=NS_battle_steering,
  .strix_name=284,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_horz,
  .del=_steering_del,
  .init=_steering_init,
  .update=_steering_update,
  .render=_steering_render,
};

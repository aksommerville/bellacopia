/* battle_balancing.c
 */

#include "game/bellacopia.h"

#define GROUNDY 120
#define XMARGIN 20.0

struct battle_balancing {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform; // Doesn't change.
    double x; // Framebuffer pixels. (y) is fixed.
    double tomx,tomy; // Tomato position in framebuffer pixels. Not relative to the hero.
    double tomt;
    int indx;
    double animclock;
    int animframe;
    double walkspeed;
    double friction; // 0..1, tomato's tendency to move with you. Higher is easier.
    double gravity; // px/s. NB no acceleration, there's not enough of a drop for it to matter much.
    int splat;
    int detach; // Nonzero if the tomato has come loose from my head.
    int win;
    double tback,tstop,tfore; // 0<tback<tstop<tfore. Thresholds for CPU movement.
  } playerv[2];
  
  double finishx; // Framebuffer pixels. Finish line can move per bias.
};

#define BATTLE ((struct battle_balancing*)battle)

/* Delete.
 */
 
static void _balancing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {

  player->x=40.0;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->x=FBW-player->x;
  }
  player->tomx=player->x;
  player->tomy=GROUNDY-40.0;
  
  player->walkspeed=45.0*(1.0-player->skill)+55.0*player->skill; // 40 is really hard (with friction 0.5 and gravity 40).
  player->friction=0.250*(1.0-player->skill)+0.600*player->skill; // High friction makes it really hard; you just can't get under the tomato. Low friction forces you to play jitterier and feels more correct.
  player->gravity=40.0;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->tback=0.002;
    player->tstop=0.200;
    player->tfore=0.400; // At 0.5, walkspeed 50 is ok but walkspeed 40 makes him drop it.
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0x9b5c17ff;
        player->tileid=0x40;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x00;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x20;
      } break;
  }
}

/* New.
 */
 
static int _balancing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  double finadj=(battle->args.bias-0x80)/128.0;
  BATTLE->finishx=FBW*0.5+finadj*30.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  
  /* Keep it simple:
   * When the tomato is far enough forward, walk forward.
   * When it's at or behind the top, walk backward.
   * Important that zero be in the back range -- that's our first move.
   */
  if (player->xform) {
         if (player->tomt<-player->tfore) player->indx=-1;
    else if (player->tomt>-player->tback) player->indx=1;
    else if ((player->indx==-1)&&(player->tomt<-player->tstop)) ;
    else player->indx=0;
  } else {
         if (player->tomt>player->tfore) player->indx=1;
    else if (player->tomt<player->tback) player->indx=-1;
    else if ((player->indx==1)&&(player->tomt>player->tstop)) ;
    else player->indx=0;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Once splattered, we're done.
   */
  if (player->splat) return;

  /* Motion and animation.
   */
  if (player->indx&&(battle->outcome==-2)) {
    double x0=player->x;
    player->x+=player->indx*player->walkspeed*elapsed;
    if (player->x<XMARGIN) player->x=XMARGIN;
    else if (player->x>FBW-XMARGIN) player->x=FBW-XMARGIN;
    if (!player->detach) {
      player->tomx+=(player->x-x0)*player->friction; // Friction tends to pull the tomato with you (regardless of its relative position).
    }
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.200;
      if (++(player->animframe)>=4) player->animframe=0;
    }
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
  
  /* Tomato physics.
   * Friction against the head during movement is already accounted for.
   * Here we'll apply gravity and escape the head.
   * Alternately: After winning, the tomato slides back to the top of your head.
   */
  if (player->win) {
    if (player->tomx>player->x+1.0) player->tomx-=10.0*elapsed;
    else if (player->tomx<player->x-1.0) player->tomx+=10.0*elapsed;
  } else {
    player->tomy+=player->gravity*elapsed;
  }
  double headx=player->x;
  double heady=GROUNDY-24.0;
  double headr=8.0;
  double tomr=8.5;
  double dx=player->tomx-headx;
  double dy=player->tomy-heady;
  double d2=dx*dx+dy*dy;
  double rsum=headr+tomr;
  double rsum2=rsum*rsum;
  if (d2<rsum2) { // Collision. Extremely likely.
    double distance=sqrt(d2);
    player->tomx=headx+(dx*rsum)/distance;
    player->tomy=heady+(dy*rsum)/distance;
    player->detach=0;
  } else {
    player->detach=1;
  }
  
  /* Tomato's visual angle is just its angle relative to the head.
   */
  player->tomt=-atan2(headx-player->tomx,heady-player->tomy);
  
  /* If the tomato touches the ground, you lose.
   * Modify (xform) if necessary so the hero looks at what she's done.
   */
  if (player->tomy>GROUNDY) {
    player->splat=1;
    if (player->tomx<player->x) player->xform=EGG_XFORM_XREV;
    else player->xform=0;
    return;
  }
  
  /* Did I win?
   */
  if (!player->detach) {
    if (
      (!player->who&&(player->x>BATTLE->finishx))||
      (player->who&&(player->x<BATTLE->finishx))
    ) {
      player->win=1;
    }
  }
}

/* Update.
 */
 
static void _balancing_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Battle ends when one player reaches the goal, or both splatter.
   * If you leave a 2-player game idle, it will time out into a tie.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->splat&&r->splat) {
      battle->outcome=0;
    } else if (l->win&&r->win) { // Unlikely but possible.
      battle->outcome=0;
    } else if (l->win) {
      battle->outcome=1;
    } else if (r->win) {
      battle->outcome=-1;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  // (xl,xr) are *my* left and right. They swap depending on xform. So (xl) is always where the lower tileid goes.
  int xl=lround(player->x);
  int xr=xl;
  if (player->xform) {
    xl+=NS_sys_tilesize>>1;
    xr-=NS_sys_tilesize>>1;
  } else {
    xl-=NS_sys_tilesize>>1;
    xr+=NS_sys_tilesize>>1;
  }
  int yb=GROUNDY-(NS_sys_tilesize>>1);
  int yt=yb-NS_sys_tilesize;
  
  uint8_t tileid=player->tileid;
  if (player->splat) {
    tileid+=6;
  } else switch (player->animframe) {
    case 1: tileid+=2; break;
    case 3: tileid+=4; break;
  }
  
  graf_tile(&g.graf,xl,yt,tileid+0x00,player->xform);
  graf_tile(&g.graf,xr,yt,tileid+0x01,player->xform);
  graf_tile(&g.graf,xl,yb,tileid+0x10,player->xform);
  graf_tile(&g.graf,xr,yb,tileid+0x11,player->xform);
  
  // Tomato.
  if (player->splat) {
    int tomx=lround(player->tomx);
    int tomy=lround(player->tomy);
    int ht=NS_sys_tilesize>>1;
    graf_tile(&g.graf,tomx-ht,yt,0x28,0);
    graf_tile(&g.graf,tomx+ht,yt,0x29,0);
    graf_tile(&g.graf,tomx-ht,yb,0x38,0);
    graf_tile(&g.graf,tomx+ht,yb,0x39,0);
  } else {
    int tomx=lround(player->tomx);
    int tomy=lround(player->tomy);
    double sint=sin(player->tomt);
    double cost=cos(player->tomt);
    graf_set_filter(&g.graf,1);
    graf_decal_rotate(&g.graf,tomx,tomy,128,0,32,sint,cost,1.0);
    graf_set_filter(&g.graf,0);
  }
}

/* Render.
 */
 
static void _balancing_render(struct battle *battle) {

  /* Background, including finish line indicator.
   */
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  const int checkw=3;
  int finx1=(int)BATTLE->finishx;
  int finx0=finx1-checkw;
  int finy=GROUNDY;
  graf_fill_rect(&g.graf,finx0,finy,checkw*2,FBH-finy,0xffffffff);
  while (finy<FBH) {
    graf_fill_rect(&g.graf,finx0,finy,checkw,checkw,0x000000ff);
    finy+=checkw;
    graf_fill_rect(&g.graf,finx1,finy,checkw,checkw,0x000000ff);
    finy+=checkw;
  }
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);

  /* Heroes. Right first in case they overlap.
   */
  graf_set_image(&g.graf,RID_image_battle_jungle);
  player_render(battle,BATTLE->playerv+1);
  player_render(battle,BATTLE->playerv+0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_balancing={
  .name="balancing",
  .objlen=sizeof(struct battle_balancing),
  .id=NS_battle_balancing,
  .strix_name=324,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz,
  .imageid_default=RID_image_temple,
  .del=_balancing_del,
  .init=_balancing_init,
  .update=_balancing_update,
  .render=_balancing_render,
};

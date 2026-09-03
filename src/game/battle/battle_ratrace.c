/* battle_ratrace.c
 */

#include "game/bellacopia.h"

#define EDGE_LIMIT 40

struct battle_ratrace {
  struct battle hdr;
  double ttl;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double x,y,t;
    double animclock;
    int animframe;
    int q; // Quadrant. 0 is SE, and they proceed clockwise from there. No wrapping: let it go four and above, or negative.
    int midway; // Goes nonzero when we reach quadrant 5, after which point we need to turn around and get back to quadrant 0.
    int done;
    double raceclock;
    
    int indt;
    
    double speed;
    double turnrate;
    double radius;
    double cpuoutness; // 0..1 = inner..outer, where on the track does cpu player aim?
  } playerv[2];
  
  /* Each "edge" is a point, arranged clockwise around the track, describing both the outer and inner wall at that point.
   * (edgev[0]) is the starting line, at the bottom.
   */
  struct edge {
    double ox,oy;
    double ix,iy;
    double olen,ilen; // Length when I am 'a'; ie length to [+1] from here.
  } edgev[EDGE_LIMIT];
  int edgec;
};

#define BATTLE ((struct battle_ratrace*)battle)

/* Delete.
 */
 
static void _ratrace_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  const struct edge *startline=BATTLE->edgev;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=startline->ix*0.666+startline->ox*0.333;
    player->y=startline->iy*0.666+startline->oy*0.333;
    player->t=M_PI*-0.5;
  } else { // Right.
    player->who=1;
    player->x=startline->ix*0.333+startline->ox*0.666;
    player->y=startline->iy*0.333+startline->oy*0.666;
    player->t=M_PI*-0.5;
  }
  player->radius=6.0;
  player->speed=60.0*(1.0-player->skill)+65.0*player->skill;
  player->turnrate=5.000*(1.0-player->skill)+6.000*player->skill;
  if (player->human=human) { // Human.
  } else { // CPU.
    // Scaling outness has a real and believable impact on performance. But he's still too easy at maximum difficulty, so we adjust speed too.
    player->cpuoutness=0.800*(1.0-player->skill)+0.200*player->skill;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x5a321fff;
        player->tileid=0x55;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x35;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x45;
      } break;
  }
}

/* Generate track.
 */
 
static int ratrace_generate_track(struct battle *battle) {
  const double midx=FBW*0.5;
  const double midy=FBH*0.5;
  double t=-M_PI;
  double dt=(M_PI*2.0)/EDGE_LIMIT;
  struct edge *edge=BATTLE->edgev;
  double ir=20.0;
  double or=60.0;
  for (;BATTLE->edgec<EDGE_LIMIT;BATTLE->edgec++,edge++,t+=dt) {
    ir+=((rand()&0xffff)*4.0)/65535.0-2.0;
    if (ir<15.0) ir=15.0; else if (ir>30.0) ir=30.0;
    or+=((rand()&0xffff)*10.0)/65535.0-5.0;
    if (or<50.0) or=50.0; else if (or>85.0) or=85.0;
    edge->ix=midx+sin(t)*ir*1.800;
    edge->iy=midy-cos(t)*ir;
    edge->ox=midx+sin(t)*or*1.800;
    edge->oy=midy-cos(t)*or;
  }
  int i=BATTLE->edgec;
  struct edge *pv=BATTLE->edgev+BATTLE->edgec-1;
  for (edge=BATTLE->edgev;i-->0;edge++) {
    double dx=edge->ox-pv->ox;
    double dy=edge->oy-pv->oy;
    pv->olen=sqrt(dx*dx+dy*dy);
    dx=edge->ix-pv->ix;
    dy=edge->iy-pv->iy;
    pv->ilen=sqrt(dx*dx+dy*dy);
    pv=edge;
  }
  return 0;
}

/* New.
 */
 
static int _ratrace_init(struct battle *battle) {
  if (ratrace_generate_track(battle)<0) return -1;
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->ttl=10.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indt=-1; break;
    case EGG_BTN_RIGHT: player->indt=1; break;
    default: player->indt=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  /* First, will try it nice and simple.
   * Based on my (q) and (midway), aim for the center of a rib near the end of my quadrant.
   * ...oooh this actually works really good.
   */
  int targetp=player->q;
  if (player->midway) targetp-=1;
  targetp&=3;
  targetp=(targetp*BATTLE->edgec)>>2;
  if ((targetp<0)||(targetp>=BATTLE->edgec)) return; // oops
  const struct edge *target=BATTLE->edgev+targetp;
  double tx=target->ix*(1.0-player->cpuoutness)+target->ox*player->cpuoutness;
  double ty=target->iy*(1.0-player->cpuoutness)+target->oy*player->cpuoutness;
  
  double t=atan2(tx-player->x,player->y-ty);
  double dt=t-player->t;
  if (dt<-M_PI) dt+=M_PI*2.0;
  else if (dt>M_PI) dt-=M_PI*2.0;
  if (dt<-0.100) player->indt=-1;
  else if (dt>0.100) player->indt=1;
  else player->indt=0;
}

/* Check and correct wall collision.
 */
 
static inline void player_check_walls(struct battle *battle,struct player *player,const struct edge *ea,const struct edge *eb) {
  double proj,rej;
  // Outer wall.
  proj=((player->x-ea->ox)*(eb->ox-ea->ox)+(player->y-ea->oy)*(eb->oy-ea->oy))/ea->olen;
  if ((proj>=0.0)&&(proj<=ea->olen)) {
    rej=((player->x-ea->ox)*(eb->oy-ea->oy)-(player->y-ea->oy)*(eb->ox-ea->ox))/ea->olen;
    if (rej>-player->radius) {
      proj/=ea->olen;
      double cx=ea->ox*(1.0-proj)+eb->ox*proj;
      double cy=ea->oy*(1.0-proj)+eb->oy*proj;
      double nx=(eb->ox-ea->ox)/ea->olen;
      double ny=(eb->oy-ea->oy)/ea->olen;
      player->x=cx-ny*(player->radius+0.00);
      player->y=cy+nx*(player->radius+0.00);
    }
  }
  /* Inner wall.
   * Oops, shit, these are a little different from outer walls, because the inner wall on the far end of the interior also thinks it's in play.
   * We're also not accounting for the case where you're outside the scalar projection, but still collide against the line's tip.
   * I don't think that matters. Players that ride the wall might see some unexpected bumps, I'm not worried about it.
   */
  proj=((player->x-ea->ix)*(eb->ix-ea->ix)+(player->y-ea->iy)*(eb->iy-ea->iy))/ea->ilen;
  if ((proj>=0.0)&&(proj<=ea->ilen)) {
    rej=((player->x-ea->ix)*(eb->iy-ea->iy)-(player->y-ea->iy)*(eb->ix-ea->ix))/ea->ilen;
    if (rej<-5.0) {
      // Very low rejection, we must be looking at the far interior wall. Ignore it.
      // In real life, we'll collide with small but positive rejections.
    } else if (rej<player->radius) {
      proj/=ea->ilen;
      double cx=ea->ix*(1.0-proj)+eb->ix*proj;
      double cy=ea->iy*(1.0-proj)+eb->iy*proj;
      double nx=(eb->ix-ea->ix)/ea->ilen;
      double ny=(eb->iy-ea->iy)/ea->ilen;
      player->x=cx+ny*(player->radius+0.00);
      player->y=cy-nx*(player->radius+0.00);
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Tick (raceclock). We don't get called when done.
  player->raceclock+=elapsed;

  /* Animate.
   * Players are always moving, so always animate, easy.
   */
  if ((player->animclock-=elapsed)<=0.0) {
    player->animclock+=0.100;
    if (++(player->animframe)>=4) player->animframe=0;
  }

  /* Turn if requested.
   */
  if (player->indt) {
    player->t+=player->indt*player->turnrate*elapsed;
    if (player->t>M_PI) player->t-=M_PI*2.0;
    else if (player->t<-M_PI) player->t+=M_PI*2.0;
  }
  
  /* Move, always.
   */
  player->x+=sin(player->t)*player->speed*elapsed;
  player->y-=cos(player->t)*player->speed*elapsed;
  
  /* Wall collisions.
   * The walls are stored in (edgev), one point per record, wound clockwise.
   * We can exploit the winding, very helpful, we know which side of each line we need to be on.
   * Checking every sprite against every segment every frame. Is that too much?
   */
  const struct edge *ea=BATTLE->edgev;
  const struct edge *eb=ea+1;
  int i=BATTLE->edgec-1;
  for (;i-->0;ea++,eb++) {
    player_check_walls(battle,player,ea,eb);
  }
  player_check_walls(battle,player,BATTLE->edgev+BATTLE->edgec-1,BATTLE->edgev);
  
  // Player-on-player collisions get handled outside.
  
  /* Check my quadrant.
   */
  int q0=player->q;
  switch (player->q&3) {
    case 0: { // SE
        if (player->x<FBW*0.5) player->q++;
        else if (player->y<FBH*0.5) player->q--;
      } break;
    case 1: { // SW
        if (player->x>FBW*0.5) player->q--;
        else if (player->y<FBH*0.5) player->q++;
      } break;
    case 2: { // NW
        if (player->x>FBW*0.5) player->q++;
        else if (player->y>FBH*0.5) player->q--;
      } break;
    case 3: { // NE
        if (player->x<FBW*0.5) player->q--;
        else if (player->y>FBH*0.5) player->q++;
      } break;
  }
  if (player->q!=q0) {
    if ((player->q>=5)&&!player->midway) {
      player->midway=1;
      bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
    } else if ((player->q<=0)&&player->midway) {
      player->done=1;
      bm_sound_pan(RID_sound_treasure,player->who?PLAYER_PAN:-PLAYER_PAN);
    }
  }
}

/* Update.
 */
 
static void _ratrace_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  // Update players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->done) continue;
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  // Check for player-on-player collisions. A little easier to reason about them from out here, than inside player update.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  double rsum=l->radius+r->radius;
  double rsum2=rsum*rsum;
  double dx=r->x-l->x;
  double dy=r->y-l->y;
  double d2=dx*dx+dy*dy;
  if (d2<rsum2) {
    double d=sqrt(d2);
    double pen=rsum-d;
    if (pen>0.0) {
      double nx=dx/d; // Unit vector pointing from (l) to (d).
      double ny=dy/d;
      l->x-=pen*0.5*nx;
      l->y-=pen*0.5*ny;
      r->x+=pen*0.5*nx;
      r->y+=pen*0.5*ny;
    }
  }
  
  /* Race is over when both players are done.
   * Or if just one is done, count down the TTL, and terminate on expiry.
   */
  if (l->done&&r->done) {
    if (l->raceclock<r->raceclock) battle->outcome=1;
    else if (l->raceclock>r->raceclock) battle->outcome=-1;
    else battle->outcome=0;
  } else if (l->done||r->done) {
    if ((BATTLE->ttl-=elapsed)<=0.0) {
      if (l->done) battle->outcome=1;
      else if (r->done) battle->outcome=-1;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  uint8_t rot=(int8_t)((player->t*128.0)/M_PI);
  int dstx=(int)player->x;
  int dsty=(int)player->y;
  uint8_t tileid=player->tileid;
  switch (player->animframe) {
    case 1: tileid+=1; break;
    case 3: tileid+=2; break;
  }
  graf_fancy(&g.graf,dstx,dsty,tileid,0,rot,NS_sys_tilesize,0,player->color);
}

/* Render.
 */
 
static void _ratrace_render(struct battle *battle) {

  /* Background.
   */
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  uint32_t edgecolor=battle->ctab[BATTLE_COLOR_GROUND_TEXT];
  struct edge *edge=BATTLE->edgev;
  int i=BATTLE->edgec;
  graf_line_strip_begin(&g.graf,(int)BATTLE->edgev[BATTLE->edgec-1].ox,(int)BATTLE->edgev[BATTLE->edgec-1].oy,edgecolor);
  for (;i-->0;edge++) {
    graf_line_strip_more(&g.graf,(int)edge->ox,(int)edge->oy,edgecolor);
  }
  graf_line_strip_begin(&g.graf,(int)BATTLE->edgev[BATTLE->edgec-1].ix,(int)BATTLE->edgev[BATTLE->edgec-1].iy,edgecolor);
  for (edge=BATTLE->edgev,i=BATTLE->edgec;i-->0;edge++) {
    graf_line_strip_more(&g.graf,(int)edge->ix,(int)edge->iy,edgecolor);
  }
  graf_line(&g.graf,
    (int)BATTLE->edgev[0].ix,(int)BATTLE->edgev[0].iy,edgecolor,
    (int)BATTLE->edgev[0].ox,(int)BATTLE->edgev[0].oy,edgecolor
  );
  
  /* Players.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_set_image(&g.graf,RID_image_battle_underground);
  player_render(battle,l);
  player_render(battle,r);
  
  /* Scoreboard.
   */
  int ldx=(FBW>>1)-6;
  int lfx=ldx-10;
  int ltx=ldx-6;
  int rfx=(FBW>>1)+6;
  int rdx=rfx+10;
  int rtx=rdx-6;
  int topy=(FBH>>1)-3;
  int btmy=topy+9;
  graf_fancy(&g.graf,ldx,topy,l->done?0x61:0x26,l->midway?EGG_XFORM_XREV:0,0,NS_sys_tilesize,0,l->midway?0xff0000ff:0x00ff00ff);
  graf_fancy(&g.graf,rdx,topy,r->done?0x61:0x26,r->midway?EGG_XFORM_XREV:0,0,NS_sys_tilesize,0,r->midway?0xff0000ff:0x00ff00ff);
  graf_fancy(&g.graf,lfx,topy,l->tileid+3,0,0,NS_sys_tilesize,0,0x808080ff);
  graf_fancy(&g.graf,rfx,topy,r->tileid+3,EGG_XFORM_XREV,0,NS_sys_tilesize,0,0x808080ff);
  graf_fancy(&g.graf,ltx,btmy,0x60,0,0,NS_sys_tilesize,0,0x808080ff);
  graf_fancy(&g.graf,rtx,btmy,0x60,0,0,NS_sys_tilesize,0,0x808080ff);
  
  /* Times on the scoreboard.
   */
  graf_set_image(&g.graf,RID_image_cave_sprites);
  int lds=(int)(l->raceclock*10.0);
  if (lds<0) lds=0; else if (lds>999) lds=999;
  graf_tile(&g.graf,ltx-1,btmy,0x51+lds/100,0);
  graf_tile(&g.graf,ltx+3,btmy,0x51+(lds/10)%10,0);
  graf_tile(&g.graf,ltx+9,btmy,0x51+lds%10,0);
  int rds=(int)(r->raceclock*10.0);
  if (rds<0) rds=0; else if (rds>999) rds=999;
  graf_tile(&g.graf,rtx-1,btmy,0x51+rds/100,0);
  graf_tile(&g.graf,rtx+3,btmy,0x51+(rds/10)%10,0);
  graf_tile(&g.graf,rtx+9,btmy,0x51+rds%10,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_ratrace={
  .name="ratrace",
  .objlen=sizeof(struct battle_ratrace),
  .id=NS_battle_ratrace,
  .strix_name=314,
  .no_article=0,
  .no_contest=1,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_horz,
  .imageid_default=RID_image_caves,
  .del=_ratrace_del,
  .init=_ratrace_init,
  .update=_ratrace_update,
  .render=_ratrace_render,
};

/* battle_ceilingtouching.c
 */

#include "game/bellacopia.h"

#define CEILINGY 56
#define GROUNDY 120
#define TOUCH_LIMIT 64 /* In theory, there could be up to FBW touches. That would be fantastically difficult to arrange, I'm not worried about it. */

struct battle_ceilingtouching {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t leg_tileid; // Dot and princess share their bottom 4 tiles.
    uint8_t xform;
    double x; // Center, two meters wide. (y) is fixed.
    int indx,intouch;
    int pvintouch;
    double touchclock;
    double touchtime; // constish
    double animclock;
    int animframe;
    double walkspeed;
    int radius; // Half the width of each touch, in pixels.
    int score; // Updates instantly on a touch.
    int dispscore; // Slides to (score).
    int blackout;
    int cpuready; // -1,0,1 = go left, not ready, go right
    double cpux;
    double fuzzlo,fuzzhi; // CPU players are deliberately imperfect.
  } playerv[2];
  
  /* Indexed by horizontal pixels.
   * Value is (0,1,2) = (none,left,right).
   */
  uint8_t ownership[FBW];
  
  double dispscoreclock;
  double playclock;
};

#define BATTLE ((struct battle_ceilingtouching*)battle)

/* Delete.
 */
 
static void _ceilingtouching_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=30.0;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  
  player->touchtime=0.500*(1.0-player->skill)+0.300*player->skill;
  player->walkspeed=35.0*(1.0-player->skill)+55.0*player->skill;
  player->radius=20;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->walkspeed*=0.800;
    player->touchtime*=1.125;
    player->fuzzlo= 5.000*(1.0-player->skill)+0.000*player->skill;
    player->fuzzhi=18.000*(1.0-player->skill)+4.000*player->skill;
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0xb5ad75ff;
        player->tileid=0xc2;
        player->leg_tileid=0xe2;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x62;
        player->leg_tileid=0x82;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0xa2;
        player->leg_tileid=0x82;
      } break;
  }
}

/* New.
 */
 
static int _ceilingtouching_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playclock=15.0;
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
  if (player->blackout) {
    if (!(input&(EGG_BTN_UP|EGG_BTN_SOUTH))) player->blackout=0;
  } else {
    if (input&(EGG_BTN_UP|EGG_BTN_SOUTH)) player->intouch=1; // Was just going to be UP, but might as well do SOUTH too, since that's the general Action button.
    else player->intouch=0;
  }
}

/* List all ranges of ceiling which are not already touched by (who).
 * Never returns >dsta.
 * Can return 0, if this player has touched the entire ceiling.
 */

struct range {
  int x,w;
};
 
static int ceilingtouching_list_open_space(struct range *dst,int dsta,struct battle *battle,int who) {
  who+=1;
  int dstc=0;
  int x=0;
  while ((x<FBW)&&(dstc<dsta)) {
    #if 0 /* XXX Infinite Repainting */
    if (BATTLE->ownership[x]==who) {
    #else /* XXX Paint Once */
    if (BATTLE->ownership[x]) {
    #endif
      x++;
      continue;
    }
    dst[dstc].x=x++;
    dst[dstc].w=1;
    #if 0 /* XXX Infinite Repainting */
    while ((x<FBW)&&(BATTLE->ownership[x]!=who)) { x++; dst[dstc].w++; }
    #else /* XXX Paint Once */
    while ((x<FBW)&&!BATTLE->ownership[x]) { x++; dst[dstc].w++; }
    #endif
    dstc++;
  }
  return dstc;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // If my arm is up, drop input and the rest of the state.
  if (player->touchclock>0.0) {
    player->indx=0;
    player->intouch=0;
    player->cpuready=0;
    return;
  }
  
  // If (cpuready), approach (cpux) and when we reach it, touch.
  if (player->cpuready) {
    player->indx=player->cpuready;
    if (
      ((player->cpuready<0)&&(player->x<=player->cpux))||
      ((player->cpuready>0)&&(player->x>=player->cpux))
    ) {
      player->indx=0;
      player->intouch=1;
      player->cpuready=0;
    }
    return;
  }
  
  /* Choose our next touch position.
   */
  struct range openv[16];
  int openc=ceilingtouching_list_open_space(openv,16,battle,player->who);
  if (openc<1) { // I have covered the whole ceiling. Great! Walk back and forth.
    if (player->x<FBW*0.5) {
      player->cpux=FBW-10.0;
    } else {
      player->cpux=10.0;
    }
  } else {
    /* Select the nearest open range of at least my radius.
     * If there isn't one, take the nearest of any size.
     * Also, if the ceiling directly overhead is open, that wins.
     */
    int minw=player->radius;
    int x=(int)player->x;
    struct range *best=openv;
    int bestdistance=INT_MAX;
    struct range *touch=openv;
    int i=openc;
    for (;i-->0;touch++) {
      int distance;
      if ((touch->x<=x)&&(touch->x+touch->w>x)) { // Directly overhead. Use this one, no need to check the others.
        best=touch;
        break;
      }
      if (touch->x>x) distance=touch->x-x;
      else distance=x-(touch->x+touch->w);
      if (distance<bestdistance) { // It's closer.
        if ((touch->w<minw)&&(best->w>=minw)) {
          // ...but the other one is wide enough and we're not. Skip this one.
        } else {
          best=touch;
          bestdistance=distance;
        }
      }
    }
    /* Now choose a position within (best).
     * The ideal is to cross its near edge by (player->radius) pixels.
     */
    if (best->x>=x) {
      player->cpux=best->x+player->radius;
    } else if (best->x+best->w<=x) {
      player->cpux=best->x+best->w-player->radius;
    } else {
      player->cpux=player->x;
    }
    /* Then make a mistake.
     */
    double off=(rand()&0xffff)/65535.0;
    off=(1.0-off)*player->fuzzlo+off*player->fuzzhi;
    if (rand()&1) player->cpux+=off;
    else player->cpux-=off;
    /* And don't let it be offscreen!
     */
    const double edge=10.0;
    if (player->cpux<edge) player->cpux=edge;
    else if (player->cpux>FBW-edge) player->cpux=FBW-edge;
  }
  if (player->cpux<player->x) player->cpuready=-1;
  else player->cpuready=1;
}

/* Start a touch.
 */
 
static void player_touch(struct battle *battle,struct player *player) {

  /* Player shows their touch face and becomes inert for a fixed interval, regardless of all else.
   */
  player->touchclock=player->touchtime;
  player->animclock=0.0;
  player->animframe=0;
  
  /* Determine the affected area.
   * Clamp to framebuffer.
   */
  int x=(int)player->x-player->radius;
  int w=player->radius<<1;
  if (x<0) {
    w+=x;
    x=0;
  } else if (x>FBW-w) {
    w=FBW-x;
  }
  
  #if 0 /* XXX: Infinite repainting. This was my first idea. Not sure I like it. */
  /* If that range is empty or all owned by me, sound a rejection and abort.
   */
  int me=player->who+1;
  int allme=1;
  int i=w; while (i-->0) {
    if (BATTLE->ownership[x+i]!=me) {
      allme=0;
      break;
    }
  }
  if (allme) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  
  /* Make it mine.
   */
  bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
  memset(BATTLE->ownership+x,me,w);
  #else /* XXX: New approach: Each cell can only get touched once, first toucher wins. */
  /* If there are no zeroes in this range, or the range is empty, reject.
   */
  int alltouched=1;
  int i=w; while (i-->0) {
    if (!BATTLE->ownership[x+i]) {
      alltouched=0;
      break;
    }
  }
  if (alltouched) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  
  /* Overwrite zeroes in range with my value.
   */
  bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
  uint8_t *dst=BATTLE->ownership+x;
  for (i=w;i-->0;dst++) {
    if (!*dst) *dst=player->who+1;
  }
  #endif
  
  /* And finally recalculate scores from scratch.
   * Score is just the sum of touch widths.
   */
  BATTLE->playerv[0].score=0;
  BATTLE->playerv[1].score=0;
  const uint8_t *p=BATTLE->ownership;
  for (i=FBW;i-->0;p++) switch (*p) {
    case 1: BATTLE->playerv[0].score++; break;
    case 2: BATTLE->playerv[1].score++; break;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Pay down touchclock or initiate a new touch.
   * If touching, nothing else happens.
   */
  if (player->touchclock>0.0) {
    player->touchclock-=elapsed;
    player->pvintouch=player->intouch;
    return;
  } else if (player->intouch&&!player->pvintouch) {
    player->touchclock=player->touchtime;
    player_touch(battle,player);
    player->pvintouch=player->intouch;
    return;
  }
  player->pvintouch=player->intouch;
  
  /* Walking?
   */
  if (player->indx) {
    if (player->indx<0) player->xform=EGG_XFORM_XREV;
    else player->xform=0;
    player->x+=player->walkspeed*player->indx*elapsed;
    if (player->x<0.0) player->x=0.0;
    else if (player->x>FBW) player->x=FBW;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.200;
      if (++(player->animframe)>=4) player->animframe=0;
    }
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
}

/* Update.
 */
 
static void _ceilingtouching_update(struct battle *battle,double elapsed) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  if (battle->outcome==-2) {
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
      else player_update_cpu(battle,player,elapsed);
      player_update_common(battle,player,elapsed);
    }
  } else {
    l->touchclock=r->touchclock=0.0;
    l->animframe=r->animframe=0;
  }
  
  if ((l->score!=l->dispscore)||(r->score!=r->dispscore)) {
    BATTLE->dispscoreclock-=elapsed;
    while (BATTLE->dispscoreclock<0.0) { // Might step this by a very fine increment, less than a frame.
      BATTLE->dispscoreclock+=0.010;
      if (l->dispscore<l->score) l->dispscore++; else if (l->dispscore>l->score) l->dispscore--;
      if (r->dispscore<r->score) r->dispscore++; else if (r->dispscore>r->score) r->dispscore--;
    }
  } else {
    BATTLE->dispscoreclock=0.0;
  }
  
  /* Game ends when time runs out or every pixel of the ceiling is touched.
   */
  if ((battle->outcome==-2)&&(((BATTLE->playclock-=elapsed)<=0.0)||(l->score+r->score>=FBW))) {
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x0=(int)player->x-(NS_sys_tilesize>>1);
  int y0=CEILINGY+(NS_sys_tilesize>>1);
  int x1=x0+NS_sys_tilesize;
  int dtl=0,dtr=1;
  if (player->xform) {
    dtl=1;
    dtr=0;
  }
  int dt=0;
  if (player->touchclock>0.0) dt=6;
  else switch (player->animframe) {
    case 1: dt=2; break;
    case 3: dt=4; break;
  }
  graf_tile(&g.graf,x0,y0,player->tileid+dt+dtl,player->xform);
  graf_tile(&g.graf,x1,y0,player->tileid+dt+dtr,player->xform);
  graf_tile(&g.graf,x0,y0+NS_sys_tilesize,player->tileid+0x10+dt+dtl,player->xform);
  graf_tile(&g.graf,x1,y0+NS_sys_tilesize,player->tileid+0x10+dt+dtr,player->xform);
  graf_tile(&g.graf,x0,y0+NS_sys_tilesize*2,player->leg_tileid+dt+dtl,player->xform);
  graf_tile(&g.graf,x1,y0+NS_sys_tilesize*2,player->leg_tileid+dt+dtr,player->xform);
  graf_tile(&g.graf,x0,y0+NS_sys_tilesize*3,player->leg_tileid+0x10+dt+dtl,player->xform);
  graf_tile(&g.graf,x1,y0+NS_sys_tilesize*3,player->leg_tileid+0x10+dt+dtr,player->xform);
}

/* Render.
 */
 
static void _ceilingtouching_render(struct battle *battle) {

  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,0,FBW,CEILINGY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,CEILINGY,FBW,1,0x000000ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  
  /* Highlight touched regions along the top.
   */
  int x=0;
  const uint8_t *p=BATTLE->ownership;
  while (x<FBW) {
    if (!*p) {
      x++;
      p++;
      continue;
    }
    int owner=*p;
    int w=1;
    p++;
    while ((x+w<FBW)&&(*p==owner)) { w++; p++; }
    graf_fill_rect(&g.graf,x,CEILINGY-5,w,5,BATTLE->playerv[owner-1].color);
    x+=w;
  }
  
  /* Draw score bars below the scene, fast to the edges.
   * The nature of scoring is such that they may fill the available space, but no more than that.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_fill_rect(&g.graf,0,GROUNDY+9,FBW,7,0x000000ff);
  graf_fill_rect(&g.graf,FBW>>1,GROUNDY+9,1,7,0xffffffff);
  if (l->dispscore>0) graf_fill_rect(&g.graf,0,GROUNDY+10,l->dispscore,5,l->color);
  if (r->dispscore>0) graf_fill_rect(&g.graf,FBW-r->dispscore,GROUNDY+10,r->dispscore,5,r->color);
  
  // Players.
  graf_set_image(&g.graf,RID_image_battle_underground);
  player_render(battle,l);
  player_render(battle,r);
  
  // Clock at the top.
  int s=(int)(BATTLE->playclock+0.999);
  if (s>0) {
    graf_set_image(&g.graf,RID_image_fonttiles);
    if (s>=10) {
      if (s>99) s=99;
      graf_tile(&g.graf,(FBW>>1)-4,20,'0'+s/10,0);
      graf_tile(&g.graf,(FBW>>1)+4,20,'0'+s%10,0);
    } else {
      graf_tile(&g.graf,FBW>>1,20,'0'+s,0);
    }
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_ceilingtouching={
  .name="ceilingtouching",
  .objlen=sizeof(struct battle_ceilingtouching),
  .id=NS_battle_ceilingtouching,
  .strix_name=322,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1, // Only the score bars update.
  .input=battle_input_dpad,
  .imageid_default=RID_image_caves,
  .del=_ceilingtouching_del,
  .init=_ceilingtouching_init,
  .update=_ceilingtouching_update,
  .render=_ceilingtouching_render,
};

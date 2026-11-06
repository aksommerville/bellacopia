/* battle_bouncing.c
 */

#include "game/batsup/battle_internal.h"

#define GROUNDY 150
#define ACORN_LIMIT 32

struct battle_bouncing {
  struct battle hdr;
  double upclock; // Counts up, no resetting.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    double x; // Framebuffer pixels; y is constant.
    double speed;
    int indx;
    int animframe;
    double animclock;
    double depressclock;
    int scoregood; // How many acorns reach my edge.
    int scorebad; // How many acorns landed on my floor.
  // For CPU:
    int pvtargetx;
    int error;
    int errorv[10];
    int errorp;
  } playerv[2];
  
  struct acorn {
    double x,y;
    double dx,dy;
    int forecast; // ((x<<16)|ms). Zero if uncomputed.
  } acornv[ACORN_LIMIT];
  int acornc;
  int acorn_pending; // Counts down to zero.
  int nextside; // Toggles 0,1.
  double aclock;
};

#define BATTLE ((struct battle_bouncing*)battle)

/* Delete.
 */
 
static void _bouncing_del(struct battle *battle) {
}

/* Pick a zero error slot at random and nonzero it.
 */
 
static void bouncing_add_error(struct player *player) {
  int candidatev[10];
  int candidatec=0;
  const int *v=player->errorv;
  int i=0;
  for (;i<10;i++,v++) if (!*v) candidatev[candidatec++]=i;
  if (candidatec<1) return;
  int p=rand()%candidatec;
  player->errorv[candidatev[p]]=1;
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=FBW*0.333;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->x=FBW-player->x;
  }
  
  player->speed=80.0*(1.0-player->skill)+150.0*player->skill;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    int errorc=(int)(6.0*(1.0-player->skill)+1.0*player->skill);
    if (errorc<1) errorc=1; else if (errorc>9) errorc=9;
    while (errorc-->0) bouncing_add_error(player);
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x50535aff;
        player->tileid=0xc3;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0xa3;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0xb3;
      } break;
  }
}

/* New.
 */
 
static int _bouncing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->nextside=(rand()&1); // First acorn can come from anywhere.
  BATTLE->aclock=1.000; // A brief delay before the first.
  BATTLE->acorn_pending=20;
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

/* Set (forecast) in one acorn.
 */
 
static void bouncing_forecast(struct battle *battle,struct acorn *acorn) {
  /* Doing it the mathematically correct way is complicated.
   * Instead, we'll find the collision incrementally, stepping up by time intervals coarser than the normal 60 hz.
   * That's a bit easier to follow, and it also introduces some desirable error.
   */
  const double interval=0.100;
  const double bottom=GROUNDY-16.0;
  double t=BATTLE->upclock;
  double y=acorn->y;
  double dy=acorn->dy;
  double x=acorn->x;
  while ((dy<0.0)||(y<bottom)) {
    t+=interval;
    y+=dy*interval;
    x+=acorn->dx*interval;
    dy+=150.0*interval;
    if (dy>200.0) dy=200.0;
  }
  int xi=(int)x;
  if (xi<0) xi=0; else if (xi>FBW) xi=FBW;
  int ms=(int)(t*1000.0);
  if (ms<0) ms=0; else if (ms>0xffff) ms=0xffff;
  acorn->forecast=(xi<<16)|ms;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Require a forecast for each acorn.
   * Note the next one expected to land in my turf.
   * Note that the boundary x values 0 and FBW are always considered OOB.
   */
  int xlo=1,xhi=FBW-1;
  if (player->who) xlo=FBW>>1;
  else xhi=FBW>>1;
  struct acorn *next=0;
  int nextms=0;
  struct acorn *acorn=BATTLE->acornv;
  int i=BATTLE->acornc;
  for (;i-->0;acorn++) {
    if (!acorn->forecast) bouncing_forecast(battle,acorn);
    int ax=acorn->forecast>>16;
    if ((ax<xlo)||(ax>xhi)) continue; // Not my problem.
    int ms=acorn->forecast&0xffff;
    if (!next||(ms<nextms)) {
      next=acorn;
      nextms=ms;
    }
  }
  
  /* When the acorn's forecast changes (either a different acorn, or it bounced),
   * reconsider errors.
   */
  if (next&&(next->forecast!=player->pvtargetx)) {
    player->error=player->errorv[player->errorp++];
    if (player->errorp>=10) player->errorp=0;
    if (player->error) player->error=(rand()&1)?20:-20;
    player->pvtargetx=next->forecast;
  } else if (!next) {
    player->error=0;
  }
  
  /* If we don't have a forecast -- perfectly normal -- return to the center of my turf.
   * Otherwise we're targetting the forecast landing point.
   */
  int targetx;
  if (next) targetx=next->forecast>>16;
  else targetx=(xlo+xhi)>>1;
  targetx+=player->error;
  xlo=targetx-4;
  xhi=targetx+4;
  if (player->x<xlo) player->indx=1;
  else if (player->x>xhi) player->indx=-1;
  else player->indx=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (battle->outcome>-2) {
    player->indx=0;
  }
  if (player->indx) {
    player->x+=player->speed*elapsed*player->indx;
    if (player->x<8.0) player->x=8.0;
    else if (player->x>FBW-8.0) player->x=FBW-8.0;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.125;
      if (++(player->animframe)>=4) player->animframe=0;
    }
    if (player->indx<0) player->xform=EGG_XFORM_XREV;
    else player->xform=0;
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
  if (player->depressclock>0.0) {
    player->depressclock-=elapsed;
  }
}

/* Update acorn.
 * Return <0 to delete.
 */
 
static int acorn_update(struct battle *battle,struct acorn *acorn,double elapsed) {
  acorn->x+=acorn->dx*elapsed;
  acorn->y+=acorn->dy*elapsed;
  acorn->dy+=150.0*elapsed;
  if (acorn->dy>200.0) acorn->dy=200.0;
  if ((acorn->dx<0.0)&&(acorn->x<-8.0)) {
    BATTLE->playerv[0].scoregood++;
    bm_sound_pan(RID_sound_collect,-PLAYER_PAN);
    return -1;
  }
  if ((acorn->dx>0.0)&&(acorn->x>FBW+8.0)) {
    BATTLE->playerv[1].scoregood++;
    bm_sound_pan(RID_sound_collect,PLAYER_PAN);
    return -1;
  }
  if ((acorn->x>0.0)&&(acorn->x<FBW)&&(acorn->y>GROUNDY)) {
    if (acorn->x<FBW*0.5) {
      BATTLE->playerv[0].scorebad++;
      bm_sound_pan(RID_sound_reject,-PLAYER_PAN);
    } else {
      BATTLE->playerv[1].scorebad++;
      bm_sound_pan(RID_sound_reject,PLAYER_PAN);
    }
    return -1;
  }
  if ((acorn->y>GROUNDY-20.0)&&(acorn->dy>0.0)) {
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      double dx=acorn->x-player->x;
      if ((dx>=-10.0)&&(dx<=10.0)) {
        acorn->forecast=0;
        acorn->dy=-acorn->dy;
        bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->depressclock=0.200;
        return 0;
      }
    }
  }
  return 0;
}

/* Spawn the next acorn.
 */
 
static void bouncing_spawn(struct battle *battle) {
  if (BATTLE->acornc>=ACORN_LIMIT) return;
  struct acorn *acorn=BATTLE->acornv+BATTLE->acornc++;
  acorn->dy=-50.0-((rand()&0xffff)*-30.0)/65535.0;
  acorn->dx=50.0+((rand()&0xffff)*60.0)/65535.0;
  if (BATTLE->nextside) {
    acorn->dx=-acorn->dx;
    acorn->x=FBW+8.0;
    BATTLE->nextside=0;
  } else {
    acorn->x=-8.0;
    BATTLE->nextside=1;
  }
  acorn->y=40.0+((rand()&0xffff)*40.0)/65535.0;
  acorn->forecast=0;
  BATTLE->acorn_pending--;
}

/* Update.
 */
 
static void _bouncing_update(struct battle *battle,double elapsed) {
  BATTLE->upclock+=elapsed;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  i=BATTLE->acornc;
  struct acorn *acorn=BATTLE->acornv+i-1;
  for (;i-->0;acorn--) {
    if (acorn_update(battle,acorn,elapsed)<0) {
      BATTLE->acornc--;
      memmove(acorn,acorn+1,sizeof(struct acorn)*(BATTLE->acornc-i));
    }
  }
  
  if (BATTLE->acorn_pending>0) {
    if ((BATTLE->aclock-=elapsed)<=0.0) {
      BATTLE->aclock+=1.000;
      bouncing_spawn(battle);
    }
  }
  
  /* Game ends when the last acorn meets its demise.
   * Ties are perfectly possible.
   */
  if (!BATTLE->acornc&&!BATTLE->acorn_pending) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    int lscore=l->scoregood-l->scorebad*2;
    int rscore=r->scoregood-r->scorebad*2;
    if (lscore>rscore) battle->outcome=1;
    else if (lscore<rscore) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=GROUNDY-(NS_sys_tilesize>>1)+1;
  uint8_t tileid=player->tileid;
  switch (player->animframe) {
    case 1: tileid+=1; break;
    case 3: tileid+=2; break;
  }
  uint8_t springtile=0x76;
  if (player->depressclock>0.0) springtile+=1;
  graf_tile(g_graf,x,y,tileid,player->xform);
  graf_tile(g_graf,x,y-NS_sys_tilesize,springtile,0);
}

/* Render score.
 */
 
static void bouncing_render_score(struct battle *battle,int x,int score) {
  char tmp[4];
  int tmpc=0;
  if (score<0) {
    tmp[tmpc++]='-';
    score=-score;
  }
  if ((score<0)||(score>999)) score=999;
  if (score>=100) tmp[tmpc++]='0'+score/100;
  if (score>=10) tmp[tmpc++]='0'+(score/10)%10;
  tmp[tmpc++]='0'+score%10;
  x-=tmpc*4;
  x+=4;
  int y=GROUNDY-32;
  const char *v=tmp;
  for (;tmpc-->0;v++,x+=8) graf_tile(g_graf,x,y,*v,0);
}

/* Render.
 */
 
static void _bouncing_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,FBW>>1,GROUNDY,1,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND_TEXT]&0xffffff7f);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(g_graf,RID_image_battle_meadow);
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  player_render(battle,l);
  player_render(battle,r);
  
  struct acorn *acorn=BATTLE->acornv;
  int i=BATTLE->acornc;
  for (;i-->0;acorn++) graf_tile(g_graf,(int)acorn->x,(int)acorn->y,0x86,0);
  
  /* Scoreboard.
   */
  int sbgy=FBH-21;
  int sbby=FBH- 8;
  graf_tile(g_graf,FBW>>1,sbgy,0x89,0);
  graf_tile(g_graf,FBW>>1,sbby,0x8a,0);
  int xl=(FBW>>1)-NS_sys_tilesize,xr=(FBW>>1)+NS_sys_tilesize;
  for (i=0;;i++,xl-=11,xr+=11) {
    int done=1;
    if (i<l->scoregood) { graf_tile(g_graf,xl,sbgy,0x88,0); done=0; }
    if (i<l->scorebad)  { graf_tile(g_graf,xl,sbby,0x87,0); done=0; }
    if (i<r->scoregood) { graf_tile(g_graf,xr,sbgy,0x88,0); done=0; }
    if (i<r->scorebad)  { graf_tile(g_graf,xr,sbby,0x87,0); done=0; }
    if (done) break;
  }
  
  /* Final score above each player's head.
   */
  if (battle->outcome>-2) {
    graf_set_image(g_graf,RID_image_fonttiles);
    graf_set_tint(g_graf,battle->ctab[BATTLE_COLOR_SKY_TEXT]);
    bouncing_render_score(battle,l->x,l->scoregood-l->scorebad*2);
    bouncing_render_score(battle,r->x,r->scoregood-r->scorebad*2);
    graf_set_tint(g_graf,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_bouncing={
  .name="bouncing",
  .objlen=sizeof(struct battle_bouncing),
  .id=118,
  .strix_name=396,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_horz,
  .imageid_default=RID_image_meadow,
  .del=_bouncing_del,
  .init=_bouncing_init,
  .update=_bouncing_update,
  .render=_bouncing_render,
};

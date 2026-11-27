/* battle_conducting.c
 */

#include "game/batsup/battle_internal.h"

#define GOALX 315
#define FAIRY_LIMIT 40
#define SMOKE_LIMIT 40
#define CARGO_LIMIT 6

struct battle_conducting {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    int y;
    double x;
    double throttle; // Visible to the user; target velocity.
    double dx; // Actual velocity, volatile.
    double dxmax,uprate,downrate; // Per skill.
    double engineanimclock;
    int engineanimframe;
    int trackc; // Up to NS_sys_mapw.
    uint8_t cargo[CARGO_LIMIT]; // tileid of the cars I'm pulling, entirely decorative.
    int derailed;
  } playerv[2];
  
  struct fairy {
    double x,y;
    double dx,dy;
    double targetx,targety;
    double animclock;
    int animframe;
    int who,trackc; // Which player are we assigning to, and to what are we bumping their (trackc) after placement. (who<0) if we're leaving.
    int state; // 0,1,2 = enter,wait,exit
  } fairyv[FAIRY_LIMIT];
  int fairyc;
  
  struct smoke {
    double x,y;
    double dx,dy;
    double animclock;
    uint8_t tileid;
    uint8_t xform;
  } smokev[SMOKE_LIMIT];
  int smokec;
};

#define BATTLE ((struct battle_conducting*)battle)

/* Delete.
 */
 
static void _conducting_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=20.0;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->y=80;
  } else { // Right.
    player->who=1;
    player->y=140;
  }
  
  player->dxmax=50.0*(1.0-player->skill)+100.0*player->skill;
  player->uprate=20.0*(1.0-player->skill)+100.0*player->skill;
  player->downrate=30.0*(1.0-player->skill)+150.0*player->skill;
  player->trackc=3;
  
  if (player->human=human) { // Human.
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x7a4a28ff;
        player->tileid=0x4c;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x2c;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x3c;
      } break;
  }
  
  // Decorative cargo.
  int i=0;
  for (;i<CARGO_LIMIT;i++) player->cargo[i]=0xaf+0x10*(rand()%6);
}

/* Add a fairy.
 */
 
static int conducting_add_fairy(struct battle *battle,struct player *player,int trackc,double when) {
  if (BATTLE->fairyc>=FAIRY_LIMIT) return -1;
  if (player->trackc>=NS_sys_mapw) return -1;
  struct fairy *fairy=BATTLE->fairyv+BATTLE->fairyc++;
  fairy->who=player->who;
  fairy->trackc=trackc;
  fairy->targety=player->y;
  fairy->targetx=trackc*NS_sys_tilesize-NS_sys_tilesize*0.5;
  fairy->animclock=((rand()&0xffff)*0.150)/65535.0; // Don't animate them all in sync.
  
  /* Now the interesting part: Choose a speed and distance such that the track gets built in order.
   * If we mess it up, it's not the end of the world. Fairies will wait at their target until the track is ready.
   */
  double speed=50.0+((rand()&0xffff)*50.0)/65535.0;
  double distance=speed*when;
  double t=((rand()&0xffff)*M_PI*2.0)/65535.0;
  double nx=sin(t);
  double ny=cos(t);
  fairy->x=fairy->targetx+nx*distance;
  fairy->y=fairy->targety+ny*distance;
  fairy->dx=-speed*nx;
  fairy->dy=-speed*ny;
  
  return 0;
}

/* New.
 */
 
static int _conducting_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  
  /* Generate all the fairies here at init.
   * They'll arrive in order; we'll arrange that by starting them ever further offscreen.
   * Interleave the two players' fairies so we can keep the timing roughly equal.
   */
  int ltp=l->trackc+1,rtp=r->trackc+1;
  double lwhen=1.0,rwhen=1.0;
  for (;;) {
    if (ltp>NS_sys_mapw) {
      if (rtp>NS_sys_mapw) break; // Both finished.
      if (conducting_add_fairy(battle,r,rtp++,rwhen)<0) return -1;
      rwhen+=0.250+((rand()&0xffff)*0.500)/65535.0;
    } else if (rtp>=NS_sys_mapw) {
      if (conducting_add_fairy(battle,l,ltp++,lwhen)<0) return -1;
      lwhen+=0.250+((rand()&0xffff)*0.500)/65535.0;
    } else { // Grow both tracks.
      if (conducting_add_fairy(battle,l,ltp++,lwhen)<0) return -1;
      if (conducting_add_fairy(battle,r,rtp++,rwhen)<0) return -1;
      // Choose two intervals and add the longer choice to the shorter current time.
      double wa=0.250+((rand()&0xffff)*0.500)/65535.0;
      double wb=0.250+((rand()&0xffff)*0.500)/65535.0;
      if (wa>wb) {
        double tmp=wa;
        wa=wb;
        wb=tmp;
      }
      if (lwhen<rwhen) {
        lwhen+=wb;
        rwhen+=wa;
      } else {
        lwhen+=wa;
        rwhen+=wb;
      }
    }
  }
   
  return 0;
}

/* Adjust throttle. Controllers call directly.
 */
 
static void conducting_throttle_up(struct battle *battle,struct player *player,double elapsed) {
  player->throttle+=0.500*elapsed;
  if (player->throttle>1.0) player->throttle=1.0;
}

static void conducting_throttle_down(struct battle *battle,struct player *player,double elapsed) {
  player->throttle-=0.500*elapsed;
  if (player->throttle<0.0) player->throttle=0.0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: conducting_throttle_up(battle,player,elapsed); break;
    case EGG_BTN_DOWN: conducting_throttle_down(battle,player,elapsed); break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // If my track is complete, floor it.
  if (player->trackc>=NS_sys_mapw) {
    conducting_throttle_up(battle,player,elapsed);
    return;
  }
  
  /* Check distance to end of track, and throttle accordingly.
   * I think we can do this without the actual math, and have it come out agreeable.
   */
  double tracklen=(double)(player->trackc*NS_sys_tilesize);
  double remaining=tracklen-player->x;
  
  // Below a certain distance, the target must be zero.
  if (remaining<20.0) {
    conducting_throttle_down(battle,player,elapsed);
    return;
  }
  
  /* For intermediate distance, choose an appropriate throttle level to aim for.
   */
  double target=0.100+(0.900*remaining)/200.0;
  double dthrottle=target-player->throttle;
  if (dthrottle<-0.050) conducting_throttle_down(battle,player,elapsed);
  else if (dthrottle>0.050) conducting_throttle_up(battle,player,elapsed);
}

/* Generate smoke from the engine.
 */
 
static void conducting_belch_smoke(struct battle *battle,struct player *player) {
  if (BATTLE->smokec>=SMOKE_LIMIT) return;
  struct smoke *smoke=BATTLE->smokev+BATTLE->smokec++;
  smoke->x=player->x+2.0;
  smoke->y=player->y-8.0;
  smoke->tileid=0x3d;
  smoke->xform=rand()&7;
  smoke->animclock=0.200;
  smoke->dx=((rand()&0xffff)*10.0)/65535.0-5.0;
  smoke->dy=-15.0-((rand()&0xffff)*10.0)/65535.0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Animate engine.
   */
  double earate=elapsed*player->throttle;
  if ((player->engineanimclock-=earate)<=0.0) {
    player->engineanimclock+=0.200;
    if (++(player->engineanimframe)>=2) {
      player->engineanimframe=0;
      conducting_belch_smoke(battle,player);
    }
  }
  
  /* If derailed, stop moving. The engine does continue smoking.
   */
  if (player->derailed) return;
  
  /* Slide (dx) toward a target determined by (throttle) and (dxmax).
   */
  double target=player->throttle*player->dxmax;
  if (target<player->dx) {
    if ((player->dx-=player->downrate*elapsed)<=target) player->dx=target;
  } else if (target>player->dx) {
    if ((player->dx+=player->uprate*elapsed)>=target) player->dx=target;
  }
  
  /* Move.
   */
  if (player->dx>0.0) {
    player->x+=player->dx*elapsed;
    // Derail if the center of the engine tile passes into a column with no track.
    if (player->trackc<NS_sys_mapw) {
      int etp=((int)player->x-9)/NS_sys_tilesize;
      if (etp>=player->trackc) {
        player->derailed=1;
      }
    }
  }
}

/* Update fairy.
 * Return <0 if delivered and offscreen, ie we can kill the fairy.
 */
 
static int conducting_update_fairy(struct battle *battle,struct fairy *fairy,double elapsed) {
  // Always animate.
  if ((fairy->animclock-=elapsed)<=0.0) {
    fairy->animclock+=0.150;
    if (++(fairy->animframe)>=2) fairy->animframe=0;
  }
  // If my player derailed, vanish the track and go home. Stop doing things.
  if (BATTLE->playerv[fairy->who].derailed) fairy->state=2;
  // And proceed with per-state update:
  switch (fairy->state) {
    case 0: { // Proceed toward our target.
        fairy->x+=fairy->dx*elapsed;
        fairy->y+=fairy->dy*elapsed;
        int ready=1;
        if ((fairy->dx>=0.0)&&(fairy->x>=fairy->targetx)) fairy->x=fairy->targetx;
        else if ((fairy->dx<=0.0)&&(fairy->x<=fairy->targetx)) fairy->x=fairy->targetx;
        else ready=0;
        if ((fairy->dy>=0.0)&&(fairy->y>=fairy->targety)) fairy->y=fairy->targety;
        else if ((fairy->dy<=0.0)&&(fairy->y<=fairy->targety)) fairy->y=fairy->targety;
        else ready=0;
        if (ready) {
          fairy->state=1;
        }
      } break;
    case 1: { // Wait for player's track to become ready. If she derailed, go home.
        struct player *player=BATTLE->playerv+fairy->who;
        if (player->derailed) {
          fairy->state=2;
        } else if (player->trackc==fairy->trackc-1) {
          player->trackc++;
          fairy->state=2;
        }
      } break;
    case 2: { // Leave.
        fairy->x-=fairy->dx*elapsed;
        fairy->y-=fairy->dy*elapsed;
        const double margin=10.0;
        if ((fairy->x<-margin)||(fairy->y<-margin)||(fairy->x>FBW+margin)||(fairy->y>FBH+margin)) return -1;
      } break;
  }
  return 0;
}

/* Render comparators.
 */
 
static int conducting_cmp_smoke(const void *a,const void *b) {
  const struct smoke *A=a,*B=b;
  if (A->y<B->y) return -1;
  if (A->y>B->y) return 1;
  return 0;
}

static int conducting_cmp_fairy(const void *a,const void *b) {
  const struct fairy *A=a,*B=b;
  if (A->y<B->y) return -1;
  if (A->y>B->y) return 1;
  return 0;
}

/* Update.
 */
 
static void _conducting_update(struct battle *battle,double elapsed) {

  /* Animate smoke and remove when expired. Decorative.
   */
  if (BATTLE->smokec) {
    struct smoke *smoke=BATTLE->smokev+BATTLE->smokec-1;
    int i=BATTLE->smokec;
    for (;i-->0;smoke--) {
      smoke->x+=smoke->dx*elapsed;
      smoke->y+=smoke->dy*elapsed;
      if ((smoke->animclock-=elapsed)<=0.0) {
        if (smoke->tileid==0x4f) {
          BATTLE->smokec--;
          memmove(smoke,smoke+1,sizeof(struct smoke)*(BATTLE->smokec-i));
        } else {
          smoke->animclock+=0.200;
          smoke->tileid++;
          if (smoke->tileid==0x40) smoke->tileid=0x4d;
        }
      }
    }
    qsort(BATTLE->smokev,BATTLE->smokec,sizeof(struct smoke),conducting_cmp_smoke);
  }
  
  /* Update fairies and remove when defunct.
   */
  if (BATTLE->fairyc) {
    struct fairy *fairy=BATTLE->fairyv+BATTLE->fairyc-1;
    int i=BATTLE->fairyc;
    for (;i-->0;fairy--) {
      if (conducting_update_fairy(battle,fairy,elapsed)<0) {
        BATTLE->fairyc--;
        memmove(fairy,fairy+1,sizeof(struct fairy)*(BATTLE->fairyc-i));
      }
    }
    qsort(BATTLE->fairyv,BATTLE->fairyc,sizeof(struct fairy),conducting_cmp_fairy);
  }
  
  /* Update players.
   * Controllers only run before finish, but animation and such continue.
   */
  {
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (battle->outcome==-2) {
        if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
        else player_update_cpu(battle,player,elapsed);
      }
      player_update_common(battle,player,elapsed);
    }
  }
  
  /* Battle ends when one player reaches GOALX or derails.
   * A derailed player loses, but if they're both derailed, consider (x) as in the goal case.
   * Tho now that I think about it, both players derailing on the same frame at different positions? Sounds unlikely.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if ((l->x>=GOALX)||(r->x>=GOALX)||l->derailed||r->derailed) {
      if (l->derailed&&!r->derailed) battle->outcome=-1;
      else if (!l->derailed&&r->derailed) battle->outcome=1;
      else if (l->x>r->x) battle->outcome=1;
      else if (l->x<r->x) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render fixed tracks for one player.
 */
 
static void conducting_render_tracks(struct battle *battle,struct player *player) {
  int x=NS_sys_tilesize>>1,y=player->y+7;
  int i=player->trackc;
  for (;i-->0;x+=NS_sys_tilesize) graf_tile(g_graf,x,y,0x2f,0);
}

/* Render all fairies. Shadows separate from bodies, because they're on a different layer.
 * Smoke from the engines counts as a fairy.
 */
 
static void conducting_render_shadows(struct battle *battle) {
  struct fairy *fairy=BATTLE->fairyv;
  int i=BATTLE->fairyc;
  for (;i-->0;fairy++) {
    graf_tile(g_graf,(int)fairy->x,(int)fairy->y+7,0xde,0);
  }
}

static void conducting_render_fairies(struct battle *battle) {
  // Interleave smokes and fairies; they render back to front. The individual lists remain sorted.
  struct smoke *smoke=BATTLE->smokev;
  struct fairy *fairy=BATTLE->fairyv;
  int smokep=0,fairyp=0;
  for (;;) {
    int smokey=999,fairyy=999;
    if (smokep<BATTLE->smokec) smokey=(int)smoke->y;
    if (fairyp<BATTLE->fairyc) fairyy=(int)fairy->y;
    if (fairyy<=smokey) {
      if (fairyy>=FBH) break; // Both lists exhausted.
      uint8_t xform=0;
      if ((fairy->state<2)&&(fairy->dx<0.0)) xform=EGG_XFORM_XREV;
      else if ((fairy->state==2)&&(fairy->dx>0.0)) xform=EGG_XFORM_XREV;
      graf_tile(g_graf,(int)fairy->x,fairyy,0x2d+fairy->animframe,xform);
      if (fairy->state<2) graf_tile(g_graf,(int)fairy->x,fairyy+9,0x2f,0);
      fairyp++;
      fairy++;
    } else {
      graf_tile(g_graf,(int)smoke->x,smokey,smoke->tileid,smoke->xform);
      smokep++;
      smoke++;
    }
  }
}

/* Render the player's train and throttle.
 */
 
static void conducting_render_train(struct battle *battle,struct player *player) {

  // Engine.
  int x=(int)player->x;
  int y=player->y;
  int ey=y+(player->derailed?2:0);
  uint8_t tengine=0x1e +player->engineanimframe;
  uint8_t trider=player->tileid;
  graf_tile(g_graf,x,ey,trider,0);
  graf_tile(g_graf,x,ey,tengine,0);
  
  // Cargo.
  int i=0;
  int cx=x-NS_sys_tilesize;
  for (;i<CARGO_LIMIT;i++,cx-=NS_sys_tilesize) {
    graf_tile(g_graf,cx,y,player->cargo[i],0);
  }
  
  // Throttle.
  int tx=10;
  graf_tile(g_graf,tx,y-NS_sys_tilesize,0x6f,0);
  graf_tile(g_graf,tx,y                ,0x7f,0);
  graf_tile(g_graf,tx,y+NS_sys_tilesize,0x8f,0);
  int ty0=y+NS_sys_tilesize+5;
  int ty1=y-NS_sys_tilesize-6;
  int ty=ty0+lround(player->throttle*(ty1-ty0));
  graf_tile(g_graf,tx,ty,0x9f,0);
}

/* Render.
 */
 
static void _conducting_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(g_graf,RID_image_battle_desert);
  conducting_render_tracks(battle,l);
  conducting_render_tracks(battle,r);
  
  uint32_t flagcolor=0x808080ff;
  if (battle->outcome==1) flagcolor=l->color;
  else if (battle->outcome==-1) flagcolor=r->color;
  graf_fancy(g_graf,310,((l->y+r->y)>>1)+6,0x5f,0,0,NS_sys_tilesize,0,flagcolor);
  
  conducting_render_shadows(battle);
  conducting_render_train(battle,l);
  conducting_render_train(battle,r);
  conducting_render_fairies(battle);
}

/* Type definition.
 */
 
const struct battle_type battle_type_conducting={
  .name="conducting",
  .objlen=sizeof(struct battle_conducting),
  .id=140,
  .strix_name=444,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_vert,
  .imageid_default=RID_image_desert,
  .del=_conducting_del,
  .init=_conducting_init,
  .update=_conducting_update,
  .render=_conducting_render,
};

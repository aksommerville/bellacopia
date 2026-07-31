/* battle_flying.c
 */

#include "game/bellacopia.h"

#define SCENEW 1000
#define SCENEH 75
#define RING_LIMIT 10
#define GRAVITY_RATE 100.0 /* px/s**2 */
#define GRAVITY_LIMIT 200.0 /* px/s */
#define HURT_TIME 1.000

struct battle_flying {
  struct battle hdr;
  double ttl; // After one player finishes, the other has just a few seconds before we call it.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double x,y; // Scene pixels.
    double dx,dy;
    
    int inflap; // Controller sets.
    
    int blackout;
    int pvinflap;
    double flapclock;
    double hurtclock;
    double runclock; // Counts up, total racing time.
    int done;
    
    // Relevant to report.
    int ringc;
    int rptringc;
    double rptringclock;
    double rptclock;
    
    // Constant configuration.
    double flaptime; // Blackout period after each flap.
    double flapforce; // px/s, positive.
    double flapxforce; // px/s, positive. Applies to forward motion.
    double dxlimit; // px/s
    double xdecel; // px/s**2
    double hurtdy; // px/s, positive.
    double finy; // Scene pixels, where we line up for the final report.
  } playerv[2];
  
  struct ring {
    double x,y; // Scene pixels, center of ring.
    int got[2]; // -1,0,1=missed,pending,got. Indexed by (player->who).
  } ringv[RING_LIMIT];
  int ringc;
};

#define BATTLE ((struct battle_flying*)battle)

/* Delete.
 */
 
static void _flying_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=20.0;
    player->y=SCENEH*0.500;
    player->finy=SCENEH*0.333;
  } else { // Right.
    player->who=1;
    player->x=20.0;
    player->y=SCENEH*0.500;
    player->finy=SCENEH*0.666;
  }
  player->flaptime=0.300*(1.0-player->skill)+0.200*player->skill;
  player->flapforce=100.0*(1.0-player->skill)+70.0*player->skill;
  player->flapxforce=70.0*(1.0-player->skill)+110.0*player->skill;
  player->dxlimit=200.0*(1.0-player->skill)+250.0*player->skill;
  player->xdecel=80.0*(1.0-player->skill)+100.0*player->skill;
  player->hurtdy=50.0*(1.0-player->skill)+40.0*player->skill;
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    // A wee CPU penalty.
    player->flaptime*=1.100;
    player->flapxforce*=0.900;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x704523ff;
        player->tileid=0x2c;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x0c;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x1c;
      } break;
  }
}

/* New.
 */
 
static int _flying_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->ttl=5.0; // Doesn't start ticking until the first player finishes.
  
  /* Rings go at uniform intervals.
   * Vertical positions are random.
   */
  BATTLE->ringc=RING_LIMIT;
  int runway=150; // So many leading pixels don't count.
  double ringspacing=(double)(SCENEW-runway)/BATTLE->ringc;
  double ringx=runway+ringspacing*0.5;
  int i=BATTLE->ringc;
  struct ring *ring=BATTLE->ringv;
  for (;i-->0;ring++,ringx+=ringspacing) {
    ring->x=ringx;
    ring->y=25.0+(rand()%(SCENEH-50));
  }
   
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->inflap=(input&EGG_BTN_SOUTH);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  
  /* If a flap is in progress, chill.
   */
  if (player->inflap||(player->flapclock>0.0)) {
    player->inflap=0;
    return;
  }
  
  /* If we're very low, flap no matter what.
   * Likewise very high, don't flap.
   * Avoid the spikes.
   */
  if (player->y>SCENEH*0.700) {
    player->inflap=1;
    return;
  }
  if (player->y<SCENEH*0.300) {
    return;
  }
  
  /* Find the next ring.
   */
  struct ring *next=0;
  struct ring *ring=BATTLE->ringv;
  int i=BATTLE->ringc;
  for (;i-->0;ring++) {
    if (ring->x<player->x) continue;
    if (!next||(ring->x<next->x)) next=ring;
  }
  
  /* No next ring, or sufficiently far away, flap when we cross some vertical threshold.
   * Verify against flapforce when configuring. Current settings, 0.500 is too high.
   */
  if (!next||(next->x-player->x>=60.0)) {
    if (player->y>SCENEH*0.600) player->inflap=1;
    return;
  }
  
  /* Flap if we're below the ring's vertical midpoint.
   */
  if (player->y>next->y) player->inflap=1;
}

/* Tabulate a player's score.
 * Called once, as they reach the finish line.
 */
 
static void player_collect_score(struct battle *battle,struct player *player) {
  player->ringc=0;
  player->rptringc=0;
  player->rptringclock=0.0;
  player->rptclock=0.0;
  const struct ring *ring=BATTLE->ringv;
  int i=BATTLE->ringc;
  for (;i-->0;ring++) {
    if (ring->got[player->who]>0) player->ringc++;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* When done, slide to the reference position and animate the report.
   */
  if (player->done) {
    player->flapclock=0.0;
    player->hurtclock=0.0;
    const double slidespeed=30.0;
    if (player->y<player->finy) {
      if ((player->y+=slidespeed*elapsed)>=player->finy) player->y=player->finy;
    } else if (player->y>player->finy) {
      if ((player->y-=slidespeed*elapsed)<=player->finy) player->y=player->finy;
    } else {
      if (player->rptringc<player->ringc) {
        if ((player->rptringclock-=elapsed)<=0.0) {
          player->rptringclock+=0.250;
          player->rptringc++;
          bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
        }
      }
      if ((player->rptclock+=elapsed*5.000)>=player->runclock) {
        player->rptclock=player->runclock;
      }
    }
    return;
  }

  /* Advance clocks.
   */
  if (player->flapclock>0.0) player->flapclock-=elapsed;
  if (player->hurtclock>0.0) player->hurtclock-=elapsed;
  player->runclock+=elapsed;

  /* Start a new flap?
   */
  if (player->inflap!=player->pvinflap) {
    if (player->pvinflap=player->inflap) {
      if (player->flapclock<=0.0) {
        bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->flapclock=player->flaptime;
        player->dy-=player->flapforce;
        if ((player->dx+=player->flapxforce)>player->dxlimit) player->dx=player->dxlimit;
      } else {
        bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
      }
    }
  }
  
  /* (dy) increases to (GRAVITY_LIMIT), and (dx) decreases to zero.
   */
  if ((player->dy+=GRAVITY_RATE*elapsed)>=GRAVITY_LIMIT) player->dy=GRAVITY_LIMIT;
  if (player->dx<0.0) { // Can bounce backward maybe? If so, draw it down to zero just like forward motion.
    if ((player->dx+=player->xdecel*elapsed)>=0.0) player->dx=0.0;
  } else {
    if ((player->dx-=player->xdecel*elapsed)<=0.0) player->dx=0.0;
  }
  
  /* Move.
   * Record (x) before the move, so we can check ring crossings.
   */
  double x0=player->x;
  player->x+=player->dx*elapsed;
  player->y+=player->dy*elapsed;
  
  /* If we hit the top or bottom spikes, drop (dx) to zero and react violently on y.
   */
  const double spikelen=10.0;
  if (player->y<spikelen) {
    bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->y=spikelen;
    player->hurtclock=HURT_TIME;
    player->dx=0.0;
    player->dy=player->hurtdy;
  } else if (player->y>SCENEH-spikelen) {
    bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->y=SCENEH-spikelen;
    player->hurtclock=HURT_TIME;
    player->dx=0.0;
    player->dy=-player->hurtdy;
  }
  
  /* Check rings for passing thru, missing, and bonking the edge.
   * Rings are naturally sorted by (x) but we're not exploiting that.
   * Missing a ring has no effect if you previously got it, but getting a ring overrides a prior miss.
   */
  struct ring *ring=BATTLE->ringv;
  int i=BATTLE->ringc;
  for (;i-->0;ring++) {
    if ((x0<ring->x)&&(player->x>=ring->x)) {
      if (player->y<ring->y-18.0) { // miss, above
        if (!ring->got[player->who]) {
          bm_sound_pan(RID_sound_negatory,player->who?PLAYER_PAN:-PLAYER_PAN);
          ring->got[player->who]=-1;
        }
      } else if (player->y<ring->y-12.0) { // bonk, top
        bm_sound_pan(RID_sound_bump,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->dx=-player->dx;
      } else if (player->y<ring->y+12.0) { // get.
        if (ring->got[player->who]<1) {
          bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
          ring->got[player->who]=1;
        }
      } else if (player->y<ring->y+18.0) { // bonk, bottom
        bm_sound_pan(RID_sound_bump,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->dx=-player->dx;
      } else { // miss, below
        if (!ring->got[player->who]) {
          bm_sound_pan(RID_sound_negatory,player->who?PLAYER_PAN:-PLAYER_PAN);
          ring->got[player->who]=-1;
        }
      }
    }
  }
  
  /* Did we cross the finish line?
   */
  if (player->x>=SCENEW) {
    player->done=1;
    player_collect_score(battle,player);
  }
}

/* Update.
 */
 
static void _flying_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* If both players are done, wait for their (rptclock) to reach their (runclock), and (rptringc) to reach (ringc).
   * Or if just one is done, tick down (ttl) and force the laggard to the goal when it expires.
   * Winner is the one with the most rings, with (runclock) breaking ties.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->done&&r->done) {
    if ((l->rptclock>=l->runclock)&&(l->rptringc>=l->ringc)&&(r->rptclock>=r->runclock)&&(r->rptringc>=r->ringc)) {
      if (l->ringc>r->ringc) battle->outcome=1;
      else if (l->ringc<r->ringc) battle->outcome=-1;
      else if (l->runclock<r->runclock) battle->outcome=1;
      else if (l->runclock>r->runclock) battle->outcome=-1;
      else battle->outcome=0;
    }
  } else if (l->done||r->done) {
    if ((BATTLE->ttl-=elapsed)<=0.0) {
      if (!l->done) {
        l->done=1;
        l->x=SCENEW;
        l->y=l->finy;
        player_collect_score(battle,l);
      }
      if (!r->done) {
        r->done=1;
        r->x=SCENEW;
        r->y=r->finy;
        player_collect_score(battle,r);
      }
    }
  }
}

/* Render one player sprite.
 * This happens four times per frame: Each player in each scene.
 */
 
static void player_render_1(struct battle *battle,struct player *player,int scroll,int fully) {
  uint8_t tileid=player->tileid;
  if (player->flapclock>0.0) tileid+=1;
  int dstx=(int)player->x-scroll;
  int dsty=fully+(int)player->y;
  
  int tint=0;
  if (player->hurtclock>0.0) {
    tint=(int)((player->hurtclock*255.0)/HURT_TIME);
    if (tint>0xff) tint=0xff;
    if (tint>0) graf_set_tint(&g.graf,0xff000000|tint);
  }
  
  graf_set_image(&g.graf,RID_image_battle_forest);
  graf_tile(&g.graf,dstx,dsty,tileid,0);
  if (tint>0) graf_set_tint(&g.graf,0);
  
  /* Report to my right, if done.
   * Wait for (rptclock) to go nonzero, there's an initial vertical slide before things start ticking.
   */
  if (player->done&&(player->rptclock>0.0)) {
    // Rings.
    dstx=SCENEW+80-scroll;
    int i=player->rptringc;
    for (;i-->0;dstx+=NS_sys_tilesize) {
      graf_tile(&g.graf,dstx,dsty,0x7c,0);
    }
    // Clock.
    dstx=SCENEW+20-scroll;
    int ms=(int)(player->rptclock*1000.0);
    if (ms<0) ms=0; else if (ms>99999) ms=99999;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_set_tint(&g.graf,battle->ctab[BATTLE_COLOR_SKY_TEXT]);
    if (ms>=10000) graf_tile(&g.graf,dstx,dsty,'0'+ms/10000,0); dstx+=8;
    graf_tile(&g.graf,dstx,dsty,'0'+(ms/1000)%10,0); dstx+=8;
    graf_tile(&g.graf,dstx,dsty,'.',0); dstx+=8;
    graf_tile(&g.graf,dstx,dsty,'0'+(ms/100)%10,0); dstx+=8;
    graf_tile(&g.graf,dstx,dsty,'0'+(ms/10)%10,0); dstx+=8;
    graf_tile(&g.graf,dstx,dsty,'0'+ms%10,0);
    graf_set_tint(&g.graf,0);
  }
}

/* Render one scene.
 */
 
static void player_render(struct battle *battle,struct player *player,int fully) {

  graf_fill_rect(&g.graf,0,fully,FBW,SCENEH,battle->ctab[BATTLE_COLOR_SKY]);
  
  int scroll=(int)player->x-20;
  if (scroll<0) scroll=0; // Players should start at 20, but clamp if I forget and change it.
  
  // Finish line.
  graf_set_image(&g.graf,RID_image_battle_forest);
  int finx=SCENEW-scroll;
  if ((finx>-10)&&(finx<FBW+10)) {
    int finy=fully+(NS_sys_tilesize>>1);
    int ystop=fully+SCENEH;
    for (;finy<ystop;finy+=NS_sys_tilesize) {
      graf_tile(&g.graf,finx,finy,0xbc,EGG_XFORM_SWAP);
    }
  }
  
  // Spikes top and bottom.
  int spikew=NS_sys_tilesize*3;
  int spikex=-(scroll%spikew);
  for (;spikex<FBW;spikex+=spikew) {
    graf_decal_xform(&g.graf,spikex,fully,160,96,spikew,NS_sys_tilesize,EGG_XFORM_YREV);
    graf_decal_xform(&g.graf,spikex,fully+SCENEH-NS_sys_tilesize,160,96,spikew,NS_sys_tilesize,0);
  }
  
  // Back of rings.
  int i=BATTLE->ringc;
  struct ring *ring=BATTLE->ringv;
  for (;i-->0;ring++) {
    int rx=(int)ring->x-scroll;
    if (rx<-20) continue;
    if (rx>FBW+20) continue;
    int ry=fully+(int)ring->y;
    graf_tile(&g.graf,rx,ry-(NS_sys_tilesize>>1),0x6d,0);
    graf_tile(&g.graf,rx,ry+(NS_sys_tilesize>>1),0x7d,0);
  }
  
  // Other guy first, then me.
  struct player *other;
  if (player->who) other=BATTLE->playerv;
  else other=BATTLE->playerv+1;
  player_render_1(battle,other,scroll,fully);
  player_render_1(battle,player,scroll,fully);
  
  // Front of rings.
  graf_set_image(&g.graf,RID_image_battle_forest);
  for (i=BATTLE->ringc,ring=BATTLE->ringv;i-->0;ring++) {
    int rx=(int)ring->x-scroll+6;
    if (rx<-20) continue;
    if (rx>FBW+20) continue;
    int ry=fully+(int)ring->y;
    graf_tile(&g.graf,rx,ry-(NS_sys_tilesize>>1),0x6e,0);
    graf_tile(&g.graf,rx,ry+(NS_sys_tilesize>>1),0x7e,0);
    // If this player has got or missed it, draw a highlight icon dead center vertically.
    if (ring->got[player->who]) {
      uint8_t tileid=(ring->got[player->who]>0)?0x78:0x79;
      graf_tile(&g.graf,rx,fully+(SCENEH>>1),tileid,0);
    }
  }
  
  /* If we're not done and the TTL is close to finished, fade to black.
   */
  if (!player->done&&(BATTLE->ttl<1.0)) {
    int alpha=(int)((1.0-BATTLE->ttl)*255.0);
    if (alpha>0xff) alpha=0xff;
    if (alpha>0) graf_fill_rect(&g.graf,0,fully,FBW,SCENEH,0x00000000|alpha);
  }
}

/* Render.
 */
 
static void _flying_render(struct battle *battle) {

  // Draw both scenes first, then paint over the letterbox regions. Allows player scenes to overflow a little if they need to.
  int topy=(FBH>>2)-(SCENEH>>1);
  int btmy=((FBH*3)>>2)-(SCENEH>>1);
  player_render(battle,BATTLE->playerv+0,topy);
  player_render(battle,BATTLE->playerv+1,btmy);
  graf_fill_rect(&g.graf,0,0,FBW,topy,0x000000ff);
  graf_fill_rect(&g.graf,0,topy+SCENEH,FBW,btmy-SCENEH-topy,0x000000ff);
  graf_fill_rect(&g.graf,0,btmy+SCENEH,FBW,FBH-SCENEH-topy,0x000000ff);
  
  //TODO scoreboard, clock, scroll indicators...
}

/* Type definition.
 */
 
const struct battle_type battle_type_flying={
  .name="flying",
  .objlen=sizeof(struct battle_flying),
  .id=NS_battle_flying,
  .strix_name=312,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_a,
  .imageid_default=0,
  .del=_flying_del,
  .init=_flying_init,
  .update=_flying_update,
  .render=_flying_render,
};

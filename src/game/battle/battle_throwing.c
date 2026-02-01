/* battle_throwing.c
 * Press and release A once against a sliding indicator.
 */

#include "game/bellacopia.h"

#define SKILL_RANGE 0.200 /* A CPU player with zero skill will be off by exactly this much, before clamping. */
#define ANIMATE_TIME 2.000

#define STAGE_PLAY 1
#define STAGE_POSTPLAY 2
#define STAGE_ANIMATE 3
#define STAGE_GLOAT 4
#define STAGE_DONE 5

struct battle_throwing {
  uint8_t handicap;
  uint8_t players;
  void (*cb_end)(int outcome,void *userdata);
  void *userdata;
  int outcome;
  int preoutcome;
  
  struct player {
    int who;
    int human;
    int blackout; // Only relevant for humans.
    uint8_t tileid; // RID_image_battle_goblins: hold,throw,win,lose,ball
    uint8_t tileid_icon;
    uint32_t bar_color;
    double pressp,releasep; // 0..1 or <0 if unset
    double skill; // Per handicap.
    int dstx; // Where the animated ball lands. Winner is (FBW>>1), loser somewhere else. Decorative.
    // For CPU players, established at init:
    double prepressp,prereleasep;
  } playerv[2];
  
  double finger; // 0..1
  double dfinger;
  double ltarget,rtarget; // 0<ltarget<rtarget<1
  
  int stage;
  double stageclock;
};

#define CTX ((struct battle_throwing*)ctx)

/* Delete.
 */
 
static void _throwing_del(void *ctx) {
  free(ctx);
}

/* Initialize player.
 */
 
static void player_init(void *ctx,struct player *player,int human,int appearance) {
  player->human=human;
  player->blackout=1;
  player->pressp=-1.0;
  player->releasep=-1.0;
  switch (appearance) {
    case 0: { // Loblin
        player->tileid_icon=0x68;
        player->tileid=0x80;
        player->bar_color=0x2d823dff;
      } break;
    case 1: { // Dot
        player->tileid_icon=0x66;
        player->tileid=0x70;
        player->bar_color=0x411775ff;
      } break;
    case 2: { // Princess
        player->tileid_icon=0x67;
        player->tileid=0x90;
        player->bar_color=0x0d3ac1ff;
      } break;
  }
  // CPU players decide their exact play in advance.
  if (!human) {
    double fudge=(1.0-player->skill)*SKILL_RANGE; // mmm double fudge
    if (rand()&1) player->prepressp=CTX->ltarget+fudge;
    else player->prepressp=CTX->ltarget-fudge;
    if (rand()&1) player->prereleasep=CTX->rtarget+fudge;
    else player->prereleasep=CTX->rtarget-fudge;
    const double margin=1.0/24.0;
    if (player->prepressp<margin) player->prepressp=margin;
    if (player->prereleasep>1.0-margin) player->prereleasep=1.0-margin;
    if (player->prepressp>player->prereleasep-margin) {
      if (player->prepressp<0.5) player->prereleasep+=margin;
      else player->prepressp-=margin;
    }
  }
}

/* New.
 */
 
static void *_throwing_init(
  uint8_t handicap,
  uint8_t players,
  void (*cb_end)(int outcome,void *userdata),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_throwing));
  if (!ctx) return 0;
  CTX->handicap=handicap;
  CTX->players=players;
  CTX->cb_end=cb_end;
  CTX->userdata=userdata;
  CTX->outcome=-2;
  
  CTX->finger=0.0;
  CTX->dfinger=1.000; // 1.5 seems playable but really fast.
  CTX->stage=STAGE_PLAY;
  
  // (ltarget) goes anywhere in the low half, and (rtarget) at least 1/4 above it.
  int ltargeti=(rand()&0x7fff);
  int rtargeti=ltargeti+0x4000+(rand()%(0xc000-ltargeti));
  CTX->ltarget=(double)ltargeti/65536.0;
  CTX->rtarget=(double)rtargeti/65536.0;
  // And beyond that, keep them at least 1/12 from the edges.
  if (CTX->ltarget<1.0/12.0) CTX->ltarget=1.0/12.0;
  if (CTX->rtarget>11.0/12.0) CTX->rtarget=11.0/12.0;
  
  CTX->playerv[1].skill=(double)CTX->handicap/255.0;
  CTX->playerv[0].skill=1.0-CTX->playerv[1].skill;
  
  CTX->playerv[0].who=0;
  CTX->playerv[1].who=1;
  switch (CTX->players) {
    case NS_players_cpu_cpu: {
        player_init(ctx,CTX->playerv+0,0,2);
        player_init(ctx,CTX->playerv+1,0,0);
      } break;
    case NS_players_cpu_man: {
        player_init(ctx,CTX->playerv+0,0,0);
        player_init(ctx,CTX->playerv+1,2,2);
      } break;
    case NS_players_man_cpu: {
        player_init(ctx,CTX->playerv+0,1,1);
        player_init(ctx,CTX->playerv+1,0,0);
      } break;
    case NS_players_man_man: {
        player_init(ctx,CTX->playerv+0,1,1);
        player_init(ctx,CTX->playerv+1,2,2);
      } break;
    default: _throwing_del(ctx); return 0;
  }
  return ctx;
}

/* Human player.
 */
 
static void player_update_man(void *ctx,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (input&EGG_BTN_SOUTH) return;
    player->blackout=0;
  }
  if (player->pressp<0.0) {
    if (input&EGG_BTN_SOUTH) {
      bm_sound(RID_sound_uimotion);
      player->pressp=CTX->finger;
    }
  } else if (player->releasep<0.0) {
    if (!(input&EGG_BTN_SOUTH)) {
      bm_sound(RID_sound_uiactivate);
      player->releasep=CTX->finger;
    }
  }
}

/* CPU player.
 */
 
static void player_update_cpu(void *ctx,struct player *player,double elapsed) {
  // We'll throw on the second rightward stroke.
  if (player->blackout) {
    if (CTX->dfinger<0.0) player->blackout=0;
  } else if (player->pressp<0.0) {
    if (CTX->dfinger>0.0) {
      if (CTX->finger>=player->prepressp) {
        player->pressp=CTX->finger;
      }
    }
  } else if (player->releasep<0.0) {
    if (CTX->dfinger>0.0) {
      if (CTX->finger>=player->prereleasep) {
        player->releasep=CTX->finger;
      }
    }
  }
}

/* All players.
 */
 
static void player_update_common(void *ctx,struct player *player,double elapsed) {
}

/* Cancel pitches.
 */
 
static void throwing_cancel_pitches(void *ctx) {
  if ((CTX->playerv[0].pressp>=0.0)&&(CTX->playerv[0].releasep<0.0)) {
    CTX->playerv[0].pressp=-1.0;
    CTX->playerv[0].blackout=1;
  }
  if ((CTX->playerv[1].pressp>=0.0)&&(CTX->playerv[1].releasep<0.0)) {
    CTX->playerv[1].pressp=-1.0;
    CTX->playerv[1].blackout=1;
  }
}

/* Select the decorative landing site for each ball.
 */
 
static void throwing_select_landing_sites(void *ctx) {
  if (CTX->preoutcome>0) {
    CTX->playerv[0].dstx=FBW>>1;
    CTX->playerv[1].dstx=(FBW>>1)+NS_sys_tilesize;
  } else if (CTX->preoutcome<0) {
    CTX->playerv[0].dstx=(FBW>>1)-NS_sys_tilesize;
    CTX->playerv[1].dstx=FBW>>1;
  } else {
    CTX->playerv[0].dstx=(FBW>>1)-4;
    CTX->playerv[1].dstx=(FBW>>1)+4;
  }
}

/* Set outcome. Both players' (pressp,releasep) must be set.
 */
 
static void throwing_set_preoutcome(void *ctx) {
  double lscore=0.0,rscore=0.0; // Lower is better.
  #define DIFF(score,a,b) { \
    double d=(a)-(b); \
    if (d<0.0) d=-d; \
    score+=d; \
  }
  double la=CTX->playerv[0].pressp;
  double lz=CTX->playerv[0].releasep;
  double ra=CTX->playerv[1].pressp;
  double rz=CTX->playerv[1].releasep;
  if (la>lz) { double tmp=la; la=lz; lz=tmp; }
  if (ra>rz) { double tmp=ra; ra=rz; rz=tmp; }
  DIFF(lscore,la,CTX->ltarget)
  DIFF(lscore,lz,CTX->rtarget)
  DIFF(rscore,ra,CTX->ltarget)
  DIFF(rscore,rz,CTX->rtarget)
  #undef DIFF
  if (lscore<rscore) {
    CTX->preoutcome=1;
  } else if (lscore>rscore) {
    CTX->preoutcome=-1;
  } else {
    CTX->preoutcome=0;
  }
}

/* Update in play mode.
 */
 
static void throwing_update_play(void *ctx,double elapsed) {

  // Update players.
  struct player *player=CTX->playerv;
  int i=2; for (;i-->0;player++) {
    if (player->human) player_update_man(ctx,player,elapsed,g.input[player->human]);
    else player_update_cpu(ctx,player,elapsed);
    player_update_common(ctx,player,elapsed);
  }
  
  /* If all four control points are established, advance to POSTPLAY.
   */
  if (
    (CTX->playerv[0].pressp>=0.0)&&
    (CTX->playerv[0].releasep>=0.0)&&
    (CTX->playerv[1].pressp>=0.0)&&
    (CTX->playerv[1].releasep>=0.0)
  ) {
    CTX->stage=STAGE_POSTPLAY;
    CTX->stageclock=1.0;
    throwing_set_preoutcome(ctx);
    throwing_select_landing_sites(ctx);
    return;
  }
  
  /* Advance the finger.
   * Finger pingpongs, and if anyone is holding their button at the edges, they're dropped.
   */
  CTX->finger+=CTX->dfinger*elapsed;
  if (CTX->finger>1.0) {
    CTX->finger=1.0;
    if (CTX->dfinger>0.0) {
      CTX->dfinger=-CTX->dfinger;
      throwing_cancel_pitches(ctx);
    }
  } else if (CTX->finger<0.0) {
    CTX->finger=0.0;
    if (CTX->dfinger<0.0) {
      CTX->dfinger=-CTX->dfinger;
      throwing_cancel_pitches(ctx);
    }
  }
}

/* Update.
 */
 
static void _throwing_update(void *ctx,double elapsed) {

  // Done?
  if (CTX->outcome>-2) {
    if (CTX->cb_end) {
      CTX->cb_end(CTX->outcome,CTX->userdata);
      CTX->cb_end=0;
    }
    return;
  }
  
  switch (CTX->stage) {
    case STAGE_PLAY: throwing_update_play(ctx,elapsed); break;
    case STAGE_POSTPLAY: {
        if ((CTX->stageclock-=elapsed)<=0.0) {
          CTX->stage=STAGE_ANIMATE;
          CTX->stageclock=ANIMATE_TIME;
        }
      } break;
    case STAGE_ANIMATE: {
        if ((CTX->stageclock-=elapsed)<=0.0) {
          CTX->stage=STAGE_GLOAT;
          CTX->stageclock=1.0;
        }
      } break;
    case STAGE_GLOAT: {
        if ((CTX->stageclock-=elapsed)<=0.0) {
          CTX->stage=STAGE_DONE;
          CTX->outcome=CTX->preoutcome;
        }
      } break;
  }
}

/* Render one player.
 */
 
static void player_render(void *ctx,struct player *player,int x,int y,uint8_t xform) {
  uint8_t tileid=player->tileid;
  int py=y;
  if (CTX->stage==STAGE_ANIMATE) {
    tileid+=1;
  } else if (CTX->stage>=STAGE_GLOAT) {
    if (player->who) {
      if (CTX->preoutcome<=0) tileid+=2;
      else tileid+=3;
    } else {
      if (CTX->preoutcome>=0) tileid+=2;
      else tileid+=3;
    }
    if (tileid==player->tileid+2) {
      if (g.framec&8) py--;
    }
  }
  graf_tile(&g.graf,x,py,tileid,xform);
  
  int ballx=x;
  if (xform) ballx-=11; else ballx+=11;
  int bally=y+3;
  
  if (CTX->stage==STAGE_ANIMATE) {
    double t=1.0-(CTX->stageclock/ANIMATE_TIME);
    ballx=(int)(ballx*(1.0-t)+player->dstx*t);
    t=(t-0.5)*2.0;
    t*=t;
    t=1.0-t;
    bally=y-(int)(48.0*t);
    
  } else if (CTX->stage>=STAGE_GLOAT) {
    ballx=player->dstx;
    if ((ballx>(FBW>>1)-7)&&(ballx<(FBW>>1)+7)) {
      bally=y+3;
    } else {
      bally=y+5;
    }
  }
  graf_tile(&g.graf,ballx,bally,player->tileid+4,0);
}

/* Render.
 */
 
static void _throwing_render(void *ctx) {
  const int groundy=130;
  graf_fill_rect(&g.graf,0,0,FBW,groundy,0x785830ff);
  graf_fill_rect(&g.graf,0,groundy,FBW,FBH-groundy,0x3c2011ff);
  graf_fill_rect(&g.graf,0,groundy,FBW,1,0x000000ff);
  
  int barw=(FBW*2)/3;
  int barh=20;
  int barx=(FBW>>1)-(barw>>1);
  int bary=40;
  graf_fill_rect(&g.graf,barx,bary,barw,barh,0xffffffff);
  
  struct player *player=CTX->playerv;
  int i=2; for (;i-->0;player++) {
    if ((player->pressp>=0.0)&&(player->releasep>=0.0)) {
      int ax=barx+(int)(player->pressp*barw);
      int zx=barx+(int)(player->releasep*barw);
      if (ax>zx) {
        int tmp=ax;
        ax=zx;
        zx=tmp;
      }
      graf_fill_rect(&g.graf,ax,bary+(barh*(player->who+1))/3,zx-ax,1,player->bar_color);
    }
  }
  
  graf_set_image(&g.graf,RID_image_battle_goblins);
  if (CTX->stage==STAGE_PLAY) {
    int fingerx=(int)(CTX->finger*barw);
    if (fingerx<0) fingerx=0;
    else if (fingerx>=barw) fingerx=barw-1;
    fingerx+=barx;
    graf_tile(&g.graf,fingerx,bary+barh,0x65,0);
  }
  int ltargetx=barx+(int)(CTX->ltarget*barw);
  int rtargetx=barx+(int)(CTX->rtarget*barw);
  graf_tile(&g.graf,ltargetx,bary-(NS_sys_tilesize>>1),0x69,0);
  graf_tile(&g.graf,rtargetx,bary-(NS_sys_tilesize>>1),0x69,0);
  
  for (player=CTX->playerv,i=2;i-->0;player++) {
    if (player->pressp>=0.0) {
      graf_tile(&g.graf,barx+(int)(player->pressp*barw),bary+(barh*(player->who+1))/3,player->tileid_icon,0);
    }
    if (player->releasep>=0.0) {
      graf_tile(&g.graf,barx+(int)(player->releasep*barw),bary+(barh*(player->who+1))/3,player->tileid_icon,0);
    }
  }
  
  player_render(ctx,CTX->playerv+0,FBW/6,groundy-(NS_sys_tilesize>>1),0);
  player_render(ctx,CTX->playerv+1,(FBW*5)/6,groundy-(NS_sys_tilesize>>1),EGG_XFORM_XREV);
  graf_tile(&g.graf,(FBW>>1)+2,groundy-(NS_sys_tilesize>>1),0x6a,0); // x+2 because the brim goes a bit right
}

/* Type definition.
 */
 
const struct battle_type battle_type_throwing={
  .name="throwing",
  .strix_name=49,
  .no_article=0,
  .no_contest=0,
  .supported_players=(1<<NS_players_cpu_cpu)|(1<<NS_players_cpu_man)|(1<<NS_players_man_cpu)|(1<<NS_players_man_man),
  .del=_throwing_del,
  .init=_throwing_init,
  .update=_throwing_update,
  .render=_throwing_render,
};

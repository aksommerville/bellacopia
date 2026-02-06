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
  struct battle hdr;
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

#define BATTLE ((struct battle_throwing*)battle)

/* Delete.
 */
 
static void _throwing_del(struct battle *battle) {
}

/* Initialize player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int appearance) {
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
    if (rand()&1) player->prepressp=BATTLE->ltarget+fudge;
    else player->prepressp=BATTLE->ltarget-fudge;
    if (rand()&1) player->prereleasep=BATTLE->rtarget+fudge;
    else player->prereleasep=BATTLE->rtarget-fudge;
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
 
static int _throwing_init(struct battle *battle) {
  
  BATTLE->finger=0.0;
  BATTLE->dfinger=1.000; // 1.5 seems playable but really fast.
  BATTLE->stage=STAGE_PLAY;
  
  // (ltarget) goes anywhere in the low half, and (rtarget) at least 1/4 above it.
  int ltargeti=(rand()&0x7fff);
  int rtargeti=ltargeti+0x4000+(rand()%(0xc000-ltargeti));
  BATTLE->ltarget=(double)ltargeti/65536.0;
  BATTLE->rtarget=(double)rtargeti/65536.0;
  // And beyond that, keep them at least 1/12 from the edges.
  if (BATTLE->ltarget<1.0/12.0) BATTLE->ltarget=1.0/12.0;
  if (BATTLE->rtarget>11.0/12.0) BATTLE->rtarget=11.0/12.0;
  
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  
  BATTLE->playerv[0].who=0;
  BATTLE->playerv[1].who=1;
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  return 0;
}

/* Human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (input&EGG_BTN_SOUTH) return;
    player->blackout=0;
  }
  if (player->pressp<0.0) {
    if (input&EGG_BTN_SOUTH) {
      bm_sound(RID_sound_uimotion);
      player->pressp=BATTLE->finger;
    }
  } else if (player->releasep<0.0) {
    if (!(input&EGG_BTN_SOUTH)) {
      bm_sound(RID_sound_uiactivate);
      player->releasep=BATTLE->finger;
    }
  }
}

/* CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  // We'll throw on the second rightward stroke.
  if (player->blackout) {
    if (BATTLE->dfinger<0.0) player->blackout=0;
  } else if (player->pressp<0.0) {
    if (BATTLE->dfinger>0.0) {
      if (BATTLE->finger>=player->prepressp) {
        player->pressp=BATTLE->finger;
      }
    }
  } else if (player->releasep<0.0) {
    if (BATTLE->dfinger>0.0) {
      if (BATTLE->finger>=player->prereleasep) {
        player->releasep=BATTLE->finger;
      }
    }
  }
}

/* All players.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
}

/* Cancel pitches.
 */
 
static void throwing_cancel_pitches(struct battle *battle) {
  if ((BATTLE->playerv[0].pressp>=0.0)&&(BATTLE->playerv[0].releasep<0.0)) {
    BATTLE->playerv[0].pressp=-1.0;
    BATTLE->playerv[0].blackout=1;
  }
  if ((BATTLE->playerv[1].pressp>=0.0)&&(BATTLE->playerv[1].releasep<0.0)) {
    BATTLE->playerv[1].pressp=-1.0;
    BATTLE->playerv[1].blackout=1;
  }
}

/* Select the decorative landing site for each ball.
 */
 
static void throwing_select_landing_sites(struct battle *battle) {
  if (BATTLE->preoutcome>0) {
    BATTLE->playerv[0].dstx=FBW>>1;
    BATTLE->playerv[1].dstx=(FBW>>1)+NS_sys_tilesize;
  } else if (BATTLE->preoutcome<0) {
    BATTLE->playerv[0].dstx=(FBW>>1)-NS_sys_tilesize;
    BATTLE->playerv[1].dstx=FBW>>1;
  } else {
    BATTLE->playerv[0].dstx=(FBW>>1)-4;
    BATTLE->playerv[1].dstx=(FBW>>1)+4;
  }
}

/* Set outcome. Both players' (pressp,releasep) must be set.
 */
 
static void throwing_set_preoutcome(struct battle *battle) {
  double lscore=0.0,rscore=0.0; // Lower is better.
  #define DIFF(score,a,b) { \
    double d=(a)-(b); \
    if (d<0.0) d=-d; \
    score+=d; \
  }
  double la=BATTLE->playerv[0].pressp;
  double lz=BATTLE->playerv[0].releasep;
  double ra=BATTLE->playerv[1].pressp;
  double rz=BATTLE->playerv[1].releasep;
  if (la>lz) { double tmp=la; la=lz; lz=tmp; }
  if (ra>rz) { double tmp=ra; ra=rz; rz=tmp; }
  DIFF(lscore,la,BATTLE->ltarget)
  DIFF(lscore,lz,BATTLE->rtarget)
  DIFF(rscore,ra,BATTLE->ltarget)
  DIFF(rscore,rz,BATTLE->rtarget)
  #undef DIFF
  if (lscore<rscore) {
    BATTLE->preoutcome=1;
  } else if (lscore>rscore) {
    BATTLE->preoutcome=-1;
  } else {
    BATTLE->preoutcome=0;
  }
}

/* Update in play mode.
 */
 
static void throwing_update_play(struct battle *battle,double elapsed) {

  // Update players.
  struct player *player=BATTLE->playerv;
  int i=2; for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* If all four control points are established, advance to POSTPLAY.
   */
  if (
    (BATTLE->playerv[0].pressp>=0.0)&&
    (BATTLE->playerv[0].releasep>=0.0)&&
    (BATTLE->playerv[1].pressp>=0.0)&&
    (BATTLE->playerv[1].releasep>=0.0)
  ) {
    BATTLE->stage=STAGE_POSTPLAY;
    BATTLE->stageclock=1.0;
    throwing_set_preoutcome(battle);
    throwing_select_landing_sites(battle);
    return;
  }
  
  /* Advance the finger.
   * Finger pingpongs, and if anyone is holding their button at the edges, they're dropped.
   */
  BATTLE->finger+=BATTLE->dfinger*elapsed;
  if (BATTLE->finger>1.0) {
    BATTLE->finger=1.0;
    if (BATTLE->dfinger>0.0) {
      BATTLE->dfinger=-BATTLE->dfinger;
      throwing_cancel_pitches(battle);
    }
  } else if (BATTLE->finger<0.0) {
    BATTLE->finger=0.0;
    if (BATTLE->dfinger<0.0) {
      BATTLE->dfinger=-BATTLE->dfinger;
      throwing_cancel_pitches(battle);
    }
  }
}

/* Update.
 */
 
static void _throwing_update(struct battle *battle,double elapsed) {

  // Done?
  if (battle->outcome>-2) return;
  
  switch (BATTLE->stage) {
    case STAGE_PLAY: throwing_update_play(battle,elapsed); break;
    case STAGE_POSTPLAY: {
        if ((BATTLE->stageclock-=elapsed)<=0.0) {
          BATTLE->stage=STAGE_ANIMATE;
          BATTLE->stageclock=ANIMATE_TIME;
        }
      } break;
    case STAGE_ANIMATE: {
        if ((BATTLE->stageclock-=elapsed)<=0.0) {
          BATTLE->stage=STAGE_GLOAT;
          BATTLE->stageclock=1.0;
        }
      } break;
    case STAGE_GLOAT: {
        if ((BATTLE->stageclock-=elapsed)<=0.0) {
          BATTLE->stage=STAGE_DONE;
          battle->outcome=BATTLE->preoutcome;
        }
      } break;
  }
}

/* Render one player.
 */
 
static void player_render(struct battle *battle,struct player *player,int x,int y,uint8_t xform) {
  uint8_t tileid=player->tileid;
  int py=y;
  if (BATTLE->stage==STAGE_ANIMATE) {
    tileid+=1;
  } else if (BATTLE->stage>=STAGE_GLOAT) {
    if (player->who) {
      if (BATTLE->preoutcome<=0) tileid+=2;
      else tileid+=3;
    } else {
      if (BATTLE->preoutcome>=0) tileid+=2;
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
  
  if (BATTLE->stage==STAGE_ANIMATE) {
    double t=1.0-(BATTLE->stageclock/ANIMATE_TIME);
    ballx=(int)(ballx*(1.0-t)+player->dstx*t);
    t=(t-0.5)*2.0;
    t*=t;
    t=1.0-t;
    bally=y-(int)(48.0*t);
    
  } else if (BATTLE->stage>=STAGE_GLOAT) {
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
 
static void _throwing_render(struct battle *battle) {
  const int groundy=130;
  graf_fill_rect(&g.graf,0,0,FBW,groundy,0x785830ff);
  graf_fill_rect(&g.graf,0,groundy,FBW,FBH-groundy,0x3c2011ff);
  graf_fill_rect(&g.graf,0,groundy,FBW,1,0x000000ff);
  
  int barw=(FBW*2)/3;
  int barh=20;
  int barx=(FBW>>1)-(barw>>1);
  int bary=40;
  graf_fill_rect(&g.graf,barx,bary,barw,barh,0xffffffff);
  
  struct player *player=BATTLE->playerv;
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
  if (BATTLE->stage==STAGE_PLAY) {
    int fingerx=(int)(BATTLE->finger*barw);
    if (fingerx<0) fingerx=0;
    else if (fingerx>=barw) fingerx=barw-1;
    fingerx+=barx;
    graf_tile(&g.graf,fingerx,bary+barh,0x65,0);
  }
  int ltargetx=barx+(int)(BATTLE->ltarget*barw);
  int rtargetx=barx+(int)(BATTLE->rtarget*barw);
  graf_tile(&g.graf,ltargetx,bary-(NS_sys_tilesize>>1),0x69,0);
  graf_tile(&g.graf,rtargetx,bary-(NS_sys_tilesize>>1),0x69,0);
  
  for (player=BATTLE->playerv,i=2;i-->0;player++) {
    if (player->pressp>=0.0) {
      graf_tile(&g.graf,barx+(int)(player->pressp*barw),bary+(barh*(player->who+1))/3,player->tileid_icon,0);
    }
    if (player->releasep>=0.0) {
      graf_tile(&g.graf,barx+(int)(player->releasep*barw),bary+(barh*(player->who+1))/3,player->tileid_icon,0);
    }
  }
  
  player_render(battle,BATTLE->playerv+0,FBW/6,groundy-(NS_sys_tilesize>>1),0);
  player_render(battle,BATTLE->playerv+1,(FBW*5)/6,groundy-(NS_sys_tilesize>>1),EGG_XFORM_XREV);
  graf_tile(&g.graf,(FBW>>1)+2,groundy-(NS_sys_tilesize>>1),0x6a,0); // x+2 because the brim goes a bit right
}

/* Type definition.
 */
 
const struct battle_type battle_type_throwing={
  .name="throwing",
  .objlen=sizeof(struct battle_throwing),
  .strix_name=49,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_throwing_del,
  .init=_throwing_init,
  .update=_throwing_update,
  .render=_throwing_render,
};

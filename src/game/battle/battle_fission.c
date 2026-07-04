/* battle_fission.c
 */

#include "game/bellacopia.h"

#define GROUNDY 139 /* Must agree with image:battle_fission */
#define RODC 5
#define REACTION_REACTION 0.500 /* The overall reaction doesn't respond immediately, it catches up. */
#define CRITICAL_TIME 2.000
#define RED_ALERT_TIME 0.300
#define SCORE_GOAL 6.0

#define LEVEL_OFF 0
#define LEVEL_REACT 1
#define LEVEL_CRITICAL 2

#define THRESH_REACT    0.200
#define THRESH_CRITICAL 0.400

struct battle_fission {
  struct battle hdr;
  double blowclock; // If >0, counts down to doomsday, if you don't stop the reaction.
  double klaxtime; // Sound the klaxon when (blowclock) goes below this.
  double redalert; // Counts down at each sound of the klaxon.
  double doomsday; // If >0, counts up since the Doomsday animation started.
  double reaction; // 0..1, how nuclear is the whole deal right now.
  double no_backsies_clock; // If the critical warning begins, it stays on until this expires.
  int level;
  int stopped_music;
  double playtime;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform; // Constant.
    int xv[RODC]; // Horizontal center of each rod, left-to-right, absolute framebuffer pixels.
    double disv[RODC]; // Displacement of each rod. 0..1 = closed..open
    double reactivity; // Sum of (disv), computed each update.
    int xp; // Where the hero is standing, 0..RODC-1.
    double liftrate; // hz, positive. When holding UP.
    double pushrate; // hz, positive. When holding DOWN.
    double sliderate; // hz, positive. When idle.
    double scorecheat; // Multiplier.
    double score;
    
    double cpumoveclock;
    double cpumovedelay;
    double cpuchickentime; // 0..RED_ALERT_TIME, at what value of (blowclock) do I chicken out and push my rod.
    int cpupushing;
  } playerv[2];
};

#define BATTLE ((struct battle_fission*)battle)

/* Delete.
 */
 
static void _fission_del(struct battle *battle) {
  if (BATTLE->stopped_music) {
    if (modal_get_topmost(&modal_type_pvp)) bm_song_gently(RID_song_death_rattle);
    else bm_song_gently(bm_song_for_outerworld());
  }
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->xform=0;
    player->xv[RODC-1]=(FBW>>1)-20;
    int i=RODC-1; while (i-->0) player->xv[i]=player->xv[i+1]-20;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->xv[0]=(FBW>>1)+20;
    int i=1; for (;i<RODC;i++) player->xv[i]=player->xv[i-1]+20;
  }
  player->xp=RODC>>1;
  player->liftrate= 1.000;
  player->pushrate= 2.000;
  player->sliderate=0.050;
  player->scorecheat=0.800*(1.0-player->skill)+1.200*player->skill;
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cpumovedelay=0.300*(1.0-player->skill)+0.200*player->skill;
    player->cpuchickentime=CRITICAL_TIME*(0.950*(1.0-player->skill)+0.700*player->skill);
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x64a9d1ff;
        player->tileid=0x30;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x10;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x20;
      } break;
  }
}

/* New.
 */
 
static int _fission_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playtime=20.0;
  return 0;
}

/* Move horizontally, or adjust the focussed rod.
 */
 
static void player_move(struct battle *battle,struct player *player,int d) {
  int np=player->xp+d;
  if ((np<0)||(np>=RODC)) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  player->xp=np;
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
}

static void player_up(struct battle *battle,struct player *player,double elapsed) {
  if ((player->xp<0)||(player->xp>=RODC)) return;
  double v=player->disv[player->xp];
  if ((v+=player->liftrate*elapsed)>=1.0) v=1.0;
  player->disv[player->xp]=v;
}

static void player_down(struct battle *battle,struct player *player,double elapsed) {
  if ((player->xp<0)||(player->xp>=RODC)) return;
  double v=player->disv[player->xp];
  if ((v-=player->pushrate*elapsed)<=0.0) v=0.0;
  player->disv[player->xp]=v;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1);
  else if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1);
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player_up(battle,player,elapsed); break;
    case EGG_BTN_DOWN: player_down(battle,player,elapsed); break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* We've decided to delay. Pay that out.
   */
  if (player->cpumoveclock>0.0) {
    player->cpumoveclock-=elapsed;
    return;
  }
  
  /* If we've decided to push a rod, push it all the way.
   * In fact, take a quick breather after, too. "Whew, that was close".
   */
  if (player->cpupushing) {
    player_down(battle,player,elapsed);
    if (player->disv[player->xp]<=0.0) {
      player->cpupushing=0;
      player->cpumoveclock=0.800;
    }
    return;
  }
  
  /* If the core is critical and our focussed rod nonzero, push it.
   */
  if ((BATTLE->level==LEVEL_CRITICAL)&&(BATTLE->blowclock<=player->cpuchickentime)&&(player->disv[player->xp]>0.0)) {
    player->cpupushing=1;
    player_down(battle,player,elapsed);
    return;
  }
  
  /* Find the lowest of my rods.
   */
  int lowp=0;
  double lowv=player->disv[0];
  int i=1;
  for (;i<RODC;i++) {
    if (player->disv[i]<lowv) {
      lowp=i;
      lowv=player->disv[i];
    }
  }
  
  /* If the lowest is some threshold below the one I'm at, move toward it.
   */
  const double thresh=0.500;
  if ((lowp!=player->xp)&&(lowv<player->disv[player->xp]-thresh)) {
    if (lowp<player->xp) player_move(battle,player,-1);
    else player_move(battle,player,1);
    player->cpumoveclock=player->cpumovedelay;
    return;
  }
  
  /* Pull the current rod.
   */
  player_up(battle,player,elapsed);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  
  /* All moderating rods slowly slide back to the safe position.
   * What a great design. Kind of wish all nuclear apparatus was designed this way. [https://en.wikipedia.org/wiki/Demon_core]
   * Freshen (reactivity) while we're in there.
   */
  player->reactivity=0.0;
  int i=RODC;
  while (i-->0) {
    if ((player->disv[i]-=player->sliderate*elapsed)<=0.0) player->disv[i]=0.0;
    player->reactivity+=player->disv[i];
  }
}

/* Update.
 */
 
static void _fission_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if (BATTLE->redalert>0.0) BATTLE->redalert-=elapsed;
  
  /* If we struck doomsday, it takes over, nothing else happens.
   */
  if (BATTLE->doomsday>0.0) {
    BATTLE->doomsday+=elapsed;
    if (BATTLE->doomsday>=5.0) {
      battle->outcome=2;
    }
    return;
  }
  
  /* If playtime expires, declare a winner now.
   */
  if ((BATTLE->playtime-=elapsed)<=0.0) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
    return;
  }
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Update total reaction.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  double reactivity=(l->reactivity+r->reactivity)/(RODC*2.0);
  if (BATTLE->reaction<reactivity) {
    if ((BATTLE->reaction+=REACTION_REACTION*elapsed)>reactivity) BATTLE->reaction=reactivity;
  } else if (BATTLE->reaction>reactivity) {
    if ((BATTLE->reaction-=REACTION_REACTION*elapsed),reactivity) BATTLE->reaction=reactivity;
  }
  if (BATTLE->no_backsies_clock>0.0) {
    BATTLE->no_backsies_clock-=elapsed;
  } else {
    int level=(BATTLE->reaction<THRESH_REACT)?LEVEL_OFF:(BATTLE->reaction>THRESH_CRITICAL)?LEVEL_CRITICAL:LEVEL_REACT;
    if (level!=BATTLE->level) {
      BATTLE->level=level;
      if (BATTLE->level==LEVEL_CRITICAL) {
        BATTLE->blowclock=CRITICAL_TIME;
        BATTLE->klaxtime=2.000;
        BATTLE->no_backsies_clock=0.500;
      }
    }
  }
  
  /* At LEVEL_REACT, both players earn points according to their contribution.
   */
  double rsum=l->reactivity+r->reactivity;
  if ((BATTLE->level==LEVEL_REACT)&&(rsum>0.0)) {
    double lpart=l->reactivity/rsum;
    double rpart=1.0-lpart;
    l->score+=lpart*elapsed*l->scorecheat;
    r->score+=rpart*elapsed*r->scorecheat;
    if (l->score>=SCORE_GOAL) {
      if (r->score>=SCORE_GOAL) battle->outcome=1;
      else battle->outcome=1;
      return;
    } else if (r->score>=SCORE_GOAL) {
      battle->outcome=-1;
      return;
    }
  }
  
  /* At LEVEL_CRITICAL, we count down to doomsday.
   */
  if (BATTLE->level==LEVEL_CRITICAL) {
    if ((BATTLE->blowclock-=elapsed)<=0.0) {
      BATTLE->doomsday=0.001;
      egg_play_song(1,0,0,0.0,0.0);
      BATTLE->stopped_music=1;
    } else if (BATTLE->blowclock<=BATTLE->klaxtime) {
      bm_sound(RID_sound_klaxon);
      BATTLE->klaxtime-=0.400;
      BATTLE->redalert=RED_ALERT_TIME;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int y=58;
  graf_set_image(&g.graf,RID_image_battle_sea);
  uint8_t tileid=player->tileid;
  if (BATTLE->doomsday>0.0) {
    tileid+=2;
    if (!((g.framec+player->who*0x40)&0x70)) tileid+=1; // blink
  }
  graf_tile(&g.graf,player->xv[player->xp],y,tileid,player->xform);
  if (BATTLE->doomsday>0.0) return; // Don't bother silhouetting the rods or hand.
  int i=RODC;
  while (i-->0) {
    int rody=y+14;
    int dis=(int)(player->disv[i]*14.5);
    if (dis<0) dis=0; else if (dis>14) dis=14;
    rody-=dis;
    graf_tile(&g.graf,player->xv[i],rody,0x00,0);
    if (i==player->xp) {
      graf_tile(&g.graf,player->xv[i],rody-9,player->tileid+1,0);
    }
  }
}

/* Render the circular gauge that shows one player's contribution to the reaction.
 */
 
static void reactometer_render(struct battle *battle,struct player *player,int x,int y) {
  graf_set_image(&g.graf,RID_image_battle_sea);
  double n=player->reactivity/RODC;
  uint8_t tileid=0x02;
  if (n>=0.800) { // Try to agree with the tile's red zone.
    tileid=(g.framec&8)?0x03:0x04;
  }
  double t=-M_PI+0.500+n*(M_PI*2.0-1.000);
  uint8_t rot=(int8_t)((t*128.0)/M_PI);
  graf_fancy(&g.graf,x,y,tileid,0,0,NS_sys_tilesize,0,0x808080ff);
  graf_fancy(&g.graf,x,y,0x01,0,rot,NS_sys_tilesize,0,0x808080ff);
}

/* Horizontal bar showing one player's score.
 */
 
static void scorebar_render(struct battle *battle,int x,int y,int w,int h,double score,uint32_t color,int align) {
  int barw=((score*w)/SCORE_GOAL);
  if (barw<=0) return;
  if (barw>w) barw=w;
  if (align>0) x=x+w-barw;
  graf_fill_rect(&g.graf,x,y,barw,h,color);
}

/* Animated lightbulbs around the level indicator.
 */
 
static void lightbulbs_render(uint8_t tileid,int period,int phase) {
  const struct plan {
    int x,y,dx,c;
  } planv[]={
    {123,152, 5,16},
    {198,169,-5,16},
  };
  if (period>1) phase%=period;
  const struct plan *plan=planv;
  int pi=sizeof(planv)/sizeof(planv[0]);
  for (;pi-->0;plan++) {
    int x=plan->x;
    int i=plan->c;
    for (;i-->0;x+=plan->dx) {
      if (++phase>=period) {
        phase=0;
        graf_tile(&g.graf,x,plan->y,tileid,0);
      }
    }
  }
}

/* Render.
 */
 
static void _fission_render(struct battle *battle) {

  /* First just the background fill. Either white+black or sky+ground.
   */
  if (BATTLE->doomsday>0.0) {
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0xffffffff);
    graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,0x000000ff);
  } else {
    graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
    graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  }
  
  /* Heroes and rods render before the main scenery -- rods count on the pile for occlusion.
   */
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);

  /* Background is solid colors with a giant decal on top.
   * During doomsday, it's the same idea but with extra tints.
   * Draw it initially in the stark black-and-white.
   * We'll add the whiteout on top at the end, so it affects sprites too.
   */
  int whiteout=0; // 0..255, alpha of the desired whiteout layer.
  if (BATTLE->doomsday>0.0) {
    const double whiteout_time=1.500;
    const double fadein_time=2.000;
    double blackness=(BATTLE->doomsday-whiteout_time)/fadein_time;
    if (blackness>=1.0) whiteout=0x00;
    else if (blackness<=0.0) whiteout=0xff;
    else whiteout=(int)((1.0-blackness)*255.0);
    graf_set_tint(&g.graf,0x000000ff);
    graf_set_image(&g.graf,RID_image_battle_fission);
    graf_decal(&g.graf,0,0,0,0,FBW,FBH);
    graf_set_tint(&g.graf,0);
  } else { // Normal case, where everyone is still alive.
    graf_set_image(&g.graf,RID_image_battle_fission);
    graf_decal(&g.graf,0,0,0,0,FBW,FBH);
  }
  
  /* Control panel.
   * During Doomsday, this whole section is blacked out so don't bother.
   */
  if (BATTLE->doomsday<=0.0) {
    reactometer_render(battle,BATTLE->playerv+0,110,161);
    reactometer_render(battle,BATTLE->playerv+1,210,161);
    graf_set_image(&g.graf,RID_image_battle_sea);
    int lblx=0,lblw;
    switch (BATTLE->level) {
      case LEVEL_OFF: {
          lblw=18;
        } break;
      case LEVEL_REACT: {
          lblx=21;
          lblw=20;
          lightbulbs_render(0x05,3,g.framec/10);
        } break;
      case LEVEL_CRITICAL: {
          lblx=44;
          lblw=28;
          lightbulbs_render((g.framec&8)?0x06:0x07,1,0);
        } break;
    }
    graf_decal(&g.graf,124+lblx,157,128+lblx,0,lblw,7);
    scorebar_render(battle, 61,72,95,2,BATTLE->playerv[0].score,BATTLE->playerv[0].color,-1);
    scorebar_render(battle,164,72,95,2,BATTLE->playerv[1].score,BATTLE->playerv[1].color, 1);
  }
  
  /* Whiteout or red alert.
   */
  if (whiteout>0) {
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0xffffff00|whiteout);
  } else if (BATTLE->redalert>0.0) {
    int alpha=(int)((BATTLE->redalert*128.0)/RED_ALERT_TIME);
    if (alpha>0) graf_fill_rect(&g.graf,0,0,FBW,FBH,0xff000000|alpha);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_fission={
  .name="fission",
  .objlen=sizeof(struct battle_fission),
  .id=NS_battle_fission,
  .strix_name=269,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_fission_del,
  .init=_fission_init,
  .update=_fission_update,
  .render=_fission_render,
};

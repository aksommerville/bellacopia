/* battle_whining.c
 */

#include "game/bellacopia.h"

#define GROUNDY 120
#define TAPANIM_PERIOD 0.500
#define PAYOUT_TIME 0.200 /* Each stroke's effect pays out over so long. */
#define WINTIME 2.000

struct battle_whining {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t hugtileid;
    double x;
    int ina,inup;
    int blackout;
    int pvina,pvinup;
    uint16_t btnid_require; // 0,EGG_BTN_SOUTH,EGG_BTN_UP. Which button needs to be tapped now.
    int hug; // Nonzero if mama is hugging us right now.
    double whine; // 0..1, strength of my whine.
    int delta; // -1,1, which way is whine changing, while (whineclock) ticks.
    double whineclock; // Counts down after each tap.
    double uprate,downrate,decay; // Hz, all positive, rate of change to (whine).
    double score; // Seconds, time spent getting hugged.
    double tapdelay; // CPU, counts down.
    double tapdelaylo,tapdelayhi;
    int precision; // 0..0xffff, odds that CPU will press the right button.
  } playerv[2];
  
  double mamax;
  int mamafacedir; // -1,1 = left,right
  double mamaturnclock;
  double mamahugclock;
  double tapanimclock;
};

#define BATTLE ((struct battle_whining*)battle)

/* Delete.
 */
 
static void _whining_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=120.0;
  } else { // Right.
    player->who=1;
    player->x=200.0;
  }
  
  player->uprate  =0.250*(1.0-player->skill)+0.350*player->skill;
  player->downrate=0.070*(1.0-player->skill)+0.040*player->skill;
  player->decay   =3.000*(1.0-player->skill)+1.000*player->skill;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->uprate  *=0.900;
    player->downrate*=1.100;
    player->decay   *=1.100;
    player->tapdelaylo=0.150*(1.0-player->skill)+0.100*player->skill;
    player->tapdelayhi=player->tapdelaylo*1.500;
    player->precision=(int)(0x8000*(1.0-player->skill)+0xff00*player->skill);
    fprintf(stderr,"cpu %d delay=%.03f..%.03f precision=0x%04x\n",player->who,player->tapdelaylo,player->tapdelayhi,player->precision);
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x4a240bff;
        player->tileid=0x40;
        player->hugtileid=0x68;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x00;
        player->hugtileid=0x64;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x20;
        player->hugtileid=0x66;
      } break;
  }
}

/* New.
 */
 
static int _whining_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->mamax=160.0;
  BATTLE->mamafacedir=(rand()&1)?1:-1;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->ina=(input&EGG_BTN_SOUTH);
    player->inup=(input&EGG_BTN_UP);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* After a tap, there's a hard blackout.
   */
  if (player->tapdelay>0.0) {
    player->tapdelay-=elapsed;
    player->ina=0;
    player->inup=0;

  /* Payout in progress, or a button currently held, drop it.
   */
  } else if ((player->whineclock>0.0)||player->ina||player->inup||!player->btnid_require) {
    player->ina=0;
    player->inup=0;
    
  /* Whine and set a tap delay.
   */
  } else {
    if ((rand()&0xffff)<player->precision) { // guess right
      switch (player->btnid_require) {
        case EGG_BTN_SOUTH: player->ina=1; break;
        case EGG_BTN_UP: player->inup=1; break;
      }
    } else { // guess wrong
      switch (player->btnid_require) {
        case EGG_BTN_SOUTH: player->inup=1; break;
        case EGG_BTN_UP: player->ina=1; break;
      }
    }
    double n=(rand()&0xffff)/65535.0;
    player->tapdelay=player->tapdelaylo*(1.0-n)+player->tapdelayhi*n;
  }
}

/* Process a newly nonzero input.
 * Controllers don't invoke this, the generic pass does.
 */
 
static void player_tap(struct battle *battle,struct player *player,uint16_t btnid) {
  if (btnid==player->btnid_require) {
    bm_sound_pan(RID_sound_chatter,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->delta=1;
    player->whineclock=PAYOUT_TIME;
  } else if (!player->hug) {
    bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->delta=-1;
    player->whineclock=PAYOUT_TIME;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Which input is required?
   * And if we're being hugged, score it.
   */
  if (player->hug) {
    player->btnid_require=0;
    player->score+=elapsed;
  } else if (
    (player->who&&(BATTLE->mamafacedir>0))||
    (!player->who&&(BATTLE->mamafacedir<0))
  ) {
    player->btnid_require=EGG_BTN_UP;
  } else {
    player->btnid_require=EGG_BTN_SOUTH;
  }

  /* Did input state change?
   */
  if (player->ina!=player->pvina) {
    if (player->pvina=player->ina) {
      player_tap(battle,player,EGG_BTN_SOUTH);
    }
  }
  if (player->inup!=player->pvinup) {
    if (player->pvinup=player->inup) {
      player_tap(battle,player,EGG_BTN_UP);
    }
  }
  
  /* Paying out a whine change?
   */
  if (player->whineclock>0.0) {
    if (player->delta>0) {
      player->whine+=player->uprate*elapsed;
      if (player->whine>1.0) player->whine=1.0;
    } else if (player->delta<0) {
      player->whine-=player->downrate*elapsed;
      if (player->whine<0.0) player->whine=0.0;
    }
    player->whineclock-=elapsed;
  
  /* If we're not paying out a change, decay it.
   */
  } else {
    player->whine-=player->decay*elapsed;
    if (player->whine<0.0) player->whine=0.0;
  }
}

/* Update mama bear: The various courses of action.
 */
 
static void mama_idle(struct battle *battle,double elapsed) {
}

static void mama_approach(struct battle *battle,double elapsed,struct player *player) {
  const double speed=40.0; // px/s
  BATTLE->mamax+=speed*elapsed*BATTLE->mamafacedir;
  int ready=0;
  if (player->who) ready=(BATTLE->mamax>=player->x);
  else ready=(BATTLE->mamax<=player->x);
  if (ready) {
    BATTLE->mamax=player->x;
    player->hug=1;
    BATTLE->mamahugclock=0.500;
  } else {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    l->hug=r->hug=0;
  }
}

static void mama_turn(struct battle *battle,double elapsed) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  BATTLE->mamaturnclock=0.500;
  l->hug=r->hug=0;
}

/* Update mama bear: Decide action for this cycle.
 */
 
static void mama_update(struct battle *battle,double elapsed) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  struct player *current=(BATTLE->mamafacedir>0)?r:l;
  struct player *whinier=(l->whine>r->whine)?l:r;
  
  // If we're paying out a turn, do that.
  if (BATTLE->mamaturnclock>0.0) {
    if ((BATTLE->mamaturnclock-=elapsed)<=0.0) {
      BATTLE->mamafacedir*=-1;
    }
    
  // Ditto for hugs. There's a minimum amount of hugging.
  } else if (BATTLE->mamahugclock>0.0) {
    BATTLE->mamahugclock-=elapsed;
  
  // If the loudest whine is below some threshold, do nothing.
  } else if (whinier->whine<0.200) {
    mama_idle(battle,elapsed);
    
  // If I'm facing the loudest, approach.
  } else if (whinier==current) {
    mama_approach(battle,elapsed,current);
    
  // If the other guy's whine is some threshold greater than current, turn around.
  } else if (whinier->whine-current->whine>0.100) {
    mama_turn(battle,elapsed);
    
  // Approach the current, despite it being less whiny.
  } else {
    mama_approach(battle,elapsed,current);
  }
}

/* Update.
 */
 
static void _whining_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if ((BATTLE->tapanimclock-=elapsed)<=0.0) {
    BATTLE->tapanimclock+=TAPANIM_PERIOD;
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  mama_update(battle,elapsed);
  
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if ((l->score>=WINTIME)||(r->score>=WINTIME)) {
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0; // Ties are not actually possible; only one score can increase at a time.
    }
  }

  //XXX
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  if (player->hug) return; // Mama draws us.
  int midx=(int)player->x;
  int midy=GROUNDY-NS_sys_tilesize+1;
  const int ht=NS_sys_tilesize>>1;
  int frontx=midx,backx=midx;
  uint8_t xform;
  if (player->who) {
    frontx-=ht;
    backx+=ht;
    xform=EGG_XFORM_XREV;
  } else {
    frontx+=ht;
    backx-=ht;
    xform=0;
  }
  uint8_t tileid=player->tileid;
  if (player->whineclock>0.0) tileid+=2;
  graf_tile(&g.graf,backx ,midy-ht,tileid+0x00,xform);
  graf_tile(&g.graf,frontx,midy-ht,tileid+0x01,xform);
  graf_tile(&g.graf,backx ,midy+ht,tileid+0x10,xform);
  graf_tile(&g.graf,frontx,midy+ht,tileid+0x11,xform);
}

/* Render mama bear.
 */
 
static void mama_render(struct battle *battle) {
  int midx=(int)BATTLE->mamax;
  const int ht=NS_sys_tilesize>>1;
  int frontx=midx,backx=midx;
  uint8_t xform;
  if (BATTLE->mamafacedir<0) {
    frontx-=ht;
    backx+=ht;
    xform=EGG_XFORM_XREV;
  } else {
    frontx+=ht;
    backx-=ht;
    xform=0;
  }
  int y=GROUNDY-ht+1;
  uint8_t tileid=0x60;
  if (BATTLE->playerv[0].hug) {
    tileid=BATTLE->playerv[0].hugtileid;
  } else if (BATTLE->playerv[1].hug) {
    tileid=BATTLE->playerv[1].hugtileid;
  } else if (BATTLE->mamaturnclock>0.0) {
    tileid+=2;
  }
  graf_tile(&g.graf,backx ,y,tileid+0x20,xform);
  graf_tile(&g.graf,frontx,y,tileid+0x21,xform);
  y-=NS_sys_tilesize;
  graf_tile(&g.graf,backx ,y,tileid+0x10,xform);
  graf_tile(&g.graf,frontx,y,tileid+0x11,xform);
  y-=NS_sys_tilesize;
  graf_tile(&g.graf,backx ,y,tileid+0x00,xform);
  graf_tile(&g.graf,frontx,y,tileid+0x01,xform);
}

/* Button icons above the players.
 */

static void whining_render_btnid(struct battle *battle,int x,uint16_t btnid) {
  uint8_t tileid;
  switch (btnid) {
    case EGG_BTN_SOUTH: tileid=0x8d; break;
    case EGG_BTN_UP: tileid=0x6d; break;
    default: return;
  }
  graf_tile(&g.graf,x,GROUNDY-NS_sys_tilesize*4,tileid,0);
}

/* Vertical bar for a player's whine level.
 */
 
static void whining_bar(struct battle *battle,int x,double v,uint32_t color) {
  const int w=2;
  const int h=40;
  int fillh=(int)(v*h);
  if (fillh<0) fillh=0;
  else if (fillh>h) fillh=h;
  graf_fill_rect(&g.graf,x-2,GROUNDY-h-1,w+2,h+1,0x000000ff);
  graf_fill_rect(&g.graf,x-1,GROUNDY-fillh,w,fillh,color);
}

/* Render.
 */
 
static void _whining_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;

  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  
  // Sprites.
  graf_set_image(&g.graf,RID_image_battle_forest2);
  player_render(battle,l);
  player_render(battle,r);
  mama_render(battle);
  
  // Score as vertical bars in the middle.
  const int barw=3;
  const int barh=50;
  const int barb=GROUNDY-NS_sys_tilesize*4;
  graf_fill_rect(&g.graf,(FBW>>1)-barw-2,barb-barh-1,barw*2+3,barh+2,0x000000ff);
  int lh=(int)((l->score*barh)/WINTIME); if (lh<0) lh=0; else if (lh>barh) lh=barh;
  graf_fill_rect(&g.graf,(FBW>>1)-barw-1,barb-lh,barw,lh,l->color);
  int rh=(int)((r->score*barh)/WINTIME); if (rh<0) rh=0; else if (rh>barh) rh=barh;
  graf_fill_rect(&g.graf,(FBW>>1),barb-rh,barw,rh,r->color);
  
  // Whine level as vertical bars outside the players.
  whining_bar(battle,l->x-20,l->whine,l->color);
  whining_bar(battle,r->x+20,r->whine,r->color);
  
  // Required-input indicators.
  if (l->btnid_require||r->btnid_require) {
    graf_set_image(&g.graf,RID_image_battle_sea);
    if (BATTLE->tapanimclock>TAPANIM_PERIOD*0.5) graf_set_alpha(&g.graf,0x80);
    whining_render_btnid(battle,l->x,l->btnid_require);
    whining_render_btnid(battle,r->x,r->btnid_require);
    graf_set_alpha(&g.graf,0xff);
  }
}

/* Type definition.
 */
 
static const struct battle_input _whining_input[]={
  {1,EGG_BTN_UP|EGG_BTN_SOUTH},
  {1,0},
{0}};
 
const struct battle_type battle_type_whining={
  .name="whining",
  .objlen=sizeof(struct battle_whining),
  .id=NS_battle_whining,
  .strix_name=336,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=_whining_input,
  .imageid_default=0,
  .del=_whining_del,
  .init=_whining_init,
  .update=_whining_update,
  .render=_whining_render,
};

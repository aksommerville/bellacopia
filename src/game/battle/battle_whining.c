/* battle_whining.c
 */

#include "game/bellacopia.h"

#define GROUNDY 120

struct battle_whining {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t hugtileid;
    uint8_t guesstileid;
    double x;
    int ina;
    int blackout;
    int pvina;
    int hug; // Nonzero if mama is hugging us right now.
    double uprate; // Hz, how fast does my bar fill?
    double score; // 0..1
    double guesst; // Sample (ctlt) at the moment of our keystroke. <0 if we haven't guessed yet on this cycle.
    double recentt; // Last value of (guesst), we show it flying off the wheel.
    int favor;
    double cpuprept;
    double errlo,errhi;
  } playerv[2];
  
  /* Control wheel.
   * Rotates constantly, and there's a target both players are trying to hit.
   */
  double ctlt; // Positive radians clockwise. Does not wrap, counts up forever.
  double ctldt;
  double ctlddt;
  double ctldtlimit;
  double ctltarget;
  int ctlcross; // Goes nonzero when (ctlt) crosses (ctltarget).
  double recent_time; // Counts up from the last commit.
  double recent_target;
  
  double mamax;
  int mamafacedir; // -1,1 = left,right
  double mamaturnclock;
  double mamahugclock;
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
  player->guesst=-1.0;
  player->recentt=-1.0;
  
  player->uprate=0.250*(1.0-player->skill)+0.500*player->skill;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->uprate*=0.900; // CPU penalty.
    player->cpuprept=-1.0;
    player->errhi=1.200*(1.0-player->skill)+0.500*player->skill;
    player->errlo=player->errhi*0.250;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x4a240bff;
        player->tileid=0x40;
        player->hugtileid=0x68;
        player->guesstileid=0x44;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x00;
        player->hugtileid=0x64;
        player->guesstileid=0x24;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x20;
        player->hugtileid=0x66;
        player->guesstileid=0x34;
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
  BATTLE->ctldt=3.000; // rad/sec
  BATTLE->ctlddt=0.500; // rad/sec**2
  BATTLE->ctldtlimit=6.000; // rad/sec
  BATTLE->ctltarget=((rand()&0xffff)*M_PI*2.0)/65535.0;
  BATTLE->recent_target=-1.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->ina=(input&EGG_BTN_SOUTH);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  player->ina=0;
  if (player->guesst>=0.0) { // Already guessed for this cycle.
    player->cpuprept=-1.0;
    return;
  }
  
  // Prepare my guess if we haven't yet.
  if (player->cpuprept<0.0) {
    double err=(rand()&0xffff)/65535.0;
    err=player->errlo*(1.0-err)+player->errhi*err;
    if (rand()&1) player->cpuprept=BATTLE->ctltarget+err;
    else player->cpuprept=BATTLE->ctltarget-err;
    if (player->cpuprept<0.0) player->cpuprept=0.0; // Maybe clamp it the first time, no big deal.
  }
  
  // Hold A when control wheel passes my chosen angle.
  if (BATTLE->ctlt>=player->cpuprept) player->ina=1;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Plant a guess when she presses A.
  if (player->ina!=player->pvina) {
    if (player->pvina=player->ina) {
      if (player->guesst<0.0) {
        bm_sound_pan(RID_sound_chatter,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->guesst=BATTLE->ctlt;
      }
    }
  }

  // If we're being hugged, advance score.
  if (player->hug) {
    player->score+=player->uprate*elapsed;
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
  struct player *whinier=l->favor?l:r->favor?r:current;
  
  // If we're paying out a turn, do that.
  if (BATTLE->mamaturnclock>0.0) {
    if ((BATTLE->mamaturnclock-=elapsed)<=0.0) {
      BATTLE->mamafacedir*=-1;
    }
    
  // Ditto for hugs. There's a minimum amount of hugging.
  } else if (BATTLE->mamahugclock>0.0) {
    BATTLE->mamahugclock-=elapsed;
  
  // Possible that (whinier) doesn't have (favor) set, eg initially. If so, do nothing.
  } else if (!whinier->favor) {
    mama_idle(battle,elapsed);
    
  // If I'm facing the loudest, approach.
  } else if (whinier==current) {
    mama_approach(battle,elapsed,current);
    
  // Turn to face the favorite child.
  } else {
    mama_turn(battle,elapsed);
  }
}

/* Turn the control wheel.
 */
 
static void wheel_update(struct battle *battle,double elapsed) {

  BATTLE->recent_time+=elapsed;

  /* Accelerate.
   */
  BATTLE->ctldt+=BATTLE->ctlddt*elapsed;
  if (BATTLE->ctldt>BATTLE->ctldtlimit) BATTLE->ctldt=BATTLE->ctldtlimit;

  /* Advance, and check crossing.
   */
  double nt=BATTLE->ctlt+BATTLE->ctldt*elapsed;
  if ((BATTLE->ctlt<BATTLE->ctltarget)&&(nt>=BATTLE->ctltarget)) BATTLE->ctlcross=1;
  BATTLE->ctlt=nt;
  
  /* Did we cross a threshold a little beyond the target?
   * If not, carry on and do nothing.
   */
  if (!BATTLE->ctlcross) return;
  double dt=BATTLE->ctlt-BATTLE->ctltarget;
  if (dt<M_PI*0.250) return;
  
  /* Rate each player's guess and set the nearer as the new champion.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  double ld=l->guesst-BATTLE->ctltarget;
  double rd=r->guesst-BATTLE->ctltarget;
  if (ld<0.0) ld=-ld;
  if (rd<0.0) rd=-rd;
  if (ld<rd) {
    l->favor=1;
    r->favor=0;
  } else {
    l->favor=0;
    r->favor=1;
  }
  
  /* Pick a new target at least some tasteful interval beyond the current position.
   */
  const double footroom=M_PI*0.500;
  const double range=M_PI*2.0-footroom;
  double ndt=((rand()&0xffff)*range)/65535.0;
  nt=BATTLE->ctlt+footroom+ndt;
  BATTLE->recent_target=BATTLE->ctltarget;
  BATTLE->ctltarget=nt;
  BATTLE->ctlcross=0;
  l->recentt=l->guesst;
  r->recentt=r->guesst;
  l->guesst=-1.0;
  r->guesst=-1.0;
  l->cpuprept=-1.0;
  r->cpuprept=-1.0;
  BATTLE->recent_time=0.0;
}

/* Update.
 */
 
static void _whining_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  mama_update(battle,elapsed);
  wheel_update(battle,elapsed);
  
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if ((l->score>=1.0)||(r->score>=1.0)) {
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0; // Ties are not actually possible; only one score can increase at a time.
    }
  }
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
  if (player->favor) tileid+=2;
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

/* Vertical bar for a player's whine level.
 */
 
static void whining_bar(struct battle *battle,int x,double v,uint32_t color) {
  const int w=2;
  const int h=40;
  int fillh=(int)(v*h);
  if (fillh<0) fillh=0;
  else if (fillh>h) fillh=h;
  graf_fill_rect(&g.graf,x-2,GROUNDY+4,w+2,h+2,0x000000ff);
  graf_fill_rect(&g.graf,x-1,GROUNDY+5+h-fillh,w,fillh,color);
}

/* Render a tile on the control wheel's rim.
 */
 
static void whining_decorate_wheel(struct battle *battle,double midx,double midy,double t,uint8_t tileid,double age) {
  const double fadetime=1.000;
  if (age>fadetime) return;
  double radius=12.0+age*10.0;
  int x=lround(midx+sin(t)*radius);
  int y=lround(midy-cos(t)*radius);
  int alpha=0xff-((age*255.0)/fadetime);
  if (alpha>0) {
    if (alpha<0xff) graf_set_alpha(&g.graf,alpha);
    graf_tile(&g.graf,x,y,tileid,0);
    graf_set_alpha(&g.graf,0xff);
  }
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
  
  // Control wheel.
  double wheelx=FBW*0.5;
  double wheely=GROUNDY+20.0;
  whining_decorate_wheel(battle,wheelx,wheely,BATTLE->ctltarget,0x54,0.0);
  double cost=cos(BATTLE->ctlt);
  double sint=sin(BATTLE->ctlt);
  graf_set_filter(&g.graf,1);
  graf_decal_rotate(&g.graf,(int)wheelx,(int)wheely,64,0,32,sint,cost,0.750);
  graf_set_filter(&g.graf,0);
  if (BATTLE->recent_target>=0.0) whining_decorate_wheel(battle,wheelx,wheely,BATTLE->recent_target,0x54,BATTLE->recent_time);
  if (l->recentt>=0.0) whining_decorate_wheel(battle,wheelx,wheely,l->recentt,l->guesstileid,BATTLE->recent_time);
  if (r->recentt>=0.0) whining_decorate_wheel(battle,wheelx,wheely,r->recentt,r->guesstileid,BATTLE->recent_time);
  if (l->guesst>=0.0) whining_decorate_wheel(battle,wheelx,wheely,l->guesst,l->guesstileid,0.0);
  if (r->guesst>=0.0) whining_decorate_wheel(battle,wheelx,wheely,r->guesst,r->guesstileid,0.0);
  
  // Score as vertical bars outside the wheel.
  whining_bar(battle,(FBW>>1)-40,l->score,l->color);
  whining_bar(battle,(FBW>>1)+40,r->score,r->color);
}

/* Type definition.
 */
 
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
  .input=battle_input_a,
  .imageid_default=0,
  .del=_whining_del,
  .init=_whining_init,
  .update=_whining_update,
  .render=_whining_render,
};

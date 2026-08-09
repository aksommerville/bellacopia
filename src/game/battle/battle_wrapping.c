/* battle_wrapping.c
 */

#include "game/bellacopia.h"

#define QUEUE_SIZE 5
#define WR_LIMIT 4 /* Might be possible to have multiple, if you're fast. */
#define SCORE_LIMIT 4
#define SCORE_TTL 1.000
#define DISPSCORE_TIME 0.010

struct battle_wrapping {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t queuev[QUEUE_SIZE]; // Upcoming gifts, always full. tileid in image:battle_tundra. Pop from [0].
    double queueoffset; // Snaps to 1 when we shuffle (queuev), then slides back to zero.
    int blackout;
    int indx,indy;
    uint8_t gift; // Tileid in image:battle_tundra, the active one. There's always an active gift.
    double x,y; // In player-zone pixels.
    int px,py,pw,ph; // Wrapping paper, the target.
    double dx,dy;
    double reject; // Counts down after rejected placement, for visual highlight.
    int score;
    int dispscore; // Lags behind actual score, so you can watch it spin.
    double dispscoreclock; // Counts down per score unit, interval will likely be shorter than a frame.
    double cpudstx,cpudsty;
    
    // Constants per skill.
    double velmax; // px/s
    double accel; // px/s**2
    double decel; // px/s**2
    double scorelo; // 10..99, can go OOB to force clamping.
    double scorehi; // ''
    double errorlo; // 0..1, CPU only.
    double errorhi; // ''
    
    // Gifts being wrapped and tallied.
    struct wr {
      int px,py,pw,ph; // Original bounds of paper.
      double x,y; // Position of toy on the paper initially, then position of the wrapped gift.
      uint8_t tileid;
      double sealp; // 0..1, status of initial sealing. (tileid) changes when we reach 1.
      int l,r,t,b; // Sanitized distance of (x,y) to each edge.
    } wrv[WR_LIMIT];
    int wrc;
    
    // Score toasts.
    struct score {
      double x,y;
      double dy; // Float down if it starts high, otherwise up.
      double ttl;
      int v; // 10..99, always 2 digits
    } scorev[SCORE_LIMIT];
    int scorec;
    
  } playerv[2];
};

#define BATTLE ((struct battle_wrapping*)battle)

/* Delete.
 */
 
static void _wrapping_del(struct battle *battle) {
}

/* Choose a random location and size for new wrapping paper.
 */
 
static void player_random_paper(struct player *player) {
  const int sizelo=40; // Gifts are 16x16, keep it substantially larger than that.
  const int sizehi=100; // Must be under 160.
  player->pw=sizelo+(rand()%(sizehi-sizelo+1));
  player->ph=sizelo+(rand()%(sizehi-sizelo+1));
  player->px=rand()%(160-player->pw);
  player->py=rand()%(180-player->ph);
}

static void player_default_position(struct player *player) {
  player->x=player->who?30.0:130.0;
  player->y=75.0;
  // Resetting position also resets velocity, would be weird otherwise.
  player->dx=player->dy=0.0;
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  
  /* Using slightly drunken controls, too much eggnog, as the driver of difficulty.
   * There's also an explicit score range, and the CPU's error margin.
   */
  player->velmax=180.0*player->skill+120.0*(1.0-player->skill);
  player->accel=200.0*player->skill+100.0*(1.0-player->skill);
  player->decel=300.0*player->skill+200.0*(1.0-player->skill);
  player->scorelo= 15.0*player->skill+10.0*(1.0-player->skill);
  player->scorehi=110.0*player->skill+99.0*(1.0-player->skill);
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->errorlo=0.010*player->skill+0.100*(1.0-player->skill);
    player->errorhi=0.100*player->skill+0.300*(1.0-player->skill);
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x808080ff;//TODO
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
  
  /* Fill the queue with random gifts.
   * The choice of tile is purely decorative.
   */
  uint8_t *dst=player->queuev;
  int i=QUEUE_SIZE;
  for (;i-->0;dst++) *dst=0x97+rand()%9;
  player->gift=0x97+rand()%9;
  player_random_paper(player);
  player_default_position(player);
}

/* New.
 */
 
static int _wrapping_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Shuffle the queue, append something random, and set the offset.
 */
 
static void player_queue_next(struct battle *battle,struct player *player) {
  player->gift=player->queuev[0];
  player_default_position(player);
  memmove(player->queuev,player->queuev+1,QUEUE_SIZE-1);
  player->queuev[QUEUE_SIZE-1]=0x97+rand()%9;
  player->queueoffset=1.0;
}

/* Commit active gift at the current location.
 */
 
static void player_commit(struct battle *battle,struct player *player) {

  /* If OOB, reject.
   * Otherwise make a sound.
   */
  const int margin=10; // Minimum width is 40, so this is never more than half of the visible paper. Must be at least 8, otherwise the toy's edge might be off the paper.
  int x=(int)player->x;
  int y=(int)player->y;
  if (
    (x<player->px+margin)||
    (y<player->py+margin)||
    (x>=player->px+player->pw-margin)||
    (y>=player->py+player->ph-margin)
  ) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->reject=0.500;
    return;
  }
  bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);

  /* Score it.
   */
  double halfw=player->pw*0.5;
  double halfh=player->ph*0.5;
  double midx=player->px+halfw;
  double midy=player->py+halfh;
  double dx=(player->x-midx)/halfw;
  double dy=(player->y-midy)/halfh;
  double q=1.0-sqrt(dx*dx+dy*dy);
  int v=lround(player->scorelo*(1.0-q)+player->scorehi*q);
  if (v<10) v=10; else if (v>99) v=99;
  player->score+=v;
  
  /* If there's room in (scorev), add a toast.
   */
  if (player->scorec<SCORE_LIMIT) {
    struct score *score=player->scorev+player->scorec++;
    score->x=player->x;
    score->y=player->y;
    score->ttl=SCORE_TTL;
    score->v=v;
    score->dy=(player->y>=90.0)?-20.0:20.0;
  } else {
    fprintf(stderr,"SCOREV EXHAUSTED\n");
  }

  /* Add a new (wr) if there's room.
   */
  if (player->wrc<WR_LIMIT) {
    struct wr *wr=player->wrv+player->wrc++;
    wr->px=player->px;
    wr->py=player->py;
    wr->pw=player->pw;
    wr->ph=player->ph;
    wr->x=player->x;
    wr->y=player->y;
    wr->tileid=player->gift;
    wr->sealp=0.0;
    if ((wr->l=(int)player->x-player->px-8)<1) wr->l=1;
    if ((wr->t=(int)player->y-player->py-8)<1) wr->t=1;
    if ((wr->r=player->pw-wr->l-16)<1) wr->r=1;
    if ((wr->b=player->ph-wr->t-16)<1) wr->b=1;
  }

  player_queue_next(battle,player);
  player_random_paper(player);
  player->cpudstx=player->cpudsty=0.0; // CPU will reassess on its next update.
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0; break;
  }
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player->indy=-1; break;
    case EGG_BTN_DOWN: player->indy=1; break;
    default: player->indy=0; break;
  }
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    player_commit(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Choose a target some controlled margin off of the paper's center. Choose once per gift.
   * (0,0) is not a valid position; there's a margin we can't cross.
   * This is a critical part of the CPU player; without it he'll score 99 every time.
   */
  double targetx=player->cpudstx,targety=player->cpudsty;
  if ((targetx<1.0)||(targety<1.0)) {
    double radius=(player->pw>player->ph)?player->ph:player->pw;
    double error=(rand()&0xffff)/65535.0;
    error=(1.0-error)*player->errorlo+error*player->errorhi;
    error*=radius;
    double t=((rand()&0xffff)*M_PI*2.0)/65535.0;
    targetx=player->cpudstx=player->px+player->pw*0.5+sin(t)*error;
    targety=player->cpudsty=player->py+player->ph*0.5+cos(t)*error;
  }
  
  /* Steer toward the targets if we're some small interval away.
   * I was worried that this would be too perfect, but actually there's some built-in fudge due to acceleration/deceleration.
   * Even a seemingly perfect CPU implementation tends to overshoot and backtrack. It works great.
   */
  const double tolerance=2.000;
  double dx=targetx-player->x;
  if (dx>tolerance) player->indx=1;
  else if (dx<-tolerance) player->indx=-1;
  else player->indx=0;
  double dy=targety-player->y;
  if (dy>tolerance) player->indy=1;
  else if (dy<-tolerance) player->indy=-1;
  else player->indy=0;
  
  /* If we chose not to steer, plant the gift.
   */
  if (!player->indx&&!player->indy) player_commit(battle,player);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Advance dispscore.
   * Mind that this has a very fine interval, and will often tick multiple times per frame.
   */
  if (player->dispscore<player->score) {
    player->dispscoreclock-=elapsed;
    while ((player->dispscoreclock<=0.0)&&(player->dispscore<player->score)) {
      player->dispscoreclock+=DISPSCORE_TIME;
      player->dispscore++;
    }
  }

  /* Tick down rejection clock.
   */
  if (player->reject>0.0) {
    if ((player->reject-=elapsed)<=0.0) {
      player->reject=0.0;
    }
  }

  /* Animate the queue.
   */
  if (player->queueoffset>0.0) {
    if ((player->queueoffset-=elapsed*1.500)<=0.0) {
      player->queueoffset=0.0;
    }
  }
  
  /* Accelerate or decelerate per axis according to dpad.
   * The deceleration rate should be greater than acceleration, so when you reverse direction use decel until it crosses zero.
   * Er, to say that more simply: Accelerate when velocity sign agrees with input, otherwise decelerate.
   */
  if ((player->indx<0)&&(player->dx<=0.0)) {
    if ((player->dx-=player->accel*elapsed)<-player->velmax) player->dx=-player->velmax;
  } else if ((player->indx>0)&&(player->dx>=0.0)) {
    if ((player->dx+=player->accel*elapsed)>player->velmax) player->dx=player->velmax;
  } else if (player->dx<0.0) {
    if (((player->dx+=player->decel*elapsed)>=0.0)&&!player->indx) player->dx=0.0;
  } else if (player->dx>0.0) {
    if (((player->dx-=player->decel*elapsed)<=0.0)&&!player->indx) player->dx=0.0;
  }
  if ((player->indy<0)&&(player->dy<=0.0)) {
    if ((player->dy-=player->accel*elapsed)<-player->velmax) player->dy=-player->velmax;
  } else if ((player->indy>0)&&(player->dy>=0.0)) {
    if ((player->dy+=player->accel*elapsed)>player->velmax) player->dy=player->velmax;
  } else if (player->dy<0.0) {
    if (((player->dy+=player->decel*elapsed)>=0.0)&&!player->indy) player->dy=0.0;
  } else if (player->dy>0.0) {
    if (((player->dy-=player->decel*elapsed)<=0.0)&&!player->indy) player->dy=0.0;
  }
  
  /* Apply velocity and clamp to edges.
   */
  const double xlo=9.0,ylo=9.0,xhi=151.0,yhi=171.0;
  player->x+=player->dx*elapsed;
  player->y+=player->dy*elapsed;
  if (player->x<xlo) player->x=xlo; else if (player->x>xhi) player->x=xhi;
  if (player->y<ylo) player->y=ylo; else if (player->y>yhi) player->y=yhi;
  
  /* Advance animation of finished gifts, and drop any that left the screen.
   */
  int i=player->wrc;
  struct wr *wr=player->wrv+i-1;
  for (;i-->0;wr--) {
    if (wr->sealp>=1.0) { // Wrapped, shipping.
      const double shipspeed=200.0;
      int defunct=0;
      if (player->who) {
        if ((wr->x+=shipspeed*elapsed)>=170.0) defunct=1;
      } else {
        if ((wr->x-=shipspeed*elapsed)<=-10.0) defunct=1;
      }
      if (defunct) {
        player->wrc--;
        memmove(wr,wr+1,sizeof(struct wr)*(player->wrc-i));
      }
    } else { // Wrapping.
      if ((wr->sealp+=2.000*elapsed)>=1.0) {
        wr->tileid=0xa7;
      }
    }
  }
  
  /* Animate and reap score toasts.
   */
  struct score *score=player->scorev+player->scorec-1;
  for (i=player->scorec;i-->0;score--) {
    if ((score->ttl-=elapsed)<=0.0) {
      player->scorec--;
      memmove(score,score+1,sizeof(struct score)*(player->scorec-i));
    } else {
      score->y+=score->dy*elapsed;
    }
  }
}

/* Update.
 */
 
static void _wrapping_update(struct battle *battle,double elapsed) {
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (battle->outcome==-2) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    } else {
      player->indx=player->indy=0;
    }
    player_update_common(battle,player,elapsed);
  }
  
  /* First to reach 1225 wins. (but don't clamp there)
   * OK to declare end while the dispscore are still ticking.
   * If we time out, it's a tie even if points have been awarded -- I don't think that's likely.
   * Non-timeout ties are vanishingly unlikely; they'd have to both reach 1225 on the same frame *and* land on the same score.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if ((l->score>=1225)||(r->score>=1225)) {
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 * After completion, only transient things render (wrv and scorev).
 */
 
static void player_render(struct battle *battle,struct player *player,int fullx,int fully,int fullw,int fullh) {
  int x,y,i;
  const uint32_t paperwhite=0xb3a986ff; // Substantially off-white, to help the text read over it.
  const uint32_t paperpink=0x9e1f73ff;
  
  /* Wrapping paper.
   * Before the gifts being wrapped, I think. Let them be a funny distraction.
   */
  if (battle->outcome==-2) {
    graf_set_input(&g.graf,0);
    graf_fill_rect(&g.graf,fullx+player->px,fully+player->py,player->pw,player->ph,paperwhite);
  }
  
  /* Outgoing gifts, wrapping and departing.
   */
  const struct wr *wr=player->wrv;
  for (i=player->wrc;i-->0;wr++) {
    if (wr->sealp>=1.0) { // Sealed, departing.
      graf_set_image(&g.graf,RID_image_battle_tundra);
      graf_tile(&g.graf,fullx+(int)wr->x,fully+(int)wr->y,wr->tileid,0);
    } else { // Sealing.
      /* The outer bounds collapse from (px,py,pw,ph) to a 16x16 square around the gift.
       * As they collapse, a pink border grows around the edge. From 0 to 8 pixels per edge, proportionate again to (sealp).
       * We've precalculated in (l,r,t,b) the amount of travel for each edge overall.
       */
      int l=wr->px+(int)(wr->l*wr->sealp);
      int t=wr->py+(int)(wr->t*wr->sealp);
      int r=wr->px+wr->pw-(int)(wr->r*wr->sealp);
      int b=wr->py+wr->ph-(int)(wr->b*wr->sealp);
      int border=lround(wr->sealp*8.0);
      graf_fill_rect(&g.graf,fullx+l,fully+t,r-l,b-t,paperwhite);
      graf_set_image(&g.graf,RID_image_battle_tundra);
      graf_tile(&g.graf,fullx+(int)wr->x,(int)wr->y,wr->tileid,0);
      if (border>0) {
        graf_fill_rect(&g.graf,fullx+l,fully+t,border,b-t,paperpink);
        graf_fill_rect(&g.graf,fullx+l,fully+t,r-l,border,paperpink);
        graf_fill_rect(&g.graf,fullx+r-border,fully+t,border,b-t,paperpink);
        graf_fill_rect(&g.graf,fullx+l,fully+b-border,r-l,border,paperpink);
      }
    }
  }
  
  // When game ended, stop here.
  if (battle->outcome>-2) return;

  /* Queue of pending gifts.
   */
  const int queue_spacing=17;
  graf_set_image(&g.graf,RID_image_battle_tundra);
  x=(FBW>>1)+(fullx?10:-10);
  y=QUEUE_SIZE*queue_spacing-10;
  y-=(int)(player->queueoffset*queue_spacing);
  const uint8_t *qv=player->queuev;
  for (i=QUEUE_SIZE;i-->0;qv++,y-=queue_spacing) {
    graf_tile(&g.graf,x,y,*qv,0);
  }
  
  /* Active gift.
   */
  x=fullx+(int)player->x;
  y=fully+(int)player->y;
  if (player->reject>0.0) {
    int tint=0x40+((player->reject*128.0)/0.500);
    graf_set_tint(&g.graf,0xff000000|tint);
  }
  graf_tile(&g.graf,x,y,player->gift,0);
  graf_set_tint(&g.graf,0);
  
  // (scorev) renders in a separate pass.
}

/* Render a total score.
 */
 
static void wrapping_render_score(struct battle *battle,int v,int x,int y,int align) {
  // Three digits is probably enough, but let's allow four for safety.
  if (v<0) v=0; else if (v>9999) v=9999;
  char text[4];
  int textc=0;
  if (v>=1000) text[textc++]='0'+v/1000;
  if (v>=100) text[textc++]='0'+(v/100)%10;
  if (v>=10) text[textc++]='0'+(v/10)%10;
  text[textc++]='0'+v%10;
  // If we're right-aligned, reverse text.
  int dx=8;
  if (align>0) {
    int i=textc>>1,ap=0,bp=textc;
    while (i-->0) {
      bp--;
      char tmp=text[ap];
      text[ap]=text[bp];
      text[bp]=tmp;
      ap++;
    }
    dx=-dx;
  }
  // Then render it in order.
  const char *textp=text;
  for (;textc-->0;textp++,x+=dx) graf_tile(&g.graf,x,y,*textp,0);
}

/* Render.
 */
 
static void _wrapping_render(struct battle *battle) {

  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x104020ff);
  
  /* Player zones.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  player_render(battle,l,0,0,FBW>>1,FBH);
  player_render(battle,r,FBW>>1,0,FBW>>1,FBH);
  
  /* Score toasts per player.
   * Keeping separate because they should always be on top, and they can share a batch.
   * We clamp per-gift scores to 10..99 so they are always two digits.
   */
  if (l->scorec||r->scorec) {
    graf_set_image(&g.graf,RID_image_tinyfonttiles);
    struct score *score;
    int i;
    graf_set_tint(&g.graf,0x000000ff);
    graf_set_alpha(&g.graf,0xc0);
    for (i=l->scorec,score=l->scorev;i-->0;score++) {
      graf_tile(&g.graf,(int)score->x-3,(int)score->y+1,'0'+score->v/10,0);
      graf_tile(&g.graf,(int)score->x+3,(int)score->y+1,'0'+score->v%10,0);
    }
    for (i=r->scorec,score=r->scorev;i-->0;score++) {
      graf_tile(&g.graf,(FBW>>1)+(int)score->x-3,(int)score->y+1,'0'+score->v/10,0);
      graf_tile(&g.graf,(FBW>>1)+(int)score->x+3,(int)score->y+1,'0'+score->v%10,0);
    }
    graf_set_tint(&g.graf,0);
    graf_set_alpha(&g.graf,0xff);
    for (i=l->scorec,score=l->scorev;i-->0;score++) {
      graf_tile(&g.graf,(int)score->x-3,(int)score->y,'0'+score->v/10,0);
      graf_tile(&g.graf,(int)score->x+3,(int)score->y,'0'+score->v%10,0);
    }
    for (i=r->scorec,score=r->scorev;i-->0;score++) {
      graf_tile(&g.graf,(FBW>>1)+(int)score->x-3,(int)score->y,'0'+score->v/10,0);
      graf_tile(&g.graf,(FBW>>1)+(int)score->x+3,(int)score->y,'0'+score->v%10,0);
    }
  }
  
  /* Total scores per player.
   */
  graf_set_image(&g.graf,RID_image_fonttiles);
  wrapping_render_score(battle,l->dispscore,(FBW>>1)-8,FBH-10,1);
  wrapping_render_score(battle,r->dispscore,(FBW>>1)+8,FBH-10,-1);
  
  /* Dividing line between the players.
   */
  graf_set_input(&g.graf,0);
  graf_line(&g.graf,FBW>>1,0,0x000000ff,FBW>>1,FBH,0x000000ff);
}

/* Type definition.
 */
 
const struct battle_type battle_type_wrapping={
  .name="wrapping",
  .objlen=sizeof(struct battle_wrapping),
  .id=NS_battle_wrapping,
  .strix_name=316,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_dpad_a,
  .imageid_default=0,
  .del=_wrapping_del,
  .init=_wrapping_init,
  .update=_wrapping_update,
  .render=_wrapping_render,
};

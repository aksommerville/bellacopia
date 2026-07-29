/* battle_cutting.c
 */

#include "game/bellacopia.h"

#define SAMPLE_LIMIT 64

struct ipt {
  uint8_t x,y;
};

// Use the separate "cutting" tool to generate these off image:battle_cutting.
static const struct ipt pattern_heart[]={
  {68,116},{60,108},{52,100},{44,92},{36,84},{28,76},{20,68},{20,60},{20,52},{20,44},{20,36},{20,28},
  {28,20},{36,12},{44,12},{52,12},{60,20},{68,28},{76,20},{84,12},{92,12},{100,12},{108,20},{116,28},
  {116,36},{116,44},{116,52},{116,60},{116,68},{108,76},{100,84},{92,92},{84,100},{76,108},{68,116},
{0,0}};
static const struct ipt pattern_diamond[]={
  {68,116},{60,116},{52,108},{44,100},{36,92},{28,84},{28,76},{20,68},{20,60},{28,52},{28,44},{36,36},
  {44,28},{52,20},{60,12},{68,12},{76,20},{84,28},{92,36},{100,44},{100,52},{108,60},{108,68},{100,76},
  {100,84},{92,92},{84,100},{76,108},{68,116},
{0,0}};
static const struct ipt pattern_star[]={
  {116,116},{116,108},{108,100},{100,92},{92,84},{84,76},{92,68},{100,60},{108,60},{116,52},{116,44},
  {108,44},{100,44},{92,44},{84,44},{76,36},{76,28},{76,20},{68,12},{60,20},{52,28},{52,36},{44,44},{36,44},
  {28,44},{20,44},{12,44},{12,52},{20,60},{28,60},{36,68},{44,76},{36,84},{28,92},{20,100},{12,108},{12,116},
  {20,116},{28,116},{36,116},{44,108},{52,108},{60,100},{68,92},{76,100},{84,108},{92,108},{100,116},
  {108,116},{116,116},
{0,0}};
static const struct ipt pattern_circle[]={
  {76,108},{68,108},{60,108},{52,108},{44,100},{36,92},{28,84},{20,76},{20,68},{20,60},{20,52},{28,44},
  {36,36},{44,28},{52,20},{60,20},{68,20},{76,20},{84,28},{92,36},{100,44},{108,52},{108,60},{108,68},
  {108,76},{100,84},{92,92},{84,100},{76,108},
{0,0}};
static const struct ipt pattern_hat[]={
  {108,116},{100,116},{92,116},{84,116},{76,116},{68,116},{60,116},{52,116},{44,116},{36,116},{28,116},
  {20,116},{12,108},{20,100},{28,100},{36,100},{36,92},{44,84},{44,76},{36,68},{36,60},{28,52},{20,44},
  {12,36},{12,28},{12,20},{12,12},{20,12},{28,20},{36,28},{44,28},{52,36},{60,36},{68,44},{76,52},{84,60},
  {84,68},{92,76},{92,84},{92,92},{100,100},{108,100},{116,108},{108,116},
{0,0}};
static const struct ipt pattern_seashell[]={
  {84,116},{76,116},{68,116},{60,116},{52,116},{44,116},{36,108},{44,100},{36,92},{28,92},{20,84},{12,76},
  {12,68},{12,60},{12,52},{12,44},{20,36},{28,28},{36,20},{44,20},{52,12},{60,12},{68,12},{76,12},{84,20},
  {92,20},{100,28},{108,36},{116,44},{116,52},{116,60},{116,68},{116,76},{108,84},{100,92},{92,92},{84,100},
  {92,108},{84,116},
{0,0}};
static const struct ipt pattern_claw[]={
  {108,116},{100,116},{92,116},{84,116},{76,116},{68,116},{60,116},{52,116},{44,116},{36,108},{28,108},
  {20,100},{20,92},{12,84},{12,76},{12,68},{12,60},{12,52},{20,44},{20,36},{28,28},{36,20},{44,20},{52,20},
  {52,28},{52,36},{52,44},{52,52},{60,52},{68,52},{76,52},{76,60},{76,68},{76,76},{84,76},{92,76},{100,76},
  {100,84},{100,92},{108,100},{116,108},{108,116},
{0,0}};
static const struct ipt pattern_beet[]={
  {84,116},{84,108},{84,100},{84,92},{76,84},{68,76},{68,68},{68,60},{68,52},{68,44},{68,36},{76,28},
  {84,20},{76,12},{68,12},{60,20},{52,12},{44,12},{36,20},{44,28},{52,36},{44,44},{52,52},{52,60},{52,68},
  {44,76},{36,84},{36,92},{36,100},{44,108},{52,108},{60,116},{68,116},{76,116},{84,116},
{0,0}};

static const struct ipt *patternv[]={
  pattern_heart,
  pattern_diamond,
  pattern_star,
  pattern_circle,
  pattern_hat,
  pattern_seashell,
  pattern_claw,
  pattern_beet,
};

struct battle_cutting {
  struct battle hdr;
  double playclock;
  int patternp;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    const struct ipt *pattern;
    int patternc;
    double speed;
    double dt;
    double animclock;
    int animframe;
    
    int incut,indt; // Controller sets only these.
    int blackout;
    
    double x,y; // Pixels, in player's view (128x128).
    double dx,dy; // px/s, only relevant when cutting. Speed baked in.
    double t; // Radians, angle of sweeping preview.
    uint8_t rot; // Angle for tile. Same as (t) while cutting.
    int pvincut;
    int done;
    double score; // Smaller is better.
    
    struct ipt samplev[SAMPLE_LIMIT];
    int samplec;
    
    int cputargetp; // Advances through (pattern). Points to one that we've already consumed; +1 before reading again.
    int cpudt; // -1,0,1, which way we need to turn before the next cut.
    double cput; // Selected angle.
    double cpurunclock; // Counts down while cutting, calculated from desired leg length.
    double straightenage; // Constish, how eagerly do we skip close-to-collinear points. 1/100..1/10 feels reasonable.
    double mistakery; // Constish, pixels, how far off target can we deliberately go. Random per stroke, this is just the limit.
  } playerv[2];
};

#define BATTLE ((struct battle_cutting*)battle)

/* Delete.
 */
 
static void _cutting_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  
  /* Pick a pattern.
   * Originally each player was going to get their own, but I don't like that.
   */
  player->pattern=patternv[BATTLE->patternp];
  player->patternc=0;
  while (player->pattern[player->patternc].x) player->patternc++;
  
  player->x=player->pattern[0].x;
  player->y=player->pattern[0].y;
  player->speed=50.0; // px/sec.
  player->dt=5.0; // rad/sec.
  // Actually (speed,dt) don't matter much. We're not using time in the score. High (dt), say above 5, can be tricky for humans.
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
  
    // Straightenage has a huge impact on CPU's play. Zero makes him try to connect the dots exactly. 1/10 is visibly skippy for most patterns.
    player->straightenage=0.100*(1.0-player->skill)+0.010*player->skill;
    
    // Mistakery is straightforward: Before deciding to approach a point, we deliberately move it by up to so much, in pixels.
    player->mistakery=6.000*(1.0-player->skill)+1.000*player->skill;
  
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xb01010ff;
        player->tileid=0x82;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x80;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x80;
      } break;
  }
}

/* New.
 */
 
static int _cutting_init(struct battle *battle) {

  // Both players will get the same pattern, and the choice is entirely random.
  int patternc=sizeof(patternv)/sizeof(patternv[0]);
  BATTLE->patternp=rand()%patternc;

  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playclock=30.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->incut=(input&EGG_BTN_SOUTH);
  }
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indt=-1; break;
    case EGG_BTN_RIGHT: player->indt=1; break;
    default: player->indt=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* While (cpudt) nonzero, we're turning.
   * Zero it after we cross the target angle.
   */
  if (player->cpudt) {
    player->incut=0;
    if (player->cpudt<0) {
      if (player->t<=player->cput) player->cpudt=0;
    } else {
      if (player->t>=player->cput) player->cpudt=0;
    }
    player->indt=player->cpudt;
    return;
  }
  player->indt=0;

  /* While (cpurunclock) counts down, we're cutting.
   */
  if (player->cpurunclock>0.0) {
    player->cpurunclock-=elapsed;
    player->incut=1;
    return;
  }
  player->incut=0;
  
  /* If (cputargetp) is beyond the pattern, do nothing.
   * This does happen, don't worry about it.
   */
  player->cputargetp++;
  if (player->cputargetp>=player->patternc) {
    player->done=1;
    return;
  }
  
  /* Here's the interesting part.
   * Advance (cputargetp), take the angle from our position to that point, and consume more while they're within some threshold of that angle.
   * Having selected that range of pattern points, fuzz out the final position just a little bit per intentional error.
   * Then compute the desired angle and run time (which introduces yet more error).
   * First chunk here selects the first step and a unit vector in that direction.
   */
  const struct ipt *firststep=player->pattern+player->cputargetp;
  double dx=firststep->x+0.5-player->x;
  double dy=firststep->y+0.5-player->y;
  double d2=dx*dx+dy*dy;
  while (d2<1.0) { // Distance too small, take the next point, and watch for the end.
    player->cputargetp++;
    if (player->cputargetp>=player->patternc) {
      player->done=1;
      return;
    }
    firststep=player->pattern+player->cputargetp;
    dx=firststep->x+0.5-player->x;
    dy=firststep->y+0.5-player->y;
    d2=dx*dx+dy*dy;
  }
  double firstlen=sqrt(d2);
  dx/=firstlen;
  dy/=firstlen;
  
  /* Keep advancing (cputargetp) while the angle roughly agrees with (dx,dy).
   */
  int advc=1;
  while (player->cputargetp+1<player->patternc) {
    const struct ipt *next=player->pattern+player->cputargetp+1;
    double nextdx=next->x+0.5-player->x;
    double nextdy=next->y+0.5-player->y;
    double nextd2=nextdx*nextdx+nextdy*nextdy;
    if (nextd2<1.0) break; // Hmm. Faulty pattern? Get out, and let the next pass worry about it.
    double nextlen=sqrt(nextd2);
    nextdx/=nextlen;
    nextdy/=nextlen;
    // Squared distance between these two unit vectors.
    double diffx=nextdx-dx;
    double diffy=nextdy-dy;
    double diff2=diffx*diffx+diffy*diffy;
    if (diff2>player->straightenage) break;
    player->cputargetp++;
    advc++;
  }
  
  /* Now our target point is (cputargetp), but we have an opportunity to fuzz it away, to deliberately introduce error.
   */
  double targetx=player->pattern[player->cputargetp].x+0.5;
  double targety=player->pattern[player->cputargetp].y+0.5;
  const double minmistake=0.500; // There's a constant floor to mistakery. To keep things shook, there must always be some amount of mistake.
  double mistake=(rand()&0xffff)/65535.0;
  mistake=minmistake*(1.0-mistake)+player->mistakery*mistake;
  double misst=((rand()&0xffff)*M_PI*2.0)/65535.0;
  targetx+=cos(misst)*mistake;
  targety-=sin(misst)*mistake;
  
  /* Choose angle and run time to that point.
   * (t) does not get normalized on the fly like we usually do, to allow it to cross (cput), possibly into the >pi range.
   * We'll normalize the delta ourselves, right here.
   */
  dx=targetx-player->x;
  dy=targety-player->y;
  d2=dx*dx+dy*dy;
  if (d2<1.0) d2=1.0; // Huh? If it's really close, lie about the distance.
  double len=sqrt(d2);
  double nx=dx/len;
  double ny=dy/len;
  player->cput=atan2(nx,-ny);
  while (player->t<-M_PI) player->t+=M_PI*2.0;
  while (player->t>M_PI) player->t-=M_PI*2.0;
  double dt=player->cput-player->t;
  while (dt<-M_PI) dt+=M_PI*2.0;
  while (dt>M_PI) dt-=M_PI*2.0;
  if (dt>0.0) player->cpudt=1;
  else if (dt<0.0) player->cpudt=-1;
  // and if they happen to be exactly equal, keep (cpudt) at zero.
  // Denormalize if necessary. Order of (t,cput) must agree with (cpudt).
  if (player->cpudt<0) {
    while (player->cput>player->t) player->cput-=M_PI*2.0;
  } else if (player->cpudt>0) {
    while (player->cput<player->t) player->cput+=M_PI*2.0;
  }
  player->cpurunclock=len/player->speed;
}

/* Sample player's position.
 * Called at the end of each stroke.
 */
 
static void player_sample(struct battle *battle,struct player *player) {
  int x=lround(player->x);
  int y=lround(player->y);
  
  // If Manhattan distance to the last sample is zero or one, drop it.
  const struct ipt *pv=player->samplev+player->samplec-1;
  int dx=x-pv->x; if (dx<0) dx=-dx;
  int dy=y-pv->y; if (dy<0) dy=-dy;
  if (dx+dy<=1) return;
  
  // If our buffer is full, eliminate the odd-indexed samples.
  if (player->samplec>=SAMPLE_LIMIT) {
    struct ipt *to=player->samplev+1;
    struct ipt *from=player->samplev+2;
    int nc=(player->samplec>>1)-1;
    int i=nc;
    for (;i-->0;to++,from+=2) *to=*from;
    player->samplec=nc;
  }
  
  // Append.
  player->samplev[player->samplec++]=(struct ipt){x,y};
}

/* Rate (a) sample points by their distance to the nearest (b) line.
 */
 
static double cutting_distance_to_segment(double x,double y,double ax,double ay,double bx,double by) {
  // Translate both against (a).
  double dx=x-ax;
  double dy=y-ay;
  bx-=ax;
  by-=ay;
  // Take the denormalized scalar projection -- if <=0, use straight distance to (a).
  double proj=dx*bx+dy*by;
  if (proj<=0.0) return sqrt(dx*dx+dy*dy);
  double blen2=bx*bx+by*by;
  proj/=blen2;
  if (proj>=1.0) return sqrt((dx-bx)*(dx-bx)+(dy-by)*(dy-by));
  // Scalar projection is in bounds, so take the scalar rejection.
  double rej=(dx*by-dy*bx)/sqrt(blen2);
  if (rej<0.0) rej=-rej;
  return rej;
}
 
static double cutting_rate_line(const struct ipt *a,int ac,const struct ipt *_b,int bc) {
  if (ac<1) return 999.999;
  int ac0=ac;
  double sum=0.0;
  for (;ac-->0;a++) {
    double ax=a->x+0.5;
    double ay=a->y+0.5;
    double best=999.999;
    int bi=bc-1;
    const struct ipt *b=_b;
    for (;bi-->0;b++) {
      double len=cutting_distance_to_segment(ax,ay,b[0].x+0.5,b[0].y+0.5,b[1].x+0.5,b[1].y+0.5);
      if (len<best) best=len;
    }
    sum+=best;
  }
  return sum/ac0;
}

/* Set (player->done) if he is.
 * This is called at the end of each stroke.
 */
 
static void player_check_completion(struct battle *battle,struct player *player) {

  // You need at least 4 samples to produce any kind of polygon. (4, not 3, because in our case the first and last points are the same vertex).
  if (player->samplec<4) return;
  
  // Last sample must be close to the first. Manhattan distance is fine, it's not rocket science.
  const struct ipt *first=player->samplev;
  const struct ipt *last=player->samplev+player->samplec-1;
  int dx=last->x-first->x; if (dx<0) dx=-dx;
  int dy=last->y-first->y; if (dy<0) dy=-dy;
  if (dx+dy>20) return;
  
  /* Now ensure there's at least one sample a good distance away.
   * This guards against players strobing A right at the start.
   */
  int ok=0;
  const struct ipt *sample=player->samplev;
  int i=player->samplec;
  for (;i-->0;sample++) {
    dx=sample->x-first->x; if (dx<0) dx=-dx;
    dy=sample->y-first->y; if (dy<0) dy=-dy;
    if (dx+dy>50) {
      ok=1;
      break;
    }
  }
  if (!ok) return;
  
  /* If first and last are not exactly the same, close the loop.
   * If we happen to be at the end of the buffer, overwrite the last sample.
   */
  if ((first->x!=last->x)||(first->y!=last->y)) {
    if (player->samplec<SAMPLE_LIMIT) {
      player->samplev[player->samplec++]=*first;
    } else {
      player->samplev[player->samplec-1]=*first;
    }
  }
  
  player->done=1;
  player->incut=0;
  
  /* Score is the distance of each point to its nearest reference line.
   * Do that both ways and add: Pattern points against sample lines, plus sample points against pattern lines.
   */
  player->score=
    cutting_rate_line(player->pattern,player->patternc,player->samplev,player->samplec)+
    cutting_rate_line(player->samplev,player->samplec,player->pattern,player->patternc);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Animate.
   */
  if (player->incut) {
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.180;
      if (++(player->animframe)>=2) player->animframe=0;
    }
  } else {
    player->animframe=0;
    player->animclock=0;
  }
  
  /* When cutting begins, lock in the angle.
   */
  if (player->incut!=player->pvincut) {
    if (player->pvincut=player->incut) {
      player->rot=(int8_t)((player->t*128.0)/M_PI);
      player->dx=sin(player->t)*player->speed;
      player->dy=-cos(player->t)*player->speed;
      if (!player->samplec) player->samplev[player->samplec++]=(struct ipt){(int)player->x,(int)player->y};
    } else {
      player_sample(battle,player);
      player_check_completion(battle,player);
    }
  }
  
  /* If cutting, move along the locked-in direction.
   */
  if (player->incut) {
    player->x+=player->dx*elapsed;
    player->y+=player->dy*elapsed;
    if (player->x<0.0) player->x=0.0; else if (player->x>=127.0) player->x=127.0;
    if (player->y<0.0) player->y=0.0; else if (player->y>=127.0) player->y=127.0;
    
  /* Otherwise, rotate per input.
   * Do not normalize angle. CPU depends on being able to cross it.
   * This might mean it goes a full circle around sometimes, but at least it doesn't get stuck.
   */
  } else {
    player->t+=player->indt*player->dt*elapsed;
  }
}

/* Update.
 */
 
static void _cutting_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* Update players if not done.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->done) continue;
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Game ends when both players are done or playclock expires.
   * (score) is the sum of distances from point to reference line, so lower is better.
   * Ties are very unlikely, unless both players fail to finish in time.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (((BATTLE->playclock-=elapsed)<=0.0)||(l->done&&r->done)) {
    if (!l->done) l->score=999999.999;
    if (!r->done) r->score=999999.999;
    if (l->score<r->score) battle->outcome=1;
    else if (l->score>r->score) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render unsigned integer.
 */
 
static void render_uint(int x,int y,int v) {
  if (v<0) v=0; else if (v>999) v=999;
  if (v>=100) {
    graf_tile(&g.graf,x-8,y,'0'+v/100,0);
    graf_tile(&g.graf,x,y,'0'+(v/10)%10,0);
    graf_tile(&g.graf,x+8,y,'0'+v%10,0);
  } else if (v>=10) {
    graf_tile(&g.graf,x-4,y,'0'+v/10,0);
    graf_tile(&g.graf,x+4,y,'0'+v%10,0);
  } else {
    graf_tile(&g.graf,x,y,'0'+v,0);
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  /* Measurements, backdrop, and pattern.
   */
  const int dstw=128;
  const int dsth=128;
  int dstx=player->who?((FBW>>1)+10):((FBW>>1)-10-dstw);
  int dsty=(FBH>>1)-(dsth>>1);
  graf_fill_rect(&g.graf,dstx,dsty,dstw,dsth,0xe0e0e0ff);
  
  /* Render pattern.
   */
  graf_set_image(&g.graf,RID_image_battle_cutting);
  const struct ipt *ipt=player->pattern;
  int i=player->patternc;
  for (;i-->0;ipt++) graf_tile(&g.graf,dstx+ipt->x,dsty+ipt->y,0x84,0);
  
  /* The line.
   */
  if (player->samplec>=1) {
    graf_set_input(&g.graf,0);
    graf_line_strip_begin(&g.graf,dstx+player->samplev[0].x,dsty+player->samplev[0].y,player->color);
    const struct ipt *sample=player->samplev+1;
    for (i=player->samplec-1;i-->0;sample++) {
      graf_line_strip_more(&g.graf,dstx+sample->x,dsty+sample->y,player->color);
    }
    // And one last segment to the focus, if we're cutting.
    if (player->incut) {
      graf_line_strip_more(&g.graf,dstx+lround(player->x),dsty+lround(player->y),player->color);
    }
  }
  
  // No guideline or scissors if done.
  if (player->done) return;
  
  /* When not cutting, show the target line.
   */
  if (!player->incut) {
    const double radius=12.0;
    int ax=dstx+(int)player->x;
    int ay=dsty+(int)player->y;
    int bx=dstx+(int)(player->x+sin(player->t)*radius);
    int by=dsty+(int)(player->y-cos(player->t)*radius);
    graf_set_input(&g.graf,0);
    graf_line(&g.graf,ax,ay,player->color,bx,by,player->color);
  }
  
  /* Scissors.
   */
  int px=dstx+(int)player->x;
  int py=dsty+(int)player->y;
  uint8_t tileid=player->tileid+player->animframe;
  graf_set_image(&g.graf,RID_image_battle_cutting);
  graf_set_filter(&g.graf,1);
  graf_fancy(&g.graf,px,py,tileid,0,player->rot,NS_sys_tilesize,0,player->color);
  graf_set_filter(&g.graf,0);
}

/* Render.
 */
 
static void _cutting_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  player_render(battle,l);
  player_render(battle,r);
  
  if (battle->outcome==-2) {
    graf_set_image(&g.graf,RID_image_fonttiles);
    int s=(int)(BATTLE->playclock+0.999);
    if (s<1) s=1;
    render_uint(FBW>>1,20,s);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_cutting={
  .name="cutting",
  .objlen=sizeof(struct battle_cutting),
  .id=NS_battle_cutting,
  .strix_name=306,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_a,
  .imageid_default=0,
  .del=_cutting_del,
  .init=_cutting_init,
  .update=_cutting_update,
  .render=_cutting_render,
};

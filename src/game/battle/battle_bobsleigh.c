/* battle_bobsleigh.c
 * Generate a track of straight-line edges, with a straight guideline down the middle.
 * Apply gravity according to the guideline, and user can press LEFT and RIGHT to turn a little.
 * Big velocity penalty for hitting the wall.
 */

#include "game/bellacopia.h"

struct battle_bobsleigh {
  struct battle hdr;
  double playclock;
  
  /* Each leg contains its guideline and two walls.
   * Guidelines and walls connect to the next neighbor.
   * The first leg is just a fixed point.
   * To do anything sensible, you need to examine two adjacent legs.
   */
  struct leg {
    double guidex,guidey;
    double lx,ly;
    double rx,ry;
    double nx,ny; // Unit vector in my direction of gravity (ie toward me, from the previous point).
    double len; // Distance in pixels from the previous guide point.
  } *legv;
  int legc,lega;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    
    int finished;
    double runtime;
    
    int steer; // -1,0,1. Controller sets.
    
    double x,y;
    double dx,dy;
    int legp;
    double steer_accel; // m/s**2 per skill
    double sampleclock; // For validating motion and triggering emergency measures.
    double samplex,sampley;
  } playerv[2];
};

#define BATTLE ((struct battle_bobsleigh*)battle)

/* Delete.
 */
 
static void _bobsleigh_del(struct battle *battle) {
  if (BATTLE->legv) free(BATTLE->legv);
}

/* Append a leg. Wall points are initially undefined.
 */
 
static struct leg *bobsleigh_append_leg(struct battle *battle,double x,double y) {
  if (BATTLE->legc>=BATTLE->lega) {
    int na=BATTLE->lega+16;
    if (na>INT_MAX/sizeof(struct leg)) return 0;
    void *nv=realloc(BATTLE->legv,sizeof(struct leg)*na);
    if (!nv) return 0;
    BATTLE->legv=nv;
    BATTLE->lega=na;
  }
  struct leg *leg=BATTLE->legv+BATTLE->legc++;
  leg->guidex=x;
  leg->guidey=y;
  return leg;
}

/* Set guide and both walls of (dst) by extending the guide line from (b) thru (a) to the given (distance).
 */
 
static void bobsleigh_set_tip(struct leg *dst,const struct leg *a,const struct leg *b,double distance) {
  double dx=a->guidex-b->guidex;
  double dy=a->guidey-b->guidey;
  double abd=sqrt(dx*dx+dy*dy);
  double nx=dx/abd;
  double ny=dy/abd;
  dst->guidex=dst->lx=dst->rx=a->guidex+nx*distance;
  dst->guidey=dst->ly=dst->ry=a->guidey+ny*distance;
}

/* Generate track, main entry point.
 */
 
static int bobsleigh_generate_track(struct battle *battle) {

  /* The track will always proceed top-to-bottom, to reinforce the impression of gravity.
   * Add vertices at uniform vertical intervals, within some horizontal limit of the previous point.
   */
  #define RIBBONC 9
  #define HORZLIMIT 40
  #define HORZMARGIN 30
  if (!bobsleigh_append_leg(battle,0.0,0.0)) return -1; // Special first point; we'll come back to populate it later.
  int x=HORZMARGIN+(rand()%(FBW-HORZMARGIN*2));
  struct leg *leg=bobsleigh_append_leg(battle,x,FBH/(RIBBONC+1.0));
  if (!leg) return -1;
  int i=1; for (;i<RIBBONC;i++) {
    double y=((i+0.5)*FBH)/(RIBBONC+1.0);
    int xlo=x-HORZLIMIT;
    int xhi=x+HORZLIMIT;
    if (xlo<HORZMARGIN) xlo=HORZMARGIN;
    if (xhi>FBW-HORZMARGIN) xhi=FBW-HORZMARGIN;
    x=xlo+rand()%(xhi-xlo+1);
    if (!(leg=bobsleigh_append_leg(battle,x,y))) return -1;
  }
  #undef RIBBONC
  #undef HORZLIMIT
  #undef HORZMARGIN
  if (!bobsleigh_append_leg(battle,0.0,0.0)) return -1; // Special last point; we'll come back to populate it later.
  if (BATTLE->legc<4) return -1; // Ensure we have at least 2 intermediate points, not counting the special end caps.
  
  /* The first and last points will be a fixed distance from their neighbor, continuing the angle into that neighbor.
   * Their wall points are the same as their guide points, the track tapers off.
   */
  struct leg *first=BATTLE->legv;
  struct leg *last=BATTLE->legv+BATTLE->legc-1;
  const double tipdistance=30.0;
  bobsleigh_set_tip(first,BATTLE->legv+1,BATTLE->legv+2,tipdistance);
  bobsleigh_set_tip(last,BATTLE->legv+BATTLE->legc-2,BATTLE->legv+BATTLE->legc-3,tipdistance);
  
  /* Just for aesthetic purposes, find the horizontal limits and center horizontally.
   * Better to do this now than at the end, since right now only guide points are in play.
   */
  double xlo=BATTLE->legv[0].guidex,xhi=BATTLE->legv[0].guidey;
  for (i=BATTLE->legc,leg=BATTLE->legv;i-->0;leg++) {
    if (leg->guidex<xlo) xlo=leg->guidex;
    else if (leg->guidex>xhi) xhi=leg->guidex;
  }
  double xmid=(xlo+xhi)*0.5;
  double adjust=(FBW*0.5)-xmid;
  for (i=BATTLE->legc,leg=BATTLE->legv;i-->0;leg++) {
    leg->guidex+=adjust;
  }
  first->lx=first->rx=first->guidex;
  first->ly=first->ry=first->guidey;
  last->lx=last->rx=last->guidex;
  last->ly=last->ry=last->guidey;
  
  /* Each point except the first and last now get wall points.
   * A leg's wall points are collinear to its guide point, at a fixed distance.
   * Halfway between the previous and next.
   * So at each guide point, take a unit vector to previous and next guide points.
   * Add those unit vectors and renormalize that sum.
   * Beware if previous, this, and next are collinear, we need to fudge it.
   */
  const double walldistance=20.0;
  struct leg *prev=BATTLE->legv;
  struct leg *next=BATTLE->legv+2;
  leg=BATTLE->legv+1;
  for (i=BATTLE->legc-2;i-->0;) {
  
    // Unit vectors to previous and next. Because we created them in vertical ribbons, there's always nonzero distance.
    double pdx=prev->guidex-leg->guidex;
    double pdy=prev->guidey-leg->guidey;
    double ndx=next->guidex-leg->guidex;
    double ndy=next->guidey-leg->guidey;
    double pdist=sqrt(pdx*pdx+pdy*pdy);
    double ndist=sqrt(ndx*ndx+ndy*ndy);
    pdx/=pdist;
    pdy/=pdist;
    ndx/=ndist;
    ndy/=ndist;
    
    // Add those unit vectors to get a denormalized vector in the correct wall direction. Unknown yet which wall.
    double wax=pdx+ndx;
    double way=pdy+ndy;
    double wad2=wax*wax+way*way;
    if (wad2<1.0) {
      // Previous, guide, and next are collinear. Just rotate next a quarter-turn instead. And no need to renormalize.
      wax=ndy;
      way=-ndx;
    } else {
      // Sensible wall point. Normalize it.
      double walen=sqrt(wad2);
      wax/=walen;
      way/=walen;
    }
    
    // (wa) is now a unit vector. Scale it out to the desired distance.
    wax*=walldistance;
    way*=walldistance;
    
    // Other wall is just (wa) reversed.
    double wbx=-wax;
    double wby=-way;
    
    // Sign of the cross-product of (wa) against (nd) tells us which wall is which.
    double cp=wax*ndy-way*ndx;
    if (cp<0.0) {
      leg->lx=leg->guidex+wax;
      leg->ly=leg->guidey+way;
      leg->rx=leg->guidex+wbx;
      leg->ry=leg->guidey+wby;
    } else {
      leg->lx=leg->guidex+wbx;
      leg->ly=leg->guidey+wby;
      leg->rx=leg->guidex+wax;
      leg->ry=leg->guidey+way;
    }
  
    prev=leg;
    leg=next;
    next++;
  }
  
  /* Assign gravity vectors and lengths.
   * I guess we already knew these, during the wall-placement pass above, but meh.
   */
  for (prev=0,i=BATTLE->legc,leg=BATTLE->legv;i-->0;leg++) {
    if (prev) {
      leg->nx=leg->guidex-prev->guidex;
      leg->ny=leg->guidey-prev->guidey;
      leg->len=sqrt(leg->nx*leg->nx+leg->ny*leg->ny);
      leg->nx/=leg->len;
      leg->ny/=leg->len;
    } else {
      leg->nx=leg->ny=leg->len=0.0;
    }
    prev=leg;
  }
  
  /* One more pass to center vertically, now that we have correct wall points.
   */
  double ylo=FBH*0.5,yhi=FBH*0.5;
  for (i=BATTLE->legc,leg=BATTLE->legv;i-->0;leg++) {
    if (leg->ly<ylo) ylo=leg->ly; else if (leg->ly>yhi) yhi=leg->ly;
    if (leg->ry<ylo) ylo=leg->ry; else if (leg->ry>yhi) yhi=leg->ry;
  }
  double ymid=(ylo+yhi)*0.5;
  adjust=(FBH*0.5)-ymid;
  for (i=BATTLE->legc,leg=BATTLE->legv;i-->0;leg++) {
    leg->guidey+=adjust;
    leg->ly+=adjust;
    leg->ry+=adjust;
  }
  
  return 0;
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  struct leg *leg=BATTLE->legv+1;
  const double offage=0.250; // Players start so far off center, toward one of the walls.
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=leg->guidex*(1.0-offage)+leg->lx*offage;
    player->y=leg->guidey*(1.0-offage)+leg->ly*offage;
  } else { // Right.
    player->who=1;
    player->x=leg->guidex*(1.0-offage)+leg->rx*offage;
    player->y=leg->guidey*(1.0-offage)+leg->ry*offage;
  }
  player->legp=2; // We start right at point [1], so the first target is [2].
  player->steer_accel=10.0*(1.0-player->skill)+25.0*player->skill;
  if (player->human=human) { // Human.
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xe0f6f1ff;
      } break;
    case NS_face_dot: {
        player->color=0xa668f3ff; // 0x411775ff Brighter than normal Dot, to contrast with the black line.
      } break;
    case NS_face_princess: {
        player->color=0x5d83f4ff; // 0x0d3ac1ff Same problem as Dot.
      } break;
  }
  player->sampleclock=1.000; // Can start longer than the usual sample period.
  player->samplex=player->x;
  player->sampley=player->y;
}

/* New.
 */
 
static int _bobsleigh_init(struct battle *battle) {
  if (bobsleigh_generate_track(battle)<0) return -1;
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playclock=30.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->steer=-1; break;
    case EGG_BTN_RIGHT: player->steer=1; break;
    default: player->steer=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  /* Don't overthink this. Steer toward the endpoint of my current leg.
   * This naive algorithm seems slightly better than nothing at all.
   * If you play at balanced bias and don't touch the controls, monster usually wins.
   * And more important I think: Monster vs monster, the bias almost always determines the winner.
   */
  player->steer=0;
  if ((player->legp>=0)&&(player->legp<BATTLE->legc)) {
    double pdx=player->dx;
    double pdy=player->dy;
    double pd2=pdx*pdx+pdy*pdy;
    if (pd2>=1.0) {
      const struct leg *leg=BATTLE->legv+player->legp;
      double dx=leg->guidex-player->x;
      double dy=leg->guidey-player->y;
      double t=atan2(dx,-dy);
      double pt=atan2(pdx,-pdy);
      double dt=t-pt;
      while (dt<-M_PI) dt+=M_PI*2.0;
      while (dt>M_PI) dt-=M_PI*2.0;
      const double thresh=0.010;
      if (dt>thresh) player->steer=1;
      else if (dt<-thresh) player->steer=-1;
    }
  }
}

/* Nonzero if these two segments intersect.
 */
 
static int bobsleigh_check_wall(
  double ax,double ay,
  double bx,double by,
  double wax,double way,
  double wbx,double wby
) {
  // Cross-product of (a) and (b) with respect to (wa)..(wb). If their signs match, no collision.
  double cpa=(ax-wax)*(wby-way)-(ay-way)*(wbx-wax);
  double cpb=(bx-wax)*(wby-way)-(by-way)*(wbx-wax);
  if ((cpa>=0.0)&&(cpb>=0.0)) return 0;
  if ((cpa<0.0)&&(cpb<0.0)) return 0;
  // Cross-product of (wa) and (wb) with respect to (a)..(b). If their signs match, no collision.
  cpa=(wax-ax)*(by-ay)-(way-ay)*(bx-ax);
  cpb=(wbx-ax)*(by-ay)-(wby-ay)*(bx-ax);
  if ((cpa>=0.0)&&(cpb>=0.0)) return 0;
  if ((cpa<0.0)&&(cpb<0.0)) return 0;
  // Locate the intersection. It must lie in bounds of both segments.
  // We already have denormalized rejections (cpa,cpb) of wall against travel -- these are proportionate to the intersection's position on wall.
  cpa=-cpa;
  double cpsum=cpa+cpb;
  double wt=cpa/cpsum;
  double wdx=wbx-wax;
  double wdy=wby-way;
  double wlen=sqrt(wdx*wdx+wdy*wdy);
  double ix=wax+(wdx*wt);
  double iy=way+(wdy*wt);
  // (i) should lie on both segments, possibly OOB. Check axiswise that (a<=i<=b) for both segments.
  if (ax<bx) {
    if ((ix<ax)||(ix>bx)) return 0;
  } else {
    if ((ix<bx)||(ix>ax)) return 0;
  }
  if (ay<by) {
    if ((iy<ay)||(iy>by)) return 0;
  } else {
    if ((iy<by)||(iy>ay)) return 0;
  }
  if (wax<wbx) {
    if ((ix<wax)||(ix>wbx)) return 0;
  } else {
    if ((ix<wbx)||(ix>wax)) return 0;
  }
  if (way<wby) {
    if ((iy<way)||(iy>wby)) return 0;
  } else {
    if ((iy<wby)||(iy>way)) return 0;
  }
  // OK, they intersect.
  return 1;
}

/* Bounce player off a wall, after detecting collision.
 * (player->x,y) should be the attempted position. (x0,y0) is its starting position this frame, we'll restore to there.
 */
 
static void bobsleigh_bounce_wall(struct battle *battle,struct player *player,double x0,double y0,double wax,double way,double wbx,double wby) {
  double wdx=wbx-wax;
  double wdy=wby-way;
  double wlen=sqrt(wdx*wdx+wdy*wdy);
  wdx/=wlen;
  wdy/=wlen;
  double wnx=-wdy;
  double wny=wdx;
  double proj=player->dx*wnx+player->dy*wny;
  const double bounce=1.500; // 1..2
  const double penalty=0.800;
  player->dx-=wnx*proj*bounce;
  player->dy-=wny*proj*bounce;
  player->dx*=penalty;
  player->dy*=penalty;
  player->x=x0;
  player->y=y0;
  if (proj<0.0) proj=-proj;
  if (proj>=10.0) bm_sound_pan(RID_sound_bump,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (!player->finished) {
    player->runtime+=elapsed;
    if ((player->sampleclock-=elapsed)<=0.0) {
      player->sampleclock+=0.500;
      const double minvel=2.0; // pixels per the period just above
      double dx=player->x-player->samplex;
      double dy=player->y-player->sampley;
      double d2=dx*dx+dy*dy;
      if (d2<minvel*minvel) {
        //fprintf(stderr,"player %d at %f,%f, emergency measures\n",player->who,player->x,player->y);
        if ((player->legp>=1)&&(player->legp<BATTLE->legc)) {
          struct leg *leg=BATTLE->legv+player->legp;
          player->dx=leg->guidex-player->x;
          player->dy=leg->guidey-player->y;
          double len=sqrt(player->dx*player->dx+player->dy*player->dy);
          double scale=20.0/len;
          player->dx*=scale;
          player->dy*=scale;
        }
      }
      player->samplex=player->x;
      player->sampley=player->y;
    }
  }
  
  /* Accelerate per focussed leg.
   * When our projection on that leg exceeds 1, step to the next leg. But clamp at the end of the track.
   */
  if ((player->legp>=0)&&(player->legp<BATTLE->legc)) {
    const struct leg *leg=BATTLE->legv+player->legp;
    if (player->legp>0) {
      const struct leg *prev=leg-1;
      double proj=((player->x-prev->guidex)*(leg->guidex-prev->guidex)+(player->y-prev->guidey)*(leg->guidey-prev->guidey))/leg->len;
      if (proj>=leg->len) {
        player->legp++;
        if (player->legp>=BATTLE->legc-1) {
          player->legp=BATTLE->legc-1;
          if (!player->finished) {
            player->finished=1;
            bm_sound_pan(RID_sound_treasure,player->who?PLAYER_PAN:-PLAYER_PAN);
            if (BATTLE->playclock>5.0) BATTLE->playclock=5.0;
          }
        }
        leg=BATTLE->legv+player->legp;
      } else if (proj<0.0) { // Totally possible to bounce backward to a previous leg.
        if (player->legp>1) player->legp--;
        leg=BATTLE->legv+player->legp;
      }
    }
    const double accel=10.0;
    player->dx+=leg->nx*elapsed*accel;
    player->dy+=leg->ny*elapsed*accel;
  }
  
  /* Apply steering.
   * Effecting this as acceleration perpendicular to the current direction of travel.
   */
  if (player->steer) { //&&(player->legp>0)&&(player->legp<BATTLE->legc)) {
    double dx=player->dx,dy=player->dy;
    double d2=player->dx*player->dx+player->dy*player->dy;
    if (d2>1.0) {
      //const struct leg *leg=BATTLE->legv+player->legp;
      double nx,ny;
      if (player->steer<0) {
        nx=player->dy;
        ny=-player->dx;
      } else {
        nx=-player->dy;
        ny=player->dx;
      }
      double vel=sqrt(d2);
      nx/=vel;
      ny/=vel;
      player->dx+=nx*elapsed*player->steer_accel;
      player->dy+=ny*elapsed*player->steer_accel;
    }
  }
  
  /* Apply velocity.
   */
  double x0=player->x;
  double y0=player->y;
  player->x+=player->dx*elapsed;
  player->y+=player->dy*elapsed;
  
  /* Detect collisions against the walls.
   * If we collide: Rectify, bounce, and apply a heavy velocity penalty.
   * Hitting walls is the main driver of failure in this game.
   * We could narrow this and only check the two or three legs that might be in play but that gets complicated.
   * Try checking all walls every time and see how bad the performance impact is.
   * Another tricky problem: The walls form a concave polygon. I don't know how to account for the negative parts.
   * So instead I'm dumbing it down and just looking for intersections between this frame's travel and each wall segment.
   * When a collision is detected:
   *  - Rectify all the way back to the starting point. It will always be close.
   *  - Update (dx,dy) to reflect off the wall.
   *  - Reduce (dx,dy) as a penalty.
   *  - No more than one collision per frame.
   */
  double dx=player->x-x0;
  double dy=player->y-y0;
  double d2=dx*dx+dy*dy;
  if (d2>0.0) {
    struct leg *prev=BATTLE->legv;
    struct leg *leg=prev+1;
    int i=BATTLE->legc-1,legid=1;
    for (;i-->0;leg++,prev++,legid++) {
      if (bobsleigh_check_wall(x0,y0,player->x,player->y,prev->lx,prev->ly,leg->lx,leg->ly)) {
        bobsleigh_bounce_wall(battle,player,x0,y0,prev->lx,prev->ly,leg->lx,leg->ly);
        break;
      }
      if (bobsleigh_check_wall(x0,y0,player->x,player->y,prev->rx,prev->ry,leg->rx,leg->ry)) {
        bobsleigh_bounce_wall(battle,player,x0,y0,prev->rx,prev->ry,leg->rx,leg->ry);
        break;
      }
    }
  }
  
  // Not doing player-on-player collisions.
}

/* Update.
 */
 
static void _bobsleigh_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Race is over when both players have reached (legp==legc-1).
   * It is possible for players to get stuck, so we'll want a dead-man clock too.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->finished&&r->finished) {
    if (l->runtime<r->runtime) battle->outcome=1;
    else if (l->runtime>r->runtime) battle->outcome=-1;
    else battle->outcome=0;
  } else if ((BATTLE->playclock-=elapsed)<=0.0) {
    if (l->finished) battle->outcome=1;
    else if (r->finished) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int dstx=(int)player->x;
  int dsty=(int)player->y;
  int nx,ny; // Nose, relative to (dstx,dsty). Not normalized.
  const double nose_radius=4.0;
  double dx=player->dx,dy=player->dy;
  double d2=dx*dx+dy*dy;
  if (d2<1.0) { // Very slow, just point down.
    nx=0;
    ny=(int)nose_radius;
    dx=0.0;
    dy=1.0;
  } else {
    double len=sqrt(d2);
    double fnx=player->dx/len;
    double fny=player->dy/len;
    nx=lround(fnx*nose_radius);
    ny=lround(fny*nose_radius);
  }
  graf_set_image(&g.graf,RID_image_icepalace_sprites);
  graf_set_filter(&g.graf,1);
  double t=atan2(dx,-dy);
  uint8_t rotation=(int8_t)((t*128.0)/M_PI);
  graf_fancy(&g.graf,dstx,dsty,0x1e,0,rotation,NS_sys_tilesize,0,player->color);
  graf_set_filter(&g.graf,0);
}

/* Render clock.
 */
 
static void bobsleigh_render_clock(int x,int y,int align,double fs,uint32_t color) {
  graf_set_tint(&g.graf,color);
  graf_set_image(&g.graf,RID_image_fonttiles);
  char text[8]={'0',':','0','0','.','0','0','0'}; // Include leading "0:" as a hint that it's a time. We actually stop after 20 seconds.
  int ms=(int)(fs*1000.0);
  if (ms<0) ms=0;
  int sec=ms/1000; ms%=1000;
  if (sec>=60) {
    text[0]=text[2]=text[3]=text[5]=text[6]=text[7]='9';
  } else {
    text[2]+=sec/10;
    text[3]+=sec%10;
    text[5]+=ms/100;
    text[6]+=(ms/10)%10;
    text[7]+=ms%10;
  }
  int textp,textdp,dx;
  if (align<0) {
    textp=0;
    textdp=1;
    dx=8;
  } else {
    textp=7;
    textdp=-1;
    dx=-8;
  }
  int i=8; for (;i-->0;textp+=textdp,x+=dx) {
    graf_tile(&g.graf,x,y,text[textp],0);
  }
  graf_set_tint(&g.graf,0);
}

/* Render.
 */
 
static void _bobsleigh_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  
  /* Start and finish lines.
   */
  if (BATTLE->legc>=3) {
    struct leg *leg=BATTLE->legv+1;
    graf_line(&g.graf,(int)leg->lx,(int)leg->ly,0x003090ff,(int)leg->rx,(int)leg->ry,0x003090ff);
    leg=BATTLE->legv+BATTLE->legc-2;
    graf_line(&g.graf,(int)leg->lx,(int)leg->ly,0xc0c0c0ff,(int)leg->rx,(int)leg->ry,0xc0c0c0ff);
  }
  
  /* Trace the walls.
   */
  if (BATTLE->legc>0) {
    graf_line_strip_begin(&g.graf,(int)BATTLE->legv[0].lx,(int)BATTLE->legv[0].ly,0xffffffff);
    struct leg *leg=BATTLE->legv+1;
    int i=BATTLE->legc-1;
    for (;i-->0;leg++) {
      graf_line_strip_more(&g.graf,(int)leg->lx,(int)leg->ly,0xffffffff);
    }
    for (i=BATTLE->legc,leg=BATTLE->legv+BATTLE->legc-1;i-->0;leg--) {
      graf_line_strip_more(&g.graf,(int)leg->rx,(int)leg->ry,0xffffffff);
    }
  }
  
  /* Players.
   */
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  /* Clocks.
   */
  bobsleigh_render_clock(6,6,-1,BATTLE->playerv[0].runtime,BATTLE->playerv[0].color);
  bobsleigh_render_clock(FBW-6,6,1,BATTLE->playerv[1].runtime,BATTLE->playerv[1].color);
}

/* Type definition.
 */
 
const struct battle_type battle_type_bobsleigh={
  .name="bobsleigh",
  .objlen=sizeof(struct battle_bobsleigh),
  .id=NS_battle_bobsleigh,
  .strix_name=230,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_bobsleigh_del,
  .init=_bobsleigh_init,
  .update=_bobsleigh_update,
  .render=_bobsleigh_render,
};

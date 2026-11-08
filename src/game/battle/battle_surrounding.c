/* battle_surrounding.c
 */

#include "game/batsup/battle_internal.h"

/* Player wins instantly if their snake maxes out.
 * This is long enough that I doubt it will ever happen. Certainly not with a CPU player involved.
 */
#define SNEGMENT_LIMIT 256

struct battle_surrounding {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double x,y; // Head position in framebuffer pixels.
    double t; // Head angle. Zero is rightward.
    int indt;
    int oob; // Nonzero, I lose. For both boundaries and snake collisions.
    double speed; // px/sec
    double rspeed; // rad/sec
    
    struct snegment {
      double x,y,t;
    } snegmentv[SNEGMENT_LIMIT];
    int snegmentc;
  } playerv[2];
  
  struct damage {
    int x,y,animframe;
    double animclock;
  } damagev[2];
  int damagec;
};

#define BATTLE ((struct battle_surrounding*)battle)

/* Delete.
 */
 
static void _surrounding_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->y=FBH*0.5;
  player->x=FBW*0.25;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
  }
  player->t=((rand()&0xffff)*M_PI*2.0)/65535.0-M_PI;
  
  player->speed=80.0*(1.0-player->skill)+30.0*player->skill;
  player->rspeed=2.500*(1.0-player->skill)+4.000*player->skill;
  
  if (player->human=human) { // Human.
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xc46b24ff;
        player->tileid=0xfe;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0xde;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0xee;
      } break;
  }
  
  // Players start with one snegment, the list is never empty.
  struct snegment *sneg=player->snegmentv+player->snegmentc++;
  sneg->x=player->x;
  sneg->y=player->y;
  sneg->t=player->t;
}

/* New.
 */
 
static int _surrounding_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indt=-1; break;
    case EGG_BTN_RIGHT: player->indt=1; break;
    default: player->indt=0; break;
  }
}

/* Measure distance to a point along a normal. Translate (q) before calling, to the reference origin.
 * <0 if it's outside the collision radius.
 */
 
static double surrounding_check_point(double qx,double qy,double nx,double ny) {
  double proj=qx*nx+qy*ny;
  if (proj<0.0) return -1.0; // Behind me.
  double cp=qx*ny-qy*nx;
  if (cp<0.0) cp=-cp; // We don't care which side it's on.
  if (cp>=10.0) return -1.0; // Outside the collision radius.
  return proj;
}

/* Measure freedom from the player's head in a given direction.
 */
 
static double surrounding_measure_freedom(struct battle *battle,struct player *player,double t) {
  /* We're looking for the smallest positive scalar projection, where scalar rejection is below the collision radius.
   * In theory, we should omit the head and neck of this snake, but in practice, we're only testing forward (t) so they get skipped anyway.
   */
  double freedom=999.999;
  double nx=cos(t);
  double ny=sin(t);
  struct player *qplayer=BATTLE->playerv;
  int qplayeri=2;
  for (;qplayeri-->0;qplayer++) {
    struct snegment *sneg=qplayer->snegmentv;
    int snegi=qplayer->snegmentc;
    for (;snegi-->0;sneg++) {
      double q=surrounding_check_point(sneg->x-player->x,sneg->y-player->y,nx,ny);
      if ((q>1.0)&&(q<freedom)) freedom=q;
    }
    double q=surrounding_check_point(qplayer->x-player->x,qplayer->y-player->y,nx,ny);
    if ((q>1.0)&&(q<freedom)) freedom=q;
  }
  // Edges of course also count.
  const double margin=8.0;
  if ((nx<-0.001)||(nx>0.001)) {
    double dx=(nx<0.0)?(player->x-margin):(FBW-player->x-margin);
    double dy=(dx*ny)/nx;
    double q=sqrt(dx*dx+dy*dy);
    if (q<freedom) freedom=q;
  }
  if ((ny<-0.001)||(ny>0.001)) {
    double dy=(ny<0.0)?(player->y-margin):(FBH-player->y-margin);
    double dx=(dy*nx)/ny;
    double q=sqrt(dx*dx+dy*dy);
    if (q<freedom) freedom=q;
  }
  return freedom;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  /* This is a tough one! I honestly don't know what a correct solution looks like.
   * Try a simple heuristic: Trace some number of rays from the snake's head and turn toward whichever has the most freedom.
   */
  #define RAYC 5
  const double range=(M_PI*9.0)/10.0; // Width of test field in radians. Will center on our face direction.
  const double stept=range/(RAYC-1);
  double testt=player->t-range*0.5;
  double bestt=player->t,bestd=0.0;
  int i=RAYC;
  for (;i-->0;testt+=stept) {
    double testd=surrounding_measure_freedom(battle,player,testt);
    if (testd>bestd) {
      bestt=testt;
      bestd=testd;
    }
  }
  #undef RAYC
  
  /* Then the easy bit, turn toward (bestt).
   */
  const double tolerance=0.100;
  double dt=bestt-player->t;
  while (dt>M_PI) dt-=M_PI*2.0;
  while (dt<-M_PI) dt+=M_PI*2.0;
  if (dt>tolerance) player->indt=1;
  else if (dt<-tolerance) player->indt=-1;
  else player->indt=0;
}

/* Add a damage indicator.
 */
 
static void surrounding_damage(struct battle *battle,int x,int y) {
  if (BATTLE->damagec>=2) return;
  struct damage *damage=BATTLE->damagev+BATTLE->damagec++;
  damage->animclock=0.100;
  damage->animframe=0;
  damage->x=x;
  damage->y=y;
  bm_sound_pan(RID_sound_ouch,0.0);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // If we are snaked to the max, stop updating. Wouldn't want to pull our head off.
  if (battle->outcome>-2) return;
  if (player->snegmentc>=SNEGMENT_LIMIT) return;
  if (player->oob) return;

  if (player->indt) {
    player->t+=player->indt*elapsed*player->rspeed;
    if (player->t<-M_PI) player->t+=M_PI*2.0;
    else if (player->t>M_PI) player->t-=M_PI*2.0;
  }
  
  double sint=sin(player->t);
  double cost=cos(player->t);
  player->x+=player->speed*elapsed*cost;
  player->y+=player->speed*elapsed*sint;
  if ((player->x<0.0)||(player->y<0.0)||(player->x>FBW)||(player->y>FBH)) {
    player->oob=1;
    surrounding_damage(battle,(int)player->x,(int)player->y);
    return;
  }
  
  /* The last snegment in the list is special, it rotates to follow the head.
   * But its position is fixed like the others.
   * There is always at least one.
   */
  struct snegment *sneg=player->snegmentv+player->snegmentc-1;
  double dx=player->x-sneg->x;
  double dy=player->y-sneg->y;
  sneg->t=atan2(dy,dx);
  
  /* If the distance to our neck crosses some threshold, add a new snegment.
   */
  double d2=dx*dx+dy*dy;
  if (d2>=100.0) {
    struct snegment *neck=player->snegmentv+player->snegmentc++;
    neck->x=player->x;
    neck->y=player->y;
    neck->t=player->t;
  }
  
  /* Check collisions against my own snegments, not including the one neck snegment.
   * They present as rectangles, but are in fact circles.
   */
  const double r=10.0;
  const double r2=r*r;
  int i=player->snegmentc-1;
  for (sneg=player->snegmentv;i-->0;sneg++) {
    dx=player->x-sneg->x;
    dy=player->y-sneg->y;
    d2=dx*dx+dy*dy;
    if (d2<r2) {
      player->oob=1;
      surrounding_damage(battle,(int)((player->x+sneg->x)*0.5),(int)((player->y+sneg->y)*0.5));
      return;
    }
  }
  
  /* Then the same idea against the other snake, including its neck.
   */
  struct player *other=BATTLE->playerv+(player->who?0:1);
  for (i=other->snegmentc,sneg=other->snegmentv;i-->0;sneg++) {
    dx=player->x-sneg->x;
    dy=player->y-sneg->y;
    d2=dx*dx+dy*dy;
    if (d2<r2) {
      player->oob=1;
      surrounding_damage(battle,(int)((player->x+sneg->x)*0.5),(int)((player->y+sneg->y)*0.5));
      return;
    }
  }
  
  /* And finally, if we collide with other's head, we're both at fault.
   * This will happen, and fast, if players don't touch the controls.
   */
  dx=player->x-other->x;
  dy=player->y-other->y;
  d2=dx*dx+dy*dy;
  if (d2<r2) {
    player->oob=1;
    other->oob=1;
    surrounding_damage(battle,(int)((player->x+other->x)*0.5),(int)((player->y+other->y)*0.5));
    return;
  }
}

/* Update.
 */
 
static void _surrounding_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  struct damage *damage=BATTLE->damagev;
  for (i=BATTLE->damagec;i-->0;damage++) {
    if ((damage->animclock-=elapsed)<=0.0) {
      damage->animclock+=0.200;
      if (++(damage->animframe)>=2) damage->animframe=0;
    }
  }
  
  /* Battle ends if a player goes oob or maxes out. Oob includes snake collisions.
   * Only oob counts toward victory. If one player maxes, it's a tie.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->oob||r->oob||(l->snegmentc>=SNEGMENT_LIMIT)||(r->snegmentc>=SNEGMENT_LIMIT)) {
    if (l->oob&&r->oob) battle->outcome=0; // eg head-on collision, happens by default
    else if (l->oob) battle->outcome=-1;
    else if (r->oob) battle->outcome=1;
    else battle->outcome=0; // tie due to max
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  graf_set_filter(g_graf,1);
  
  // Snegments back to front, in the hopes that each will cover the butt line of the previous.
  int i=player->snegmentc;
  struct snegment *sneg=player->snegmentv+i-1;
  for (;i-->0;sneg--) {
    uint8_t rot=(int8_t)((sneg->t*128.0)/M_PI);
    graf_fancy(g_graf,(int)sneg->x,(int)sneg->y,player->tileid+1,0,rot,NS_sys_tilesize,0,player->color);
  }
  
  // Head.
  uint8_t rot=(int8_t)((player->t*128.0)/M_PI);
  graf_fancy(g_graf,(int)player->x,(int)player->y,player->tileid,0,rot,NS_sys_tilesize,0,player->color);
  
  graf_set_filter(g_graf,0);
}

/* Render.
 */
 
static void _surrounding_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(g_graf,RID_image_battle_underground);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  struct damage *damage=BATTLE->damagev;
  int i=BATTLE->damagec;
  for (;i-->0;damage++) {
    graf_tile(g_graf,damage->x,damage->y,0xfc+damage->animframe,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_surrounding={
  .name="surrounding",
  .objlen=sizeof(struct battle_surrounding),
  .id=122,
  .strix_name=404,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz,
  .imageid_default=RID_image_caves,
  .del=_surrounding_del,
  .init=_surrounding_init,
  .update=_surrounding_update,
  .render=_surrounding_render,
};

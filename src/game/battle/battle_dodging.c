/* battle_dodging.c
 */

#include "game/bellacopia.h"

#define GUN_LIMIT 32
#define BULLET_LIMIT 128

#define PLAYER_MARGIN 20.0 /* Players restricted to so far from edges. Important! Guns are a little off the edge too; don't let them cheat on the edges. */

#define GUN_ENTER_TIME 0.500
#define GUN_EXIT_TIME 0.500
#define GUN_ANGLE_RANGE (M_PI*0.200) /* How far off cardinal can the guns aim? Either direction. <pi/2 */
#define GUN_MARGIN 8 /* How far we rest, from edge of screen. */
#define GUN_OFF_MARGIN 16 /* Minimum distance from bounding edges. */
#define GUN_PERIOD_INITIAL 0.300
#define GUN_PERIOD_FINAL 0.100
#define GUN_PERIOD_DELTA 0.020

#define GUN_STAGE_ENTER 1
#define GUN_STAGE_MENACE 2
#define GUN_STAGE_HOLD 3
#define GUN_STAGE_EXIT 4
#define GUN_STAGE_LASER 5

struct battle_dodging {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    int face;
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    int indx,indy;
    double animclock;
    int animframe;
    double x,y; // Framebuffer pixels.
    double walkspeed;
    int dead;
    double soulballradius;
    double soulballclock;
    int soulballframe;
    double bullet_distance2;
    double laser_distance;
    double cpuckclock;
    double cpucktime;
    double cpux,cpuy; // Aim for here.
    double suicide_clock;
  } playerv[2];
  
  struct gun {
    double x,y; // Framebuffer pixels of final position, constant.
    double t; // Final angle, 0=up.
    double presence; // 0..1 = offscreen..ready
    int edgex,edgey; // Unit vector describing my screen edge.
    int stage; // GUN_STAGE_*
    double clock; // Counts down to next stage.
    double t0; // Initial angle (could infer from edgex,edgey)
    uint8_t tileid; // 0xf0,0xf1 = rifle,laser
    int lx,ly; // Laser far position.
  } gunv[GUN_LIMIT];
  int gunc;
  
  double gunclock;
  double gunperiod;
  int laser_odds; // 0..0xffff
  int laser_odds_increase;
  
  struct bullet {
    double x,y;
    double dx,dy;
    uint8_t rot;
  } bulletv[BULLET_LIMIT];
  int bulletc;
};

#define BATTLE ((struct battle_dodging*)battle)

/* Delete.
 */
 
static void _dodging_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.333;
  } else { // Right.
    player->who=1;
    player->x=FBW*0.666;
    player->xform=EGG_XFORM_XREV;
  }
  player->y=FBH*0.5;
  
  player->walkspeed=50.0*(1.0-player->skill)+120.0*player->skill; // px/s
  player->bullet_distance2=36.0;
  player->laser_distance=5.0;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cpuckclock=0.0;
    player->cpucktime=0.200; // Don't scan the whole field every frame. Maybe reduce per skill?
    player->cpux=player->x;
    player->cpuy=player->y;
    player->suicide_clock=9.0+player->skill*5.0; // After a few seconds, we calmly walk thru the sea of bullets to wait for death.
  }
  
  switch (player->face=face) {
    case NS_face_monster: {
        player->color=0xaf620cff;
        player->tileid=0x82;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x56;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x58;
      } break;
  }
}

/* New.
 */
 
static int _dodging_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  BATTLE->gunperiod=GUN_PERIOD_INITIAL;
  BATTLE->gunclock=BATTLE->gunperiod;
  BATTLE->laser_odds=0;
  BATTLE->laser_odds_increase=100; // After 650 guns, it's all lasers and should become impossible to survive.
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
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
}

/* Assess danger at a given point, for the CPU.
 * 0<=danger<=1
 */
 
static double dodging_assess_danger(struct battle *battle,double x,double y) {
  int i;
  struct gun *gun;
  struct bullet *bullet;
  double danger=0.0;
  
  /* If it's OOB, call it maximum danger, don't try to go this way.
   */
  if (x<PLAYER_MARGIN) return 1.0;
  if (y<PLAYER_MARGIN) return 1.0;
  if (x>FBW-PLAYER_MARGIN) return 1.0;
  if (y>FBH-PLAYER_MARGIN) return 1.0;
  
  /* Check all bullets.
   * If our scalar projection <0, ignore it.
   * Rejection above some threshold (close to our radius), ignore it.
   * Otherwise, more dangerous for lower projection.
   */
  for (i=BATTLE->bulletc,bullet=BATTLE->bulletv;i-->0;bullet++) {
    double ax=x-bullet->x;
    double ay=y-bullet->y;
    double proj=ax*bullet->dx+ay*bullet->dy;
    if (proj<0.0) continue; // It's past us, might as well not exist.
    double len=sqrt(bullet->dx*bullet->dx+bullet->dy*bullet->dy);
    if (len<=0.0) continue;
    proj/=len;
    proj=1.0-(proj/FBH); // Normalize projection against screen's height, just to have some fixed reference.
    if (proj<=danger) continue; // Far away, don't worry about it.
    double rej=(ax*bullet->dy-ay*bullet->dx)/len;
    if ((rej<-10.0)||(rej>10.0)) continue; // Far away perpendicularly, don't worry about it.
    if (proj>=1.0) return 1.0; // Extremely close or my math is screwy -- report maximum danger here.
    danger=proj;
  }
  
  /* Check all guns.
   * If it's in the HOLD or EXIT stage, ignore it.
   * Otherwise project onto its line of sight like we did for bullets.
   * If it's a laser, don't bother taking the projection -- all points along the line of sight are maximum danger.
   */
  for (i=BATTLE->gunc,gun=BATTLE->gunv;i-->0;gun++) {
    if (gun->stage==GUN_STAGE_HOLD) continue; // spent.
    if (gun->stage==GUN_STAGE_EXIT) continue; // spent.
    double gundx=sin(gun->t);
    double gundy=-cos(gun->t);
    double ax=x-gun->x;
    double ay=y-gun->y;
    double rej=ax*gundy-ay*gundx;
    if ((rej<-12.0)||(rej>12.0)) continue; // Far away perpendicularly, don't worry about it.
    if (gun->tileid==0xf1) { // Laser: Consider the line of sight extremely dangerous regardless of distance.
      return 1.0;
    } else { // Rifle: Weigh the distance too; we can outrun bullets.
      double proj=ax*gundx+ay*gundy;
      if (proj<0.0) continue; // Shouldn't be possible, but ok ignore it.
      proj=1.0-(proj/FBH); // Normalize projection against screen's height, just to have some fixed reference.
      if (proj<=danger) continue; // Far away, don't worry about it.
      if (proj>=1.0) return 1.0; // Extremely close or my math is screwy -- report maximum danger here.
      danger=proj;
    }
  }
  
  return danger;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (battle->outcome>-2) return;
  
  player->suicide_clock-=elapsed;

  /* When (cpuckclock) expires, reassess the field.
   */
  if ((player->cpuckclock-=elapsed)<=0.0) {
    player->cpuckclock+=player->cpucktime;
    
    /* If our suicide clock has expired, just walk back to where we started.
     * If our opponent survives this long, he earned a win.
     */
    if (player->suicide_clock<=0.0) {
      player->cpux=player->who?(FBW*0.666):(FBW*0.333);
      player->cpuy=FBH*0.5;
    } else {
      /* Consider nine points: Where I am, and a circle of cardinals and diagonals at some closeish distance.
       * We'll call out to compute the danger at each point, and walk towards the safest.
       * Aim to put these points just a little further than the further we can walk in an assessment cycle.
       */
      const double dcard=24.0;
      double ddiag=dcard*M_SQRT1_2;
      double safex=player->x,safey=player->y,safescore=999.999;
      const int zerofirst[]={0,1,-1}; // Arrange for "no movement" to get checked first so it wins ties.
      int xp=0; for (;xp<3;xp++) {
        int dx=zerofirst[xp];
        int yp=0; for (;yp<3;yp++) {
          int dy=zerofirst[yp];
          double x=player->x,y=player->y;
          if (dx&&dy) {
            x+=ddiag*dx;
            y+=ddiag*dy;
          } else {
            x+=dcard*dx;
            y+=dcard*dy;
          }
          double score=dodging_assess_danger(battle,x,y);
          if (score<safescore) {
            safex=x;
            safey=y;
            safescore=score;
          }
        }
      }
      player->cpux=safex;
      player->cpuy=safey;
    }
  }
  
  /* Advance toward (cpux,cpuy).
   * Allow some tolerance to prevent jitter. It needn't be exact.
   */
  const double tolerance=3.0;
  if (player->x<player->cpux-tolerance) player->indx=1;
  else if (player->x>player->cpux+tolerance) player->indx=-1;
  else player->indx=0;
  if (player->y<player->cpuy-tolerance) player->indy=1;
  else if (player->y>player->cpuy+tolerance) player->indy=-1;
  else player->indy=0;
}

/* Kill player.
 */
 
static void player_die(struct battle *battle,struct player *player) {
  if (player->dead) return;
  player->dead=1;
  bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->soulballradius=0.0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // If we're dead, advance the soulballs and nothing else.
  if (player->dead) {
    player->soulballradius+=80.0*elapsed;
    if ((player->soulballclock-=elapsed)<=0.0) {
      player->soulballclock+=0.125;
      if (++(player->soulballframe)>=6) player->soulballframe=0;
    }
    return;
  }
  
  // Animate whenever alive, whether moving or not.
  if ((player->animclock-=elapsed)<=0.0) {
    player->animclock+=0.200;
    if (++(player->animframe)>=2) player->animframe=0;
  }

  /* Neutralize and return if finished.
   * Importantly, this means after one player dies, the other can't.
   * But they are still able to die on the same frame.
   * (and that's not as improbable as it sounds, lasers could do it).
   */
  if (battle->outcome>-2) {
    player->indx=player->indy=0;
    return;
  }

  // Move.
  if (player->indx||player->indy) {
    player->x+=player->indx*player->walkspeed*elapsed;
    player->y+=player->indy*player->walkspeed*elapsed;
    if (player->x<PLAYER_MARGIN) player->x=PLAYER_MARGIN;
    else if (player->x>FBW-PLAYER_MARGIN) player->x=FBW-PLAYER_MARGIN;
    if (player->y<PLAYER_MARGIN) player->y=PLAYER_MARGIN;
    else if (player->y>FBH-PLAYER_MARGIN) player->y=FBH-PLAYER_MARGIN;
    if (player->indx<0) player->xform=EGG_XFORM_XREV;
    else if (player->indx>0) player->xform=0;
  }
  
  /* Check bullet collisions.
   */
  struct bullet *bullet=BATTLE->bulletv;
  int i=BATTLE->bulletc;
  for (;i-->0;bullet++) {
    double dx=bullet->x-player->x;
    double dy=bullet->y-player->y;
    double d2=dx*dx+dy*dy;
    if (d2<player->bullet_distance2) {
      player_die(battle,player);
      return;
    }
  }
  
  /* Check laser collisions.
   */
  struct gun *gun=BATTLE->gunv;
  for (i=BATTLE->gunc;i-->0;gun++) {
    if (gun->stage!=GUN_STAGE_LASER) continue;
    /* Translate my position and the laser's far position against the gun's reference position.
     * Then take their cross product.
     * Length of a laser line is always (FBW+FBH), see gun_update(). So we can hard-code that for efficiency's sake.
     */
    double ax=gun->lx-gun->x;
    double ay=gun->ly-gun->y;
    double bx=player->x-gun->x;
    double by=player->y-gun->y;
    double cp=ax*by-ay*bx;
    double rej=cp/(FBW+FBH);
    if ((rej>-player->laser_distance)&&(rej<player->laser_distance)) {
      player_die(battle,player);
      return;
    }
  }
}

/* Spawn a bullet.
 */
 
static struct bullet *dodging_spawn_bullet(struct battle *battle,struct gun *gun) {
  if (BATTLE->bulletc>=BULLET_LIMIT) return 0;
  struct bullet *bullet=BATTLE->bulletv+BATTLE->bulletc++;
  
  double speed=(rand()&0xffff)/65535.0;
  speed=50.0*(1.0-speed)+100.0*speed;
  
  bullet->x=gun->x;
  bullet->y=gun->y;
  bullet->rot=(int8_t)((gun->t*128.0)/M_PI);
  bullet->dx=sin(gun->t);
  bullet->dy=-cos(gun->t);
  
  // Slide out along the barrel a little. Then bake speed into (dx,dy).
  bullet->x+=bullet->dx*5.0;
  bullet->y+=bullet->dy*5.0;
  bullet->dx*=speed;
  bullet->dy*=speed;
  
  double pan=(bullet->x*2.0)/FBW-1.0;
  bm_sound_pan(RID_sound_tick,pan);

  return bullet;
}

/* Update bullet.
 * Return nonzero to delete.
 */
 
static int bullet_update(struct battle *battle,struct bullet *bullet,double elapsed) {
  const double EDGE=6.0;
  bullet->x+=bullet->dx*elapsed;
  bullet->y+=bullet->dy*elapsed;
  if (bullet->x<-EDGE) return -1;
  if (bullet->x>FBW+EDGE) return -1;
  if (bullet->y<-EDGE) return -1;
  if (bullet->y>FBH+EDGE) return -1;
  return 0;
}

/* Spawn a gun.
 */
 
static struct gun *dodging_spawn_gun(struct battle *battle) {
  if (BATTLE->gunc>=GUN_LIMIT) return 0;
  struct gun *gun=BATTLE->gunv+BATTLE->gunc++;
  
  gun->presence=0.0;
  gun->stage=GUN_STAGE_ENTER;
  gun->clock=GUN_ENTER_TIME;
  
  // Am I a laser?
  if ((rand()&0xffff)<BATTLE->laser_odds) {
    gun->tileid=0xf1;
  } else {
    gun->tileid=0xf0;
  }
  if ((BATTLE->laser_odds+=BATTLE->laser_odds_increase)>0xffff) BATTLE->laser_odds=0xffff;
  
  // Pick an edge, position, and angle at random.
  gun->edgex=gun->edgey=0;
  switch (rand()&3) {
    case 0: gun->edgex=-1; gun->x=    GUN_MARGIN; gun->t=M_PI* 0.5; break;
    case 1: gun->edgex= 1; gun->x=FBW-GUN_MARGIN; gun->t=M_PI*-0.5; break;
    case 2: gun->edgey=-1; gun->y=    GUN_MARGIN; gun->t=M_PI* 1.0; break;
    case 3: gun->edgey= 1; gun->y=FBH-GUN_MARGIN; gun->t=M_PI* 0.0; break;
  }
  if (gun->edgex) {
    gun->y=GUN_OFF_MARGIN+(rand()%(FBH-GUN_OFF_MARGIN*2));
  } else {
    gun->x=GUN_OFF_MARGIN+(rand()%(FBW-GUN_OFF_MARGIN*2));
  }
  gun->t0=gun->t;
  gun->t+=((rand()&0xffff)*GUN_ANGLE_RANGE*2)/65535.0-GUN_ANGLE_RANGE;
  
  // (lx,ly) are populated at all times, for rifles as well as lasers. We use for the warning line.
  double radius=FBW+FBH;
  gun->lx=(int)(gun->x+radius*sin(gun->t));
  gun->ly=(int)(gun->y-radius*cos(gun->t));

  return gun;
}

/* Update gun.
 * Return nonzero to delete.
 */
 
static int gun_update(struct battle *battle,struct gun *gun,double elapsed) {

  // Tick clock and advance stage on expiry.
  if ((gun->clock-=elapsed)<=0.0) switch (gun->stage) {
    case GUN_STAGE_ENTER: {
        gun->stage=GUN_STAGE_MENACE;
        gun->clock=0.500;
        gun->presence=1.0;
      } break;
    case GUN_STAGE_MENACE: {
        if (gun->tileid==0xf1) { // Laser.
          bm_sound(RID_sound_laser); // Don't pan; laser blasts happen everywhere at once.
          gun->stage=GUN_STAGE_LASER;
          gun->clock=0.666;
        } else { // Rifle.
          dodging_spawn_bullet(battle,gun);
          gun->stage=GUN_STAGE_HOLD;
          gun->clock=0.250; // Constant is probably ok.
        }
      } break;
    case GUN_STAGE_HOLD: {
        gun->stage=GUN_STAGE_EXIT;
        gun->clock=GUN_EXIT_TIME;
      } break;
    case GUN_STAGE_LASER: {
        gun->stage=GUN_STAGE_HOLD;
        gun->clock=0.250;
      } break;
    default: return -1;
    
  // Update presence for ENTER and EXIT stages.
  } else switch (gun->stage) {
    case GUN_STAGE_ENTER: {
        gun->presence=1.0-gun->clock/GUN_ENTER_TIME;
        if (gun->presence<0.0) gun->presence=0.0; else if (gun->presence>1.0) gun->presence=1.0;
      } break;
    case GUN_STAGE_EXIT: {
        gun->presence=gun->clock/GUN_EXIT_TIME;
        if (gun->presence<0.0) gun->presence=0.0; else if (gun->presence>1.0) gun->presence=1.0;
      } break;
  }
  return 0;
}

/* Update.
 */
 
static void _dodging_update(struct battle *battle,double elapsed) {
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Spawn a gun?
   */
  if (battle->outcome==-2) {
    if ((BATTLE->gunperiod-=GUN_PERIOD_DELTA*elapsed)<GUN_PERIOD_FINAL) BATTLE->gunperiod=GUN_PERIOD_FINAL;
    if ((BATTLE->gunclock-=elapsed)<=0.0) {
      BATTLE->gunclock+=BATTLE->gunperiod;
      dodging_spawn_gun(battle);
    }
  }
  
  /* Update guns.
   */
  struct gun *gun=BATTLE->gunv+BATTLE->gunc-1;
  for (i=BATTLE->gunc;i-->0;gun--) {
    if (gun_update(battle,gun,elapsed)) {
      BATTLE->gunc--;
      memmove(gun,gun+1,sizeof(struct gun)*(BATTLE->gunc-i));
    }
  }
  
  /* Update bullets.
   */
  struct bullet *bullet=BATTLE->bulletv+BATTLE->bulletc-1;
  for (i=BATTLE->bulletc;i-->0;bullet--) {
    if (bullet_update(battle,bullet,elapsed)) {
      BATTLE->bulletc--;
      memmove(bullet,bullet+1,sizeof(struct bullet)*(BATTLE->bulletc-i));
    }
  }
  
  /* Check completion. First guy to die loses.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->dead) {
      if (r->dead) battle->outcome=0;
      else battle->outcome=-1;
    } else if (r->dead) battle->outcome=1;
  }
}

/* Render sprites.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  if (player->dead) return;
  int x=lround(player->x);
  int y=lround(player->y);
  graf_tile(&g.graf,x,y,player->tileid+player->animframe,player->xform);
}

static void soulballs_render(struct battle *battle,struct player *player) {
  if (!player->dead) return;
  graf_set_image(&g.graf,RID_image_hero);
  uint8_t tileid=0xa6;
  switch (player->soulballframe) {
    case 1: tileid+=1; break;
    case 2: tileid+=2; break;
    case 3: tileid+=3; break;
    case 4: tileid+=2; break;
    case 5: tileid+=1; break;
  }
  int ballc=(player->face==NS_face_dot)?7:6; // Witch souls have seven circles and all others no more than six. It's a firm rule of our universe.
  double t=0.0;
  double dt=(M_PI*2.0)/ballc;
  int i=ballc;
  for (;i-->0;t+=dt) {
    int x=(int)(player->x+player->soulballradius*sin(t));
    int y=(int)(player->y-player->soulballradius*cos(t));
    graf_tile(&g.graf,x,y,tileid,0);
  }
}

static void gun_render(struct battle *battle,struct gun *gun) {
  double adjx=gun->x,adjy=gun->y;
  adjx+=(1.0-gun->presence)*gun->edgex*NS_sys_tilesize;
  adjy+=(1.0-gun->presence)*gun->edgey*NS_sys_tilesize;
  int x=lround(adjx);
  int y=lround(adjy);
  double adjt=gun->t*gun->presence+gun->t0*(1.0-gun->presence);
  uint8_t rot=(int8_t)((adjt*128.0)/M_PI);
  graf_fancy(&g.graf,x,y,gun->tileid,0,rot,NS_sys_tilesize,0,0x808080ff);
}

static void bullet_render(struct battle *battle,struct bullet *bullet) {
  int x=lround(bullet->x);
  int y=lround(bullet->y);
  graf_fancy(&g.graf,x,y,0xf2,0,bullet->rot,NS_sys_tilesize,0,0x808080ff);
}

/* Render.
 */
 
static void _dodging_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(&g.graf,RID_image_meadow_sprites);
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  int i;
  struct gun *gun;
  struct bullet *bullet;
  
  // Render players sorted vertically, they can overlap.
  if (l->y<=r->y) {
    player_render(battle,l);
    player_render(battle,r);
  } else {
    player_render(battle,r);
    player_render(battle,l);
  }
  
  /* Lasers.
   * Also for all guns, show a warning line a split second before they fire.
   */
  graf_set_input(&g.graf,0);
  for (gun=BATTLE->gunv,i=BATTLE->gunc;i-->0;gun++) {
    uint32_t color=0;
    switch (gun->stage) {
      case GUN_STAGE_LASER: color=0xff4020ff; break; // active laser
      case GUN_STAGE_MENACE: {
          if (gun->clock<0.300) {
            if (gun->tileid==0xf1) { // laser warning
              color=0xff000040;
            } else { // rifle warning
              //color=0x00200030; // No warning lines for bullets, it's very distracting.
            }
          }
        } break;
    }
    if (!color) continue;
    int x=lround(gun->x);
    int y=lround(gun->y);
    graf_line(&g.graf,x,y,color,gun->lx,gun->ly,color);
  }
  graf_set_image(&g.graf,RID_image_meadow_sprites);
  
  // Guns.
  graf_set_filter(&g.graf,1);
  for (gun=BATTLE->gunv,i=BATTLE->gunc;i-->0;gun++) {
    gun_render(battle,gun);
  }
  
  // Bullets.
  for (bullet=BATTLE->bulletv,i=BATTLE->bulletc;i-->0;bullet++) {
    bullet_render(battle,bullet);
  }
  graf_set_filter(&g.graf,0);
  
  // Smoke.
  //TODO
  
  // If a player is dead, render her soulballs.
  soulballs_render(battle,l);
  soulballs_render(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_dodging={
  .name="dodging",
  .objlen=sizeof(struct battle_dodging),
  .id=NS_battle_dodging,
  .strix_name=320,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_dpad,
  .imageid_default=0,
  .del=_dodging_del,
  .init=_dodging_init,
  .update=_dodging_update,
  .render=_dodging_render,
};

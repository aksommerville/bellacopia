/* battle_laziness.c
 * Dodge falling things, aiming to minimize motion.
 * It's essentially random.
 * CPU players do have a few handicap-dependent parameters but they don't amount to much.
 * We hedge the randomness a little by multiplying each player's score by some constant established at setup.
 */

#include "game/bellacopia.h"

#define SKY_COLOR 0x785830ff
#define GROUND_COLOR 0x3c2011ff
#define GROUNDY 160
#define DANGERY 140
#define RADIUS 12
#define DURATION 10.0
#define HAZARD_LIMIT 20
#define HAZARD_TIME 0.150
#define COLC NS_sys_mapw
#define HOLD_TIME_BEST 0.100
#define HOLD_TIME_WORST 0.300
#define WALK_SPEED_BEST 100.0
#define WALK_SPEED_WORST 60.0
#define WALK_SPEED_HUMAN 80.0

struct battle_laziness {
  struct battle hdr;
  double clock;
  
  struct player {
    // Configuration, constant after init:
    int who; // my index
    int human; // 0 for cpu, or input index
    int imageid;
    uint8_t tileidv[4]; // Four frames of walking animation, and the first is also idle.
    int natural_right; // Boolean. Dot and Princess face left naturally, and monsters right. Why did I do that.
    double skill; // 0..1, digest of handicap.
    double penalty; // Score gets multiplied by so much.
    double holdtime; // s. CPU player will not change input state until so much time elapses.
    double walk_speed;
    uint32_t meter_color;
    // Volatile state for all modes:
    double animclock;
    int animframe;
    int indx; // Input state -1,0,1
    int facedx; // -1,1, never zero
    double x; // Framebuffer pixels. Y is constant.
    double score; // 0..1, running total of effort. We don't clamp but 1.0 is the mathematical limit.
    // Volatile state for CPU player:
    double holdclock;
    double targetx;
  } playerv[2];
  
  struct hazard {
    int x;
    double y;
    double dy;
    uint8_t tileid;
    int col;
  } hazardv[HAZARD_LIMIT];
  int hazardc;
  double hazardclock;
  int spawntrack[COLC];
  int collx,colly; // If nonzero, highlight a collision centered here.
};

#define BATTLE ((struct battle_laziness*)battle)

/* Delete.
 */
 
static void _laziness_del(struct battle *battle) {
}

/* Initialize one player.
 * (appearance) is 0,1,2 = sloblin,dot,princess
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int appearance) {
  player->human=human;
  if (human) player->walk_speed=WALK_SPEED_HUMAN;
  else player->walk_speed=WALK_SPEED_WORST*(1.0-player->skill)+WALK_SPEED_BEST*player->skill;
  player->holdtime=HOLD_TIME_WORST*(1.0-player->skill)+HOLD_TIME_BEST*player->skill;
  switch (appearance) {
    case 0: {
        player->imageid=RID_image_cave_sprites;
        player->tileidv[0]=0x0a;
        player->tileidv[1]=0x0b;
        player->tileidv[2]=0x0a;
        player->tileidv[3]=0x0b;
        player->natural_right=1;
        player->meter_color=0x2d823dff;
      } break;
    case 1: {
        player->imageid=RID_image_hero;
        player->tileidv[0]=0x12;
        player->tileidv[1]=0x22;
        player->tileidv[2]=0x12;
        player->tileidv[3]=0x32;
        player->meter_color=0x411775ff;
      } break;
    case 2: {
        player->imageid=RID_image_hero;
        player->tileidv[0]=0xa0;
        player->tileidv[1]=0xa1;
        player->tileidv[2]=0xa0;
        player->tileidv[3]=0xa2;
        player->meter_color=0x0d3ac1ff;
      } break;
  }
}

/* Initialize penalties.
 */
 
static void laziness_balanced_penalty(struct battle *battle) {
  const double peak=0.5;
  // Based on (handicap), the favored player gets 1..peak, and penalized player gets the inverse of that.
  double balance=((battle->args.bias-0x80)*peak)/128.0;
  if (balance<0.0) {
    BATTLE->playerv[1].penalty=1.0-balance;
    BATTLE->playerv[0].penalty=1.0/BATTLE->playerv[1].penalty;
  } else {
    BATTLE->playerv[0].penalty=1.0+balance;
    BATTLE->playerv[1].penalty=1.0/BATTLE->playerv[0].penalty;
  }
}

static void laziness_biased_penalty(struct battle *battle,struct player *penal,struct player *favor) {
  // Take the balanced penalties, then thumb the scale toward our bias.
  laziness_balanced_penalty(battle);
  penal->penalty+=0.25;
  favor->penalty-=0.25;
}

/* New.
 */

static int _laziness_init(struct battle *battle) {
  BATTLE->clock=DURATION;
  
  BATTLE->playerv[1].skill=(double)battle->args.bias/255.0;
  BATTLE->playerv[0].skill=1.0-BATTLE->playerv[1].skill;
  
  BATTLE->playerv[0].who=0;
  BATTLE->playerv[1].who=1;
  BATTLE->playerv[0].x=(COLC/3)*NS_sys_tilesize+(NS_sys_tilesize>>1); // Starting positions should be column-quantized.
  BATTLE->playerv[1].x=((COLC*2)/3)*NS_sys_tilesize+(NS_sys_tilesize>>1);
  BATTLE->playerv[0].facedx=1;
  BATTLE->playerv[1].facedx=-1;
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  if (BATTLE->playerv[0].human&&!BATTLE->playerv[1].human) laziness_biased_penalty(battle,BATTLE->playerv+1,BATTLE->playerv+0);
  else if (!BATTLE->playerv[0].human&&BATTLE->playerv[1].human) laziness_biased_penalty(battle,BATTLE->playerv+0,BATTLE->playerv+1);
  else laziness_balanced_penalty(battle);
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Important! We don't change state at every update.
   * Use a private (holdclock) to nerf the AI in a controllable fashion.
   * Only thing we do at every update is if we've crossed our target, stop.
   */
  if ((player->holdclock-=elapsed)<=0.0) {
    player->holdclock+=player->holdtime;
  } else {
    if ((player->indx<0)&&(player->x<=player->targetx)) player->indx=0;
    else if ((player->indx>0)&&(player->x>=player->targetx)) player->indx=0;
    return;
  }

  /* Quantize my position for easier comparison against hazards.
   */
  int x=(int)player->x/NS_sys_tilesize;
  int lx=x-1,rx=x+1;
  
  /* Find the lowest hazard in my column and its immediate neighbors.
   */
  struct hazard *lhazard=0,*hazard=0,*rhazard=0;
  struct hazard *q=BATTLE->hazardv;
  int i=BATTLE->hazardc;
  for (;i-->0;q++) {
    if (q->col==lx) {
      if (!lhazard||(q->y>lhazard->y)) lhazard=q;
    } else if (q->col==rx) {
      if (!rhazard||(q->y>rhazard->y)) rhazard=q;
    } else if (q->col==x) {
      if (!hazard||(q->y>hazard->y)) hazard=q;
    }
  }
  
  /* If none of these hazards is actually above my head, freeze.
   */
  int danger=0;
  if (lhazard) {
    double dx=lhazard->x-player->x;
    if ((dx>=-RADIUS)&&(dx<=RADIUS)) danger=1;
  }
  if (rhazard) {
    double dx=rhazard->x-player->x;
    if ((dx>=-RADIUS)&&(dx<=RADIUS)) danger=1;
  }
  if (hazard) {
    double dx=hazard->x-player->x;
    if ((dx>=-RADIUS)&&(dx<=RADIUS)) danger=1;
  }
  if (!danger) {
    player->indx=0;
    return;
  }
  
  /* Walk toward whichever column is highest.
   */
  double ly=0.0,y=0.0,ry=0.0;
  if (lhazard) ly=lhazard->y;
  if (hazard) y=hazard->y;
  if (rhazard) ry=rhazard->y;
  if ((y<=ly)&&(y<=ry)) player->targetx=x+0.5;
  else if (ly<=ry) player->targetx=x-0.5;
  else player->targetx=x+1.5;
  player->targetx*=NS_sys_tilesize;
  const double margin=1.0;
  if (player->targetx<player->x-margin) player->indx=-1;
  else if (player->targetx>player->x+margin) player->indx=1;
  else player->indx=0;
}

/* Update either mode, with (indx) decided.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Motion and animation.
  if (player->indx) {
    player->facedx=player->indx;
    player->x+=player->indx*player->walk_speed*elapsed;
    if (player->x<0.0) player->x=0.0;
    else if (player->x>FBW) player->x=FBW;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.200;
      if (++(player->animframe)>=4) player->animframe=0;
    }
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }

  // Update effort and score.
  if (player->indx) {
    player->score+=(elapsed*player->penalty)/DURATION;
  }
  
  // Check collisions.
  if (battle->outcome>-2) return;
  struct hazard *hazard=BATTLE->hazardv;
  int i=BATTLE->hazardc;
  for (;i-->0;hazard++) {
    if (hazard->y<DANGERY) continue;
    double dx=hazard->x-player->x;
    if (dx>RADIUS) continue;
    if (dx<-RADIUS) continue;
    int px=(int)player->x;
    int py=GROUNDY-(NS_sys_tilesize>>1);
    int hx=hazard->x;
    int hy=(int)hazard->y;
    BATTLE->collx=(px+hx)>>1;
    BATTLE->colly=(py+hy)>>1;
    battle->outcome=player->who?1:-1;
    return;
  }
}

/* Update hazard. Returns zero if defunct.
 */
 
static int hazard_update(struct battle *battle,struct hazard *hazard,double elapsed) {
  hazard->y+=hazard->dy*elapsed;
  if (hazard->y>GROUNDY) return 0;
  return 1;
}

/* Spawn a random hazard.
 */
 
static struct hazard *hazard_spawn(struct battle *battle) {
  if (BATTLE->hazardc>=HAZARD_LIMIT) return 0;
  struct hazard *hazard=BATTLE->hazardv+BATTLE->hazardc++;
  
  /* Quantize to meters horizontally.
   * Track spawns at each column and take pains to balance the set.
   */
  int spawnmin=INT_MAX;
  int i=COLC;
  while (i-->0) if (BATTLE->spawntrack[i]<spawnmin) spawnmin=BATTLE->spawntrack[i];
  uint8_t colv[COLC];
  int colc=0;
  for (i=COLC;i-->0;) if (BATTLE->spawntrack[i]==spawnmin) colv[colc++]=i;
  if (colc<1) return 0; // oops?
  int col=colv[rand()%colc];
  BATTLE->spawntrack[col]++;
  hazard->x=col*NS_sys_tilesize+(NS_sys_tilesize>>1);
  hazard->col=col;
  
  hazard->y=-(NS_sys_tilesize>>1);
  hazard->dy=100.0;
  hazard->tileid=0x61+rand()%4;
  return hazard;
}

/* Update.
 */
 
static void _laziness_update(struct battle *battle,double elapsed) {

  if (battle->outcome>-2) return;
  
  if ((BATTLE->clock-=elapsed)<=0.0) { // Played to the bell. Pick a winner.
    if (BATTLE->playerv[0].score<BATTLE->playerv[1].score) battle->outcome=1;
    else if (BATTLE->playerv[0].score>BATTLE->playerv[1].score) battle->outcome=-1;
    else battle->outcome=0;
    return;
  }
  
  struct player *player=BATTLE->playerv;
  int i=2; for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  struct hazard *hazard=BATTLE->hazardv+BATTLE->hazardc-1;
  for (i=BATTLE->hazardc;i-->0;hazard--) {
    if (!hazard_update(battle,hazard,elapsed)) {
      BATTLE->hazardc--;
      memmove(hazard,hazard+1,sizeof(struct hazard)*(BATTLE->hazardc-i));
    }
  }
  
  if (BATTLE->hazardc<HAZARD_LIMIT) {
    if ((BATTLE->hazardclock-=elapsed)<=0.0) {
      BATTLE->hazardclock+=HAZARD_TIME;
      hazard_spawn(battle);
    }
  }
}

/* Render player.
 */
 
static void render_player(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=GROUNDY-(NS_sys_tilesize>>1);
  uint8_t tileid=player->tileidv[player->animframe];
  uint8_t xform=0;
  if (player->natural_right) {
    if (player->facedx<0) xform=EGG_XFORM_XREV;
  } else {
    if (player->facedx>0) xform=EGG_XFORM_XREV;
  }
  graf_set_image(&g.graf,player->imageid);
  graf_tile(&g.graf,x,y,tileid,xform);
}

/* Render meter.
 */
 
static void render_meter(struct battle *battle,int x,int y,int w,int h,double v,uint32_t rgba) {
  const uint32_t bgcolor=0x181010ff;
  int fillw=(int)(w*v);
  if (fillw<=0) {
    graf_fill_rect(&g.graf,x,y,w,h,bgcolor);
  } else if (fillw>=w) {
    graf_fill_rect(&g.graf,x,y,w,h,rgba);
  } else {
    graf_fill_rect(&g.graf,x,y,fillw,h,rgba);
    graf_fill_rect(&g.graf,x+fillw,y,w-fillw,h,bgcolor);
  }
}

/* Render.
 */
 
static void _laziness_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,SKY_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH,GROUND_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  
  render_player(battle,BATTLE->playerv+0);
  render_player(battle,BATTLE->playerv+1);
  
  graf_set_image(&g.graf,RID_image_battle_goblins);
  struct hazard *hazard=BATTLE->hazardv;
  int i=BATTLE->hazardc;
  for (;i-->0;hazard++) {
    int y=(int)hazard->y;
    graf_tile(&g.graf,hazard->x,y,hazard->tileid,0);
  }
  
  if (BATTLE->collx) {
    graf_tile(&g.graf,BATTLE->collx,BATTLE->colly,0x60,0);
  }
  
  /* Score meters.
   * Nominally, your score is the proportion of total time that you spent walking. Starts at zero and can theoretically reach one.
   * Players that have any idea at all what they're doing will spend most of their time idle.
   * So, in order to not waste a lot of screen width, we'll assume a range substantially lower than one, and bump it up dynamically if needed.
   */
  const int meterw=FBW>>1;
  const int meterh=6;
  const int groundh=FBH-GROUNDY;
  double lv=BATTLE->playerv[0].score;
  double rv=BATTLE->playerv[1].score;
  double hiv=(lv>rv)?lv:rv;
  if (hiv>0.5) ; // yikes, what you doing, player? Use the full range.
  else if (hiv>0.25) { lv*=2.0; rv*=2.0; }
  else { lv*=4.0; rv*=4.0; }
  render_meter(battle,(FBW>>1)-(meterw>>1),GROUNDY+(groundh/3)-(meterh>>1),meterw,meterh,lv,BATTLE->playerv[0].meter_color);
  render_meter(battle,(FBW>>1)-(meterw>>1),GROUNDY+((groundh*2)/3)-(meterh>>1),meterw,meterh,rv,BATTLE->playerv[1].meter_color);
  
  /* Clock.
   */
  int ms=(int)(BATTLE->clock*1000.0);
  int sec=(ms+999)/1000;
  int x=(FBW>>1)-4;
  int y=10;
  graf_set_image(&g.graf,RID_image_fonttiles);
  if (BATTLE->clock<3.0) {
    ms%=1000;
    graf_set_tint(&g.graf,(ms>=500)?0xff8080ff:0xffff00ff);
  }
  if (sec>=10) graf_tile(&g.graf,x,y,'0'+sec/10,0);
  graf_tile(&g.graf,x+8,y,'0'+sec%10,0);
  graf_set_tint(&g.graf,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_laziness={
  .name="laziness",
  .objlen=sizeof(struct battle_laziness),
  .strix_name=53,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_laziness_del,
  .init=_laziness_init,
  .update=_laziness_update,
  .render=_laziness_render,
};

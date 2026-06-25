/* battle_hockey.c
 * Fire one puck at a time until somebody scores 3.
 */

#include "game/bellacopia.h"

#define CPU_PENALTY 0.750

struct battle_hockey {
  struct battle hdr;
  int choice;
  
  double goalx,goaly;
  double goaldx;
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double x,y; // Framebuffer pixels, position of my puck at its home. My own position is a little off (only render should care).
    double velmin,velmax;
    
    // Input state. Controllers set.
    int blackout;
    int dt; // -1,0,1
    int stick;
    
    double t; // Radians clockwise, direction puck will go. 0=up.
    double power; // 0..1
    double puckx,pucky;
    double puckdx,puckdy;
    int ondeck; // Nonzero if the puck is at rest and can be shot.
    int score;
    
    // CPU
    double tchoice;
    
  } playerv[2];
};

#define BATTLE ((struct battle_hockey*)battle)

/* Delete.
 */
 
static void _hockey_del(struct battle *battle) {
}

/* Put my puck at my toes.
 */
 
static void hockey_spawn_puck(struct battle *battle,struct player *player) {
  player->ondeck=1;
  player->puckx=player->x;
  player->pucky=player->y;
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=100.0;
  } else { // Right.
    player->who=1;
    player->x=FBW-100.0;
  }
  player->y=80.0+(1.0-player->skill)*70.0;
  player->blackout=1;
  player->velmax=(1.0-player->skill)*100.0+player->skill*300.0;
  if (player->human=human) { // Human.
  } else { // CPU.
    player->velmax*=CPU_PENALTY;
  }
  player->velmin=player->velmax*0.5;
  switch (face) {
    case NS_face_monster: {
        player->color=0xe0f6f1ff;
        player->tileid=0x1c;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x1b;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x1d;
      } break;
  }
  hockey_spawn_puck(battle,player);
}

/* New.
 */
 
static int _hockey_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  // And I keep getting this backward: At HIGH skill, the player is favored to win. High bias means high skill for the CPU and low skill for Dot.
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  BATTLE->goalx=40.0+rand()%(FBW-80);
  BATTLE->goaly=20.0;
  BATTLE->goaldx=50.0;
  if (rand()&1) BATTLE->goaldx=-BATTLE->goaldx;
  BATTLE->playclock=20.0;
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->dt=-1; break;
    case EGG_BTN_RIGHT: player->dt=1; break;
    default: player->dt=0; break;
  }
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->stick=(input&EGG_BTN_SOUTH)?1:0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->t>player->tchoice+0.010) player->dt=-1;
  else if (player->t<player->tchoice-0.010) player->dt=1;
  else player->dt=0;
  if (player->power>=1.0) {
    player->stick=0;
    player->tchoice=((rand()&0xffff)*M_PI*0.200)/65535.0-M_PI*0.100;
  } else {
    player->stick=1;
  }
}

/* Commit swing.
 */
 
static void hockey_swing(struct battle *battle,struct player *player) {
  double nx=sin(player->t);
  double ny=-cos(player->t);
  double vel=(1.0-player->power)*player->velmin+player->power*player->velmax;
  player->puckdx=nx*vel;
  player->puckdy=ny*vel;
  player->ondeck=0;
  player->power=0.0;
  bm_sound_pan(RID_sound_whack,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Update player angle per input.
  if (player->dt) {
    player->t+=1.500*elapsed*player->dt;
    const double limit=M_PI*0.250;
    if (player->t<-limit) player->t=-limit;
    else if (player->t>limit) player->t=limit;
  }
  
  // If the puck is in motion, update it.
  if (!player->ondeck) {
    player->puckx+=player->puckdx*elapsed;
    player->pucky+=player->puckdy*elapsed;
    const double margin=8.0;
    if (
      (player->puckx<-margin)||(player->puckx>FBW+margin)||
      (player->pucky<-margin)||(player->pucky>FBH+margin)
    ) {
      hockey_spawn_puck(battle,player);
    } else if (BATTLE->playerv[0].score+BATTLE->playerv[1].score<5) {
      double dy=player->pucky-BATTLE->goaly;
      if ((dy>=-8.0)&&(dy<=8.0)) {
        double dx=player->puckx-BATTLE->goalx;
        if ((dx>=-14.0)&&(dx<=14.0)) {
          player->score++;
          hockey_spawn_puck(battle,player);
          bm_sound_pan(RID_sound_treasure,player->who?PLAYER_PAN:-PLAYER_PAN);
        }
      }
    }
  }
  
  // Puck not in motion, are we swinging?
  if (player->ondeck) {
    if (player->stick) {
      player->power+=elapsed;
      if (player->power>1.0) player->power=1.0;
    } else if (player->power>0.0) {
      hockey_swing(battle,player);
    }
  }
}

/* Update.
 */
 
static void _hockey_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  // Update players and pucks.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  // Move goal.
  BATTLE->goalx+=BATTLE->goaldx*elapsed;
  const double goallimit=30.0;
  if ((BATTLE->goalx<goallimit)&&(BATTLE->goaldx<0.0)) BATTLE->goaldx=-BATTLE->goaldx;
  else if ((BATTLE->goalx>FBW-goallimit)&&(BATTLE->goaldx>0.0)) BATTLE->goaldx=-BATTLE->goaldx;
  
  // Check completion.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if ((BATTLE->playclock-=elapsed)<=0.0) {
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
  } else if (l->score>=3) {
    battle->outcome=1;
  } else if (r->score>=3) {
    battle->outcome=-1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  graf_fancy(&g.graf,(int)player->puckx,(int)player->pucky,0x0b,0,0,NS_sys_tilesize,0,player->color);
  
  double sint=sin(player->t);
  double cost=cos(player->t);
  int playerx=(int)(player->x-cost*10.0);
  int playery=(int)(player->y-sint*10.0);
  uint8_t rotation=(int8_t)((player->t*128.0)/M_PI);
  graf_fancy(&g.graf,playerx,playery,player->tileid,0,rotation,NS_sys_tilesize,0,player->color);
  
  int stickx=(int)(player->x-cost*7.0);
  int sticky=(int)(player->y-sint*7.0);
  double stickt=player->t+player->power*M_PI;
  uint8_t stickrotation=(int8_t)((stickt*128.0)/M_PI);
  graf_fancy(&g.graf,stickx,sticky,0x0c,0,stickrotation,NS_sys_tilesize,0,player->color);
}

/* Render.
 */
 
static void _hockey_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0xd0e5ecff);
  graf_set_image(&g.graf,RID_image_icepalace_sprites);
  
  // Goal.
  int goalx=(int)BATTLE->goalx;
  int goaly=(int)BATTLE->goaly;
  graf_tile(&g.graf,goalx-(NS_sys_tilesize>>1),goaly,0x0d,0);
  graf_tile(&g.graf,goalx+(NS_sys_tilesize>>1),goaly,0x0e,0);
  
  // Players.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_set_filter(&g.graf,1);
  player_render(battle,l);
  player_render(battle,r);
  graf_set_filter(&g.graf,0);
  
  // Scoreboard.
  int dstx=(FBW>>1)-32;
  int dsty=FBH-8;
  int i=0;
  for (;i<5;i++,dstx+=16) {
    uint32_t color=0x808080ff;
    if (i<l->score) color=l->color;
    else if (i>=5-r->score) color=r->color;
    graf_fancy(&g.graf,dstx,dsty,0x08,0,0,NS_sys_tilesize,0,color);
  }
  graf_set_image(&g.graf,RID_image_fonttiles);
  int s=(int)(BATTLE->playclock+0.999);
  if (s<0) s=0; else if (s>99) s=99;
  graf_set_tint(&g.graf,0x000000ff);
  if (s>=10) graf_tile(&g.graf,(FBW>>1)-4,FBH-24,'0'+s/10,0);
  graf_tile(&g.graf,(FBW>>1)+4,FBH-24,'0'+s%10,0);
  graf_set_tint(&g.graf,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_hockey={
  .name="hockey",
  .objlen=sizeof(struct battle_hockey),
  .id=NS_battle_hockey,
  .strix_name=232,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_hockey_del,
  .init=_hockey_init,
  .update=_hockey_update,
  .render=_hockey_render,
};

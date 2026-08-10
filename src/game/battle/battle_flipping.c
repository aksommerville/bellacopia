/* battle_flipping.c
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_visbits.h"

#define GROUNDY 160
#define YBASE (GROUNDY-28.0) /* For a hero standing limply on the trampoline. */

struct battle_flipping {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // 1x2
    uint8_t xform;
    int x; // Center of sprite and trampoline, constant.
    double y; // Also framebuffer pixels but volatile.
    double dy;
    double t;
    int indt;
    
    int done; // <0 if faulted, >0 if completed a flip
    int seated;
    double seatp;
    double seatmag;
    
    double turnspeed;
    double gravity;
    double termvel;
    double bouncelo,bouncehi; // Multipliers.
    double tipt; // Positive radians.
    double cputhresh;
    
    int cpuwait; // Nonzero when seated, when seated turns off, examine velocity and decide whether to go for it.
    int cputurn; // -1,0,1: The decision.
    
  } playerv[2];
};

#define BATTLE ((struct battle_flipping*)battle)

/* Delete.
 */
 
static void _flipping_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=(FBW>>1)-40;
  } else { // Right.
    player->who=1;
    player->x=(FBW>>1)+40;
    player->xform=EGG_XFORM_XREV;
  }
  player->y=YBASE-25.0;
  
  player->turnspeed=4.000*(1.0-player->skill)+7.000*player->skill; // rad/sec. 5 feels good.
  player->gravity=300.0; // px/sec**2
  player->termvel=200.0; // px/sec
  player->bouncelo=0.700*(1.0-player->skill)+0.900*player->skill;
  player->bouncehi=1.100*(1.0-player->skill)+1.250*player->skill;
  player->tipt=M_PI*0.250;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cpuwait=1; // Always hit the trampoline first before deciding anything.
    player->cputhresh=-170.0;
    player->bouncehi*=0.950; // Mild CPU penalty.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xd9ccb1ff; // Skin color. Feathers would be more fitting, but they're too close to Dot.
        player->tileid=0x50;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x10;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x30;
      } break;
  }
}

/* New.
 */
 
static int _flipping_init(struct battle *battle) {
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

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If we're seated, set some flags and wait.
   */
  player->indt=0;
  if (player->seated) {
    player->cpuwait=1;
    player->cputurn=0;
    return;
  }

  /* If we've decided to turn, do it until we cross 2 pi or -2 pi.
   */
  if (player->cputurn<0) {
    player->indt=(player->t>M_PI*-2.0)?-1:0;
    return;
  } else if (player->cputurn>0) {
    player->indt=(player->t<M_PI*2.0)?1:0;
    return;
  }
  
  /* First update after (seated) goes off, check our velocity.
   * If it's sufficiently negative, let's rock and roll.
   */
  if (player->cpuwait) {
    player->cpuwait=0;
    if (player->dy<player->cputhresh) {
      player->cputurn=(rand()&1)?-1:1;
    }
  }
}

/* Negative if fatal, positive if victory.
 */
 
static int flipping_assess_attack_angle(struct battle *battle,struct player *player,double t) {
  int flipped=0;
  if (t>M_PI*1.800) flipped=1;
  else if (t<M_PI*-1.800) flipped=1;
  while (t<-M_PI) t+=M_PI*2.0;
  while (t>M_PI) t-=M_PI*2.0;
  if (flipped&&(t>-0.100*M_PI)&&(t<0.100*M_PI)) return 1;
  // We have the option of returning -1 to fail. I'm not sure we want that.
  return 0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Once faulted or flipped, we're done.
   */
  if (player->done) return;
  
  /* If seated, touching the dpad causes a fault, and we tick a timer or something to the next bounce.
   */
  if (player->seated) {
    /* Failing due to misplaced rotation is pretty harsh. I think nix this.
    if (player->indt) {
      player->done=-1;
      return;
    }
    /**/
    player->seatp+=elapsed*3.000;
    if (player->seatp>=1.0) { // Bounce complete. (dy) was already updated at the start, so just release (seated) and carry on.
      player->seated=0;
    } else {
      return;
    }
  }
  
  /* Turn?
   */
  if (player->indt) {
    player->t+=player->turnspeed*player->indt*elapsed;
    // Do not wrap! When she lands, we're going to check like ">pi*2" for a full turn.
  }
  
  /* Apply gravity or bounce.
   */
  player->dy+=player->gravity*elapsed;
  if (player->dy>player->termvel) player->dy=player->termvel;
  player->y+=player->dy*elapsed;
  if (player->y>=YBASE) {
    if (battle->outcome>-2) { // Other player must have ended it. Just stop.
      player->done=1;
      player->t=0.0;
      player->y=YBASE;
      return;
    }
    int assess=flipping_assess_attack_angle(battle,player,player->t);
    if (assess) { // Negative for a fault, positive to win.
      player->done=assess;
      player->t=0.0;
      player->y=YBASE;
      return;
    }
    while (player->t<-M_PI) player->t+=M_PI*2.0;
    while (player->t>M_PI) player->t-=M_PI*2.0;
    if (player->t<0.0) player->t=-player->t;
    double n=player->t/player->tipt;
    if (n<0.0) n=0.0; else if (n>1.0) n=1.0;
    n=player->bouncelo*n+player->bouncehi*(1.0-n);
    player->t=0.0;
    player->y=YBASE;
    player->seatmag=player->dy/30.0;
    if (player->seatmag<1.0) player->seatmag=1.0;
    else if (player->seatmag>6.0) player->seatmag=6.0;
    player->dy=-player->dy*n;
    if (player->dy>-50.0) {
      player->dy=-50.0;
    }
    player->seated=1;
    player->seatp=0.0;
    bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
  }
}

/* Update.
 */
 
 static int slomo=0;
 
static void _flipping_update(struct battle *battle,double elapsed) {
  
  //if (slomo--<0) slomo=5; else return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* First player to become done, the game is over.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->done||r->done) {
    if (l->done==r->done) battle->outcome=0;
    else if (l->done<0) battle->outcome=-1;
    else if (l->done>0) battle->outcome=1;
    else if (r->done<0) battle->outcome=1;
    else if (r->done>0) battle->outcome=-1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int ht=NS_sys_tilesize>>1;
  
  // Trampoline.
  int distension=0;
  if (player->seated&&!player->done) {
    double p=(player->seatp-0.5)*2.0;
    p=1.0-p*p;
    distension=lround(p*player->seatmag);
    if (distension<0) distension=0;
    else if (distension>5) distension=5;
  }
  int trampy=GROUNDY-ht;
  uint8_t tramptile=0x11+0x10*distension;
  graf_tile(&g.graf,player->x-ht,trampy,tramptile,0);
  graf_tile(&g.graf,player->x+ht,trampy,tramptile+1,0);
  
  // Hero.
  int srcx=(player->tileid&0x0f)*NS_sys_tilesize;
  int srcy=(player->tileid>>4)*NS_sys_tilesize;
  int dstx=player->x;
  int dsty=(int)player->y+distension;
  double t=player->t;
  if (player->done<0) { // Faulted. Lie on your back.
    if (player->who) {
      t=M_PI*0.5;
      dstx+=10;
    } else {
      t=M_PI*-0.5;
      dstx-=10;
    }
    dsty+=12;
  }
  graf_set_filter(&g.graf,1);
  batsup_render_decal(dstx,dsty,srcx,srcy,NS_sys_tilesize,NS_sys_tilesize*2.0,player->xform,t,1.0);
  graf_set_filter(&g.graf,0);
}

/* Render.
 */
 
static void _flipping_render(struct battle *battle) {

  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_set_image(&g.graf,RID_image_battle_desert);
  player_render(battle,l);
  player_render(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_flipping={
  .name="flipping",
  .objlen=sizeof(struct battle_flipping),
  .id=NS_battle_flipping,
  .strix_name=318,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz,
  .imageid_default=RID_image_desert,
  .del=_flipping_del,
  .init=_flipping_init,
  .update=_flipping_update,
  .render=_flipping_render,
};

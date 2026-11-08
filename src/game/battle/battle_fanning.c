/* battle_fanning.c
 */

#include "game/batsup/battle_internal.h"

struct battle_fanning {
  struct battle hdr;
  double playtime; // Counts down.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    double x,y;
    int indx,indy,infan;
    double animclock;
    int animframe;
    int fanning;
    double walkspeed;
    double whifft;
    int blackout;
    double range; // Pixels, further distance from target where Princess will be affected.
    double range2;
    double fanlo,fanhi; // Hz, efficacy of fan per proximity.
    double score; // 0..1
    double cpuclock;
  } playerv[2];
  
  /* The Princess uses struct player because I expect some overlap between players and her.
   * Note that when we say "Princess" in this battle, we're talking about the game's subject.
   * She might look like the Princess or the Courtier, that's not important.
   */
  struct player princess;
};

#define BATTLE ((struct battle_fanning*)battle)

/* Delete.
 */
 
static void _fanning_del(struct battle *battle) {
}

/* Set player face.
 */
 
static void player_set_face(struct player *player,int face) {
  switch (face) {
    case NS_face_monster: {
        player->color=0xcb9815ff;
        player->tileid=0x49;
      } break;
    case NS_face_dot: {
        player->color=0xa668f3ff;
        player->tileid=0x29;
      } break;
    case NS_face_princess: {
        player->color=0x5d83f4ff;
        player->tileid=0x39;
      } break;
  }
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->y=FBH*0.5;
  player->x=FBW*0.333;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->x=FBW-player->x;
  }
  
  player->walkspeed=70.0*(1.0-player->skill)+150.0*player->skill;
  player->range=16.0*(1.0-player->skill)+32.0*player->skill;
  player->range2=player->range*player->range;
  player->fanhi=0.200*(1.0-player->skill)+0.400*player->skill;
  player->fanlo=player->fanhi*0.333;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpuclock=0.500;
    // Artificial CPU penalty, because when balanced they are way too good at this.
    if (battle->args.lctl||battle->args.rctl) {
      player->walkspeed*=0.800;
      player->fanhi*=0.700;
      player->fanlo=player->fanhi*0.333;
    }
  }
  player_set_face(player,face);
}

/* New.
 */
 
static int _fanning_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  
  /* Initialize the Princess.
   * When one of the players is wearing Princess face, we're Monster (ie the Courtier).
   * Otherwise, typical 1-player scenario, we're Princess.
   */
  BATTLE->princess.who=-1;
  BATTLE->princess.human=-1;
  int gotdot=0,gotprincess=0,gotmonster=0;
  switch (battle->args.lface) { case NS_face_monster: gotmonster=1; break; case NS_face_princess: gotprincess=1; break; case NS_face_dot: gotdot=1; break; }
  switch (battle->args.rface) { case NS_face_monster: gotmonster=1; break; case NS_face_princess: gotprincess=1; break; case NS_face_dot: gotdot=1; break; }
  if (!gotprincess) player_set_face(&BATTLE->princess,NS_face_princess); // Dot vs Courtier: I'm the Princess.
  else if (!gotmonster) player_set_face(&BATTLE->princess,NS_face_monster); // Dot vs Princess: I'm the Courtier.
  else player_set_face(&BATTLE->princess,NS_face_dot); // Princess vs Courtier: I'm Dot. This one should never happen.
  BATTLE->princess.x=FBW*0.5;
  BATTLE->princess.y=FBH*0.5;
  BATTLE->princess.walkspeed=50.0;
  BATTLE->princess.cpuclock=1.000; // Stand still for a bit initially.
  
  BATTLE->playtime=9.0;
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
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
    player->infan=(input&EGG_BTN_SOUTH)?1:0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Decisions play out for a few frames no matter what, just to avoid the most egregious jitter.
   */
  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    return;
  }
  player->cpuclock+=0.150;

  /* The two ideal places to be are 16 pixels left and right of the Princess.
   */
  struct player *prn=&BATTLE->princess;
  double idealax=prn->x-16.0;
  double idealbx=prn->x+16.0;
  
  /* If we're within some range of either of those points, ensure we're facing the right direction, then fan.
   */
  const double ok2=100.0;
  double dy=prn->y-player->y;
  double adx=idealax-player->x;
  double bdx=idealbx-player->x;
  double ad2=adx*adx+dy*dy;
  double bd2=bdx*bdx+dy*dy;
  if ((ad2<ok2)||(bd2<ok2)) {
    if ((prn->x<player->x)&&!player->xform) {
      player->indx=-1;
      player->indy=0;
      player->infan=0;
    } else if ((prn->x>player->x)&&player->xform) {
      player->indx=1;
      player->indy=0;
      player->infan=0;
    } else {
      player->indx=0;
      player->indy=0;
      player->infan=1;
    }
    return;
  }
  
  /* Walk toward whichever is closer.
   */
  const double closeenough=3.0;
       if (dy<-closeenough) player->indy=-1;
  else if (dy>closeenough) player->indy=1;
  else player->indy=0;
  if (ad2<bd2) {
         if (adx<-closeenough) player->indx=-1;
    else if (adx>closeenough) player->indx=1;
    else player->indx=0;
  } else {
         if (bdx<-closeenough) player->indx=-1;
    else if (bdx>closeenough) player->indx=1;
    else player->indx=0;
  }
  player->infan=0;
}

/* Increase player's score if Princess is in whiff range.
 */
 
static void player_check_whiff(struct battle *battle,struct player *player,double elapsed) {
  struct player *prn=&BATTLE->princess;
  double targetx=player->x;
  double targety=player->y;
  if (player->xform) {
    if (prn->x>=player->x) return;
    targetx-=16.0;
  } else {
    if (prn->x<=player->x) return;
    targetx+=16.0;
  }
  double dx=prn->x-targetx;
  double dy=prn->y-targety;
  double d2=dx*dx+dy*dy;
  if (d2>=player->range2) return;
  double dist=sqrt(d2);
  double n=1.0-dist/player->range;
  double rate=player->fanlo*(1.0-n)+player->fanhi*n;
  player->score+=rate*elapsed;
  if (player->score>1.0) player->score=1.0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Start or stop fanning?
   */
  if (player->infan) {
    if (!player->fanning) {
      player->fanning=1;
      player->animclock=0.0;
      player->animframe=0;
      player->whifft=0.0;
    }
  } else if (player->fanning) {
    player->fanning=0;
    player->animclock=0.0;
    player->animframe=0;
    player->whifft=0.0;
  }
  
  /* When fanning, animate and check the princess.
   * Motion and changing direction are forbidden.
   */
  if (player->fanning) {
    player->whifft+=elapsed;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.250;
      if (++(player->animframe)>=2) player->animframe=0;
    }
    player_check_whiff(battle,player,elapsed);
    return;
  }
  
  /* Walking?
   */
  if (player->indx||player->indy) {
    player->x+=player->walkspeed*elapsed*player->indx;
    player->y+=player->walkspeed*elapsed*player->indy;
    if (player->x<0.0) player->x=0.0; else if (player->x>FBW) player->x=FBW;
    if (player->y<0.0) player->y=0.0; else if (player->y>FBH) player->y=FBH;
    if (player->indx<0) player->xform=EGG_XFORM_XREV;
    else if (player->indx>0) player->xform=0;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.125;
      if (++(player->animframe)>=4) player->animframe=0;
    }
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
}

/* Update princess.
 */
 
static void princess_update(struct battle *battle,struct player *player,double elapsed) {
  
  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
  } else {
    /* Choose one of the nine* directions, and a time to walk in that direction.
     * [*] Standing still is a direction.
     * If we're close to an edge, always walk away from it.
     * Beyond that, I don't think we need to cut short for edges, just let them collide if it happens.
     */
    const double edge=0.250;
    if (player->x<FBW*edge) player->indx=1;
    else if (player->x>FBW*(1.0-edge)) player->indx=-1;
    else switch (rand()%3) {
      case 0: player->indx=0; break;
      case 1: player->indx=1; break;
      default: player->indx=-1; break;
    }
    if (player->y<FBH*edge) player->indy=1;
    else if (player->y<FBH*(1.0-edge)) player->indy=-1;
    else switch (rand()%3) {
      case 0: player->indy=0; break;
      case 1: player->indy=1; break;
      default: player->indy=-1; break;
    }
    double n=(rand()&0xffff)/65535.0;
    player->cpuclock=0.200*(1.0-n)+0.800*n;
  }
  
  /* Maybe a little overkill, but let the generic controller take it from here.
   * This way, we can't mess up the animation or clamping.
   */
  player_update_common(battle,player,elapsed);
}

/* Update.
 */
 
static void _fanning_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  princess_update(battle,&BATTLE->princess,elapsed);
  
  /* Battle ends immediately if either player breaches 1.0.
   * Otherwise, we time out after a while and the higher score wins.
   * Ties are possible, eg if both players are dead.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    BATTLE->playtime-=elapsed;
    if ((BATTLE->playtime<=0.0)||(l->score>=1.0)||(r->score>=1.0)) {
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=(int)player->y;
  uint8_t tileid=player->tileid;
  
  // Fan?
  if (player->fanning) {
    int fx=x;
    if (player->xform) fx-=8;
    else fx+=8;
    uint8_t ftileid=0x1c+player->animframe;
    graf_tile(g_graf,fx,y,ftileid,player->xform);
    
  // Animate walking (safe to do even when still).
  } else switch (player->animframe) {
    case 1: tileid+=1; break;
    case 3: tileid+=2; break;
  }

  graf_tile(g_graf,x,y,tileid,player->xform);
}

/* Render whiff indicator.
 */
 
static void fanning_render_whiff(struct battle *battle,struct player *player) {
  double nx=1.0;
  if (player->xform) nx=-1.0;
  double fx=player->x+12.0*nx;
  double fy=player->y;
  uint32_t colora=0xffffff00; // near
  uint32_t colorb=0xffffff00; // far
  double aa=sin(player->whifft*12.000)*64.0;
  int aai=0x80+(int)aa;
  colora|=aai;
  graf_line(g_graf,(int)(fx+nx*3.0),(int)(fy-3.0),colora,(int)(fx+nx*20.0),(int)(fy-10.0),colorb);
  graf_line(g_graf,(int)(fx+nx*3.0),(int)(fy+3.0),colora,(int)(fx+nx*20.0),(int)(fy+10.0),colorb);
  graf_line(g_graf,(int)(fx+nx*5.0),(int)fy,colora,(int)(fx+nx*25.0),(int)fy,colorb);
}

/* Render one score bar.
 */
 
static void fanning_render_scoreboard(struct battle *battle,struct player *player) {
  int barw=(FBW>>1)-12;
  int barh=4;
  int bary=5;
  int fillw=(int)(player->score*barw);
  if (fillw<0) fillw=0; else if (fillw>barw) fillw=barw;
  int barx,fillx;
  if (player->who) { // Bar on the screen's right half; fill right aligned.
    barx=FBW-3-barw;
    fillx=barx+barw-fillw;
  } else { // Bar on the screen's left half; fill left aligned.
    barx=3;
    fillx=3;
  }
  graf_fill_rect(g_graf,barx-1,bary-1,barw+2,barh+2,0x00000080);
  graf_fill_rect(g_graf,fillx,bary,fillw,barh,player->color);
}

/* Render.
 */
 
static void _fanning_render(struct battle *battle) {

  graf_fill_rect(g_graf,0,0,FBW,FBH,0x145c32ff); // Carpet color from image:castle_int. Doesn't have color table support.
  
  // Whiff indicators for each player, before the sprites.
  if (BATTLE->playerv[0].fanning) fanning_render_whiff(battle,BATTLE->playerv+0);
  if (BATTLE->playerv[1].fanning) fanning_render_whiff(battle,BATTLE->playerv+1);
  
  graf_set_image(g_graf,RID_image_battle_desert);
  
  struct player *playerv[3]; // In render order.
  struct player *a=BATTLE->playerv;
  struct player *b=a+1;
  struct player *c=&BATTLE->princess;
  if ((a->y<=b->y)&&(a->y<=c->y)) {
    playerv[0]=a;
    if (b->y<=c->y) {
      playerv[1]=b;
      playerv[2]=c;
    } else {
      playerv[1]=c;
      playerv[2]=b;
    }
  } else if ((b->y<=a->y)&&(b->y<=c->y)) {
    playerv[0]=b;
    if (a->y<=c->y) {
      playerv[1]=a;
      playerv[2]=c;
    } else {
      playerv[1]=c;
      playerv[2]=a;
    }
  } else {
    playerv[0]=c;
    if (a->y<=b->y) {
      playerv[1]=a;
      playerv[2]=b;
    } else {
      playerv[1]=b;
      playerv[2]=a;
    }
  }
  player_render(battle,playerv[0]);
  player_render(battle,playerv[1]);
  player_render(battle,playerv[2]);
  
  // Scoreboard and clock.
  fanning_render_scoreboard(battle,BATTLE->playerv+0);
  fanning_render_scoreboard(battle,BATTLE->playerv+1);
  if (battle->outcome==-2) {
    int s=(BATTLE->playtime+0.999);
    if (s<1) s=1; else if (s>9) s=9;
    graf_set_image(g_graf,RID_image_fonttiles);
    graf_tile(g_graf,FBW>>1,8,'0'+s,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_fanning={
  .name="fanning",
  .objlen=sizeof(struct battle_fanning),
  .id=121,
  .strix_name=402,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_dpad_a,
  .imageid_default=RID_image_desert,
  .del=_fanning_del,
  .init=_fanning_init,
  .update=_fanning_update,
  .render=_fanning_render,
};

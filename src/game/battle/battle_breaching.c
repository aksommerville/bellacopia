/* battle_breaching.c
 */

#include "game/bellacopia.h"

struct battle_breaching {
  struct battle hdr;
  double playtime;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double x,y,t;
    int indt,inkick; // Controllers set only these.
    int blackout;
    double animclock;
    int animframe;
    double kickclock;
    double velocity;
    int pvinkick;
    double decel;
    double kicktime;
    double velmax;
    double vellimit;
    int breaching;
    double breachdx,breachdy;
    double birdx,birdy;
    uint8_t birdxform;
  } playerv[2];
};

#define BATTLE ((struct battle_breaching*)battle)

/* Delete.
 */
 
static void _breaching_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.333;
    player->t=M_PI*0.250; // Don't start upright, because that's a great angle.
    player->birdx=(FBW>>1)-10;
    player->birdy=60.0;
    player->birdxform=EGG_XFORM_XREV;
  } else { // Right.
    player->who=1;
    player->x=FBW*0.666;
    player->t=M_PI*-0.250;
    player->birdx=(FBW>>1)+10;
    player->birdy=60.0;
    player->birdxform=0;
  }
  player->y=120.0;
  // (decel,kicktime,velmax) must be such that some velocity remains when the kick completes and we become interactive again.
  player->decel=55.0*(1.0-player->skill)+55.0*player->skill;
  player->kicktime=0.500*(1.0-player->skill)+0.400*player->skill;
  player->velmax=85.0*(1.0-player->skill)+95.0*player->skill;
  player->vellimit=140.0*(1.0-player->skill)+160.0*player->skill;
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->velmax*=0.900; // CPU penalty. He's pretty easy anyway at 0x80, but I want to be really no challenge at all.
    player->vellimit*=0.900;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x303137ff;
        player->tileid=0xe6;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0xa6;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0xc6;
      } break;
  }
}

/* New.
 */
 
static int _breaching_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playtime=9.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->inkick=(input&EGG_BTN_SOUTH);
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
  if (player->breaching) return;
  
  /* Aim for an upward angle, tilted toward the further horizontal screen edge.
   */
  double targett=M_PI*0.100;
  if (player->x>200.0) targett*=-1.0;
  else if (player->y<120.0) ;
  else if (player->t<0.0) targett*=-1.0;
  
  double dt=targett-player->t;
  if (dt<-M_PI) dt+=M_PI*2.0;
  else if (dt>M_PI) dt-=M_PI*2.0;
  if (dt<-0.010) player->indt=-1;
  else if (dt>0.010) player->indt=1;
  else player->indt=0;
  
  /* Kick whenever we can.
   */
  if (player->kickclock<=0.0) player->inkick=1;
  else player->inkick=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* When the battle ends, let the current breach finish (it won't change the outcome), then turn upright and approach the surface.
   */
  if (battle->outcome>-2) {
    player->indt=0;
    player->inkick=0;
    if (!player->breaching) {
      if (player->t<-0.100) player->t+=3.000*elapsed;
      else if (player->t>0.100) player->t-=3.000*elapsed;
      else player->t=0.0;
      if ((player->velocity-=40.0*elapsed)<=40.0) player->velocity=40.0;
      player->x+=sin(player->t)*player->velocity*elapsed;
      player->y-=cos(player->t)*player->velocity*elapsed;
      if (player->x<0.0) player->x=0.0;
      else if (player->x>320.0) player->x=320.0;
      if (player->y<70.0) player->y=70.0;
      return;
    }
  }

  /* Rotate?
   */
  if (player->indt&&!player->breaching) {
    player->t+=5.000*player->indt*elapsed;
    if (player->t<-M_PI) player->t+=M_PI*2.0;
    if (player->t>M_PI) player->t-=M_PI*2.0;
  }
  
  /* Kicking?
   */
  int kick_down=0;
  if (player->breaching) {
    player->kickclock=0.0;
  } else {
    if (player->inkick!=player->pvinkick) {
      if (player->pvinkick=player->inkick) {
        kick_down=1;
      }
    }
    if (player->kickclock>0.0) {
      player->kickclock-=elapsed;
    } else if (kick_down) {
      player->kickclock=player->kicktime;
      player->velocity+=player->velmax;
      if (player->velocity>player->vellimit) player->velocity=player->vellimit;
      player->animclock=0.0;
      player->animframe=0;
    }
  }
  
  /* Animation.
   */
  if (player->kickclock<=0.0) {
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.250;
      if (++(player->animframe)>=2) player->animframe=0;
    }
  }
  
  /* Motion?
   */
  if (player->breaching) {
    if ((player->breachdy+=240.0*elapsed)>200.0) player->breachdy=200.0;
    player->t=atan2(player->breachdx,-player->breachdy);
    player->x+=player->breachdx*elapsed;
    player->y+=player->breachdy*elapsed;
    if ((battle->outcome==-2)&&(player->y<player->birdy)) player->birdy=player->y;
    if (player->x<0.0) player->x=0.0;
    else if (player->x>320.0) player->x=320.0;
    if (player->y>70.0) {
      bm_sound_pan(RID_sound_glug2,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->breaching=0;
      player->velocity=sqrt(player->breachdx*player->breachdx+player->breachdy*player->breachdy);
    }
  } else if ((player->velocity-=player->decel*elapsed)<=0.0) {
    player->velocity=0.0;
  } else {
    player->x+=sin(player->t)*player->velocity*elapsed;
    player->y-=cos(player->t)*player->velocity*elapsed;
    int scrape=0;
    if (player->x<0.0) { player->x=0.0; scrape=1; }
    else if (player->x>320.0) { player->x=320.0; scrape=1; }
    if (player->y>160.0) { player->y=160.0; scrape=1; }
    else if (player->y<70.0) {
      scrape=0;
      player->kickclock=0.0;
      player->breaching=1;
      bm_sound_pan(RID_sound_glug,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->breachdx=sin(player->t)*player->velocity;
      player->breachdy=-cos(player->t)*player->velocity;
      if (player->breachdy>-1.0) player->breachdy=-1.0;
      player->velocity=0.0;
    }
    // Decelerate dramatically if we hit the edge.
    if (scrape) {
      if ((player->velocity-=100.0*elapsed)<=0.0) player->velocity=0.0;
    }
  }
}

/* Update.
 */
 
static void _breaching_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  if (battle->outcome==-2) {
    if ((BATTLE->playtime-=elapsed)<=0.0) {
      struct player *l=BATTLE->playerv;
      struct player *r=l+1;
      if (l->birdy<r->birdy) battle->outcome=1;
      else if (l->birdy>r->birdy) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  uint8_t tileid=player->tileid;
  if (player->kickclock>0.0) tileid+=4;
  else if (player->animframe) tileid+=2;
  int srcx=(tileid&0x0f)*NS_sys_tilesize;
  int srcy=(tileid>>4)*NS_sys_tilesize;
  int w=NS_sys_tilesize*2;
  double sint=sin(player->t);
  double cost=cos(player->t);
  graf_decal_rotate(&g.graf,(int)player->x,(int)player->y,srcx,srcy,w,sint,cost,1.0);
}

/* Render.
 */
 
static void _breaching_render(struct battle *battle) {

  /* Fill with sea blue, then tile a bunch of 3x1 tile decals for the sky, waves, and floor.
   */
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x0020e0ff);
  graf_set_image(&g.graf,RID_image_battle_sea);
  {
    int srcx_floor=128;
    int srcy_floor=16;
    int srcx_waves=176;
    int srcy_waves=32;
    int srcx_sky=176;
    int srcy_sky=16;
    int w=48;
    int h=16;
    int x=0;
    int y_floor=FBH-h;
    int y_waves=60;
    for (;x<FBW;x+=w) {
      graf_decal(&g.graf,x,y_floor,srcx_floor,srcy_floor,w,h);
      graf_decal(&g.graf,x,y_waves,srcx_waves,srcy_waves,w,h);
      int y=y_waves-h;
      for (;y>=-h;y-=h) {
        graf_decal(&g.graf,x,y,srcx_sky,srcy_sky,w,h);
      }
    }
  }
  
  /* Players as a rotated decal.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_set_filter(&g.graf,1);
  player_render(battle,l);
  player_render(battle,r);
  graf_set_filter(&g.graf,0);
  
  /* A bird for each player.
   */
  uint8_t birdtile=(g.framec&16)?0x0d:0x0e;
  graf_fancy(&g.graf,(int)l->birdx,(int)l->birdy,birdtile,l->birdxform,0,NS_sys_tilesize,0,l->color);
  graf_fancy(&g.graf,(int)r->birdx,(int)r->birdy,birdtile,r->birdxform,0,NS_sys_tilesize,0,r->color);
  
  /* Clock.
   */
  if (battle->outcome==-2) {
    int sec=(int)(BATTLE->playtime+0.999);
    if (sec<1) sec=1; else if (sec>9) sec=9;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_tile(&g.graf,FBW>>1,FBH-10,'0'+sec,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_breaching={
  .name="breaching",
  .objlen=sizeof(struct battle_breaching),
  .id=NS_battle_breaching,
  .strix_name=308,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz_a,
  .imageid_default=0,
  .del=_breaching_del,
  .init=_breaching_init,
  .update=_breaching_update,
  .render=_breaching_render,
};

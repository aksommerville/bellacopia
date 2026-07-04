/* battle_flapping.c
 */

#include "game/bellacopia.h"

struct battle_flapping {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    int y; // Framebuffer pixels, center of 32x32 image.
    int flap; // Per controller;
    int inpoison;
    int pvflap;
    double flapclock; // If >0, render flapped.
    double flaptime; // Const.
    double boatx;
    double bobblephase; // Radians, moves constantly higher and wraps around.
    double bobblerate; // rad/sec
    double bobbleeffect; // Sine of bobblephase. Expresses as rendered y offset, and affects wind delivery.
    double windlo,windhi; // Const, px/s. Range of deliveries per flap.
    double decel; // Const, px/s**2.
    double wind; // px/s, boat's current velocity.
    int done;
    double runclock;
    double cpuclock;
    double cputime;
  } playerv[2];
};

#define BATTLE ((struct battle_flapping*)battle)

/* Delete.
 */
 
static void _flapping_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->y=70;
  } else { // Right.
    player->who=1;
    player->y=130;
  }
  player->boatx=80.0;
  player->flaptime=0.666;
  player->bobblephase=((rand()&0xffff)/65535.0)*M_PI*2.0;
  player->bobblerate=5.000;
  player->windhi=10.0*(1.0-player->skill)+20.0*player->skill;
  player->decel=2.0;
  if (player->human=human) { // Human.
    player->inpoison=1;
  } else { // CPU.
    player->windhi*=0.800; // CPU penalty.
    player->cpuclock=0.600;
    player->cputime=0.850;
  }
  player->windlo=player->windhi*0.200;
  switch (face) {
    case NS_face_monster: {
        player->color=0xb8af9dff;
        player->tileid=0x80;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x40;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x60;
      } break;
  }
}

/* New.
 */
 
static int _flapping_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->inpoison) {
    if (!(input&EGG_BTN_SOUTH)) player->inpoison=0;
  } else {
    player->flap=(input&EGG_BTN_SOUTH)?1:0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    player->flap=0;
    return;
  }
  player->cpuclock+=player->cputime+((rand()&0xffff)*0.100)/65535.0;
  player->flap=1;
}

/* Apply a new flap to one player's boat.
 */
 
static void player_apply_flap(struct battle *battle,struct player *player) {
  double n=(player->bobbleeffect+1.0)/2.0;
  n*=n;
  n*=n;
  //fprintf(stderr,"%s n=%f\n",__func__,n);
  double addvel=player->windlo*(1.0-n)+player->windhi*n;
  player->wind+=addvel;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (player->flapclock>0.0) {
    player->flapclock-=elapsed;
  }

  player->bobblephase+=player->bobblerate*elapsed;
  if (player->bobblephase>=M_PI) player->bobblephase-=M_PI*2.0;
  player->bobbleeffect=sin(player->bobblephase);
  
  if (player->done) {
    player->wind=0.0;
    return;
  }
  player->runclock+=elapsed;

  if (player->flap!=player->pvflap) {
    player->pvflap=player->flap;
    if (player->flap&&(player->flapclock<=0.0)) {
      player->flapclock=player->flaptime;
      player_apply_flap(battle,player);
    }
  }
  
  player->boatx+=player->wind*elapsed;
  const double edgex=250.0;
  if (player->boatx>=edgex) {
    player->boatx=edgex;
    player->done=1;
  }
  if ((player->wind-=player->decel*elapsed)<=0.0) player->wind=0.0;
}

/* Update.
 */
 
static void _flapping_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->done&&r->done) {
      if (l->runclock<r->runclock) battle->outcome=1;
      else if (l->runclock>r->runclock) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int x=40;
  const int ht=NS_sys_tilesize>>1;
  uint8_t tileid=player->tileid;
  if (player->flapclock>0.0) tileid+=2;
  graf_tile(&g.graf,x-ht,player->y-ht,tileid+0x00,0);
  graf_tile(&g.graf,x+ht,player->y-ht,tileid+0x01,0);
  graf_tile(&g.graf,x-ht,player->y+ht,tileid+0x10,0);
  graf_tile(&g.graf,x+ht,player->y+ht,tileid+0x11,0);
  
  int bobble=6-(int)((player->bobbleeffect+1.0)*3.0);
  if (bobble<1) bobble=1; else if (bobble>6) bobble=6;
  int bx=(int)player->boatx-NS_sys_tilesize;
  int by=player->y-NS_sys_tilesize+bobble;
  int bh=NS_sys_tilesize*2-bobble;
  int bsrcx=NS_sys_tilesize*4;
  int bsrcy=NS_sys_tilesize*4;
  if (player->wind>=40.0) bsrcy+=NS_sys_tilesize*4;
  else if (player->wind>=10.0) bsrcy+=NS_sys_tilesize*2;
  graf_decal(&g.graf,bx,by,bsrcx,bsrcy,NS_sys_tilesize*2,bh);
}

static void render_time(struct battle *battle,struct player *player) {
  int ms=(int)(player->runclock*1000.0);
  if (ms<0) ms=0;
  int sec=ms/1000; ms%=1000;
  if (sec>99) {
    sec=99;
    ms=999;
  }
  int x=275;
  if (sec>=10) graf_tile(&g.graf,x,player->y,'0'+sec/10,0); x+=8;
  graf_tile(&g.graf,x,player->y,'0'+sec%10,0); x+=8;
  graf_tile(&g.graf,x,player->y,'.',0); x+=8;
  graf_tile(&g.graf,x,player->y,'0'+ms/100,0); x+=8;
  graf_tile(&g.graf,x,player->y,'0'+(ms/10)%10,0); x+=8;
  graf_tile(&g.graf,x,player->y,'0'+ms%10,0);
}

/* Render.
 */
 
static void _flapping_render(struct battle *battle) {
  const int HORIZONY=40;
  const int SHOREW=60;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,HORIZONY,SHOREW,FBH-HORIZONY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,FBW-SHOREW,HORIZONY,SHOREW,FBH-HORIZONY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,SHOREW,HORIZONY+4,FBW-(SHOREW<<1),FBH-HORIZONY,0x3060ffff);
  graf_fill_rect(&g.graf,0,HORIZONY,SHOREW,1,0x000000ff);
  graf_fill_rect(&g.graf,SHOREW,HORIZONY+4,FBW-(SHOREW<<1),1,0x000000ff);
  graf_fill_rect(&g.graf,FBW-SHOREW,HORIZONY,SHOREW,1,0x000000ff);
  graf_fill_rect(&g.graf,SHOREW,HORIZONY,1,FBH-HORIZONY,0x000000ff);
  graf_fill_rect(&g.graf,FBW-SHOREW,HORIZONY,1,FBH-HORIZONY,0x000000ff);
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_set_image(&g.graf,RID_image_battle_sea);
  player_render(battle,l);
  player_render(battle,r);
  
  // Show times for finished players.
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_set_tint(&g.graf,battle->ctab[BATTLE_COLOR_GROUND_TEXT]);
  if (l->done) render_time(battle,l);
  if (r->done) render_time(battle,r);
  graf_set_tint(&g.graf,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_flapping={
  .name="flapping",
  .objlen=sizeof(struct battle_flapping),
  .id=NS_battle_flapping,
  .strix_name=272,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_flapping_del,
  .init=_flapping_init,
  .update=_flapping_update,
  .render=_flapping_render,
};

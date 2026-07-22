/* battle_stirring.c
 */

#include "game/bellacopia.h"

#define REMARK_LIMIT 32

struct battle_stirring {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid_body; // 2x2
    uint8_t tileid_arm; // single
    uint8_t armdir; // 0x40,0x10,0x08,0x02, or 0x00 initially
    uint8_t xform;
    int x; // Center of hero.
    uint8_t pvarmdir;
    int score;
    struct remark {
      double x,y,dx,dy,ttl;
      uint8_t tileid;
    } remarkv[REMARK_LIMIT];
    int remarkc;
    double cpuclock;
    double cputime;
  } playerv[2];
};

#define BATTLE ((struct battle_stirring*)battle)

/* Delete.
 */
 
static void _stirring_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW/3;
    player->xform=0;
  } else { // Right.
    player->who=1;
    player->x=(FBW*2)/3;
    player->xform=EGG_XFORM_XREV;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cputime=0.400*(1.0-player->skill)+0.100*player->skill;
    player->cpuclock=player->cputime+0.250;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xb1aa94ff;
        player->tileid_body=0x4a;
        player->tileid_arm=0x59;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid_body=0x0a;
        player->tileid_arm=0x14;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid_body=0x2a;
        player->tileid_arm=0x34;
      } break;
  }
}

/* New.
 */
 
static int _stirring_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_LEFT: player->armdir=0x10; break;
    case EGG_BTN_RIGHT: player->armdir=0x08; break;
    case EGG_BTN_UP: player->armdir=0x40; break;
    case EGG_BTN_DOWN: player->armdir=0x02; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->cpuclock-=elapsed)>0.0) return;
  player->cpuclock+=player->cputime;
  switch (player->armdir) {
    case 0x40: player->armdir=0x08; break;
    case 0x08: player->armdir=0x02; break;
    case 0x02: player->armdir=0x10; break;
    default: player->armdir=0x40;
  }
}

/* Add remark.
 */
 
static void player_add_remark(struct battle *battle,struct player *player,uint8_t tileid) {
  if (player->remarkc>=REMARK_LIMIT) return;
  struct remark *remark=player->remarkv+player->remarkc++;
  remark->x=player->x;
  if (player->xform&EGG_XFORM_XREV) remark->x-=12.0;
  else remark->x+=12.0;
  remark->y=110.0;
  remark->dx=((rand()&0xffff)*40.0)/65535.0-20.0;
  remark->dy=-40.0;
  remark->ttl=0.500;
  remark->tileid=tileid;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* If armdir changed, rate it.
   * We don't require continuity of the circle, just each stroke must be perpendicular to the last.
   * If a player wants to stir just one corner, they're welcome to.
   */
  if (player->armdir!=player->pvarmdir) {
    int good=0;
    switch (player->pvarmdir) {
      case 0: good=1; break; // From the initial position, your first stroke is always kosher.
      case 0x40: good=(player->armdir&0x18); break;
      case 0x10: good=(player->armdir&0x42); break;
      case 0x08: good=(player->armdir&0x42); break;
      case 0x02: good=(player->armdir&0x18); break;
    }
    player->pvarmdir=player->armdir;
    if (good) {
      player->score++;
      bm_sound_pan(RID_sound_tick,player->who?PLAYER_PAN:-PLAYER_PAN);
      player_add_remark(battle,player,0x78);
    } else {
      player->score--;
      if (player->score<0) player->score=0;
      bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
      player_add_remark(battle,player,0x79);
    }
  }
  
  /* Update remarks.
   */
  struct remark *remark=player->remarkv+player->remarkc-1;
  int i=player->remarkc;
  for (;i-->0;remark--) {
    if ((remark->ttl-=elapsed)<=0.0) {
      player->remarkc--;
      memmove(remark,remark+1,sizeof(struct remark)*(player->remarkc-i));
    } else {
      remark->x+=remark->dx*elapsed;
      remark->y+=remark->dy*elapsed;
    }
  }
}

/* Update.
 */
 
static void _stirring_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (battle->outcome==-2) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
    player_update_common(battle,player,elapsed);
  }
  
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    const int winscore=40;
    if (l->score>=winscore) {
      if (r->score>=winscore) battle->outcome=0;
      else battle->outcome=1;
    } else if (r->score>=winscore) battle->outcome=-1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player,int groundy) {

  int x0=player->x-(NS_sys_tilesize>>1);
  int x1=x0+NS_sys_tilesize;
  int x2=x1+6; // cauldron
  int y0=groundy-NS_sys_tilesize-(NS_sys_tilesize>>1);
  int y1=y0+NS_sys_tilesize;
  int y2=y1+4; // cauldron
  int armx=x1+1;
  int army=y1-6;
  int armdx=1;
  if (player->xform&EGG_XFORM_XREV) {
    x1=x0;
    x0=x1+NS_sys_tilesize;
    x2=x1-6;
    armx=x1-1;
    armdx=-1;
  }
  switch (player->armdir) {
    case 0x40: army-=2; break;
    case 0x10: armx+=-2*armdx; break;
    case 0x08: armx+=2*armdx; break;
    case 0x02: army+=2; break;
  }
  
  graf_tile(&g.graf,x2,y2,0x19,0);
  graf_tile(&g.graf,armx,army,player->tileid_arm,player->xform);
  graf_tile(&g.graf,x0,y0,player->tileid_body+0x00,player->xform);
  graf_tile(&g.graf,x1,y0,player->tileid_body+0x01,player->xform);
  graf_tile(&g.graf,x0,y1,player->tileid_body+0x10,player->xform);
  graf_tile(&g.graf,x1,y1,player->tileid_body+0x11,player->xform);
  
  struct remark *remark=player->remarkv;
  int i=player->remarkc;
  for (;i-->0;remark++) {
    graf_tile(&g.graf,(int)remark->x,(int)remark->y,remark->tileid,0);
  }
}

/* Render.
 */
 
static void _stirring_render(struct battle *battle) {

  const int groundy=130;
  graf_fill_rect(&g.graf,0,0,FBW,groundy,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,groundy,FBW,FBH-groundy,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,groundy,FBW,1,0x000000ff);
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_set_image(&g.graf,RID_image_battle_forest);
  player_render(battle,l,groundy);
  player_render(battle,r,groundy);
  
  // Score bars.
  if (l->score>0) graf_fill_rect(&g.graf,(FBW>>1)-8,groundy-l->score,6,l->score,l->color);
  if (r->score>0) graf_fill_rect(&g.graf,(FBW>>1)+2,groundy-r->score,6,r->score,r->color);
}

/* Type definition.
 */
 
const struct battle_type battle_type_stirring={
  .name="stirring",
  .objlen=sizeof(struct battle_stirring),
  .id=NS_battle_stirring,
  .strix_name=294,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_dpad,
  .imageid_default=0,
  .del=_stirring_del,
  .init=_stirring_init,
  .update=_stirring_update,
  .render=_stirring_render,
};

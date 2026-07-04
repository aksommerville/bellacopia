/* battle_fire.c
 * Foxfire burns and burns!
 */

#include "game/bellacopia.h"

#define GROUNDY 130

struct battle_fire {
  struct battle hdr;
  int burning;
  double burnclock;
  int burnframe; // Not sequential. Random in 0..35.
  uint32_t burncolor;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // Top left of the first of two 2x2 pics.
    uint8_t xform;
    int x; // Center of 2x2-meter pic.
    double rub; // 0..1 = near..far, position of the stick.
    double drub; // Signed hz.
    double heat; // How many stick lengths travelled, less cooldown.
    double heatmax; // How much heat to ignite. Should be the main expression of skill.
    double speed; // hz. Must be greater than (coolrate).
    double coolrate; // hz, heat loss. Positive.
    int input; // -1,0,1 = absolute left,idle,right. Controller sets.
    double cputurnrate; // sec, how often do we change direction. Should be the main driver of difficulty.
    double cputurnclock;
  } playerv[2];
};

#define BATTLE ((struct battle_fire*)battle)

/* Delete.
 */
 
static void _fire_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=(FBW>>1)-24;
    player->xform=0;
  } else { // Right.
    player->who=1;
    player->x=(FBW>>1)+24;
    player->xform=EGG_XFORM_XREV;
  }
  player->rub=0.5;
  player->drub=0.0;
  player->speed=5.0; // sticks/sec
  player->coolrate=3.0; // -sticks/sec
  // Using (8,4) for heatmax and (280,200) for cputurnrate, I can win pretty easy at 0xa0, and with determined effort at 0xc0. 0x80 is a breeze.
  player->heatmax=8.0*(1.0-player->skill)+4.0*player->skill;
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cputurnclock=0.700; // Wait a sec before the first stroke. The initial delta is zero.
    // At speed==5, a turn rate of 200 ms or lower should yield perfect play. (allowing for some rounding and frame-rate fuzz).
    player->cputurnrate=0.280*(1.0-player->skill)+0.200*player->skill;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xb05610ff;
        player->tileid=0x40;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x00;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x20;
      } break;
  }
}

/* New.
 */
 
static int _fire_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->input=-1; break;
    case EGG_BTN_RIGHT: player->input=1; break;
    default: player->input=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->cputurnclock-=elapsed)<=0.0) {
    player->cputurnclock+=player->cputurnrate;
    // Don't just flip the sign here: First stroke, it's zero.
    if (player->input>0) player->input=-1;
    else if (player->input<0) player->input=1;
    else player->input=(rand()&1)?-1:1; // First stroke, random direction, just for appearances.
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // If (input) disagrees with (drub), update (drub).
  if ((player->input<0)&&(player->drub>=0.0)) {
    player->drub=-player->speed;
  } else if ((player->input>0)&&(player->drub<=0.0)) {
    player->drub=player->speed;
  }
  
  // Advance rub, and zero the delta when we hit an edge.
  // If we didn't hit an edge, advance (heat) per (drub). (heat) is in pixels.
  double delta=player->drub*elapsed;
  player->rub+=delta;
  if (player->rub<=0.0) {
    player->rub=0.0;
    player->drub=0.0;
  } else if (player->rub>=1.0) {
    player->rub=1.0;
    player->drub=0.0;
  } else {
    if (delta<0.0) delta=-delta;
    player->heat+=delta;
  }
  
  // Lose heat constantly.
  if ((player->heat-=player->coolrate*elapsed)<=0.0) player->heat=0.0;
}

/* Update.
 */
 
static void _fire_update(struct battle *battle,double elapsed) {
  
  // If we're burning, animate it randomly.
  if (BATTLE->burning) {
    if ((BATTLE->burnclock-=elapsed)<=0.0) {
      BATTLE->burnclock+=0.150;
      BATTLE->burnframe=rand()%36;
    }
  }
  
  // Players update only before the outcome is established.
  if (battle->outcome==-2) {
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
      else player_update_cpu(battle,player,elapsed);
      player_update_common(battle,player,elapsed);
    }
  }
  
  // End of game?
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->heat>=l->heatmax) {
      if (r->heat>=r->heatmax) battle->outcome=0; // Ties are possible but unlikely.
      else battle->outcome=1;
    } else if (r->heat>=r->heatmax) {
      battle->outcome=-1;
    }
    if (battle->outcome==1) {
      BATTLE->burning=1;
      BATTLE->burncolor=l->color;
    } else if (battle->outcome==-1) {
      BATTLE->burning=1;
      BATTLE->burncolor=r->color;
    } else if (battle->outcome==0) {
      // Ties just aren't going to happen, but they are technically possible. Dunno about the color, whatever, say red.
      BATTLE->burning=1;
      BATTLE->burncolor=0xff0000ff;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  graf_set_image(&g.graf,RID_image_battle_forest);
  
  // Take some measurements and pick my body tiles.
  int xa=player->x-(NS_sys_tilesize>>1);
  int xb=xa+NS_sys_tilesize;
  int yb=GROUNDY-(NS_sys_tilesize>>1);
  int ya=yb-NS_sys_tilesize;
  if (player->xform) {
    int tmp=xa;
    xa=xb;
    xb=tmp;
  }
  int draw_stick=1; // Only omit the stick if we won; the victory frame has both arms.
  uint8_t tileid=player->tileid;
  if (
    ((battle->outcome==1)&&!player->who)||
    ((battle->outcome==-1)&&player->who)
  ) {
    tileid+=2;
    draw_stick=0;
  }
  
  // Body tiles.
  graf_tile(&g.graf,xa,ya,tileid+0x00,player->xform);
  graf_tile(&g.graf,xb,ya,tileid+0x01,player->xform);
  graf_tile(&g.graf,xa,yb,tileid+0x10,player->xform);
  graf_tile(&g.graf,xb,yb,tileid+0x11,player->xform);
  
  // Stick.
  if (draw_stick) {
    int dx=(int)(player->rub*5.5);
    if (dx<0) dx=0; else if (dx>5) dx=5;
    int x;
    if (player->xform) {
      x=xb-1-dx;
    } else {
      x=xb+1+dx;
    }
    int y=yb-5;
    graf_tile(&g.graf,x,y,player->tileid+4,player->xform);
  }
  
  // Power meter.
  if (battle->outcome==-2) {
    int y=ya-NS_sys_tilesize;
    graf_tile(&g.graf,xa,y,0x68,player->xform);
    graf_tile(&g.graf,xb,y,0x69,player->xform);
    int darkx,lightx;
    int barw=19,barh=4;
    int bary=y-2;
    int lightw=(int)((player->heat*barw)/player->heatmax);
    if (lightw<0) lightw=0; else if (lightw>barw) lightw=barw;
    int darkw=barw-lightw;
    if (player->xform) {
      lightx=xa-4-lightw;
      darkx=lightx-darkw;
    } else {
      lightx=xa+4;
      darkx=lightx+lightw;
    }
    if (darkw>0) graf_fill_rect(&g.graf,darkx,bary,darkw,barh,0x5a3e16ff);
    if (lightw>0) graf_fill_rect(&g.graf,lightx,bary,lightw,barh,0xf10b0bff);
  }
}

/* Render.
 */
 
static void _fire_render(struct battle *battle) {

  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  
  // The fire. 2x2 tiles.
  {
    const int dstx=(FBW>>1)-NS_sys_tilesize;
    const int dsty=GROUNDY-NS_sys_tilesize*2;
    int xa=dstx+(NS_sys_tilesize>>1);
    int xb=xa+NS_sys_tilesize;
    int ya=dsty+(NS_sys_tilesize>>1);
    int yb=ya+NS_sys_tilesize;
    graf_set_image(&g.graf,RID_image_battle_forest);
    graf_tile(&g.graf,xa,ya,0x60,0);
    graf_tile(&g.graf,xb,ya,0x61,0);
    graf_tile(&g.graf,xa,yb,0x70,0);
    graf_tile(&g.graf,xb,yb,0x71,0);
    if (BATTLE->burning) {
      int plan=BATTLE->burnframe;
      int n;
      n=plan%3-1; plan/=3;
      xa+=n; xb+=n;
      n=plan%3-1; plan/=3;
      ya+=n; yb+=n;
      uint8_t xform=(plan&1)?EGG_XFORM_XREV:0;
      if (xform) {
        int tmp=xa;
        xa=xb;
        xb=tmp;
      }
      plan>>=1;
      uint8_t tileid=plan;
      if (tileid>2) tileid=0;
      tileid<<=1;
      tileid+=0x62;
      graf_fancy(&g.graf,xa,ya,tileid+0x00,xform,0,NS_sys_tilesize,0,BATTLE->burncolor);
      graf_fancy(&g.graf,xb,ya,tileid+0x01,xform,0,NS_sys_tilesize,0,BATTLE->burncolor);
      graf_fancy(&g.graf,xa,yb,tileid+0x10,xform,0,NS_sys_tilesize,0,BATTLE->burncolor);
      graf_fancy(&g.graf,xb,yb,tileid+0x11,xform,0,NS_sys_tilesize,0,BATTLE->burncolor);
    }
  }
  
  // The two players.
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_fire={
  .name="fire",
  .objlen=sizeof(struct battle_fire),
  .id=NS_battle_fire,
  .strix_name=267,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_fire_del,
  .init=_fire_init,
  .update=_fire_update,
  .render=_fire_render,
};

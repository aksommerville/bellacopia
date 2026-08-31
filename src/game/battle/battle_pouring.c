/* battle_pouring.c
 */

#include "game/bellacopia.h"

#define PLAYER_STATE_READY 0
#define PLAYER_STATE_POUR 1
#define PLAYER_STATE_DONE 2

struct battle_pouring {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid_body; // 2x2
    uint8_t tileid_arm; // Single, 3 options arranged horizontally: idle, pour, done
    int blackout;
    int inpour;
    int state; // PLAYER_STATE_*
    double tptime; // Flying teapot time, seconds, counting up.
    double animclock;
    int animframe;
    double fill; // Pixels, 0..30
    int overflow;
    double startdelay; // cpu
    double cpufill; // cpu
  } playerv[2];
};

#define BATTLE ((struct battle_pouring*)battle)

/* Delete.
 */
 
static void _pouring_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->startdelay=0.250+1.000*((rand()&0xffff)/65535.0);
    // Sometimes, randomly, the CPU will overflow. So humans shouldn't just watch him and release right after.
    int spillodds=(int)(0x8000*(1.0-player->skill)+0x2000*player->skill);
    if ((rand()&0xffff)<spillodds) {
      player->cpufill=35.0;
    } else {
      player->cpufill=20.0*(1.0-player->skill)+29.0*player->skill;
    }
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x8d5f1fff;
        player->tileid_body=0x1e;
        player->tileid_arm=0x5c;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid_body=0x0a;
        player->tileid_arm=0x3c;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid_body=0x2a;
        player->tileid_arm=0x4c;
      } break;
  }
}

/* New.
 */
 
static int _pouring_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->inpour=(input&EGG_BTN_SOUTH);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->startdelay>0.0) {
    player->startdelay-=elapsed;
  } else if (player->fill<player->cpufill) {
    player->inpour=1;
  } else {
    player->inpour=0;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  switch (player->state) {
    case PLAYER_STATE_READY: {
        if (player->inpour) {
          // Begin pouring.
          player->state=PLAYER_STATE_POUR;
        }
      } break;
    case PLAYER_STATE_POUR: {
        if (!player->inpour) {
          // Stop pouring.
          player->state=PLAYER_STATE_DONE;
        } else {
          player->fill+=elapsed*10.0;
          if (player->fill>30.0) {
            player->state=PLAYER_STATE_DONE;
            player->overflow=1;
          }
        }
      } break;
    case PLAYER_STATE_DONE: {
        player->tptime+=elapsed;
        if ((player->animclock-=elapsed)<=0.0) {
          player->animclock+=0.150;
          if (++(player->animframe)>=4) player->animframe=0;
        }
      } break;
  }
}

/* Update.
 */
 
static void _pouring_update(struct battle *battle,double elapsed) {
  
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
    if ((l->state==PLAYER_STATE_DONE)&&(r->state==PLAYER_STATE_DONE)) {
      if (l->overflow&&r->overflow) battle->outcome=0; // Both overflowed: tie
      else if (l->overflow) battle->outcome=-1; // One overflowed: other wins
      else if (r->overflow) battle->outcome=1;
      else if (l->fill>r->fill) battle->outcome=1; // Most tea wins
      else if (l->fill<r->fill) battle->outcome=-1;
      else battle->outcome=0; // Identical pours. Narrowly possible.
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  /* Choose some layout parameters based on (who).
   */
  int xback,xfront,xarm;
  int ytop,ybottom,yarm;
  uint8_t xform;
  if (player->who) {
    xform=EGG_XFORM_XREV;
    xfront=(FBW*2)/3+7;
    xback=xfront+NS_sys_tilesize;
    xarm=xfront-3;
  } else {
    xform=0;
    xfront=FBW/3-7;
    xback=xfront-NS_sys_tilesize;
    xarm=xfront+3;
  }
  ybottom=FBH>>1;
  ytop=ybottom-NS_sys_tilesize;
  yarm=ybottom-8;
  if (player->tileid_body==0x1e) yarm+=2; // Beaver prefers a lower arm
  uint8_t tileid_arm=player->tileid_arm;
  switch (player->state) {
    case PLAYER_STATE_POUR: tileid_arm+=1; break;
    case PLAYER_STATE_DONE: tileid_arm+=2; break;
  }
  
  graf_tile(&g.graf,xarm,yarm,tileid_arm,xform);
  graf_tile(&g.graf,xback,ytop,player->tileid_body,xform);
  graf_tile(&g.graf,xfront,ytop,player->tileid_body+1,xform);
  graf_tile(&g.graf,xback,ybottom,player->tileid_body+0x10,xform);
  graf_tile(&g.graf,xfront,ybottom,player->tileid_body+0x11,xform);
  
  // Flying teapot.
  if (player->state==PLAYER_STATE_DONE) {
    int tpx=(int)(xarm+player->tptime*10.0*(player->who?-1.0:1.0));
    int tpy=(int)(yarm-player->tptime*40.0);
    uint8_t tileid=0x3f;
    switch (player->animframe) {
      case 1: tileid+=0x10; break;
      case 2: tileid+=0x20; break;
      case 3: tileid+=0x10; break;
    }
    graf_tile(&g.graf,tpx,tpy,tileid,xform);
  }
  
  // Teacup.
  int tcy=ybottom+NS_sys_tilesize;
  int tcx;
  if (player->who) {
    tcx=xfront-NS_sys_tilesize*2-(NS_sys_tilesize>>1)-1;
    graf_tile(&g.graf,xfront-NS_sys_tilesize*1,tcy,0xbd,EGG_XFORM_XREV);
    graf_tile(&g.graf,xfront-NS_sys_tilesize*2,tcy,0xbe,EGG_XFORM_XREV);
    graf_tile(&g.graf,xfront-NS_sys_tilesize*3,tcy,0xbf,EGG_XFORM_XREV);
    graf_tile(&g.graf,xfront-NS_sys_tilesize*1,tcy+NS_sys_tilesize,0xcd,EGG_XFORM_XREV);
    graf_tile(&g.graf,xfront-NS_sys_tilesize*2,tcy+NS_sys_tilesize,0xce,EGG_XFORM_XREV);
    graf_tile(&g.graf,xfront-NS_sys_tilesize*3,tcy+NS_sys_tilesize,0xcf,EGG_XFORM_XREV);
  } else {
    tcx=xfront+(NS_sys_tilesize>>1)+2;
    graf_tile(&g.graf,xfront+NS_sys_tilesize*1,tcy,0xbd,0);
    graf_tile(&g.graf,xfront+NS_sys_tilesize*2,tcy,0xbe,0);
    graf_tile(&g.graf,xfront+NS_sys_tilesize*3,tcy,0xbf,0);
    graf_tile(&g.graf,xfront+NS_sys_tilesize*1,tcy+NS_sys_tilesize,0xcd,0);
    graf_tile(&g.graf,xfront+NS_sys_tilesize*2,tcy+NS_sys_tilesize,0xce,0);
    graf_tile(&g.graf,xfront+NS_sys_tilesize*3,tcy+NS_sys_tilesize,0xcf,0);
  }
  int h=(int)player->fill;
  if (h>0) {
    if (h>30) h=30;
    graf_decal(&g.graf,tcx,tcy-(NS_sys_tilesize>>1)+30-h,208,208+30-h,32,h);
    // And if it overflowed, an extra indicator:
    if (player->overflow) {
      graf_set_tint(&g.graf,(player->animframe&2)?0xff0000ff:0xffff00ff);
      graf_decal(&g.graf,tcx,tcy-24,224,0,32,16);
      graf_set_tint(&g.graf,0);
    }
  }
}

/* Render.
 */
 
static void _pouring_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_set_image(&g.graf,RID_image_battle_forest);
  player_render(battle,l);
  player_render(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_pouring={
  .name="pouring",
  .objlen=sizeof(struct battle_pouring),
  .id=NS_battle_pouring,
  .strix_name=328,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .imageid_default=RID_image_mountains,
  .del=_pouring_del,
  .init=_pouring_init,
  .update=_pouring_update,
  .render=_pouring_render,
};

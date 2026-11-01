/* battle_karatechop.c
 */

#include "game/batsup/battle_internal.h"

struct battle_karatechop {
  struct battle hdr;
  double timer;
  int committed; // timer expired
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid_hand;
    uint8_t tileid_thing;
    int x; // Framebuffer pixels. Hand goes here, and the rest centered on it.
    int disposition; // -1=hurt, 0=pending, 1=broke
    double charge; // 0..1
    double peak; // 0..1, charge at the moment of commit (ie your score).
    int instate,pvinstate;
    double chargeup; // How much to add per keystroke.
    double chargedown; // hz, rate of decline.
    double thresh;
    double cpuclock;
    double cpuintervallo;
    double cpuintervalhi;
  } playerv[2];
};

#define BATTLE ((struct battle_karatechop*)battle)

/* Delete.
 */
 
static void _karatechop_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=FBW/3;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
  }
  
  player->chargeup=0.100*(1.0-player->skill)+0.150*player->skill;
  player->chargedown=0.400*(1.0-player->skill)+0.200*player->skill;
  player->thresh=0.750;
  
       if (player->skill>=0.800) player->tileid_thing=0x00; // egg
  else if (player->skill>=0.600) player->tileid_thing=0x04; // stick
  else if (player->skill>=0.300) player->tileid_thing=0x08; // bone -- default
  else                           player->tileid_thing=0x0c; // cinder block
  
  if (player->human=human) { // Human.
    player->instate=player->pvinstate=1;
  } else { // CPU.
    player->cpuintervallo=0.150*(1.0-player->skill)+0.140*player->skill;
    player->cpuintervalhi=player->cpuintervallo+0.080;
    player->cpuclock=0.500; // A tasteful intro delay.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xa8a0a0ff;
        player->tileid_hand=0x21;
      } break;
    case NS_face_dot: {
        player->color=0xe0c098ff;
        player->tileid_hand=0x21;
      } break;
    case NS_face_princess: {
        player->color=0xe4be8eff;
        player->tileid_hand=0x21;
      } break;
  }
}

/* New.
 */
 
static int _karatechop_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->timer=5.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  player->instate=(input&EGG_BTN_SOUTH)?1:0;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->cpuclock-=elapsed)<=0.0) {
    player->instate=1;
    double n=(rand()&0xffff)/65535.0;
    player->cpuclock+=player->cpuintervallo*(1.0-n)+player->cpuintervalhi*n;
  } else {
    player->instate=0;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Charge loss happens on a curve, it gets stronger near the top.
   * The top of the meter is three times as lossy as the bottom.
   */
  double chargedown=player->chargedown+player->chargedown*player->charge*2.0;
  if ((player->charge-=chargedown*elapsed)<=0.0) {
    player->charge=0.0;
  }

  if (player->instate!=player->pvinstate) {
    if (player->pvinstate=player->instate) {
      if ((player->charge+=player->chargeup)>=1.0) {
        player->charge=1.0;
      }
    }
  }
}

/* Update player, for the brief period between commit and complete.
 * The bit when our hand comes down fast.
 * Controller does not run anymore.
 */
 
static void player_update_post(struct battle *battle,struct player *player,double elapsed) {
  if (player->disposition) return;
  if ((player->charge-=3.000*elapsed)<=0.0) {
    player->charge=0.0;
    if (player->peak>=player->thresh) {
      bm_sound_pan(RID_sound_whack,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->disposition=1;
    } else if (player->peak>=0.100) {
      bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->disposition=-1;
    } else { // Extremely low, maybe they dropped the gamepad. Don't make a sound.
      player->disposition=-1;
    }
  }
}

/* Time up, start chopping!
 */
 
static void karatechop_commit(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  l->peak=l->charge;
  r->peak=r->charge;
  BATTLE->committed=1;
}

/* Update.
 */
 
static void _karatechop_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  if (BATTLE->committed) {
    for (;i-->0;player++) player_update_post(battle,player,elapsed);
  } else {
    for (;i-->0;player++) {
      if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
      else player_update_cpu(battle,player,elapsed);
      player_update_common(battle,player,elapsed);
    }
  }
  
  /* Both chops get committed when the timer expires.
   */
  if (!BATTLE->committed) {
    if ((BATTLE->timer-=elapsed)<=0.0) {
      karatechop_commit(battle);
    }
  }

  /* Game ends when committed and both players have a disposition.
   * Outcome is based only on (peak). It doesn't actually matter whether you break the thing or not, that's just decoration.
   * Ties are narrowly possible.
   */
  if ((battle->outcome==-2)&&BATTLE->committed) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->disposition&&r->disposition) {
      if (l->peak<r->peak) battle->outcome=-1;
      else if (l->peak>r->peak) battle->outcome=1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int colspace=16;
  int y=140;
  graf_tile(g_graf,player->x-colspace,y,0x30,0);
  graf_tile(g_graf,player->x+colspace,y,0x30,0);
  y-=NS_sys_tilesize;
  graf_tile(g_graf,player->x-colspace,y,0x20,0);
  graf_tile(g_graf,player->x+colspace,y,0x20,0);
  y-=4;
  uint8_t tileid=player->tileid_thing;
  if (player->disposition>0) tileid+=2;
  graf_tile(g_graf,player->x-8,y,tileid+0x10,0);
  graf_tile(g_graf,player->x+8,y,tileid+0x11,0);
  y-=NS_sys_tilesize;
  graf_tile(g_graf,player->x-8,y,tileid+0x00,0);
  graf_tile(g_graf,player->x+8,y,tileid+0x01,0);
  int bottom=y;
  int top=20;
  int handy=(int)(bottom*(1.0-player->charge)+top*player->charge);
  uint32_t color=player->color;
  uint8_t xform=player->who?EGG_XFORM_XREV:0;
  if (player->disposition<0) {
    color=0xff0000ff;
  }
  graf_fancy(g_graf,player->x,handy,player->tileid_hand,xform,0,NS_sys_tilesize,0,color);
  // Plus, if committed, draw the indicator hand.
  if (BATTLE->committed) {
    handy=(int)(bottom*(1.0-player->peak)+top*player->peak);
    int x=FBW>>1;
    if (player->who) x+=7; else x-=7;
    graf_fancy(g_graf,x,handy,player->tileid_hand+0x10,xform,0,NS_sys_tilesize,0,player->color);
  }
}

/* Render.
 */
 
static void _karatechop_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_set_image(g_graf,RID_image_battle_karatechop);
  
  // Once committed, draw a ruler for the peak hands to point at.
  if (BATTLE->committed) {
    int y=15;
    uint8_t tileid=0x40;
    for (;y<120;y+=NS_sys_tilesize,tileid=0x50) graf_tile(g_graf,FBW>>1,y,tileid,0);
    graf_tile(g_graf,FBW>>1,y,0x60,0);
  }
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // During main play, show the clock.
  if (!BATTLE->committed) {
    int s=(int)(BATTLE->timer+0.999);
    if (s<1) s=1; else if (s>9) s=9;
    graf_set_image(g_graf,RID_image_fonttiles);
    graf_set_tint(g_graf,battle->ctab[BATTLE_COLOR_SKY_TEXT]);
    graf_tile(g_graf,FBW>>1,30,'0'+s,0);
    graf_set_tint(g_graf,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_karatechop={
  .name="karatechop",
  .objlen=sizeof(struct battle_karatechop),
  .id=112,
  .strix_name=384,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_a_mash,
  .imageid_default=RID_image_caves,
  .del=_karatechop_del,
  .init=_karatechop_init,
  .update=_karatechop_update,
  .render=_karatechop_render,
};

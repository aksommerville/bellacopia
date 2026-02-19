/* battle_watching.c
 * Tiny Dot walks around a giant stovetop with four pots. Look at one to prevent it from boiling.
 */

#include "game/bellacopia.h"

#define FRAME_PERIOD 0.150
#define START_HEAT_RANGE 0.250 /* They start randomly from zero to this. */
#define COLD_HEAT 0.500 /* Below this, no animation. */
#define HOT_HEAT  0.750 /* Above this, take damage. */
#define COOLRATE_WORST 1.500
#define COOLRATE_BEST  1.800
#define HEATRATE_WORST 0.200
#define HEATRATE_BEST  0.150
#define HEATRATE_FUZZ  1.500 /* >1 */
#define DAMAGE_RATE 0.100 /* damage/second if a pot is at maximum heat */
#define FINGERTIME_WORST 0.500
#define FINGERTIME_BEST  0.125
#define HEATRATE_MAX   0.500
#define HEATRATE_ACCEL 0.010

struct battle_watching {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid; // Constant; the down frame. +1=up +2=right
    uint8_t position; // 0x40,0x10,0x00,0x08,0x02 = north,west,center,east,south
    uint8_t facedir; // 0x40,0x10,0x08,0x02
    double coolrate;
    double heatratelo,heatratehi; // Pots select randomly in this range, repeatedly.
    double damage; // 0..1, lose at 1
    double fingertime;
    double fingerclock;
    struct pot {
      double heatrate;
      double heat; // 0..1 = cold..hot
      double animclock;
      int animframe_fire;
      int animframe_soup;
      int animframe_lid;
    } potv[4];
    struct pot *cpufocus;
    int cpuaxis;
  } playerv[2];
};

#define BATTLE ((struct battle_watching*)battle)

/* Delete.
 */
 
static void _watching_del(struct battle *battle) {
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
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x20;
      } break;
    case NS_face_dot: {
        player->tileid=0x00;
      } break;
    case NS_face_princess: {
        player->tileid=0x10;
      } break;
  }
  player->coolrate=COOLRATE_WORST*(1.0-player->skill)+COOLRATE_BEST*player->skill;
  player->heatratelo=HEATRATE_WORST*(1.0-player->skill)+HEATRATE_BEST*player->skill;
  player->heatratehi=player->heatratelo*HEATRATE_FUZZ;
  player->fingertime=FINGERTIME_WORST*(1.0-player->skill)+FINGERTIME_BEST*player->skill;
  player->fingerclock=player->fingertime;
  player->cpuaxis=rand()&1;
  // Randomize pots.
  struct pot *pot=player->potv;
  int i=4;
  for (;i-->0;pot++) {
    pot->heatrate=player->heatratelo+(((rand()&0xffff)*(player->heatratehi-player->heatratelo))/65535.0);
    pot->heat=((rand()&0xffff)*START_HEAT_RANGE)/65535.0;
    pot->animclock=((rand()&0xffff)*FRAME_PERIOD)/65535.0;
    pot->animframe_fire=rand()&3;
    pot->animframe_soup=rand()&7;
    pot->animframe_lid=0; // All pots start cool.
  }
}

/* New.
 */
 
static int _watching_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Which pot am I looking at? Can be null.
 */
 
static struct pot *player_get_pot(struct player *player) {
  switch (player->position) {
    case 0x40: switch (player->facedir) {
        case 0x10: return player->potv+0;
        case 0x08: return player->potv+1;
      } break;
    case 0x10: switch (player->facedir) {
        case 0x40: return player->potv+0;
        case 0x02: return player->potv+2;
      } break;
    case 0x08: switch (player->facedir) {
        case 0x40: return player->potv+1;
        case 0x02: return player->potv+3;
      } break;
    case 0x02: switch (player->facedir) {
        case 0x10: return player->potv+2;
        case 0x08: return player->potv+3;
      } break;
  }
  return 0;
}

/* Move player or change direction.
 */
 
static void player_move(struct battle *battle,struct player *player,int dx,int dy) {

  uint8_t nface=player->facedir;
  if (dx<0) nface=0x10;
  else if (dx>0) nface=0x08;
  else if (dy<0) nface=0x40;
  else if (dy>0) nface=0x02;
  else return;
  
  uint8_t nposition=player->position;
  switch (player->position) {
    case 0x40: if (dy>0) nposition=0x00; break;
    case 0x10: if (dx>0) nposition=0x00; break;
    case 0x08: if (dx<0) nposition=0x00; break;
    case 0x02: if (dy<0) nposition=0x00; break;
    default: {
        if (dx<0) nposition=0x10;
        else if (dx>0) nposition=0x08;
        else if (dy<0) nposition=0x40;
        else nposition=0x02;
      } break;
  }
  
  if ((nface==player->facedir)&&(nposition==player->position)) {
    bm_sound_pan(RID_sound_reject,player->who?0.250:-0.250);
    return;
  }
  bm_sound_pan(RID_sound_uimotion,player->who?0.250:-0.250);
  
  struct pot *pvpot=player_get_pot(player);
  player->facedir=nface;
  player->position=nposition;
  if (pvpot) {
    pvpot->heatrate=player->heatratelo+(((rand()&0xffff)*(player->heatratehi-player->heatratelo))/65535.0);
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (input!=pvinput) {
    if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1,0);
    else if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1,0);
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) player_move(battle,player,0,-1);
    else if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) player_move(battle,player,0,1);
  }
}

/* Choose hottest pot, for the CPU player.
 */
 
static struct pot *player_choose_focus(struct battle *battle,struct player *player) {
  struct pot *hottest=0;
  struct pot *pot=player->potv;
  int i=4;
  for (;i-->0;pot++) {
    if (pot->heat<0.500) continue; // Not interested until a certain heat threshold.
    if (!hottest||(pot->heat>hottest->heat)) hottest=pot;
  }
  return hottest;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* We have a minimum time between actions.
   * If that hasn't elapsed yet, just roll with it.
   */
  if (player->fingerclock>0.0) {
    player->fingerclock-=elapsed;
    return;
  }

  /* If we're already watching a pot, stay with it until it cools off to a certain threshold.
   */
  if (player->cpufocus&&(player_get_pot(player)==player->cpufocus)) {
    if (player->cpufocus->heat>0.100) return;
  }
  
  /* Reexamine focus.
   * If it changed, toggle the axis.
   * Axis is decorative*, but it's really weird if we don't toggle it now and then.
   * [*] It's decorative for our purposes. In theory advantages could be had by picking the axis that lines you up with the second-hottest pot.
   */
  struct pot *nfocus=player_choose_focus(battle,player);
  if (nfocus!=player->cpufocus) {
    player->cpufocus=nfocus;
    player->cpuaxis^=1;
  }
  if (!player->cpufocus) return;
  
  /* Move or turn to watch the focus.
   * A nice quirk of our modelling is that swapping the axis just means swapping (facedir,position).
   */
  player->fingerclock+=player->fingertime;
  int focusix=player->cpufocus-player->potv;
  uint8_t nposition=player->position;
  uint8_t nfacedir=player->facedir;
  switch (focusix) {
    case 0: nposition=0x40; nfacedir=0x10; break;
    case 1: nposition=0x40; nfacedir=0x08; break;
    case 2: nposition=0x02; nfacedir=0x10; break;
    case 3: nposition=0x02; nfacedir=0x08; break;
  }
  if (player->cpuaxis) {
    uint8_t tmp=nposition;
    nposition=nfacedir;
    nfacedir=tmp;
  }
  
  if (nposition!=player->position) {
    switch (nposition) {
      case 0x40: switch (player->position) {
          case 0x10: player_move(battle,player,1,0); return;
          case 0x08: player_move(battle,player,-1,0); return;
          default: player_move(battle,player,0,-1); return;
        }
      case 0x10: switch (player->position) {
          case 0x40: player_move(battle,player,0,1); return;
          case 0x02: player_move(battle,player,0,-1); return;
          default: player_move(battle,player,-1,0); return;
        }
      case 0x08: switch (player->position) {
          case 0x40: player_move(battle,player,0,1); return;
          case 0x02: player_move(battle,player,0,-1); return;
          default: player_move(battle,player,1,0); return;
        }
      case 0x02: switch (player->position) {
          case 0x10: player_move(battle,player,1,0); return;
          case 0x08: player_move(battle,player,-1,0); return;
          default: player_move(battle,player,0,1); return;
        }
    }
  }
  if (nfacedir!=player->facedir) {
    switch (nfacedir) {
      case 0x40: player_move(battle,player,0,-1); return;
      case 0x10: player_move(battle,player,-1,0); return;
      case 0x08: player_move(battle,player,1,0); return;
      case 0x02: player_move(battle,player,0,1); return;
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void watch_pot(struct battle *battle,struct player *player,struct pot *pot,double elapsed) {
  if ((pot->heat-=player->coolrate*elapsed)<=0.0) pot->heat=0.0;
}
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Heat rates constantly increase to a limit where it's impossible not to have something boiling.
  if ((player->heatratelo+=elapsed*HEATRATE_ACCEL)>HEATRATE_MAX) player->heatratelo=HEATRATE_MAX;
  if ((player->heatratehi+=elapsed*HEATRATE_ACCEL)>HEATRATE_MAX) player->heatratehi=HEATRATE_MAX;

  // If we're watching a pot, reduce its heat.
  struct pot *watched=player_get_pot(player);
  if (watched) watch_pot(battle,player,watched,elapsed);
  
  // Increase heat and animate pots.
  struct pot *pot=player->potv;
  int i=4;
  for (;i-->0;pot++) {
    if ((pot->animclock-=elapsed)<=0.0) {
      pot->animclock+=FRAME_PERIOD;
      if (++(pot->animframe_fire)>=4) pot->animframe_fire=0;
      if (++(pot->animframe_soup)>=8) pot->animframe_soup=0;
      if (++(pot->animframe_lid)>=4) pot->animframe_lid=0;
    }
    // The watched pot doesn't get hotter, it's already getting colder. But it does continue dealing damage, if still over the threshold.
    if (pot!=watched) {
      if ((pot->heat+=pot->heatrate*elapsed)>1.0) pot->heat=1.0;
    }
    if (pot->heat>HOT_HEAT) {
      player->damage+=((pot->heat-HOT_HEAT)/(1.0-HOT_HEAT))*DAMAGE_RATE*elapsed;
    }
  }
}

/* Update.
 */
 
static void _watching_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  if (BATTLE->playerv[0].damage>=1.0) {
    if (BATTLE->playerv[1].damage>=1.0) {
      battle->outcome=0;
    } else {
      battle->outcome=-1;
    }
  } else if (BATTLE->playerv[1].damage>=1.0) {
    battle->outcome=1;
  }
}

/* Render pot.
 */
 
static void pot_render(
  struct battle *battle,struct player *player,struct pot *pot,
  int x,int y,uint8_t xform
) {
  const int ht=NS_sys_tilesize>>1;
  uint8_t tileid_bottom=0xf0;
  uint8_t tileid_top=0xe0;
  int liddx=0,liddy=0;
  
  switch (pot->animframe_fire) {
    case 0: tileid_bottom+=4; break;
    case 1: break;
    case 2: tileid_bottom+=4; break;
    case 3: tileid_bottom+=2; break;
  }
  tileid_top+=pot->animframe_soup*2;
  
  if (pot->heat<COLD_HEAT) {
    // Cold, keep the lid on tight.
  } else if (pot->heat<HOT_HEAT) {
    // Warning of warming. Rattle a little.
    liddy=(pot->animframe_lid&1)?-2:-1;
  } else {
    // Too hot, rattle alarmingly.
    switch (pot->animframe_lid) {
      case 0: liddy=-2; break;
      case 1: liddy=-6; break;
      case 2: liddy=-8; break;
      case 3: liddy=-6; break;
    }
  }
  
  if (xform) {
    tileid_bottom+=1;
    tileid_top+=1;
  }
  graf_tile(&g.graf,x-ht,y-ht,tileid_top,xform);
  graf_tile(&g.graf,x+ht,y-ht,tileid_top^1,xform);
  graf_tile(&g.graf,x-ht,y+ht,tileid_bottom,xform);
  graf_tile(&g.graf,x+ht,y+ht,tileid_bottom^1,xform);
  graf_tile(&g.graf,x-ht+liddx,y-NS_sys_tilesize-ht+liddy,0xf7,0);
  graf_tile(&g.graf,x+ht+liddx,y-NS_sys_tilesize-ht+liddy,0xf8,0);
  graf_tile(&g.graf,x-ht+liddx,y-ht+liddy,0xf9,0);
  graf_tile(&g.graf,x+ht+liddx,y-ht+liddy,0xfa,0);
  if (xform) {
    graf_tile(&g.graf,x-ht-NS_sys_tilesize,y-ht,0xf6,xform);
  } else {
    graf_tile(&g.graf,x+ht+NS_sys_tilesize,y-ht,0xf6,0);
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int margin=90;
  int midx=player->who?(FBW-margin):margin;
  int midy=FBH>>1;
  
  uint8_t tileid=player->tileid;
  uint8_t xform=0;
  int px=midx,py=midy;
  switch (player->facedir) {
    case 0x40: tileid+=1; break;
    case 0x10: tileid+=2; xform=EGG_XFORM_XREV; break;
    case 0x08: tileid+=2; break;
  }
  switch (player->position) {
    case 0x40: py-=NS_sys_tilesize*2; break;
    case 0x10: px-=NS_sys_tilesize*2; break;
    case 0x08: px+=NS_sys_tilesize*2; break;
    case 0x02: py+=NS_sys_tilesize*2; break;
  }
  graf_tile(&g.graf,px,py,tileid,xform);
  
  const int cheatdown=6;
  pot_render(battle,player,player->potv+0,midx-NS_sys_tilesize*2,midy-NS_sys_tilesize*2+cheatdown,EGG_XFORM_XREV);
  pot_render(battle,player,player->potv+1,midx+NS_sys_tilesize*2,midy-NS_sys_tilesize*2+cheatdown,0);
  pot_render(battle,player,player->potv+2,midx-NS_sys_tilesize*2,midy+NS_sys_tilesize*2+cheatdown,EGG_XFORM_XREV);
  pot_render(battle,player,player->potv+3,midx+NS_sys_tilesize*2,midy+NS_sys_tilesize*2+cheatdown,0);
}

/* Damage meter.
 */
 
static void render_damage_meter(struct battle *battle,struct player *player) {
  const int margin=90;
  int midx=player->who?(FBW-margin):margin;
  int midy=FBH/10;
  int barw=80;
  int barh=4;
  int x=midx-(barw>>1);
  int y=midy-(barh>>1);
  int fillw=(int)(player->damage*barw);
  if (fillw<0) fillw=0; else if (fillw>barw) fillw=barw;
  graf_fill_rect(&g.graf,x,y,barw,barh,0x40202080);
  graf_fill_rect(&g.graf,x,y,fillw,barh,0xff0000ff);
}

/* Render.
 */
 
static void _watching_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_fractia);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // Damage meters. TODO prettier
  render_damage_meter(battle,BATTLE->playerv+0);
  render_damage_meter(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_watching={
  .name="watching",
  .objlen=sizeof(struct battle_watching),
  .strix_name=149,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_watching_del,
  .init=_watching_init,
  .update=_watching_update,
  .render=_watching_render,
};

/* battle_clambaking.c
 */

#include "game/batsup/battle_internal.h"

struct battle_clambaking {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    int x,y;
    int burning;
    int blackout;
    double flameclock;
    int flameframe;
    int open;
    double score; // 0..1
    double uprate; // score/sec, added while burning and open
    double downrate; // score/sec positive, subtracted while burning and closed
    double clamclock;
    double cpuattnclock; // CPU only looks at the clam at discrete intervals.
    double cpuattnlo,cpuattnhi;
  } playerv[2];
};

#define BATTLE ((struct battle_clambaking*)battle)

/* Delete.
 */
 
static void _clambaking_del(struct battle *battle) {
}

/* Set the clam clock.
 */
 
static void clambaking_random_clam_time(struct player *player) {
  player->clamclock=0.700+((rand()&0xffff)*1.500)/65535.0;
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=100;
  player->y=100;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  
  player->uprate=0.300*(1.0-player->skill)+0.350*player->skill;
  player->downrate=0.400*(1.0-player->skill)+0.300*player->skill;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    if (battle->args.lctl||battle->args.rctl) { // cpu penalty
      player->uprate*=0.800;
      player->downrate*=1.100;
    }
    player->cpuattnhi=1.200*(1.0-player->skill)+0.400*player->skill;
    player->cpuattnlo=player->cpuattnhi*0.5;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x0f663cff;
        player->tileid=0x76;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x36;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x56;
      } break;
  }
  clambaking_random_clam_time(player);
}

/* New.
 */
 
static int _clambaking_init(struct battle *battle) {
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
    player->burning=(input&EGG_BTN_SOUTH)?1:0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  /* CPU's play is perfect, except it only examines the clam at random intervals.
   * Creates the appearance of human-like inattention.
   */
  if (player->cpuattnclock>0.0) {
    player->cpuattnclock-=elapsed;
    return;
  }
  double n=(rand()&0xffff)/65535.0;
  player->cpuattnclock+=player->cpuattnlo*(1.0-n)+player->cpuattnhi*n;
  if (player->open) {
    player->burning=1;
  } else {
    player->burning=0;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (battle->outcome>-2) {
    player->burning=0;
    return;
  }
  
  if (player->burning) {
    if ((player->flameclock-=elapsed)<=0.0) {
      player->flameclock+=0.100+((rand()&0xffff)*0.100)/65535.0;
      switch (player->flameframe) {
        case 0: player->flameframe=(rand()&1)?1:2; break;
        case 1: player->flameframe=(rand()&1)?0:2; break;
        case 2: player->flameframe=(rand()&1)?0:1; break;
      }
    }
    if (player->open) {
      player->score+=player->uprate*elapsed;
      // Let it exceed 1.
    } else {
      player->score-=player->downrate*elapsed;
      if (player->score<0.0) player->score=0.0;
    }
  }
  
  if (battle->outcome==-2) {
    if ((player->clamclock-=elapsed)<=0.0) {
      clambaking_random_clam_time(player);
      if (player->open) player->open=0;
      else player->open=1;
    }
  }
}

/* Update.
 */
 
static void _clambaking_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (battle->outcome==-2) {
      if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
    player_update_common(battle,player,elapsed);
  }
  
  /* Game ends when the first player reaches score one.
   * Ties are possible but even less likely than you think, because we don't clamp at 1.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if ((l->score>=1.0)||(r->score>=1.0)) {
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    }
  }

  //XXX
  if (g_input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x0=player->x-(NS_sys_tilesize>>1);
  int y0=player->y-(NS_sys_tilesize>>1);
  int x1,y1=y0+NS_sys_tilesize;
  if (player->xform) {
    x1=x0;
    x0+=NS_sys_tilesize;
  } else {
    x1=x0+NS_sys_tilesize;
  }
  uint8_t tileid=player->tileid;
  if (player->burning) tileid+=2;
  graf_tile(g_graf,x0,y0,tileid+0x00,player->xform);
  graf_tile(g_graf,x1,y0,tileid+0x01,player->xform);
  graf_tile(g_graf,x0,y1,tileid+0x10,player->xform);
  graf_tile(g_graf,x1,y1,tileid+0x11,player->xform);
  
  int c0=x1,c1;
  if (player->xform) {
    c0-=NS_sys_tilesize+(NS_sys_tilesize>>1);
    c1=c0-NS_sys_tilesize;
  } else {
    c0+=NS_sys_tilesize+(NS_sys_tilesize>>1);
    c1=c0+NS_sys_tilesize;
  }
  uint8_t ctile=0x3c;
  if (player->score>=1.0) ctile+=0x40; // cooked
  else if (player->open) ctile+=0x20;
  graf_tile(g_graf,c0,y0,ctile+0x00,player->xform);
  graf_tile(g_graf,c1,y0,ctile+0x01,player->xform);
  graf_tile(g_graf,c0,y1,ctile+0x10,player->xform);
  graf_tile(g_graf,c1,y1,ctile+0x11,player->xform);
  
  if (player->burning) {
    int f0,f1;
    if (player->xform) {
      f0=x1-NS_sys_tilesize;
      f1=f0-NS_sys_tilesize;
    } else {
      f0=x1+NS_sys_tilesize;
      f1=f0+NS_sys_tilesize;
    }
    uint8_t ftile=0x3a;
    switch (player->flameframe) {
      case 1: ftile+=0x20; break;
      case 2: ftile+=0x40; break;
    }
    graf_tile(g_graf,f0,y0,ftile+0x00,player->xform);
    graf_tile(g_graf,f1,y0,ftile+0x01,player->xform);
    graf_tile(g_graf,f0,y1,ftile+0x10,player->xform);
    graf_tile(g_graf,f1,y1,ftile+0x11,player->xform);
  }
}

/* Render.
 */
 
static void _clambaking_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_set_image(g_graf,RID_image_battle_sea2);
  player_render(battle,l);
  player_render(battle,r);
  
  int barw=3;
  int fullw=3+(barw<<1);
  int fullx=(FBW>>1)-(fullw>>1);
  int barh=40;
  int fullh=barh+2;
  int fully=30;
  graf_fill_rect(g_graf,fullx,fully,fullw,fullh,0x000000ff);
  {
    int fillh=(int)(l->score*barh);
    if (fillh>0) {
      if (fillh>barh) fillh=barh;
      graf_fill_rect(g_graf,fullx+1,fully+1+barh-fillh,barw,fillh,l->color);
    }
  }
  {
    int fillh=(int)(r->score*barh);
    if (fillh>0) {
      if (fillh>barh) fillh=barh;
      graf_fill_rect(g_graf,fullx+1+barw+1,fully+1+barh-fillh,barw,fillh,r->color);
    }
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_clambaking={
  .name="clambaking",
  .objlen=sizeof(struct battle_clambaking),
  .id=139,
  .strix_name=442,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .imageid_default=RID_image_labyrinth,
  .del=_clambaking_del,
  .init=_clambaking_init,
  .update=_clambaking_update,
  .render=_clambaking_render,
};

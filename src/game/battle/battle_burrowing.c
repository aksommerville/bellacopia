/* battle_burrowing.c
 */

#include "game/batsup/battle_internal.h"

#define PRIZEX 250

struct battle_burrowing {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double x;
    int y;
    int burax,burzx; // Center of tiles for the left and right edges of the burrow.
    int incrawl; // -1,0,1, tho there's no conceivable reason one would crawl left.
    int indig;
    int blackout;
    double digclock;
    double digtimelo,digtimehi;
    double digrange; // Further extent we can dig from, in pixels.
    double crawlspeed;
    int digadvance; // How many pixels per dig. Constant.
    int digadvance_actual; // How much does the current dig yield.
    int pvindig;
    double animclock;
    int animframe;
    int win;
    double cpuclock;
    double cpudelaylo,cpudelayhi;
  } playerv[2];
};

#define BATTLE ((struct battle_burrowing*)battle)

/* Delete.
 */
 
static void _burrowing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=70.0;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->y=FBH/3;
  } else { // Right.
    player->who=1;
    player->y=(FBH*2)/3;
  }
  player->burax=(int)player->x-8;
  player->burzx=player->burax+16; // Can go as low as 13 without undesirable artifacts.
  
  player->digtimehi=0.400*(1.0-player->skill)+0.200*player->skill;
  player->digtimelo=player->digtimehi*0.750;
  player->digadvance=(int)(14.0*(1.0-player->skill)+17.0*player->skill);
  player->digrange=4.0*(1.0-player->skill)+12.0*player->skill;
  player->crawlspeed=30.0*(1.0-player->skill)+50.0*player->skill;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    // CPU decision making runs at a coarse and random interval, to make him imperfect.
    player->cpuclock=0.500;
    player->cpudelayhi=0.600*(1.0-player->skill)+0.400*player->skill;
    player->cpudelaylo=player->cpudelayhi*0.750;
    // Plus a hefty speed penalty, if there's a human.
    if (battle->args.lctl||battle->args.rctl) {
      player->digtimehi*=1.500;
      player->crawlspeed*=0.500;
    }
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0x3c3f4dff;
        player->tileid=0x20;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x00;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x10;
      } break;
  }
}

/* New.
 */
 
static int _burrowing_init(struct battle *battle) {
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
    player->indig=(input&EGG_BTN_SOUTH)?1:0;
  }
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->incrawl=-1; break;
    case EGG_BTN_RIGHT: player->incrawl=1; break;
    default: player->incrawl=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    return;
  }
  double n=(rand()&0xffff)/65535.0;
  player->cpuclock=player->cpudelaylo*(1.0-n)+player->cpudelayhi*n;
  
  /* If we're in range, dig, even if very low quality.
   */
  double dx=(double)player->burzx-player->x;
  if (dx<player->digrange) {
    player->indig=1;
    player->incrawl=0;
    return;
  }
  
  /* And otherwise crawl.
   */
  player->incrawl=1;
  player->indig=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Capture presses of the dig button.
   */
  int newdig=0;
  if (player->indig!=player->pvindig) {
    if (player->pvindig=player->indig) newdig=1;
  }

  /* If digging, that must finish before anything else happens.
   */
  if (player->digclock>0.0) {
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.200;
      if (++(player->animframe)>=2) player->animframe=0;
    }
    if ((player->digclock-=elapsed)<=0.0) {
      player->burzx+=player->digadvance_actual;
      bm_sound_pan(RID_sound_collect,0.0);
      player->animclock=0.0;
      player->animframe=0;
      //TODO toast?
    }
    return;
  }
  
  /* Crawling?
   */
  if (player->incrawl) {
    player->x+=player->crawlspeed*elapsed*player->incrawl;
    if (player->x<player->burax) player->x=player->burax;
    else if (player->x>player->burzx) player->x=player->burzx;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.125;
      if (++(player->animframe)>=4) player->animframe=0;
    }
    if (player->x>=PRIZEX-8) {
      bm_sound_pan(RID_sound_treasure,0.0);
      player->win=1;
      return;
    }
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
  
  /* Start digging?
   */
  if (newdig) {
    if (player->incrawl) {
      bm_sound_pan(RID_sound_reject,0.0);
    } else {
      double dx=(double)player->burzx-player->x;
      double quality=player->digrange-dx;
      if (quality<=0.0) {
        bm_sound_pan(RID_sound_reject,0.0);
      } else {
        if (quality>1.0) quality=1.0; // Shouldn't be possible but I'm not certain.
        player->digadvance_actual=(int)(player->digadvance*quality);
        if (player->digadvance_actual<1) player->digadvance_actual=1; // Minimum 1 pixel if you're within the range.
        double n=(rand()&0xffff)/65535.0;
        player->digclock=player->digtimelo*(1.0-n)+player->digtimehi*n;
        bm_sound_pan(RID_sound_dig,0.0);
        player->animclock=0.0;
        player->animframe=0;
      }
    }
  }
}

/* Update.
 */
 
static void _burrowing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->win||r->win) {
      if (l->win&&r->win) battle->outcome=0;
      else if (l->win) battle->outcome=1;
      else if (r->win) battle->outcome=-1;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int dstx=(int)player->x;
  int dsty=(int)player->y;
  uint8_t tileid=player->tileid;
  if (player->win) {
    graf_tile(g_graf,dstx,dsty,tileid,0);
  } else {
    if (player->digclock>0.0) {
      tileid+=3+player->animframe;
    } else if (player->incrawl) {
      switch (player->animframe) {
        case 1: tileid+=1; break;
        case 3: tileid+=2; break;
      }
    }
    graf_tile(g_graf,dstx,dsty,tileid,0);
    graf_tile(g_graf,PRIZEX,dsty,player->tileid+5,0);
  }
}

/* Render burrow.
 */
 
static void burrowing_render_burrow(struct battle *battle,int ax,int zx,int y) {
  
  /* Intermediate tiles starting right of (a) and stopping before the left edge of (z).
   */
  int xstop=zx-(NS_sys_tilesize>>1);
  int x=ax+NS_sys_tilesize;
  for (;x<xstop;x+=NS_sys_tilesize) graf_fancy(g_graf,x,y,0x31,0,0,NS_sys_tilesize,0,battle->ctab[BATTLE_COLOR_SKY]);
  
  /* One more intermediate tile to make up the right gap
   */
  graf_fancy(g_graf,zx-(NS_sys_tilesize>>1),y,0x31,0,0,NS_sys_tilesize,0,battle->ctab[BATTLE_COLOR_SKY]);
  
  /* Then the caps on top of it.
   */
  graf_fancy(g_graf,ax,y,0x30,0,0,NS_sys_tilesize,0,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fancy(g_graf,zx,y,0x32,0,0,NS_sys_tilesize,0,battle->ctab[BATTLE_COLOR_SKY]);
}

/* Render.
 */
 
static void _burrowing_render(struct battle *battle) {
  struct player *l=BATTLE->playerv; // "L" as in "Top".
  struct player *r=l+1;

  /* Fill the background the GROUND color, then use fancies to carve out the burrows with SKY color.
   */
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(g_graf,RID_image_battle_underground2);
  burrowing_render_burrow(battle,l->burax,l->burzx,l->y);
  burrowing_render_burrow(battle,r->burax,r->burzx,r->y);
  
  player_render(battle,l);
  player_render(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_burrowing={
  .name="burrowing",
  .objlen=sizeof(struct battle_burrowing),
  .id=123,
  .strix_name=406,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_horz_a,
  .imageid_default=RID_image_caves,
  .del=_burrowing_del,
  .init=_burrowing_init,
  .update=_burrowing_update,
  .render=_burrowing_render,
};

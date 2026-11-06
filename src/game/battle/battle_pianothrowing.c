/* battle_pianothrowing.c
 */

#include "game/batsup/battle_internal.h"

struct battle_pianothrowing {
  struct battle hdr;
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    int x,y; // Center of 2x2-tile sprite.
    int thrown;
    int landed;
    int blackout;
    double px,py; // Center of 3x2-tile piano.
    double pdx,pdy; // Piano motion, after (thrown) and before (landed).
    double charge; // 0..1
    int incharge,pvincharge,inthrow;
    double gravity_rate;
    double gravity_limit;
    double charge_up; // 0..1, per stroke
    double charge_down; // positive hz
    double dxlo,dxhi;
    double cpuclock;
    double cpustroke; // Time between strokes for CPU player.
    double cputhresh; // CPU must charge to this level before throwing, determined at init.
    double throwalert; // If >0, show the "Right to throw" alert.
  } playerv[2];
};

#define BATTLE ((struct battle_pianothrowing*)battle)

/* Delete.
 */
 
static void _pianothrowing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=40;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->y=85;
  } else { // Right.
    player->who=1;
    player->y=155;
  }
  player->px=player->x;
  player->py=player->y-NS_sys_tilesize*2.0;
  
  player->gravity_rate=100.0;
  player->gravity_limit=100.0;
  player->charge_up=0.100*(1.0-player->skill)+0.200*player->skill;
  player->charge_down=0.600*(1.0-player->skill)+0.400*player->skill;
  player->dxhi=100.0*(1.0-player->skill)+130.0*player->skill;
  double effect=0.200*(1.0-player->skill)+0.700*player->skill;
  player->dxlo=player->dxhi*effect;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpuclock=1.0; // Long initial delay to catch my breath first.
    player->cputhresh=0.500*(1.0-player->skill)+0.900*player->skill;
    player->cputhresh-=((rand()&0xffff)*0.050)/65535.0; // Two CPU players of equal skill should get differentish thresholds.
    player->cpustroke=0.150; // Counterintuitively, this can be constant. Charge effort is already skill-sensitive.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x2f313dff;
        player->tileid=0xa0;
      } break;
    case NS_face_dot: {
        player->color=0xa668f3ff; // bright color, black background
        player->tileid=0x60;
      } break;
    case NS_face_princess: {
        player->color=0x5d83f4ff; // ''
        player->tileid=0x80;
      } break;
  }
}

/* New.
 */
 
static int _pianothrowing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playclock=9.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&(EGG_BTN_SOUTH|EGG_BTN_RIGHT))) player->blackout=0;
  } else {
    player->incharge=(input&EGG_BTN_SOUTH)?1:0;
    player->inthrow=(input&EGG_BTN_RIGHT)?1:0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  player->cpuclock-=elapsed;
  if (player->incharge||(player->cpuclock>0.0)) { // If charge is still on, we're strobing too fast. Always skip at least one frame.
    player->incharge=0;
    player->inthrow=0;
    if (player->charge>=player->cputhresh) { // Liftoff!
      player->inthrow=1;
    }
    return;
  }
  player->cpuclock+=player->cpustroke;
  player->incharge=1;
  player->inthrow=0;
}

/* Throw it.
 */

static void pianothrowing_throw(struct battle *battle,struct player *player) {
  if (player->thrown) return; // Redundant calls do happen, if the timer expires. Ignore them.
  player->thrown=1;
  player->pdx=player->dxlo*(1.0-player->charge)+player->dxhi*player->charge;
  player->pdy=-80.0;
  bm_sound_pan(RID_sound_jump,0.0);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Once landed, we're done.
   */
  if (player->landed) return;
  
  /* If thrown, the piano moves and nothing else happens.
   */
  if (player->thrown) {
    player->px+=player->pdx*elapsed;
    player->py+=player->pdy*elapsed;
    player->pdy+=player->gravity_rate*elapsed;
    if (player->pdy>player->gravity_limit) player->pdy=player->gravity_limit;
    if (player->py>=player->y) {
      player->py=player->y;
      player->landed=1;
      bm_sound_pan(RID_sound_bump,0.0);
    }
    return;
  }
  
  /* "Right to throw" when a human's charge is high.
   * Because Right isn't a perfectly obvious choice of input.
   * It's a clock to avoid jitter.
   */
  if (player->throwalert>0.0) {
    player->throwalert-=elapsed;
  }
  const double alertthresh=0.900;
  if (!player->thrown&&player->human&&(player->charge>alertthresh)) player->throwalert=0.500;
  
  /* If they pressed Right, throw it.
   */
  if (player->inthrow) {
    pianothrowing_throw(battle,player);
    return;
  }
  
  /* If they pressed South, increase charge.
   */
  if (player->incharge!=player->pvincharge) {
    if (player->pvincharge=player->incharge) {
      player->charge+=player->charge_up;
      if (player->charge>=1.0) player->charge=1.0;
    }
  }
  
  /* Drop charge at a constant rate.
   */
  player->charge-=player->charge_down*elapsed;
  if (player->charge<0.0) player->charge=0.0;
}

/* Update.
 */
 
static void _pianothrowing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  /* When the clock expires, throw both.
   * That doesn't end the battle immediately.
   */
  if (!l->thrown||!r->thrown) {
    if ((BATTLE->playclock-=elapsed)<=0.0) {
      pianothrowing_throw(battle,l);
      pianothrowing_throw(battle,r);
    }
  }
  
  /* Battle ends when both players have landed.
   * Score is just (px-x). (arguably just px, since x should be the same for both, but let's do it right).
   */
  if (l->landed&&r->landed) {
    double lscore=l->px-l->x;
    double rscore=r->px-r->x;
    if (lscore>rscore) battle->outcome=1;
    else if (lscore<rscore) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  uint8_t tileid=player->tileid;
  if (player->thrown) tileid+=2;
  int x0=player->x-(NS_sys_tilesize>>1);
  int y0=player->y-(NS_sys_tilesize>>1);
  int x1=x0+NS_sys_tilesize;
  int y1=y0+NS_sys_tilesize;
  graf_tile(g_graf,x0,y0,tileid+0x00,0);
  graf_tile(g_graf,x1,y0,tileid+0x01,0);
  graf_tile(g_graf,x0,y1,tileid+0x10,0);
  graf_tile(g_graf,x1,y1,tileid+0x11,0);
  
  int px1=(int)player->px;
  int px0=px1-NS_sys_tilesize;
  int px2=px1+NS_sys_tilesize;
  int py0=(int)player->py-(NS_sys_tilesize>>1);
  int py1=py0+NS_sys_tilesize;
  uint8_t ptileid=0x0a;
  if (player->landed) ptileid+=3;
  graf_tile(g_graf,px0,py0,ptileid+0x00,0);
  graf_tile(g_graf,px1,py0,ptileid+0x01,0);
  graf_tile(g_graf,px2,py0,ptileid+0x02,0);
  graf_tile(g_graf,px0,py1,ptileid+0x10,0);
  graf_tile(g_graf,px1,py1,ptileid+0x11,0);
  graf_tile(g_graf,px2,py1,ptileid+0x12,0);
  
  if (player->thrown) return; // No charge indicator or throw alert if thrown.
  
  int barh=32;
  int fillh=lround(player->charge*barh);
  if (fillh<0) fillh=0; else if (fillh>barh) fillh=barh;
  int barx=player->x+24;
  int bary=player->y-(barh>>1);
  int barw=3;
  graf_fill_rect(g_graf,barx-1,bary-1,barw+2,barh+2,0x000000ff);
  if (fillh>0) graf_fill_rect(g_graf,barx,bary+barh-fillh,barw,fillh,player->color);
  graf_set_image(g_graf,RID_image_battle_jungle);
  
  if (player->throwalert>0.0) {
    int x=x1+NS_sys_tilesize*2;
    tileid=0x2a;
    if (g_framec&8) tileid+=3;
    graf_tile(g_graf,x,player->y,tileid+0,0); x+=NS_sys_tilesize;
    graf_tile(g_graf,x,player->y,tileid+1,0); x+=NS_sys_tilesize;
    graf_tile(g_graf,x,player->y,tileid+2,0);
  }
}

/* Render.
 */
 
static void _pianothrowing_render(struct battle *battle) {
  struct player *l=BATTLE->playerv; // "L" as in "Top".
  struct player *r=l+1;

  /* Background. Players stand within the ground, the horizon is purely decorative.
   */
  const int horizony=60;
  graf_fill_rect(g_graf,0,0,FBW,horizony,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,horizony,FBW,FBH-horizony,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,horizony,FBW,1,0x000000ff);
  
  graf_set_image(g_graf,RID_image_battle_jungle);
  player_render(battle,l);
  player_render(battle,r);
  
  /* If either player is unthrown, show the clock.
   */
  if (!l->thrown||!r->thrown) {
    int s=(int)(BATTLE->playclock+0.999);
    if (s<1) s=1; else if (s>9) s=9;
    graf_set_image(g_graf,RID_image_fonttiles);
    graf_set_tint(g_graf,battle->ctab[BATTLE_COLOR_SKY_TEXT]);
    graf_tile(g_graf,FBW>>1,10,'0'+s,0);
    graf_set_tint(g_graf,0);
  }
}

/* Type definition.
 */

static const struct battle_input battle_input_pianothrowing[]={
  {1,EGG_BTN_SOUTH},
  {1,0},
  {1,EGG_BTN_SOUTH},
  {1,0},
  {1,EGG_BTN_SOUTH},
  {1,0},
  {3,EGG_BTN_RIGHT},
  {0},
};
 
const struct battle_type battle_type_pianothrowing={
  .name="pianothrowing",
  .objlen=sizeof(struct battle_pianothrowing),
  .id=117,
  .strix_name=394,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_pianothrowing,
  .imageid_default=RID_image_temple,
  .del=_pianothrowing_del,
  .init=_pianothrowing_init,
  .update=_pianothrowing_update,
  .render=_pianothrowing_render,
};

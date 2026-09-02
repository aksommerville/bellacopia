/* battle_oateating.c
 */

#include "game/bellacopia.h"

#define SCORE_LIMIT 8

struct battle_oateating {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint16_t scoretilev[SCORE_LIMIT]; // ((tileid<<8)|xform). 0xd2 is the blank pip; 0xd1 and 0xe1 are oats.
    int blackout;
    double munchclock; // Counts down after each munch.
    double munchtimelo,munchtimehi;
    double cpuclock; // Additional delay for cpu players.
    double cputimelo,cputimehi;
  } playerv[2];
};

#define BATTLE ((struct battle_oateating*)battle)

/* Delete.
 */
 
static void _oateating_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  
  player->munchtimehi=0.700*(1.0-player->skill)+0.500*player->skill;
  player->munchtimelo=player->munchtimehi*0.5;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cputimehi=0.600*(1.0-player->skill)+0.400*player->skill;
    player->cputimelo=player->cputimehi*0.5;
    player->cpuclock=player->cputimehi; // Start with the longest possible delay.
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0xdeb7b7ff;
        player->tileid=0xb0;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x70;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x90;
      } break;
  }
  // Start scoreboard with pips and xform zero.
  uint16_t *dst=player->scoretilev;
  int i=SCORE_LIMIT;
  for (;i-->0;dst++) *dst=0xd200;
}

/* New.
 */
 
static int _oateating_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Add an oat to one player's scoreboard.
 */
 
static void player_add_score(struct battle *battle,struct player *player) {
  int p=SCORE_LIMIT-1;
  while ((p>0)&&(player->scoretilev[p-1]==0xd200)) p--;
  uint8_t tileid=(rand()&1)?0xd1:0xe1;
  uint8_t xform=rand()&7;
  player->scoretilev[p]=(tileid<<8)|xform;
}

/* Eat another oat, or produce the rejection if we can't.
 */
 
static void player_eat(struct battle *battle,struct player *player) {
  if (battle->outcome>-2) return;
  if (player->munchclock>0.0) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
  double n=(rand()&0xffff)/65535.0;
  player->munchclock=n*player->munchtimelo+(1.0-n)*player->munchtimehi;
  player_add_score(battle,player);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) player_eat(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->munchclock>0.0) return;
  if ((player->cpuclock-=elapsed)>0.0) return;
  double n=(rand()&0xffff)/65535.0;
  player->cpuclock=n*player->cputimelo+(1.0-n)*player->cputimehi;
  player_eat(battle,player);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->munchclock>0.0) {
    player->munchclock-=elapsed;
  }
}

/* Update.
 */
 
static void _oateating_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    int ldone=(l->scoretilev[SCORE_LIMIT-1]!=0xd200);
    int rdone=(r->scoretilev[SCORE_LIMIT-1]!=0xd200);
    if (ldone&&rdone) battle->outcome=0; // Ties are unlikely but possible.
    else if (ldone) battle->outcome=1;
    else if (rdone) battle->outcome=-1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player,int midx,int midy) {

  // Decide which side we go on, per (player->who).
  uint8_t xform;
  int frontx,backx,armx,scorex;
  if (player->who) {
    xform=EGG_XFORM_XREV;
    frontx=midx+NS_sys_tilesize;
    backx=frontx+NS_sys_tilesize;
    armx=frontx-4;
    scorex=midx+8;
  } else {
    xform=0;
    frontx=midx-NS_sys_tilesize;
    backx=frontx-NS_sys_tilesize;
    armx=frontx+4;
    scorex=midx-8;
  }
  
  uint8_t tileid=player->tileid;
  uint8_t armtile=tileid+4;
  if (player->munchclock>0.0) {
    tileid+=2;
    armtile+=0x10;
  }
  
  graf_tile(&g.graf,frontx,midy-NS_sys_tilesize,tileid+1,xform);
  graf_tile(&g.graf,frontx,midy,tileid+0x11,xform);
  graf_tile(&g.graf,backx,midy-NS_sys_tilesize,tileid,xform);
  graf_tile(&g.graf,backx,midy,tileid+0x10,xform);
  graf_tile(&g.graf,armx,midy-7,armtile,xform);
  
  /* Then a scoreboard above the rest.
   */
  int y=midy-NS_sys_tilesize*2;
  int i=0; for (;i<SCORE_LIMIT;i++,y-=10) {
    graf_tile(&g.graf,scorex,y,player->scoretilev[i]>>8,player->scoretilev[i]&0xff);
  }
}

/* Render.
 */
 
static void _oateating_render(struct battle *battle) {
  const int groundy=130;
  int midx=FBW>>1; // Position of the sack's lower tile.
  int midy=groundy-6;
  
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,groundy,FBW,FBH-groundy,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,groundy,FBW,1,0x000000ff);
  
  graf_set_image(&g.graf,RID_image_battle_skeleton);
  graf_tile(&g.graf,midx,midy,0xe0,0);
  graf_tile(&g.graf,midx,midy-NS_sys_tilesize,0xd0,0);
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  player_render(battle,l,midx,midy);
  player_render(battle,r,midx,midy);
}

/* Type definition.
 */
 
const struct battle_type battle_type_oateating={
  .name="oateating",
  .objlen=sizeof(struct battle_oateating),
  .id=NS_battle_oateating,
  .strix_name=330,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .imageid_default=0,
  .del=_oateating_del,
  .init=_oateating_init,
  .update=_oateating_update,
  .render=_oateating_render,
};

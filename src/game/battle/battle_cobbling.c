/* battle_cobbling.c
 */

#include "game/bellacopia.h"

#define END_COOLDOWN 1.0
#define STAGE_LIMIT 16 /* If you want to win, you'll always stage exactly two things. But for silliness's sake, you can go much higher. */
#define CPU_MOVETIME_SLOW 0.500
#define CPU_MOVETIME_FAST 0.125
#define CPU_MOVETIME_VARIANCE 1.100 /* >1 */
#define CPU_HEADSTART_SLOW 0.500
#define CPU_HEADSTART_FAST 0.250

struct battle_cobbling {
  struct battle hdr;
  double end_cooldown;
  
  struct player {
    int who;
    int human;
    int x,y; // Center of the hero tile.
    uint8_t tileid; // Idle hero tile. +1 is swinging the hammer.
    uint8_t xform;
    uint8_t stagev[STAGE_LIMIT];
    int stagec;
    int pvinput;
    double hammerclock;
    double hammertime;
    int shoec;
    double moveclock;
    double movetimelo,movetimehi; // Random at each move, so CPU players of equal handicap will tend not to tie.
    double skill; // Only relevant to CPU players, and if you get down it, we actually only need during init.
  } playerv[2];
};

#define BATTLE ((struct battle_cobbling*)battle)

/* Delete.
 */
 
static void _cobbling_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int appearance) {
  if (player==BATTLE->playerv) {
    player->who=0;
    player->x=FBW/3;
    player->skill=1.0-battle->args.bias/255.0;
  } else {
    player->who=1;
    player->x=(FBW*2)/3;
    player->xform=EGG_XFORM_XREV;
    player->skill=battle->args.bias/255.0;
  }
  if (player->human=human) {
    player->pvinput=EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_SOUTH;
  } else {
    player->moveclock=CPU_MOVETIME_SLOW*(1.0-player->skill)+CPU_MOVETIME_FAST*player->skill;
    player->movetimehi=player->moveclock*CPU_MOVETIME_VARIANCE;
    player->movetimelo=player->moveclock/CPU_MOVETIME_VARIANCE;
    player->moveclock=CPU_HEADSTART_SLOW*(1.0-player->skill)+CPU_HEADSTART_FAST*player->skill;
  }
  switch (appearance) {
    case 0: { // Coblin
        player->tileid=0xc0;
      } break;
    case 1: { // Dot
        player->tileid=0xb0;
      } break;
    case 2: { // Princess
        player->tileid=0xd0;
      } break;
  }
  player->y=FBH>>1;
  player->hammertime=0.250;//TODO probly should vary with handicap
}

/* New.
 */

static int _cobbling_init(struct battle *battle) {
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Add something to the stage.
 */
 
static void player_add_stage(struct battle *battle,struct player *player,uint8_t tileid) {
  if (player->stagec>=STAGE_LIMIT) {
    bm_sound(RID_sound_reject);
    return;
  }
  player->stagev[player->stagec++]=tileid;
  bm_sound(RID_sound_collect);
}

/* Whack the stage.
 */
 
static void player_whack(struct battle *battle,struct player *player) {
  int valid=0;
  if (player->stagec==2) {
    int leatherc=0,solec=0,otherc=0;
    const uint8_t *q=player->stagev;
    int i=player->stagec;
    for (;i-->0;q++) {
      if (*q==0xb3) leatherc++;
      else if (*q==0xc3) solec++;
      else otherc++;
    }
    if ((leatherc==1)&&(solec==1)&&(otherc==0)) valid=1;
  }
  player->stagec=0;
  if (valid) {
    bm_sound(RID_sound_whack);
    player->shoec++;
  } else {
    bm_sound(RID_sound_reject);
    //TODO visual rejection
  }
  player->hammerclock=player->hammertime;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (input!=player->pvinput) {
    if ((input&EGG_BTN_UP)&&!(player->pvinput&EGG_BTN_UP)) player_add_stage(battle,player,0xb3); // Up for leather.
    if ((input&EGG_BTN_DOWN)&&!(player->pvinput&EGG_BTN_DOWN)) player_add_stage(battle,player,0xc3); // Down for sole.
    if ((input&EGG_BTN_SOUTH)&&!(player->pvinput&EGG_BTN_SOUTH)) player_whack(battle,player); // And South to whack them together.
    player->pvinput=input;
  }
}

/* Update CPU player.
 */

static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->moveclock-=elapsed)<=0.0) {
    player->moveclock+=player->movetimelo+((rand()&0xffff)*(player->movetimehi-player->movetimelo))/65535.0;
    switch (player->stagec) {
      case 0: player_add_stage(battle,player,0xb3); break;
      case 1: player_add_stage(battle,player,0xc3); break;
      case 2: player_whack(battle,player); break;
    }
  }
}

/* Update.
 */
 
static void _cobbling_update(struct battle *battle,double elapsed) {

  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->hammerclock>0.0) {
      player->hammerclock-=elapsed;
      continue;
    }
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
  }
  
  // First to three wins, because shoes always come in threes.
  // Ties are technically possible.
  if (BATTLE->playerv[0].shoec>=3) {
    BATTLE->end_cooldown=END_COOLDOWN;
    if (BATTLE->playerv[1].shoec>=3) battle->outcome=0;
    else battle->outcome=1;
  } else if (BATTLE->playerv[1].shoec>=3) {
    BATTLE->end_cooldown=END_COOLDOWN;
    battle->outcome=-1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  if (player->hammerclock>0.0) {
    graf_tile(&g.graf,player->x,player->y,player->tileid+1,player->xform);
    int hx=player->x;
    if (player->xform) hx-=NS_sys_tilesize; else hx+=NS_sys_tilesize;
    graf_tile(&g.graf,hx,player->y,0xb2,player->xform);
  } else {
    graf_tile(&g.graf,player->x,player->y,player->tileid,player->xform);
  }
  
  const int stagespacing=3;
  int stagex=player->x+(player->xform?-NS_sys_tilesize:NS_sys_tilesize);
  int stagey=player->y;
  uint8_t *stagetile=player->stagev;
  int i=player->stagec;
  for (;i-->0;stagetile++,stagey-=stagespacing) graf_tile(&g.graf,stagex,stagey,*stagetile,0);
  
  int dx=player->xform?-12:12;
  int shoex=stagex+(player->xform?-NS_sys_tilesize:NS_sys_tilesize);
  int shoey=player->y;
  for (i=player->shoec;i-->0;shoex+=dx) graf_tile(&g.graf,shoex,shoey,0xc2,player->xform);
}

/* Render.
 */
 
static void _cobbling_render(struct battle *battle) {

  // Background.
  const uint32_t light=0xa09080ff;
  const uint32_t dark= 0x402010ff;
  const int horizon=FBH>>1;
  graf_gradient_rect(&g.graf,0,0,FBW,horizon,dark,dark,light,light);
  graf_gradient_rect(&g.graf,0,horizon,FBW,FBH-horizon,light,light,dark,dark);
  
  graf_set_image(&g.graf,RID_image_battle_goblins);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // I made graphics imaging this whole supply-chain decoration.
  // Cows slide into the factory and hides slide out, sliding all the way to above the heroes. Mutatis mutandi trees to soles.
  // But that seems a little ridiculous now.
}

/* Type definition.
 */
 
const struct battle_type battle_type_cobbling={
  .name="cobbling",
  .objlen=sizeof(struct battle_cobbling),
  .strix_name=57,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_cobbling_del,
  .init=_cobbling_init,
  .update=_cobbling_update,
  .render=_cobbling_render,
};

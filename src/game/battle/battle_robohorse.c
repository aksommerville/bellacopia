/* battle_robohorse.c
 */

#include "game/batsup/battle_internal.h"

#define LEGH 21

struct battle_robohorse {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid_head;
    uint8_t tileid_arm;
    double lleg,rleg; // 0..1, pullup of each leg. Left=Rear.
    double x; // butt position in framebuffer pixels.
    double y; // belly in view pixels (0..90, ground at 80)
    double ly,ry; // Hoof positions captured from previous frame.
    int inl,inr; // Input state -1,0,1
    int blackout;
    double gravity;
    double gravity_rate;
    double gravity_peak;
    double bounce;
    double bounce_loss;
    double bounce_augment;
    int nextleg;
    double wrong_leg_penalty;
    double anti_machine_gun;
    double cpu_safety;
  } playerv[2];
};

#define BATTLE ((struct battle_robohorse*)battle)

/* Delete.
 */
 
static void _robohorse_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  player->x=40.0;
  player->y=80.0-LEGH*0.5;
  player->lleg=0.5;
  player->rleg=0.5;
  player->ly=player->ry=80.0;
  player->nextleg=-1;
  
  player->gravity_rate=200.0;
  player->gravity_peak=80.0;
  player->bounce_augment=0.450*(1.0-player->skill)+0.600*player->skill;
  player->bounce_loss=55.0*(1.0-player->skill)+45.0*player->skill;
  player->wrong_leg_penalty=0.100*(1.0-player->skill)+0.300*player->skill;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpu_safety=0.500;
    if (rand()&1) player->inl=-1; else player->inl=1;
    player->inr=-player->inl;
    player->bounce_augment*=0.666; // CPU penalty. It's possible to beat him without this, he's actually not very good at it. But then, neither are we.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x866c48ff;
        player->tileid_head=0x32;
        player->tileid_arm=0x72;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid_head=0x20;
        player->tileid_arm=0x52;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid_head=0x40;
        player->tileid_arm=0x62;
      } break;
  }
}

/* New.
 */
 
static int _robohorse_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    player->inl=player->inr=0;
    if (!(input&(EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_SOUTH|EGG_BTN_WEST))) {
      player->blackout=0;
    }
    return;
  }
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player->inl=-1; break;
    case EGG_BTN_DOWN: player->inl=1; break;
    default: player->inl=0; break;
  }
  switch (input&(EGG_BTN_SOUTH|EGG_BTN_WEST)) {
    case EGG_BTN_WEST: player->inr=-1; break;
    case EGG_BTN_SOUTH: player->inr=1; break;
    default: player->inr=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  int nextl=player->inl,nextr=player->inr;
  if ((player->cpu_safety-=elapsed)<=0.0) {
    nextl=-player->inl;
    nextr=-player->inr;
  } else if (player->nextleg==0) {
    nextl=1;
    nextr=-1;
  } else if (player->nextleg==1) {
    nextl=-1;
    nextr=1;
  }
  if ((nextl!=player->inl)||(nextr!=player->inr)) {
    player->cpu_safety=0.500;
    player->inl=nextl;
    player->inr=nextr;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (player->anti_machine_gun>0.0) player->anti_machine_gun-=elapsed;

  /* Move legs toward center, or where the player asks.
   */
  double lrate=1.000,rrate=1.000,ltarget=0.5,rtarget=0.5;
       if (player->inl<0) { ltarget=1.0; lrate=2.000; }
  else if (player->inl>0) { ltarget=0.0; lrate=2.000; }
       if (player->inr<0) { rtarget=1.0; rrate=2.000; }
  else if (player->inr>0) { rtarget=0.0; rrate=2.000; }
  if (player->lleg<ltarget) {
    if ((player->lleg+=lrate*elapsed)>=ltarget) player->lleg=ltarget;
  } else if (player->lleg>ltarget) {
    if ((player->lleg-=lrate*elapsed)<=ltarget) player->lleg=ltarget;
  }
  if (player->rleg<rtarget) {
    if ((player->rleg+=rrate*elapsed)>=rtarget) player->rleg=rtarget;
  } else if (player->rleg>rtarget) {
    if ((player->rleg-=rrate*elapsed)<=rtarget) player->rleg=rtarget;
  }
  
  /* Leg touches the ground?
   */
  double ly=player->y+(1.0-player->lleg)*LEGH;
  double ry=player->y+(1.0-player->rleg)*LEGH;
  int whichlo;
  double lowy,highy,lowvel;
  if (ly>ry) {
    whichlo=0;
    lowy=ly;
    highy=ry;
    lowvel=(ly-player->ly)/elapsed;
  } else {
    whichlo=1;
    lowy=ry;
    highy=ly;
    lowvel=(ry-player->ry)/elapsed;
  }
  player->ly=ly;
  player->ry=ry;
  if (lowy>80.0) {
    player->y-=lowy-80.0;
    double dy=lowy-highy;
    if ((lowvel>=2.0)&&(dy>=2.0)&&(player->anti_machine_gun<=0.0)) {
      double bounce=0.0;
      if (!whichlo&(player->inl>0)) {
        bounce=lowvel*player->bounce_augment;
      } else if (whichlo&(player->inr>0)) {
        bounce=lowvel*player->bounce_augment;
      }
      if ((player->nextleg>=0)&&(player->nextleg!=whichlo)) {
        bounce*=player->wrong_leg_penalty;
      }
      player->nextleg=whichlo^1;
      if (bounce>player->bounce) {
        bm_sound_pan(RID_sound_jump,0.0);
        player->bounce=bounce;
        player->gravity=-bounce*0.200;
        player->anti_machine_gun=0.200;
      }
    }
    if (player->gravity>0.0) player->gravity=0.0;
  } else if (player->gravity<0.0) {
    player->gravity+=elapsed*player->gravity_rate;
    player->y+=player->gravity*elapsed;
  } else if (lowy<80.0) {
    player->gravity+=elapsed*player->gravity_rate;
    if (player->gravity>player->gravity_peak) player->gravity=player->gravity_peak;
    player->y+=player->gravity*elapsed;
  }
  
  /* Bounce?
   */
  if (player->bounce>0.0) {
    if ((player->bounce-=elapsed*player->bounce_loss)<=0.0) {
      player->bounce=0.0;
    } else {
      player->x+=player->bounce*elapsed;
    }
  }
}

/* Update.
 */
 
static void _robohorse_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  // Soon as a horse's nose touches the finish line, it's over. Freeze.
  // Ties are just narrowly possible by nature, but we'll actually fuzz it out a little and round to the pixel they render at.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  double finishx=248.0; // actually refers to the horse's butt, not his snout
  if ((l->x>=finishx)||(r->x>=finishx)) {
    int lqx=(int)l->x;
    int rqx=(int)r->x;
    if (lqx>rqx) battle->outcome=1;
    else if (lqx<rqx) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int viewtop=player->who?90:0;
  int xbody=(int)player->x;
  int yground=viewtop+80;
  int ybody=viewtop+(int)player->y-NS_sys_tilesize*2;
  int xrear=xbody+0;
  int xfore=xbody+18;
  int yrear=(int)(ybody+NS_sys_tilesize*2-9-LEGH*player->lleg);
  int yfore=(int)(ybody+NS_sys_tilesize*2-9-LEGH*player->rleg);
  graf_fill_rect(g_graf,0,yground,FBW,10,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,yground,FBW,1,0x000000ff);
  graf_set_image(g_graf,RID_image_battle_war);
  graf_decal(g_graf,xrear,yrear,NS_sys_tilesize*6,NS_sys_tilesize*3,NS_sys_tilesize*1,NS_sys_tilesize*2);
  graf_decal(g_graf,xfore,yfore,NS_sys_tilesize*6,NS_sys_tilesize*3,NS_sys_tilesize*1,NS_sys_tilesize*2);
  graf_decal(g_graf,xbody,ybody,NS_sys_tilesize*3,NS_sys_tilesize*3,NS_sys_tilesize*3,NS_sys_tilesize*2);
  
  int armx=31,army=viewtop+64;
  armx+=player->inl;
  army+=player->inr;
  graf_tile(g_graf,armx,army,player->tileid_arm,0);
  graf_tile(g_graf,20,viewtop+56,player->tileid_head,0);
  graf_tile(g_graf,20,viewtop+72,player->tileid_head+0x10,0);
}

/* Render.
 */
 
static void _robohorse_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_set_image(g_graf,RID_image_battle_war);
  int y=0;
  for (;y<=FBH;y+=NS_sys_tilesize) graf_tile(g_graf,300,y,0x37,0);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_robohorse={
  .name="robohorse",
  .objlen=sizeof(struct battle_robohorse),
  .id=108,
  .strix_name=376,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_vert_ab,
  .imageid_default=0,
  .del=_robohorse_del,
  .init=_robohorse_init,
  .update=_robohorse_update,
  .render=_robohorse_render,
};

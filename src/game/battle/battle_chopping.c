#include "game/bellacopia.h"

#define END_COOLDOWN_TIME 1.000
#define VEG_LIMIT 128
#define YP1 70
#define YP2 130
#define XEND 180 /* Right edge of the conveyor belts. */
#define PLEFTX 140
#define PKNIFEX 146 /* PLEFTX+offset to knife -- must agree with graphics. */
#define PREPOPULATE_X 80
#define ANIMINTERVAL_MIN 0.015 /* It's OK to go faster than 60 hz; the clock is able to step multiple frames at once. */
#define ANIMINTERVAL_MAX 0.030
#define VEG_INTERVAL_MIN 0.500
#define VEG_INTERVAL_MAX 1.250
#define VEG_COUNT 10
#define CPU_COOLDOWN_MIN 0.060
#define CPU_COOLDOWN_MAX 0.120
#define CPU_COOLUP_MIN   0.060
#define CPU_COOLUP_MAX   0.120

struct battle_chopping {
  struct battle hdr;
  double end_cooldown;
  int check_veg;
  double vegclock;
  int remaining;
  
  struct player {
    int who; // My index: 0,1
    int human; // 0=cpu, or playerid
    int y; // fb pixels, center of belt.
    double animinterval;
    double animclock; // Not just decorative! Animation drives the vegetables' motion too.
    int animframe;
    int texid_output; // Decorative content to go in the output box. 16x32.
    int srcx; // Constant.
    int srcy; // Constant.
    int chopping; // Nonzero to render with knife down. (srcx+32).
    int blackout;
    int score;
    double cooldown; // CPU only; holds knife down for a fixed interval.
    double kcooldown,kcoolup; // CPU only; cooldown and coolup times per handicap.
    double kcooldownextra,kcoolupextra;
  } playerv[2];
  
  struct veg {
    int x; // fb pixels, left edge
    int y; // fb pixels, top edge (player.y-8)
    int w; // 1..16
    int h; // Always 16.
    int srcx;
    int srcy;
    int who; // 0,1. Which player.
    int defunct;
  } vegv[VEG_LIMIT];
  int vegc;
};

#define BATTLE ((struct battle_chopping*)battle)

static void veg_init(struct battle *battle,struct veg *veg,struct player *player);

/* Delete.
 */
 
static void player_cleanup(struct player *player) {
  egg_texture_del(player->texid_output);
}
 
static void _chopping_del(struct battle *battle) {
  player_cleanup(BATTLE->playerv+0);
  player_cleanup(BATTLE->playerv+1);
}

/* Initialize player.
 */
 
static void player_init(struct battle *battle,struct player *player,int y,int human,int face) {
  player->y=y;
  player->human=human;
  player->animclock=0.0;
  player->animframe=rand()&7;
  player->texid_output=egg_texture_new();
  egg_texture_load_raw(player->texid_output,16,32,64,0,0);
  egg_texture_clear(player->texid_output);
  switch (face) {
    case NS_face_monster: player->srcx=64; break;
    case NS_face_dot: player->srcx=0; break;
    case NS_face_princess: player->srcx=128; break;
  }
  player->srcy=176;
  player->chopping=0;
  player->blackout=1;
  player->score=0;
  if (!human) {
    player->cooldown=0.0;
    player->kcooldown=CPU_COOLDOWN_MAX-(battle->args.bias*(CPU_COOLDOWN_MAX-CPU_COOLDOWN_MIN))/256.0;
    player->kcoolup=CPU_COOLUP_MAX-(battle->args.bias*(CPU_COOLUP_MAX-CPU_COOLUP_MIN))/256.0;
    player->kcooldownextra=player->kcooldown;
    player->kcoolupextra=player->kcoolup;
  }
}

/* Preupdate.
 * Advance model state the same way regular update does, to get the initial vegetables belted.
 * We assume that player knives are up, and they can't come down.
 */
 
static int chopping_preupdate(struct battle *battle,double elapsed) {
  int stop=0;
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    int stepc=0;
    player->animclock-=elapsed;
    while (player->animclock<=0.0) {
      stepc++;
      player->animclock+=player->animinterval;
    }
    if (stepc) {
      struct veg *veg=BATTLE->vegv;
      int i=BATTLE->vegc;
      for (;i-->0;veg++) {
        if (veg->who!=player->who) continue;
        int pvr=veg->x+veg->w;
        veg->x+=stepc;
        if (veg->x>=PREPOPULATE_X) stop=1;
      }
    }
  }
  if ((BATTLE->vegclock-=elapsed)<=0.0) {
    BATTLE->vegclock+=VEG_INTERVAL_MIN+((rand()&0xffff)*(VEG_INTERVAL_MAX-VEG_INTERVAL_MIN))/65535.0;
    BATTLE->remaining--;
    veg_init(battle,BATTLE->vegv+BATTLE->vegc++,BATTLE->playerv+0);
    veg_init(battle,BATTLE->vegv+BATTLE->vegc++,BATTLE->playerv+1);
  }
  return !stop;
}

/* New.
 */

static int _chopping_init(struct battle *battle) {
  BATTLE->remaining=VEG_COUNT;
  
  BATTLE->playerv[0].who=0;
  BATTLE->playerv[1].who=1;
  player_init(battle,BATTLE->playerv+0,YP1,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,YP2,battle->args.rctl,battle->args.rface);
  
  // Assign animinterval according to handicap. At middle handicap, they both run at the middle speed.
  double nhc=(double)battle->args.bias/255.0;
  if (nhc<0.0) nhc=0.0; else if (nhc>1.0) nhc=1.0;
  BATTLE->playerv[0].animinterval=ANIMINTERVAL_MAX*(1.0-nhc)+ANIMINTERVAL_MIN*nhc;
  BATTLE->playerv[1].animinterval=ANIMINTERVAL_MIN*(1.0-nhc)+ANIMINTERVAL_MAX*nhc;
  
  // It's a little silly, but get the initial belt population by running fake updates until an arbitrary horizontal threshold is crossed.
  while (chopping_preupdate(battle,0.020)) ;

  return 0;
}

/* Prepare a new vegetable.
 */
 
static void veg_init(struct battle *battle,struct veg *veg,struct player *player) {
  veg->x=-16;
  veg->y=player->y-10;
  veg->w=16;
  veg->h=16;
  veg->who=player->who;
  veg->defunct=0;
  switch (rand()%6) { // Veg image is purely decorative, and we don't make the two partners match.
    case 0: veg->srcx=128; veg->srcy=112; break;
    case 1: veg->srcx=128; veg->srcy=128; break;
    case 2: veg->srcx=128; veg->srcy=144; break;
    case 3: veg->srcx=144; veg->srcy=112; break;
    case 4: veg->srcx=144; veg->srcy=128; break;
    case 5: veg->srcx=144; veg->srcy=144; break;
  }
}

/* Defunct this vegetable and commit it to the player's score.
 */
 
static void veg_finish(struct battle *battle,struct player *player,struct veg *veg) {
  veg->defunct=1;
  BATTLE->check_veg=1;
  player->score++;
  bm_sound(RID_sound_collect);
  
  int dstx=rand()%(17-veg->w);
  int dsty_max=20;
  int dsty_min=16-(player->score/3);
  int dsty=dsty_min+rand()%(dsty_max-dsty_min);
  graf_set_output(&g.graf,player->texid_output);
  graf_set_image(&g.graf,RID_image_battle_early);
  graf_decal(&g.graf,dstx,dsty,veg->srcx,veg->srcy,veg->w,veg->h);
  graf_set_output(&g.graf,1);
}

/* Check for vegetables at the chopping point.
 * If any exist, chop them and return nonzero.
 */
 
static int chopping_chop(struct battle *battle,struct player *player) {
  int result=0;
  struct veg *veg=BATTLE->vegv;
  int i=BATTLE->vegc;
  for (;i-->0;veg++) {
    if (veg->who!=player->who) continue;
    if (veg->x>=PKNIFEX) continue;
    if (veg->x+veg->w<=PKNIFEX) continue;
    if (BATTLE->vegc>=VEG_LIMIT) break; // Oops no room for it.
    result=1;
    int aw=PKNIFEX-veg->x;
    struct veg *nub=BATTLE->vegv+BATTLE->vegc++;
    *nub=*veg;
    nub->x=PKNIFEX+1; // Bounce out by one pixel on chopping, for aesthetic purposes only.
    nub->w=veg->w-aw;
    nub->srcx=veg->srcx+aw;
    veg->w=aw;
  }
  return result;
}

/* Update human player.
 */
 
static void player_update_human(struct battle *battle,struct player *player,double elapsed) {
  if (player->blackout) {
    if (g.input[player->human]&EGG_BTN_SOUTH) return;
    player->blackout=0;
  }
  if (g.input[player->human]&EGG_BTN_SOUTH) {
    if (!player->chopping) {
      player->chopping=1;
      if (chopping_chop(battle,player)) {
        bm_sound(RID_sound_chop);
      } else {
        bm_sound(RID_sound_chopmiss);
      }
    }
  } else {
    player->chopping=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->chopping) {
    if ((player->cooldown-=elapsed)<=0.0) {
      player->chopping=0;
      player->cooldown=player->kcoolup+player->kcoolupextra*((rand()&0xffff)/65535.0);
    }
  } else if (player->cooldown>0.0) {
    player->cooldown-=elapsed;
  } else {
    struct veg *veg=BATTLE->vegv;
    int i=BATTLE->vegc;
    for (;i-->0;veg++) {
      if (veg->who!=player->who) continue;
      if (veg->defunct) continue;
      if (veg->x>=PKNIFEX) continue;
      if (veg->x+veg->w<=PKNIFEX) continue;
      player->chopping=1;
      player->cooldown=player->kcooldown+player->kcooldownextra*((rand()&0xffff)/65535.0);
      if (chopping_chop(battle,player)) {
        bm_sound(RID_sound_chop);
      } else {
        bm_sound(RID_sound_chopmiss); // Probably not possible.
      }
      return;
    }
  }
}

/* Update player, regardless of controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  int stepc=0;
  player->animclock-=elapsed;
  while (player->animclock<=0.0) {
    stepc++;
    player->animclock+=player->animinterval;
    if (++(player->animframe)>=8) player->animframe=0;
  }
  if (stepc) {
    struct veg *veg=BATTLE->vegv;
    int i=BATTLE->vegc;
    for (;i-->0;veg++) {
      if (veg->who!=player->who) continue;
      int pvr=veg->x+veg->w;
      veg->x+=stepc;
      if (player->chopping&&(pvr<=PKNIFEX)&&(veg->x+veg->w>PKNIFEX)) { // A down knife holds up traffic.
        veg->x=PKNIFEX-veg->w;
      }
      if (veg->x>=XEND) veg_finish(battle,player,veg);
    }
  }
}

/* Update.
 */
 
static void _chopping_update(struct battle *battle,double elapsed) {
  
  // Update players.
  if (battle->outcome<=-2) {
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (player->human) player_update_human(battle,player,elapsed);
      else player_update_cpu(battle,player,elapsed);
      player_update_common(battle,player,elapsed);
    }
  }
  
  // If a veg defuncted, scan and remove all defuncts.
  if (BATTLE->check_veg) {
    BATTLE->check_veg=0;
    int i=BATTLE->vegc;
    struct veg *veg=BATTLE->vegv+i-1;
    for (;i-->0;veg--) {
      if (!veg->defunct) continue;
      BATTLE->vegc--;
      memmove(veg,veg+1,sizeof(struct veg)*(BATTLE->vegc-i));
    }
  }
  
  /* Add a vegetable to both players when our clock elapses.
   * Timing is identical regardless of conveyor belt speeds. (otherwise a faster belt is actually a huge advantage!)
   * Or if we're depleted, end the game when the last bit defuncts.
   */
  if (BATTLE->remaining) {
    if ((battle->outcome<=-2)&&(BATTLE->vegc<=VEG_LIMIT-2)) {
      if ((BATTLE->vegclock-=elapsed)<=0.0) {
        BATTLE->vegclock+=VEG_INTERVAL_MIN+((rand()&0xffff)*(VEG_INTERVAL_MAX-VEG_INTERVAL_MIN))/65535.0;
        BATTLE->remaining--;
        veg_init(battle,BATTLE->vegv+BATTLE->vegc++,BATTLE->playerv+0);
        veg_init(battle,BATTLE->vegv+BATTLE->vegc++,BATTLE->playerv+1);
      }
    }
  } else if (!BATTLE->vegc&&(battle->outcome<=-2)) {
    BATTLE->end_cooldown=END_COOLDOWN_TIME;
    if (BATTLE->playerv[0].score>BATTLE->playerv[1].score) battle->outcome=1;
    else if (BATTLE->playerv[0].score<BATTLE->playerv[1].score) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render one player.
 * This is the whole thing: Belt, vegetables, hero, output box.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  
  // Start with the belt.
  graf_set_image(&g.graf,RID_image_battle_early);
  int x=XEND-8;
  int y=player->y;
  for (;x>-8;x-=16) {
    graf_tile(&g.graf,x,y,0xa0+player->animframe,0);
  }
  
  // Box background.
  graf_tile(&g.graf,XEND+8,y,0xa8,0);
  
  // Decal of veg bits sitting in the box.
  graf_set_input(&g.graf,player->texid_output);
  graf_decal(&g.graf,XEND,y-24,0,0,16,32);
  
  // Box foreground. To ensure that bits don't occlude the front face.
  graf_set_image(&g.graf,RID_image_battle_early);
  graf_tile(&g.graf,XEND+8,y,0xa9,0);
  
  // Vegetables on the belt.
  struct veg *veg=BATTLE->vegv;
  int i=BATTLE->vegc;
  for (;i-->0;veg++) {
    if (veg->who!=player->who) continue;
    graf_decal(&g.graf,veg->x,veg->y,veg->srcx,veg->srcy,veg->w,veg->h);
  }
  
  // The cook.
  graf_decal(&g.graf,PLEFTX,player->y-40,player->srcx+(player->chopping?32:0),player->srcy,32,48);
  
  // Score indicator.
  graf_decal(&g.graf,XEND+40,player->y-24,160,144,32,32);
  graf_tile(&g.graf,XEND+46,player->y-9,0x30+(player->score/100)%10,0);
  graf_tile(&g.graf,XEND+52,player->y-9,0x30+(player->score/ 10)%10,0);
  graf_tile(&g.graf,XEND+58,player->y-9,0x30+(player->score    )%10,0);
}

/* Render.
 */
 
static void _chopping_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x5ca77fff);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_chopping={
  .name="chopping",
  .objlen=sizeof(struct battle_chopping),
  .strix_name=15,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_chopping_del,
  .init=_chopping_init,
  .update=_chopping_update,
  .render=_chopping_render,
};

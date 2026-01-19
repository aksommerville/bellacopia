#include "game/game.h"

#define END_COOLDOWN_TIME 2.000
#define VEG_LIMIT 128
#define YP1 60
#define YP2 120
#define XEND 180 /* Right edge of the conveyor belts. */
#define PLEFTX 140
#define PKNIFEX 146 /* PLEFTX+offset to knife -- must agree with graphics. */
#define ANIMINTERVAL_MIN 0.015 /* It's OK to go faster than 60 hz; the clock is able to step multiple frames at once. */
#define ANIMINTERVAL_MAX 0.030
#define VEG_INTERVAL_MIN 0.500
#define VEG_INTERVAL_MAX 1.250
#define VEG_COUNT 10
#define CPU_COOLDOWN_MIN 0.080
#define CPU_COOLDOWN_MAX 0.150
#define CPU_COOLUP_MIN   0.080
#define CPU_COOLUP_MAX   0.150

struct battle_chopping {
  int playerc;
  int handicap;
  void (*cb_finish)(void *userdata,int outcome);
  void *userdata;
  int outcome;
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

#define CTX ((struct battle_chopping*)ctx)

/* Delete.
 */
 
static void player_cleanup(struct player *player) {
  egg_texture_del(player->texid_output);
}
 
static void _chopping_del(void *ctx) {
  player_cleanup(CTX->playerv+0);
  player_cleanup(CTX->playerv+1);
  free(ctx);
}

/* Initialize player.
 */
 
static void player_init(void *ctx,struct player *player,int y,int human) {
  player->y=y;
  player->human=human;
  player->animclock=0.0;
  player->animframe=rand()&7;
  player->texid_output=egg_texture_new();
  egg_texture_load_raw(player->texid_output,16,32,64,0,0);
  if (human) player->srcx=0;
  else player->srcx=64;
  player->srcy=176;
  player->chopping=0;
  player->blackout=1;
  player->score=0;
  if (!human) {
    player->cooldown=0.0;
    player->kcooldown=CPU_COOLDOWN_MAX-(CTX->handicap*(CPU_COOLDOWN_MAX-CPU_COOLDOWN_MIN))/256.0;
    player->kcoolup=CPU_COOLUP_MAX-(CTX->handicap*(CPU_COOLUP_MAX-CPU_COOLUP_MIN))/256.0;
  }
}

/* Init.
 */
 
static void *_chopping_init(
  int playerc,int handicap,
  void (*cb_finish)(void *userdata,int outcome),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_chopping));
  if (!ctx) return 0;
  CTX->playerc=playerc;
  CTX->handicap=handicap;
  CTX->cb_finish=cb_finish;
  CTX->userdata=userdata;
  CTX->outcome=-2;
  CTX->remaining=VEG_COUNT;
  
  CTX->playerv[0].who=0;
  CTX->playerv[1].who=1;
  player_init(ctx,CTX->playerv+0,YP1,1);
  player_init(ctx,CTX->playerv+1,YP2,(playerc==2)?2:0);
  
  // Assign animinterval according to handicap. At middle handicap, they both run at the middle speed.
  double nhc=(double)CTX->handicap/255.0;
  if (nhc<0.0) nhc=0.0; else if (nhc>1.0) nhc=1.0;
  CTX->playerv[0].animinterval=ANIMINTERVAL_MAX*(1.0-nhc)+ANIMINTERVAL_MIN*nhc;
  CTX->playerv[1].animinterval=ANIMINTERVAL_MIN*(1.0-nhc)+ANIMINTERVAL_MAX*nhc;
  
  //TODO Prepopulate conveyor belts so there's no more than a second of lead-in time. Ensure they're spaced and counted etc as if they'd spawned naturally.
  
  return ctx;
}

/* Prepare a new vegetable.
 */
 
static void veg_init(void *ctx,struct veg *veg,struct player *player) {
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
 
static void veg_finish(void *ctx,struct player *player,struct veg *veg) {
  veg->defunct=1;
  CTX->check_veg=1;
  player->score++;
  bm_sound(RID_sound_collect,0.0);
  
  int dstx=rand()%(17-veg->w);
  int dsty_max=20;
  int dsty_min=16-(player->score/3);
  int dsty=dsty_min+rand()%(dsty_max-dsty_min);
  graf_set_output(&g.graf,player->texid_output);
  graf_set_image(&g.graf,RID_image_battle_fishing);
  graf_decal(&g.graf,dstx,dsty,veg->srcx,veg->srcy,veg->w,veg->h);
  graf_set_output(&g.graf,1);
}

/* Check for vegetables at the chopping point.
 * If any exist, chop them and return nonzero.
 */
 
static int chopping_chop(void *ctx,struct player *player) {
  int result=0;
  struct veg *veg=CTX->vegv;
  int i=CTX->vegc;
  for (;i-->0;veg++) {
    if (veg->who!=player->who) continue;
    if (veg->x>=PKNIFEX) continue;
    if (veg->x+veg->w<=PKNIFEX) continue;
    if (CTX->vegc>=VEG_LIMIT) break; // Oops no room for it.
    result=1;
    int aw=PKNIFEX-veg->x;
    struct veg *nub=CTX->vegv+CTX->vegc++;
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
 
static void player_update_human(void *ctx,struct player *player,double elapsed) {
  if (player->blackout) {
    if (g.input[player->human]&EGG_BTN_SOUTH) return;
    player->blackout=0;
  }
  if (g.input[player->human]&EGG_BTN_SOUTH) {
    if (!player->chopping) {
      player->chopping=1;
      if (chopping_chop(ctx,player)) {
        bm_sound(RID_sound_chop,0.0);
      } else {
        bm_sound(RID_sound_chopmiss,0.0);
      }
    }
  } else {
    player->chopping=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(void *ctx,struct player *player,double elapsed) {
  if (player->chopping) {
    if ((player->cooldown-=elapsed)<=0.0) {
      player->chopping=0;
      player->cooldown=player->kcoolup;
    }
  } else if (player->cooldown>0.0) {
    player->cooldown-=elapsed;
  } else {
    struct veg *veg=CTX->vegv;
    int i=CTX->vegc;
    for (;i-->0;veg++) {
      if (veg->who!=player->who) continue;
      if (veg->defunct) continue;
      if (veg->x>=PKNIFEX) continue;
      if (veg->x+veg->w<=PKNIFEX) continue;
      player->chopping=1;
      player->cooldown=player->kcooldown;
      if (chopping_chop(ctx,player)) {
        bm_sound(RID_sound_chop,0.0);
      } else {
        bm_sound(RID_sound_chopmiss,0.0); // Probably not possible.
      }
      return;
    }
  }
}

/* Update player, regardless of controller.
 */
 
static void player_update_common(void *ctx,struct player *player,double elapsed) {
  int stepc=0;
  player->animclock-=elapsed;
  while (player->animclock<=0.0) {
    stepc++;
    player->animclock+=player->animinterval;
    if (++(player->animframe)>=8) player->animframe=0;
  }
  if (stepc) {
    struct veg *veg=CTX->vegv;
    int i=CTX->vegc;
    for (;i-->0;veg++) {
      if (veg->who!=player->who) continue;
      int pvr=veg->x+veg->w;
      veg->x+=stepc;
      if (player->chopping&&(pvr<=PKNIFEX)&&(veg->x+veg->w>PKNIFEX)) { // A down knife holds up traffic.
        veg->x=PKNIFEX-veg->w;
      }
      if (veg->x>=XEND) veg_finish(ctx,player,veg);
    }
  }
}

/* Update.
 */
 
static void _chopping_update(void *ctx,double elapsed) {
  if (CTX->outcome>-2) {
    if (CTX->end_cooldown>0.0) {
      if ((CTX->end_cooldown-=elapsed)<=0.0) {
        CTX->cb_finish(CTX->userdata,CTX->outcome);
      }
    }
  }
  
  // Update players.
  if (CTX->outcome<=-2) {
    struct player *player=CTX->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (player->human) player_update_human(ctx,player,elapsed);
      else player_update_cpu(ctx,player,elapsed);
      player_update_common(ctx,player,elapsed);
    }
  }
  
  // If a veg defuncted, scan and remove all defuncts.
  if (CTX->check_veg) {
    CTX->check_veg=0;
    int i=CTX->vegc;
    struct veg *veg=CTX->vegv+i-1;
    for (;i-->0;veg--) {
      if (!veg->defunct) continue;
      CTX->vegc--;
      memmove(veg,veg+1,sizeof(struct veg)*(CTX->vegc-i));
    }
  }
  
  /* Add a vegetable to both players when our clock elapses.
   * Timing is identical regardless of conveyor belt speeds. (otherwise a faster belt is actually a huge advantage!)
   * Or if we're depleted, end the game when the last bit defuncts.
   */
  if (CTX->remaining) {
    if ((CTX->outcome<=-2)&&(CTX->vegc<=VEG_LIMIT-2)) {
      if ((CTX->vegclock-=elapsed)<=0.0) {
        CTX->vegclock+=VEG_INTERVAL_MIN+((rand()&0xffff)*(VEG_INTERVAL_MAX-VEG_INTERVAL_MIN))/65535.0;
        CTX->remaining--;
        veg_init(ctx,CTX->vegv+CTX->vegc++,CTX->playerv+0);
        veg_init(ctx,CTX->vegv+CTX->vegc++,CTX->playerv+1);
      }
    }
  } else if (!CTX->vegc&&(CTX->outcome<=-2)) {
    CTX->end_cooldown=END_COOLDOWN_TIME;
    if (CTX->playerv[0].score>CTX->playerv[1].score) CTX->outcome=1;
    else if (CTX->playerv[0].score<CTX->playerv[1].score) CTX->outcome=-1;
    else CTX->outcome=0;
  }
}

/* Render one player.
 * This is the whole thing: Belt, vegetables, hero, output box.
 */
 
static void player_render(void *ctx,struct player *player) {
  
  // Start with the belt.
  graf_set_image(&g.graf,RID_image_battle_fishing);
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
  graf_set_image(&g.graf,RID_image_battle_fishing);
  graf_tile(&g.graf,XEND+8,y,0xa9,0);
  
  // Vegetables on the belt.
  struct veg *veg=CTX->vegv;
  int i=CTX->vegc;
  for (;i-->0;veg++) {
    if (veg->who!=player->who) continue;
    graf_decal(&g.graf,veg->x,veg->y,veg->srcx,veg->srcy,veg->w,veg->h);
  }
  
  // The cook.
  graf_decal(&g.graf,PLEFTX,player->y-40,player->srcx+(player->chopping?32:0),player->srcy,32,48);
  
  // Score indicator.
  graf_decal(&g.graf,XEND+40,player->y-24,128,16,32,32);
  graf_tile(&g.graf,XEND+46,player->y-9,0x30+(player->score/100)%10,0);
  graf_tile(&g.graf,XEND+52,player->y-9,0x30+(player->score/ 10)%10,0);
  graf_tile(&g.graf,XEND+58,player->y-9,0x30+(player->score    )%10,0);
}

/* Render.
 */
 
static void _chopping_render(void *ctx) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x5ca77fff);
  player_render(ctx,CTX->playerv+0);
  player_render(ctx,CTX->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_chopping={
  .name="chopping",
  .battletype=NS_battletype_chopping,
  .flags=BATTLE_FLAG_ONEPLAYER|BATTLE_FLAG_TWOPLAYER,
  .foe_name_strix=54,
  .battle_name_strix=55,
  .del=_chopping_del,
  .init=_chopping_init,
  .update=_chopping_update,
  .render=_chopping_render,
};

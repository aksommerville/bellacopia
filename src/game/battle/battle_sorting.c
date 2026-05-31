/* battle_sorting.c
 * Put the falling things in piles, like with like.
 */

#include "game/bellacopia.h"

#define THING_LIMIT 128
#define TOAST_LIMIT 6

static const uint32_t colorv[]={
  0xff0000ff,
  0x00ff00ff,
  0xff00ffff,
  0xffff00ff,
  0x00ffffff,
};

struct battle_sorting {
  struct battle hdr;
  double playtime;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    int fldx,fldy,fldw,fldh; // Framebuffer pixels, bounds of my field.
    uint32_t bgcolor;
    int drop_poison;
    double xspeed,yspeed,fastspeed;
    int score;
    
    /* The one falling thing is not yet a member of (thingv).
     */
    struct thing {
      int x,y; // Relative to field.
      uint32_t color;
    } thingv[THING_LIMIT];
    int thingc;
    double thingx,thingy;
    uint32_t thingcolor; // Zero if no falling thing.
    
    // CPU
    int cpu_thingc; // We reassess target when (thingc) changes.
    double targetx;
    double fudgelo,fudgehi; // Pixels, how far off we can land.
    
  } playerv[2];
  
  struct toast {
    double ttl;
    int x,y; // Framebuffer pixels
    int v;
  } toastv[TOAST_LIMIT];
  int toastc;
  double toastclock; // They all move together.
};

#define BATTLE ((struct battle_sorting*)battle)

/* Delete.
 */
 
static void _sorting_del(struct battle *battle) {
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
    player->drop_poison=1;
  } else { // CPU.
    player->cpu_thingc=-1;
    player->fudgehi=2.0*player->skill+8.0*(1.0-player->skill);
    player->fudgelo=0.0*player->skill+4.0*(1.0-player->skill);
  }
  switch (face) {
    case NS_face_monster: {
        player->bgcolor=0x8a552bff;
      } break;
    case NS_face_dot: {
        player->bgcolor=0x411775ff;
      } break;
    case NS_face_princess: {
        player->bgcolor=0x0d3ac1ff;
      } break;
  }
  player->xspeed=80.0*player->skill+40.0*(1.0-player->skill);
  player->yspeed=25.0*player->skill+15.0*(1.0-player->skill);
  player->fastspeed=150.0*player->skill+100.0*(1.0-player->skill);
  if (!player->human) { // CPU speed penalty.
    const double penalty=0.750;
    player->xspeed*=penalty;
    player->yspeed*=penalty;
    player->fastspeed*=penalty;
  }
  
  player->fldw=120;
  player->fldh=140;
  player->fldx=(player->who?((FBW>>1)+10):((FBW>>1)-10-player->fldw));
  player->fldy=(FBH>>1)-(player->fldh>>1);
}

/* New.
 */
 
static int _sorting_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playtime=20.0;
  return 0;
}

/* Award points for a new thing at (ax), relative to a same-color thing at (bx).
 */
 
static void sorting_get_points(struct battle *battle,struct player *player,double ax,double bx,double y) {
  double dx=ax-bx;
  if (dx<0.0) dx=-dx;
  const double xlimit=10.0;
  int score=(int)(xlimit-dx);
  if (score<=0) return;
  player->score+=score;
  bm_sound_pan(RID_sound_collect,player->who?0.250:-0.250);
  
  if (BATTLE->toastc<TOAST_LIMIT) {
    struct toast *toast=BATTLE->toastv+BATTLE->toastc++;
    toast->ttl=1.0;
    toast->x=player->fldx+(int)ax;
    toast->y=player->fldy+(int)y-10;
    toast->v=score;
  }
}

/* Find the highest thing near (x), ie the thing you would land on.
 */
 
static struct thing *sorting_find_table(struct battle *battle,struct player *player,int x) {
  struct thing *table=0;
  struct thing *thing=player->thingv;
  int i=player->thingc;
  const int thresh=14;
  for (;i-->0;thing++) {
    int dx=x-thing->x;
    if ((dx<=-thresh)||(dx>=thresh)) continue;
    if (!table||(thing->y<table->y)) table=thing;
  }
  return table;
}

/* Move the active thing.
 */
 
static void sorting_move(struct battle *battle,struct player *player,int d,double elapsed) {
  if (!player->thingcolor) return;
  player->thingx+=player->xspeed*d*elapsed;
  if (player->thingx<8.0) player->thingx=8.0;
  else if (player->thingx>player->fldw-8.0) player->thingx=player->fldw-8.0;
}

static void sorting_drop(struct battle *battle,struct player *player,int fast,double elapsed) {
  if (!player->thingcolor) return;
  double speed=fast?player->fastspeed:player->yspeed;
  player->thingy+=speed*elapsed;
  struct thing *table=sorting_find_table(battle,player,player->thingx);
  double floor;
  if (table) floor=table->y-2.0;
  else floor=player->fldh-8.0;
  if (player->thingy>=floor) {
    bm_sound_pan(RID_sound_bump,player->who?0.250:-0.250);
    if (table) {
      if (table->color==player->thingcolor) {
        sorting_get_points(battle,player,player->thingx,table->x,table->y);
      }
    }
    if (player->thingc<THING_LIMIT) {
      struct thing *thing=player->thingv+player->thingc++;
      thing->color=player->thingcolor;
      thing->x=(int)player->thingx;
      thing->y=(int)floor;
    }
    player->thingcolor=0;
    player->drop_poison=1;
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: sorting_move(battle,player,-1,elapsed); break;
    case EGG_BTN_RIGHT: sorting_move(battle,player,1,elapsed); break;
  }
  if (player->drop_poison) {
    if (!(input&EGG_BTN_DOWN)) player->drop_poison=0;
    sorting_drop(battle,player,0,elapsed);
  } else if (input&EGG_BTN_DOWN) {
    sorting_drop(battle,player,1,elapsed);
  } else {
    sorting_drop(battle,player,0,elapsed);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (!player->thingcolor) return;
  
  /* Find the highest resting thing of this color.
   * If there's something else on top of it, well oops that sucks.
   * If nothing of this color has been dropped, find some open space.
   */
  if (player->cpu_thingc!=player->thingc) {
    player->cpu_thingc=player->thingc;
    if (!player->thingc) {
      player->targetx=player->thingx; // First thing goes wherever RNG chose.
    } else {
      struct thing *floor=0;
      struct thing *thing=player->thingv;
      int i=player->thingc;
      for (;i-->0;thing++) {
        if (thing->color!=player->thingcolor) continue;
        if (!floor||(thing->y<floor->y)) floor=thing;
      }
      if (floor) {
        player->targetx=floor->x;
      } else {
        /* Find the widest gap between things. There might be a smart way to do this, but smart really isn't our style, is it?
         * Check to the right of each thing, to its closest neighbor. Record the longest.
         * Record the leftmost and rightmost thing.
         */
        int xlo=player->fldw-8,xhi=8;
        int narrowx=0,narroww=0;
        for (i=0,thing=player->thingv;i<player->thingc;i++,thing++) {
          if (thing->x<xlo) xlo=thing->x;
          if (thing->x>xhi) xhi=thing->x;
          int bi=0,gapw=FBW;
          struct thing *bthing=player->thingv;
          for (;bi<player->thingc;bi++,bthing++) {
            if (bthing->x<=thing->x) continue;
            int dx=bthing->x-thing->x;
            if (dx<gapw) gapw=dx;
          }
          if (gapw>=FBW) continue;
          if (gapw>narroww) {
            narrowx=thing->x;
            narroww=gapw;
          }
        }
        if (xlo>narroww) { narrowx=0; narroww=xlo; }
        xhi=player->fldw-xhi;
        if (xhi>narroww) { narrowx=player->fldw-xhi; narroww=xhi; }
        player->targetx=narrowx+(narroww>>1);
      }
    }
    double fudge=((rand()&0xffff)/65535.0); // mmm double fudge
    if (rand()&1) player->targetx+=fudge; else player->targetx-=fudge;
  }
  
  /* Approach (targetx) and if we cross it, drop fast.
   * We could start dropping sooner, if we estimate the diagonal path when moving and dropping together.
   * Highly doable but would be more complicated, and I think we need to nerf the CPU either way so hey.
   */
  int fast=0;
  if (player->thingx<player->targetx) {
    sorting_move(battle,player,1,elapsed);
    if (player->thingx>=player->targetx) player->thingx=player->targetx;
  } else if (player->thingx>player->targetx) {
    sorting_move(battle,player,-1,elapsed);
    if (player->thingx<=player->targetx) player->thingx=player->targetx;
  } else {
    fast=1;
  }
  sorting_drop(battle,player,fast,elapsed);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (!player->thingcolor&&(player->thingc<THING_LIMIT)) {
    player->thingx=8.0+(rand()%(player->fldw-16));
    player->thingy=8.0;
    const int colorc=sizeof(colorv)/sizeof(colorv[0]);
    int colorp=rand()%colorc;
    player->thingcolor=colorv[colorp];
  }
}

/* Update.
 */
 
static void _sorting_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  // Game is over when the clock says so. Ties are perfectly possible.
  if ((BATTLE->playtime-=elapsed)<=0.0) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
    return;
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  struct toast *toast=BATTLE->toastv;
  for (i=BATTLE->toastc;i-->0;toast++) {
    toast->ttl-=elapsed;
  }
  while ((BATTLE->toastc>0)&&(BATTLE->toastv[BATTLE->toastc-1].ttl<=0.0)) BATTLE->toastc--;
  if ((BATTLE->toastclock-=elapsed)<=0.0) {
    BATTLE->toastclock+=0.060;
    for (toast=BATTLE->toastv,i=BATTLE->toastc;i-->0;toast++) {
      toast->y--;
    }
  }
}

/* Render player.
 */
 
static void player_render_back(struct battle *battle,struct player *player) {
  graf_fill_rect(&g.graf,player->fldx-2,player->fldy-2,player->fldw+4,player->fldh+4,0xffffffff);
  graf_fill_rect(&g.graf,player->fldx-1,player->fldy-1,player->fldw+2,player->fldh+2,0x000000ff);
  graf_fill_rect(&g.graf,player->fldx,player->fldy,player->fldw,player->fldh,player->bgcolor);
}

static void player_render_front(struct battle *battle,struct player *player) {
  struct thing *thing=player->thingv;
  int i=player->thingc;
  for (;i-->0;thing++) {
    graf_fancy(&g.graf,player->fldx+thing->x,player->fldy+thing->y,0x4f,0,0,NS_sys_tilesize,0,thing->color);
  }
  if (player->thingcolor) {
    graf_fancy(&g.graf,player->fldx+(int)player->thingx,player->fldy+(int)player->thingy,0x4f,0,0,NS_sys_tilesize,0,player->thingcolor);
  }
}

static void sorting_render_score(struct battle *battle,struct player *player) {
  int n=player->score;
  if (n<0) n=0; else if (n>999) n=999;
  int y=player->fldy-8;
  int x=player->fldx+(player->fldw>>1)+6;
  graf_tile(&g.graf,x,y,'0'+n%10,0); if (!(n/=10)) return; x-=8;
  graf_tile(&g.graf,x,y,'0'+n%10,0); if (!(n/=10)) return; x-=8;
  graf_tile(&g.graf,x,y,'0'+n%10,0);
}

/* Render.
 */
 
static void _sorting_render(struct battle *battle) {

  int ms=(int)(BATTLE->playtime*1000.0);
  if (ms<0) ms=0;
  int sec=ms/1000; ms%=1000;
  if (sec||ms) sec++;
  if (sec>99) sec=99; // huh?

  // Flash the background when end time approaches.
  uint32_t bgcolor=0x808080ff;
  if (sec<=3) {
    if (ms>500) {
      int r=0x80+((ms-500)*128)/500;
      if (r>0xff) r=0xff;
      bgcolor=(r<<24)|0x008080ff;
    }
  }
  graf_fill_rect(&g.graf,0,0,FBW,FBH,bgcolor);
  player_render_back(battle,BATTLE->playerv+0);
  player_render_back(battle,BATTLE->playerv+1);

  graf_set_image(&g.graf,RID_image_battle_fractia2);
  player_render_front(battle,BATTLE->playerv+0);
  player_render_front(battle,BATTLE->playerv+1);
  graf_set_image(&g.graf,RID_image_fonttiles);
  sorting_render_score(battle,BATTLE->playerv+0);
  sorting_render_score(battle,BATTLE->playerv+1);
  if (sec>=10) {
    graf_tile(&g.graf,(FBW>>1)-4,10,'0'+sec/10,0);
    graf_tile(&g.graf,(FBW>>1)+4,10,'0'+sec%10,0);
  } else {
    graf_tile(&g.graf,FBW>>1,10,'0'+sec,0);
  }

  if (BATTLE->toastc) {
    graf_set_image(&g.graf,RID_image_pause);
    graf_set_tint(&g.graf,0xffffffff);
    struct toast *toast=BATTLE->toastv;
    int i=BATTLE->toastc;
    for (;i-->0;toast++) {
      if (toast->ttl<=0.0) continue;
      if (toast->v<10) {
        graf_tile(&g.graf,toast->x,toast->y,0x90+toast->v,0);
      } else {
        graf_tile(&g.graf,toast->x,toast->y,0x90+toast->v/10,0);
        graf_tile(&g.graf,toast->x+4,toast->y,0x90+toast->v%10,0);
      }
    }
    graf_set_tint(&g.graf,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_sorting={
  .name="sorting",
  .objlen=sizeof(struct battle_sorting),
  .id=NS_battle_sorting,
  .strix_name=165,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_sorting_del,
  .init=_sorting_init,
  .update=_sorting_update,
  .render=_sorting_render,
};

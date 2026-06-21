/* battle_rescuing.c
 * Mom throws her babies out the window of her burning building, and you trampoline them to safety.
 */

#include "game/bellacopia.h"

#define BGCOLC 20
#define BGROWC 12
#define WINDOW_LIMIT 6
#define WINDOW_ANIM_PERIOD 0.200
#define WINDOW_ANIM_FRAMEC 4
#define BABY_LIMIT 12 /* Must be even; max half of this per side. */
#define GRAVITY 150.0 /* m/s**2 */

struct battle_rescuing {
  struct battle hdr;
  double battleclock; // Counts up from init.
  
  /* The animated windows contain 0x20, the interior background.
   * So this entire list is constant.
   */
  struct egg_render_tile bgvtxv[BGCOLC*BGROWC];
  
  struct window {
    int vtxp; // Index in (bgvtxv); use that for the position.
    int animframe;
    double animclock;
    int opened;
    uint8_t xform; // For interior content only, not the flames or frames.
    double mamaclock;
    double armclock;
    int who;
  } windowv[WINDOW_LIMIT];
  int windowc;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    double x; // Center of trampoline, in framebuffer pixels.
    uint8_t xform; // Natural for right left player: hero left of trampoline, facing right.
    uint8_t tileid;
    double walkspeed; // px/s
    int input; // -1,0,1. Man and CPU controllers set this and that's all.
    double animclock;
    int animframe;
    int score; // How many rescued.
    double bounceclock;
    double screechclock;
  } playerv[2];
  
  /* "baby" is all that have lived or will, including the mothers.
   * The entire set of defenestrations is decided during init.
   */
  struct baby {
  // Set at init:
    int who; // 0,1. Which player's building did we fly out of, and whose score do we count toward.
    double jumptime; // We become active when battleclock crosses this.
    int adult; // Nonzero if we're mom, otherwise we're a baby.
    int catchme; // Advice to my CPU player, whether he's supposed to catch or miss.
  // Sort themselves out live:
    double x,y; // Center, in framebuffer pixels.
    double dx,dy; // (dx) is constant; (dy) adjusts per gravity while flying.
    int outcome; // (-1,0,1) = (splattered,unfinished,rescued).
    int jumped; // Nonzero if we've started.
    double animclock;
    int animframe;
    int bounced;
  } babyv[BABY_LIMIT];
  int babyc;
};

#define BATTLE ((struct battle_rescuing*)battle)

/* Delete.
 */
 
static void _rescuing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.333;
    player->xform=EGG_XFORM_XREV;
  } else { // Right.
    player->who=1;
    player->x=FBW*0.666;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x2b;
      } break;
    case NS_face_dot: {
        player->tileid=0x27;
      } break;
    case NS_face_princess: {
        player->tileid=0x29;
      } break;
  }
  player->walkspeed=40.0*(1.0-player->skill)+60.0*player->skill;
}

/* Add a window.
 * Updates both (bgvtxv) and (windowv).
 */
 
static struct window *rescuing_add_window(struct battle *battle,int x,int y,int opened) {
  if ((x<0)||(x>=BGCOLC)||(y<0)||(y>=BGROWC)||(BATTLE->windowc>=WINDOW_LIMIT)) return 0;
  struct window *window=BATTLE->windowv+BATTLE->windowc++;
  window->vtxp=y*BGCOLC+x;
  window->animframe=rand()%WINDOW_ANIM_FRAMEC;
  window->animclock=((rand()&0xffff)*WINDOW_ANIM_PERIOD)/65535.0;
  if (window->opened=opened) {
    window->mamaclock=2.000;
  }
  if (x>=BGCOLC>>1) {
    window->xform=EGG_XFORM_XREV;
    window->who=1;
  }
  BATTLE->bgvtxv[window->vtxp].tileid=0x20;
  return window;
}

/* Generate the background grid.
 */
 
static void rescuing_fill_bg(struct battle *battle,int x,int y,int w,int h,uint8_t tileid) {
  struct egg_render_tile *dstrow=BATTLE->bgvtxv+y*BGCOLC+x;
  for (;h-->0;dstrow+=BGCOLC) {
    struct egg_render_tile *dstp=dstrow;
    int xi=w;
    for (;xi-->0;dstp++) {
      dstp->tileid=tileid;
    }
  }
}

static void rescuing_fill_sky(struct battle *battle,int x,int y,int w,int h) {
  struct egg_render_tile *dstrow=BATTLE->bgvtxv+y*BGCOLC+x;
  for (;h-->0;dstrow+=BGCOLC) {
    struct egg_render_tile *dstp=dstrow;
    int xi=w;
    for (;xi-->0;dstp++) {
      // Prefer 0x00 full blank. Randomly 0x01..0x05 stars.
      int choice=rand()%10;
      if (choice>0x05) dstp->tileid=0x00;
      else dstp->tileid=choice;
    }
  }
}
 
static void rescuing_generate_background(struct battle *battle) {

  // First populate output positions. Our grid matches the framebuffer width exactly, but not the height.
  int dsty=(FBH>>1)-((BGROWC*NS_sys_tilesize)>>1)+(NS_sys_tilesize>>1);
  int dstx0=NS_sys_tilesize>>1;
  struct egg_render_tile *vtx=BATTLE->bgvtxv;
  int row=0;
  for (;row<BGROWC;row++,dsty+=NS_sys_tilesize) {
    int col=0;
    int dstx=dstx0;
    for (;col<BGCOLC;col++,dstx+=NS_sys_tilesize,vtx++) {
      vtx->x=dstx;
      vtx->y=dsty;
    }
  }

  const int buildingw=4;
  rescuing_fill_bg(battle,0,BGROWC-1,BGCOLC,1,0x06); // Sidewalk at the bottom row.
  rescuing_fill_bg(battle,0,0,buildingw,BGROWC-1,0x10); // Left building, initial.
  rescuing_fill_bg(battle,BGCOLC-buildingw,0,buildingw,BGROWC-1,0x10); // Right building, initial.
  rescuing_fill_sky(battle,buildingw,0,BGCOLC-buildingw*2,BGROWC-1);
  // Windows...
  rescuing_add_window(battle,2,2,1);
  rescuing_add_window(battle,2,4,0);
  rescuing_add_window(battle,2,6,0);
  rescuing_add_window(battle,BGCOLC-3,2,1);
  rescuing_add_window(battle,BGCOLC-3,4,0);
  rescuing_add_window(battle,BGCOLC-3,6,0);
}

/* If this is a CPU player, decide how many babies it should miss, pick so many at random and mark them.
 * The CPU always misses at least one, otherwise it's unbeatable.
 */
 
static void rescuing_prepare_misses(struct battle *battle,struct player *player) {
  if (player->human) return;
  int missc;
  if (player->skill>=0.750) missc=1;
  else if (player->skill>=0.500) missc=2;
  else if (player->skill>=0.250) missc=3;
  else missc=4;
  struct baby *babyv[BABY_LIMIT];
  int babyc=0;
  struct baby *q=BATTLE->babyv;
  int i=BATTLE->babyc;
  for (;i-->0;q++) {
    if (q->who!=player->who) continue;
    babyv[babyc++]=q;
  }
  if (missc>babyc) missc=babyc;
  while (missc-->0) {
    int p=rand()%babyc;
    struct baby *baby=babyv[p];
    babyc--;
    memmove(babyv+p,babyv+p+1,sizeof(void*)*(babyc-p));
    baby->catchme=0;
  }
}

/* Decide when the babies are going to fly, and whether the CPU player should catch them.
 */
 
static void rescuing_generate_babies_plan(struct battle *battle) {
  int babyc_per_player=BABY_LIMIT>>1;
  double when=2.0;
  #define NEXTJUMPTIME ({ \
    when+=1.0+((rand()&0xffff)*2.0)/65535.0; \
    (when); \
  })
  int i=babyc_per_player;
  while (i-->0) {
  
    struct baby *baby=BATTLE->babyv+BATTLE->babyc++;
    baby->who=0;
    baby->jumptime=NEXTJUMPTIME;
    baby->adult=!i;
    baby->catchme=1;
    
    baby=BATTLE->babyv+BATTLE->babyc++;
    baby->who=1;
    baby->jumptime=NEXTJUMPTIME;
    baby->adult=!i;
    baby->catchme=1;
  }
  #undef NEXTJUMPTIME
  rescuing_prepare_misses(battle,BATTLE->playerv+0);
  rescuing_prepare_misses(battle,BATTLE->playerv+1);
}

/* New.
 */
 
static int _rescuing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  rescuing_generate_background(battle);
  rescuing_generate_babies_plan(battle);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->input=-1; break;
    case EGG_BTN_RIGHT: player->input=1; break;
    default: player->input=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  
  struct baby *baby=0;
  struct baby *q=BATTLE->babyv;
  int i=BATTLE->babyc;
  for (;i-->0;q++) {
    if (q->outcome) continue;
    if (q->bounced) continue;
    if (q->who!=player->who) continue;
    if (q->jumptime>BATTLE->battleclock) continue;
    baby=q;
    break;
  }
  
  if (!baby) {
    player->input=0;
    return;
  }
  double babyx=baby->x;
  if (!baby->catchme) {
    if (player->who) babyx-=40.0;
    else babyx+=40.0;
  }
  const double far=10.0;
  double dx=babyx-player->x;
  if (dx<-far) player->input=-1;
  else if (dx>far) player->input=1;
  else if ((dx<0.0)&&(player->input<0)) ;
  else if ((dx>0.0)&&(player->input>0)) ;
  else player->input=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Move and animate if requested.
   * Clamp to the screen edge. Don't worry about the other player.
   */
  if (player->input) {
    player->x+=player->input*player->walkspeed*elapsed;
    if (player->x<0.0) player->x=0.0;
    else if (player->x>FBW) player->x=FBW;
    
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.200;
      if (++(player->animframe)>=2) player->animframe=0;
    }
    
    if ((player->screechclock-=elapsed)<=0.0) {
      player->screechclock+=0.150;
      bm_sound_pan(RID_sound_screech,player->who?PLAYER_PAN:-PLAYER_PAN);
    }
  } else {
    player->animclock=0.0;
    player->animframe=0;
    player->screechclock=0.0;
  }
  
  if (player->bounceclock>0.0) player->bounceclock-=elapsed;
}

/* Update a baby.
 */
 
static void baby_update(struct battle *battle,struct baby *baby,double elapsed) {
  
  /* Not started, or outcome already established?
   */
  if (baby->outcome||(BATTLE->battleclock<baby->jumptime)) return;
  
  /* Beginning the jump?
   */
  if (!baby->jumped) {
    bm_sound_pan(RID_sound_throw,baby->who?PLAYER_PAN:-PLAYER_PAN);
    if (!baby->adult) {
      struct window *window=BATTLE->windowv;
      int i=BATTLE->windowc;
      for (;i-->0;window++) {
        if (!window->opened) continue;
        if (window->who!=baby->who) continue;
        window->armclock=0.750;
        break;
      }
    }
    baby->jumped=1;
    int col=baby->who?(BGCOLC-3):2;
    baby->x=col*NS_sys_tilesize+(NS_sys_tilesize>>1);
    baby->y=(2*NS_sys_tilesize)+(NS_sys_tilesize>>1);
    double n=(rand()&0xffff)/65535.0;
    baby->dx=-10.0*(1.0-n)+60.0*n;
    if (baby->who) baby->dx=-baby->dx;
    baby->dy=-100.0;
  }
  
  /* Tick animation.
   */
  if ((baby->animclock-=elapsed)<=0.0) {
    baby->animclock+=0.200;
    if (++(baby->animframe)>=2) baby->animframe=0;
  }
  
  /* Apply gravity and move.
   */
  baby->dy+=GRAVITY*elapsed;
  baby->x+=baby->dx*elapsed;
  baby->y+=baby->dy*elapsed;
  
  /* Splat or land?
   */
  if (baby->y>BATTLE->bgvtxv[BGCOLC*(BGROWC-2)].y) {
    struct player *player=BATTLE->playerv+baby->who;
    if (baby->bounced) {
      bm_sound_pan(RID_sound_collect,baby->who?PLAYER_PAN:-PLAYER_PAN);
      baby->outcome=1;
      baby->y=BATTLE->bgvtxv[BGCOLC*(BGROWC-1)].y-4;
      player->score++;
    } else {
      bm_sound_pan(RID_sound_splat,baby->who?PLAYER_PAN:-PLAYER_PAN);
      baby->outcome=-1;
      baby->y=BATTLE->bgvtxv[BGCOLC*(BGROWC-2)].y+(NS_sys_tilesize>>1);
    }
    return;
  }
  
  /* Bounce?
   */
  if ((baby->y>BATTLE->bgvtxv[BGCOLC*(BGROWC-3)].y)&&(baby->dy>0.0)&&!baby->bounced) {
    struct player *player=BATTLE->playerv+baby->who;
    double distance=baby->x-player->x;
    if (distance<0.0) distance=-distance;
    if (distance<20.0) {
      bm_sound_pan(RID_sound_jump,baby->who?PLAYER_PAN:-PLAYER_PAN);
      baby->bounced=1;
      player->bounceclock=0.500;
      baby->dy=-40.0;
    }
  }
}

/* Update.
 */
 
static void _rescuing_update(struct battle *battle,double elapsed) {

  BATTLE->battleclock+=elapsed;

  // Animate flames.
  struct window *window=BATTLE->windowv;
  int i=BATTLE->windowc;
  for (;i-->0;window++) {
    if ((window->animclock-=elapsed)<=0.0) {
      window->animclock+=WINDOW_ANIM_PERIOD;
      if (++(window->animframe)>=WINDOW_ANIM_FRAMEC) window->animframe=0;
    }
    if (window->mamaclock>0.0) window->mamaclock-=elapsed;
    if (window->armclock>0.0) window->armclock-=elapsed;
  }
  
  // Do update players even after finished, why not.
  struct player *player=BATTLE->playerv;
  for (i=2;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  if (battle->outcome>-2) return;
  
  // Update babies, and if at least one has outcome unestablished, proceed.
  int finished=1;
  struct baby *baby=BATTLE->babyv;
  for (i=BATTLE->babyc;i-->0;baby++) {
    baby_update(battle,baby,elapsed);
    if (!baby->outcome) finished=0;
  }
  
  if (finished) {
    int l=BATTLE->playerv[0].score;
    int r=BATTLE->playerv[1].score;
    if (l>r) battle->outcome=1;
    else if (l<r) battle->outcome=-1;
    else battle->outcome=0;
  }

  //XXX
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int midx=(int)player->x; // center of trampoline
  int xhero,xmid,xtoe; // Three tiles.
  if (player->xform) {
    xtoe=midx-(NS_sys_tilesize>>1);
    xmid=xtoe+NS_sys_tilesize;
    xhero=xmid+NS_sys_tilesize;
  } else {
    xtoe=midx+(NS_sys_tilesize>>1);
    xmid=xtoe-NS_sys_tilesize;
    xhero=xmid-NS_sys_tilesize;
  }
  int y=BATTLE->bgvtxv[BGCOLC*(BGROWC-2)].y;
  uint8_t tramptile=0x23;
  if (player->bounceclock>0.0) tramptile+=2;
  uint8_t herotile=player->tileid;
  if (player->animframe) herotile+=1;
  graf_tile(&g.graf,xtoe,y,tramptile+1,player->xform);
  graf_tile(&g.graf,xmid,y,tramptile,player->xform);
  graf_tile(&g.graf,xhero,y,herotile,player->xform);
}

/* Render window.
 */
 
static void window_render(struct battle *battle,struct window *window) {
  int x=BATTLE->bgvtxv[window->vtxp].x;
  int y=BATTLE->bgvtxv[window->vtxp].y;
  uint8_t firetile=0x21;
  uint8_t firexform=0;
  switch (window->animframe) {
    case 1: firetile++; break;
    case 2: firexform=EGG_XFORM_XREV; break;
    case 3: firexform=EGG_XFORM_XREV; firetile++; break;
  }
  graf_tile(&g.graf,x,y,firetile,firexform);
  if (window->opened) {
    graf_tile(&g.graf,x,y,0x12,0); // Full frame.
    if (window->mamaclock>0.0) {
      uint8_t mt=0x14;
      if (((int)(window->mamaclock*6.0))&1) mt+=2;
      if (window->xform&EGG_XFORM_XREV) {
        graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y,mt+1,EGG_XFORM_XREV);
        graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y,mt,EGG_XFORM_XREV);
      } else {
        graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y,mt,0);
        graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y,mt+1,0);
      }
    } else if (window->armclock>0.0) {
      graf_tile(&g.graf,x,y,0x18,window->xform);
    }
    graf_tile(&g.graf,x,y,0x13,0); // Frame foreground.
  } else {
    graf_tile(&g.graf,x,y,0x11,0);
  }
}

/* Render one baby or mom.
 * Note that the distressed mom during our intro, and the arm flinging a baby out the window,
 * those are part of the window, not us.
 */
 
static void baby_render(struct battle *battle,struct baby *baby) {
  if (!baby->jumped) return;
  int x=(int)baby->x;
  int y=(int)baby->y;
  if (baby->adult) {
    uint8_t xform=baby->who?EGG_XFORM_XREV:0;
    if (baby->outcome>0) { // Mom, standing upright. (x,y) is the center of her lower tile.
      graf_tile(&g.graf,x,y,0x1d,xform);
      graf_tile(&g.graf,x,y-NS_sys_tilesize,0x0d,xform);
    } else if (baby->outcome<0) { // Mom, kersplatted.
      if (xform) {
        graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y,0x0c,xform);
        graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y,0x0b,xform);
      } else {
        graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y,0x0b,0);
        graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y,0x0c,0);
      }
    } else { // Mom, falling.
      uint8_t tileid=0x07;
      if (baby->animframe) tileid+=2;
      if (xform) {
        graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y,tileid+1,xform);
        graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y,tileid,xform);
      } else {
        graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y,tileid,0);
        graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y,tileid+1,0);
      }
    }
  } else { // The real babies are close enough to symmetric, don't bother with (xform).
    if (baby->outcome>0) { // Baby, rescued.
      graf_tile(&g.graf,x,y,0x1b,0);
    } else if (baby->outcome<0) { // Baby, kersplatted.
      graf_tile(&g.graf,x,y,0x1c,0);
    } else { // Baby, falling.
      graf_tile(&g.graf,x,y,0x19+baby->animframe,0);
    }
  }
}

/* Render.
 */
 
static void _rescuing_render(struct battle *battle) {
  graf_set_image(&g.graf,RID_image_battle_fractia2);
  graf_tile_batch(&g.graf,BATTLE->bgvtxv,BGCOLC*BGROWC);
  
  struct window *window=BATTLE->windowv;
  int i=BATTLE->windowc;
  for (;i-->0;window++) window_render(battle,window);
  
  // Draw the dead babies first.
  struct baby *baby=BATTLE->babyv;
  for (i=BATTLE->babyc;i-->0;baby++) {
    if (baby->outcome>=0) continue;
    baby_render(battle,baby);
  }
  
  // Players are permitted to overlap. Better to draw [0] last, so in one-player mode the human shows in front.
  player_render(battle,BATTLE->playerv+1);
  player_render(battle,BATTLE->playerv+0);
  
  // Then the live babies.
  for (baby=BATTLE->babyv,i=BATTLE->babyc;i-->0;baby++) {
    if (baby->outcome<0) continue;
    baby_render(battle,baby);
  }
  
  // If the outcome is established, print each player's score above their head.
  if (battle->outcome>-2) {
    graf_set_image(&g.graf,RID_image_fonttiles);
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    graf_tile(&g.graf,(int)l->x+24,FBH-30,'0'+l->score,0);
    graf_tile(&g.graf,(int)r->x-24,FBH-30,'0'+r->score,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_rescuing={
  .name="rescuing",
  .objlen=sizeof(struct battle_rescuing),
  .id=NS_battle_rescuing,
  .strix_name=157,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_rescuing_del,
  .init=_rescuing_init,
  .update=_rescuing_update,
  .render=_rescuing_render,
};

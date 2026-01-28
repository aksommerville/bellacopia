//TODO bluefish. For now, this is an exact copy of greenfish.
// I want the fish to deploy a parachute at the crest, then at some random point, detach and dive.

#include "game/bellacopia.h"

#define GROUNDY 160 /* >16 off the framebuffer's bottom. */
#define END_COOLDOWN 1.5
#define SKY_COLOR 0x5e9fc7ff
#define GROUND_COLOR 0x126e29ff
#define FLIGHT_TIME 3.000 /* How long for the fish to go from bottom to top and back. */
#define FISHDY_INITIAL -300.0
#define FISHDY_ACCEL 250.0

struct battle_bluefish {
  uint8_t handicap;
  uint8_t players;
  void (*cb_end)(int outcome,void *userdata);
  void *userdata;
  double wanimclock;
  int wanimframe;
  int outcome; // -2 until established
  double cooldown;
  
  // Positions in floating-point framebuffer pixels; velocities in px/s.
  double dotx;
  int dotframe; // 48x48 decals in one row.
  double dotanimclock;
  uint8_t dotxform;
  double fishx,fishy;
  double fishdx,fishdy;
  uint8_t fishtileid;
  uint8_t fishxform;
  double fishanimclock;
};

#define CTX ((struct battle_bluefish*)ctx)

/* Delete.
 */
 
static void _bluefish_del(void *ctx) {
  free(ctx);
}

/* New.
 */
 
static void *_bluefish_init(
  uint8_t handicap,
  uint8_t players,
  void (*cb_end)(int outcome,void *userdata),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_bluefish));
  if (!ctx) return 0;
  CTX->handicap=handicap;
  CTX->players=players;
  CTX->cb_end=cb_end;
  CTX->userdata=userdata;
  CTX->outcome=-2;
  
  CTX->dotx=FBW>>1;
  
  /* Choose fish position and direction.
   * Initial position is anywhere along the framebuffer, at the bottom.
   * Horizontal velocity will be constant. So pick one that yields a landing position also within the framebuffer's width.
   */
  CTX->fishy=FBH;
  CTX->fishx=rand()%FBW;
  double targetx=rand()%FBW; // Anywhere is fine, even exactly the same as (fishx).
  CTX->fishdx=(targetx-CTX->fishx)/FLIGHT_TIME;
  CTX->fishtileid=0x19;
  CTX->fishxform=(targetx<CTX->fishx)?EGG_XFORM_XREV:0;
  CTX->fishdy=FISHDY_INITIAL;
  
  return ctx;
}

/* Dot's motion.
 */
 
static void bluefish_walk(void *ctx,double elapsed,int d) {
  if ((CTX->dotanimclock-=elapsed)<=0.0) {
    CTX->dotanimclock+=0.200;
    if (++(CTX->dotframe)>=2) CTX->dotframe=0;
  }
  if (d<0) CTX->dotxform=EGG_XFORM_XREV;
  else CTX->dotxform=0;
  double speed=80.0; // px/s
  CTX->dotx+=speed*elapsed*d;
  if (CTX->dotx<0.0) CTX->dotx=0.0;
  else if (CTX->dotx>FBW) CTX->dotx=FBW;
}

static void bluefish_walk_none(void *ctx,double elapsed) {
  CTX->dotframe=0;
  CTX->dotanimclock=0.0;
}

/* Move the fish.
 */
 
static void bluefish_move_fish(void *ctx,double elapsed) {
  if ((CTX->fishanimclock-=elapsed)<=0.0) {
    CTX->fishanimclock+=0.200;
    if (CTX->fishtileid==0x19) CTX->fishtileid=0x29;
    else CTX->fishtileid=0x19;
  }
  double pvdy=CTX->fishdy;
  CTX->fishdy+=FISHDY_ACCEL*elapsed;
  if ((pvdy<=0.0)&&(CTX->fishdy>0.0)) { // Crested.
    if (CTX->fishxform==0) CTX->fishxform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
    else if (CTX->fishxform==EGG_XFORM_XREV) CTX->fishxform=EGG_XFORM_SWAP;
  }
  CTX->fishy+=CTX->fishdy*elapsed;
  CTX->fishx+=CTX->fishdx*elapsed;
}

/* Check both terminal conditions.
 * Return -2 if undetermined, -1 if the fish escaped, or 1 if Dot caught it.
 * In theory, 0 if tied, but we don't do ties.
 */
 
static int bluefish_check_termination(void *ctx) {
  if (CTX->fishy>FBH) return -1; // Fish is back in the sea.
  if (CTX->fishdy<=0.0) return -2; // Still rising.
  if ((CTX->fishy>=GROUNDY-32.0)&&(CTX->fishy<=GROUNDY-8.0)) { // Catchy vertical range...
    double dx=CTX->fishx-CTX->dotx;
    if (dx<0.0) dx=-dx;
    if (dx<=25.0) return 1; // Caught!
  }
  return -2;
}

/* Update.
 */
 
static void _bluefish_update(void *ctx,double elapsed) {
  
  // Animate water.
  if ((CTX->wanimclock-=elapsed)<=0.0) {
    CTX->wanimclock+=0.150;
    if (++(CTX->wanimframe)>=6) CTX->wanimframe=0; // 4 frames, pingponging
  }
  
  // Finished? Tick the cooldown, but no further model activity.
  if (CTX->outcome!=-2) {
    if (CTX->cb_end) {
      if ((CTX->cooldown-=elapsed)<=0.0) {
        CTX->cb_end(CTX->outcome,CTX->userdata);
        CTX->cb_end=0;
      }
    }
    return;
  }
  
  // Dot's motion.
  switch (g.input[0]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: bluefish_walk(ctx,elapsed,-1); break;
    case EGG_BTN_RIGHT: bluefish_walk(ctx,elapsed,1); break;
    default: bluefish_walk_none(ctx,elapsed); break;
  }
  
  // Fish's motion.
  bluefish_move_fish(ctx,elapsed);
  
  // Termination.
  CTX->outcome=bluefish_check_termination(ctx);
  if (CTX->outcome!=-2) {
    CTX->cooldown=END_COOLDOWN;
    if (CTX->outcome>0) {
      CTX->dotframe=3;
      CTX->fishx=CTX->dotx;
      CTX->fishy=GROUNDY-14.0;
      CTX->fishtileid=0x1c;
      if (CTX->dotxform) {
        CTX->fishxform=EGG_XFORM_XREV;
        CTX->fishx-=19.0;
      } else {
        CTX->fishxform=0;
        CTX->fishx+=19.0;
      }
    } else {
      CTX->dotframe=2;
    }
  }
}

/* Render.
 */
 
static void _bluefish_render(void *ctx) {

  // Sky, earth, and horizon. Then everything comes off RID_image_battle_fishing.
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,SKY_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,GROUND_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_fishing);
  
  // Dot.
  int dotdstx=(int)CTX->dotx-24;
  int dotdsty=GROUNDY-47;
  int dotsrcx=48*CTX->dotframe;
  int dotsrcy=64;
  graf_decal_xform(&g.graf,dotdstx,dotdsty,dotsrcx,dotsrcy,48,48,CTX->dotxform);
  
  // Fish.
  int fishdstx=(int)CTX->fishx;
  int fishdsty=(int)CTX->fishy;
  graf_tile(&g.graf,fishdstx,fishdsty,CTX->fishtileid,CTX->fishxform);
  
  // Animated row of water at the bottom.
  uint8_t watertileid=0x3a;
  switch (CTX->wanimframe) {
    case 1: watertileid+=1; break;
    case 2: watertileid+=2; break;
    case 3: watertileid+=3; break;
    case 4: watertileid+=2; break;
    case 5: watertileid+=1; break;
  }
  int waterx=NS_sys_tilesize>>1;
  int watery=FBH-(NS_sys_tilesize>>1);
  for (;waterx<FBW;waterx+=NS_sys_tilesize) graf_tile(&g.graf,waterx,watery,watertileid,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_bluefish={
  .name="bluefish",
  .strix_name=0,
  .no_article=0,
  .no_contest=0,
  .supported_players=(1<<NS_players_man_cpu),
  .del=_bluefish_del,
  .init=_bluefish_init,
  .update=_bluefish_update,
  .render=_bluefish_render,
};

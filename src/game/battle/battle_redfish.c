/* battle_redfish.c
 * At the crest of his flight, he stops and produces two decoys.
 */

#include "game/bellacopia.h"

#define GROUNDY 160 /* >16 off the framebuffer's bottom. */
#define END_COOLDOWN 1.5
#define SKY_COLOR 0x5e9fc7ff
#define GROUND_COLOR 0x126e29ff
#define FLIGHT_TIME 3.000 /* How long for the fish to go from bottom to top and back. */
#define FISHDY_INITIAL -300.0
#define FISHDY_ACCEL 250.0

struct battle_redfish {
  struct battle hdr;
  double wanimclock;
  int wanimframe;
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
  double crestclock;
  // Decoys:
  double dax,day,dadx,dady;
  double dbx,dby,dbdx,dbdy;
  uint8_t datileid,dbtileid;
  uint8_t daxform,dbxform;
};

#define BATTLE ((struct battle_redfish*)battle)

/* Delete.
 */
 
static void _redfish_del(struct battle *battle) {
}

/* New.
 */

static int _redfish_init(struct battle *battle) {
  
  BATTLE->dotx=FBW>>1;
  
  /* Choose fish position and direction.
   * Initial position is anywhere along the framebuffer, at the bottom.
   * Horizontal velocity will be constant. So pick one that yields a landing position also within the framebuffer's width.
   */
  BATTLE->fishy=FBH;
  BATTLE->fishx=rand()%FBW;
  double targetx=rand()%FBW; // Anywhere is fine, even exactly the same as (fishx).
  BATTLE->fishdx=(targetx-BATTLE->fishx)/FLIGHT_TIME;
  BATTLE->fishtileid=0x1a;
  BATTLE->fishxform=(targetx<BATTLE->fishx)?EGG_XFORM_XREV:0;
  BATTLE->fishdy=FISHDY_INITIAL;
  
  return 0;
}

/* Dot's motion.
 */
 
static void redfish_walk(struct battle *battle,double elapsed,int d) {
  if ((BATTLE->dotanimclock-=elapsed)<=0.0) {
    BATTLE->dotanimclock+=0.200;
    if (++(BATTLE->dotframe)>=2) BATTLE->dotframe=0;
  }
  if (d<0) BATTLE->dotxform=EGG_XFORM_XREV;
  else BATTLE->dotxform=0;
  double speed=80.0; // px/s
  BATTLE->dotx+=speed*elapsed*d;
  if (BATTLE->dotx<0.0) BATTLE->dotx=0.0;
  else if (BATTLE->dotx>FBW) BATTLE->dotx=FBW;
}

static void redfish_walk_none(struct battle *battle,double elapsed) {
  BATTLE->dotframe=0;
  BATTLE->dotanimclock=0.0;
}

/* Generate decoys, and shake up the fish's horizontal velocity.
 */
 
static void redfish_produce_decoys(struct battle *battle) {
  BATTLE->dax=BATTLE->dbx=BATTLE->fishx;
  BATTLE->day=BATTLE->dby=BATTLE->fishy;
  BATTLE->datileid=0x2b+rand()%5;
  BATTLE->dbtileid=0x2b+rand()%5;
  BATTLE->daxform=rand()&7;
  BATTLE->dbxform=rand()&7;
  BATTLE->dady=BATTLE->dbdy=BATTLE->fishdy;
  
  // The only important thing with dx is which slot the fish goes in.
  // (a) will always go left of (b), doesn't matter.
  // And decoys don't care about direction re xform, anything goes.
  double dxm=BATTLE->fishdx;
  double dxl=dxm-60.0;
  double dxr=dxm+60.0;
  switch (rand()%3) {
    case 0: {
        BATTLE->fishdx=dxl;
        BATTLE->dadx=dxm;
        BATTLE->dbdx=dxr;
      } break;
    case 1: {
        BATTLE->dadx=dxl;
        BATTLE->fishdx=dxm;
        BATTLE->dbdx=dxr;
      } break;
    case 2: {
        BATTLE->dadx=dxl;
        BATTLE->dbdx=dxm;
        BATTLE->fishdx=dxr;
      } break;
  }
  if (BATTLE->fishdx<0.0) BATTLE->fishxform=EGG_XFORM_SWAP;
  else BATTLE->fishxform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
}

/* Move the fish.
 * This does run after termination.
 */
 
static void redfish_move_fish(struct battle *battle,double elapsed) {

  // Animate if not terminated.
  if (battle->outcome==-2) {
    if ((BATTLE->fishanimclock-=elapsed)<=0.0) {
      BATTLE->fishanimclock+=0.200;
      if (BATTLE->fishtileid==0x1a) BATTLE->fishtileid=0x2a;
      else BATTLE->fishtileid=0x1a;
    }
  }
  
  // At the crest, we delay a little before producing decoys and falling again.
  if (BATTLE->crestclock>0.0) {
    if ((BATTLE->crestclock-=elapsed)<=0.0) {
      redfish_produce_decoys(battle);
    }
    return;
  }
  
  // Move fish, if we're not terminated yet.
  if (battle->outcome==-2) {
    double pvdy=BATTLE->fishdy;
    BATTLE->fishdy+=FISHDY_ACCEL*elapsed;
    if ((pvdy<=0.0)&&(BATTLE->fishdy>0.0)) { // Crested.
      BATTLE->crestclock=0.500;
      if (BATTLE->fishxform==0) BATTLE->fishxform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
      else if (BATTLE->fishxform==EGG_XFORM_XREV) BATTLE->fishxform=EGG_XFORM_SWAP;
    }
    BATTLE->fishy+=BATTLE->fishdy*elapsed;
    BATTLE->fishx+=BATTLE->fishdx*elapsed;
  }
  
  // Move decoys if present.
  if (BATTLE->datileid) {
    BATTLE->dady+=FISHDY_ACCEL*elapsed;
    BATTLE->dbdy+=FISHDY_ACCEL*elapsed;
    BATTLE->dax+=BATTLE->dadx*elapsed;
    BATTLE->day+=BATTLE->dady*elapsed;
    BATTLE->dbx+=BATTLE->dbdx*elapsed;
    BATTLE->dby+=BATTLE->dbdy*elapsed;
  }
}

/* Check both terminal conditions.
 * Return -2 if undetermined, -1 if the fish escaped, or 1 if Dot caught it.
 * In theory, 0 if tied, but we don't do ties.
 */
 
static int redfish_check_termination(struct battle *battle) {
  if (BATTLE->fishy>FBH) return -1; // Fish is back in the sea.
  if (BATTLE->fishdy<=0.0) return -2; // Still rising.
  if ((BATTLE->fishy>=GROUNDY-32.0)&&(BATTLE->fishy<=GROUNDY-8.0)) { // Catchy vertical range...
    double dx=BATTLE->fishx-BATTLE->dotx;
    if (dx<0.0) dx=-dx;
    if (dx<=25.0) return 1; // Caught!
  }
  return -2;
}

/* Update.
 */
 
static void _redfish_update(struct battle *battle,double elapsed) {
  
  // Animate water.
  if ((BATTLE->wanimclock-=elapsed)<=0.0) {
    BATTLE->wanimclock+=0.150;
    if (++(BATTLE->wanimframe)>=6) BATTLE->wanimframe=0; // 4 frames, pingponging
  }
  
  // Fish's motion. Continues after completion, for the decoys.
  redfish_move_fish(battle,elapsed);
  
  // Finished? Tick the cooldown, but no further model activity.
  if (battle->outcome!=-2) return;
  
  // Dot's motion.
  switch (g.input[0]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: redfish_walk(battle,elapsed,-1); break;
    case EGG_BTN_RIGHT: redfish_walk(battle,elapsed,1); break;
    default: redfish_walk_none(battle,elapsed); break;
  }
  
  // Termination.
  battle->outcome=redfish_check_termination(battle);
  if (battle->outcome!=-2) {
    BATTLE->cooldown=END_COOLDOWN;
    if (battle->outcome>0) {
      BATTLE->dotframe=3;
      BATTLE->fishx=BATTLE->dotx;
      BATTLE->fishy=GROUNDY-14.0;
      BATTLE->fishtileid=0x1d;
      if (BATTLE->dotxform) {
        BATTLE->fishxform=EGG_XFORM_XREV;
        BATTLE->fishx-=19.0;
      } else {
        BATTLE->fishxform=0;
        BATTLE->fishx+=19.0;
      }
    } else {
      BATTLE->dotframe=2;
    }
  }
}

/* Render.
 */
 
static void _redfish_render(struct battle *battle) {

  // Sky, earth, and horizon. Then everything comes off RID_image_battle_fishing.
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,SKY_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,GROUND_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_fishing);
  
  // Dot.
  int dotdstx=(int)BATTLE->dotx-24;
  int dotdsty=GROUNDY-47;
  int dotsrcx=48*BATTLE->dotframe;
  int dotsrcy=64;
  graf_decal_xform(&g.graf,dotdstx,dotdsty,dotsrcx,dotsrcy,48,48,BATTLE->dotxform);
  
  // Fish.
  int fishdstx=(int)BATTLE->fishx;
  int fishdsty=(int)BATTLE->fishy;
  graf_tile(&g.graf,fishdstx,fishdsty,BATTLE->fishtileid,BATTLE->fishxform);
  
  // Decoys.
  if (BATTLE->datileid) {
    graf_tile(&g.graf,BATTLE->dax,BATTLE->day,BATTLE->datileid,BATTLE->daxform);
  }
  if (BATTLE->dbtileid) {
    graf_tile(&g.graf,BATTLE->dbx,BATTLE->dby,BATTLE->dbtileid,BATTLE->dbxform);
  }
  
  // Animated row of water at the bottom.
  uint8_t watertileid=0x3a;
  switch (BATTLE->wanimframe) {
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
 
const struct battle_type battle_type_redfish={
  .name="redfish",
  .objlen=sizeof(struct battle_redfish),
  .strix_name=0,
  .no_article=0,
  .no_contest=0,
  .support_pvp=0,
  .support_cvc=0,
  .update_during_report=1,
  .del=_redfish_del,
  .init=_redfish_init,
  .update=_redfish_update,
  .render=_redfish_render,
};

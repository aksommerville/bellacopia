/* battle_seamonster.c
 * Not a real battle; just a cutscene showing Dot getting eaten by the sea monster.
 */

#include "game/bellacopia.h"

#define GROUNDY 160 /* >16 off the framebuffer's bottom. */
#define SKY_COLOR 0x5e9fc7ff
#define GROUND_COLOR 0x126e29ff
#define TENTACLE_COUNT 5
#define MONSTER_SPEED 60.0 /* px/s. */
#define MONSTER_Y_TOP 100 /* >=FBH-80 */
#define MONSTER_Y_BOTTOM 250

struct battle_seamonster {
  struct battle hdr;
  double wanimclock;
  int wanimframe;
  
  int dot_eaten;
  double monstery;
  double monsterdy;
  struct tentacle {
    int x;
    double animclock;
    int animframe;
  } tentaclev[TENTACLE_COUNT];
};

#define BATTLE ((struct battle_seamonster*)battle)

/* Delete.
 */
 
static void _seamonster_del(struct battle *battle) {
}

/* New.
 */
 
static int _seamonster_init(struct battle *battle) {
  BATTLE->monstery=MONSTER_Y_BOTTOM;
  BATTLE->monsterdy=-1.0;
  
  struct tentacle *tentacle=BATTLE->tentaclev;
  int i=0;
  for (;i<TENTACLE_COUNT;i++,tentacle++) {
    tentacle->x=(FBW>>1)-80+(i+1)*26;
    tentacle->animclock=((rand()&0xffff)*0.500)/65535.0;
    tentacle->animframe=rand()%2;
  }
  return 0;
}

/* Update.
 */
 
static void _seamonster_update(struct battle *battle,double elapsed) {
  
  // Animate water.
  if ((BATTLE->wanimclock-=elapsed)<=0.0) {
    BATTLE->wanimclock+=0.150;
    if (++(BATTLE->wanimframe)>=6) BATTLE->wanimframe=0; // 4 frames, pingponging
  }
  
  if (battle->outcome>-2) return;
  
  struct tentacle *tentacle=BATTLE->tentaclev;
  int i=TENTACLE_COUNT;
  for (;i-->0;tentacle++) {
    if ((tentacle->animclock-=elapsed)<=0.0) {
      tentacle->animclock+=0.500;
      if (++(tentacle->animframe)>=2) tentacle->animframe=0;
    }
  }
  
  BATTLE->monstery+=BATTLE->monsterdy*MONSTER_SPEED*elapsed;
  if (BATTLE->monstery<=MONSTER_Y_TOP) {
    BATTLE->monstery=MONSTER_Y_TOP;
    if (BATTLE->monsterdy<0.0) {
      BATTLE->monsterdy=1.0;
      BATTLE->dot_eaten=1;
    }
  } else if (BATTLE->monstery>MONSTER_Y_BOTTOM) {
    battle->outcome=-1;
  }
  
  //XXX AUX2 to end the battle and unset the controlling flag, for testing purposes.
  if (g.input[0]&EGG_BTN_AUX2) {
    fprintf(stderr,"*** AUX2: Ending seamonster cutscene and unsetting its flag. ***\n");
    store_set_fld(NS_fld_caught_seamonster,0);
    battle->outcome=1;
  }
}

/* Render.
 */
 
static void _seamonster_render(struct battle *battle) {

  // Sky, earth, and horizon. Then everything comes off RID_image_battle_fishing.
  // Using the same colors as regular fishing battles. Maybe we want something more Labyrinth-appropriate?
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,SKY_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,GROUND_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_fishing);
  
  // Dot, always using the lose frame.
  if (!BATTLE->dot_eaten) {
    int dotdstx=120;
    int dotdsty=GROUNDY-47;
    int dotsrcx=96;
    int dotsrcy=64;
    graf_decal_xform(&g.graf,dotdstx,dotdsty,dotsrcx,dotsrcy,48,48,0);
  }
  
  // Tentacles.
  int smdsty=(int)BATTLE->monstery;
  int tentsrcx=64;
  int tentsrcy=176;
  int tentw=32;
  int tenth=80;
  int tenty=smdsty-60;
  struct tentacle *tentacle=BATTLE->tentaclev;
  int i=TENTACLE_COUNT;
  for (;i-->0;tentacle++) {
    uint8_t xform=0;
    if (tentacle->animframe==1) xform=EGG_XFORM_XREV;
    graf_decal_xform(&g.graf,tentacle->x-16,tenty,tentsrcx,tentsrcy,tentw,tenth,xform);
  }
  
  // Sea Monster.
  int smsrcx=96;
  int smsrcy=176;
  int smw=160;
  int smh=80;
  graf_decal(&g.graf,(FBW>>1)-(smw>>1),smdsty,smsrcx,smsrcy,smw,smh);
  
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
 
const struct battle_type battle_type_seamonster={
  .name="seamonster",
  .objlen=sizeof(struct battle_seamonster),
  .strix_name=20,
  .no_article=0,
  .no_contest=0,
  .support_pvp=0,
  .support_cvc=0,
  .update_during_report=1,
  .del=_seamonster_del,
  .init=_seamonster_init,
  .update=_seamonster_update,
  .render=_seamonster_render,
};

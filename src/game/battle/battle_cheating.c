/* battle_cheating.c
 * Asymmetric. Zero or one player only.
 * Three-card monte, but he's cheating, so you can only win by slapping him first.
 */

#include "game/bellacopia.h"

#define STAGE_WELCOME 1
#define STAGE_SHUFFLE 2
#define STAGE_QUERY   3
#define STAGE_REVEAL  4
#define STAGE_SLAP    5

struct battle_cheating {
  struct battle hdr;
  double difficulty;
  uint8_t ptileid;
  int stage;
  double stageclock; // Counts up.
  double slapx; // 0..1, hero's normalized position leading to the slap.
  double slapclock; // Counts down while the slap takes place.
  int cpuquerystep;
  double cpuslaptime; // In STAGE_SHUFFLE. May be unreasonably high, if you want her to lose.
  
  struct cup {
    double x; // -1..0..1
    double y; // 0..1 = cover..expose
    double targetx; // -1,0,1
  } cupv[3];
  int acornp; // 0..2
  int cursorp; // 0..2
};

#define BATTLE ((struct battle_cheating*)battle)

/* Delete.
 */
 
static void _cheating_del(struct battle *battle) {
}

/* Assign (targetx) randomly to each cup.
 */
 
static void cheating_random_targets(struct battle *battle) {
  // There's only 6 possible arrangements (3!), so might as well just enumerate them.
  switch (rand()%6) {
    case 0: {
        BATTLE->cupv[0].targetx=-1.0;
        BATTLE->cupv[1].targetx= 0.0;
        BATTLE->cupv[2].targetx= 1.0;
      } break;
    case 1: {
        BATTLE->cupv[0].targetx=-1.0;
        BATTLE->cupv[1].targetx= 1.0;
        BATTLE->cupv[2].targetx= 0.0;
      } break;
    case 2: {
        BATTLE->cupv[0].targetx= 0.0;
        BATTLE->cupv[1].targetx=-1.0;
        BATTLE->cupv[2].targetx= 1.0;
      } break;
    case 3: {
        BATTLE->cupv[0].targetx= 0.0;
        BATTLE->cupv[1].targetx= 1.0;
        BATTLE->cupv[2].targetx=-1.0;
      } break;
    case 4: {
        BATTLE->cupv[0].targetx= 1.0;
        BATTLE->cupv[1].targetx=-1.0;
        BATTLE->cupv[2].targetx= 0.0;
      } break;
    case 5: {
        BATTLE->cupv[0].targetx= 1.0;
        BATTLE->cupv[1].targetx= 0.0;
        BATTLE->cupv[2].targetx=-1.0;
      } break;
  }
}

/* New.
 */
 
static int _cheating_init(struct battle *battle) {
  if (battle->args.rctl) {
    fprintf(stderr,"Cheating contest is one-player only (or zero).\n");
    return -1;
  }
  BATTLE->difficulty=battle_scalar_difficulty(battle);
  if (battle->args.lface==NS_face_princess) {
    BATTLE->ptileid=0x25;
  } else { // Anything else, make it Dot.
    BATTLE->ptileid=0x05;
  }
  BATTLE->cursorp=1;
  
  BATTLE->stage=STAGE_WELCOME;
  BATTLE->stageclock=0.0;
  
  /* All cups start exposed, at their natural positions.
   */
  BATTLE->cupv[0].x=-1.0;
  BATTLE->cupv[0].y=1.0;
  BATTLE->cupv[1].x=0.0;
  BATTLE->cupv[1].y=1.0;
  BATTLE->cupv[2].x=1.0;
  BATTLE->cupv[2].y=1.0;
  cheating_random_targets(battle);
  
  /* Acorn goes under a random cup, doesn't matter which.
   */
  BATTLE->acornp=rand()%3;
  
  /* Under CPU control, decide timing of the slap.
   * This is within the SHUFFLE stage.
   * If greater than 5 or so, she'll lose.
   * (difficulty) is the odds that the Princess will lose. It's usually 0xa0 = 0.627.
   */
  if (!battle->args.lctl) {
    double n=(rand()&0xffff)/65535.0;
    if (n<BATTLE->difficulty) { // Lose
      BATTLE->cpuslaptime=10.0;
    } else { // Win. Pick a tasteful delay.
      n=(rand()&0xffff)/65535.0;
      BATTLE->cpuslaptime=3.0+n*1.500;
    }
  }
  
  return 0;
}

/* Update in WELCOME and SHUFFLE stage, is she slapping?
 */
 
static void cheating_slappable(struct battle *battle) {
  if (battle->args.lctl) {
    if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
      BATTLE->stage=STAGE_SLAP;
      BATTLE->stageclock=0.0;
    }
  } else {
    if (BATTLE->stageclock>BATTLE->cpuslaptime) {
      BATTLE->stage=STAGE_SLAP;
      BATTLE->stageclock=0.0;
    }
  }
}

/* Update, WELCOME stage.
 */
 
static void cheating_update_WELCOME(struct battle *battle,double elapsed) {
  if (BATTLE->stageclock<2.0) {
    // Hold still, let her see the acorn.
  } else if (BATTLE->stageclock<3.0) {
    // Cups descend.
    double y=(3.0-BATTLE->stageclock)/1.0;
    struct cup *cup=BATTLE->cupv;
    int i=3;
    for (;i-->0;cup++) {
      cup->y=y;
    }
  } else {
    // OK let's go!
    BATTLE->stage=STAGE_SHUFFLE;
    BATTLE->stageclock=0.0;
  }
}

/* Update, SHUFFLE stage.
 */
 
static void cheating_update_SHUFFLE(struct battle *battle,double elapsed) {
  double dx=3.0*elapsed; // Unit is the distance between two cups at rest.
  int ready=1,i=3;
  struct cup *cup=BATTLE->cupv;
  for (;i-->0;cup++) {
    if (cup->x<cup->targetx-dx) {
      cup->x+=dx;
      ready=0;
    } else if (cup->x>cup->targetx+dx) {
      cup->x-=dx;
      ready=0;
    } else {
      cup->x=cup->targetx;
    }
  }
  if (ready) {
    if (BATTLE->stageclock>=5.0) {
      BATTLE->stage=STAGE_QUERY;
      BATTLE->stageclock=0.0;
    } else {
      cheating_random_targets(battle);
    }
  }
}

/* Finish QUERY stage. You always lose.
 */
 
static void cheating_begin_reveal(struct battle *battle) {
  bm_sound(RID_sound_reject);
  BATTLE->stage=STAGE_REVEAL;
  BATTLE->stageclock=0.0;
  BATTLE->acornp=-1; // ...now you don't
    
  /* The cups' indices are not their horizontal order.
   * To keep things simple for further stages, replace (cursorp) such that it now refers to a cup index, rather than a horizontal position.
   */
  double xlo=-2.0,xhi=2.0;
  switch (BATTLE->cursorp) {
    case 0: xhi=-0.5; break;
    case 1: xlo=-0.5; xhi=0.5; break;
    case 2: xlo=0.5; break;
  }
  struct cup *cup=BATTLE->cupv;
  int i=0;
  for (;i<3;i++,cup++) {
    if ((cup->x>xlo)&&(cup->x<xhi)) {
      BATTLE->cursorp=i;
      break;
    }
  }
}

/* Update, QUERY stage for robots.
 */
 
static void cheating_update_QUERY_cpu(struct battle *battle,double elapsed) {
  int nstep=(int)(BATTLE->stageclock*4.0);
  if (nstep==BATTLE->cpuquerystep) return;
  switch (BATTLE->cpuquerystep=nstep) {
    case 1: {
        bm_sound(RID_sound_uimotion);
        BATTLE->cursorp=0;
      } break;
    case 2: {
        bm_sound(RID_sound_uimotion);
        BATTLE->cursorp=1;
      } break;
    case 3: {
        bm_sound(RID_sound_uimotion);
        BATTLE->cursorp=2;
      } break;
    case 4: {
        cheating_begin_reveal(battle);
      } break;
  }
}

/* Update, QUERY stage for humans.
 */
 
static void cheating_update_QUERY_man(struct battle *battle,double elapsed) {
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)&&(BATTLE->cursorp>0)) {
    bm_sound(RID_sound_uimotion);
    BATTLE->cursorp--;
  } else if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)&&(BATTLE->cursorp<2)) {
    bm_sound(RID_sound_uimotion);
    BATTLE->cursorp++;
  } else if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    cheating_begin_reveal(battle);
  }
}

/* Update, REVEAL stage.
 */
 
static void cheating_update_REVEAL(struct battle *battle,double elapsed) {
  if (BATTLE->stageclock<1.0) {
    // Raise the focussed cup.
    struct cup *cup=BATTLE->cupv+BATTLE->cursorp;
    cup->y=(BATTLE->stageclock-0.0)/1.0;
  } else if (BATTLE->stageclock<2.0) {
    // Pause for a beat.
  } else if (BATTLE->stageclock<3.0) {
    // Raise the other cups.
    double y=(BATTLE->stageclock-2.0)/1.0;
    struct cup *cup=BATTLE->cupv;
    int i=0;
    for (;i<3;i++,cup++) {
      if (i!=BATTLE->cursorp) cup->y=y;
    }
  } else if (BATTLE->stageclock<4.0) {
    // Pause another beat.
  } else {
    // Game over, you lose.
    battle->outcome=-1;
  }
}

/* Update, SLAP stage.
 */
 
static void cheating_update_SLAP(struct battle *battle,double elapsed) {

  // If we started the slap, tick it back down.
  if (BATTLE->slapclock>0.0) {
    if ((BATTLE->slapclock-=elapsed)<=0.0) {
      battle->outcome=1;
    }
    return;
  }
  
  // Did we reach the slap zone?
  if (BATTLE->stageclock>=0.500) {
    BATTLE->slapx=1.0;
    BATTLE->slapclock=0.500;
    bm_sound(RID_sound_whack);
    return;
  }
  
  // Approach the slap zone.
  BATTLE->slapx=BATTLE->stageclock/0.500;
}

/* Update.
 */
 
static void _cheating_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  BATTLE->stageclock+=elapsed;
  switch (BATTLE->stage) {
    case STAGE_WELCOME: {
        cheating_update_WELCOME(battle,elapsed);
        cheating_slappable(battle);
      } break;
    case STAGE_SHUFFLE: {
        cheating_update_SHUFFLE(battle,elapsed);
        cheating_slappable(battle);
      } break;
    case STAGE_QUERY: {
        if (battle->args.lctl) {
          cheating_update_QUERY_man(battle,elapsed);
        } else {
          cheating_update_QUERY_cpu(battle,elapsed);
        }
      } break;
    case STAGE_REVEAL: {
        cheating_update_REVEAL(battle,elapsed);
      } break;
    case STAGE_SLAP: {
        cheating_update_SLAP(battle,elapsed);
      } break;
  }
}

/* Color for slap starbust.
 */
 
static uint32_t slap_starbust_color(double n) {
  if (n<0.0) n=0.0; else if (n>1.0) n=1.0;
  
  int a=0xff;
       if (n<0.125) a=(int)(n*8.0);
  else if (n>0.875) a=(int)((1.0-n)*8.0);
  if (a<0) a=0;
  else if (a>0xff) a=0xff;
  
  int frame=(int)(n*10.0);
  switch (frame) {
    case 0: return 0xff000000|a;
    case 1: return 0xffff0000|a;
    case 2: return 0x00ff0000|a;
    case 3: return 0xff800000|a;
    case 4: return 0xff000000|a;
    case 5: return 0xffff0000|a;
    case 6: return 0x00ff0000|a;
    case 7: return 0xff800000|a;
    default: return 0xff000000|a;
  }
}

/* Render.
 */
 
static void _cheating_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_fractia);
  
  /* Start with some measurements.
   * Table goes in the middle, player on the left, and hustler on the right.
   */
  int tbx=FBW>>1;
  int tby=FBH>>1;
  int px=tbx-NS_sys_tilesize-(NS_sys_tilesize>>1);
  int hx=tbx+NS_sys_tilesize+(NS_sys_tilesize>>1);
  double cupxmax=NS_sys_tilesize*0.750;
  double cupymax=NS_sys_tilesize*0.500;
  
  /* Fancy starbust behind the hustler when he's slapped.
   */
  if (BATTLE->slapclock>0.0) {
    uint32_t color=slap_starbust_color(1.0-BATTLE->slapclock/0.500);
    const int ht=NS_sys_tilesize>>1;
    graf_fancy(&g.graf,hx-ht,tby-NS_sys_tilesize-ht,0x29,0,0,NS_sys_tilesize,0,color);
    graf_fancy(&g.graf,hx+ht,tby-NS_sys_tilesize-ht,0x2a,0,0,NS_sys_tilesize,0,color);
    graf_fancy(&g.graf,hx-ht,tby-NS_sys_tilesize+ht,0x39,0,0,NS_sys_tilesize,0,color);
    graf_fancy(&g.graf,hx+ht,tby-NS_sys_tilesize+ht,0x3a,0,0,NS_sys_tilesize,0,color);
  }
  
  /* Hero on the left.
   */
  uint8_t ptileid=BATTLE->ptileid;
  if (BATTLE->stage==STAGE_REVEAL) {
    ptileid+=1; // oh no
  } else if (BATTLE->stage==STAGE_SLAP) {
    px+=(int)(BATTLE->slapx*26.0);
    if (BATTLE->slapclock>0.200) {
      ptileid+=2;
      graf_tile(&g.graf,px+NS_sys_tilesize,tby-NS_sys_tilesize,ptileid+0x01,0);
      graf_tile(&g.graf,px+NS_sys_tilesize,tby,ptileid+0x11,0);
    }
  }
  graf_tile(&g.graf,px,tby-NS_sys_tilesize,ptileid,0);
  graf_tile(&g.graf,px,tby,ptileid+0x10,0);
  
  /* Hustler on the right.
   */
  uint8_t htileid=0x09;
  if (BATTLE->slapclock>0.0) htileid=0x0a;
  graf_tile(&g.graf,hx,tby-NS_sys_tilesize,htileid,0);
  graf_tile(&g.graf,hx,tby,htileid+0x10,0);
  
  /* Table in the middle.
   * (tby) is the center of the tile, image is oriented low in its tiles.
   * (tbx) is the junction of the two table tiles.
   */
  graf_tile(&g.graf,tbx-(NS_sys_tilesize>>1),tby,0x45,0);
  graf_tile(&g.graf,tbx+(NS_sys_tilesize>>1),tby,0x46,0);
  
  /* Acorn, if it's exposed.
   */
  if ((BATTLE->acornp>=0)&&(BATTLE->acornp<3)) {
    struct cup *cup=BATTLE->cupv+BATTLE->acornp;
    if (cup->y>0.0) {
      int x=tbx+(int)(cup->x*cupxmax);
      graf_tile(&g.graf,x,tby,0x48,0);
    }
  }
  
  /* Cups.
   */
  struct cup *cup=BATTLE->cupv;
  int i=3;
  for (;i-->0;cup++) {
    int x=tbx+(int)(cup->x*cupxmax);
    int y=tby-(int)(cup->y*cupymax);
    graf_tile(&g.graf,x,y,0x47,0);
  }
  
  /* Cursor, in QUERY stage.
   */
  if (BATTLE->stage==STAGE_QUERY) {
    int n=BATTLE->cursorp-1;
    int x=tbx+(int)(n*cupxmax);
    int y=tby-NS_sys_tilesize;
    graf_tile(&g.graf,x,y,BATTLE->ptileid-1,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_cheating={
  .name="cheating",
  .objlen=sizeof(struct battle_cheating),
  .strix_name=168,
  .no_article=0,
  .no_contest=0,
  .support_pvp=0,
  .support_cvc=1,
  .del=_cheating_del,
  .init=_cheating_init,
  .update=_cheating_update,
  .render=_cheating_render,
};

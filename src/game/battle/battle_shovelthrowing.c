/* battle_shovelthrowing.c
 * Not a regular battle. Happens when you dig too much without finding something.
 * The skeleton grabs your shovel, throws it, and tucks himself back into his hole.
 * If you fail to catch the shovel, you lose like usual. No prize if you win.
 * Intent is to raise the cost of guessing when you dig.
 */

#include "game/bellacopia.h"

#define GROUNDY 150
#define HOLEX 40 /* Horizontal center of the hole. */

#define STAGE_OOPS  1 /* Dot holds the shovel, skeleton glares. */
#define STAGE_YOINK 2 /* Skeleton grabs the shovel. */
#define STAGE_PLAY  3 /* Shovel in flight, Dot is interactive, skeleton returns to his hole. */
#define STAGE_DONE  4

struct battle_shovelthrowing {
  struct battle hdr;
  int stage;
  double stageclock; // Counts down.
  double skelx; // Horizontal center of skeleton at his initial position. Does not change as he animates.
  double skelclock; // For the STAGE_PLAY animation. Counts upward, only in STAGE_PLAY.
  double dotx; // Her horizontal center. Sprite is 48x48, and vertical is fixed.
  uint8_t dotxform; // Natural orientation is right.
  int dotframe;
  double dotclock;
  double shovelx,shovely;
  double shoveldx,shoveldy;
};

#define BATTLE ((struct battle_shovelthrowing*)battle)

/* Delete.
 */
 
static void _shovelthrowing_del(struct battle *battle) {
}

/* New.
 */
 
static int _shovelthrowing_init(struct battle *battle) {
  BATTLE->stage=STAGE_OOPS;
  BATTLE->stageclock=1.000;
  BATTLE->skelx=HOLEX+30.0;
  BATTLE->dotx=HOLEX+60.0;
  BATTLE->dotxform=EGG_XFORM_XREV;
  BATTLE->shovelx=BATTLE->skelx+20.0;
  BATTLE->shovely=GROUNDY-40.0;
  double n=(rand()&0xffff)/65535.0; // Speeds are tuned such that 0.890 is catchable and 0.900 is not.
  BATTLE->shoveldx=(1.0-n)*5.0+n*100.0;
  BATTLE->shoveldy=-200.0;
  return 0;
}

/* Update.
 */
 
static void _shovelthrowing_update(struct battle *battle,double elapsed) {
  
  // Stage timeout?
  if ((BATTLE->stageclock-=elapsed)<=0.0) {
    switch (BATTLE->stage) {
      case STAGE_OOPS: {
          BATTLE->stage=STAGE_YOINK;
          BATTLE->stageclock=1.000;
        } break;
      case STAGE_YOINK: { // Throw the shovel and begin play.
          BATTLE->stage=STAGE_PLAY;
          BATTLE->stageclock=999.999;
          BATTLE->dotxform=0; // She faces left initially, but turn right automatically to follow the shovel.
        } break;
      default: { // PLAY and DONE shouldn't time out.
          BATTLE->stageclock=999.999;
        }
    }
  }
  
  // Update per stage.
  switch (BATTLE->stage) {
    case STAGE_PLAY: {
        BATTLE->skelclock+=elapsed;
        
        /* Move shovel.
         * If it crosses the horizon, you lose.
         */
        BATTLE->shovelx+=BATTLE->shoveldx*elapsed;
        BATTLE->shovely+=BATTLE->shoveldy*elapsed;
        if (BATTLE->shovely>=GROUNDY) {
          BATTLE->shovely=GROUNDY;
          BATTLE->stage=STAGE_DONE;
          BATTLE->stageclock=999.999;
          battle->outcome=-1;
          return;
        }
        
        /* Update shovel's gravity.
         * I don't think we need a terminal velocity.
         */
        BATTLE->shoveldy+=200.0*elapsed;
        
        /* Move Dot.
         */
        int dx=0;
        switch (g.input[0]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
          case EGG_BTN_LEFT: dx=-1; BATTLE->dotxform=EGG_XFORM_XREV; break;
          case EGG_BTN_RIGHT: dx=1; BATTLE->dotxform=0; break;
        }
        if (dx) {
          const double speed=78.0; // There should be some throws she can't reach. Say above a norm of 0.900? (see _init).
          BATTLE->dotx+=speed*elapsed*dx;
          if (BATTLE->dotx<0.0) BATTLE->dotx=0.0;
          else if (BATTLE->dotx>FBW) BATTLE->dotx=FBW;
          if ((BATTLE->dotclock-=elapsed)<=0.0) {
            BATTLE->dotclock+=0.200;
            if (++(BATTLE->dotframe)>=2) BATTLE->dotframe=0;
          }
        } else {
          BATTLE->dotframe=0;
          BATTLE->dotclock=0.0;
        }
        
        /* Catch the shovel?
         */
        if ((BATTLE->shoveldy>0.0)&&(BATTLE->shovely>GROUNDY-30.0)) {
          const double radius=15.0;
          double sdx=BATTLE->shovelx-BATTLE->dotx;
          if ((sdx>-radius)&&(sdx<radius)) {
            bm_sound(RID_sound_collect);
            battle->outcome=1;
            BATTLE->stage=STAGE_DONE;
            BATTLE->stageclock=999.999;
            return;
          }
        }
        
      } break;
    case STAGE_DONE: {
        BATTLE->skelclock+=elapsed;
      } break;
  }
}

/* Render.
 */
 
static void _shovelthrowing_render(struct battle *battle) {

  // Sky and horizon. Don't draw the green earth yet.
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_skeleton);
  
  // Back of the hole.
  graf_decal(&g.graf,HOLEX-24,GROUNDY-9,80,96,48,16);
  
  // Dot.
  int dotdstx=(int)BATTLE->dotx-24;
  int dotsrcx=0;
  switch (BATTLE->stage) {
    case STAGE_OOPS: dotsrcx=3*48; break;
    case STAGE_YOINK: dotsrcx=2*48; break;
    case STAGE_PLAY: dotsrcx=BATTLE->dotframe*48; break;
    case STAGE_DONE: {
        if (battle->outcome>0) dotsrcx=4*48;
        else dotsrcx=2*48;
      } break;
  }
  graf_decal_xform(&g.graf,dotdstx,GROUNDY-47,dotsrcx,0,48,48,BATTLE->dotxform);
  
  // Skeleton.
  int skeldstx=(int)BATTLE->skelx-24;
  int skeldsty=GROUNDY-47;
  int skelsrcx=0;
  uint8_t skelxform=0;
  int skelvisible=0;
  switch (BATTLE->stage) {
    case STAGE_OOPS: skelvisible=1; skelsrcx=0; break;
    case STAGE_YOINK: skelvisible=1; skelsrcx=48; break;
    case STAGE_PLAY:
    case STAGE_DONE: {
        // In STAGE_PLAY, the skeleton plays out a little animation. Orchestrated here.
        const double t_turn=0.500; // These times are absolute, ie they must increase.
        const double t_door=2.000;
        const double t_home=3.000;
        if (BATTLE->skelclock<t_turn) { // Arm raised from throwing.
          skelvisible=1;
          skelsrcx=96;
        } else if (BATTLE->skelclock<t_door) { // Walking back to the hole.
          skelvisible=1;
          skelsrcx=0;
          skelxform=EGG_XFORM_XREV;
          double t=(BATTLE->skelclock-t_turn)/(t_door-t_turn);
          skeldstx=(int)(HOLEX*t+BATTLE->skelx*(1.0-t))-24;
        } else if (BATTLE->skelclock<t_home) { // Descending.
          skelvisible=1;
          skelsrcx=0;
          skelxform=EGG_XFORM_XREV;
          skeldstx=HOLEX-24;
          double t=(BATTLE->skelclock-t_door)/(t_home-t_door);
          skeldsty+=(int)(t*48.0);
        } // Otherwise he's not visible.
      } break;
  }
  if (skelvisible) {
    graf_decal_xform(&g.graf,skeldstx,skeldsty,skelsrcx,48,48,48,skelxform);
  }
  
  // Now draw the green earth. It needs to occlude the skeleton.
  graf_set_input(&g.graf,0);
  graf_fill_rect(&g.graf,0,GROUNDY+1,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(&g.graf,RID_image_battle_skeleton);
  
  // Front of the hole.
  graf_decal(&g.graf,HOLEX-24,GROUNDY-9,32,96,48,16);
  
  // Shovel if in flight. Don't draw after a victory; Dot is drawn holding it.
  if ((battle->outcome<=0)&&(BATTLE->stage>=STAGE_PLAY)) {
    int shdstx=(int)BATTLE->shovelx-16;
    int shdsty=(int)BATTLE->shovely-8;
    graf_decal(&g.graf,shdstx,shdsty,0,96,32,16);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_shovelthrowing={
  .name="shovelthrowing",
  .objlen=sizeof(struct battle_shovelthrowing),
  .id=NS_battle_shovelthrowing,
  .strix_name=225,
  .no_article=0,
  .no_contest=0,
  .support_pvp=0,
  .support_cvc=0,
  .update_during_report=1,
  .del=_shovelthrowing_del,
  .init=_shovelthrowing_init,
  .update=_shovelthrowing_update,
  .render=_shovelthrowing_render,
};

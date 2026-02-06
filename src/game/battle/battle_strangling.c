#include "game/bellacopia.h"

#define END_COOLDOWN 1.0
#define SKY_COLOR 0x5e9fc7ff
#define GROUND_COLOR 0x126e29ff
#define GROUNDY 150
#define LABEL_SPEED 40.0 /* px/s */
#define CHANGE_TIME_EASY 5.0 /* Should be a little longer than needed to win; it doesn't change. */
#define CHANGE_TIME_HARD 2.0
#define CHANGE_TIME_VARIATION 1.0 /* Added to change time, randomly. */
#define STRANGLE_RATE 0.250 /* hz, inverse duration of the game if you have perfect strangling. */
#define STATE_HOLD_TIME 0.200 /* Flicker mitigation (with real gameplay consequences): After any state change, suppress others for so long. */

// CPU player parameters. I basically guess-and-checked these until 0x80 is easy and 0xff is hard but winnable.
// Further tweaking is recommended.
#define TAP_TIME_LO 0.250
#define TAP_TIME_HI 0.400
#define AWARE_TIME_LO 1.700
#define AWARE_TIME_HI 3.000

#define LABELID_HOLD 1
#define LABELID_TAP 2
#define LABEL_LIMIT 2

struct battle_strangling {
  struct battle hdr;
  double end_cooldown;
  double animclock;
  int animframe;
  double changetime;
  double changeclock;
  int btnid_hold; // EGG_BTN_(LEFT,RIGHT,UP,DOWN), exactly one bit.
  int btnid_tap; // EGG_BTN_(SOUTH,WEST), exactly one bit.
  int state; // 1=right attacking, 2=left attacking. Also the row in our source images.
  double stateclock;
  
  struct label {
    int labelid;
    int texid,x,y,w,h;
    double fx,fy,dx,dy;
  } labelv[LABEL_LIMIT];
  int labelc;
  
  struct player {
    int who; // My index in this list, ie 0 or 1.
    int human; // 0=cpu, otherwise the input index (1 or 2).
    int input,pvinput;
    double power; // Increases with each correct tap. Decreases with time. Drops to zero on incorrect input.
    double hp; // 0..1
    // Further fields only relevant to cpu players:
    int input_choice;
    double tapclock;
    double awareclock;
    double taptime; // constant after init
    double awaretime; // constant after init
  } playerv[2];
};

#define BATTLE ((struct battle_strangling*)battle)

/* Delete.
 */
 
static void _strangling_del(struct battle *battle) {
  struct label *label=BATTLE->labelv;
  int i=BATTLE->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
}

/* Add or replace a label.
 */
 
static struct label *strangling_label_by_labelid(struct battle *battle,int labelid) {
  struct label *label=BATTLE->labelv;
  int i=BATTLE->labelc;
  for (;i-->0;label++) {
    if (label->labelid==labelid) return label;
  }
  return 0;
}
 
static void strangling_set_label(struct battle *battle,int labelid,int strix) {
  struct label *label=strangling_label_by_labelid(battle,labelid);
  if (!label) {
    if (BATTLE->labelc>=LABEL_LIMIT) return;
    label=BATTLE->labelv+BATTLE->labelc++;
    memset(label,0,sizeof(struct label));
    label->labelid=labelid;
  }
  const char *src=0;
  int srcc=text_get_string(&src,RID_strings_battle,strix);
  egg_texture_del(label->texid);
  if (srcc>0) {
    label->texid=font_render_to_texture(0,g.font,src,srcc,FBW,font_get_line_height(g.font),0xffffffff);
    egg_texture_get_size(&label->w,&label->h,label->texid);
  } else {
    label->texid=0;
    label->w=label->h=0;
  }
}

/* Change requirements randomly.
 */
 
static void strangling_change_requirements(struct battle *battle) {
  int hold=rand()&3; // 0..3 = left,right,up,down
  int tap=rand()&1; // 0..1 = south,west
  int holdbase=21;
  int tapbase=33;
  if (battle->args.bias>0x80) {
    switch (rand()%3) {
      case 1: holdbase=25; break;
      case 2: holdbase=29; break;
    }
    if (rand()&1) tapbase=35;
  }
  strangling_set_label(battle,LABELID_HOLD,holdbase+hold);
  strangling_set_label(battle,LABELID_TAP,tapbase+tap);
  switch (hold) {
    case 0: BATTLE->btnid_hold=EGG_BTN_LEFT; break;
    case 1: BATTLE->btnid_hold=EGG_BTN_RIGHT; break;
    case 2: BATTLE->btnid_hold=EGG_BTN_UP; break;
    case 3: BATTLE->btnid_hold=EGG_BTN_DOWN; break;
  }
  switch (tap) {
    case 0: BATTLE->btnid_tap=EGG_BTN_SOUTH; break;
    case 1: BATTLE->btnid_tap=EGG_BTN_WEST; break;
  }
}

/* New.
 */
 
static int _strangling_init(struct battle *battle) {
  
  double diff=(double)battle->args.bias/255.0;
  double invdiff=1.0-diff;
  BATTLE->changetime=CHANGE_TIME_EASY*invdiff+CHANGE_TIME_HARD*diff;
  BATTLE->changeclock=BATTLE->changetime+((rand()&0xffff)*CHANGE_TIME_VARIATION)/65535.0;
  strangling_change_requirements(battle);
  
  // Init players. Visually, we don't support cpu-vs-cpu, no princess pictures, but technically it's ok.
  // But our difficulty is not a peer bias, so a cpu-vs-cpu battle would be random regardless of handicap.
  BATTLE->playerv[0].who=0;
  BATTLE->playerv[0].hp=1.0;
  BATTLE->playerv[1].who=1;
  BATTLE->playerv[1].hp=1.0;
  BATTLE->playerv[0].human=battle->args.lctl;
  BATTLE->playerv[1].human=battle->args.rctl;
  
  // Set (taptime,awaretime) in both players, tho only cpu players need it. Human players will safely ignore.
  double normhand=battle_scalar_difficulty(battle);
  double invhand=1.0-normhand;
  BATTLE->playerv[0].taptime=TAP_TIME_LO*invhand+TAP_TIME_HI*normhand;
  BATTLE->playerv[0].awaretime=AWARE_TIME_LO*invhand+AWARE_TIME_HI*normhand;
  BATTLE->playerv[1].taptime=TAP_TIME_LO*normhand+TAP_TIME_HI*invhand;
  BATTLE->playerv[1].awaretime=AWARE_TIME_LO*normhand+AWARE_TIME_HI*invhand;
  BATTLE->playerv[0].awareclock=BATTLE->playerv[0].awaretime*0.5;
  BATTLE->playerv[1].awareclock=BATTLE->playerv[1].awaretime*0.5;
  
  // Random position and direction for all labels.
  struct label *label=BATTLE->labelv;
  int i=BATTLE->labelc;
  for (;i-->0;label++) {
    label->x=rand()%((FBW-label->w)|1);
    label->y=rand()%((GROUNDY-label->h)|1);
    label->fx=label->x;
    label->fy=label->y;
    double t=((rand()&0xffff)*M_PI*2.0)/65536.0;
    label->dx=sin(t)*LABEL_SPEED;
    label->dy=cos(t)*LABEL_SPEED;
  }
  
  return 0;
}

/* Update a CPU player.
 * Select new value for (player->input).
 */
 
static void strangling_update_player_cpu(struct battle *battle,struct player *player,double elapsed) {

  // If we were holding a thumb button, release it. They stay on for just one frame.
  player->pvinput=player->input;
  player->input&=~(EGG_BTN_SOUTH|EGG_BTN_WEST);
  
  // Tick (awareclock) and on expiry, note the current requirements.
  if ((player->awareclock-=elapsed)<=0.0) {
    player->awareclock+=player->awaretime;
    player->input_choice=BATTLE->btnid_hold|BATTLE->btnid_tap;
  }
  
  // Tick (tapclock) and on expiry, press what we think are the right buttons.
  // The dpad bit latches and the thumb button will pop off next frame. (not that it matters, latching the dpad).
  if ((player->tapclock-=elapsed)<=0.0) {
    player->tapclock+=player->taptime;
    player->input=player->input_choice;
  }
}

/* Update either player, with its (input,pvinput) freshly updated.
 */
 
static void strangling_update_player_common(struct battle *battle,struct player *player,double elapsed) {
  // Power drains constantly.
  const double drain_rate=0.300;
  if ((player->power-=drain_rate*elapsed)<=0.0) {
    player->power=0.0;
  }
  // Must hold the one specific d-pad button.
  int holdok=((player->input&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN))==BATTLE->btnid_hold);
  // If they tap the wrong thumb button, a penalty. If holding wrong, any tap is wrong.
  int tapgood=0,tapbad=0;
  if ((player->input&EGG_BTN_SOUTH)&&!(player->pvinput&EGG_BTN_SOUTH)) {
    if (holdok&&(BATTLE->btnid_tap==EGG_BTN_SOUTH)) tapgood=1; else tapbad=1;
  }
  if ((player->input&EGG_BTN_WEST)&&!(player->pvinput&EGG_BTN_WEST)) {
    if (holdok&&(BATTLE->btnid_tap==EGG_BTN_WEST)) tapgood=1; else tapbad=1;
  }
  if (tapbad) {
    player->power=0.0;
  }
  if (tapgood) {
    if ((player->power+=0.200)>=1.0) player->power=1.0;
  }
}

/* Update.
 */
 
static void _strangling_update(struct battle *battle,double elapsed) {
  
  // Finished?
  if (battle->outcome>-2) return;
  
  // Tick animation.
  if ((BATTLE->animclock-=elapsed)<=0.0) {
    BATTLE->animclock+=0.200;
    if (++(BATTLE->animframe)>=2) BATTLE->animframe=0;
  }
  
  // Change requirements periodically.
  if ((BATTLE->changeclock-=elapsed)<=0.0) {
    BATTLE->changeclock=BATTLE->changetime+((rand()&0xffff)*CHANGE_TIME_VARIATION)/65535.0;
    strangling_change_requirements(battle);
  }
  
  // Move labels.
  struct label *label=BATTLE->labelv;
  int i=BATTLE->labelc;
  for (;i-->0;label++) {
    label->fx+=label->dx*elapsed;
    label->fy+=label->dy*elapsed;
    label->x=(int)label->fx;
    label->y=(int)label->fy;
    if ((label->x<0)&&(label->dx<0.0)) label->dx=-label->dx;
    else if ((label->x>FBW-label->w)&&(label->dx>0.0)) label->dx=-label->dx;
    if ((label->y<0)&&(label->dy<0.0)) label->dy=-label->dy;
    else if ((label->y>GROUNDY-label->h)&&(label->dy>0.0)) label->dy=-label->dy;
  }
  
  // Collect input, AI, and update each player's power.
  struct player *player=BATTLE->playerv;
  for (i=2;i-->0;player++) {
    if (player->human) {
      player->input=g.input[player->human];
      player->pvinput=g.pvinput[player->human];
    } else {
      strangling_update_player_cpu(battle,player,elapsed);
    }
    strangling_update_player_common(battle,player,elapsed);
  }
  
  // Select state based on players' power.
  // Different thresholds for catch and release, to avoid rapid toggling.
  if (BATTLE->stateclock>0.0) {
    BATTLE->stateclock-=elapsed;
  } else {
    const double threshup  =0.250;
    const double threshdown=0.210;
    int pvstate=BATTLE->state;
    if (BATTLE->state&2) {
      if (BATTLE->playerv[0].power<threshdown) BATTLE->state&=~2;
    } else {
      if (BATTLE->playerv[0].power>threshup) BATTLE->state|=2;
    }
    if (BATTLE->state&1) {
      if (BATTLE->playerv[1].power<threshdown) BATTLE->state&=~1;
    } else {
      if (BATTLE->playerv[1].power>threshup) BATTLE->state|=1;
    }
    if (pvstate!=BATTLE->state) {
      BATTLE->stateclock=STATE_HOLD_TIME;
    }
  }
  
  // Drop HP based on state.
  // Ties are mathematically possible, but practically unlikely.
  // We'll drop the left player first; right wins ties.
  // HP is not visible to the user.
  if (BATTLE->state&1) {
    if ((BATTLE->playerv[0].hp-=STRANGLE_RATE*elapsed*BATTLE->playerv[1].power)<=0.0) {
      battle->outcome=-1;
      BATTLE->end_cooldown=END_COOLDOWN;
      return;
    }
  }
  if (BATTLE->state&2) {
    if ((BATTLE->playerv[1].hp-=STRANGLE_RATE*elapsed*BATTLE->playerv[0].power)<=0.0) {
      battle->outcome=1;
      BATTLE->end_cooldown=END_COOLDOWN;
      return;
    }
  }
}

/* Render a power meter.
 * Caller supplies the final outer bounds.
 */
 
static void strangling_render_meter(struct battle *battle,int x,int y,int w,int h,double power) {
  int ph=(int)(h*power);
  if (ph<0) ph=0;
  else if (ph>h) ph=h;
  if (ph<h) graf_fill_rect(&g.graf,x,y,w,h,0x808080ff);
  if (ph>0) graf_fill_rect(&g.graf,x,y+h-ph,w,ph,0xff0000ff);
}

/* Render.
 */
 
static void _strangling_render(struct battle *battle) {

  // Background.
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,SKY_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,GROUND_COLOR);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  
  // Labels.
  if (battle->outcome==-2) {
    struct label *label=BATTLE->labelv;
    int i=BATTLE->labelc;
    for (;i-->0;label++) {
      if (!label->texid) continue;
      graf_set_input(&g.graf,label->texid);
      graf_decal(&g.graf,label->x,label->y,0,0,label->w,label->h);
    }
  }
  
  // Battle scene. I'm going super cheap on this: There's only 6 possible states, and 2 frames each except the victory states.
  // Each of the 10 possible scenes has been drawn explicitly.
  // Bottom row overlaps the horizon.
  const int srcw=96;
  const int srch=64;
  int srcx,srcy;
  if (battle->outcome==-1) {
    srcx=0;
    srcy=srch*4;
  } else if (battle->outcome==1) {
    srcx=srcw;
    srcy=srch*4;
  } else {
    srcx=srcw*BATTLE->animframe;
    srcy=srch*BATTLE->state;
  }
  graf_set_image(&g.graf,RID_image_battle_strangling);
  graf_decal(&g.graf,(FBW>>1)-(srcw>>1),GROUNDY-srch+1,srcx,srcy,srcw,srch);
  
  // Power meters left and right.
  int barw=10,barh=80;
  strangling_render_meter(battle,(FBW>>1)-60-barw,GROUNDY-5-barh,barw,barh,BATTLE->playerv[0].power);
  strangling_render_meter(battle,(FBW>>1)+60,GROUNDY-5-barh,barw,barh,BATTLE->playerv[1].power);
}

/* Type definition.
 */
 
const struct battle_type battle_type_strangling={
  .name="strangling",
  .objlen=sizeof(struct battle_strangling),
  .strix_name=19,
  .no_article=0,
  .no_contest=0,
  // We do support both pvp and cvc. But the appearance is always Dot vs Root Devil.
  .support_pvp=1,
  .support_cvc=1,
  .del=_strangling_del,
  .init=_strangling_init,
  .update=_strangling_update,
  .render=_strangling_render,
};

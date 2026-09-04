/* battle_rebounding.c
 * An advanced digital simulation of table tennis.
 */

#include "game/bellacopia.h"

/* Bounds of the field in framebuffer pixels.
 */
#define FLDL 5
#define FLDR 315
#define FLDT 20 /* Room for scoreboard above. */
#define FLDB 175

#define PLAYER_BUTTROOM 5
#define SCORE_COUNT 3 /* Should be odd. */

struct battle_rebounding {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    double barspeed; // px/sec, constant.
    double control; // 0..1 = reflect..control, constant. Degree to which the strike position influences the outgoing direction.
    double controlboost; // 0..1 = none..lots, constant. Degree to which motion at the moment of contact influences the outgoing direction.
    double barr; // Pixels, constant, half of the bar's height.
    double y; // Framebuffer pixels, center of bar.
    int indy;
    int score;
    double cpurecalc; // Counts down between CPU recalculations.
    double prediction; // Absolute vertical position where I expect the ball to cross my line.
    double cpuerror; // Signed pixels, how far off we're going to be.
  } playerv[2];
  
  double ballx,bally,ballr; // All in framebuffer pixels.
  double balldx,balldy; // px/sec
};

#define BATTLE ((struct battle_rebounding*)battle)

/* Delete.
 */
 
static void _rebounding_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  player->y=(FLDT+FLDB)*0.5;
  
  player->barr=10.0*(1.0-player->skill)+20.0*player->skill;
  player->barspeed=60.0*(1.0-player->skill)+90.0*player->skill;
  player->control=0.5;
  player->controlboost=0.25;
  player->cpuerror=1.0;
  
  if (player->human=human) { // Human.
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xf4ece2ff;
      } break;
    case NS_face_dot: {
        player->color=0xa668f3ff; // bright color, black background
      } break;
    case NS_face_princess: {
        player->color=0x5d83f4ff; // ''
      } break;
  }
}

/* Reset ball. Centered in the field, with random direction.
 */
 
static void rebounding_reset_ball(struct battle *battle) {
  BATTLE->ballx=(FLDL+FLDR)*0.5;
  BATTLE->bally=(FLDT+FLDB)*0.5;
  BATTLE->ballr=1.0; // Constant? I'm thinking we'll render as a single pixel.
  
  /* Ball's initial direction is a constant speed and random angle, within 1/8 turn of the horizon.
   * In other words, the angle has a total range of Pi, and we'll split it high/low.
   */
  const double speed=100.0;
  double t=((rand()&0xffff)*M_PI)/65535.0;
  if (t>=M_PI*0.5) { // Right.
    t=M_PI*0.25+t-M_PI*0.5;
  } else { // Left.
    t=M_PI*-0.75+t;
  }
  BATTLE->balldx=sin(t)*speed;
  BATTLE->balldy=-cos(t)*speed;
}

/* New.
 */
 
static int _rebounding_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  rebounding_reset_ball(battle);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player->indy=-1; break;
    case EGG_BTN_DOWN: player->indy=1; break;
    default: player->indy=0; break;
  }
}

/* Predict the ball's next paddle crossing.
 */
 
static double predict_ball(const struct battle *battle) {
  double x;
  if (BATTLE->balldx<0.0) {
    x=FLDL+PLAYER_BUTTROOM;
    if (BATTLE->ballx<=x) return BATTLE->bally;
  } else {
    x=FLDR-PLAYER_BUTTROOM;
    if (BATTLE->ballx>=x) return BATTLE->bally;
  }
  double dx=x-BATTLE->ballx;
  double s=dx/BATTLE->balldx;
  double y=BATTLE->bally+BATTLE->balldy*s;
  int panic=100; // More than a hundred bounces, let's assume something's wrong and return whatever.
  while (panic-->0) {
    if (y<FLDT) {
      y=FLDT+(FLDT-y);
    } else if (y>FLDB) {
      y=FLDB-(y-FLDB);
    } else {
      return y;
    }
  }
  return BATTLE->bally;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  int incoming=(
    (player->who&&(BATTLE->balldx>0.0))||
    (!player->who&&(BATTLE->balldx<0.0))
  );
  
  /* When the ball is moving away, we can't predict it (we don't know about the other player's interaction yet).
   * So just approach the middle.
   */
  if (!incoming) {
    const double thresh=5.0;
    const double mid=(FLDT+FLDB)*0.5;
    if (player->y<mid-thresh) player->indy=1;
    else if (player->y>mid+thresh) player->indy=-1;
    else player->indy=0;
    player->cpurecalc=0.0; // And recalculate as soon as it changes direction.
    return;
  }
  
  /* Tick down a private clock so we're not repeating the same calculation over and over.
   */
  if ((player->cpurecalc-=elapsed)<=0.0) {
    player->cpurecalc+=0.500;
    player->prediction=predict_ball(battle)+player->cpuerror;
  }
  
  /* Approach the prediction.
   * When (indy) is set, hold it until we cross the middle.
   * Otherwise, require a modest threshold off center.
   */
  const double thresh=5.0;
  if (player->indy<0) {
    if (player->y<=player->prediction) player->indy=0;
  } else if (player->indy>0) {
    if (player->y>=player->prediction) player->indy=0;
  } else if (player->y<player->prediction-thresh) player->indy=1;
  else if (player->y>player->prediction+thresh) player->indy=-1;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->indy) {
    player->y+=player->barspeed*elapsed*player->indy;
    if (player->y<FLDT+player->barr) player->y=FLDT+player->barr;
    else if (player->y>FLDB-player->barr) player->y=FLDB-player->barr;
  }
}

/* Score a point and reset the ball.
 */
 
static void rebounding_score(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
  const int thresh=(SCORE_COUNT>>1)+1;
  player->score++;
  if (player->score>=thresh) {
    battle->outcome=player->who?-1:1;
  } else {
    rebounding_reset_ball(battle);
  }
  
  // Reset CPU errors.
  int i=2; for (player=BATTLE->playerv;i-->0;player++) {
    player->cpuerror=1.0;
  }
}

/* Ball struck a paddle.
 */
 
static void rebounding_rebound(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_whack,player->who?PLAYER_PAN:-PLAYER_PAN);
  
  /* Choose two new vectors.
   * "wild" is the perfect elastic collision, kind of boring. Extremely boring if you play a whole game of it.
   * "tame" is chosen purely from the ball's position relative to the paddle. Input direction doesn't matter.
   */
  const double verticalest=M_PI*0.050;
  double speed=sqrt(BATTLE->balldx*BATTLE->balldx+BATTLE->balldy*BATTLE->balldy);
  double wilddx=-BATTLE->balldx/speed;
  double wilddy=BATTLE->balldy/speed;
  double tamep=(BATTLE->bally-player->y)/player->barr;
  if (tamep<-1.0) tamep=-1.0; else if (tamep>1.0) tamep=1.0;
  double tamet=M_PI*0.5+tamep*(M_PI*0.5-verticalest);
  if (player->who) tamet=-tamet;
  double tamedx=sin(tamet);
  double tamedy=-cos(tamet);
  
  /* Select the degree of control.
   * If the paddle is moving (regardless whether toward or away), it exerts more control.
   */
  double ctl=player->control;
  if (player->indy) ctl+=player->controlboost;
  if (ctl<0.0) ctl=0.0; else if (ctl>1.0) ctl=1.0;
  
  /* Choose a vector between the two reference vectors.
   * And a speed which is some proportion larger than the input speed -- ball keeps getting faster.
   * Then scale that out to the new speed.
   */
  double nspeed=speed*1.100;
  double dx=wilddx*(1.0-ctl)+tamedx*ctl;
  double dy=wilddy*(1.0-ctl)+tamedy*ctl;
  double len=sqrt(dx*dx+dy*dy);
  BATTLE->balldx=(dx*nspeed)/len;
  BATTLE->balldy=(dy*nspeed)/len;
  
  /* Force ball's horizontal position to the paddle.
   * If we don't do this, at very high speed it can hit the paddle and the far wall at the same time!
   */
  BATTLE->ballx=player->who?(FLDR-PLAYER_BUTTROOM):(FLDL+PLAYER_BUTTROOM);
  
  /* CPU players increase and reverse their intentional error on each of their whacks.
   * It's fine to do this to humans too, it just gets ignored.
   */
  player->cpuerror*=-2.000;
}

/* Update.
 */
 
static void _rebounding_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  // Move ball optimistically.
  double pvx=BATTLE->ballx;
  BATTLE->ballx+=BATTLE->balldx*elapsed;
  BATTLE->bally+=BATTLE->balldy*elapsed;
  
  // Collisions against the top and bottom walls reflect perfectly.
  if (
    ((BATTLE->bally<=FLDT)&&(BATTLE->balldy<0.0))||
    ((BATTLE->bally>=FLDB)&&(BATTLE->balldy>0.0))
  ) {
    bm_sound(RID_sound_bump);
    BATTLE->balldy=-BATTLE->balldy;
  }
  
  // Check paddles on the one frame when we cross them.
  int pzone=(pvx<FLDL+PLAYER_BUTTROOM)?-1:(pvx>FLDR-PLAYER_BUTTROOM)?1:0;
  int nzone=(BATTLE->ballx<FLDL+PLAYER_BUTTROOM)?-1:(BATTLE->ballx>FLDR-PLAYER_BUTTROOM)?1:0;
  if (nzone&&!pzone) { // Entered a butt zone.
    player=(nzone<0)?l:r;
    if (BATTLE->bally<player->y-player->barr) ; // whiff, above
    else if (BATTLE->bally>player->y+player->barr) ; // whiff, below
    else rebounding_rebound(battle,player);
  }
  
  // Collisions against the left and right walls cause a point and reset.
  if (BATTLE->ballx<FLDL) rebounding_score(battle,r);
  else if (BATTLE->ballx>FLDR) rebounding_score(battle,l);

  //XXX
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=player->who?(FLDR-PLAYER_BUTTROOM):(FLDL+PLAYER_BUTTROOM);
  int ya=lround(player->y-player->barr);
  int yz=lround(player->y+player->barr);
  uint32_t color=player->color;
  graf_line(&g.graf,x,ya,color,x,yz,color);
}

/* Render.
 */
 
static void _rebounding_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  uint32_t linecolor=0xffffffff;
  graf_line_strip_begin(&g.graf,FLDL,FLDT,linecolor);
  graf_line_strip_more(&g.graf,FLDR,FLDT,linecolor);
  graf_line_strip_more(&g.graf,FLDR,FLDB,linecolor);
  graf_line_strip_more(&g.graf,FLDL,FLDB,linecolor);
  graf_line_strip_more(&g.graf,FLDL,FLDT,linecolor);
  
  player_render(battle,l);
  player_render(battle,r);
  
  if (battle->outcome==-2) {
    int bx=lround(BATTLE->ballx-BATTLE->ballr);
    int by=lround(BATTLE->bally-BATTLE->ballr);
    graf_fill_rect(&g.graf,bx,by,2,2,0x40ff60ff);
  }
  
  const int sbcellw=6;
  const int sbcellh=6;
  const int sbxspacing=7;
  int sby=(FLDT>>1)-(sbcellh>>1);
  int sbx=(FBW>>1)-((SCORE_COUNT*sbxspacing)>>1);
  int i=0;
  for (;i<SCORE_COUNT;i++,sbx+=sbxspacing) {
    uint32_t color=0x202020ff;
    if (i<l->score) color=l->color;
    else if (i>=SCORE_COUNT-r->score) color=r->color;
    graf_fill_rect(&g.graf,sbx,sby,sbcellw,sbcellh,color);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_rebounding={
  .name="rebounding",
  .objlen=sizeof(struct battle_rebounding),
  .id=NS_battle_rebounding,
  .strix_name=334,
  .no_article=0,
  .no_contest=0,
  .no_timeout=1,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_vert,
  .imageid_default=0,
  .del=_rebounding_del,
  .init=_rebounding_init,
  .update=_rebounding_update,
  .render=_rebounding_render,
};

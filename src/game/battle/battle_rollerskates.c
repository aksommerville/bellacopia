/* battle_rollerskates.c
 */

#include "game/batsup/battle_internal.h"

#define ROCK_LIMIT 7

struct battle_rollerskates {
  struct battle hdr;
  uint32_t gclra,gclrb; // Two colors for the ground bands.
  
  /* Length of track in pixels.
   * Just the business part, from starting line to finish line.
   * Parts outside this range will be rendered too.
   */
  int tracklen;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double position; // Track pixels.
    double animclock;
    int animframe;
    double painclock; // Counts down after a rock collision.
    double jump; // Vertical displacement. ***LESS than zero when active***
    double jump_power;
    int injump,inbrake;
    int pvinjump;
    int blackout;
    double speed; // Current speed, px/sec. Starts at zero.
    double racetime;
    struct rock *rock_already; // Can't hit the same rock twice. We only need to record the most recent.
    int cpuslow;
  // Constant parameters for acceleration:
    double accel_normal,decel_normal,limit_normal;
    double accel_brake,decel_brake,limit_brake;
    double accel_pain,decel_pain,limit_pain;
    double jump_start,jump_rate,jump_limit; // jump_start is negative, ie going up. rate and limit are positive.
    double paintime;
  } playerv[2];
  
  struct rock {
    int x;
    double hlt0,hlt1; // Count down after we hurt a player, so it can highlight.
  } rockv[ROCK_LIMIT];
  int rockc;
};

#define BATTLE ((struct battle_rollerskates*)battle)

/* Delete.
 */
 
static void _rollerskates_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  
  #define FLD(tag,lo,hi) player->tag=lo*(1.0-player->skill)+hi*player->skill;
  FLD(accel_normal,140.0,160.0)
  FLD(decel_normal,100.0,100.0) // irrelevant
  FLD(limit_normal,500.0,500.0) // Not clear to me whether faster makes it easier.
  FLD(accel_brake,  40.0, 50.0)
  FLD(decel_brake, 180.0,300.0)
  FLD(limit_brake, 100.0, 40.0)
  FLD(accel_pain,   40.0, 40.0)
  FLD(decel_pain,  600.0,500.0)
  FLD(limit_pain,   50.0, 30.0)
  FLD(jump_start, -100.0,-90.0)
  FLD(jump_rate,   600.0,650.0)
  FLD(jump_limit,  150.0,150.0)
  FLD(paintime,0.600,0.500)
  #undef FLD
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xa59781ff;
        player->tileid=0xd6;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x96;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0xb6;
      } break;
  }
}

/* Darken RGBA.
 */
 
static uint32_t rgba_darken(uint32_t ref,uint8_t portion) {
  uint8_t r=((ref>>24)*portion)>>8;
  uint8_t g=(((ref>>16)&0xff)*portion)>>8;
  uint8_t b=(((ref>>8)&0xff)*portion)>>8;
  return (r<<24)|(g<<16)|(b<<8)|0xff;
}

/* Position for new rock.
 */
 
static int rollerskates_new_rock_position(const struct battle *battle) {
  // Set an arbitrary minimum. It's awkward if they appear right at the start.
  // Also don't let them appear right on the finish line.
  int lo=50,hi=BATTLE->tracklen-50;
  // Then do it dumb: Pick a random position, check proximity to other rocks, give up after so many tries.
  const int minspacing=50;
  int panic=50;
  while (panic-->0) {
    int ok=1;
    int x=lo+rand()%(hi-lo);
    const struct rock *rock=BATTLE->rockv;
    int i=BATTLE->rockc;
    for (;i-->0;rock++) {
      int dx=x-rock->x;
      if ((dx>-minspacing)&&(dx<minspacing)) {
        ok=0;
        break;
      }
    }
    if (ok) return x;
  }
  return -1;
}

/* New.
 */
 
static int _rollerskates_init(struct battle *battle) {
  
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  BATTLE->tracklen=2500;
  
  BATTLE->gclra=battle->ctab[BATTLE_COLOR_GROUND];
  BATTLE->gclrb=rgba_darken(BATTLE->gclra,0xc0);
  
  /* Random rocks.
   */
  while (BATTLE->rockc<ROCK_LIMIT) {
    int x=rollerskates_new_rock_position(battle);
    if (x<0) break;
    struct rock *rock=BATTLE->rockv+BATTLE->rockc++;
    rock->x=x;
  }
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&(EGG_BTN_LEFT|EGG_BTN_SOUTH))) player->blackout=0;
  } else {
    player->injump=(input&EGG_BTN_SOUTH)?1:0;
    player->inbrake=(input&EGG_BTN_LEFT)?1:0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Peak velocity is 500.
   * If we're going too fast, slow down until some much lower threshold.
   */
  const double hispeed=450.0;
  const double lospeed=300.0;
  if (player->cpuslow) {
    player->inbrake=1;
    if (player->speed<lospeed) player->cpuslow=0;
  } else {
    player->inbrake=0;
    if (player->speed>hispeed) player->cpuslow=1;
  }
  
  /* If my speed is ridiculously low, release all the inputs and let it ride.
   * Don't even bother looking for rocks at very low speed; let it warm up first.
   */
  if (player->speed<20.0) {
    player->inbrake=0;
    player->injump=0;
    return;
  }
  
  /* Find the next rock ahead of me.
   * This thresholding causes us to abort jumps when our center crosses the rock's center. I think that's correctish.
   */
  struct rock *next=0;
  struct rock *rock=BATTLE->rockv;
  int i=BATTLE->rockc;
  for (;i-->0;rock++) {
    if (rock->x<=player->position) continue;
    if (!next||(rock->x<next->x)) next=rock;
  }
  
  /* No rock means we're clear until the goal.
   * Stop jumping and let it ride.
   */
  if (!next) {
    player->injump=0;
    return;
  }
  
  /* Determine collision time to this rock, based on current speed.
   * Actually, ten pixels near of it, since that's the collision radius.
   * We will of course overestimate here because we're not accounting for acceleration.
   */
  double dangerx=next->x-10.0;
  double time_to_impact=(dangerx-player->position)/player->speed;
  if (time_to_impact<0.200) {
    player->injump=1;
  } else {
    player->injump=0;
  }
}

/* Hit a rock.
 */
 
static void rollerskates_trip(struct battle *battle,struct player *player,struct rock *rock) {
  bm_sound_pan(RID_sound_ouch,0.0);
  player->painclock=player->paintime;
  if (player->who) rock->hlt1=0.333;
  else rock->hlt0=0.333;
  player->rock_already=rock;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (battle->outcome>-2) {
    player->injump=0;
    player->inbrake=0;
  }
  if (player->position<BATTLE->tracklen) {
    player->racetime+=elapsed;
  }

  /* Jumping?
   */
  if (player->jump<0.0) {
    if (!player->injump&&(player->jump_power<0.0)) { // Early abort.
      player->jump_power=0.0;
    }
    player->jump+=player->jump_power*elapsed;
    player->jump_power+=player->jump_rate*elapsed;
    if (player->jump_power>player->jump_limit) player->jump_power=player->jump_limit;
    if (player->jump>=0.0) {
      bm_sound_pan(RID_sound_weebump,0.0);
    }
  } else {
    player->jump_power=player->jump_start;
  }
  if (player->injump&&!player->pvinjump) {
    if ((player->jump<0.0)||(player->painclock>0.0)) {
      // Can't start jumping while already jumping or in pain.
      // But don't update (pvinjump). Let it retrigger when they're ready.
    } else {
      player->pvinjump=1;
      player->jump_power=player->jump_start;
      player->jump=-0.001;
      bm_sound_pan(RID_sound_jump,0.0);
    }
  } else if (!player->injump&&player->pvinjump) {
    player->pvinjump=0;
  }

  /* Animation, for normal state only. Reset clock if we leave normal state.
   * (not that i expect that to matter).
   */
  if ((player->painclock>0.0)||(player->jump<0.0)) {
    player->animclock=0.0;
    player->animframe=0;
  } else if ((player->animclock-=elapsed)<=0.0) {
    player->animclock+=0.100;
    if (++(player->animframe)>=2) player->animframe=0;
  }
  
  /* Select acceleration parameters based on state: Jump, Pain, Brake, Normal.
   * Also "Finished" is a state, with a target velocity of zero.
   */
  double accel_rate,decel_rate,limit;
  if (player->position>=BATTLE->tracklen) {
    accel_rate=0.0;
    decel_rate=500.0;
    limit=0.0;
  } else if (player->jump<0.0) {
    accel_rate=0.0;
    decel_rate=0.0;
    limit=player->speed;
  } else if (player->painclock>0.0) {
    player->painclock-=elapsed;
    accel_rate=player->accel_pain;
    decel_rate=player->decel_pain;
    limit=player->limit_pain;
  } else if (player->inbrake) {
    accel_rate=player->accel_brake;
    decel_rate=player->decel_brake;
    limit=player->limit_brake;
  } else {
    accel_rate=player->accel_normal;
    decel_rate=player->decel_normal;
    limit=player->limit_normal;
  }
  
  /* Step speed towards the selected limit, at the selected rate.
   */
  if (player->speed<limit) {
    if ((player->speed+=accel_rate*elapsed)>=limit) player->speed=limit;
  } else if (player->speed>limit) {
    if ((player->speed-=decel_rate*elapsed)<=limit) player->speed=limit;
  }
  if (player->speed<=0.0) { // Speed zero means the race is over. Turn things off.
    player->animframe=0;
    player->painclock=0.0;
  }
  
  /* Apply speed.
   */
  player->position+=player->speed*elapsed;
  
  /* Check for rocks.
   */
  if ((player->jump>=0.0)&&(player->painclock<=0.0)) {
    const double radius=10.0;
    struct rock *rock=BATTLE->rockv;
    int i=BATTLE->rockc;
    for (;i-->0;rock++) {
      if (rock==player->rock_already) continue;
      double dx=(double)rock->x-player->position;
      if ((dx>=-radius)&&(dx<=radius)) {
        rollerskates_trip(battle,player,rock);
        break;
      }
    }
  }
}

/* Update.
 */
 
static void _rollerskates_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  struct rock *rock=BATTLE->rockv;
  for (i=BATTLE->rockc;i-->0;rock++) {
    if (rock->hlt0>0.0) rock->hlt0-=elapsed;
    if (rock->hlt1>0.0) rock->hlt1-=elapsed;
  }

  /* Battle ends when somebody crosses the finish line.
   * If they finish on the same frame, we'll dutifully check their times, but of course those will be equal.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    int ldone=(l->position>=BATTLE->tracklen);
    int rdone=(r->position>=BATTLE->tracklen);
    if (ldone||rdone) {
      if (!ldone) battle->outcome=-1;
      else if (!rdone) battle->outcome=1;
      else if (l->racetime<r->racetime) battle->outcome=1;
      else if (l->racetime>r->racetime) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player, at a given horizontal position.
 */
 
static void player_render(struct battle *battle,struct player *player,int dstx,int dsty) {
  uint8_t tileid=player->tileid;
  if (player->painclock>0.0) tileid+=3;
  else if (player->jump<0.0) { tileid+=2; dsty+=lround(player->jump); }
  else if (player->inbrake) tileid+=4;
  else if (player->animframe) tileid+=1;
  graf_tile(g_graf,dstx,dsty,tileid+0x10,0);
  graf_tile(g_graf,dstx,dsty-NS_sys_tilesize,tileid,0);
}

/* Render one viewport.
 */
 
static void rollerskates_render_half(struct battle *battle,struct player *player,int y0) {

  /* First fill the buffer with (gclra), then draw full vertical bars of (gclrb) where warranted.
   */
  graf_fill_rect(g_graf,0,y0,FBW,FBH>>1,BATTLE->gclra);
  const int player_screenx=FBW/3;
  const int colw=40;
  int grounday=30;
  int groundzy=85;
  int leftx=(int)player->position-player_screenx;
  int col=leftx/colw; if (leftx<0) col--;
  int colx=col*colw-leftx;
  if (col&1) { col++; colx+=colw; }
  for (;colx<FBW;colx+=colw*2,col+=2) {
    graf_fill_rect(g_graf,colx,y0,colw,FBH>>1,BATTLE->gclrb);
  }
  
  /* Start and finish lines, if visible.
   * These are 8x64 decals, and we need to compute their upper edge so it traces the horizon.
   */
  graf_set_image(g_graf,RID_image_battle_meadow);
  if ((leftx<8)&&(leftx+FBW>0)) {
    int dstx=-leftx;
    int dsty=y0+grounday+(dstx*(groundzy-grounday))/FBW;
    graf_decal(g_graf,dstx,dsty,176,128,8,64);
  }
  if ((leftx<BATTLE->tracklen+8)&&(leftx+FBW>BATTLE->tracklen)) {
    int dstx=BATTLE->tracklen-leftx;
    int dsty=y0+grounday+(dstx*(groundzy-grounday))/FBW;
    graf_decal(g_graf,dstx,dsty,184,128,8,64);
  }
  
  /* Then a triangle strip for the sky, and a line at the horizon.
   */
  graf_set_input(g_graf,0);
  graf_triangle_strip_begin(g_graf,
    FBW,y0+groundzy,battle->ctab[BATTLE_COLOR_SKY],
    0,y0+grounday,battle->ctab[BATTLE_COLOR_SKY],
    FBW,y0+grounday,battle->ctab[BATTLE_COLOR_SKY]
  );
  graf_triangle_strip_more(g_graf,0,y0,battle->ctab[BATTLE_COLOR_SKY]);
  graf_triangle_strip_more(g_graf,FBW,y0,battle->ctab[BATTLE_COLOR_SKY]);
  graf_line(g_graf,0,y0+(int)grounday,0x000000ff,FBW,y0+(int)groundzy,0x000000ff);
  
  /* Opponent.
   */
  graf_set_image(g_graf,RID_image_battle_meadow);
  struct player *other=BATTLE->playerv+(player->who?0:1);
  int ox=(int)other->position-leftx;
  if ((ox>-8)&&(ox<FBW+8)) {
    int oy=y0+grounday+(ox*(groundzy-grounday))/FBW-2;
    graf_set_alpha(g_graf,0x80);
    player_render(battle,other,ox,oy);
    graf_set_alpha(g_graf,0xff);
  }
  
  /* Rocks.
   */
  const struct rock *rock=BATTLE->rockv;
  int i=BATTLE->rockc;
  for (;i-->0;rock++) {
    int dstx=rock->x-leftx;
    if ((dstx<-8)||(dstx>=FBW+8)) continue;
    int dsty=y0+grounday+(dstx*(groundzy-grounday))/FBW;
    if ((rock->hlt0>0.0)&&!player->who) {
      graf_set_tint(g_graf,0xff000000|(int)(rock->hlt0*700.0));
      graf_tile(g_graf,dstx,dsty,0xcb,0);
      graf_set_tint(g_graf,0);
    } else if ((rock->hlt1>0.0)&&player->who) {
      graf_set_tint(g_graf,0xff000000|(int)(rock->hlt1*700.0));
      graf_tile(g_graf,dstx,dsty,0xcb,0);
      graf_set_tint(g_graf,0);
    } else {
      graf_tile(g_graf,dstx,dsty,0xcb,0);
    }
  }
  
  /* Hero.
   */
  player_render(battle,player,player_screenx,y0+46);
}

/* Render.
 */
 
static void _rollerskates_render(struct battle *battle) {
  /* Draw the top (aka "left") scene first, then the bottom (aka "right").
   * My hope is that sprites will never breach their top edge, so we are safe from clipping.
   * Breaching the horizontals or bottom is no problem.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  rollerskates_render_half(battle,l,0);
  rollerskates_render_half(battle,r,FBH>>1);
  graf_fill_rect(g_graf,0,FBH>>1,FBW,1,0x000000ff);
}

/* Type definition.
 */
 
static const struct battle_input battle_input_rollerskates[]={
  {1,EGG_BTN_LEFT|EGG_BTN_SOUTH},
{0}};
 
const struct battle_type battle_type_rollerskates={
  .name="rollerskates",
  .objlen=sizeof(struct battle_rollerskates),
  .id=119,
  .strix_name=398,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_rollerskates,
  .imageid_default=RID_image_meadow,
  .del=_rollerskates_del,
  .init=_rollerskates_init,
  .update=_rollerskates_update,
  .render=_rollerskates_render,
};

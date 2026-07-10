/* battle_figureskating.c
 */

#include "game/bellacopia.h"

#define EVENT_LIMIT 16

struct battle_figureskating {
  struct battle hdr;
  int choice;
  
  int guidedir; // -1,1. Never zero.
  int guidenow; // Nonzero if players should be moving right now.
  int eventp;
  double gameclock;
  
  /* We'll plan the whole sequence ahead of time.
   * (eventp) points to the next event.
   * (guidedir) starts zero and the first event has nonzero time.
   */
  struct event {
    double when; // Absolute seconds, compare to (gameclock).
    int guidedir; // Zero to unset (guidenow) and preserve previous (guidedir). You can't turn without pointing.
  } eventv[EVENT_LIMIT];
  int eventc;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // Constant.
    uint8_t xform;
    double animclock;
    int animframe;
    int skating; // -1,0,1
    double x; // Framebuffer pixels.
    double score; // More is better. Can be negative.
    double fudge; // Score multiplier, applied at each change.
    
    // CPU player.
    double reactlo,reacthi; // Limits for reaction time, driven by skill.
    int guidedir;
    int guidenow;
    double reactclock;
  } playerv[2];
};

#define BATTLE ((struct battle_figureskating*)battle)

/* Delete.
 */
 
static void _figureskating_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.400;
  } else { // Right.
    player->who=1;
    player->x=FBW*0.600;
    player->xform=EGG_XFORM_XREV;
  }
  player->fudge=0.800*(1.0-player->skill)+1.200*player->skill; // +-20% according to skill.
  if (player->human=human) { // Human.
  } else { // CPU.
    // With these scores and fudges, I tend to win at 0xb0 and tend to lose at 0xc0. I can't win at 0xff. 0x80 is easy.
    player->fudge*=0.800; // -20% CPU penalty.
    player->reacthi=0.600*(1.0-player->skill)+0.200*player->skill;
    player->reactlo=player->reacthi*0.5;
    player->guidedir=BATTLE->guidedir;
    player->guidenow=BATTLE->guidenow;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xe0f6f1ff;
        player->tileid=0x3a;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x38;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x3c;
      } break;
  }
}

/* Return a random number close to (target).
 */
 
static double fuzz_time(double target) {
  double n=(rand()&0xffff)/65535.0;
  n=0.900+n*0.200;
  return target*n;
}

/* New.
 */
 
static int _figureskating_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  BATTLE->guidedir=(rand()&1)?1:-1;
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  /* Generate the full schedule.
   */
  const double total_time=15.0; // We aim for this, but it's not exact.
  const double lead_delay=1.0;
  const double dance_ratio=0.500; // How much of the available time should be spent in motion?
  const int stepc=6; // No more than half of EVENT_LIMIT.
  double dance_time=(total_time-lead_delay)*dance_ratio;
  double still_time=total_time-lead_delay-dance_time; // Don't count lead_delay because we get that for free.
  double left_time=dance_time*0.5;
  double right_time=left_time;
  int leftc=stepc>>1;
  int rightc=leftc;
  if (stepc&1) {
    if (rand()&1) leftc++;
    else rightc++;
  }
  // Emit a step and a still until we run out of steps.
  double now=lead_delay;
  while (leftc||rightc) {
    if (BATTLE->eventc>EVENT_LIMIT-2) {
      fprintf(stderr,"%s:%d: EVENT_LIMIT too low\n",__FILE__,__LINE__);
      return -1;
    }
    int guidedir;
    if (leftc&&rightc) guidedir=(rand()&1)?-1:1;
    else if (leftc) guidedir=-1;
    else guidedir=1;
    struct event *event=BATTLE->eventv+BATTLE->eventc++;
    event->when=now;
    event->guidedir=guidedir;
    if (guidedir<0) {
      leftc--;
      double dur=fuzz_time(left_time/(leftc+1));
      left_time-=dur;
      now+=dur;
    } else {
      rightc--;
      double dur=fuzz_time(right_time/(rightc+1));
      right_time-=dur;
      now+=dur;
    }
    event=BATTLE->eventv+BATTLE->eventc++;
    event->when=now;
    event->guidedir=0;
    double dur=fuzz_time(still_time/(leftc+rightc+1));
    still_time-=dur;
    now+=dur;
  }
   
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->skating=-1; break;
    case EGG_BTN_RIGHT: player->skating=1; break;
    default: player->skating=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // If our state matches the walrus, keep doing what we're doing.
  if ((player->guidedir==BATTLE->guidedir)&&(player->guidenow==BATTLE->guidenow)) return;
  
  // If we don't have a reaction clock set, set it and return.
  if (player->reactclock<=0.0) {
    player->reactclock=(rand()&0xffff)/65535.0;
    player->reactclock=player->reactlo*(1.0-player->reactclock)+player->reacthi*player->reactclock;
    return;
  }
  
  // When (reactclock) reaches zero, match the walrus.
  if ((player->reactclock-=elapsed)>0.0) return;
  player->guidedir=BATTLE->guidedir;
  player->guidenow=BATTLE->guidenow;
  if (BATTLE->guidenow) player->skating=BATTLE->guidedir;
  else player->skating=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Animation.
  if (player->skating) {
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.250;
      if (++(player->animframe)>=2) player->animframe=0;
    }
    if (player->skating<0) player->xform=EGG_XFORM_XREV;
    else player->xform=0;
    // Motion.
    player->x+=25.0*elapsed*player->skating;
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
  
  // Scoring.
  const double SCORE_DANCE   = 20.000; // Doing the right nonzero thing.
  const double SCORE_IDLE    =  5.000; // Doing nothing when you're supposed to.
  const double SCORE_LAZY    = -5.000; // Doing nothing when you should be dancing.
  const double SCORE_EAGER   =-10.000; // Dancing when you should be doing nothing.
  const double SCORE_BACKWARD=-25.000; // Dancing the wrong way.
  double scale=elapsed*player->fudge;
  if (BATTLE->guidenow) {
    if (BATTLE->guidedir==player->skating) {
      player->score+=SCORE_DANCE*scale;
    } else if (!player->skating) {
      player->score+=SCORE_LAZY*scale;
    } else {
      player->score+=SCORE_BACKWARD*scale;
    }
  } else {
    if (player->skating) {
      player->score+=SCORE_EAGER*scale;
    } else {
      player->score+=SCORE_IDLE*scale;
    }
  }
}

/* Update.
 */
 
static void _figureskating_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* Tick (gameclock) and if it crosses the next event, apply that.
   * If we run out of events, game is over.
   */
  BATTLE->gameclock+=elapsed;
  if (BATTLE->eventp<BATTLE->eventc) {
    const struct event *event=BATTLE->eventv+BATTLE->eventp;
    if (BATTLE->gameclock>=event->when) {
      if (event->guidedir) {
        BATTLE->guidedir=event->guidedir;
        BATTLE->guidenow=1;
      } else {
        BATTLE->guidenow=0;
      }
      BATTLE->eventp++;
    }
  } else {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
    l->animframe=0;
    r->animframe=0;
    return;
  }
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
}

/* Render scoreboard.
 */
 
static void figureskating_render_scoreboard(struct battle *battle,struct player *player,int midx,int midy) {
  const int barw=100;
  const int barh=6;
  const int smin=0; // Scores outside 0..150 are unlikely but possible -- must clamp.
  const int smax=150;
  graf_set_input(&g.graf,0);
  int barx=midx-(barw>>1);
  int bary=midy-(barh>>1);
  int score=(int)player->score;
  int thumbw=((score-smin)*barw)/(smax-smin+1);
  if (thumbw<0) thumbw=0;
  else if (thumbw>barw) thumbw=barw;
  graf_fill_rect(&g.graf,barx,bary,barw,barh,0x808080ff);
  graf_fill_rect(&g.graf,barx,bary,thumbw,barh,player->color);
  graf_fill_rect(&g.graf,barx,bary,barw+1,1,0x000000ff);
  graf_fill_rect(&g.graf,barx,bary+barh,barw+1,1,0x000000ff);
  graf_fill_rect(&g.graf,barx,bary,1,barh+1,0x000000ff);
  graf_fill_rect(&g.graf,barx+barw,bary,1,barh+1,0x000000ff);
}

/* Render.
 */
 
static void _figureskating_render(struct battle *battle) {
  const uint32_t icecolor=0xf0f8ffff;
  const uint32_t edgecolor=0x001040ff;
  const int fadeh=20;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,icecolor);
  graf_gradient_rect(&g.graf,0,0,FBW,fadeh,edgecolor,edgecolor,icecolor,icecolor);
  graf_gradient_rect(&g.graf,0,FBH-fadeh,FBW,fadeh,icecolor,icecolor,edgecolor,edgecolor);
  graf_set_image(&g.graf,RID_image_icepalace_sprites);
  
  // Walrus high in the middle.
  uint8_t walrustile=BATTLE->guidenow?0x3e:0x36;
  uint8_t walrusxform=(BATTLE->guidedir<0)?EGG_XFORM_XREV:0;
  graf_tile(&g.graf,FBW>>1,FBH/3,walrustile,walrusxform);
  
  // Players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    uint8_t tileid=player->tileid;
    if (player->animframe) tileid+=1;
    graf_tile(&g.graf,(int)player->x,FBH>>1,tileid,player->xform);
  }
  
  /* Scoreboard.
   * The two bars align vertically, rather than the more obvious side-by-side placement.
   * Vertical makes it easier to compare them to each other.
   */
  figureskating_render_scoreboard(battle,BATTLE->playerv+0,FBW>>1,(FBH*2)/3);
  figureskating_render_scoreboard(battle,BATTLE->playerv+1,FBW>>1,(FBH*2)/3+8);
}

/* Type definition.
 */
 
const struct battle_type battle_type_figureskating={
  .name="figureskating",
  .objlen=sizeof(struct battle_figureskating),
  .id=NS_battle_figureskating,
  .strix_name=229,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_horz,
  .del=_figureskating_del,
  .init=_figureskating_init,
  .update=_figureskating_update,
  .render=_figureskating_render,
};

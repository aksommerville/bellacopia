/* battle_cannonball.c
 */

#include "game/batsup/battle_internal.h"

#define PLATFORMY 50
#define WATERY 150
#define PLATFORMW 80

struct battle_cannonball {
  struct battle hdr;
  double instructionclock;
  int instructionframe;
  double warningclock; // Counts down to forfeit after the first splash.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    double x,y;
    int indx,indy,injump;
    int blackout;
    int seated;
    int splashed; // Once nonzero, sprite doesn't render and can't move.
    int pvinjump;
    double gravity; // <0 when rising
    double xspeed;
    double jump_force;
    double gravity_rate;
    double gravity_limit;
    double splashclock;
    int splashframe;
    double cpuclock;
    double cputargetx;
    
    // Score is in three pieces: Alignment, velocity, and aggregate.
    // Rounded to integers for display.
    int distance;
    int velocity;
    int score;
  } playerv[2];
};

#define BATTLE ((struct battle_cannonball*)battle)

/* Delete.
 */
 
static void _cannonball_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=PLATFORMW-10.0;
  player->y=PLATFORMY-8.0;
  player->seated=1;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  
  player->xspeed=60.0*(1.0-player->skill)+100.0*player->skill;
  player->jump_force=140.0*(1.0-player->skill)+150.0*player->skill;
  player->gravity_rate=300.0;
  player->gravity_limit=400.0;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpuclock=1.000+((rand()&0xffff)*0.500)/65535.0; // Decorative initial delay.
    player->cputargetx=FBW*0.5;
    double maxerr=40.0*(1.0-player->skill)+10.0*player->skill;
    double minerr= 8.0*(1.0-player->skill)+ 1.0*player->skill;
    double err=(rand()&0xffff)/65535.0;
    err=(1.0-err)*minerr+err*maxerr;
    if (rand()&1) err=-err;
    player->cputargetx+=err;
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0x100e1dff;
        player->tileid=0xe0;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0xb0;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0xc0;
      } break;
  }
}

/* New.
 */
 
static int _cannonball_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->injump=(input&EGG_BTN_SOUTH)?1:0;
  }
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0; break;
  }
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player->indy=-1; break;
    case EGG_BTN_DOWN: player->indy=1; break;
    default: player->indy=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // Initial delay for no particular reason.
  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    return;
  }
  
  // Hold jump forever, and hold the appropriate dpad direction until we cross the target.
  player->injump=1;
  if (player->who) { // Start right, move left.
    if (player->x>player->cputargetx) player->indx=-1;
    else player->indx=0;
  } else { // Start left, move right.
    if (player->x<player->cputargetx) player->indx=1;
    else player->indx=0;
  }
}

/* Splash.
 */
 
static void cannonball_splash(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_splash,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->splashed=1;
  player->splashclock=0.200;
  
  /* Compute the final score.
   */
  player->distance=lround(FBW*0.5-player->x);
  if (player->distance<0) player->distance=-player->distance;
  player->velocity=lround(player->gravity);
  if (player->velocity<1) player->velocity=1; // huh?
  player->score=player->velocity-player->distance*player->distance;
  if (player->score<0) player->score=0;
  
  BATTLE->warningclock=10.0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  const double margin=7.0;

  /* Once splashed, we're done.
   * Just animate the ripples.
   */
  if (player->splashed) {
    if ((player->splashclock-=elapsed)<=0.0) {
      player->splashclock+=0.150;
      if (++(player->splashframe)>=7) player->splashframe=3; // Only the last 4 frames repeat.
    }
    return;
  }

  /* Begin or abort a jump.
   */
  if (player->injump!=player->pvinjump) {
    if (player->pvinjump=player->injump) {
      if (player->seated) {
        bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->gravity=-player->jump_force;
        player->seated=0;
      }
    } else {
      if (player->gravity<0.0) {
        player->gravity=0.0;
      }
    }
  }
  
  /* Update gravity.
   */
  if (!player->seated) {
    player->gravity+=player->gravity_rate*elapsed;
    if (player->gravity>player->gravity_limit) player->gravity=player->gravity_limit;
    player->y+=player->gravity*elapsed;
    if (player->y>=WATERY) { // Splash!
      cannonball_splash(battle,player);
    } else if (player->y>=PLATFORMY-8.0) {
      if ((player->x<PLATFORMW+margin)||(player->x>FBW-PLATFORMW-margin)) { // Hit the platform again.
        if (player->gravity>=10.0) {
          bm_sound_pan(RID_sound_weebump,player->who?PLAYER_PAN:-PLAYER_PAN);
        }
        player->y=PLATFORMY-8.0;
        player->seated=1;
        player->gravity=0.0;
      }
    }
  }
  
  /* Horizontal motion only when airborne.
   */
  if (!player->seated&&player->indx) {
    player->x+=player->xspeed*elapsed*player->indx;
    if (player->y>=PLATFORMY) { // In the drop barrel, clamp to platform edges.
      if (player->x<PLATFORMW+margin) player->x=PLATFORMW+margin;
      else if (player->x>FBW-PLATFORMW-margin) player->x=FBW-PLATFORMW-margin;
    } else { // In the upper zone, clamp to screen edge.
      if (player->x<margin) player->x=margin;
      else if (player->x>FBW-margin) player->x=FBW-margin;
    }
    if (player->indx>0) player->xform=0;
    else player->xform=EGG_XFORM_XREV;
  }
}

/* Update.
 */
 
static void _cannonball_update(struct battle *battle,double elapsed) {

  if ((BATTLE->instructionclock-=elapsed)<=0.0) {
    BATTLE->instructionclock+=0.250;
    if (++(BATTLE->instructionframe)>=2) BATTLE->instructionframe=0;
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (battle->outcome==-2) {
      if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
      else player_update_cpu(battle,player,elapsed);
    } else {
      player->indx=player->indy=player->injump=0;
    }
    player_update_common(battle,player,elapsed);
  }
  
  /* Battle ends when both players have splashed, or the warning clock expires.
   * Ties are possible.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    int timeout=0;
    if (BATTLE->warningclock>0.0) {
      if ((BATTLE->warningclock-=elapsed)<=0.0) {
        timeout=1;
      }
    }
    if ((l->splashed&&r->splashed)||timeout) {
      if (!l->splashed) battle->outcome=-1;
      else if (!r->splashed) battle->outcome=1;
      else if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=(int)player->y;
  
  if (player->splashed) {
    y=WATERY+1;
    graf_tile(g_graf,x,y,0xf0+player->splashframe,0);
    return;
  }
  
  uint8_t tileid=player->tileid;
  if (player->seated) ;
  else if (player->gravity<0.0) tileid+=1;
  else tileid+=2;
  graf_tile(g_graf,x,y,tileid,player->xform);
}

/* Scores.
 */
 
static void cannonball_render_text(int x,int y,int align,const char *pfx,int n,const char *sfx) {
  if (n<0) n=0;
  char tmp[32];
  int tmpc=0;
  for (;*pfx;pfx++) tmp[tmpc++]=*pfx;
  if (n>=1000) tmp[tmpc++]='0'+(n/1000)%10;
  if (n>= 100) tmp[tmpc++]='0'+(n/100)%10;
  if (n>=  10) tmp[tmpc++]='0'+(n/10)%10;
  tmp[tmpc++]='0'+n%10;
  for (;*sfx;sfx++) tmp[tmpc++]=*sfx;
  if (align>0) {
    int i=tmpc;
    for (;i-->0;x-=8) graf_tile(g_graf,x,y,tmp[i],0);
  } else {
    int i=0;
    for (;i<tmpc;i++,x+=8) graf_tile(g_graf,x,y,tmp[i],0);
  }
}
 
static void cannonball_render_scores(struct battle *battle,struct player *player) {
  int y=130;
  int x,align;
  if (player->who) {
    x=FBW-10;
    align=1;
  } else {
    x=10;
    align=-1;
  }
  cannonball_render_text(x,y,align,"",player->distance," cm"); y+=8;
  cannonball_render_text(x,y,align,"",player->velocity," cm/s"); y+=8;
  cannonball_render_text(x,y,align,"= ",player->score,"");
}

/* Render.
 */
 
static void _cannonball_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;

  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,WATERY,FBW,FBH-WATERY,0x0e55a4ff);
  graf_fill_rect(g_graf,0,WATERY,FBW,1,0x000000ff);
  graf_fill_rect(g_graf,0,PLATFORMY,PLATFORMW,FBH-PLATFORMY,0x000000ff);
  graf_fill_rect(g_graf,0,PLATFORMY+1,PLATFORMW-1,FBH-PLATFORMY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,FBW-PLATFORMW,PLATFORMY,PLATFORMW,FBH-PLATFORMY,0x000000ff);
  graf_fill_rect(g_graf,FBW-PLATFORMW+1,PLATFORMY+1,PLATFORMW-1,FBH-PLATFORMY,battle->ctab[BATTLE_COLOR_GROUND]);

  graf_set_image(g_graf,RID_image_battle_tundra);
  
  { // Instructions.
    int srcx=NS_sys_tilesize*10;
    int srcy=NS_sys_tilesize*(10+BATTLE->instructionframe);
    int w=NS_sys_tilesize*5;
    int h=NS_sys_tilesize;
    graf_decal(g_graf,(FBW>>1)-(w>>1),5,srcx,srcy,w,h);
  }
  
  // Arrow pointing dead center, after both have splashed.
  if (l->splashed&&r->splashed) {
    graf_tile(g_graf,FBW>>1,WATERY-NS_sys_tilesize,0xa9,0);
  }
  
  player_render(battle,l);
  player_render(battle,r);
  
  // Scores on the side, after splash per player.
  if (l->splashed||r->splashed) {
    graf_set_image(g_graf,RID_image_fonttiles);
    graf_set_tint(g_graf,battle->ctab[BATTLE_COLOR_GROUND_TEXT]);
    if (l->splashed) cannonball_render_scores(battle,l);
    if (r->splashed) cannonball_render_scores(battle,r);
    graf_set_tint(g_graf,0);
  }
  
  // Warning clock above the unfinished player.
  if ((battle->outcome==-2)&&(BATTLE->warningclock>0.0)&&(BATTLE->warningclock<5.0)) {
    int s=(int)(BATTLE->warningclock+0.999);
    if ((s>0)&&(s<=5)) {
      struct player *lazy=0;
      if (!l->splashed) lazy=l;
      else if (!r->splashed) lazy=r;
      if (lazy) {
        graf_set_image(g_graf,RID_image_fonttiles);
        graf_set_tint(g_graf,battle->ctab[BATTLE_COLOR_SKY_TEXT]);
        graf_tile(g_graf,(int)lazy->x,(int)lazy->y-14,'0'+s,0);
        graf_set_tint(g_graf,0);
      }
    }
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_cannonball={
  .name="cannonball",
  .objlen=sizeof(struct battle_cannonball),
  .id=141,
  .strix_name=446,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz_a,
  .imageid_default=RID_image_tundra,
  .del=_cannonball_del,
  .init=_cannonball_init,
  .update=_cannonball_update,
  .render=_cannonball_render,
};

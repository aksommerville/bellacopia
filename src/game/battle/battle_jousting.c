/* battle_jousting.c
 */

#include "game/bellacopia.h"

#define GROUNDY 160

struct battle_jousting {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // 5 tiles: idle,walk1,walk2,fly1,fly2
    uint8_t xform;
    double x,y; // Framebuffer pixels
    int indx,inflap; // Controller sets.
    int blackout;
    int pvinflap;
    double dy; // px/s, accounts for both gravity and flying.
    int seated;
    double flapdelay;
    double animclock;
    int animframe;
    double hurt;
    double hurtdx;
    double cpuflapclock;
    double cputurnclock;
    double cpuflaplo,cpuflaphi;
    double cputurnlo,cputurnhi;
    double flappower;
    double groundspeed;
    double airspeed;
    double lingerspeed;
  } playerv[2];
};

#define BATTLE ((struct battle_jousting*)battle)

/* Delete.
 */
 
static void _jousting_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->seated=1;
  player->y=GROUNDY-(NS_sys_tilesize>>1);
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.250;
  } else { // Right.
    player->who=1;
    player->x=FBW*0.750;
    player->xform=EGG_XFORM_XREV;
  }
  player->flappower=45.0*(1.0-player->skill)+60.0*player->skill;
  player->groundspeed=60.0*(1.0-player->skill)+80.0*player->skill;
  player->lingerspeed=80.0*(1.0-player->skill)+60.0*player->skill;
  player->airspeed=100.0*(1.0-player->skill)+120.0*player->skill;
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpuflaplo=0.300*(1.0-player->skill)+0.200*player->skill;
    player->cputurnlo=0.500*(1.0-player->skill)+2.000*player->skill;
    player->cpuflaphi=player->cpuflaplo*2.0;
    player->cputurnhi=player->cputurnlo*2.0;
    double n=(rand()&0xffff)/65535.0;
    player->cpuflapclock=player->cpuflaplo*(1.0-n)+player->cpuflaphi*n;
    n=(rand()&0xffff)/65535.0;
    player->cputurnclock=player->cputurnlo*(1.0-n)+player->cputurnhi*n;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x4b64bdff;
        player->tileid=0xa3;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x83;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x93;
      } break;
  }
}

/* New.
 */
 
static int _jousting_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0; break;
  }
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->inflap=(input&EGG_BTN_SOUTH)?1:0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If I'm too high, don't flap.
   * Otherwise, flap whenever my clock expires.
   */
  if (player->y<60.0) {
    player->inflap=0;
  } else if ((player->cpuflapclock-=elapsed)<=0.0) {
    double n=(rand()&0xffff)/65535.0;
    player->cpuflapclock=player->cpuflaplo*(1.0-n)+player->cpuflaphi*n;
    player->inflap=1;
  } else {
    player->inflap=0;
  }
  
  /* When my turn clock expires, option to turn. Face the other player.
   */
  if ((player->cputurnclock-=elapsed)<=0.0) {
    double n=(rand()&0xffff)/65535.0;
    player->cputurnclock=player->cputurnlo*(1.0-n)+player->cputurnhi*n;
    struct player *other=BATTLE->playerv;
    if (other==player) other++;
    if (other->x<player->x) {
      if (!player->xform) player->xform=EGG_XFORM_XREV;
    } else {
      if (player->xform) player->xform=0;
    }
  }
  player->indx=player->xform?-1:1;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (battle->outcome>-2) {
    player->indx=0;
    player->inflap=0;
  }

  if ((player->animclock-=elapsed)<=0.0) {
    player->animclock+=0.200;
    if (++(player->animframe)>=2) player->animframe=0;
    if (player->seated&&player->indx) bm_sound_pan(RID_sound_tick,player->who?PLAYER_PAN:-PLAYER_PAN);
  }
  
  if (player->hurt>0.0) {
    player->hurt-=elapsed;
    player->indx=0;
    player->inflap=0;
    player->x+=player->hurtdx*elapsed;
  }

  // Flap?
  if (player->flapdelay>0.0) {
    player->flapdelay-=elapsed;
  } else if (player->inflap!=player->pvinflap) {
    if (player->pvinflap=player->inflap) {
      bm_sound_pan(RID_sound_chatter,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->flapdelay=0.100;
      player->dy-=player->flappower;
    }
  }
  
  // Gravity.
  player->dy+=150.0*elapsed;
  if (player->dy>100.0) player->dy=100.0;
  
  // Vertical motion.
  player->y+=player->dy*elapsed;
  if (player->y<8.0) {
    bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->y=8.0;
    player->dy=100.0;
    player->hurt=1.000;
    player->hurtdx=0.0; // Since we're airborne, there is horizontal motion anyway.
  }
  if (player->y>GROUNDY-8.0) {
    player->dy=0.0;
    player->y=GROUNDY-8.0;
    if (!player->seated) {
      player->seated=1;
      player->animclock=0.0;
    }
  } else {
    player->seated=0;
  }
  
  // Horizontal motion.
  if (player->indx) {
    player->xform=(player->indx<0)?EGG_XFORM_XREV:0;
    double speed=player->seated?player->groundspeed:player->airspeed;
    player->x+=speed*elapsed*player->indx;
  } else if (!player->seated) {
    double speed=player->lingerspeed;
    if (player->xform) speed=-speed;
    player->x+=speed*elapsed;
  }
  if (player->x<0.0) player->x+=FBW;
  else if (player->x>FBW) player->x-=FBW;
}

/* Update.
 */
 
static void _jousting_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Player-on-player collisions are managed out here.
   * We know there's exactly two of them, so no sense generalizing into the player handler.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if ((battle->outcome==-2)&&(l->hurt<=0.0)&&(r->hurt<=0.0)) {
    double dx=r->x-l->x;
    if (dx>=FBW) dx-=FBW;
    else if (dx<-FBW) dx+=FBW;
    double dy=r->y-l->y;
    if ((dx>-10.0)&&(dx<10.0)&&(dy>-8.0)&&(dy<8.0)) {
      struct player *lp,*rp; // "l" and "r" are their permanent names. "lp" and "rp" according to current orientation.
      if (dx>0.0) {
        lp=l;
        rp=r;
      } else {
        lp=r;
        rp=l;
      }
      int lattack=(lp->xform==0);
      int rattack=(rp->xform==EGG_XFORM_XREV);
      if (!lattack&&!rattack) {
        // Improbably, both facing the wrong way. Nothing happens.
      } else if (lattack&&rattack) {
        // Likeliest: Both facing each other. Highest wins, unless they're very close, then they both bounce off.
        if (dy<-2.0) {
          battle->outcome=-1;
        } else if (dy>2.0) {
          battle->outcome=1;
        } else {
          lp->hurt=0.500;
          rp->hurt=0.500;
          lp->hurtdx=-150.0;
          rp->hurtdx=150.0;
          bm_sound(RID_sound_ouch);
        }
      } else {
        // Stab in the back: Stabber wins.
        if (lp==l) battle->outcome=lattack?1:-1;
        else battle->outcome=lattack?-1:1;
      }
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  uint8_t tileid=player->tileid;
  int x=(int)player->x;
  int y=(int)player->y;
  if (player->seated) {
    if (player->indx) tileid+=1+player->animframe;
  } else {
    tileid+=3+player->animframe;
  }
  if (player->hurt>0.0) graf_set_tint(&g.graf,0xff000080);
  graf_tile(&g.graf,x,y,tileid,player->xform);
  // Screen wraps around horizontally:
  if (x<NS_sys_tilesize>>1) graf_tile(&g.graf,x+FBW,y,tileid,player->xform);
  else if (x>FBW-(NS_sys_tilesize>>1)) graf_tile(&g.graf,x-FBW,y,tileid,player->xform);
  graf_set_tint(&g.graf,0);
}

/* Render.
 */
 
static void _jousting_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,0x000000ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(&g.graf,RID_image_battle_war);
  
  int x=NS_sys_tilesize>>1;
  int y=NS_sys_tilesize>>1;
  for (;x<FBW;x+=NS_sys_tilesize) graf_tile(&g.graf,x,y,0x88,0);
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_jousting={
  .name="jousting",
  .objlen=sizeof(struct battle_jousting),
  .id=NS_battle_jousting,
  .strix_name=298,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_horz_a,
  .imageid_default=0,
  .del=_jousting_del,
  .init=_jousting_init,
  .update=_jousting_update,
  .render=_jousting_render,
};

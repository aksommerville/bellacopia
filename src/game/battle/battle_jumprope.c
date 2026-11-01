/* battle_jumprope.c
 */

#include "game/batsup/battle_internal.h"

#define GROUNDY 130
#define GRAVITY_RATE 400.0
#define GRAVITY_LIMIT 150.0

struct battle_jumprope {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // top left of a 3x3. 3 frames horizontally.
    uint8_t xform;
    int x; // Center of sprite, framebuffer pixels.
    double y; // Bottom, framebuffer pixels.
    double gravity; // Negative for jump, positive for proper gravity.
    double jump_power; // Constant per skill.
    int injump;
    int jumpok;
    int pvinjump;
    double cpustart; // Rope radians, when do I start jumping. ~-pi/2..0
    double cpustartd; // Advance (cpustart) by so much each turn so eventually I lose.
  } playerv[2];
  
  double ropet; // Radians, counts up to pi, wraps to -pi at the top, crosses the floor at zero.
  double rope_speed; // rad/sec
  double birdanimclock;
  int birdanimframe;
};

#define BATTLE ((struct battle_jumprope*)battle)

/* Delete.
 */
 
static void _jumprope_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=120;
  player->y=GROUNDY;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  
  player->jump_power=80.0*(1.0-player->skill)+200.0*player->skill;
  player->injump=1; // So we don't need an explicit blackout.
  player->pvinjump=1;
  player->jumpok=1;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cpustart=-1.000;
    player->cpustartd=0.200*(1.0-player->skill)+0.040*player->skill;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x9e804eff;
        player->tileid=0xd0;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x70;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0xa0;
      } break;
  }
}

/* New.
 */
 
static int _jumprope_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->rope_speed=4.000;
  return 0;
}

/* Begin jump.
 */
 
static void jumprope_jump(struct battle *battle,struct player *player) {
  if (!player->jumpok) return;
  player->jumpok=0;
  bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->gravity=-player->jump_power;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  player->injump=(input&EGG_BTN_SOUTH)?1:0;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((BATTLE->ropet>=player->cpustart)&&(BATTLE->ropet<0.0)) {
    player->injump=1;
  } else if (player->injump) {
    player->cpustart+=player->cpustartd;
    player->injump=0;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (player->injump!=player->pvinjump) {
    if (player->pvinjump=player->injump) { // Press.
      jumprope_jump(battle,player);
    } else { // Release.
      if (player->gravity<0.0) player->gravity=0.0;
    }
  }
  
  player->gravity+=GRAVITY_RATE*elapsed;
  if (player->gravity>GRAVITY_LIMIT) player->gravity=GRAVITY_LIMIT;
  player->y+=player->gravity*elapsed;
  if (player->y>=GROUNDY) {
    player->y=GROUNDY;
    player->gravity=0.0;
    if (!player->jumpok) {
      player->jumpok=1;
      bm_sound_pan(RID_sound_weebump,player->who?PLAYER_PAN:-PLAYER_PAN);
    }
  }
}

/* Update rope.
 */
 
static void jumprope_update_rope(struct battle *battle,double elapsed) {
  double pvt=BATTLE->ropet;
  BATTLE->ropet+=BATTLE->rope_speed*elapsed;
  if (BATTLE->ropet>=M_PI) { // Passing zenith.
    BATTLE->ropet-=M_PI*2.0;
  } else if ((pvt<0.0)&&(BATTLE->ropet>=0.0)) { // Passing nadir -- the important end. "<0" and ">=0" are important -- the rope starts at zero and shouldn't trigger initially.
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->jumpok&&r->jumpok) battle->outcome=0; // Both on the floor. What a pair of goofballs.
    else if (l->jumpok) battle->outcome=-1;
    else if (r->jumpok) battle->outcome=1;
    else BATTLE->rope_speed+=0.500;
  }
}

/* Update.
 */
 
static void _jumprope_update(struct battle *battle,double elapsed) {

  // Always animate the birds.
  if ((BATTLE->birdanimclock-=elapsed)<=0.0) {
    BATTLE->birdanimclock+=0.125;
    if (++(BATTLE->birdanimframe)>=4) BATTLE->birdanimframe=0;
  }
  
  // If finished, don't update rope or players -- leave them suspended in the air so the user can see what ended it.
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  jumprope_update_rope(battle,elapsed);
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int xcent=player->x;
  int xback,xfore;
  if (player->xform) {
    xback=xcent+NS_sys_tilesize;
    xfore=xcent-NS_sys_tilesize;
  } else {
    xback=xcent-NS_sys_tilesize;
    xfore=xcent+NS_sys_tilesize;
  }
  int yc=(int)player->y-(NS_sys_tilesize>>1)+1;
  int yb=yc-NS_sys_tilesize;
  int ya=yb-NS_sys_tilesize;
  uint8_t tileid=player->tileid;
  if (player->jumpok) ; // On the floor. Keep the neutral tile.
  else if (player->gravity<0.0) tileid+=3;
  else tileid+=6;
  graf_tile(g_graf,xback,ya,tileid+0x00,player->xform);
  graf_tile(g_graf,xcent,ya,tileid+0x01,player->xform);
  graf_tile(g_graf,xfore,ya,tileid+0x02,player->xform);
  graf_tile(g_graf,xback,yb,tileid+0x10,player->xform);
  graf_tile(g_graf,xcent,yb,tileid+0x11,player->xform);
  graf_tile(g_graf,xfore,yb,tileid+0x12,player->xform);
  graf_tile(g_graf,xback,yc,tileid+0x20,player->xform);
  graf_tile(g_graf,xcent,yc,tileid+0x21,player->xform);
  graf_tile(g_graf,xfore,yc,tileid+0x22,player->xform);
}

/* Quadratic curve.
 */
 
struct point {
  double x,y;
};

static void quadcurve(
  struct point *dst,
  const struct point *a,const struct point *b,const struct point *c,
  struct point *ac,struct point *cb // Optional return vectors for the control points.
) {
  double acx=(a->x+c->x)*0.5;
  double acy=(a->y+c->y)*0.5;
  double cbx=(c->x+b->x)*0.5;
  double cby=(c->y+b->y)*0.5;
  dst->x=(acx+cbx)*0.5;
  dst->y=(acy+cby)*0.5;
  if (ac) { ac->x=acx; ac->y=acy; }
  if (cb) { cb->x=cbx; cb->y=cby; }
}

/* Render rope.
 */
 
static void jumprope_render_rope(struct battle *battle) {
  const double radius=40.0; // Must be at least 24 to clear their heads.
  const double midy=GROUNDY-radius;
  double ny=cos(BATTLE->ropet);
  double maxy=midy+ny*(radius+2.0); // Cheat the radius out a little here, ok to overlap the ground a little.
  double handy=midy+ny*10.0; // Hand moves, but not nearly so much as the rope.
  double xa=30.0;
  double xz=BATTLE->playerv[0].x-20.0;
  
  struct point ctl={xa,maxy};
  struct point ctllo,ctlhi;
  struct point pointv[5];
  pointv[0].x=xa;
  pointv[0].y=handy;
  pointv[4].x=xz;
  pointv[4].y=maxy;
  quadcurve(pointv+2,pointv+0,pointv+4,&ctl,&ctllo,&ctlhi);
  quadcurve(pointv+1,pointv+0,pointv+2,&ctllo,0,0);
  quadcurve(pointv+3,pointv+2,pointv+4,&ctlhi,0,0);
  
  uint32_t color=battle->ctab[BATTLE_COLOR_SKY_TEXT];
  graf_set_input(g_graf,0);
  graf_line_strip_begin(g_graf,(int)pointv[0].x,(int)pointv[0].y,color);
  graf_line_strip_more(g_graf,(int)pointv[1].x,(int)pointv[1].y,color);
  graf_line_strip_more(g_graf,(int)pointv[2].x,(int)pointv[2].y,color);
  graf_line_strip_more(g_graf,(int)pointv[3].x,(int)pointv[3].y,color);
  graf_line_strip_more(g_graf,(int)pointv[4].x,(int)pointv[4].y,color);
  int i=5; while (i-->0) pointv[i].x=FBW-pointv[i].x;
  graf_line_strip_more(g_graf,(int)pointv[4].x,(int)pointv[4].y,color);
  graf_line_strip_more(g_graf,(int)pointv[3].x,(int)pointv[3].y,color);
  graf_line_strip_more(g_graf,(int)pointv[2].x,(int)pointv[2].y,color);
  graf_line_strip_more(g_graf,(int)pointv[1].x,(int)pointv[1].y,color);
  graf_line_strip_more(g_graf,(int)pointv[0].x,(int)pointv[0].y,color);
  
  // Two little birds carrying it.
  graf_set_image(g_graf,RID_image_battle_desert);
  uint8_t tileid=0x19;
  switch (BATTLE->birdanimframe) {
    case 1: case 3: tileid+=1; break;
    case 2: tileid+=2; break;
  }
  graf_tile(g_graf,(int)xa-2,(int)handy,tileid,0);
  graf_tile(g_graf,FBW-(int)xa+2,(int)handy,tileid,EGG_XFORM_XREV);
}

/* Render.
 */
 
static void _jumprope_render(struct battle *battle) {

  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,1,0x000000ff);
  
  // When the rope is positive, draw it behind the players.
  if (BATTLE->ropet>=0.0) jumprope_render_rope(battle);
  
  graf_set_image(g_graf,RID_image_battle_desert);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // When the rope is negative, draw it in front of the players.
  if (BATTLE->ropet<0.0) jumprope_render_rope(battle);
}

/* Type definition.
 */
 
const struct battle_type battle_type_jumprope={
  .name="jumprope",
  .objlen=sizeof(struct battle_jumprope),
  .id=111,
  .strix_name=382,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .imageid_default=RID_image_desert,
  .del=_jumprope_del,
  .init=_jumprope_init,
  .update=_jumprope_update,
  .render=_jumprope_render,
};

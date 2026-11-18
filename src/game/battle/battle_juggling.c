/* battle_juggling.c
 */

#include "game/batsup/battle_internal.h"

#define GROUNDY 140
#define THING_LIMIT 6

struct battle_juggling {
  struct battle hdr;
  double graceclock; // Don't start keeping score until a second or so has passed.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    int x,y;
    int lost;
    int blackout;
    double throw_initial; // px/s, negative.
    double gravity_lo,gravity_hi; // px/s**2. Very artificial! Gravity gets stronger during the thing's ascent.
    double gravity_rate; // px/s**2. Volatile. Each throw starts at gravity_lo and increases to gravity_hi
    double gravity_limit; // px/s
    double score_rate; // points per thing per second.
    double score; // 0..1, how much time you've spent holding a thing. First to breach 1 loses.
    double cpuclock;
    double cpuholdlo,cpuholdhi; // How long CPU player holds his button.
    double cpuskiplo,cpuskiphi; // How long CPU player waits between strokes. Perfect play, this would be zeroish.
    
    struct thing {
      double x,y;
      double dy;
      uint8_t tileid;
      int hold; // 0=airborne or dropped. 1=nearest to hand, ...
    } thingv[THING_LIMIT];
    int thingc;
    struct thing *thing_active; // If present, the button is held and this one is cancellable.
    
  } playerv[2];
};

#define BATTLE ((struct battle_juggling*)battle)

/* Delete.
 */
 
static void _juggling_del(struct battle *battle) {
}

/* Random thing tile.
 */
 
static uint8_t random_thing_tile() {
  switch (rand()%7) {
    case 0: return 0xf0;
    case 1: return 0xf1;
    case 2: return 0xf2;
    case 3: return 0xf3;
    case 4: return 0xe3;
    case 5: return 0xb3;
    case 6: return 0x83;
  }
  return 0xf0;
}

/* Set position of a thing, in the player's arms.
 */
 
static void juggling_force_thing_position(struct battle *battle,struct player *player,struct thing *thing) {
  if (thing->hold<=0) return; // Not being held; let nature drive it.
  int rx,ry;
  if (thing->hold==1) { // Position 1 is special; that's the one in your hand.
    rx=17;
    ry=9;
  } else { // Other things line up a bit back of the first.
    rx=10-thing->hold*3;
    ry=8-thing->hold;
  }
  if (player->xform) rx=-rx;
  thing->x=player->x+rx;
  thing->y=player->y+ry;
}

static void juggling_slide_thing_position(struct battle *battle,struct player *player,struct thing *thing,double elapsed) {
  if (thing->hold<=0) return;
  if (player->lost) return;
  double tx=thing->x,ty=thing->y;
  if (thing->hold==1) {
    tx=17.0;
    ty=9.0;
  } else {
    tx=10.0-thing->hold*3.0;
    ty=8.0-thing->hold;
  }
  if (player->xform) tx=-tx;
  tx+=player->x;
  ty+=player->y;
  const double speed=100.0;
  if (thing->x<tx) {
    if ((thing->x+=speed*elapsed)>=tx) thing->x=tx;
  } else if (thing->x>tx) {
    if ((thing->x-=speed*elapsed)<=tx) thing->x=tx;
  }
  if (thing->y<ty) {
    if ((thing->y+=speed*elapsed)>=ty) thing->y=ty;
  } else if (thing->y>ty) {
    if ((thing->y-=speed*elapsed)<=ty) thing->y=ty;
  }
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=80;
  player->y=GROUNDY-NS_sys_tilesize-(NS_sys_tilesize>>1)+1;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  
  player->throw_initial=-200.0;
  player->gravity_lo=50.0;
  player->gravity_hi=150.0;
  player->gravity_limit=80.0;
  player->gravity_rate=player->gravity_lo;
  player->score_rate=0.030;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpuclock=0.400+((rand()&0xffff)*0.200)/65535.0;
    player->cpuholdhi=0.300;
    player->cpuholdlo=player->cpuholdhi*0.750;
    player->cpuskiphi=0.500*(1.0-player->skill)+0.100*player->skill;
    player->cpuskiplo=player->cpuskiphi*0.500;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xd466c1ff;
        player->tileid=0xc0;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x60;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x90;
      } break;
  }
  
  /* Load up with things.
   */
  int hold=THING_LIMIT; // Count down so that they start in proper render order.
  while (player->thingc<THING_LIMIT) {
    struct thing *thing=player->thingv+player->thingc++;
    thing->tileid=random_thing_tile();
    thing->dy=0.0;
    thing->hold=hold--;
    juggling_force_thing_position(battle,player,thing);
  }
}

/* New.
 */
 
static int _juggling_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->graceclock=1.000;
  return 0;
}

/* Button on: Start tossing the handiest thing.
 * (dryryn) is for the CPU. If set, we return >0 if thrown, but do nothing and return zero if nothing is ready.
 * So the CPU controller doesn't need to check whether a thing is ready.
 */
 
static int juggling_btnon(struct battle *battle,struct player *player,int dryrun) {
  if (player->thing_active) return 0; // Shouldn't be possible.
  struct thing *thing=player->thingv;
  int i=player->thingc;
  for (;i-->0;thing++) {
    if (thing->hold==1) {
      player->thing_active=thing;
      break;
    }
  }
  if (player->thing_active) {
    bm_sound_pan(RID_sound_throw,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->thing_active->dy=player->throw_initial;
    player->thing_active->hold=0;
    for (thing=player->thingv,i=player->thingc;i-->0;thing++) {
      if (thing==player->thing_active) continue;
      if (thing->hold>0) thing->hold--;
    }
    player->gravity_rate=player->gravity_lo;
    return 1;
  } else { // Nothing ready to throw.
    if (!dryrun) {
      bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    }
    return 0;
  }
}

/* Button off: If there's an active thing, cancel its rise.
 */
 
static void juggling_btnoff(struct battle *battle,struct player *player) {
  if (player->thing_active) {
    if (player->thing_active->dy<0.0) {
      player->thing_active->dy=0.0;
    }
    player->thing_active=0;
  }
}

/* Catch a thing.
 */
 
static void juggling_catch(struct battle *battle,struct player *player,struct thing *thing) {
  // We have a "catch" sound but it's too prominent. Using the illogical "mount" because it's very passive.
  bm_sound_pan(RID_sound_mount,player->who?PLAYER_PAN:-PLAYER_PAN);
  // We get a (hold) one greater than the current max. Do not fill in gaps.
  thing->hold=1;
  struct thing *other=player->thingv;
  int i=player->thingc;
  for (;i-->0;other++) {
    if (other==thing) continue;
    if (other->hold>=thing->hold) thing->hold=other->hold+1;
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) juggling_btnon(battle,player,0);
    else if (!(input&EGG_BTN_SOUTH)&&(pvinput&EGG_BTN_SOUTH)) juggling_btnoff(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // Time remaining on the clock? Just tick it down.
  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    return;
  }
  
  /* If we're holding the button, release it and set the "skip" interval.
   */
  if (player->thing_active) {
    juggling_btnoff(battle,player);
    double n=(rand()&0xffff)/65535.0;
    player->cpuclock=player->cpuskiplo*(1.0-n)+player->cpuskiphi*n;
    return;
  }
  
  /* Try throwing the thing.
   * If it fails, don't update the clock and we'll try again next frame.
   */
  if (!juggling_btnon(battle,player,1)) return;
  double n=(rand()&0xffff)/65535.0;
  player->cpuclock=player->cpuholdlo*(1.0-n)+player->cpuholdhi*n;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Increase gravity.
   * It snaps back low when you throw something.
   */
  if (player->gravity_rate<player->gravity_hi) {
    player->gravity_rate+=10.0*elapsed;
    if (player->gravity_rate>player->gravity_hi) player->gravity_rate=player->gravity_hi;
  }
  
  /* Constants for backslide.
   */
  const double xspeed=10.0;
  double rearx=player->x;
  if (player->xform) rearx+=14.0;
  else rearx-=14.0;

  /* Per-thing updates.
   */
  struct thing *thing=player->thingv;
  int i=player->thingc;
  for (;i-->0;thing++) {
  
    // If held, slide toward the ideal position and that's all.
    if (thing->hold) {
      juggling_slide_thing_position(battle,player,thing,elapsed);
      continue;
    }
    
    // Slide toward the intake position (player's rear).
    // ...ew
    if (!player->lost) {
      if (player->xform) {
        if ((thing->x+=xspeed*elapsed)>=rearx) thing->x=rearx;
      } else {
        if ((thing->x-=xspeed*elapsed)<=rearx) thing->x=rearx;
      }
    }
    
    // Apply gravity. Upward motion is just negative gravity.
    thing->dy+=player->gravity_rate*elapsed;
    if (thing->dy>=player->gravity_limit) thing->dy=player->gravity_limit;
    thing->y+=thing->dy*elapsed;
    
    // If lost, everything falls to the floor. Otherwise they fall into my arms.
    if (thing->dy>0.0) {
      double bottom=player->lost?GROUNDY:player->y;
      if (thing->y>=bottom) {
        thing->y=bottom;
        if (!player->lost) {
          juggling_catch(battle,player,thing);
        }
      }
    }
  }
  
  /* Earn undesirable points for all things held.
   */
  if (!player->lost&&(battle->outcome==-2)) {
    for (thing=player->thingv,i=player->thingc;i-->0;thing++) {
      if (!thing->hold) continue;
      player->score+=player->score_rate*elapsed;
    }
    if (player->score>=1.0) {
      player->lost=1;
      bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
      for (thing=player->thingv,i=player->thingc;i-->0;thing++) thing->hold=0;
    }
  }
}

/* Update.
 */
 
static void _juggling_update(struct battle *battle,double elapsed) {

  if (BATTLE->graceclock>0.0) BATTLE->graceclock-=elapsed;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->lost||(battle->outcome>-2)) {
      if (player->thing_active) juggling_btnoff(battle,player);
    } else if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->lost&&r->lost) battle->outcome=0;
    else if (l->lost) battle->outcome=-1;
    else if (r->lost) battle->outcome=1;
  }

  //XXX
  if (g_input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  
  // Layout.
  int xmid=player->x,ymid=player->y;
  int xbak,xfor,ytop=ymid-NS_sys_tilesize,ybtm=ymid+NS_sys_tilesize;
  if (player->xform) {
    xbak=xmid+NS_sys_tilesize;
    xfor=xmid-NS_sys_tilesize;
  } else {
    xbak=xmid-NS_sys_tilesize;
    xfor=xmid+NS_sys_tilesize;
  }
  
  // Arm.
  uint8_t tileid_arm=player->tileid+3;
  if (player->thing_active) tileid_arm+=0x10;
  int armx=xmid,army=ymid+7;
  if (player->xform) armx-=15; else armx+=15;
  graf_tile(g_graf,armx,army,tileid_arm,player->xform);
  
  // Body. 3x3 tiles.
  uint8_t tileid=player->tileid;
  if (player->lost) tileid+=4;
  graf_tile(g_graf,xbak,ytop,tileid+0x00,player->xform);
  graf_tile(g_graf,xmid,ytop,tileid+0x01,player->xform);
  graf_tile(g_graf,xfor,ytop,tileid+0x02,player->xform);
  graf_tile(g_graf,xbak,ymid,tileid+0x10,player->xform);
  graf_tile(g_graf,xmid,ymid,tileid+0x11,player->xform);
  graf_tile(g_graf,xfor,ymid,tileid+0x12,player->xform);
  graf_tile(g_graf,xbak,ybtm,tileid+0x20,player->xform);
  graf_tile(g_graf,xmid,ybtm,tileid+0x21,player->xform);
  graf_tile(g_graf,xfor,ybtm,tileid+0x22,player->xform);
  
  // Things.
  struct thing *thing=player->thingv;
  int i=player->thingc;
  for (;i-->0;thing++) {
    graf_tile(g_graf,(int)thing->x,(int)thing->y,thing->tileid,0);
  }
}

/* Score meters.
 */
 
static void juggling_render_bar(struct battle *battle,struct player *player) {
  const int barw=100,barh=4;
  int bary=GROUNDY+7;
  int barx=player->who?((FBW>>1)+4):((FBW>>1)-4-barw);
  graf_fill_rect(g_graf,barx-1,bary-1,barw+2,barh+2,0x000000ff);
  int fillw=(int)(player->score*barw);
  if (fillw>0) {
    if (fillw>barw) fillw=barw;
    int fillx=player->who?(barx+barw-fillw):barx;
    graf_fill_rect(g_graf,fillx,bary,fillw,barh,player->color);
  }
}

/* Render.
 */
 
static void _juggling_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;

  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,1,0x000000ff);
  
  graf_set_image(g_graf,RID_image_battle_meadow2);
  player_render(battle,l);
  player_render(battle,r);
  
  /* Horizontal bars below our feet showing each player's score.
   * They rise toward the center.
   */
  juggling_render_bar(battle,l);
  juggling_render_bar(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_juggling={
  .name="juggling",
  .objlen=sizeof(struct battle_juggling),
  .id=132,
  .strix_name=427,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .imageid_default=0,
  .del=_juggling_del,
  .init=_juggling_init,
  .update=_juggling_update,
  .render=_juggling_render,
};

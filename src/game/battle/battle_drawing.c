/* battle_drawing.c
 * Rules:
 *  - Hold A to enter the READY state. Has a more alert appearance.
 *  - Release to fire your dart.
 *  - First to release can do it whenever, everything is driven by the second release.
 *  - A player that never releases loses, obviously.
 *  - Winner is chosen based on the time difference between releases.
 *  - When playing against the CPU, the human will always lose if he draws first.
 */

#include "game/batsup/battle_internal.h"

#define GROUNDY 130

#define STATE_START 1 /* Initial state, game hasn't really started yet. */
#define STATE_READY 2 /* Holding A. */
#define STATE_DRAWN 3 /* Fired. The winner stays in this state forever. */
#define STATE_HURT 4 /* Hit by other player's dart because we never drew. */
#define STATE_FOUL 5 /* Drew before the other player was ready. */

struct battle_drawing {
  struct battle hdr;
  double playtime; // Counts up.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    int x,y; // Center of 2x2-tile sprite.
    int blackout;
    int state;
    double drawtime;
    double cpuclock; // Counts down to the next state change.
    int cpuresponding; // Nonzero if we've acknowledged the other guy's draw.
    double threshold; // s, how long after the other guy draws can i draw for a win.
    int labeltexid,labelw,labelh;
  } playerv[2];
  
  struct dart {
    int who;
    double x,y;
    double dx;
    int captured; // If nonzero, I'm the only dart (the left one), and I'm falling to the ground.
    int finished; // Nonzero when a captured dart hits the floor.
  } dartv[2];
  int dartc;
};

#define BATTLE ((struct battle_drawing*)battle)

/* Delete.
 */
 
static void _drawing_del(struct battle *battle) {
  egg_texture_del(BATTLE->playerv[0].labeltexid);
  egg_texture_del(BATTLE->playerv[1].labeltexid);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->y=GROUNDY-NS_sys_tilesize+1;
  player->x=30;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  
  player->state=STATE_START;
  // I hit right around 200ms every single time.
  player->threshold=0.200*(1.0-player->skill)+0.300*player->skill;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    // Set a timer before entering READY, just for aesthetic purposes.
    double n=(rand()&0xffff)/65535.0;
    player->cpuclock=1.000*(1.0-n)+2.000*n;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xad7636ff;
        player->tileid=0x5d;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x59;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x5b;
      } break;
  }
}

/* New.
 */
 
static int _drawing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Set a player's label.
 */
 
static void drawing_set_label(struct battle *battle,struct player *player,int strix,int arg) {
  struct text_insertion ins={.mode='i',.i=arg};
  char tmp[32];
  int tmpc=text_format_res(tmp,sizeof(tmp),RID_strings_battle,strix,&ins,1);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return;
  player->labeltexid=font_render_to_texture(player->labeltexid,g_font,tmp,tmpc,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY_TEXT]);
  egg_texture_get_size(&player->labelw,&player->labelh,player->labeltexid);
}

/* Enter READY state, ie press the button.
 */
 
static void drawing_READY(struct battle *battle,struct player *player) {
  if (player->state!=STATE_START) return;
  player->state=STATE_READY;
}

/* Enter DRAWN state, ie release the button and fire my dart.
 */
 
static void drawing_DRAWN(struct battle *battle,struct player *player) {
  if (player->state!=STATE_READY) return;
  
  /* If the other guy is still in START, it's a foul and you lose.
   * But we do proceed with firing the shot, just to be consistent.
   */
  struct player *other=BATTLE->playerv+(player->who?0:1);
  if (other->state==STATE_START) {
    player->state=STATE_FOUL;
    drawing_set_label(battle,player,411,0);
  } else {
    player->state=STATE_DRAWN;
  }
  
  bm_sound_pan(RID_sound_whack,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->drawtime=BATTLE->playtime;
  
  if (BATTLE->dartc<2) {
    struct dart *dart=BATTLE->dartv+BATTLE->dartc++;
    dart->who=player->who;
    dart->y=player->y;
    dart->x=player->x;
    if (player->xform) dart->x-=NS_sys_tilesize;
    else dart->x+=NS_sys_tilesize;
    dart->dx=200.0;
    if (player->xform) dart->dx=-dart->dx;
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else switch (player->state) {
    case STATE_START: if (input&EGG_BTN_SOUTH) drawing_READY(battle,player); break;
    case STATE_READY: if (!(input&EGG_BTN_SOUTH)) drawing_DRAWN(battle,player); break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  struct player *other=BATTLE->playerv+(player->who?0:1);
  
  /* When we're playing against a human:
   *  - Wait for READY at the interval established at init.
   *  - Don't fire until the other guy is READY.
   *  - If the other guy fires, we fire immediately: Human must wait.
   *  - Fire when a meaningless clock expires.
   */
  if (other->human) {
    switch (player->state) {
      case STATE_START: {
          if ((player->cpuclock-=elapsed)<=0.0) {
            drawing_READY(battle,player);
            double n=(rand()&0xffff)/65535.0;
            player->cpuclock=2.000*(1.0-n)+4.000*n; // Give them long enough to sweat a little.
          }
        } break;
      case STATE_READY: {
          if ((other->state==STATE_DRAWN)&&(other->drawtime<BATTLE->playtime)) { // Human drew first. They lose. My reflexes are unbeatable.
            drawing_DRAWN(battle,player);
          } else if (other->state==STATE_READY) { // Run the clock only when they're ready.
            if ((player->cpuclock-=elapsed)<=0.0) {
              drawing_DRAWN(battle,player);
            }
          }
        } break;
    }
    
  /* When we're playing against another CPU:
   *  - Wait for READY at the interval established at init.
   *  - Don't fire until the other guy is READY.
   *  - Fire if our clock expires, doesn't really matter who fires first.
   *  - If we're READY and the other guy has fired, reset my clock such that my odds are proportionate to my skill.
   */
  } else {
    switch (player->state) {
      case STATE_START: {
          if ((player->cpuclock-=elapsed)<=0.0) {
            drawing_READY(battle,player);
            double n=(rand()&0xffff)/65535.0;
            player->cpuclock=2.000*(1.0-n)+4.000*n; // Give them long enough to sweat a little.
          }
        } break;
      case STATE_READY: {
          // He drew and we haven't reacted yet. React now. This decides who's going to win.
          if ((other->state==STATE_DRAWN)&&!player->cpuresponding&&(other->drawtime<BATTLE->playtime)) {
            double target=other->drawtime+player->threshold;
            if ((rand()&0xffff)/65535.0<player->skill) { // I win.
              player->cpuclock=player->threshold-60.0;
            } else { // I lose.
              player->cpuclock=player->threshold+60.0;
            }
          }
          // Beyond that, we just tick down the clock and then draw.
          if ((player->cpuclock-=elapsed)<=0.0) {
            drawing_DRAWN(battle,player);
          }
        } break;
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
}

/* Rewrite (survivor) with the combined state for a captured dart.
 * (trigger) is going to be deleted.
 * (survivor) will be configured to move toward the battle's loser.
 */
 
static void drawing_combine_darts(struct battle *battle,struct dart *survivor,struct dart *trigger) {
  bm_sound_pan(RID_sound_weebump,0.0);

  // (splayer,tplayer): The player associated with each dart.
  struct player *splayer=BATTLE->playerv+survivor->who;
  struct player *tplayer=BATTLE->playerv+trigger->who;
  
  // Which one drew first?
  struct player *initiator=0,*follower=0;
  if (splayer->drawtime<tplayer->drawtime) {
    initiator=splayer;
    follower=tplayer;
  } else {
    initiator=tplayer;
    follower=splayer;
  }
  
  // Who wins?
  double windx;
  double interval=follower->drawtime-initiator->drawtime;
  int rptms=(int)(interval*1000.)+(rand()%11-5); // Randomize reported interval so it doesn't always look like a multiple of 16.
  if (rptms<1) rptms=1;
  drawing_set_label(battle,initiator,409,0);
  drawing_set_label(battle,follower,410,rptms);
  if (interval<=follower->threshold) { // Follower wins.
    if (survivor->who==follower->who) windx=survivor->dx;
    else windx=trigger->dx;
  } else { // Initiator wins.
    if (survivor->who==initiator->who) windx=survivor->dx;
    else windx=trigger->dx;
  }

  // If (trigger) is on the left, copy its position and owner to (survivor).
  if (trigger->x<survivor->x) {
    survivor->x=trigger->x;
    survivor->who=trigger->who;
  }
  survivor->dx=windx*0.250;
  survivor->captured=1;
}

/* Update one dart.
 * Return <0 to remove the dart (if it struck a player).
 * Return >0 if still active, keeps the battle running.
 */
 
static int dart_update(struct battle *battle,struct dart *dart,double elapsed) {

  // If finished, we're finished.
  if (dart->finished) return 0;

  dart->x+=dart->dx*elapsed;

  /* If captured, fall to the ground.
   */
  if (dart->captured) {
    if ((dart->y+=100.0*elapsed)>=GROUNDY) {
      dart->y=GROUNDY;
      dart->finished=1;
      bm_sound_pan(RID_sound_weebump,0.0);
    }
    
  /* Not captured.
   * Set (y) based on (x).
   * Check the other dart if there is one.
   */
  } else {
    if (BATTLE->dartc>=2) {
      struct dart *other=BATTLE->dartv;
      if (other==dart) other++; // It's either the first or the second, whichever I'm not.
      if (
        ((dart->dx>0.0)&&(dart->x>=other->x-NS_sys_tilesize))||
        ((dart->dx<0.0)&&(dart->x<=other->x+NS_sys_tilesize))
      ) {
        drawing_combine_darts(battle,other,dart);
        return -1;
      }
    }
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    // (y) is based entirely on (x), describing an arc.
    const double peak=40.0;
    double radius=(r->x-NS_sys_tilesize)-FBW*0.5;
    double t=(dart->x-FBW*0.5)/radius;
    t=1.0-t*t;
    dart->y=l->y-t*peak;
    // Did we hurt somebody?
    if ((dart->dx<0.0)&&(dart->x<l->x+NS_sys_tilesize)) {
      bm_sound_pan(RID_sound_ouch,-PLAYER_PAN);
      l->state=STATE_HURT;
      return -1;
    }
    if ((dart->dx>0.0)&&(dart->x>r->x-NS_sys_tilesize)) {
      bm_sound_pan(RID_sound_ouch,PLAYER_PAN);
      r->state=STATE_HURT;
      return -1;
    }
  }
  return 1;
}

/* Update.
 */
 
static void _drawing_update(struct battle *battle,double elapsed) {
  
  BATTLE->playtime+=elapsed;
  
  /* Players don't update at all once outcome is established.
   */
  if (battle->outcome==-2) {
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
      else player_update_cpu(battle,player,elapsed);
      player_update_common(battle,player,elapsed);
    }
  }
  
  /* Darts update until they're dead.
   */
  int darts_active=0;
  if (BATTLE->dartc) {
    int i=BATTLE->dartc;
    struct dart *dart=BATTLE->dartv+i-1;
    for (;i-->0;dart--) {
      int err=dart_update(battle,dart,elapsed);
      if (err<0) { // Remove.
        BATTLE->dartc--;
        memmove(dart,dart+1,sizeof(struct dart)*(BATTLE->dartc-i));
      } else if (err>0) { // Active.
        darts_active=1;
      }
    }
  }
  
  /* Check for outcome.
   * If anybody is in FOUL, they lose.
   * Next if anybody is in HURT, they lose.
   * Otherwise, the darts must be at rest and the winner is based on their resting position.
   * (in truth it's based on player->drawtime, but the darts already recorded that for us).
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if ((l->state==STATE_FOUL)&&(r->state==STATE_FOUL)) battle->outcome=0; // Double-foul shouldn't be possible but there might be an edge where it is.
    else if (l->state==STATE_FOUL) battle->outcome=-1;
    else if (r->state==STATE_FOUL) battle->outcome=1;
    else if ((l->state==STATE_HURT)&&(r->state==STATE_HURT)) battle->outcome=0; // Double-hurt definitely shouldn't be possible, just being consistent here.
    else if (l->state==STATE_HURT) battle->outcome=-1;
    else if (r->state==STATE_HURT) battle->outcome=1;
    else if ((l->state==STATE_DRAWN)&&(r->state==STATE_DRAWN)&&!darts_active) {
      if ((BATTLE->dartc==1)&&(BATTLE->dartv[0].finished)) {
        battle->outcome=(BATTLE->dartv[0].dx>0.0)?1:-1;
      }
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int forex=player->x,backx=player->x,topy=player->y-(NS_sys_tilesize>>1);
  int btmy=topy+NS_sys_tilesize;
  if (player->xform) {
    forex-=NS_sys_tilesize>>1;
    backx+=NS_sys_tilesize>>1;
  } else {
    forex+=NS_sys_tilesize>>1;
    backx-=NS_sys_tilesize>>1;
  }
  uint8_t tileid=player->tileid;
  switch (player->state) {
    case STATE_READY: tileid+=0x20; break;
    case STATE_DRAWN:
    case STATE_FOUL: tileid+=0x40; break;
    case STATE_HURT: tileid+=0x60; break;
  }
  graf_tile(g_graf,backx,topy,tileid+0x00,player->xform);
  graf_tile(g_graf,forex,topy,tileid+0x01,player->xform);
  graf_tile(g_graf,backx,btmy,tileid+0x10,player->xform);
  graf_tile(g_graf,forex,btmy,tileid+0x11,player->xform);
}

/* Render a dart.
 */
 
static void dart_render(struct battle *battle,struct dart *dart) {
  int x=(int)dart->x;
  int y=(int)dart->y;
  uint8_t xform=BATTLE->playerv[dart->who].xform;
  graf_tile(g_graf,x,y,0x58,xform);
  if (dart->captured) { // I'm the left side. Draw my partner.
    graf_tile(g_graf,x+9,y,0x58,EGG_XFORM_XREV);
  }
}

/* Render one player's label.
 */

static void label_render(struct battle *battle,struct player *player) {
  if ((player->labelw<1)||(player->labelh<1)) return;
  graf_set_input(g_graf,player->labeltexid);
  graf_decal(g_graf,
    player->x-(player->labelw>>1),
    player->y-NS_sys_tilesize-4-player->labelh,
    0,0,player->labelw,player->labelh
  );
}

/* Render.
 */
 
static void _drawing_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,1,0x000000ff);
  
  graf_set_image(g_graf,RID_image_battle_desert);
  player_render(battle,l);
  player_render(battle,r);
  
  struct dart *dart=BATTLE->dartv;
  int i=BATTLE->dartc;
  for (;i-->0;dart++) dart_render(battle,dart);
  
  /* If either player has a label, render it above them.
   */
  label_render(battle,l);
  label_render(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_drawing={
  .name="drawing",
  .objlen=sizeof(struct battle_drawing),
  .id=124,
  .strix_name=408,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .imageid_default=RID_image_desert,
  .del=_drawing_del,
  .init=_drawing_init,
  .update=_drawing_update,
  .render=_drawing_render,
};

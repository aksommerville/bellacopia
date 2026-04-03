/* battle_cakecarrying.c
 * Balance a cake while standing on a train that's stopping.
 */

#include "game/bellacopia.h"

#define LAYERC 4
#define CAR_COLC 17
#define CAR_ROWC 6
#define GROUNDY 134
#define CPU_PENALTY 0.750 /* CPU players walk slower than humans. */

struct battle_cakecarrying {
  struct battle hdr;
  int choice;
  double trackphase; // 0..1
  int wheelframe;
  double wheelclock;
  double bumpclock;
  double bumpinterval;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid;
    uint8_t xform;
    double x,y; // Bottom center, in framebuffer pixels.
    double walkspeed; // px/s
    int indx,injump;
    int jumpok;
    int seated; // We remain seated across the bumps, only jumping nixes it.
    double gravity;
    double jumpforce;
    double jump_decel; // constant, positive px/s**2
    double jumpforce_initial; // constant, positive px/s
    int outcome;
    double fireclock;
    int fireframe;
    
    // CPU player:
    double reflexclock; // Must count down to zero before changing input state.
    double rxonlo,rxonhi,rxofflo,rxoffhi; // Reflex constants, governs difficulty.
    
    /* Layers are sorted bottom to top.
     */
    struct layer {
      double x,y; // Absolute position, framebuffer pixels.
      double offset; // Resting value for (y), relative to the prior layer.
      double w,h;
      uint8_t tileid;
      int falling;
    } layerv[LAYERC];
  } playerv[2];
};

#define BATTLE ((struct battle_cakecarrying*)battle)

static const uint8_t car_tiles[CAR_COLC*CAR_ROWC]={
  0x55,0x56,0x56,0x56,0x56,0x56,0x56,0x56,0x58,0x56,0x56,0x56,0x56,0x56,0x56,0x56,0x57,
  0x65,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x68,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x67,
  0x65,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x68,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x67,
  0x65,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x68,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x67,
  0x65,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x68,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x67,
  0x75,0x76,0x76,0x76,0x76,0x76,0x76,0x76,0x78,0x76,0x76,0x76,0x76,0x76,0x76,0x76,0x77,
};

/* Delete.
 */
 
static void _cakecarrying_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->walkspeed=50.0;
  player->y=GROUNDY;
  player->jumpok=0; // Guilty until proven innocent, so the keystroke that started the game isn't also a Jump.
  player->jump_decel=500.0;
  player->jumpforce_initial=150.0;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=40.0;
  } else { // Right.
    player->who=1;
    player->x=FBW-40.0;
    player->xform=EGG_XFORM_XREV;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    player->walkspeed*=CPU_PENALTY;
    // Reflex times are inverse to skill.
    // The "off" time should be faster than the "on" time. Too much "off" makes the player always lose.
    player->rxonhi=(1.0-player->skill)*0.750;
    player->rxoffhi=(1.0-player->skill)*0.040;
    player->rxonlo=player->rxonhi*0.5;
    player->rxofflo=player->rxoffhi*0.5;
    // The constants can't be zero:
    player->rxonlo+=0.001;
    player->rxonhi+=0.001;
    player->rxofflo+=0.001;
    player->rxoffhi+=0.001;
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x74;
      } break;
    case NS_face_dot: {
        player->tileid=0x34;
      } break;
    case NS_face_princess: {
        player->tileid=0x54;
      } break;
  }
  double cakex=player->x;
  if (player->xform) cakex-=4.0;
  else cakex+=4.0;
  player->layerv[0]=(struct layer){cakex,player->y-12.0,  0.0,11.0,8.0,0xb9};
  player->layerv[1]=(struct layer){cakex,player->y-19.0, -7.0, 9.0,6.0,0xba};
  player->layerv[2]=(struct layer){cakex,player->y-25.0, -6.0, 6.0,6.0,0xbb};
  player->layerv[3]=(struct layer){cakex,player->y-31.0, -6.0, 4.0,6.0,0xbc};
}

/* New.
 */
 
static int _cakecarrying_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->bumpinterval=0.800;
  BATTLE->bumpclock=BATTLE->bumpinterval-0.300;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (g.input[player->human]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0; break;
  }
  player->injump=(g.input[player->human]&EGG_BTN_SOUTH)?1:0;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* First, so we're not repeating ourselves too much, decide which direction to walk.
   * Always toward the goal. But if we're already there, do nothing.
   */
  int dx=0;
  if (player->who) {
    if (player->x>FBW*0.45) dx=-1;
  } else {
    if (player->x<FBW*0.55) dx=1;
  }
  if (!dx) {
    player->indx=0;
    return;
  }

  /* The most naive play is just walk constantly toward the goal.
   * A player doing this will always lose.
   *
  player->indx=dx;
  /**/
  
  /* Determine whether our stack is stable.
   * (ndx) is the desired new dpad state, for perfect play.
   * Don't just plop that in (indx) immediately; that player would be unbeatable.
   */
  int stable=1;
  double expecty=player->y-12.0;
  struct layer *layer=player->layerv;
  int li=LAYERC;
  for (;li-->0;layer++) {
    if (layer->falling) { stable=0; break; }
    expecty+=layer->offset;
    if (layer->y<expecty) { stable=0; break; }
    expecty=layer->y;
  }
  int ndx=stable?dx:0;
  
  /* If (ndx) matches (indx), we're already doing the right thing and should keep doing it.
   */
  if (ndx==player->indx) return;
  
  /* Establish (reflexclock) if currently unset.
   * Or if set, tick it down.
   * Proceed only when it was set, and has expired.
   * This arrangement means we're always at least one frame short of perfect play.
   */
  if (player->reflexclock<=0.0) {
    double n=(rand()&0xffff)/65535.0;
    if (ndx) player->reflexclock=n*player->rxonhi+(1.0-n)*player->rxonlo;
    else player->reflexclock=n*player->rxoffhi+(1.0-n)*player->rxofflo;
    return;
  }
  if ((player->reflexclock-=elapsed)>0.0) return;
  
  /* Change input state to the ideal.
   */
  player->indx=ndx;
}

/* Cakes' positions are pegged to their player.
 * Call this when the player has moved.
 * Cakes without perfect bottom friction may move up to dx.
 */
 
static void player_move_cakes(struct battle *battle,struct player *player,double dx) {
  double friction=1.0;
  double expecty=player->y-12.0;
  struct layer *layer=player->layerv;
  int i=LAYERC;
  for (;i-->0;layer++) {
    if (layer->falling) break;
    expecty+=layer->offset;
    if (layer->y<expecty) {
      friction-=(expecty-=layer->y);
      if (friction<0.0) friction=0.0;
    }
    expecty=layer->y;
    if (friction>=1.0) layer->x+=dx;
    else if (friction<=0.0) break;
    else layer->x+=dx*(1.0-friction);
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* If the battle is over, all inputs are zeroed.
   * Likewise, if this player is in the victory state, nix it.
   * If you've lost but it's not official yet, you can keep moving.
   * We do continue updating to the bitter end.
   */
  if ((battle->outcome>-2)||(player->outcome==1)) {
    player->indx=0;
    player->injump=0;
  }
  
  // Animate the candle, whether it's lit or not.
  if ((player->fireclock-=elapsed)<=0.0) {
    player->fireclock+=0.150;
    if (++(player->fireframe)>=4) player->fireframe=0;
  }
  
  // Walk horizontally.
  if (player->indx) {
    double x0=player->x;
    player->x+=player->walkspeed*player->indx*elapsed;
    if (player->x<33.0) player->x=33.0;
    else if (player->x>288.0) player->x=288.0;
    double dx=player->x-x0;
    player_move_cakes(battle,player,dx);
  }
  
  // Jump if we're jumping.
  if (player->jumpforce>0.0) {
    if (!player->injump) {
      player->jumpforce=0.0;
    } else {
      player->jumpforce-=player->jump_decel*elapsed;
      double dy=-player->jumpforce*elapsed;
      player->y+=dy;
      double expecty=player->y-12.0;
      struct layer *layer=player->layerv;
      int i=LAYERC;
      for (;i-->0;layer++) {
        if (layer->falling) break;
        expecty+=layer->offset;
        if (layer->y>=expecty) layer->y+=dy;
        expecty=layer->y;
      }
    }
    
  // Start a new jump?
  } else if (player->seated&&player->jumpok&&player->injump) {
    player->jumpforce=player->jumpforce_initial;
    player->jumpok=0;
    player->seated=0;
    bm_sound_pan(RID_sound_jump,player->who?0.250:-0.250);
  
  // Fall to the ground, if we're not there.
  } else {
    if ((player->gravity+=elapsed*100.0)>=60.0) player->gravity=60.0;
    double y0=player->y;
    player->y+=player->gravity*elapsed;
    if (player->y>=GROUNDY) {
      player->y=GROUNDY;
      if (!player->injump) player->jumpok=1;
      player->seated=1;
    }
    double dy=player->y-y0;
    if (dy>0.0) {
      struct layer *layer=player->layerv;
      int i=LAYERC;
      for (;i-->0;layer++) {
        if (dy<=0.0) break;
        layer->y+=dy;
        dy-=0.500;
      }
    } else {
      player->gravity=0.0;
    }
  }
  
  /* Cake layers slowly approach their resting elevation, or fall free if misaligned with the next layer down.
   * The bottom layer can't fall.
   */
  struct layer *layer=player->layerv;
  int i=LAYERC;
  double expecty=player->y-12.0;
  const double layerspeed_down=25.0;
  const double layerspeed_up  = 5.0;
  double cakel=0.0,caker=FBW;
  for (;i-->0;layer++) {
    if (layer->falling||(layer->x+layer->w*0.5<cakel)||(layer->x-layer->w*0.5>caker)) {
      expecty=GROUNDY;
      layer->falling=1;
    } else {
      expecty+=layer->offset;
    }
    if (layer->y<expecty) {
      if ((layer->y+=layerspeed_down*elapsed)>=expecty) layer->y=expecty;
    } else if (layer->y>expecty) {
      if ((layer->y-=layerspeed_up*elapsed)<=expecty) layer->y=expecty;
    }
    expecty=layer->y;
    cakel=layer->x-layer->w*0.5;
    caker=layer->x+layer->w*0.5;
  }
}

/* Bump both players.
 */
 
static void cakecarrying_bump_players(struct battle *battle) {
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->y<GROUNDY) continue; // Already in the air, bumping has no impact.
    player->y-=3.0;
    struct layer *layer=player->layerv;
    int i=LAYERC;
    double dy=-3.0;
    for (;i-->0;layer++,dy-=1.000) {
      layer->y+=dy;
    }
  }
}

/* Check stability.
 * Any falling cakes are allowed to hit the ground before we report the loss.
 */
 
static int cakecarrying_is_stable(struct battle *battle) {
  struct player *player=BATTLE->playerv;
  int pi=2;
  for (;pi-->0;player++) {
    if (!player->seated) return 0; // Jumping. Let her land first.
    double expecty=player->y-12.0;
    struct layer *layer=player->layerv;
    int li=LAYERC;
    for (;li-->0;layer++) {
      if (layer->falling) expecty=GROUNDY;
      else expecty+=layer->offset;
      if (layer->y<expecty) return 0;
      expecty=layer->y;
    }
  }
  return 1;
}

/* Check outcome for one player.
 * We don't care about stability.
 */
 
static int cakecarrying_check_outcome(struct battle *battle,struct player *player) {
  if (player->outcome) return player->outcome; // You shouldn't have called.
  // If any layer is falling, we lose. Falling is permanent, it can't be reversed.
  struct layer *layer=player->layerv;
  int i=LAYERC;
  for (;i-->0;layer++) {
    if (layer->falling) return -1;
  }
  // If we're past the horizontal midpoint, we win.
  if (player->who) {
    if (player->x<FBW*0.5) return 1;
  } else {
    if (player->x>FBW*0.5) return 1;
  }
  return 0;
}

/* Update.
 */
 
static void _cakecarrying_update(struct battle *battle,double elapsed) {

  /* Simulate the train's motion.
   */
  BATTLE->trackphase+=elapsed*8.0;
  while (BATTLE->trackphase>=1.0) BATTLE->trackphase-=1.0;
  if ((BATTLE->wheelclock-=elapsed)<=0.0) {
    BATTLE->wheelclock+=0.060;
    if (++(BATTLE->wheelframe)>=4) BATTLE->wheelframe=0;
  }
  
  /* Bump every so often.
   */
  if ((BATTLE->bumpclock-=elapsed)<=0.0) {
    BATTLE->bumpclock+=BATTLE->bumpinterval;
    bm_sound(RID_sound_trainbump);
    cakecarrying_bump_players(battle);
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Each player has an independent victory state.
   * Once victory is determined, we wait for the cakes to settle.
   * If a player reaches their victory state (but not loss state), they're forbidden from moving further.
   * That's important. It means once you win, you can't un-win.
   */
  if (battle->outcome>-2) {
    // Outcome already established, can't change it.
  } else {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (!l->outcome) l->outcome=cakecarrying_check_outcome(battle,l);
    if (!r->outcome) r->outcome=cakecarrying_check_outcome(battle,r);
    // If anyone has a victory, the game ends immediately.
    if ((l->outcome>0)&&(r->outcome>0)) battle->outcome=0;
    else if (l->outcome>0) battle->outcome=1;
    else if (r->outcome>0) battle->outcome=-1;
    // If they're both undetermined, do nothing.
    else if (!l->outcome&&!r->outcome) ;
    // Somebody lost. Wait until the cakes stabilize, then report it.
    else if (cakecarrying_is_stable(battle)) {
      if ((l->outcome<0)&&(r->outcome<0)) battle->outcome=0;
      else if (l->outcome<0) battle->outcome=-1;
      else if (r->outcome<0) battle->outcome=1;
    }
  }
}

/* Render player.
 * The body and the cake are separate to promote batching, because they'll use tiles and fancies respectively.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int heady=(int)player->y;
  heady-=NS_sys_tilesize+(NS_sys_tilesize>>1);
  graf_tile(&g.graf,player->x,heady,player->tileid,player->xform);
  graf_tile(&g.graf,player->x,heady+NS_sys_tilesize,player->tileid+0x10,player->xform);
}

static void cake_render(struct battle *battle,struct player *player) {
  struct layer *layer=player->layerv;
  int i=LAYERC;
  for (;i-->0;layer++) {
    graf_tile(&g.graf,(int)layer->x,(int)layer->y,layer->tileid,0);
  }
  // Candle lights up after victory.
  if (player->outcome>0) {
    layer=player->layerv+LAYERC-1;
    int x=layer->x;
    int y=layer->y-9;
    uint8_t tileid=0xbd;
    uint8_t xform=0;
    switch (player->fireframe) {
      case 1: tileid++; break;
      case 2: xform=EGG_XFORM_XREV; break;
      case 3: tileid++; xform=EGG_XFORM_XREV; break;
    }
    graf_tile(&g.graf,x,y,tileid,xform);
  }
}

/* Render.
 */
 
static void _cakecarrying_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_fractia);
  
  /* Track.
   */
  int x=-(int)(BATTLE->trackphase*NS_sys_tilesize);
  int y=150;
  int xz=FBW+NS_sys_tilesize;
  for (;x<xz;x+=NS_sys_tilesize) {
    graf_tile(&g.graf,x,y,0xaa,0);
  }
  
  /* Body of the car.
   */
  const int carw=CAR_COLC*NS_sys_tilesize;
  const int carh=CAR_ROWC*NS_sys_tilesize;
  int carx0=(FBW>>1)-(carw>>1)+(NS_sys_tilesize>>1);
  int cary=144-carh+(NS_sys_tilesize>>1);
  if (BATTLE->bumpclock>BATTLE->bumpinterval-0.200) cary--;
  const uint8_t *src=car_tiles;
  int yi=CAR_ROWC;
  for (;yi-->0;cary+=NS_sys_tilesize) {
    int carx=carx0;
    int xi=CAR_COLC;
    for (;xi-->0;carx+=NS_sys_tilesize,src++) {
      graf_tile(&g.graf,carx,cary,*src,0);
    }
  }
  
  //TODO probly should have a neighbor car on each side
  
  /* Wheels.
   */
  uint8_t tileid=0xab+BATTLE->wheelframe;
  graf_tile(&g.graf,60,144,tileid,0);
  graf_tile(&g.graf,260,144,tileid,0);
  
  /* Right player first, because the human is left in one-player mode.
   */
  player_render(battle,BATTLE->playerv+1);
  cake_render(battle,BATTLE->playerv+1);
  player_render(battle,BATTLE->playerv+0);
  cake_render(battle,BATTLE->playerv+0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_cakecarrying={
  .name="cakecarrying",
  .objlen=sizeof(struct battle_cakecarrying),
  .strix_name=154,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_cakecarrying_del,
  .init=_cakecarrying_init,
  .update=_cakecarrying_update,
  .render=_cakecarrying_render,
};

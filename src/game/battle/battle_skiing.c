/* battle_skiing.c
 */

#include "game/bellacopia.h"

#define FLAG_LIMIT 32
#define TRAIL_LIMIT 128
#define SAMPLE_TIME 0.200

struct battle_skiing {
  struct battle hdr;
  double helloclock;
  
  struct egg_render_tile finnishline[NS_sys_mapw];
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double x,y; // In framebuffer pixels.
    int turn; // -1,0,1. Per controller.
    int turn_sticky; // Zero only if you've never turned.
    double dx_down,dy_down,dx_side,dy_side; // Speeds at the limit for downward and sideward, per skill. All positive.
    double slide_down,slide_side; // Rate of change toward each direction, positive hz, per skill.
    double sideness; // Slider between "down" and "side" velocities.
    double racetime; // Time between hello and done.
    int score; // How many flags. We could count in (flagv) but why not track it here too.
    int done;
    
    struct trail {
      int lx,rx,y;
    } trailv[TRAIL_LIMIT];
    int trailc;
    double sampleclock;
  } playerv[2];
  
  struct flag {
    double x,y; // Frambuffer pixels, center of the pair. Each flag object is two visible flags.
    uint32_t color; // 0x808080ff if unassigned. Only the first player visiting a flag wins it.
  } flagv[FLAG_LIMIT];
  int flagc;
};

#define BATTLE ((struct battle_skiing*)battle)

/* Delete.
 */
 
static void _skiing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.5-50.0;
  } else { // Right.
    player->who=1;
    player->x=FBW*0.5+50.0;
  }
  player->y=20.0;
  
  player->dx_down=0.0;
  player->dy_down= 40.0*(1.0-player->skill)+ 80.0*player->skill;
  player->dx_side= 30.0*(1.0-player->skill)+ 50.0*player->skill;
  player->dy_side= 10.0*(1.0-player->skill)+  2.0*player->skill;
  player->slide_down= 1.0*(1.0-player->skill)+ 2.0*player->skill;
  player->slide_side= 1.0*(1.0-player->skill)+ 2.0*player->skill;
  
  if (player->human=human) { // Human.
  } else { // CPU.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x965e15ff;
        player->tileid=0xa5;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x85;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x95;
      } break;
  }
}

/* New.
 */
 
static int _skiing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->helloclock=3.0;
  
  /* Generate flags.
   * In the spirit of fairness, we want an equal count left and right.
   * Easy way to effect that: Uniform horizontal spacing.
   */
  int flagc=5;
  const double usew=FBW*0.500;
  double flagspacing=usew/(double)flagc;
  double flagx=(FBW-usew)*0.5+flagspacing*0.5;
  for (;flagc-->0;flagx+=flagspacing) {
    if (BATTLE->flagc>=FLAG_LIMIT) break;
    struct flag *flag=BATTLE->flagv+BATTLE->flagc++;
    flag->color=0x808080ff;
    flag->x=flagx;
    flag->y=60.0+(rand()%(FBH-90));
  }
  
  /* Prepare a vertex buffer for the Finnish line.
   */
  int flx=NS_sys_tilesize>>1;
  int fly=FBH-(NS_sys_tilesize>>1);
  int i=NS_sys_mapw;
  struct egg_render_tile *vtx=BATTLE->finnishline;
  for (;i-->0;flx+=NS_sys_tilesize,vtx++) {
    vtx->x=flx;
    vtx->y=fly;
    vtx->tileid=0x88;
    vtx->xform=0;
  }
  i=(NS_sys_mapw>>1)-2;
  BATTLE->finnishline[i++].tileid=0x89;
  BATTLE->finnishline[i++].tileid=0x8a;
  BATTLE->finnishline[i++].tileid=0x8b;
  BATTLE->finnishline[i++].tileid=0x8c;
   
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->turn=-1; break;
    case EGG_BTN_RIGHT: player->turn=1; break;
    default: player->turn=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  /* Don't overthink this.
   * Find the next unassigned flag and steer toward it.
   * If none, or close enough, stop steering.
   * He makes bad decisions, steering toward flags on the other side of the world, and I think that's ok.
   */
  struct flag *next=0;
  struct flag *flag=BATTLE->flagv;
  int i=BATTLE->flagc;
  for (;i-->0;flag++) {
    if (flag->color!=0x808080ff) continue;
    if (flag->y<player->y) continue;
    if (!next||(flag->y<next->y)) next=flag;
  }
  if (!next) {
    player->turn=0;
    return;
  }
  double dx=next->x-player->x;
  const double turnthresh=5.0;
  if (dx>turnthresh) player->turn=1;
  else if (dx<-turnthresh) player->turn=-1;
  else player->turn=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Slide toward the desired sideness.
   */
  if (player->turn) {
    player->turn_sticky=player->turn;
    if ((player->sideness+=player->slide_side*elapsed)>=1.0) player->sideness=1.0;
  } else {
    if ((player->sideness-=player->slide_down*elapsed)<=0.0) player->sideness=0.0;
  }
  
  /* If the hello clock is still ticking, don't do anything else.
   * Do permit turning.
   */
  if (BATTLE->helloclock>0.0) return;
  
  /* If done, assume a neutral face and don't do anything else.
   */
  if (player->done) {
    player->turn=0;
    return;
  }
  
  /* Advance my clock while racing.
   */
  player->racetime+=elapsed;
  
  /* Sample my position for generating the trail, at regular intervals.
   */
  if (player->trailc<TRAIL_LIMIT) {
    if ((player->sampleclock-=elapsed)<=0.0) {
      player->sampleclock+=SAMPLE_TIME;
      player->trailv[player->trailc++]=(struct trail){(int)(player->x-2.0),(int)(player->x+2.0),(int)(player->y+2.0)};
    }
  }
  
  /* Compute velocity from (sideness) and player constants.
   */
  double dx,dy;
  if (player->sideness<=0.0) {
    dx=player->dx_down;
    dy=player->dy_down;
  } else if (player->sideness>=1.0) {
    dx=player->dx_side;
    dy=player->dy_side;
  } else {
    dx=player->dx_down*(1.0-player->sideness)+player->dx_side*player->sideness;
    dy=player->dy_down*(1.0-player->sideness)+player->dy_side*player->sideness;
  }
  dx*=player->turn_sticky;
  
  /* Apply velocity.
   */
  double y0=player->y;
  player->x+=dx*elapsed;
  player->y+=dy*elapsed;
  double edge=10.0;
  if (player->x<edge) player->x=edge;
  else if (player->x>FBW-edge) player->x=FBW-edge;
  
  /* Check flags.
   * A flag is eligible for capture only on the one frame when our (y) cross.
   * Two frames if they happen to coincide, it's possible I guess.
   */
  const double flagradius=10.0;
  struct flag *flag=BATTLE->flagv;
  int i=BATTLE->flagc;
  for (;i-->0;flag++) {
    if (flag->color!=0x808080ff) continue; // Already captured, no longer eligible.
    if (y0>flag->y) continue;
    if (player->y<flag->y) continue;
    double dx=flag->x-player->x;
    if ((dx>=-flagradius)&&(dx<=flagradius)) { // Got it!
      bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
      flag->color=player->color;
      player->score++;
    }
  }
  
  /* If we reached the bottom, become (done).
   */
  if (player->y>FBH-10.0) player->done=1;
}

/* Update.
 */
 
static void _skiing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if (BATTLE->helloclock>0.0) {
    BATTLE->helloclock-=elapsed;
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  /* Game ends when both players are done.
   * Gravity guarantees that that will happen pretty fast.
   * If one has more flags, they win. Otherwise the fastest time.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->done&&r->done) {
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else if (l->racetime<r->racetime) battle->outcome=1;
    else if (l->racetime>r->racetime) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render bits.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=(int)player->y;
  uint8_t tileid=player->tileid;
  uint8_t xform=0;
  if (player->turn) {
    tileid+=1;
    if (player->turn<0) {
      xform=EGG_XFORM_XREV;
      x-=1; // Tiles all have odd width visually, so cheat to the left when flopping.
    }
  }
  graf_tile(&g.graf,x,y,tileid,xform);
}

static void flag_render(struct battle *battle,struct flag *flag) {
  int x=(int)flag->x;
  int y=(int)flag->y;
  int space=8;
  graf_fancy(&g.graf,x-space,y,0x87,0,0,NS_sys_tilesize,0,flag->color);
  graf_fancy(&g.graf,x+space,y,0x87,EGG_XFORM_XREV,0,NS_sys_tilesize,0,flag->color);
}

/* Trail.
 */
 
static void trail_render(struct battle *battle,struct player *player) {
  if (player->trailc<2) return;
  uint32_t color=0xe0f0ffff;
  struct trail *trail=player->trailv;
  graf_line_strip_begin(&g.graf,trail->lx,trail->y,color);
  trail++;
  int i=player->trailc-1;
  for (;i-->0;trail++) graf_line_strip_more(&g.graf,trail->lx,trail->y,color);
  trail=player->trailv;
  graf_line_strip_begin(&g.graf,trail->rx,trail->y,color);
  trail++;
  for (i=player->trailc-1;i-->0;trail++) graf_line_strip_more(&g.graf,trail->rx,trail->y,color);
}

/* Render.
 */
 
static void _skiing_render(struct battle *battle) {

  // Flat colored background.
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,20,FBW,FBH-20,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,20,FBW,1,0x000000ff);
  
  // Trails.
  trail_render(battle,BATTLE->playerv+0);
  trail_render(battle,BATTLE->playerv+1);
  
  graf_set_image(&g.graf,RID_image_battle_tundra);
  
  // At the bottom show the border with Finland*.
  // [*] The "Finnish Line", get it?
  graf_tile_batch(&g.graf,BATTLE->finnishline,NS_sys_mapw);
  
  struct flag *flag=BATTLE->flagv;
  int i=BATTLE->flagc;
  for (;i-->0;flag++) flag_render(battle,flag);
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // Hello clock.
  if (BATTLE->helloclock>0.0) {
    int s=(int)(BATTLE->helloclock+0.999);
    if (s<1) s=1; else if (s>9) s=9;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_set_tint(&g.graf,battle->ctab[BATTLE_COLOR_SKY_TEXT]);
    graf_tile(&g.graf,FBW>>1,10,'0'+s,0);
    graf_set_tint(&g.graf,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_skiing={
  .name="skiing",
  .objlen=sizeof(struct battle_skiing),
  .id=NS_battle_skiing,
  .strix_name=290,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_horz,
  .imageid_default=RID_image_tundra,
  .del=_skiing_del,
  .init=_skiing_init,
  .update=_skiing_update,
  .render=_skiing_render,
};

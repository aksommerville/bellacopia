/* battle_invading.c
 * Space Invaders.
 * This is asymmetric: One player is the defender and one is the horde. Selection is random, at the start of battle.
 */

#include "game/batsup/battle_internal.h"

#define ROLE_HORDE 1
#define ROLE_DEFENDER 2

#define HORDE_COLC 4
#define HORDE_ROWC 3
#define HORDE_SIZE (HORDE_COLC*HORDE_ROWC)

#define HORDEY 10.0
#define DEFENDERY (FBH-10.0)

#define ARROW_LIMIT 32
#define FIREWORK_LIMIT HORDE_SIZE

struct battle_invading {
  struct battle hdr;
  double coinclock; // Counts down initially during the coin toss.
  double prepclock; // Counts down after the coin toss, as the role assignment is shown and heroes march into position.
  
  /* Vertical advancer: The horde must march to one side of the screen to advance vertically.
   */
  int advancerp; // 0,1 = left,right
  double advancerclock;
  int advancerframe;
  
  /* These are controlled by whichever player has ROLE_HORDE.
   * It just doesn't make sense for both players to have one of these lists.
   */
  struct hordelet {
    int alive;
    double animclock;
    int animframe;
  } hordeletv[HORDE_SIZE];
  int hordeletc;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    int role; // ROLE_HORDE or ROLE_DEFENDER, one of each. Zero during selection.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // +(0,1,2) = down,up,right
    double x,y;
    double animclock; // Only DEFENDER uses.
    int animframe;
    double advanceclock; // Only HORDE uses.
    double cpuwalkclock; // Only CPU DEFENDER uses.
    double cpufireclock; // Only CPU DEFENDER uses.
    int cpuwalkdx; // Only CPU DEFENDER uses.
    
    double dmovespeed;
    double hmovespeed;
    double advancespeed;
    double advancetime;
    double arrowspeed;
    
  } playerv[2];
  struct player *horde;
  struct player *defender;
  
  struct arrow {
    double x,y;
  } arrowv[ARROW_LIMIT];
  int arrowc;
  
  struct firework {
    int x,y;
    double animclock;
    int animframe;
  } fireworkv[FIREWORK_LIMIT];
  int fireworkc;
};

#define BATTLE ((struct battle_invading*)battle)

/* Delete.
 */
 
static void _invading_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  
  player->dmovespeed=100.0*(1.0-player->skill)+150.0*player->skill;
  player->hmovespeed=70.0*(1.0-player->skill)+120.0*player->skill;
  player->advancespeed=40.0*(1.0-player->skill)+50.0*player->skill;
  player->advancetime=0.500;
  player->arrowspeed=250.0;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cpuwalkclock=0.500;
    player->cpufireclock=0.500;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xb87a14ff;
        player->tileid=0x78;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x58;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x68;
      } break;
  }
}

/* New.
 */
 
static int _invading_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->coinclock=2.0;
  
  /* The horde starts off entirely alive, and at random phase in animation.
   */
  struct hordelet *hordelet=BATTLE->hordeletv;
  int i=HORDE_SIZE;
  for (;i-->0;hordelet++) {
    hordelet->alive=1;
    hordelet->animclock=((rand()&0xffff)*0.250)/65535.0;
    hordelet->animframe=(rand()&3);
  }
  
  return 0;
}

/* Choose roles, end of cointoss stage.
 */
 
static void invading_choose_roles(struct battle *battle) {
  BATTLE->coinclock=0.0;
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (rand()&1) {
    BATTLE->horde=l;
    BATTLE->defender=r;
  } else {
    BATTLE->horde=r;
    BATTLE->defender=l;
  }
  BATTLE->horde->role=ROLE_HORDE;
  BATTLE->defender->role=ROLE_DEFENDER;
  BATTLE->prepclock=2.0;
  BATTLE->horde->y=HORDEY;
  BATTLE->defender->y=DEFENDERY;
  l->x=(FBW>>1)-10.0;
  r->x=(FBW>>1)+10.0;
}

/* End marching stage, start playing.
 */
 
static void invading_begin_play(struct battle *battle) {
  BATTLE->coinclock=0.0;
  BATTLE->prepclock=0.0;
  BATTLE->advancerp=(rand()&1);
}

/* Count hordelets in one column, aka file.
 */
 
static int invading_file_count(const struct battle *battle,int col) {
  if ((col<0)||(col>=HORDE_COLC)) return 0;
  const struct hordelet *hordelet=BATTLE->hordeletv+col;
  int i=HORDE_ROWC,c=0;
  for (;i-->0;hordelet+=HORDE_COLC) {
    if (hordelet->alive) c++;
  }
  return c;
}

static int invading_rank_count(const struct battle *battle,int row) {
  if ((row<0)||(row>=HORDE_ROWC)) return 0;
  const struct hordelet *hordelet=BATTLE->hordeletv+row*HORDE_COLC;
  int i=HORDE_COLC,c=0;
  for (;i-->0;hordelet++) {
    if (hordelet->alive) c++;
  }
  return c;
}

/* Start the horde's advance and toggle advancement side.
 */
 
static void invading_advance_horde(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_collect,0.0);
  BATTLE->advancerp^=1;
  player->advanceclock=player->advancetime;
}

/* Move the horde horizontally.
 */
 
static void invading_move_horde(struct battle *battle,struct player *player,double elapsed,int dx) {
  /* Determine the horizontal extents of the horde.
   * (player->x) is midway between files 1 and 2.
   * Get out if we're annihilated.
   */
  int lfile=0;
  while ((lfile<HORDE_COLC)&&!invading_file_count(battle,lfile)) lfile++;
  if (lfile>=HORDE_COLC) return; // Horde is wiped out! We shouldn't have been called but there might be an edge here.
  int rfile=HORDE_COLC-1;
  while ((rfile>lfile)&&!invading_file_count(battle,rfile)) rfile--;
  
  player->x+=player->hmovespeed*elapsed*dx;
  
  double hx=player->x-NS_sys_tilesize*1.5+NS_sys_tilesize*lfile;
  double hw=(rfile-lfile)*NS_sys_tilesize;
  const double edge=4.0;
  if (hx<edge) {
    player->x=edge+NS_sys_tilesize*1.5-NS_sys_tilesize*lfile;
    if (!BATTLE->advancerp) invading_advance_horde(battle,player);
  } else if (hx+hw>FBW-edge) {
    player->x=FBW-edge-hw+NS_sys_tilesize*1.5-NS_sys_tilesize*lfile;
    if (BATTLE->advancerp) invading_advance_horde(battle,player);
  }
}

/* Fire an arrow from the defender.
 */
 
static void invading_fire_arrow(struct battle *battle,struct player *player) {
  if (BATTLE->arrowc>=ARROW_LIMIT) return;
  bm_sound_pan(RID_sound_fishpole_begin,0.0);
  struct arrow *arrow=BATTLE->arrowv+BATTLE->arrowc++;
  arrow->x=player->x;
  arrow->y=player->y-12.0;
}

/* Update human player.
 */
 
static void invading_update_horde_man(struct battle *battle,struct player *player,double elapsed) {
  switch (g_input[player->human]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: invading_move_horde(battle,player,elapsed,-1); break;
    case EGG_BTN_RIGHT: invading_move_horde(battle,player,elapsed,1); break;
  }
}
 
static void invading_update_defender_man(struct battle *battle,struct player *player,double elapsed) {
  
  // Horizontal motion, full manual control.
  const double edge=4.0;
  switch (g_input[player->human]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: {
        player->x-=player->dmovespeed*elapsed;
        if (player->x<edge) player->x=edge;
      } break;
    case EGG_BTN_RIGHT: {
        player->x+=player->dmovespeed*elapsed;
        if (player->x>FBW-edge) player->x=FBW-edge;
      } break;
  }
  
  // Shoot an arrow with A.
  if ((g_input[player->human]&EGG_BTN_SOUTH)&&!(g_pvinput[player->human]&EGG_BTN_SOUTH)) {
    invading_fire_arrow(battle,player);
  }
}

/* Update CPU player.
 */
 
static void invading_update_horde_cpu(struct battle *battle,struct player *player,double elapsed) {
  // Dead simple, just walk toward the advancer. We don't try to dodge arrows.
  invading_move_horde(battle,player,elapsed,BATTLE->advancerp?1:-1);
}
 
static void invading_update_defender_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If (cpuwalkclock) expires, pick a random direction to walk.
   * If we're close to an edge, walk toward center.
   * Or if the horde is sufficiently far away, walk toward it.
   * Finally, whatever, random.
   */
  if ((player->cpuwalkclock-=elapsed)<=0.0) {
    player->cpuwalkclock+=0.400+((rand()&0xffff)*0.400)/65535.0;
    const double edge=50.0;
    double hordedx=BATTLE->horde->x-player->x;
    if (player->x<edge) player->cpuwalkdx=1;
    else if (player->x>FBW-edge) player->cpuwalkdx=-1;
    else if (hordedx>20.0) player->cpuwalkdx=1;
    else if (hordedx<-20.0) player->cpuwalkdx=-1;
    else player->cpuwalkdx=(rand()&1)?1:-1;
  }
  
  /* Walk.
   */
  {
    const double edge=4.0;
    player->x+=player->dmovespeed*elapsed*player->cpuwalkdx;
    if (player->x<edge) player->x=edge;
    else if (player->x>FBW-edge) player->x=FBW-edge;
  }
  
  /* Fire an arrow when (cpufireclock) expires.
   */
  if ((player->cpufireclock-=elapsed)<=0.0) {
    double delay=0.300*(1.0-player->skill)+0.100*player->skill+((rand()&0xffff)*0.250)/65535.0;
    player->cpufireclock+=delay;
    invading_fire_arrow(battle,player);
  }
}

/* Horde has advanced. Check whether they breached the defenses.
 */
 
static void invading_check_horde_completion(struct battle *battle) {
  double y=BATTLE->horde->y;
  int rank=HORDE_ROWC,ok=0;
  while (rank-->0) {
    if (invading_rank_count(battle,rank)) {
      ok=1;
      break;
    }
  }
  if (!ok) return; // Horde is annihilated; stop doing things.
  y+=NS_sys_tilesize*rank;
  if (y>=BATTLE->defender->y-10.0) {
    battle->outcome=BATTLE->horde->who?-1:1;
    BATTLE->horde->advanceclock=0.0;
  }
}

/* Check whether this arrow kills any of the horde.
 * If so, we effect the kill and return nonzero -- caller eliminates the arrow.
 */
 
static int invading_check_injury(struct battle *battle,struct arrow *arrow) {
  // Don't iterate the horde. Instead treat them as a grid, and there's this one cell that the arrow intersects.
  int qx=(int)arrow->x;
  int qy=(int)arrow->y;
  int hx=(int)(BATTLE->horde->x-NS_sys_tilesize*1.5);
  int hy=(int)(BATTLE->horde->y-NS_sys_tilesize*0.5);
  if (qx<hx) return 0;
  if (qy<hy) return 0;
  int col=(qx-hx)/NS_sys_tilesize;
  int row=(qy-hy)/NS_sys_tilesize;
  if (col>=HORDE_COLC) return 0;
  if (row>=HORDE_ROWC) return 0;
  
  // Confirm there's a living hordelet here.
  struct hordelet *hordelet=BATTLE->hordeletv+row*HORDE_COLC+col;
  if (!hordelet->alive) return 0;
  
  // Now fine-tuning, there's only an inner subset of the cell that is really effective.
  int midx=hx+col*NS_sys_tilesize+(NS_sys_tilesize>>1);
  int midy=hy+row*NS_sys_tilesize+(NS_sys_tilesize>>1);
  int dx=qx-midx;
  int dy=qy-midy;
  if ((dx<-4)||(dx>4)) return 0;
  if ((dy<-6)||(dy>6)) return 0;
  
  // OK, kill him!
  hordelet->alive=0;
  bm_sound_pan(RID_sound_ouch,0.0);
  
  // Add a firework if there's room (spoiler: There's always room; hordev and fireworkv are the same size).
  if (BATTLE->fireworkc<FIREWORK_LIMIT) {
    struct firework *firework=BATTLE->fireworkv+BATTLE->fireworkc++;
    firework->x=midx;
    firework->y=midy;
    firework->animclock=0.050;
    firework->animframe=0;
  }
  
  // And if this was the last of the horde, defender wins.
  int i=HORDE_SIZE;
  for (hordelet=BATTLE->hordeletv;i-->0;hordelet++) {
    if (hordelet->alive) return 1;
  }
  battle->outcome=BATTLE->defender->who?-1:1;
  return 1;
}

/* Update.
 */
 
static void _invading_update(struct battle *battle,double elapsed) {

  /* Intro stage? Just tick down the clock.
   */
  if (BATTLE->coinclock>0.0) {
    BATTLE->coinclock-=elapsed;
    if (BATTLE->coinclock<=0.0) {
      invading_choose_roles(battle);
    }
    return;
  }
  if (BATTLE->prepclock>0.0) {
    BATTLE->prepclock-=elapsed;
    if (BATTLE->prepclock<=0.0) {
      invading_begin_play(battle);
    }
    return;
  }
  
  /* Move and remove arrows.
   */
  if (BATTLE->arrowc) {
    struct arrow *arrow=BATTLE->arrowv+BATTLE->arrowc-1;
    int i=BATTLE->arrowc;
    double dy=BATTLE->defender->arrowspeed*elapsed;
    for (;i-->0;arrow--) {
      arrow->y-=dy;
      if ((arrow->y<-8.0)||invading_check_injury(battle,arrow)) {
        BATTLE->arrowc--;
        memmove(arrow,arrow+1,sizeof(struct arrow)*(BATTLE->arrowc-i));
      }
    }
  }
  
  /* Animate and remove fireworks.
   */
  if (BATTLE->fireworkc) {
    struct firework *firework=BATTLE->fireworkv+BATTLE->fireworkc-1;
    int i=BATTLE->fireworkc;
    for (;i-->0;firework--) {
      if ((firework->animclock-=elapsed)<=0.0) {
        firework->animclock+=0.050;
        if (++(firework->animframe)>=6) {
          BATTLE->fireworkc--;
          memmove(firework,firework+1,sizeof(struct firework)*(BATTLE->fireworkc-i));
        }
      }
    }
  }
  
  /* Animate the horde, defender, and advancer.
   */
  struct hordelet *hordelet=BATTLE->hordeletv;
  int i=HORDE_SIZE;
  for (;i-->0;hordelet++) {
    if ((hordelet->animclock-=elapsed)<=0.0) {
      hordelet->animclock+=0.250;
      if (++(hordelet->animframe)>=4) hordelet->animframe=0;
    }
  }
  if ((BATTLE->defender->animclock-=elapsed)<=0.0) {
    BATTLE->defender->animclock+=0.250;
    if (++(BATTLE->defender->animframe)>=4) BATTLE->defender->animframe=0;
  }
  if ((BATTLE->advancerclock-=elapsed)<=0.0) {
    BATTLE->advancerclock+=0.125;
    if (++(BATTLE->advancerframe)>=8) BATTLE->advancerframe=0;
  }
  
  /* If the horde is advancing, pay that out.
   */
  if (BATTLE->horde->advanceclock>0.0) {
    BATTLE->horde->advanceclock-=elapsed;
    BATTLE->horde->y+=BATTLE->horde->advancespeed*elapsed;
    invading_check_horde_completion(battle);
  }
  
  /* Update players.
   * There are four separate implementations: (man,cpu) * (horde,defender)
   * All player updates suspend when outcome established. Note that animation does proceed.
   */
  if (battle->outcome==-2) {
    if (BATTLE->horde->human) invading_update_horde_man(battle,BATTLE->horde,elapsed);
    else invading_update_horde_cpu(battle,BATTLE->horde,elapsed);
    if (BATTLE->defender->human) invading_update_defender_man(battle,BATTLE->defender,elapsed);
    else invading_update_defender_cpu(battle,BATTLE->defender,elapsed);
  }
}

/* Coin toss.
 */
 
static void invading_render_cointoss(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  int coiny=(FBH>>1)+3;
  uint8_t cointile=0x89;
  if (((int)(BATTLE->coinclock*7.0))&1) cointile+=1;
  double t=BATTLE->coinclock-1.0;
  t*=t;
  double dy=20.0-t*20.0;
  coiny-=(int)dy;
  graf_tile(g_graf,FBW>>1,coiny,cointile,0);
  graf_tile(g_graf,(FBW>>1)-10,FBH>>1,l->tileid+2,0);
  graf_tile(g_graf,(FBW>>1)+10,FBH>>1,r->tileid,EGG_XFORM_XREV);
}

/* Preparation: Shown the coin toss outcome and march into position.
 */
 
static void invading_render_prep(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  int coiny=(FBH>>1)+3;
  uint8_t cointile=0x89;
  graf_tile(g_graf,FBW>>1,coiny,cointile,0);
  uint8_t facetile=BATTLE->horde->who?0x8b:0x8c; // Heads or tails, doesn't really matter as long as we do the same thing every time.
  graf_tile(g_graf,FBW>>1,(FBH>>1)-10,facetile,0);
  
  double t=1.0-BATTLE->prepclock/2.0;
  int hdy=(int)((FBH*0.5-HORDEY)*t);
  int ddy=(int)((DEFENDERY-FBH*0.5)*t);
  if (BATTLE->playerv[0].role==ROLE_HORDE) { // Left player goes up, right down.
    graf_tile(g_graf,(FBW>>1)-10,(FBH>>1)-hdy,l->tileid+1,0);
    graf_tile(g_graf,(FBW>>1)+10,(FBH>>1)+ddy,r->tileid,EGG_XFORM_XREV);
  } else { // Left player goes down, right up.
    graf_tile(g_graf,(FBW>>1)-10,(FBH>>1)+ddy,l->tileid,0);
    graf_tile(g_graf,(FBW>>1)+10,(FBH>>1)-hdy,r->tileid+1,EGG_XFORM_XREV);
  }
}

/* Render player as the horde.
 */
 
static void invading_render_horde(struct battle *battle,struct player *player) {
  int x0=(int)player->x-NS_sys_tilesize-(NS_sys_tilesize>>1);
  int y0=(int)player->y;
  struct hordelet *hordelet=BATTLE->hordeletv;
  int y=y0,yi=HORDE_ROWC;
  for (;yi-->0;y+=NS_sys_tilesize) {
    int x=x0,xi=HORDE_COLC;
    for (;xi-->0;x+=NS_sys_tilesize,hordelet++) {
      if (!hordelet->alive) continue;
      uint8_t tileid=player->tileid;
      switch (hordelet->animframe) {
        case 1: tileid+=3; break;
        case 3: tileid+=4; break;
      }
      graf_tile(g_graf,x,y,tileid,0);
    }
  }
}

/* Render player as the defender.
 */
 
static void invading_render_defender(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=(int)player->y;
  uint8_t tileid=player->tileid;
  switch (player->animframe) {
    case 1: tileid+=5; break;
    case 3: tileid+=6; break;
    default: tileid+=1; break;
  }
  graf_tile(g_graf,x,y,tileid,0);
}

/* Render.
 */
 
static void _invading_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,0x000000ff);
  graf_set_image(g_graf,RID_image_battle_war);
  
  if (BATTLE->coinclock>0.0) {
    invading_render_cointoss(battle);
    return;
  }
  if (BATTLE->prepclock>0.0) {
    invading_render_prep(battle);
    return;
  }
  
  // Advancer, if playing.
  if (battle->outcome==-2) {
    uint8_t tileid=0x48+BATTLE->advancerframe;
    int x=BATTLE->advancerp?(FBW-4):4;
    int y=NS_sys_tilesize>>1;
    for (;y<FBH+8;y+=NS_sys_tilesize) graf_tile(g_graf,x,y,tileid,0);
  }
  
  {
    struct firework *firework=BATTLE->fireworkv;
    int i=BATTLE->fireworkc;
    for (;i-->0;firework++) graf_tile(g_graf,firework->x,firework->y,0xf3+firework->animframe,0);
  }
  
  invading_render_horde(battle,BATTLE->horde);
  invading_render_defender(battle,BATTLE->defender);
  
  {
    struct arrow *arrow=BATTLE->arrowv;
    int i=BATTLE->arrowc;
    for (;i-->0;arrow++) graf_tile(g_graf,(int)arrow->x,(int)arrow->y,0xf2,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_invading={
  .name="invading",
  .objlen=sizeof(struct battle_invading),
  .id=135,
  .strix_name=433,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz_a,
  .imageid_default=RID_image_battlefield,
  .del=_invading_del,
  .init=_invading_init,
  .update=_invading_update,
  .render=_invading_render,
};

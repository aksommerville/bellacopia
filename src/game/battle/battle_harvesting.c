/* battle_harvesting.c
 */

#include "game/batsup/battle_internal.h"

#define ROWC 4
#define BGVTXC (ROWC*NS_sys_mapw*2)
#define VEG_LIMIT 16
#define SCORE_LIMIT 5

struct battle_harvesting {
  struct battle hdr;
  double playclock;
  
  struct veg {
    int x,row;
    uint8_t tileid;
    uint8_t xform;
    double ttl;
  } vegv[VEG_LIMIT];
  int vegc;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    double x;
    int row;
    int indx,indy,inpull;
    int pvindy,pvinpull;
    double celebrateclock;
    double pullclock;
    double animclock;
    int animframe;
    double walkspeed;
    double pulltimelo,pulltimehi;
    int score;
    struct veg *veg; // While pulling.
    struct score {
      uint8_t tileid,xform;
    } scorev[SCORE_LIMIT];
    double cpudelay;
    double cpurowtimelo,cpurowtimehi; // Extra delay after changing row.
    double cpupulltimelo,cpupulltimehi; // Extra delay after each pull.
    struct veg *cputarget;
  } playerv[2];
  
  /* First half are the furrow backs, second half are the fronts.
   * They render in two passes, so vegetables can tuck in between.
   * Use (bgvtxv[row*NS_sys_mapw]) for row positions.
   */
  struct egg_render_tile bgvtxv[BGVTXC];
};

#define BATTLE ((struct battle_harvesting*)battle)

/* Delete.
 */
 
static void _harvesting_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=20+(rand()%(FBW-40));
  player->row=rand()%ROWC;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  player->pvinpull=1; // So we don't need a blackout.
  
  player->walkspeed=60.0*(1.0-player->skill)+100.0*player->skill;
  player->pulltimehi=0.600*(1.0-player->skill)+0.300*player->skill;
  player->pulltimelo=player->pulltimehi*0.5;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    if (battle->args.lctl||battle->args.rctl) { // Take a penalty when my opponent is human; by default CPU is a little too good.
      player->walkspeed*=0.800;
      player->pulltimehi*=1.200;
      player->pulltimelo*=1.200;
    }
    player->cpudelay=0.750; // A tasteful delay before starting.
    player->cpurowtimehi=0.500*(1.0-player->skill)+0.100*player->skill;
    player->cpurowtimelo=player->cpurowtimehi*0.5;
    player->cpupulltimehi=0.800*(1.0-player->skill)+0.400*player->skill;
    player->cpupulltimelo=player->cpupulltimehi*0.5;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x5a2f10ff;
        player->tileid=0x67;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x57;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x36;
      } break;
  }
}

/* Add a random vegetable.
 */
 
static int veg_position_ok(const struct battle *battle,const struct veg *veg) {
  const struct veg *other=BATTLE->vegv;
  int i=BATTLE->vegc;
  for (;i-->0;other++) {
    if (other==veg) continue;
    if (other->row!=veg->row) continue;
    int dx=other->x-veg->x;
    if (dx<0) dx=-dx;
    if (dx<12) return 0;
  }
  return 1;
}

static struct veg *harvesting_add_veg(struct battle *battle) {
  if (BATTLE->vegc>=VEG_LIMIT) return 0;
  struct veg *veg=BATTLE->vegv+BATTLE->vegc++;
  
  // Pick random positions until we find one with adequate breathing room.
  int panic=100;
  for (;;) {
    if (--panic<=0) {
      BATTLE->vegc--;
      return 0;
    }
    veg->x=20+rand()%(FBW-40);
    veg->row=rand()%ROWC;
    if (veg_position_ok(battle,veg)) break;
  }
  
  // Tile and xform are random and decorative.
  veg->tileid=0x2d+rand()%3;
  veg->xform=(rand()&1)?EGG_XFORM_XREV:0;

  return veg;
}

/* New.
 */
 
static int _harvesting_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->playclock=9.0;
  
  struct egg_render_tile *vtx=BATTLE->bgvtxv;
  
  /* First place the furrow backs LRTB in bgvtxv.
   * These establish the general layout.
   */
  int r=0; for (;r<ROWC;r++) {
    int y=35+r*40;
    int x=NS_sys_tilesize>>1,c=0;
    for (;c<NS_sys_mapw;c++,x+=NS_sys_tilesize) {
      vtx->x=x;
      vtx->y=y;
      vtx->tileid=0x49+(rand()&1);
      vtx->xform=0;
      vtx++;
    }
  }
  
  /* Then another pass for the fronts, offset a little down from the backs.
   */
  struct egg_render_tile *back=BATTLE->bgvtxv;
  int i=ROWC*NS_sys_mapw;
  for (;i-->0;back++) {
    vtx->x=back->x;
    vtx->y=back->y;
    vtx->tileid=0x4b+(rand()&1);
    vtx->xform=0;
    vtx++;
  }
  
  int bgvtxc=vtx-BATTLE->bgvtxv;
  if (bgvtxc!=BGVTXC) {
    fprintf(stderr,"!!!FATAL!!! bgvtxc=%d, expected %d. %s:%d\n",bgvtxc,BGVTXC,__FILE__,__LINE__);
    return -1;
  }
  
  /* Add vegetables until we're full, or maybe RNG panic.
   */
  while (harvesting_add_veg(battle)) ;
  
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
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player->indy=-1; break;
    case EGG_BTN_DOWN: player->indy=1; break;
    default: player->indy=0; break;
  }
  player->inpull=(input&EGG_BTN_SOUTH)?1:0;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  
  /* If we're pulling or celebrating, wait for that to finish.
   */
  if (player->celebrateclock>0.0) {
    player->inpull=0;
    return;
  }
  if (player->pullclock>0.0) {
    return;
  }

  /* If we have a delay, tick it down.
   */
  if (player->cpudelay>0.0) {
    player->cpudelay-=elapsed;
    return;
  }
  
  /* If we were holding SOUTH or DOWN, release it and spend on frame letting common catch up.
   */
  if (player->indy||player->inpull) {
    player->indy=player->inpull=0;
    return;
  }
  
  /* If we have a target, confirm it's still valid.
   */
  if (player->cputarget) {
    struct player *other=BATTLE->playerv+(player->who?0:1);
    if (player->cputarget->ttl>0.0) {
      player->cputarget=0;
    } else if (other->veg==player->cputarget) {
      player->cputarget=0;
    }
  }
  
  /* If we do not have a target, find the nearest.
   * In the unlikely event that we can't find one, set a long delay.
   */
  if (!player->cputarget) {
    struct player *other=BATTLE->playerv+(player->who?0:1);
    struct veg *veg=BATTLE->vegv;
    int i=BATTLE->vegc;
    for (;i-->0;veg++) {
      if (veg->ttl>0.0) continue; // Ineligible.
      if (veg==other->veg) continue; // Ineligible.
      if (!player->cputarget) player->cputarget=veg; // Anything is better than nothing.
      else if ((veg->row==player->row)&&(player->cputarget->row!=player->row)) player->cputarget=veg; // Current row before others.
      else if (veg->row==player->row) { // Nearest horizontally.
        int odx=player->cputarget->x-(int)player->x; if (odx<0) odx=-odx;
        int ndx=veg->x-(int)player->x; if (ndx<0) ndx=-ndx;
        if (ndx<odx) player->cputarget=veg;
      }
    }
    if (!player->cputarget) { // Picked clean! (this isn't possible)
      player->indx=player->indy=0;
      player->cpudelay=999.999;
      return;
    }
  }
  
  /* If the chosen veg is in another row, advance toward it.
   * This doesn't happen every time, but is likely enough, and CPU plays it very dumbly.
   */
  if (player->cputarget->row!=player->row) {
    if (player->cputarget->row<player->row) player->indy=-1;
    else player->indy=1;
    double n=(rand()&0xffff)/65535.0;
    player->cpudelay=player->cpurowtimelo*(1.0-n)+player->cpurowtimehi*n;
    player->indx=0;
    return;
  }
  
  /* Advance toward the target, or if we're close, pick it.
   */
  int dx=player->cputarget->x-(int)player->x;
  if (dx<-4) player->indx=-1;
  else if (dx>4) player->indx=1;
  else {
    player->indx=0;
    player->inpull=1;
    double n=(rand()&0xffff)/65535.0;
    player->cpudelay=player->cpupulltimelo*(1.0-n)+player->cpupulltimehi*n;
    player->cputarget=0;
  }
}

/* Pull state.
 */
 
static void harvesting_cancel_pull(struct battle *battle,struct player *player) {
  player->pullclock=0.0;
  player->veg=0;
}
 
static void harvesting_finish_pull(struct battle *battle,struct player *player) {
  player->pullclock=0.0;
  if (!player->veg) return; // Shouldn't be possible.
  bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
  if (player->score<SCORE_LIMIT) {
    struct score *score=player->scorev+player->score++;
    score->tileid=player->veg->tileid;
    score->xform=player->veg->xform;
  }
  player->veg->ttl=0.500;
  player->veg=0;
  player->celebrateclock=0.500;
}
 
static void harvesting_begin_pull(struct battle *battle,struct player *player) {
  
  // Find a vegetable in range that isn't already being pulled.
  player->veg=0;
  int px=(int)player->x;
  struct player *other=BATTLE->playerv+(player->who?0:1);
  struct veg *veg=BATTLE->vegv;
  int i=BATTLE->vegc;
  for (;i-->0;veg++) {
    if (veg==other->veg) continue; // Find your own!
    if (veg->ttl>0.0) continue; // Already pulled.
    if (veg->row!=player->row) continue;
    int dx=veg->x-px;
    if (dx<0) dx=-dx;
    if (dx>=10) continue;
    player->veg=veg;
    break;
  }
  if (!player->veg) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  
  bm_sound_pan(RID_sound_dig,player->who?PLAYER_PAN:-PLAYER_PAN);
  double n=(rand()&0xffff)/65535.0;
  player->pullclock=player->pulltimelo*(1.0-n)+player->pulltimehi*n;
  player->animframe=0;
  player->animclock=0.0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  
  /* Celebration counts down and doesn't cancel.
   * No motion or new pulling during celebration.
   * This does run during game-over, since typically a pull ends the game.
   */
  if (player->celebrateclock>0.0) {
    player->celebrateclock-=elapsed;
    return;
  }

  /* Game over? Revert to neutral.
   */
  if (battle->outcome>-2) {
    player->celebrateclock=0.0;
    player->pullclock=0.0;
    player->animframe=0;
    return;
  }
  
  /* Pull clock counts down but cancels if they release the button.
   * No motion during pull.
   */
  if (player->pullclock>0.0) {
    if (!player->inpull) {
      player->pvinpull=player->inpull;
      harvesting_cancel_pull(battle,player);
    } else {
      player->pullclock-=elapsed;
      if (player->pullclock<=0.0) {
        harvesting_finish_pull(battle,player);
      }
    }
    return;
  }
  
  /* Fresh stroke of the button begins a pull.
   */
  if (player->inpull&&!player->pvinpull) {
    player->pvinpull=player->inpull;
    harvesting_begin_pull(battle,player);
    return;
  } else if (!player->inpull) {
    player->pvinpull=0;
  }
  
  /* Fresh vertical stroke changes row.
   * Horizontal movement continues if present.
   */
  if (player->indy!=player->pvindy) {
    if (player->pvindy=player->indy) {
      int nrow=player->row+player->indy;
      if ((nrow<0)||(nrow>=ROWC)) {
        bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
      } else {
        bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->row=nrow;
      }
    }
  }
  
  /* Horizontal movement is continuous.
   */
  if (player->indx) {
    const double edge=9.0;
    player->x+=player->walkspeed*elapsed*player->indx;
    if (player->x<edge) player->x=edge;
    else if (player->x>FBW-edge) player->x=FBW-edge;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.125;
      if (++(player->animframe)>=4) player->animframe=0;
    }
    if (player->indx>0) player->xform=0;
    else player->xform=EGG_XFORM_XREV;
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
}

/* Update.
 */
 
static void _harvesting_update(struct battle *battle,double elapsed) {
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  struct veg *veg=BATTLE->vegv+BATTLE->vegc-1;
  for (i=BATTLE->vegc;i-->0;veg--) {
    if (veg->ttl<=0.0) continue;
    veg->ttl-=elapsed;
    if (veg->ttl<=0.0) {
      // CPU should unset its target when the ttl goes positive, but unset here too just in case.
      if (veg==l->cputarget) l->cputarget=0;
      else if (veg<l->cputarget) l->cputarget--;
      if (veg==r->cputarget) r->cputarget=0;
      else if (veg<r->cputarget) r->cputarget--;
      if (veg<l->veg) l->veg--;
      if (veg<r->veg) r->veg--;
      BATTLE->vegc--;
      memmove(veg,veg+1,sizeof(struct veg)*(BATTLE->vegc-i));
    }
  }
  
  /* Battle ends when playclock expires or one player reaches SCORE_LIMIT.
   * Ties are very possible.
   */
  if (battle->outcome==-2) {
    BATTLE->playclock-=elapsed;
    if ((BATTLE->playclock<=0.0)||(l->score>=SCORE_LIMIT)||(r->score>=SCORE_LIMIT)) {
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    }
  } else {
    BATTLE->playclock=0.0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=BATTLE->bgvtxv[player->row*NS_sys_mapw].y-NS_sys_tilesize-1;
  uint8_t tileid=player->tileid;
  if (player->celebrateclock>0.0) tileid+=4;
  else if (player->pullclock>0.0) tileid+=3;
  else switch (player->animframe) {
    case 1: tileid+=1; break;
    case 3: tileid+=2; break;
  }
  graf_tile(g_graf,x,y,tileid,player->xform);
}

/* Render.
 */
 
static void _harvesting_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(g_graf,RID_image_battle_meadow2);
  
  player_render(battle,l);
  player_render(battle,r);
  
  graf_tile_batch(g_graf,BATTLE->bgvtxv,ROWC*NS_sys_mapw); // Furrow backs.
  
  struct veg *veg=BATTLE->vegv;
  int i=BATTLE->vegc;
  for (;i-->0;veg++) {
    int y=BATTLE->bgvtxv[veg->row*NS_sys_mapw].y;
    if (veg->ttl>0.0) y-=26; // Celebration position. Any higher and the front furrows may occlude it.
    else y-=3; // Resting position.
    graf_tile(g_graf,veg->x,y,veg->tileid,veg->xform);
  }
  
  graf_tile_batch(g_graf,BATTLE->bgvtxv+ROWC*NS_sys_mapw,ROWC*NS_sys_mapw); // Furrow fronts.
  
  /* Scoreboard.
   */
  int x=(FBW>>1)-20;
  for (i=0;i<SCORE_LIMIT;i++,x-=12) {
    if (i<l->score) graf_tile(g_graf,x,FBH-8,l->scorev[i].tileid,l->scorev[i].xform);
    else graf_tile(g_graf,x,FBH-8,0x17,0);
  }
  for (x=(FBW>>1)+21,i=0;i<SCORE_LIMIT;i++,x+=12) {
    if (i<r->score) graf_tile(g_graf,x,FBH-8,r->scorev[i].tileid,r->scorev[i].xform);
    else graf_tile(g_graf,x,FBH-8,0x17,0);
  }
  int sec=(int)(BATTLE->playclock+0.999);
  if (sec>0) {
    if (sec>9) sec=9;
    graf_set_image(g_graf,RID_image_fonttiles);
    graf_tile(g_graf,FBW>>1,FBH-8,'0'+sec,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_harvesting={
  .name="harvesting",
  .objlen=sizeof(struct battle_harvesting),
  .id=133,
  .strix_name=429,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz_a,
  .imageid_default=0,
  .del=_harvesting_del,
  .init=_harvesting_init,
  .update=_harvesting_update,
  .render=_harvesting_render,
};

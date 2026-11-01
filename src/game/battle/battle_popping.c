/* battle_popping.c
 */

#include "game/batsup/battle_internal.h"

#define STALKC 30
#define COBC 6 /* Cobs per stalk. */
#define GROUNDY 160

struct battle_popping {
  struct battle hdr;
  double timelimit;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    double tlo,thi;
    int x; // Horizontal center.
    int lx,ly; // Mount point for the agro-laser.
    double t; // Laser angle. Zero is down.
    int indt; // Input state for rotation. >0=clockwise
    int inblast;
    int blackout;
    double rotrate;
    double blast_radius;
    double heatlo,heathi; // hz
    int overcooked; // Goes nonzero after everything in the laser's line of sight has popped, so we can stop checking.
    int score;
    uint8_t noteid;
    double cpuclock;
    double cputarget;
    double cpumin,cpumax; // Shortest and longest intervals we'll blast for. In between, we stop when overcooked.
  } playerv[2];
  
  struct stalk {
    int x; // Framebuffer pixels, center of stalk.
    uint8_t tileid; // 0x00,0x01,0x02: Top of three tiles.
    struct cob {
      int y;
      uint8_t tileid; // Left of three: Empty, raw, heating.
      uint8_t xform;
      double heat; // 0=raw .. 1=popped. At intermediate heats, we blink.
    } cobv[COBC];
  } stalkv[STALKC];
  
  struct kernel {
    double x,y;
    double dx,dy;
    uint8_t tileid;
    uint8_t xform;
    int still;
  } kernelv[STALKC*COBC];
  int kernelc;
};

#define BATTLE ((struct battle_popping*)battle)

/* Delete.
 */
 
static void _popping_del(struct battle *battle) {
  egg_play_song(3,0,0,0.0,0.0);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=50;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->tlo=M_PI*-0.420;
    player->thi=M_PI*0.200;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->x=FBW-player->x;
    player->tlo=M_PI*-0.200;
    player->thi=M_PI*0.420;
  }
  player->lx=player->x;
  player->ly=GROUNDY-NS_sys_tilesize*6-9;
  
  player->rotrate=0.500*(1.0-player->skill)+1.000*player->skill;
  player->blast_radius=4.000*(1.0-player->skill)+6.000*player->skill;
  player->heathi=2.500*(1.0-player->skill)+4.000*player->skill;
  player->heatlo=player->heathi*0.5;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cputarget=M_PI; // out of range, so we'll pick a target first update
    player->cpumin=0.500;
    player->cpumax=3.000;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x3e3f44ff;
        player->tileid=0x4c;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x0c;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x2c;
      } break;
  }
}

/* New.
 */
 
static int _popping_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  /* Place stalks evenly across the horizontal.
   */
  struct stalk *stalk=BATTLE->stalkv;
  int i=0; for (;i<STALKC;i++,stalk++) {
    stalk->x=((i+1)*FBW)/(STALKC+1);
    stalk->tileid=rand()%3;
    struct cob *cob=stalk->cobv;
    int j=0; for (;j<COBC;j++,cob++) {
      cob->y=GROUNDY-(j+1)*6;
      cob->tileid=0x30+(rand()&7)*0x10;
      cob->xform=(j&1)?EGG_XFORM_XREV:0;
      cob->heat=0.0;
    }
  }
  
  egg_play_song(3,RID_song_lasers,1,0.300,0.0);
  BATTLE->timelimit=20.0; // Important to end well before the timeout, esp for cvc. They're random, so when there's little corn left, it drags on.
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_SOUTH))) player->blackout=0;
    return;
  }
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indt=1; break;
    case EGG_BTN_RIGHT: player->indt=-1; break;
    default: player->indt=0; break;
  }
  player->inblast=(input&EGG_BTN_SOUTH);
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If we're blasting, tick the clock.
   * If it exceeds (cpumax), stop blasting.
   * If it's above (cpumin) and we're overcooked, stop blasting.
   */
  if (player->inblast) {
    player->cpuclock+=elapsed;
    player->indt=0;
    if (
      (player->cpuclock>=player->cpumax)||
      ((player->cpuclock>=player->cpumin)&&player->overcooked)
    ) {
      player->inblast=0;
      player->cputarget=M_PI;
    }
    return;
  }
  
  /* If we don't have a target yet, pick one at random.
   * Go a little bit inside the valid range.
   */
  if ((player->cputarget<=player->tlo)||(player->cputarget>=player->thi)) {
    double range=(player->thi-player->tlo)-0.100;
    player->cputarget=player->tlo+0.050+((rand()&0xffff)*range)/65535.0;
  }
  
  /* If we're moving, stop and blast when we've crossed the target.
   */
  if (player->indt>0) {
    if (player->t>=player->cputarget) {
      player->indt=0;
      player->inblast=1;
      player->cpuclock=0.0;
    }
  } else if (player->indt<0) {
    if (player->t<=player->cputarget) {
      player->indt=0;
      player->inblast=1;
      player->cpuclock=0.0;
    }
  } else {
    if (player->cputarget<player->t) player->indt=-1;
    else player->indt=1;
  }
}

/* Return where this player's laser intersects the horizon, in horizontal framebuffer pixels.
 */
 
static int popping_get_intersection(const struct battle *battle,const struct player *player) {
  double nx=sin(player->t);
  double ny=-cos(player->t);
  return player->lx+(int)(((GROUNDY-player->ly)*nx)/ny);
}

/* Pop!
 */
 
static void popping_pop(struct battle *battle,struct player *player,struct stalk *stalk,struct cob *cob) {
  cob->heat=1.0;
  bm_sound_pan(RID_sound_pop,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->score++;
  if (BATTLE->kernelc<STALKC*COBC) {
    struct kernel *kernel=BATTLE->kernelv+BATTLE->kernelc++;
    kernel->x=stalk->x+(cob->xform?-4:4);
    kernel->y=cob->y;
    kernel->tileid=0x48+(rand()&3);
    kernel->xform=(rand()&7);
    kernel->dx=((rand()&0xffff)*50.0)/65535.0-25.0;
    kernel->dy=-60.0;
  }
}

/* Heat corn in sight of the laser.
 */
 
static void popping_update_laser(struct battle *battle,struct player *player,double elapsed) {

  if (!player->noteid) {
    player->noteid=0x50+player->who;
    egg_song_event_note_on(3,player->who,player->noteid,0x40);
  }

  // Once we've updated cornlessly once, we don't need to check again until she moves.
  if (player->overcooked) return;
  // It's way overkill, but let's go ahead and calculate the scalar rejection of each cob, at each frame.
  double ax=(double)player->lx;
  double ay=(double)player->ly;
  double by=(double)GROUNDY-ay; // pre-translate, reference (a).
  double bx=(double)popping_get_intersection(battle,player)-ax;
  double len=sqrt(bx*bx+by*by);
  struct stalk *stalk=BATTLE->stalkv;
  int stalki=STALKC;
  int heatc=0;
  for (;stalki-->0;stalk++) {
    struct cob *cob=stalk->cobv;
    int cobi=COBC;
    for (;cobi-->0;cob++) {
      if (cob->heat>=1.0) continue; // stop! stop! for the love of god, i'm already dead!
      double cx=(double)(stalk->x+(cob->xform?-4:4));
      double cy=(double)cob->y;
      double cp=(cx-ax)*by-(cy-ay)*bx;
      double rej=cp/len;
      if (rej<0.0) rej=-rej;
      if (rej>player->blast_radius) continue;
      const double hotfloor=2.000; // If at least this close, it's perfect heating.
      double degree=1.0-(rej-hotfloor)/(player->blast_radius-hotfloor);
      if (degree<=0.0) continue;
      heatc++;
      if (degree>1.0) degree=1.0;
      degree=player->heatlo*(1.0-degree)+player->heathi*degree;
      if ((cob->heat+=degree*elapsed)>=1.0) {
        popping_pop(battle,player,stalk,cob);
      }
    }
  }
  if (!heatc) player->overcooked=1;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Neutralize after finish.
   */
  if (battle->outcome>-2) {
    player->inblast=0;
    player->indt=0;
    if (player->noteid) {
      egg_song_event_note_off(3,player->who,player->noteid);
      player->noteid=0;
      return;
    }
  }
  
  if (player->noteid&&!player->inblast) {
    egg_song_event_note_off(3,player->who,player->noteid);
    player->noteid=0;
  }

  /* If blasting, rotation is forbidden.
   * Check for cobs near the beam and heat them.
   */
  if (player->inblast) {
    popping_update_laser(battle,player,elapsed);
    
  /* Not blasting? Can rotate.
   */
  } else if (player->indt) {
    player->overcooked=0;
    player->t+=player->rotrate*elapsed*player->indt;
    if (player->t<player->tlo) player->t=player->tlo;
    else if (player->t>player->thi) player->t=player->thi;
  }
}

/* Update.
 */
 
static void _popping_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  struct kernel *kernel=BATTLE->kernelv;
  for (i=BATTLE->kernelc;i-->0;kernel++) {
    if (kernel->still) continue;
    kernel->x+=kernel->dx*elapsed;
    if ((kernel->y+=kernel->dy*elapsed)>=GROUNDY) {
      if (kernel->dy>20.0) { // fast, let's bounce first.
        kernel->dy=kernel->dy*-0.500;
        kernel->y=GROUNDY-0.5;
      } else { // stop
        kernel->still=1;
        kernel->y=GROUNDY+((rand()%5)-2); // a little artificial variety in the final vert positions
      }
    }
    if ((kernel->dy+=100.0*elapsed)>=80.0) kernel->dy=80.0; // gravity
  }
  
  /* Game ends when timelimit expires, and whoever's ahead wins.
   */
  if (battle->outcome==-2) {
    if ((BATTLE->timelimit-=elapsed)<=0.0) {
      struct player *l=BATTLE->playerv;
      struct player *r=l+1;
      if (l->score>r->score) { battle->outcome=1; bm_sound_pan(RID_sound_cockadoodledoo,0.0); }
      else if (l->score<r->score) { battle->outcome=-1; bm_sound_pan(RID_sound_cockadoodledoo,0.0); }
      else battle->outcome=0;
    }
  }
  
  /* Game ends when one player crosses 50%.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    const int halfway=(STALKC*COBC)>>1;
    if (l->score>halfway) { battle->outcome=1; bm_sound_pan(RID_sound_cockadoodledoo,0.0); }
    else if (r->score>halfway) { battle->outcome=-1; bm_sound_pan(RID_sound_cockadoodledoo,0.0); }
    else if (l->score+r->score>=STALKC*COBC) battle->outcome=0; // They both land at 50%. It's possible; there's an even count of cobs. Extremely unlikely tho.
  }
}

/* Render laser beam for one player.
 */
 
static void laser_render(struct battle *battle,struct player *player) {
  int ax=player->lx;
  int ay=player->ly;
  int by=GROUNDY;
  int bx=popping_get_intersection(battle,player);
  graf_set_input(g_graf,0);
  uint32_t acolor=0xffff00ff,bcolor=0xff0000ff;
  if (g_framec&4) acolor=bcolor=0x00ff00ff;
  graf_line(g_graf,ax,ay,acolor,bx,by,bcolor);
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  int y=GROUNDY-NS_sys_tilesize*8-6;
  uint8_t tileid=player->tileid;
  if (player->inblast) tileid+=3;
  else if (player->indt>0) tileid+=1;
  else if (player->indt<0) tileid+=2;
  graf_tile(g_graf,player->x,y,tileid,player->xform);
  graf_tile(g_graf,player->x,y+NS_sys_tilesize,tileid+0x10,player->xform);

  int srcx=NS_sys_tilesize*8;
  int srcy=0;
  int w=NS_sys_tilesize*3;
  double t=player->t;
  graf_set_filter(g_graf,1);
  graf_decal_rotate(g_graf,player->lx,player->ly,srcx,srcy,w,sin(t),cos(t),0.900);
  graf_set_filter(g_graf,0);
}

/* Render stalk.
 */
 
static uint32_t blend_rgba(uint32_t a,uint32_t b,double bw) {
  if (bw<=0.0) return a;
  if (bw>=1.0) return b;
  double aw=1.0-bw;
  int rr=(int)((a>>24)*aw+(b>>24)*bw); if (rr<0) rr=0; else if (rr>0xff) rr=0xff;
  int gg=(int)(((a>>16)&0xff)*aw+((b>>16)&0xff)*bw); if (gg<0) gg=0; else if (gg>0xff) gg=0xff;
  int bb=(int)(((a>>8)&0xff)*aw+((b>>8)&0xff)*bw); if (bb<0) bb=0; else if (bb>0xff) bb=0xff;
  return (rr<<24)|(gg<<16)|(bb<<8)|0xff;
}
 
static void stalk_render(struct battle *battle,struct stalk *stalk) {
  int y=GROUNDY-(NS_sys_tilesize>>1)+1;
  graf_fancy(g_graf,stalk->x,y,stalk->tileid+0x20,0,0,NS_sys_tilesize,0,0x808080ff); y-=NS_sys_tilesize;
  graf_fancy(g_graf,stalk->x,y,stalk->tileid+0x10,0,0,NS_sys_tilesize,0,0x808080ff); y-=NS_sys_tilesize;
  graf_fancy(g_graf,stalk->x,y,stalk->tileid+0x00,0,0,NS_sys_tilesize,0,0x808080ff);
  struct cob *cob=stalk->cobv;
  int i=COBC;
  for (;i-->0;cob++) {
    int x=stalk->x;
    if (cob->xform) x-=2; else x+=1; // Stalk has odd width.
    uint8_t tileid=cob->tileid;
    uint32_t color=0xff0000ff;
    if (cob->heat>=1.0) ; // Popped. Use the natural tile, just a leaf.
    else if (cob->heat<=0.0) tileid+=1; // Raw.
    else { // Heating.
      tileid+=2;
      color=blend_rgba(0xe5c014ff,0xff0000ff,cob->heat);
    }
    graf_fancy(g_graf,x,cob->y,tileid,cob->xform,0,NS_sys_tilesize,0,color);
  }
}

/* Render.
 */
 
static void _popping_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;

  // Sky and ground.
  graf_fill_rect(g_graf,0,0,FBW,GROUNDY,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,(FBH-GROUNDY),battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,1,0x000000ff);
  
  // Towers.
  graf_set_image(g_graf,RID_image_battle_meadow);
  graf_decal(g_graf,l->x-NS_sys_tilesize*2-(NS_sys_tilesize>>1),GROUNDY-NS_sys_tilesize*7+1,NS_sys_tilesize*3,0,NS_sys_tilesize*5,NS_sys_tilesize*7);
  graf_decal(g_graf,r->x-NS_sys_tilesize*2-(NS_sys_tilesize>>1),GROUNDY-NS_sys_tilesize*7+1,NS_sys_tilesize*3,0,NS_sys_tilesize*5,NS_sys_tilesize*7);
  
  // Corn.
  graf_set_image(g_graf,RID_image_battle_meadow);
  struct stalk *stalk=BATTLE->stalkv;
  int i=STALKC; for (;i-->0;stalk++) stalk_render(battle,stalk);
  
  // Popped kernels.
  struct kernel *kernel=BATTLE->kernelv;
  for (i=BATTLE->kernelc;i-->0;kernel++) graf_tile(g_graf,(int)kernel->x,(int)kernel->y,kernel->tileid,kernel->xform);
  
  // Laser beams.
  if (l->inblast) laser_render(battle,l);
  if (r->inblast) laser_render(battle,r);
  
  // Players and their laser turrets.
  graf_set_image(g_graf,RID_image_battle_meadow);
  player_render(battle,l);
  player_render(battle,r);
  
  // Scoreboard and clock.
  {
    int sbw=170;
    int sbx=(FBW>>1)-(sbw>>1);
    int sby=20;
    int sbh=5;
    graf_fill_rect(g_graf,sbx-1,sby-1,sbw+2,sbh+2,0xffffffff);
    graf_fill_rect(g_graf,(FBW>>1)-1,sby-1,2,sbh+2,0xe0e0e0ff);
    const int range=STALKC*COBC;
    int lw=(l->score*sbw)/range;
    int rw=(r->score*sbw)/range;
    if (lw>0) graf_fill_rect(g_graf,sbx,sby,lw,sbh,l->color);
    if (rw>0) graf_fill_rect(g_graf,sbx+sbw-rw,sby,rw,sbh,r->color);
    
    graf_set_image(g_graf,RID_image_battle_meadow);
    uint8_t tileid=0x58,xform=0;
    if (battle->outcome==1) { tileid++; xform=EGG_XFORM_XREV; }
    else if (battle->outcome==-1) tileid++;
    graf_tile(g_graf,FBW>>1,sby-7,tileid,xform);
    
    graf_set_image(g_graf,RID_image_fonttiles);
    graf_set_tint(g_graf,battle->ctab[BATTLE_COLOR_SKY_TEXT]);
    int sec=(int)(BATTLE->timelimit+0.999);
    if (sec<0) sec=0;
    else if (sec>99) sec=99;
    if (sec>=10) {
      graf_tile(g_graf,(FBW>>1)-4,sby+sbh+8,0x30+sec/10,0);
      graf_tile(g_graf,(FBW>>1)+4,sby+sbh+8,0x30+sec%10,0);
    } else {
      graf_tile(g_graf,FBW>>1,sby+sbh+8,0x30+sec,0);
    }
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_popping={
  .name="popping",
  .objlen=sizeof(struct battle_popping),
  .id=109,
  .strix_name=378,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz_a,
  .imageid_default=RID_image_meadow,
  .del=_popping_del,
  .init=_popping_init,
  .update=_popping_update,
  .render=_popping_render,
};

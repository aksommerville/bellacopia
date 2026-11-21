/* battle_hurdles.c
 */

#include "game/batsup/battle_internal.h"

#define TRACKLEN 1000.0 /* Pixels. */
#define HURDLE_LIMIT 6

struct battle_hurdles {
  struct battle hdr;
  double uptime;
  double finishclock; // Counts down after one player finishes.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double x; // 0..TRACKLEN, pixels.
    double y; // Signed pixels from baseline. Always zero or negative.
    double charge; // 0..1, highly volatile
    double tripclock; // >0 when we're tripped
    double animclock;
    int animframe;
    int injump;
    int jumpok; // Nonzero when seated and jump key has been released.
    double finishtime; // zero or sample of (uptime) at completion.
    double gravity; // Negative when jumping.
    double cpuclock; // CPU strobes its run button constantly.
    
    double chargeup; // 0..1, how much charge to add per stroke
    double losslo,losshi; // hz, rate of charge loss. Scales to drop faster at the top.
    double speed; // px/s, running speed at maximum charge.
    double triptime; // sec, duration of a trip penalty.
    double jumpforce; // px/sec, negative, initial gravity when jumping.
    double gravity_rate; // px/sec**2
    double gravity_limit; // px/sec
    double cpulo,cpuhi; // sec. Strobe intervals for CPU player. Just a wee range so it's not too mechanical.
    
    struct hurdle {
      double x;
      int tripped;
    } hurdlev[HURDLE_LIMIT];
    int hurdlec;
    
  } playerv[2];
};

#define BATTLE ((struct battle_hurdles*)battle)

/* Delete.
 */
 
static void _hurdles_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  player->jumpok=0; // Start false so we don't need a blackout.
  
  player->chargeup=0.150*(1.0-player->skill)+0.200*player->skill;
  player->speed=190.0*(1.0-player->skill)+210.0*player->skill;
  player->jumpforce=-130.0*(1.0-player->skill)-170.0*player->skill;
  player->triptime=0.300*(1.0-player->skill)+0.200*player->skill;
  // Loss and gravity are tunable but I think constant is more appropriate:
  player->losslo=0.500;
  player->losshi=0.900;
  player->gravity_rate=500.0;
  player->gravity_limit=200.0;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    if (battle->args.lctl||battle->args.rctl) player->speed*=0.900;
    player->cpuhi=0.200*(1.0-player->skill)+0.190*player->skill;
    player->cpulo=player->cpuhi*0.900;
    player->cpuclock=0.500; // Give a wee head start to the puny human.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xba0e0eff;
        player->tileid=0x73;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x53;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x63;
      } break;
  }
  
  /* Count of hurdles is linear from skill. Minimum 1.
   * Space them evenly along the track, leaving some margin fore and aft.
   */
  const int frontmargin=150;
  const int backmargin=150;
  const int availablew=(int)TRACKLEN-frontmargin-backmargin;
  int hurdlec=lround((1.0-player->skill)*HURDLE_LIMIT);
  if (hurdlec<1) hurdlec=1;
  else if (hurdlec>HURDLE_LIMIT) hurdlec=HURDLE_LIMIT;
  int spacing=availablew;
  if (hurdlec>1) spacing/=(hurdlec-1);
  int x=frontmargin;
  if (hurdlec<=1) x+=(spacing>>1);
  while (player->hurdlec<hurdlec) {
    struct hurdle *hurdle=player->hurdlev+player->hurdlec++;
    hurdle->x=x;
    hurdle->tripped=0;
    x+=spacing;
  }
}

/* New.
 */
 
static int _hurdles_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Increase charge.
 */
 
static void hurdles_poke_charge(struct battle *battle,struct player *player) {
  if (player->finishtime>0.0) return;
  if (player->y<0.0) return; // Charge holds constant during a jump.
  if (player->tripclock>0.0) return;
  player->charge+=player->chargeup;
  // Let it exceed 1.0; the common update will clamp after decaying.
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) hurdles_poke_charge(battle,player);
  player->injump=(input&EGG_BTN_UP)?1:0;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Strobe our run button constantly, even when tripped or jumping.
   * If it's not playable, it's noop, no problem.
   */
  if ((player->cpuclock-=elapsed)<=0.0) {
    double n=(rand()&0xffff)/65535.0;
    player->cpuclock+=player->cpulo*(1.0-n)+player->cpuhi*n;
    hurdles_poke_charge(battle,player);
  }
  
  /* Hold jump if there's a hurdle near.
   * Hopefully this fuzzes out to where we don't need to care about velocity.
   */
  player->injump=0;
  struct hurdle *hurdle=player->hurdlev;
  int i=player->hurdlec;
  for (;i-->0;hurdle++) {
    double dx=hurdle->x-player->x;
    if (dx<=0.0) continue;
    if (dx<20.0) {
      player->injump=1;
      break;
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Apply gravity.
   */
  player->y+=player->gravity*elapsed;
  if (player->y>=0.0) { // Hit ground. This hits repeatedly while seated.
    player->y=0.0;
    player->gravity=0.0;
    if (!player->injump) player->jumpok=1;
  } else { // Falling. Increase gravity, and zero it if they release the jump key.
    if (!player->injump&&(player->gravity<0.0)) player->gravity=0.0;
    player->gravity+=player->gravity_rate*elapsed;
    if (player->gravity>player->gravity_limit) player->gravity=player->gravity_limit;
  }
  
  /* If we're penalized due to a trip, pay that out.
   * Charge drops to zero when tripped.
   */
  if (player->tripclock>0.0) {
    player->tripclock-=elapsed;
    player->charge=0.0;
    player->animclock=0.0;
    player->animframe=0;
    return;
  }

  /* Decay and clamp charge.
   * Clamp the high end too; poking lets it run over.
   * Charge holds constant during a jump.
   */
  if ((player->charge>0.0)&&(player->y>=0.0)) {
    double loss=player->charge;
    if (loss>1.0) loss=1.0;
    loss=player->losslo*(1.0-loss)+player->losshi*loss;
    player->charge-=loss*elapsed;
    if (player->charge<0.0) player->charge=0.0;
    else if (player->charge>1.0) player->charge=1.0;
  }
  
  /* If the charge is nonzero, we're walking.
   */
  double x0=player->x;
  if (player->charge>0.0) {
    player->x+=player->speed*player->charge*elapsed;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.125;
      if (++(player->animframe)>=4) player->animframe=0;
    }
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
  
  /* Finish?
   */
  if ((player->finishtime<=0.0)&&(player->x>=TRACKLEN)) {
    player->finishtime=BATTLE->uptime;
  }
  if (player->finishtime>0.0) return;
  
  /* Start a jump?
   * Finish and abort are up top, that's gravity.
   */
  if ((player->y>=0.0)&&player->jumpok&&player->injump) {
    bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->gravity=player->jumpforce;
    player->jumpok=0;
    player->y=-0.001; // <0 so it shows the jump face this frame.
  }
  
  /* Trip a hurdle if we cross it and are not jumping.
   */
  if ((player->x>x0)&&(player->y>=0.0)) {
    struct hurdle *hurdle=player->hurdlev;
    int i=player->hurdlec;
    for (;i-->0;hurdle++) {
      if (hurdle->tripped) continue;
      if (hurdle->x<x0) continue;
      if (hurdle->x>player->x) continue;
      hurdle->tripped=1;
      player->tripclock=player->triptime;
      bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
      break;
    }
  }
}

/* Set outcome.
 */
 
static void hurdles_finish(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  int ldone=(l->finishtime>0.0);
  int rdone=(r->finishtime>0.0);
  if (!ldone&&!rdone) { // Neither finished -- we shouldn't have been called. But judge based on their progress.
    if (l->x>r->x) battle->outcome=1;
    else if (l->x<r->x) battle->outcome=-1;
    else battle->outcome=0;
  } else if (!ldone) battle->outcome=-1; // One unfinished...
  else if (!rdone) battle->outcome=1;
  else if (l->finishtime<r->finishtime) battle->outcome=1; // Both finshed, compare times (the usual case)...
  else if (l->finishtime>r->finishtime) battle->outcome=-1;
  else battle->outcome=0; // Technically possible for both to finish on the same frame.
}

/* Update.
 */
 
static void _hurdles_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  BATTLE->uptime+=elapsed;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* One player will finish first. After that, allow a brief interval, say 5 seconds, for the other to finish.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if ((l->finishtime>0.0)||(r->finishtime>0.0)) { // At least one is finished.
      if (BATTLE->finishclock<=0.0) { // First frame after the first finish: Set finishclock.
        BATTLE->finishclock=5.0;
      } else if ((BATTLE->finishclock-=elapsed)<=0.0) { // Finishclock expired -- end it.
        hurdles_finish(battle);
      } else if ((l->finishtime>0.0)&&(r->finishtime>0.0)) { // Both finished -- end it.
        hurdles_finish(battle);
      }
    }
  }
}

/* Render player.
 * Overwrites half of the framebuffer. They won't draw OOB.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int top=player->who?(FBH>>1):0;
  int scroll=(int)(player->x-50.0); // Left edge of world, in framebuffer pixels. Can be negative.
  
  /* Grass with darker bars, think World Class Track Meet.
   */
  int barw=40,barspace=80;
  graf_fill_rect(g_graf,0,top,FBW,FBH>>1,battle->ctab[BATTLE_COLOR_GROUND]);
  int barx=scroll/barspace;
  if (scroll<0) barx-=1;
  barx*=barspace;
  barx-=scroll;
  for (;barx<FBW;barx+=barspace) graf_fill_rect(g_graf,barx,top,barw,FBH>>1,0x00000040);
  
  graf_set_image(g_graf,RID_image_battle_war);
  
  /* Track.
   */
  int ty0=top+(FBH>>2)+2;
  int ty1=ty0+NS_sys_tilesize;
  int tx=NS_sys_tilesize-1-scroll%NS_sys_tilesize;
  while (tx>0) tx-=NS_sys_tilesize;
  int xstop=FBW+(NS_sys_tilesize>>1);
  for (;tx<xstop;tx+=NS_sys_tilesize) {
    graf_tile(g_graf,tx,ty0,0x05,0);
    graf_tile(g_graf,tx,ty1,0x15,0);
  }
  
  /* Milestones.
   * 10 pixels = 1 mile; stone every 10 miles ie 100 pixels.
   * No zero or negative stones, and none past the finish line.
   */
  int decamile=scroll/100;
  if (scroll<0) decamile--;
  if (decamile<1) decamile=1;
  int mx=decamile*100-scroll;
  int my=ty0-20;
  int mile=decamile*10;
  int lastmile=(int)TRACKLEN/10;
  for (;mx<FBW+8;mile+=10,mx+=100) {
    if (mile>lastmile) break; // Do draw the finish line's milestone, but none after.
    graf_tile(g_graf,mx,my,0x16,0);
    if (mile>=100) { // three digits
      graf_tile(g_graf,mx-4,my,0x06+mile/100,0);
      graf_tile(g_graf,mx,my,0x06+(mile/10)%10,0);
      graf_tile(g_graf,mx+4,my,0x06+mile%10,0);
    } else { // two digits
      graf_tile(g_graf,mx-2,my,0x06+mile/10,0);
      graf_tile(g_graf,mx+2,my,0x06+mile%10,0);
    }
  }
  
  /* Start and finish line.
   * They're both one tile duplicated vertically, but we want to chop off the top and bottom two rows, so use decals.
   * Little horizontal offset because we don't want them centered on the hero.
   */
  int linex=-scroll+4;
  if ((linex>-4)&&(linex<FBW+4)) {
    graf_decal(g_graf,linex-8,ty0-6,112,18,16,14);
    graf_decal(g_graf,linex-8,ty0+8,112,16,16,14);
  }
  linex=(int)TRACKLEN-scroll+10;
  if ((linex>-8)&&(linex<FBW+8)) {
    graf_decal(g_graf,linex-8,ty0-6,112,50,16,14);
    graf_decal(g_graf,linex-8,ty0+8,112,48,16,14);
  }
  
  /* Hurdles.
   */
  struct hurdle *hurdle=player->hurdlev;
  int i=player->hurdlec;
  for (;i-->0;hurdle++) {
    int x=(int)hurdle->x-scroll;
    if ((x<-8)||(x>FBW+8)) continue;
    uint8_t tileid=hurdle->tripped?0x19:0x18;
    graf_tile(g_graf,x,ty0+8,tileid,0);
  }
  
  /* Hero.
   */
  int px=(int)player->x-scroll;
  int py=top+(FBH>>2)+10;
  uint8_t ptile=player->tileid;
  if (player->tripclock>0.0) { // Tripped.
    ptile+=4;
  } else if (player->y<0.0) { // Jumping.
    ptile+=3;
    py+=lround(player->y);
  } else switch (player->animframe) { // Idle or walking.
    case 1: ptile+=1; break;
    case 3: ptile+=2; break;
  }
  graf_tile(g_graf,px,py,ptile,0);
  
  /* Charge meter.
   */
  barx=20;
  barw=4;
  int barh=(FBH>>1)-30;
  int bary=top+(FBH>>2)-(barh>>1);
  graf_fill_rect(g_graf,barx-1,bary-1,barw+2,barh+2,0x000000ff);
  int fillh=lround(player->charge*barh);
  if (fillh>0) {
    if (fillh>barh) fillh=barh;
    graf_fill_rect(g_graf,barx,bary+barh-fillh,barw,fillh,player->color);
  }
  
  /* If finished, print my time on the track, a bit to the right.
   */
  if (player->finishtime>0.0) {
    int ms=(int)(player->finishtime*1000.0);
    if (ms>0) {
      int sec=ms/1000;
      ms%=1000;
      if (sec>99) { // huh?
        sec=99;
        ms=999;
      }
      int clockx=100;
      int clocky=ty0+(NS_sys_tilesize>>1);
      graf_set_image(g_graf,RID_image_fonttiles);
      graf_set_tint(g_graf,0x000000ff); // Drawing over our track tile, which is lightish.
      if (sec>=10) graf_tile(g_graf,clockx,clocky,'0'+sec/10,0); clockx+=8;
      graf_tile(g_graf,clockx,clocky,'0'+sec%10,0); clockx+=8;
      graf_tile(g_graf,clockx,clocky,'.',0); clockx+=8;
      graf_tile(g_graf,clockx,clocky,'0'+ms/100,0); clockx+=8;
      graf_tile(g_graf,clockx,clocky,'0'+(ms/10)%10,0); clockx+=8;
      graf_tile(g_graf,clockx,clocky,'0'+ms%10,0);
      graf_set_tint(g_graf,0);
    }
  }
}

/* Render.
 */
 
static void _hurdles_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  player_render(battle,l);
  player_render(battle,r);
  
  int thumbw=2,thumbh=2;
  int barh=3+(thumbh<<1);
  int bary=(FBH>>1)-(barh>>1);
  graf_fill_rect(g_graf,0,bary,FBW,barh,0x000000ff);
  int lx=(int)((l->x*(FBW-thumbw))/TRACKLEN);
  if (lx<0) lx=0; else if (lx>FBW-thumbw) lx=FBW-thumbw;
  graf_fill_rect(g_graf,lx,bary+1,thumbw,thumbh,l->color);
  int rx=(int)((r->x*(FBW-thumbw))/TRACKLEN);
  if (rx<0) rx=0; else if (rx>FBW-thumbw) rx=FBW-thumbw;
  graf_fill_rect(g_graf,rx,bary+2+thumbh,thumbw,thumbh,r->color);
}

/* Type definition.
 */
 
static const struct battle_input battle_input_hurdles[]={
  {1,EGG_BTN_SOUTH},
  {1,0},
  {1,EGG_BTN_SOUTH},
  {1,0},
  {1,EGG_BTN_SOUTH},
  {1,0},
  {1,EGG_BTN_UP|EGG_BTN_SOUTH},
  {1,EGG_BTN_UP},
  {1,EGG_BTN_UP|EGG_BTN_SOUTH},
  {1,EGG_BTN_UP},
{0}};
 
const struct battle_type battle_type_hurdles={
  .name="hurdles",
  .objlen=sizeof(struct battle_hurdles),
  .id=134,
  .strix_name=431,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_hurdles,
  .imageid_default=RID_image_battlefield,
  .del=_hurdles_del,
  .init=_hurdles_init,
  .update=_hurdles_update,
  .render=_hurdles_render,
};

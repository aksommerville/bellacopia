/* battle_crossing.c
 */

#include "game/batsup/battle_internal.h"

// Background tiles are just the asphalt and lane lines; largely decorative.
#define BGCOLC 20
#define BGROWC 8
#define BGLINEC 9
#define BGVTXC (BGCOLC*BGROWC+BGLINEC*(BGROWC-1))

#define CARC 30
#define GOALY ((FBH>>1)-((BGROWC*NS_sys_tilesize)>>1)-(NS_sys_tilesize>>1))

struct battle_crossing {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // (idle,walk1,walk2)
    uint8_t xform;
    int animframe;
    double animclock;
    double x,y; // framebuffer pixels
    double speed;
    int indx,indy;
    int dead,win;
    int soulballc;
    double dethklok;
    int cputargetrow; // Starts at BGROWC and counts down to -1, decrementing when we decide to move.
  } playerv[2];
  
  struct car {
    int row;
    double x,y;
    uint8_t tileid;
    uint32_t color;
  } carv[CARC];
  
  struct egg_render_tile bgvtxv[BGVTXC];
  double rowspeed[BGROWC]; // px/sec signed
  int rowyv[BGROWC];
};

#define BATTLE ((struct battle_crossing*)battle)

/* Delete.
 */
 
static void _crossing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=FBW/3;
  player->y=(FBH>>1)+((BGROWC*NS_sys_tilesize)>>1)+(NS_sys_tilesize>>1);
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  
  player->speed=40.0*(1.0-player->skill)+80.0*player->skill;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cputargetrow=BGROWC;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x808080ff;
        player->tileid=0x93;
        player->soulballc=6;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x73;
        player->soulballc=7; // Unwritten rule of our universe: Witch souls have seven circles, and no other soul has more than six*.
        // [*] The soul of a monk or nun probably has seven circles too, but the proposition hasn't been tested.
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x83;
        player->soulballc=6;
      } break;
  }
}

/* Car colors.
 */
 
static uint32_t random_car_color() {
  // Nothing too bright or dark. Keep them in 0x40..0xc0.
  uint8_t r=0x40+(rand()&0x3f);
  uint8_t g=0x40+(rand()&0x3f);
  uint8_t b=0x40+(rand()&0x3f);
  // But then force higher saturation by zeroing one of the channels at random. (or 1/4 times, don't)
  switch (rand()%4) {
    case 0: r=0; break;
    case 1: g=0; break;
    case 2: b=0; break;
  }
  return (r<<24)|(g<<16)|(b<<8)|0xff;
}

/* New.
 */
 
static int _crossing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  /* Select a speed for each row.
   */
  const double rowspeedlo=20.0;
  const double rowspeedhi=90.0;
  int row=BGROWC;
  while (row-->0) {
    double v=rowspeedlo+((rand()&0xffff)*(rowspeedhi-rowspeedlo))/65535.0;
    if (rand()&1) v=-v;
    BATTLE->rowspeed[row]=v;
  }
  
  /* Generate background vertices.
   */
  struct egg_render_tile *vtx=BATTLE->bgvtxv;
  int y=(FBH>>1)-((NS_sys_tilesize*BGROWC)>>1)+(NS_sys_tilesize>>1);
  int yi=0;
  int linespacing=FBW/BGLINEC;
  for (;yi<BGROWC;yi++,y+=NS_sys_tilesize) {
    BATTLE->rowyv[yi]=y;
    int x=NS_sys_tilesize>>1;
    int xi=BGCOLC;
    for (;xi-->0;x+=NS_sys_tilesize,vtx++) {
      vtx->x=x;
      vtx->y=y;
      vtx->tileid=0x68;
      vtx->xform=rand()&7;
    }
    if (yi<BGROWC-1) {
      int liney=y+(NS_sys_tilesize>>1);
      for (xi=BGLINEC,x=linespacing>>1;xi-->0;x+=linespacing,vtx++) {
        vtx->x=x;
        vtx->y=liney;
        vtx->tileid=0x5b;
        vtx->xform=rand()&(EGG_XFORM_XREV|EGG_XFORM_YREV);
      }
    }
  }
  if (vtx!=BATTLE->bgvtxv+BGVTXC) {
    fprintf(stderr,"!!PANIC!! Generated wrong count of vertices; might have clobbered memory. expect=%p actual=%p\n",BATTLE->bgvtxv+BGVTXC,vtx);
    return -1;
  }
  
  /* Generate cars.
   */
  const double proxlimit=50.0;
  struct car *car=BATTLE->carv;
  int carc=0;
  for (;carc<CARC;carc++,car++) {
    car->row=carc%BGROWC;
    car->y=BATTLE->rowyv[car->row];
    car->tileid=0x78+(rand()&3);
    car->color=random_car_color();
    for (;;) {
      int tooclose=0;
      car->x=rand()%FBW;
      const struct car *other=BATTLE->carv;
      int i=carc;
      for (;i-->0;other++) {
        if (other->row!=car->row) continue;
        double dx=other->x-car->x;
        if ((dx>-proxlimit)&&(dx<proxlimit)) {
          tooclose=1;
          break;
        }
      }
      if (!tooclose) break;
    }
  }
  
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
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If we've set (cputargetrow) forward, walk straight up.
   */
  double targety;
  if (player->cputargetrow<0) targety=BATTLE->rowyv[0]-NS_sys_tilesize;
  else if (player->cputargetrow>=BGROWC) targety=BATTLE->rowyv[BGROWC-1]+NS_sys_tilesize;
  else targety=BATTLE->rowyv[player->cputargetrow];
  if (player->y>targety) {
    player->indy=-1;
    player->indx=0;
    return;
  }
  
  /* If we're on the last row, the next move is obvious.
   */
  if (player->cputargetrow<=0) {
    player->cputargetrow=-1;
    player->indy=-1;
    player->indx=0;
    return;
  }
  
  /* Consider traffic on the next row.
   * If it looks reasonably clear, start walking.
   * Also, while we're iterating cars, find the nearest left and right on my row.
   */
  int nextrow=player->cputargetrow-1;
  int nextleft=(BATTLE->rowspeed[nextrow]<0.0);
  const struct car *nearl=0,*nearr=0,*nearnext=0;
  const struct car *car=BATTLE->carv;
  int i=CARC;
  for (;i-->0;car++) {
    if (car->row==nextrow) {
      if (nextleft) {
        if ((car->x>player->x-NS_sys_tilesize)&&(!nearnext||(car->x<nearnext->x))) nearnext=car;
      } else {
        if ((car->x<player->x+NS_sys_tilesize)&&(!nearnext||(car->x>nearnext->x))) nearnext=car;
      }
    } else if (car->row==player->cputargetrow) {
      if ((car->x<player->x)&&(!nearl||(car->x>nearl->x))) nearl=car;
      else if ((car->x>player->x)&&(!nearr||(car->x<nearr->x))) nearr=car;
    }
  }
  if (!nearnext) { // Next lane is clear, let's do it!
    player->cputargetrow--;
    return;
  }
  double dnear=nearnext->x-player->x;
  if (!nextleft) dnear=-dnear;
  if (dnear>60.0) { // Maybe we should involve rowspeed and player->speed in this estimation.
    player->cputargetrow--;
    return;
  }
  // Another thing we might consider: If we don't think it's good to advance, but we're about to get run over, maybe advance anyway.
  // Frog does end up squashed at the screen's edge sometimes, just standing there.
  
  /* We're not advancing yet.
   * If there's headroom in the current row's direction of travel, walk that way instead.
   */
  player->indx=player->indy=0;
  if (BATTLE->rowspeed[player->cputargetrow]<0.0) { // Our lane is moving leftward.
    if (!nearl||(player->x-nearl->x>=30.0)) player->indx=-1;
  } else { // Our lane is moving rightward.
    if (!nearr||(nearr->x-player->x>=30.0)) player->indx=1;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* If dead, there's not much for us to do.
   */
  if (player->dead) {
    player->dethklok+=elapsed;
    return;
  }
  
  /* If the battle's over, stop doing things.
   */
  if (battle->outcome>-2) {
    player->animframe=0;
    return;
  }

  /* Walk if we're walking.
   */
  if (player->indx||player->indy) {
    player->x+=player->speed*elapsed*player->indx;
    player->y+=player->speed*elapsed*player->indy;
    if (player->x<8.0) player->x=8.0;
    else if (player->x>FBW-8.0) player->x=FBW-8.0;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.125;
      if (++(player->animframe)>=4) player->animframe=0;
    }
    if (player->indx>0) player->xform=0;
    else if (player->indx<0) player->xform=EGG_XFORM_XREV;
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
  
  /* Did we get run over?
   */
  const double thresh=12.0;
  const double thresh2=thresh*thresh;
  const struct car *car=BATTLE->carv;
  int i=CARC;
  for (;i-->0;car++) {
    double dy=car->y-player->y;
    if ((dy<=-thresh)||(dy>=thresh)) continue;
    double dx=car->x-player->x;
    if ((dx<=-thresh)||(dx>=thresh)) continue;
    double d2=dx*dx+dy*dy;
    if (d2>thresh2) continue;
    bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->dead=1;
    return;
  }
  
  /* Did we reach the goal?
   */
  if (player->y<GOALY) {
    bm_sound_pan(RID_sound_treasure,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->win=1;
  }
}

/* Update.
 */
 
static void _crossing_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  struct car *car=BATTLE->carv;
  for (i=CARC;i-->0;car++) {
    double dx=BATTLE->rowspeed[car->row]*elapsed;
    car->x+=dx;
    if (car->x<-10.0) {
      car->x=FBW+8.0;
      car->tileid=0x78+(rand()&3);
      car->color=random_car_color();
    } else if (car->x>FBW+10.0) {
      car->x=-8.0;
      car->tileid=0x78+(rand()&3);
      car->color=random_car_color();
    }
  }

  /* Game ends when one player has reached the goal, or when both are dead.
   * Narrowly possible for both to reach it on the same frame.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->win&&r->win) battle->outcome=0;
    else if (l->win) battle->outcome=1;
    else if (r->win) battle->outcome=-1;
    else if (l->dead&&r->dead) battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int dstx=(int)player->x;
  int dsty=(int)player->y;
  if (player->dead) {
    int tileid=((int)(player->dethklok*10.0))%6;
    switch (tileid) {
      case 0: tileid=0xa6; break;
      case 1: tileid=0xa7; break;
      case 2: tileid=0xa8; break;
      case 3: tileid=0xa9; break;
      case 4: tileid=0xa8; break;
      case 5: tileid=0xa7; break;
    }
    graf_set_image(g_graf,RID_image_hero);
    double r=player->dethklok*100.0;
    double t=0.0;
    double dt=(M_PI*2.0)/(double)player->soulballc;
    int i=player->soulballc;
    for (;i-->0;t+=dt) {
      int x=dstx+(int)(r*cos(t));
      int y=dsty+(int)(r*sin(t));
      graf_tile(g_graf,x,y,tileid,0);
    }
    graf_set_image(g_graf,RID_image_battle_meadow);
    return;
  }
  uint8_t tileid=player->tileid;
  switch (player->animframe) {
    case 1: tileid+=1; break;
    case 3: tileid+=2; break;
  }
  graf_tile(g_graf,dstx,dsty,tileid,player->xform);
}

/* Render.
 */
 
static void _crossing_render(struct battle *battle) {

  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(g_graf,RID_image_battle_meadow);
  graf_tile_batch(g_graf,BATTLE->bgvtxv,BGVTXC);
  
  struct car *car=BATTLE->carv;
  int i=CARC;
  for (;i-->0;car++) {
    uint8_t xform=0;
    if (BATTLE->rowspeed[car->row]<0.0) xform=EGG_XFORM_XREV;
    graf_fancy(g_graf,(int)car->x,(int)car->y,car->tileid,xform,0,NS_sys_tilesize,0,car->color);
  }
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_crossing={
  .name="crossing",
  .objlen=sizeof(struct battle_crossing),
  .id=115,
  .strix_name=390,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_dpad,
  .imageid_default=0,
  .del=_crossing_del,
  .init=_crossing_init,
  .update=_crossing_update,
  .render=_crossing_render,
};

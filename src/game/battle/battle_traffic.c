/* battle_traffic.c
 * Stand in one of four quadrants of an intersection to pass two of four lanes of traffic.
 */

#include "game/bellacopia.h"

// Unlike our general 16-pixel tiles, this game uses 12-pixel tiles.
#define TILESIZE 12
#define COLC 12
#define ROWC 12

#define CAR_LIMIT 16 /* Per field. */

struct battle_traffic {
  struct battle hdr;
  int choice;
  int texid; // Scratch texture, to get clipping.
  double playtime;
  
  /* We run two independent instances of the game, which are entirely wrapped in "player".
   */
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    int col,row; // 0,1
    uint8_t tileid;
    int dstx,dsty; // Top-left corner of the full field.
    double spawnclock;
    double spawninterval;
    int honkc,passc;
    
    // Tabulated score:
    int passpoints;
    int honkpoints;
    int score;
    
    // CPU player:
    double moveclock;
    int mistakep,mistakec; // Every so many moves, we'll deliberately go the wrong way.
    
    struct car {
      double x,y; // Field pixels.
      double ndx,ndy; // Constant preferred direction. One is zero and the other -1 or 1.
      double dx,dy; // Actual velocity.
      uint8_t tileid;
      uint8_t xform; // Must agree with (ndx,ndy). Natural orientation is down.
      uint32_t color;
      double brakeclock;
      double headlightclock;
      double honkclock;
      int defunct;
    } carv[CAR_LIMIT];
    int carc;
    
  } playerv[2];
};

#define BATTLE ((struct battle_traffic*)battle)

/* Delete.
 */
 
static void _traffic_del(struct battle *battle) {
  egg_texture_del(BATTLE->texid);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  const int fldw=TILESIZE*COLC;
  const int fldh=TILESIZE*ROWC;
  player->dsty=(FBH>>1)-(fldh>>1)+10;
  player->spawninterval=0.250*(1.0-player->skill)+0.500*player->skill;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->dstx=(FBW-(fldw<<1))/3;
  } else { // Right.
    player->who=1;
    player->dstx=fldw+((FBW-(fldw<<1))*2)/3;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    player->mistakec=1+(int)(player->skill*10.0);
    player->mistakep=rand()%player->mistakec;
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x0e;
      } break;
    case NS_face_dot: {
        player->tileid=0x0c;
      } break;
    case NS_face_princess: {
        player->tileid=0x0d;
      } break;
  }
}

/* New.
 */
 
static int _traffic_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  BATTLE->texid=egg_texture_new();
  if (egg_texture_load_raw(BATTLE->texid,COLC*TILESIZE,ROWC*TILESIZE,COLC*TILESIZE*4,0,0)<0) return -1;
  
  BATTLE->playtime=20.0;
  
  return 0;
}

/* Move player.
 */
 
static void player_move(struct battle *battle,struct player *player,int dx,int dy) {
  player->col+=dx;
  if (player->col<0) player->col=0;
  else if (player->col>1) player->col=1;
  player->row+=dy;
  if (player->row<0) player->row=0;
  else if (player->row>1) player->row=1;
  bm_sound_pan(RID_sound_uimotion,player->who?0.250:-0.250);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1,0);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1,0);
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) player_move(battle,player,0,-1);
  if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) player_move(battle,player,0,1);
}

/* Update CPU player.
 * There's a dirty little secret to this game: If you just rotate the dpad constantly, you'll wind up playing perfectly.
 * Our CPU player will play dumb and pretend it doesn't know that.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  // If my moveclock is set, tick it down and nothing else.
  if (player->moveclock>0.0) {
    player->moveclock-=elapsed;
  } else {
    /* Count the cars in the four lanes. If any move puts us in a lane with fewer cars, do it.
     * We don't need to fiddle with their floating-point positions. Determine their lane based on xform.
     * Cars beyond the traffic box don't count.
     */
    const int midx=(COLC*TILESIZE)>>1;
    const int midy=(ROWC*TILESIZE)>>1;
    int lc=0,rc=0,tc=0,bc=0;
    struct car *car=player->carv;
    int i=player->carc;
    for (;i-->0;car++) {
      if (car->defunct) continue;
      switch (car->xform) {
        case 0: { // Down, ie Left lane.
            if (car->y<midy) lc++;
          } break;
        case EGG_XFORM_SWAP|EGG_XFORM_XREV: { // Right, ie Bottom lane.
            if (car->x<midx) bc++;
          } break;
        case EGG_XFORM_SWAP|EGG_XFORM_YREV: { // Left, ie Top lane.
            if (car->x>midx) tc++;
          } break;
        case EGG_XFORM_XREV|EGG_XFORM_YREV: { // Up, ie Right lane.
            if (car->y>midy) rc++;
          } break;
      }
    }
    
    // Make an occasional mistake, to keep it spicy.
    // When we strike this, there's good chance it will stick.
    // So when a car honks at us, it will also increment a zero mistakep.
    if (!player->mistakep) {
      int tmp;
      tmp=lc; lc=rc; rc=tmp;
      tmp=tc; tc=bc; bc=tc;
    }
    
    int dx=0,dy=0,gainx=0,gainy=0;
    if (player->col) { // In right lane.
      dx=-1;
      gainx=rc-lc;
    } else { // In left lane.
      dx=1;
      gainx=lc-rc;
    }
    if (player->row) { // In bottom lane.
      dy=-1;
      gainy=bc-tc;
    } else { // In top lane.
      dy=1;
      gainy=tc-bc;
    }
    if ((gainx>gainy)&&(gainx>0)) {
      player_move(battle,player,dx,0);
      player->moveclock=0.500;
      if (++(player->mistakep)>=player->mistakec) player->mistakep=0;
    } else if ((gainy>gainx)&&(gainy>0)) {
      player_move(battle,player,0,dy);
      player->moveclock=0.500;
      if (++(player->mistakep)>=player->mistakec) player->mistakep=0;
    }
  }
}

/* Nonzero if the given cell has no cars nearby.
 */
 
static int traffic_clearish(struct battle *battle,struct player *player,int col,int row) {
  double l=col*TILESIZE-10.0;
  double t=row*TILESIZE-10.0;
  double r=l+TILESIZE+20.0;
  double b=t+TILESIZE+20.0;
  struct car *car=player->carv;
  int i=player->carc;
  for (;i-->0;car++) {
    if (car->defunct) continue;
    if (car->x<l) continue;
    if (car->x>r) continue;
    if (car->y<t) continue;
    if (car->y>b) continue;
    return 0;
  }
  return 1;
}

/* Spawn a car.
 */
 
static void traffic_spawn_car(struct battle *battle,struct player *player) {
  if (player->carc>=CAR_LIMIT) return;
  
  /* Don't create a car at any intake that's already clogged.
   */
  int xb=COLC>>1,yb=ROWC>>1;
  int xa=xb-1,ya=yb-1;
  struct candidate {
    int x,y,ndx,ndy;
  } candidatev[4];
  int candidatec=0;
  if (traffic_clearish(battle,player,xa,0)) candidatev[candidatec++]=(struct candidate){xa,0,0,1};
  if (traffic_clearish(battle,player,xb,ROWC-1)) candidatev[candidatec++]=(struct candidate){xb,ROWC-1,0,-1};
  if (traffic_clearish(battle,player,0,ya)) candidatev[candidatec++]=(struct candidate){0,yb,1,0};
  if (traffic_clearish(battle,player,COLC-1,ya)) candidatev[candidatec++]=(struct candidate){COLC-1,ya,-1,0};
  if (candidatec<1) return; // Don't make one if fully clogged. This is unlikely.
  int candidatep=rand()%candidatec;
  
  struct car *car=player->carv+player->carc++;
  car->x=candidatev[candidatep].x*TILESIZE+(TILESIZE>>1);
  car->y=candidatev[candidatep].y*TILESIZE+(TILESIZE>>1);
  car->ndx=candidatev[candidatep].ndx;
  car->ndy=candidatev[candidatep].ndy;
  if (car->ndx<0) car->xform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
  else if (car->ndx>0) car->xform=EGG_XFORM_SWAP|EGG_XFORM_XREV;
  else if (car->ndy<0) car->xform=EGG_XFORM_XREV|EGG_XFORM_YREV;
  else car->xform=0;
  const double speed=60.0;
  car->dx=car->ndx*speed;
  car->dy=car->ndy*speed;
  car->tileid=0x1c;
  switch (rand()%6) {
    case  0: car->color=0xc06020ff; break;
    case  1: car->color=0x308050ff; break;
    case  2: car->color=0x40ee60ff; break;
    case  3: car->color=0x102040ff; break;
    case  4: car->color=0x502070ff; break;
    default: car->color=0x405060ff; break;
  }
  car->brakeclock=0.0;
  car->headlightclock=0.0;
  car->defunct=0;
}

/* Update car.
 */
 
static void car_update(struct battle *battle,struct player *player,struct car *car,double elapsed) {
  if (car->defunct) return;
  
  /* Check for blockages.
   */
  int blocked=0;
  const double radius=4.0;
  const double far=30.0;
  double bl=car->x-radius,bt=car->y-radius,br=car->x+radius,bb=car->y+radius;
  switch (car->xform) {
    case 0: bt+=12.0; bb+=far; break;
    case EGG_XFORM_SWAP|EGG_XFORM_XREV: bl+=12.0; br+=far; break;
    case EGG_XFORM_SWAP|EGG_XFORM_YREV: bl-=far; br-=12.0; break;
    case EGG_XFORM_XREV|EGG_XFORM_YREV: bt-=far; bb-=12.0; break;
  }
  double herox=(((COLC>>1)-1)+player->col)*TILESIZE+(TILESIZE>>1);
  double heroy=(((ROWC>>1)-1)+player->row)*TILESIZE+(TILESIZE>>1);
  if ((herox>=bl)&&(herox<=br)&&(heroy>=bt)&&(heroy<=bb)) {
    blocked=1;
  } else {
    struct car *other=player->carv;
    int i=player->carc;
    for (;i-->0;other++) {
      if (other==car) continue;
      if (other->x<bl) continue;
      if (other->x>br) continue;
      if (other->y<bt) continue;
      if (other->y>bb) continue;
      blocked=1;
      break;
    }
  }
  
  if (blocked) {
    car->brakeclock=0.250;
    const double brakerate=150.0;
    if (car->dx<0.0) {
      if ((car->dx+=brakerate*elapsed)>=0.0) car->dx=0.0;
    } else if (car->dx>0.0) {
      if ((car->dx-=brakerate*elapsed)<=0.0) car->dx=0.0;
    } else if (car->dy<0.0) {
      if ((car->dy+=brakerate*elapsed)>=0.0) car->dy=0.0;
    } else if (car->dy>0.0) {
      if ((car->dy-=brakerate*elapsed)<=0.0) car->dy=0.0;
    } else if (car->honkclock<=0.0) {
      car->honkclock=1.0+((rand()&0xffff)*2.0)/65535.0;
    } else if ((car->honkclock-=elapsed)<=0.0) {
      bm_sound_pan(RID_sound_honk+(rand()&3),player->who?0.500:-0.500);
      car->headlightclock=0.500;
      player->honkc++;
      if (!player->mistakep) player->mistakep=1;
    }
    
  } else {
    car->honkclock=0.0;
    // Not blocked. If we're not at full speed yet, accelerate.
    const double runrate=60.0;
    const double gasrate=200.0;
    if (car->ndx<0.0) {
      if ((car->dx-=gasrate*elapsed)<=-runrate) car->dx=-runrate;
    } else if (car->ndx>0.0) {
      if ((car->dx+=gasrate*elapsed)>=runrate) car->dx=runrate;
    } else if (car->ndy<0.0) {
      if ((car->dy-=gasrate*elapsed)<=-runrate) car->dy=-runrate;
    } else if (car->ndy>0.0) {
      if ((car->dy+=gasrate*elapsed)>=runrate) car->dy=runrate;
    } else {
      car->defunct=1;
    }
  }
  
  if (car->brakeclock>0.0) car->brakeclock-=elapsed;
  if (car->headlightclock>0.0) car->headlightclock-=elapsed;
  
  car->x+=car->dx*elapsed;
  car->y+=car->dy*elapsed;
  
  if (
    (car->x<-TILESIZE)||
    (car->x>(COLC+1)*TILESIZE)||
    (car->y<-TILESIZE)||
    (car->y>(ROWC+1)*TILESIZE)
  ) {
    car->defunct=1;
    player->passc++;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Tick the spawn clock and make a new car when it lapses.
   */
  if ((player->spawnclock-=elapsed)<=0.0) {
    player->spawnclock+=player->spawninterval;
    traffic_spawn_car(battle,player);
  }
  
  /* Update cars.
   */
  struct car *car=player->carv;
  int i=player->carc;
  for (;i-->0;car++) {
    car_update(battle,player,car,elapsed);
  }
  
  /* Drop defunct cars.
   */
  for (car=player->carv+player->carc-1,i=player->carc;i-->0;car--) {
    if (!car->defunct) continue;
    player->carc--;
    memmove(car,car+1,sizeof(struct car)*(player->carc-i));
  }
}

/* Tabulate score.
 */
 
static void player_tabulate(struct battle *battle,struct player *player) {
  player->honkpoints=-5*player->honkc;
  player->passpoints=player->passc;
  player->score=player->honkpoints+player->passpoints;
  if (player->score<0) player->score=0;
  else if (player->score>999) player->score=999;
}

/* Update.
 */
 
static void _traffic_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  if ((BATTLE->playtime-=elapsed)<=0.0) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    player_tabulate(battle,l);
    player_tabulate(battle,r);
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 * Don't draw the background. That's done in an early pass, so the two players can share a batch of fancies.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  graf_set_output(&g.graf,BATTLE->texid);
  graf_set_image(&g.graf,RID_image_battle_traffic);
  
  graf_decal(&g.graf,0,0,0,0,COLC*TILESIZE,ROWC*TILESIZE);

  const int col0=(COLC>>1)-1;
  const int row0=(ROWC>>1)-1;
  int x,y;
  x=(player->col+col0)*TILESIZE+(TILESIZE>>1);
  y=(player->row+row0)*TILESIZE+(TILESIZE>>1);
  graf_fancy(&g.graf,x,y,player->tileid,0,0,TILESIZE,0,0x808080ff);
  
  struct car *car=player->carv;
  int i=player->carc;
  for (;i-->0;car++) {
    if (car->defunct) continue;
    x=(int)car->x;
    y=(int)car->y;
    graf_fancy(&g.graf,x,y,car->tileid,car->xform,0,TILESIZE,0,car->color);
    if (car->brakeclock>0.0) {
      int bx=x,by=y;
      switch (car->xform) {
        case 0: by-=6; break;
        case EGG_XFORM_XREV|EGG_XFORM_SWAP: bx-=6; break;
        case EGG_XFORM_YREV|EGG_XFORM_SWAP: bx+=6; break;
        case EGG_XFORM_XREV|EGG_XFORM_YREV: by+=6; break;
      }
      graf_fancy(&g.graf,bx,by,car->tileid+1,car->xform,0,TILESIZE,0,0x808080ff);
    }
    if (car->headlightclock>0.0) {
      int bx=x,by=y;
      switch (car->xform) {
        case 0: by+=6; break;
        case EGG_XFORM_XREV|EGG_XFORM_SWAP: bx+=6; break;
        case EGG_XFORM_YREV|EGG_XFORM_SWAP: bx-=6; break;
        case EGG_XFORM_XREV|EGG_XFORM_YREV: by-=6; break;
      }
      graf_fancy(&g.graf,bx,by,car->tileid+2,car->xform,0,TILESIZE,0,0x808080ff);
    }
  }
  
  graf_set_output(&g.graf,1);
  graf_set_input(&g.graf,BATTLE->texid);
  if (battle->outcome>-2) graf_set_tint(&g.graf,0x00000080);
  graf_decal(&g.graf,player->dstx,player->dsty,0,0,COLC*TILESIZE,ROWC*TILESIZE);
  graf_set_tint(&g.graf,0);
  
  graf_set_image(&g.graf,RID_image_battle_traffic);
  y=player->dsty-8;
  graf_tile(&g.graf,player->dstx+20,y,0x0f,0);
  graf_tile(&g.graf,player->dstx+80,y,0x1f,0);
  
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_set_tint(&g.graf,0xff0000ff);
  x=player->dstx+33;
  if (player->honkc>=100) { graf_tile(&g.graf,x,y,'0'+(player->honkc/100)%10,0); x+=8; }
  if (player->honkc>=10) { graf_tile(&g.graf,x,y,'0'+(player->honkc/10)%10,0); x+=8; }
  graf_tile(&g.graf,x,y,'0'+player->honkc%10,0);
  graf_set_tint(&g.graf,0x008000ff);
  x=player->dstx+93;
  if (player->passc>=100) { graf_tile(&g.graf,x,y,'0'+(player->passc/100)%10,0); x+=8; }
  if (player->passc>=10) { graf_tile(&g.graf,x,y,'0'+(player->passc/10)%10,0); x+=8; }
  graf_tile(&g.graf,x,y,'0'+player->passc%10,0);
  graf_set_tint(&g.graf,0);
  
  if (battle->outcome>-2) {
    const int fldw=COLC*TILESIZE;
    const int fldh=ROWC*TILESIZE;
    int digitc;
    if (player->score>=100) digitc=3;
    else if (player->score>=10) digitc=2;
    else digitc=1;
    x=player->dstx+(fldw>>1)-(digitc*4)+4;
    y=player->dsty+(fldh>>1);
    if (player->score>=100) { graf_tile(&g.graf,x,y,'0'+(player->score/100)%10,0); x+=8; }
    if (player->score>=10) { graf_tile(&g.graf,x,y,'0'+(player->score/10)%10,0); x+=8; }
    graf_tile(&g.graf,x,y,'0'+player->score%10,0);
  }
}

/* Render.
 */
 
static void _traffic_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_traffic);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_traffic={
  .name="traffic",
  .objlen=sizeof(struct battle_traffic),
  .strix_name=183,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_traffic_del,
  .init=_traffic_init,
  .update=_traffic_update,
  .render=_traffic_render,
};

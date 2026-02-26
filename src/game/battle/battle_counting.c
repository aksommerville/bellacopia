/* battle_counting.c
 * Noninteractive scene with lots of moving things and you have to count one class of them.
 */

#include "game/bellacopia.h"

#define ANIMAL_LIMIT 40
#define ANIMALC_MIN 5
#define ANIMAL_X_LO  50.0
#define ANIMAL_X_HI 270.0
#define ANIMAL_Y_LO  50.0
#define ANIMAL_Y_HI 120.0
#define ANIMAL_SPEED_LO  8.0
#define ANIMAL_SPEED_HI 30.0
#define CPU_STROKE_TIME 0.500

struct battle_counting {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    int dstx;
    uint8_t tileid;
    uint8_t xform;
    int guess;
    int committed;
    int phony; // CPU player when the other is human -- does nothing.
    int cpuguess;
    double cpuclock;
  } playerv[2];
  
  int prompt_texid;
  int promptw,prompth;
  
  struct animal {
    double x,y; // Framebuffer pixels.
    double dx,dy; // px/s
    double animclock;
    int animframe;
    uint8_t tileid;
    uint8_t xform;
    char colorname; // 'r','g','b'
    uint32_t color;
  } animalv[ANIMAL_LIMIT];
  int animalc;
  
  int catc,bugc,pigc,birdc; // Add up to (animalc).
  int redc,greenc,bluec; // Add up to (animalc).
  int cv[12]; // (cat,bug,pig,bird)*3+(red,green,blue)
  char targetcolor; // 'r','g','b'
  uint8_t targettile;
  int targetc;
  
  double clock;
  int useclock;
};

#define BATTLE ((struct battle_counting*)battle)

/* Delete.
 */
 
static void _counting_del(struct battle *battle) {
  egg_texture_del(BATTLE->prompt_texid);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->cpuclock=1.500; // A wee delay before the first stroke.
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->dstx=20;
    player->xform=0;
  } else { // Right.
    player->who=1;
    player->dstx=FBW-20-32;
    player->xform=EGG_XFORM_XREV;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    // (BATTLE->targetc) is established. Make my guess.
    if (((rand()&0xffff)/65535.0)<=player->skill) {
      player->cpuguess=BATTLE->targetc;
    } else {
      player->cpuguess=rand()%8+1;
      if (player->cpuguess>=BATTLE->targetc) player->cpuguess++;
    }
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x59;
      } break;
    case NS_face_dot: {
        player->tileid=0x19;
      } break;
    case NS_face_princess: {
        player->tileid=0x39;
      } break;
  }
}

/* Init animal.
 */
 
static uint8_t channel_near(int focus) {
  focus+=(rand()&0x1f)-16;
  if (focus<0) return 0;
  if (focus>0xff) return 0xff;
  return focus;
}
 
static uint32_t reddish() {
  uint8_t r=channel_near(0xf0);
  uint8_t g=channel_near(0x10);
  uint8_t b=channel_near(0x10);
  return (r<<24)|(g<<16)|(b<<8)|0xff;
}

static uint32_t greenish() {
  uint8_t r=channel_near(0x10);
  uint8_t g=channel_near(0x80);
  uint8_t b=channel_near(0x10);
  return (r<<24)|(g<<16)|(b<<8)|0xff;
}

static uint32_t blueish() {
  uint8_t r=channel_near(0x10);
  uint8_t g=channel_near(0x10);
  uint8_t b=channel_near(0xf0);
  return (r<<24)|(g<<16)|(b<<8)|0xff;
}
 
static void animal_init(struct battle *battle,struct animal *animal) {
  animal->x=ANIMAL_X_LO+((rand()&0xffff)*(ANIMAL_X_HI-ANIMAL_X_LO))/65535.0;
  animal->y=ANIMAL_Y_LO+((rand()&0xffff)*(ANIMAL_Y_HI-ANIMAL_Y_LO))/65535.0;
  double speed=ANIMAL_SPEED_LO+((rand()&0xffff)*(ANIMAL_SPEED_HI-ANIMAL_SPEED_LO))/65535.0;
  double angle=(((rand()&0xffff)-0x8000)*M_PI)/32768.0;
  animal->dx=speed*cos(angle);
  animal->dy=speed*sin(angle);
  if (animal->dx<0.0) animal->xform=EGG_XFORM_XREV;
  else animal->xform=0;
  animal->animclock=((rand()&0xffff)*0.250)/65535.0;
  animal->animframe=rand()&3;
  int cvp;
  switch (rand()%4) {
    case 0: animal->tileid=0x13; BATTLE->catc++; cvp=0; break;
    case 1: animal->tileid=0x16; BATTLE->bugc++; cvp=3; break;
    case 2: animal->tileid=0x23; BATTLE->pigc++; cvp=6; break;
    case 3: animal->tileid=0x26; BATTLE->birdc++; cvp=9; break;
  }
  switch (rand()%3) {
    case 0: animal->color=reddish();  animal->colorname='r'; cvp+=0; BATTLE->redc++; break;
    case 1: animal->color=greenish(); animal->colorname='g'; cvp+=1; BATTLE->greenc++; break;
    case 2: animal->color=blueish();  animal->colorname='b'; cvp+=2; BATTLE->bluec++; break;
  }
  BATTLE->cv[cvp]++;
}

/* New.
 */
 
static int _counting_init(struct battle *battle) {

  // Initialize animals randomly, scaling the count per difficulty.
  BATTLE->animalc=(int)(battle_scalar_difficulty(battle)*ANIMAL_LIMIT);
  if (BATTLE->animalc<ANIMALC_MIN) BATTLE->animalc=ANIMALC_MIN;
  else if (BATTLE->animalc>ANIMAL_LIMIT) BATTLE->animalc=ANIMAL_LIMIT;
  struct animal *animal=BATTLE->animalv;
  int i=BATTLE->animalc;
  for (;i-->0;animal++) animal_init(battle,animal);
  
  // Select a target among those colors of animal with a count in 1..9.
  int targetv[12];
  int targetc=0;
  for (i=0;i<12;i++) {
    if ((BATTLE->cv[i]>=1)&&(BATTLE->cv[i]<=9)) {
      targetv[targetc++]=i;
    }
  }
  if (targetc<1) { // Narrowly possible. Pick anything, valid or not.
    targetv[0]=rand()%12;
    targetc=1;
  }
  int targetp=rand()%targetc;
  BATTLE->targetc=BATTLE->cv[targetv[targetp]];
  BATTLE->targetcolor="rgb"[targetv[targetp]%3];
  switch (targetv[targetp]/3) {
    case 0: BATTLE->targettile=0x13; break; // cat
    case 1: BATTLE->targettile=0x16; break; // bug
    case 2: BATTLE->targettile=0x23; break; // pig
    case 3: BATTLE->targettile=0x26; break; // bird
  }
  
  // Generate prompt.
  struct text_insertion insv[2]={
    {.mode='r',.r={.rid=RID_strings_battle}},
    {.mode='r',.r={.rid=RID_strings_battle}},
  };
  switch (BATTLE->targetcolor) {
    case 'r': insv[0].r.strix=191; break;
    case 'g': insv[0].r.strix=192; break;
    case 'b': insv[0].r.strix=193; break;
  }
  switch (BATTLE->targettile) {
    case 0x13: insv[1].r.strix=194; break;
    case 0x16: insv[1].r.strix=195; break;
    case 0x23: insv[1].r.strix=196; break;
    case 0x26: insv[1].r.strix=197; break;
  }
  char tmp[64];
  int tmpc=text_format_res(tmp,sizeof(tmp),RID_strings_battle,190,insv,2);
  if ((tmpc<1)||(tmpc>sizeof(tmp))) tmpc=0;
  BATTLE->prompt_texid=font_render_to_texture(0,g.font,tmp,tmpc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&BATTLE->promptw,&BATTLE->prompth,BATTLE->prompt_texid);

  // Initialize players.
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  // When there's two CPU players, the one on the left plays like a man.
  if (BATTLE->playerv[0].human) {
    if (!BATTLE->playerv[1].human) BATTLE->playerv[1].phony=1; // Dot vs monster.
  } else {
    if (BATTLE->playerv[1].human) BATTLE->playerv[0].phony=1; // Man on the right. Unusual.
    else BATTLE->playerv[1].phony=1; // Princess vs monster.
  }
  
  // Global time limit if there's a phony player.
  if (BATTLE->playerv[0].phony||BATTLE->playerv[1].phony) {
    BATTLE->useclock=1;
    BATTLE->clock=10.0;
  }
  
  return 0;
}

/* Change guess.
 */
 
static void player_move(struct battle *battle,struct player *player,int d) {
  player->guess+=d;
  if (player->guess<0) player->guess=0;
  else if (player->guess>9) player->guess=9;
  bm_sound_pan(RID_sound_uimotion,player->who?0.250:-0.250);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->committed) return;
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    bm_sound_pan(RID_sound_uiactivate,player->who?0.250:-0.250);
    player->committed=1;
  }
  if (!player->committed) {
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) player_move(battle,player,1);
    else if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) player_move(battle,player,-1);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->phony||player->committed) return;
  if ((player->cpuclock-=elapsed)>0.0) return;
  player->cpuclock+=CPU_STROKE_TIME;
  if (player->guess<player->cpuguess) player_move(battle,player,1);
  else if (player->guess>player->cpuguess) player_move(battle,player,-1);
  else {
    bm_sound_pan(RID_sound_uiactivate,player->who?0.250:-0.250);
    player->committed=1;
  }
}

/* Update animal.
 */
 
static void animal_update(struct battle *battle,struct animal *animal,double elapsed) {
  animal->x+=animal->dx*elapsed;
  if ((animal->x<ANIMAL_X_LO)&&(animal->dx<0.0)) {
    animal->dx=-animal->dx;
    animal->xform=0;
  } else if ((animal->x>ANIMAL_X_HI)&&(animal->dx>0.0)) {
    animal->dx=-animal->dx;
    animal->xform=EGG_XFORM_XREV;
  }
  animal->y+=animal->dy*elapsed;
  if ((animal->y<ANIMAL_Y_LO)&&(animal->dy<0.0)) animal->dy=-animal->dy;
  else if ((animal->y>ANIMAL_Y_HI) &&(animal->dy>0.)) animal->dy=-animal->dy;
  if ((animal->animclock-=elapsed)<=0.0) {
    animal->animclock+=0.250;
    if (++(animal->animframe)>=4) animal->animframe=0;
  }
}

/* Update.
 */
 
static void _counting_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if (BATTLE->useclock) {
    if ((BATTLE->clock-=elapsed)<=0.0) {
      if (BATTLE->playerv[0].human) battle->outcome=-1;
      else battle->outcome=1;
      return;
    }
  }
  
  // Update players. If they're all phony or committed, we're done.
  struct player *player=BATTLE->playerv;
  int i=2,done=1;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    if (player->phony||player->committed) ;
    else done=0;
  }
  if (done) {
    int lok=(BATTLE->playerv[0].guess==BATTLE->targetc);
    int rok=(BATTLE->playerv[1].guess==BATTLE->targetc);
    if (BATTLE->playerv[0].phony) { // One-player, on the right.
      battle->outcome=rok?-1:1;
    } else if (BATTLE->playerv[1].phony) { // One-player, on the left, the usual case.
      battle->outcome=lok?1:-1;
    } else if ((lok&&rok)||(!lok&&!rok)) { // Both right or both wrong -- tie.
      battle->outcome=0;
    } else if (lok) {
      battle->outcome=1;
    } else {
      battle->outcome=-1;
    }
  }
  
  struct animal *animal=BATTLE->animalv;
  for (i=BATTLE->animalc;i-->0;animal++) {
    animal_update(battle,animal,elapsed);
  }
}

/* Sort animals vertically.
 * Two passes of a cocktail sort, doesn't necessarily sort the whole list.
 */
 
static void sort_animals(struct animal *animalv,int animalc) {
  int i=1,done=1;
  for (;i<animalc;i++) {
    struct animal *a=animalv+i-1;
    struct animal *b=animalv+i;
    if (a->y>b->y) {
      struct animal tmp=*b;
      animalv[i]=*a;
      animalv[i-1]=tmp;
      done=0;
    }
  }
  if (done) return;
  for (i=animalc;i-->1;) {
    struct animal *a=animalv+i-1;
    struct animal *b=animalv+i;
    if (a->y>b->y) {
      struct animal tmp=*b;
      animalv[i]=*a;
      animalv[i-1]=tmp;
    }
  }
}

/* Render.
 */
 
static void _counting_render(struct battle *battle) {
  const int ht=NS_sys_tilesize>>1;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x406080ff);
  graf_set_image(&g.graf,RID_image_battle_labyrinth2);
  
  sort_animals(BATTLE->animalv,BATTLE->animalc);
  
  struct indicator { int x,y; } indicatorv[16];
  int indicatorc=0;
  struct animal *animal=BATTLE->animalv;
  int i=BATTLE->animalc;
  for (;i-->0;animal++) {
    uint8_t tileid=animal->tileid;
    switch (animal->animframe) {
      case 1: tileid+=1; break;
      case 3: tileid+=2; break;
    }
    graf_fancy(&g.graf,(int)animal->x,(int)animal->y,tileid,animal->xform,0,NS_sys_tilesize,0,animal->color);
    if ((battle->outcome>-2)&&(indicatorc<16)) {
      if ((animal->colorname==BATTLE->targetcolor)&&(animal->tileid==BATTLE->targettile)) {
        indicatorv[indicatorc++]=(struct indicator){(int)animal->x,(int)animal->y};
      }
    }
  }
  
  struct indicator *indicator=indicatorv;
  for (i=indicatorc;i-->0;indicator++) {
    graf_tile(&g.graf,indicator->x,indicator->y-NS_sys_tilesize,0x0e,0);
  }
  
  struct digit { int x,y; uint8_t tileid; } digitv[2];
  int digitc=0;
  struct player *player=BATTLE->playerv;
  for (i=2;i-->0;player++) {
    uint8_t tl=player->tileid;
    if (player->committed) tl+=2;
    uint8_t tr=tl+1;
    if (player->xform) {
      tl++;
      tr--;
    }
    graf_tile(&g.graf,player->dstx+ht                ,FBH-24,tl+0x00,player->xform);
    graf_tile(&g.graf,player->dstx+ht+NS_sys_tilesize,FBH-24,tr+0x00,player->xform);
    graf_tile(&g.graf,player->dstx+ht                ,FBH- 8,tl+0x10,player->xform);
    graf_tile(&g.graf,player->dstx+ht+NS_sys_tilesize,FBH- 8,tr+0x10,player->xform);
    if (!player->phony) {
      uint8_t bubbletile=0x1d;
      int ndx=53,ndy=5;
      if (player->committed) {
        bubbletile=0x3d;
        if (player->xform) ndx=59; else ndx=47;
        ndy=4;
      }
      uint8_t tl=bubbletile,tr=bubbletile+1;
      if (player->xform) {
        tl++;
        tr--;
        graf_tile(&g.graf,player->dstx-ht-NS_sys_tilesize*1+2,FBH-32,tl+0x00,player->xform);
        graf_tile(&g.graf,player->dstx-ht-NS_sys_tilesize*0+2,FBH-32,tr+0x00,player->xform);
        graf_tile(&g.graf,player->dstx-ht-NS_sys_tilesize*1+2,FBH-16,tl+0x10,player->xform);
        graf_tile(&g.graf,player->dstx-ht-NS_sys_tilesize*0+2,FBH-16,tr+0x10,player->xform);
        digitv[digitc++]=(struct digit){player->dstx-72+ndx,FBH-32+ndy,'0'+player->guess};
      } else {
        graf_tile(&g.graf,player->dstx+ht+NS_sys_tilesize*2,FBH-32,tl+0x00,player->xform);
        graf_tile(&g.graf,player->dstx+ht+NS_sys_tilesize*3,FBH-32,tr+0x00,player->xform);
        graf_tile(&g.graf,player->dstx+ht+NS_sys_tilesize*2,FBH-16,tl+0x10,player->xform);
        graf_tile(&g.graf,player->dstx+ht+NS_sys_tilesize*3,FBH-16,tr+0x10,player->xform);
        digitv[digitc++]=(struct digit){player->dstx+ndx,FBH-32+ndy,'0'+player->guess};
      }
    }
  }
  
  if (digitc) {
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_set_tint(&g.graf,0x000040ff);
    struct digit *digit=digitv;
    for (i=digitc;i-->0;digit++) {
      graf_tile(&g.graf,digit->x,digit->y,digit->tileid,0);
    }
    graf_set_tint(&g.graf,0);
  }
  
  if (BATTLE->useclock&&(BATTLE->clock>0.0)) {
    int ms=(int)(BATTLE->clock*1000.0);
    int sec=ms/1000+1;
    if (sec<1) sec=1; else if (sec>9) sec=9;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_tile(&g.graf,FBW>>1,FBH-20,'0'+sec,0);
  }
  
  graf_set_input(&g.graf,BATTLE->prompt_texid);
  graf_decal(&g.graf,(FBW>>1)-(BATTLE->promptw>>1),10,0,0,BATTLE->promptw,BATTLE->prompth);
}

/* Type definition.
 */
 
const struct battle_type battle_type_counting={
  .name="counting",
  .objlen=sizeof(struct battle_counting),
  .strix_name=179,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_counting_del,
  .init=_counting_init,
  .update=_counting_update,
  .render=_counting_render,
};

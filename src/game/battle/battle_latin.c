/* battle_latin.c
 * Shown a picture of a plant or animal, and you must pick among three options for its Linnean name.
 * Difficulty is expressed as a required correct count, and error tolerance.
 * We're asymmetric:
 *  - One player: Just pick the name, no opponent.
 *  - Two players: Buzz in, then pick, think Jeopardy.
 *  - Two CPU players operates like one player, we pick one of the CPU players to actually play.
 * We're missing a Dot-vs-Princess mode for one player, which I guess could be implemented without much fuss.
 * There's room for nine subjects:
 *  - Dog:          Canis familiaris
 *  - Cat:          Felis catus
 *  - Elephant:     Elephas maximus
 *  - Red Kangaroo: Osphranter rufus
 *  - Beet:         Beta vulgaris
 *  - Tulip:        Tulipa gesneriana (several species but this I'm told is the common garden one)
 *  - Bald Eagle:   Haliaeetus leucocephalus (NB "eagle" is taxonomically dubious)
 *  - GW Shark:     Carcharodon carcharias
 *  - Mushroom:     Agaricus bisporus (The White Button Mushroom, same species as Portobello and Cremini)
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_visbits.h"

#define STAR_LIMIT 5
#define POOP_LIMIT 5
#define VTX_LIMIT 80
#define TILEID_BLANK 0x06 /* Top row, right of Dot's faces. Will always be blank. */
#define TIMER_LO  5.000
#define TIMER_HI 15.000
#define PLAN_LIMIT 10

static const struct subject {
  uint8_t tileid_pic;
  uint8_t tileid_name;
} subjectv[]={
  {0x70,0x0a}, // Dog
  {0x75,0x1a}, // Cat
  {0x7a,0x2a}, // Elephant
  {0xa0,0x3a}, // Red Kangaroo
  {0xa5,0x4a}, // Beet
  {0xaa,0x5a}, // Tulip
  {0xd0,0x6a}, // Bald Eagle
  {0xd5,0x54}, // Great White Shark
  {0xda,0x64}, // Mushroom
};

struct battle_latin {
  struct battle hdr;
  int mode; // Human player count: 0,1,2. We have these three distinct modes of operation.
  
  struct player {
    int who;
    int human;
    uint8_t tileid;
    int starc,poopc; // score
    int star_limit; // So many stars and you win. Must be >0.
    int poop_limit; // So many poops and the *next* wrong answer loses. Zero is legal.
    int fingerp; // 0,1,2
    int pvinput;
    int buzz,sad; // Controls portrait tiles.
    int plan[PLAN_LIMIT]; // 0=wrong, 1=right; for CPU players
    int planp;
    int next_guess;
    double cpuclock;
    struct egg_render_tile *vportrait; // 4
    struct egg_render_tile *vfinger; // 1
    struct egg_render_tile *vstar; // star_limit
    struct egg_render_tile *vpoop; // poop_limit
  } playerv[2];
  int playerc; // 1,2. How many virtual participants, not necessarily human.
  
  /* We draw everything with tiles, and the scene barely changes at all during play.
   * So it makes a lot of sense to prepare the full batch in advance.
   * The first 35 are reserved: 15 for the reference picture, then three of six for the options, then two for the clock.
   */
  struct egg_render_tile vtxv[VTX_LIMIT];
  int vtxc;
  
  uint32_t subjects_used; // bits, 1<<index. Beware that there may be more answer opportunities than subjects. When it saturates, reset.
  int correctp; // 0,1,2. Which option is correct.
  int dirty; // Players can set to force a check of score at the end of the update.
  double timer; // One-player games have a timer per subject.
  double timerk; // Constant, per difficulty. If <=0, unlimited.
};

#define BATTLE ((struct battle_latin*)battle)

/* Delete.
 */
 
static void _latin_del(struct battle *battle) {
}

/* Choose a new subject at random and populate picture, options, and correctp.
 */
 
static void latin_populate_option(struct battle *battle,int optionp,uint8_t tileid) {
  if ((optionp<0)||(optionp>2)) return;
  struct egg_render_tile *vtx=BATTLE->vtxv+15+6*optionp;
  int i=6;
  for (;i-->0;vtx++,tileid++) vtx->tileid=tileid;
}
 
static void latin_next_subject(struct battle *battle) {
  int subjectc=sizeof(subjectv)/sizeof(subjectv[0]);
  
  // If every subject has already been played, reset and allow them all again.
  if (BATTLE->subjects_used==((1<<subjectc)-1)) {
    BATTLE->subjects_used=0;
  }
  
  // Pick one randomly that hasn't been used. If it runs unreasonably long, allow one we've used already. Not the end of the world.
  int panic=100,subjectp=0;
  while (panic-->0) {
    subjectp=rand()%subjectc;
    if (!(BATTLE->subjects_used&(1<<subjectp))) break;
  }
  BATTLE->subjects_used|=1<<subjectp;
  const struct subject *subject=subjectv+subjectp;
  
  // Apply to picture. These are the first 15 vertices.
  struct egg_render_tile *vtx=BATTLE->vtxv;
  uint8_t tileid0=subject->tileid_pic;
  int yi=3;
  for (;yi-->0;tileid0+=0x10) {
    uint8_t tileid=tileid0;
    int xi=5;
    for (;xi-->0;tileid++,vtx++) {
      vtx->tileid=tileid;
    }
  }
  
  // Pick a random index for the two wrong options. Ensure we've picked 3 different subjects, but do give up after a sensible panic interval.
  int wrongp0,wrongp1;
  for (panic=100;panic-->0;) {
    wrongp0=rand()%subjectc;
    wrongp1=rand()%subjectc;
    if ((wrongp0!=wrongp1)&&(wrongp0!=subjectp)&&(wrongp1!=subjectp)) break;
  }
  
  // Record where the correct option is going, then populate all three.
  switch (BATTLE->correctp=rand()%3) {
    case 0: {
        latin_populate_option(battle,0,subject->tileid_name);
        latin_populate_option(battle,1,subjectv[wrongp0].tileid_name);
        latin_populate_option(battle,2,subjectv[wrongp1].tileid_name);
      } break;
    case 1: {
        latin_populate_option(battle,0,subjectv[wrongp0].tileid_name);
        latin_populate_option(battle,1,subject->tileid_name);
        latin_populate_option(battle,2,subjectv[wrongp1].tileid_name);
      } break;
    case 2: {
        latin_populate_option(battle,0,subjectv[wrongp0].tileid_name);
        latin_populate_option(battle,1,subjectv[wrongp1].tileid_name);
        latin_populate_option(battle,2,subject->tileid_name);
      } break;
  }
  
  // Reset timer.
  BATTLE->timer=BATTLE->timerk;
}

/* Add vertices for picture, options, and teacher.
 */
 
static int latin_add_static_vertices(struct battle *battle) {
  const int piccolc=5;
  const int picrowc=3;
  const int optcolc=6;
  const int optrowc=1;
  const int optc=3;
  const int picw=piccolc*NS_sys_tilesize;
  const int pich=picrowc*NS_sys_tilesize;
  const int optw=optcolc*NS_sys_tilesize;
  const int opth=optrowc*NS_sys_tilesize;
  const int midspace=40; // Between pic and opts.
  const int optspace=0; // Between opts. Zero is fine, there's some baked-in spacing.
  const int teachervtxc=13; // 3x4, plus one for the pointer.
  
  int addvtxc=piccolc*picrowc+optcolc*optrowc*optc+teachervtxc;
  if (BATTLE->vtxc) return -1; // We must be the first thing done, vertex-wise.
  if (addvtxc>VTX_LIMIT) return -1;
  struct egg_render_tile *vtx=BATTLE->vtxv;
  BATTLE->vtxc=addvtxc;
  
  int fullh=pich+midspace+opth*optc+optspace*(optc-1); // Everything but the top and bottom margins.
  int fully=(FBH>>1)-(fullh>>1);
  
  // Picture.
  int picx0=(FBW>>1)-(picw>>1)+(NS_sys_tilesize>>1);
  int picy=fully+(NS_sys_tilesize>>1);
  int yi=picrowc;
  for (;yi-->0;picy+=NS_sys_tilesize) {
    int picx=picx0;
    int xi=piccolc;
    for (;xi-->0;picx+=NS_sys_tilesize,vtx++) {
      vtx->x=picx;
      vtx->y=picy;
      vtx->tileid=TILEID_BLANK;
      vtx->xform=0;
    }
  }
  
  // Options.
  int opty=picy+midspace;
  int optx0=(FBW>>1)-(optw>>1)+(NS_sys_tilesize>>1);
  for (yi=optc;yi-->0;opty+=NS_sys_tilesize+optspace) {
    int optx=optx0;
    int xi=optcolc;
    for (;xi-->0;optx+=NS_sys_tilesize,vtx++) {
      vtx->x=optx;
      vtx->y=opty;
      vtx->tileid=TILEID_BLANK;
      vtx->xform=0;
    }
  }
  
  // Teacher, the main 3x4 body.
  // Half a tile right of the picture, and half a tile offset from its top.
  int tx0=(FBW>>1)+(picw>>1)+NS_sys_tilesize; // Includes the half-tile to center us.
  int ty=fully+NS_sys_tilesize; // Includes the half-tile to center us.
  uint8_t tileid0=0x17;
  for (yi=4;yi-->0;ty+=NS_sys_tilesize,tileid0+=0x10) {
    int tx=tx0;
    int xi=3;
    uint8_t tileid=tileid0;
    for (;xi-->0;tx+=NS_sys_tilesize,tileid++,vtx++) {
      vtx->x=tx;
      vtx->y=ty;
      vtx->tileid=tileid;
      vtx->xform=0;
    }
  }
  
  // Teacher, pointer extends one tile left from her second row.
  vtx->x=tx0-NS_sys_tilesize;
  vtx->y=fully+NS_sys_tilesize*2;
  vtx->tileid=0x26;
  vtx->xform=0;
  vtx++;
  
  // Confirm we got all the iteration and counting right.
  if (vtx!=BATTLE->vtxv+addvtxc) {
    fprintf(stderr,"!!! %s !!! Expected %d vertices ie %p, but ended at %p\n",__func__,addvtxc,BATTLE->vtxv+addvtxc,vtx);
    return -1;
  }
  return 0;
}

/* Add vertices for a player.
 */

static int latin_add_player_vertices(struct battle *battle,struct player *player) {

  /* Ensure we've got space, and provision the vertex lists.
   */
  int reqc=4+1+player->star_limit+player->poop_limit;
  if (BATTLE->vtxc>VTX_LIMIT-reqc) {
    fprintf(stderr,"!!! %s !!! Insufficient vertex buffer.\n",__func__);
    return -1;
  }
  player->vportrait=BATTLE->vtxv+BATTLE->vtxc; BATTLE->vtxc+=4;
  player->vfinger=BATTLE->vtxv+BATTLE->vtxc; BATTLE->vtxc+=1;
  player->vstar=BATTLE->vtxv+BATTLE->vtxc; BATTLE->vtxc+=player->star_limit;
  player->vpoop=BATTLE->vtxv+BATTLE->vtxc; BATTLE->vtxc+=player->poop_limit;
  
  /* Portrait starts with the idle face, ie (tileid).
   */
  int lx,rx,vy,right;
  uint8_t xform;
  if (!player->who) { // Left.
    lx=NS_sys_tilesize*2+(NS_sys_tilesize>>1);
    rx=lx+NS_sys_tilesize;
    xform=0;
  } else { // Right.
    lx=FBW-NS_sys_tilesize*2-(NS_sys_tilesize>>1);
    rx=lx-NS_sys_tilesize;
    xform=EGG_XFORM_XREV;
  }
  vy=NS_sys_tilesize*9;
  player->vportrait[0]=(struct egg_render_tile){lx,vy+NS_sys_tilesize*0,player->tileid+0x00,xform};
  player->vportrait[1]=(struct egg_render_tile){rx,vy+NS_sys_tilesize*0,player->tileid+0x01,xform};
  player->vportrait[2]=(struct egg_render_tile){lx,vy+NS_sys_tilesize*1,player->tileid+0x10,xform};
  player->vportrait[3]=(struct egg_render_tile){rx,vy+NS_sys_tilesize*1,player->tileid+0x11,xform};
  
  /* Finger goes a little left or right of the middle* option, centered against it vertically.
   * Horizontal gap to taste.
   * [*] Or whatever (fingerp) defaults to.
   */
  struct egg_render_tile *opt=BATTLE->vtxv+15+player->fingerp*6;
  player->vfinger->y=opt->y;
  if (BATTLE->playerc==2) { // 2-player game, finger is only visible when buzzed in.
    player->vfinger->tileid=TILEID_BLANK;
  } else {
    player->vfinger->tileid=player->tileid+0x16;
  }
  if (!player->who) { // Left.
    player->vfinger->x=opt->x-NS_sys_tilesize-4;
    player->vfinger->xform=0;
  } else { // Right.
    opt+=5;
    player->vfinger->x=opt->x+NS_sys_tilesize+4;
    player->vfinger->xform=EGG_XFORM_XREV;
  }
  
  /* Poops and stars get centered horizontally against my portrait, and above it.
   * We don't need to check (who) for this, we can just average the first two portait positions.
   * Stars on top, poops on bottom, just like real life.
   * No horizontal spacing between indicators; it's baked in.
   */
  int midx=(player->vportrait[0].x+player->vportrait[1].x)>>1;
  int y=player->vportrait[0].y-NS_sys_tilesize-(NS_sys_tilesize>>1);
  int x=midx-((player->poop_limit*NS_sys_tilesize)>>1)+(NS_sys_tilesize>>1);
  int i=player->poop_limit;
  struct egg_render_tile *vtx=player->vpoop;
  for (;i-->0;vtx++,x+=NS_sys_tilesize) {
    vtx->x=x;
    vtx->y=y;
    vtx->tileid=0x50;
    vtx->xform=0;
  }
  y-=NS_sys_tilesize;
  x=midx-((player->star_limit*NS_sys_tilesize)>>1)+(NS_sys_tilesize>>1);
  for (i=player->star_limit,vtx=player->vstar;i-->0;vtx++,x+=NS_sys_tilesize) {
    vtx->x=x;
    vtx->y=y;
    vtx->tileid=0x40;
    vtx->xform=0;
  }
  
  return 0;
}

/* Initialize player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face,uint8_t difficulty) {
  player->who=(player==BATTLE->playerv)?0:1;
  player->human=human;
  switch (face) {
    case NS_face_princess: {
        player->tileid=0x20;
      } break;
    case NS_face_dot: default: {
        player->tileid=0x00;
      } break;
  }
  player->fingerp=1;
  player->pvinput=EGG_BTN_SOUTH|EGG_BTN_UP|EGG_BTN_DOWN;
  
  // (poop_limit) is POOP_LIMIT..0 inclusive, according to difficulty.
  player->poop_limit=(difficulty*(POOP_LIMIT+1))>>8;
  if (player->poop_limit<0) player->poop_limit=0;
  else if (player->poop_limit>POOP_LIMIT) player->poop_limit=POOP_LIMIT;
  player->poop_limit=POOP_LIMIT-player->poop_limit; // It's backward; more difficulty means fewer poops.
  
  // (star_limit) is 1..STAR_LIMIT inclusive, according to difficulty.
  player->star_limit=1+((difficulty*STAR_LIMIT)>>8);
  if (player->star_limit<1) player->star_limit=1;
  else if (player->star_limit>STAR_LIMIT) player->star_limit=STAR_LIMIT;
  
  // Prepare a plan for CPU players. Fixed odds, since difficulty is already accounted for by the thresholds.
  if (!human) {
    int correctc=(int)(PLAN_LIMIT*0.750);
    if (correctc>=PLAN_LIMIT) correctc=PLAN_LIMIT-1;
    while (correctc-->0) {
      int panic=100;
      while (panic-->0) {
        int p=rand()%PLAN_LIMIT;
        if (player->plan[p]) continue;
        player->plan[p]=1;
        break;
      }
    }
    player->next_guess=-1;
    player->cpuclock=0.500;
  }
}

/* New.
 */
 
static int _latin_init(struct battle *battle) {
  
  /* First determine our mode of operation, and who's playing.
   */
  if (battle->args.lctl&&battle->args.rctl) { // Two humans.
    BATTLE->mode=2;
    BATTLE->playerc=2;
    player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface,battle->args.bias);
    player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface,0xff-battle->args.bias);
    BATTLE->timerk=0.0; // No timer in multiplayer matches. (NB global timeout is disabled too, so we don't have a dead-man switch here).
  } else if (battle->args.lctl||battle->args.rctl) { // One human, doesn't really matter which. They go on the left regardless.
    BATTLE->mode=1;
    BATTLE->playerc=1;
    if (battle->args.lctl) {
      player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface,battle->args.bias);
      BATTLE->timerk=((TIMER_LO*battle->args.bias)+(TIMER_HI*(0xff-battle->args.bias)))/255.0;
    } else {
      player_init(battle,BATTLE->playerv+0,battle->args.rctl,battle->args.rface,0xff-battle->args.bias);
      BATTLE->timerk=((TIMER_HI*battle->args.bias)+(TIMER_LO*(0xff-battle->args.bias)))/255.0;
    }
  } else { // Two CPU players. One must be Dot or the Princess.
    BATTLE->mode=0;
    BATTLE->playerc=1;
    if (battle->args.lface==NS_face_princess) {
      player_init(battle,BATTLE->playerv+0,0,NS_face_princess,battle->args.bias);
      BATTLE->timerk=((TIMER_LO*battle->args.bias)+(TIMER_HI*(0xff-battle->args.bias)))/255.0;
    } else if (battle->args.rface==NS_face_princess) {
      player_init(battle,BATTLE->playerv+0,0,NS_face_princess,0xff-battle->args.bias);
      BATTLE->timerk=((TIMER_HI*battle->args.bias)+(TIMER_LO*(0xff-battle->args.bias)))/255.0;
    } else if (battle->args.lface==NS_face_dot) {
      player_init(battle,BATTLE->playerv+0,0,NS_face_dot,battle->args.bias);
      BATTLE->timerk=((TIMER_LO*battle->args.bias)+(TIMER_HI*(0xff-battle->args.bias)))/255.0;
    } else if (battle->args.rface==NS_face_dot) {
      player_init(battle,BATTLE->playerv+0,0,NS_face_dot,0xff-battle->args.bias);
      BATTLE->timerk=((TIMER_HI*battle->args.bias)+(TIMER_LO*(0xff-battle->args.bias)))/255.0;
    } else {
      fprintf(stderr,"%s: For zero-player mode, one side must have a Dot or Princess face.\n",__func__);
      return -1;
    }
  }
  
  /* Populate our vertex list.
   * The subject and options will initially be unset.
   */
  if (latin_add_static_vertices(battle)<0) return -1;
  if (latin_add_player_vertices(battle,BATTLE->playerv+0)<0) return -1;
  if (BATTLE->playerc==2) {
    if (latin_add_player_vertices(battle,BATTLE->playerv+1)<0) return -1;
  }
  
  /* Pick a subject to start.
   */
  latin_next_subject(battle);
  
  return 0;
}

/* Update a player's tiles due to score or mood change.
 */
 
static void player_update_face(struct battle *battle,struct player *player) {

  // If the game is over, use either Buzz or Lose.
  uint8_t tileid=player->tileid;
  if (battle->outcome>-2) {
    if (player->who) {
      if (battle->outcome==-1) tileid+=2;
      else tileid+=4;
    } else {
      if (battle->outcome==1) tileid+=2;
      else tileid+=4;
    }
  // Game not over, portrait is driven by two flags.
  } else if (player->sad) tileid+=4;
  else if (player->buzz) tileid+=2;
  player->vportrait[0].tileid=tileid+0x00;
  player->vportrait[1].tileid=tileid+0x01;
  player->vportrait[2].tileid=tileid+0x10;
  player->vportrait[3].tileid=tileid+0x11;
  
  // Poops and stars get one of two tiles depending on their count.
  struct egg_render_tile *vtx;
  int i;
  for (i=0,vtx=player->vstar;i<player->star_limit;i++,vtx++) {
    if (i<player->starc) vtx->tileid=0x41;
    else vtx->tileid=0x40;
  }
  for (i=0,vtx=player->vpoop;i<player->poop_limit;i++,vtx++) {
    if (i<player->poopc) vtx->tileid=0x51;
    else vtx->tileid=0x50;
  }
  
  // If 2-player, the hand is only visible when buzzed in.
  if (BATTLE->playerc==2) {
    if (player->buzz) player->vfinger->tileid=player->tileid+0x16;
    else player->vfinger->tileid=TILEID_BLANK;
  }
}

/* Activate selection.
 */
 
static void player_activate(struct battle *battle,struct player *player) {

  // In a 2-player game, you have to buzz in first.
  if (BATTLE->playerc==2) {
    if (player->buzz) {
      // Already buzzed in, great.
    } else {
      struct player *other=player->who?(BATTLE->playerv+0):(BATTLE->playerv+1);
      if (other->buzz) {
        // Other guy buzzed in. Wait your turn!
        bm_sound_pan(RID_sound_reject,player->who?0.250:-0.250);
        return;
      }
      // Buzz in.
      player->buzz=1;
      player->sad=0;
      player_update_face(battle,player);
      bm_sound_pan(RID_sound_uiactivate,player->who?0.250:-0.250);
      return;
    }
  }

  player->buzz=0;
  if (player->fingerp==BATTLE->correctp) {
    bm_sound_pan(RID_sound_treasure,player->who?0.250:-0.250);
    player->starc++;
    player->sad=0;
  } else {
    bm_sound_pan(RID_sound_reject,player->who?0.250:-0.250);
    player->poopc++;
    player->sad=1;
  }
  BATTLE->dirty=1;
  player_update_face(battle,player);
}

/* Move cursor.
 */
 
static void player_move(struct battle *battle,struct player *player,int d) {

  // In a 2-player game, you have to buzz in first. Moving implicitly buzzes in, if the other guy hasn't done yet.
  if (BATTLE->playerc==2) {
    if (player->buzz) {
      // Already buzzed in, great.
    } else {
      struct player *other=player->who?(BATTLE->playerv+0):(BATTLE->playerv+1);
      if (other->buzz) {
        // Other guy buzzed in. Wait your turn!
        bm_sound_pan(RID_sound_reject,player->who?0.250:-0.250);
        return;
      }
      // Buzz in.
      player->buzz=1;
      player->sad=0;
      player_update_face(battle,player);
    }
  }
  
  player->fingerp+=d;
  if (player->fingerp<0) player->fingerp=2;
  else if (player->fingerp>2) player->fingerp=0;
  player->vfinger->y=BATTLE->vtxv[15+player->fingerp*6].y;
  bm_sound_pan(RID_sound_uimotion,player->who?0.250:-0.250);
  if (player->sad) { // Drop the sadness from the last play, we're back to business now.
    player->sad=0;
    player_update_face(battle,player);
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed) {
  if (g.input[player->human]!=player->pvinput) {
    if ((g.input[player->human]&EGG_BTN_UP)&&!(player->pvinput&EGG_BTN_UP)) player_move(battle,player,-1);
    else if ((g.input[player->human]&EGG_BTN_DOWN)&&!(player->pvinput&EGG_BTN_DOWN)) player_move(battle,player,1);
    if ((g.input[player->human]&EGG_BTN_SOUTH)&&!(player->pvinput&EGG_BTN_SOUTH)) player_activate(battle,player);
    player->pvinput=g.input[player->human];
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->cpuclock-=elapsed)>0.0) return;
  player->cpuclock+=0.500;
  if (player->next_guess<0) {
    int correct=player->plan[player->planp++];
    if (player->planp>=PLAN_LIMIT) player->planp=0;
    if (correct) {
      player->next_guess=BATTLE->correctp;
    } else {
      int p=rand()&1;
      if (p>=BATTLE->correctp) p++;
      player->next_guess=p;
    }
    // Skip one cycle at the start of each round, otherwise it looks unnatural.
    return;
  }
  if (player->fingerp<player->next_guess) {
    player_move(battle,player,1);
  } else if (player->fingerp>player->next_guess) {
    player_move(battle,player,-1);
  } else {
    player->next_guess=-1;
    player_activate(battle,player);
  }
}

/* Status of player: -1,0,1
 */
 
static int player_get_status(const struct player *player) {
  /* Sic '>' for poop and '>=' for star.
   * Being maximally charitable to the humans playing, they win when all the stars are got, but they lose only when the poops overflow.
   */
  if (player->poopc>player->poop_limit) return -1;
  if (player->starc>=player->star_limit) return 1;
  return 0;
}

/* Update.
 */
 
static void _latin_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  BATTLE->dirty=0;
  
  /* If we have a timer and it expires, poop on the player.
   * There can only be one player when timers are in play.
   */
  if (BATTLE->timer>0.0) {
    if ((BATTLE->timer-=elapsed)<=0.0) {
      bm_sound_pan(RID_sound_reject,-0.250);
      BATTLE->playerv[0].poopc++;
      BATTLE->playerv[0].sad=1;
      BATTLE->dirty=1;
      player_update_face(battle,BATTLE->playerv+0);
    }
  }
  
  if (!BATTLE->dirty) { // Skip the player updates if we just timed out.
    struct player *player=BATTLE->playerv;
    int i=BATTLE->playerc;
    for (;i-->0;player++) {
      if (player->human) player_update_man(battle,player,elapsed);
      else player_update_cpu(battle,player,elapsed);
    }
  }
  
  if (BATTLE->dirty) {
    int l=0,r=0; // Status of each player (-1,0,1)=(lose,incomplete,win). In a one-player game, (r) is always "incomplete".
    l=player_get_status(BATTLE->playerv+0);
    if (BATTLE->playerc>=2) r=player_get_status(BATTLE->playerv+1);
    // If they're the same nonzero value, it's a tie.
    if (l&&(l==r)) battle->outcome=0;
    // Did somebody win or lose?
    else if (l<0) battle->outcome=-1;
    else if (r<0) battle->outcome=1;
    else if (l>0) battle->outcome=1;
    else if (r>0) battle->outcome=-1;
    // If we didn't set outcome, start the next subject.
    if (battle->outcome==-2) {
      latin_next_subject(battle);
    // Or if we did set it, update faces one more time.
    } else {
      player_update_face(battle,BATTLE->playerv+0);
      if (BATTLE->playerc==2) player_update_face(battle,BATTLE->playerv+1);
    }
  }
}

/* Render.
 */
 
static void _latin_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_latin);
  graf_tile_batch(&g.graf,BATTLE->vtxv,BATTLE->vtxc);
  if (BATTLE->timerk>0.0) batsup_render_hourglass(FBW>>1,FBH>>1,BATTLE->timer,BATTLE->timerk);
}

/* Type definition.
 */
 
const struct battle_type battle_type_latin={
  .name="latin",
  .objlen=sizeof(struct battle_latin),
  .strix_name=156,
  .no_article=0,
  .no_contest=0,
  .no_timeout=1,
  .support_pvp=1,
  .support_cvc=1,
  .del=_latin_del,
  .init=_latin_init,
  .update=_latin_update,
  .render=_latin_render,
};

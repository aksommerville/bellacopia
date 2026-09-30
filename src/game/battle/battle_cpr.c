/* battle_cpr.c
 * Rhythm game with one button.
 */

#include "game/batsup/battle_internal.h"

/* With these parameters, you can revive the patient in a bar or two less than the song's length.
 */
#define THUMP_TIME     0.200 /* sec. How long do we stay compressed, visual indication and also input blackout. */
#define INCREASE_BEST  0.170
#define INCREASE_WORST 0.110
#define DECREASE_BEST  0.040
#define DECREASE_WORST 0.050
#define CPU_PENALTY    0.700 /* Multiplied to (increase) when there's one CPU player. */

#define METRONOME_W 40 /* Must agree with graphics. */
#define METRONOME_SPEED 0.060 /* Sec/pixel, and may run under 1 frame. */
#define THUMP_LIMIT 6 /* Circular buffer, and its length determines how much we compare against for scoring. */

struct battle_cpr {
  struct battle hdr;
  int choice;
  double secperbeat; // Constant, computed from song at init.
  double playhead; // Sampled at the start of each update.
  double beat; // -1..0..1 = early..perfect..late
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t face;
    double thump;
    double goodrange; // ms, how close must we be to score at all. <secperbeat/2.
    double increase; // 0..1, earn so much for a good thump.
    double decrease; // 0..1, lose so much continuously per beat.
    double score; // 0..1, when it reaches 1 you win.
    struct metronome {
      double displacement; // -1..1
      int beat; // Nonzero if a beat goes here.
    } metronome[METRONOME_W];
    int metronomep;
    double metroclock;
    double metrobeat;
    int mprint;
    double thumpv[THUMP_LIMIT];
    int thumpc,thumpp;
    // Man:
    int pvinput;
    // CPU:
    double errlo,errhi;
    double cpuclock;
  } playerv[2];
};

#define BATTLE ((struct battle_cpr*)battle)

/* Delete.
 */
 
static void _cpr_del(struct battle *battle) {
  battle_unsong();
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  
  player->metronomep=rand()%METRONOME_W; // It looks weird if they're in phase with each other.
  player->increase=INCREASE_WORST*(1.0-player->skill)+INCREASE_BEST*player->skill;
  player->decrease=DECREASE_WORST*(1.0-player->skill)+DECREASE_BEST*player->skill;
  player->goodrange=BATTLE->secperbeat/2.0;
  player->goodrange=player->skill*player->goodrange+(1.0-player->skill)*0.080;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    // CPU increase penalty is applied by _cpr_init.
    player->goodrange*=0.800;
    player->errhi=0.040*player->skill+0.060*(1.0-player->skill);
    player->errlo=player->errhi*0.5;
    player->cpuclock=BATTLE->secperbeat*2.0; // Give the humans a wee head start.
  }
  switch (player->face=face) {
    case NS_face_monster: {
        player->color=0x1cb77cff;
        player->tileid=0x8a;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x86;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x69;
      } break;
  }
}

/* New.
 */
 
static int _cpr_init(struct battle *battle) {
  
  // Read tempo off the song resource.
  const uint8_t *src=0;
  int srcc=res_get(&src,EGG_TID_song,RID_song_heart_thumpin);
  if ((srcc<6)||memcmp(src,"\0EAU",4)) {
    fprintf(stderr,"song:%d(heart_thumpin) not valid EAU\n",RID_song_heart_thumpin);
    return -1;
  }
  int msperqnote=(src[4]<<8)|src[5];
  if (msperqnote<1) {
    fprintf(stderr,"song:%d invalid tempo %d\n",RID_song_heart_thumpin,msperqnote);
    return -1;
  }
  // The song's tempo is rated for beats twice as fast as we want, hence *2. It works out to 688 ms.
  BATTLE->secperbeat=(double)(msperqnote*2.0)/1000.0;

  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  /* If exactly one player is CPU, give it a stiff penalty against increase.
   * CPU players are perfect, they hit every beat.
   * So it's (increase) and (decrease) that determine whether they will win.
   */
  if (battle->args.lctl&&!battle->args.rctl) {
    BATTLE->playerv[1].increase*=CPU_PENALTY;
  } else if (!battle->args.lctl&&battle->args.rctl) {
    BATTLE->playerv[0].increase*=CPU_PENALTY;
  }
  
  battle_song(RID_song_heart_thumpin);
  
  return 0;
}

/* Impulse, for either player controller.
 */
 
static void player_thump(struct battle *battle,struct player *player) {
  if (battle->outcome>-2) return;
  if (player->thump>0.0) return;
  player->thump=THUMP_TIME;
  player->mprint=0;
  
  /* Record the thump time.
   */
  player->thumpv[player->thumpp++]=BATTLE->playhead;
  if (player->thumpp>=THUMP_LIMIT) player->thumpp=0;
  if (player->thumpc<THUMP_LIMIT) player->thumpc++;
  
  /* Walk backward thru all recorded thumps.
   * Expect each to be (secperbeat) earlier, on a scale anchored by the new beat.
   * If we walk off the start, the song has looped, reset that scale.
   * Experimentally, my RMS is usually in 10..20 ms and never hits 100 ms unless I'm trying to fail. Beats are 688 ms.
   */
  int i=player->thumpc;
  int checkp=player->thumpp;
  double expecttime=-1.0; // <0 to reset, call the next thump perfect
  double offness2=0.0; // Sum of squared deltas.
  while (i-->0) {
    checkp--;
    if (checkp<0) checkp=THUMP_LIMIT-1;
    expecttime-=BATTLE->secperbeat;
    if (expecttime<0.0) expecttime=player->thumpv[checkp];
    double d=player->thumpv[checkp]-expecttime;
    offness2+=d*d;
  }
  double offness=sqrt(offness2/player->thumpc);
  
  /* Compare RMS of thump times to our goodrange.
   */
  double quality=1.0-offness/player->goodrange;
  if (quality<=0.0) return;
  player->score+=player->increase*quality;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (input!=player->pvinput) {
    if ((input&EGG_BTN_DOWN)&&!(player->pvinput&EGG_BTN_DOWN)) {
      player_thump(battle,player);
    }
    player->pvinput=input;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->cpuclock-=elapsed)<=0.0) {
    double err=(rand()&0xffff)/65535.0;
    err=err*player->errlo+(1.0-err)*player->errhi;
    if (rand()&1) err=-err;
    player->cpuclock+=BATTLE->secperbeat+err;
    player_thump(battle,player);
  }
}

/* -1..1 metronome displacement for 0..THUMP_TIME.
 */
 
static double displacement_per_thump(struct battle *battle,struct player *player,double thump) {
  if (thump<=0.0) {
    player->mprint=0;
    return 0.0;
  }
  switch (player->mprint++) {
    case 0: return  1.000; break;
    case 1: return -0.400; break;
    case 2: return  0.200; break;
    case 3: return -0.100; break;
    case 4: return  0.050; break;
    default: return 0.0;
  }
}

static double displacement_per_beat(struct battle *battle,struct player *player,double beat) {
  if (beat<=0.0) return 0.0;
  return displacement_per_thump(battle,player,0.001);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Stop thumping after expiration.
  if ((player->thump-=elapsed)<=0.0) player->thump=0.0;
  
  // Advance metronome.
  player->metroclock-=elapsed;
  while (player->metroclock<0.0) {
    player->metroclock+=METRONOME_SPEED;
    player->metronomep++;
    if (player->metronomep>=METRONOME_W) player->metronomep=0;
    struct metronome *m=player->metronome+player->metronomep;
    if ((player->metrobeat<0.0)&&(BATTLE->beat>=0.0)) {
      m->beat=1;
      if (player->score>=1.0) player->mprint=0;
    } else m->beat=0;
    if (player->score>=1.0) m->displacement=displacement_per_beat(battle,player,BATTLE->beat);
    else m->displacement=displacement_per_thump(battle,player,player->thump);
    player->metrobeat=BATTLE->beat;
  }
  
  if (battle->outcome>-2) return; // Don't decrease score after game over, even (especially!) unfinished ones.
  if (player->score>=1.0) {
    // Patient is revived, stop touching the score.
  } else {
    double elbeats=elapsed/BATTLE->secperbeat;
    if ((player->score-=elbeats*player->decrease)<=0.0) player->score=0.0;
  }
}

/* Update.
 */
 
static void _cpr_update(struct battle *battle,double elapsed) {

  /* Capture and normalize playhead.
   */
  BATTLE->playhead=egg_song_get_playhead(1);
  double phbeats=BATTLE->playhead/BATTLE->secperbeat;
  double dummy;
  double partial=modf(phbeats,&dummy);
  if (partial>=0.5) partial-=1.0;
  BATTLE->beat=partial*2.0;

  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (battle->outcome==-2) {
      if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
    player_update_common(battle,player,elapsed);
  }
  
  /* Did anybody win?
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->score>=1.0) {
    if (r->score>=1.0) battle->outcome=0;
    else battle->outcome=1;
  } else if (r->score>=1.0) battle->outcome=-1;
}

/* Render player.
 * "tiles" happens first, with image:battle_fractia armed.
 * Stuff that uses other images or raw primitives goes in "over".
 */
 
#define EDGE_MARGIN 100
#define PLAYERY 100
 
static void player_render_tiles(struct battle *battle,struct player *player) {
  int x=player->who?(FBW-EDGE_MARGIN):EDGE_MARGIN;
  int y=PLAYERY; // Center of hero.
  const int ht=NS_sys_tilesize>>1;
  
  // The hero.
  uint8_t tileid=player->tileid;
  if (player->thump>0.0) tileid+=2;
  graf_tile(g_graf,x-ht,y-ht,tileid+0x00,0);
  graf_tile(g_graf,x+ht,y-ht,tileid+0x01,0);
  graf_tile(g_graf,x-ht,y+ht,tileid+0x10,0);
  graf_tile(g_graf,x+ht,y+ht,tileid+0x11,0);
  
  // The patient.
  tileid=(player->score>=1.0)?0x94:(player->thump>0.0)?0x92:0x90;
  graf_tile(g_graf,x-ht-3,y+ht,tileid,0);
  graf_tile(g_graf,x+ht-3,y+ht,tileid+1,0);
  
  // 3x3 metronome above the hero. Just its static background.
  int my=y-NS_sys_tilesize*3;
  graf_tile(g_graf,x-NS_sys_tilesize,my-NS_sys_tilesize,0x5d,0);
  graf_tile(g_graf,x                ,my-NS_sys_tilesize,0x5e,0);
  graf_tile(g_graf,x+NS_sys_tilesize,my-NS_sys_tilesize,0x5f,0);
  graf_tile(g_graf,x-NS_sys_tilesize,my                ,0x6d,0);
  graf_tile(g_graf,x                ,my                ,0x6e,0);
  graf_tile(g_graf,x+NS_sys_tilesize,my                ,0x6f,0);
  graf_tile(g_graf,x-NS_sys_tilesize,my+NS_sys_tilesize,0x7d,0);
  graf_tile(g_graf,x                ,my+NS_sys_tilesize,0x7e,0);
  graf_tile(g_graf,x+NS_sys_tilesize,my+NS_sys_tilesize,0x7f,0);
  
  // 1x3 corostat inward of the metronome. Just its static background.
  int cx=x;
  if (player->who) cx-=ht*5;
  else cx+=ht*5;
  graf_tile(g_graf,cx,my-NS_sys_tilesize,0x8f,0);
  graf_tile(g_graf,cx,my                ,0x9f,0);
  graf_tile(g_graf,cx,my+NS_sys_tilesize,0xaf,0);
  
  // Corostat indicators.
  int notec=0,heart=0;
  uint8_t top_note_alpha=0;
  if (player->score>=1.0) {
    heart=1;
    notec=3;
    top_note_alpha=0xff;
  } else {
    int n=(int)(player->score*768);
    if (n>0) {
      notec=(n>>8)+1;
      if (notec<0) notec=0; else if (notec>3) notec=3;
      top_note_alpha=n&0xff;
    }
  }
  int dstx=cx-1;
  int dsty=my-NS_sys_tilesize-1;
  if (heart) graf_tile(g_graf,dstx,dsty,0x8e,0); dsty+=10;
  int i=3; for (;i-->0;dsty+=10) {
    if (i<notec) {
      if (i==notec-1) graf_set_alpha(g_graf,top_note_alpha);
      graf_tile(g_graf,dstx,dsty,0x9e,0);
      graf_set_alpha(g_graf,0xff);
    }
  }
}
 
static void player_render_over(struct battle *battle,struct player *player) {
  int x=player->who?(FBW-EDGE_MARGIN):EDGE_MARGIN;
  int my=PLAYERY-NS_sys_tilesize*3;
  int sx=x-20;
  int sy=my-20;
  int sw=40;
  int sh=38;
  
  int notexv[8];
  int notexc=0;
  
  graf_set_input(g_graf,0);
  int dstx=sx;
  int i=0,stripc=0;
  for (;i<METRONOME_W;i++,dstx++) {
    if (i==player->metronomep) {
      graf_line(g_graf,dstx,sy,0xffffffff,dstx,sy+sh,0xffffffff);
      stripc=0;
    } else {
      if ((notexc<8)&&player->metronome[i].beat) notexv[notexc++]=dstx;
      int dsty=sy+28-(int)(player->metronome[i].displacement*9.0);
      if (!stripc) {
        graf_line_strip_begin(g_graf,dstx,dsty,0xffff00ff);
      }
      graf_line_strip_more(g_graf,dstx,dsty,0xffff00ff);
      stripc++;
    }
  }
  graf_set_image(g_graf,RID_image_battle_fractia);
  while (notexc-->0) {
    graf_tile(g_graf,notexv[notexc],sy+10,0x5c,0);
  }
  
  // There's a separate set of tiles for the metronome's foreground, to clip out stray notes.
  graf_tile(g_graf,x-NS_sys_tilesize,my-NS_sys_tilesize,0x2d,0);
  graf_tile(g_graf,x                ,my-NS_sys_tilesize,0x2e,0);
  graf_tile(g_graf,x+NS_sys_tilesize,my-NS_sys_tilesize,0x2f,0);
  graf_tile(g_graf,x-NS_sys_tilesize,my                ,0x3d,0);
  graf_tile(g_graf,x                ,my                ,0x3e,0);
  graf_tile(g_graf,x+NS_sys_tilesize,my                ,0x3f,0);
  graf_tile(g_graf,x-NS_sys_tilesize,my+NS_sys_tilesize,0x4d,0);
  graf_tile(g_graf,x                ,my+NS_sys_tilesize,0x4e,0);
  graf_tile(g_graf,x+NS_sys_tilesize,my+NS_sys_tilesize,0x4f,0);
  
  /* If I'm a nurse and the battle is over, render the PSA.
   * I want to bill Bellacopia generally as "you can play lots of these minigames in real life!". (eg Throwing Contest, Home Run Derby...)
   * But that is emphatically not the case for CPR, and it might not be obvious to a child.
   */
  if ((battle->outcome>-2)&&(player->face==NS_face_monster)) {
    int srcx=NS_sys_tilesize*11;
    int srcy=0;
    int w=NS_sys_tilesize*3;
    int h=NS_sys_tilesize*2;
    graf_decal(g_graf,x-(w>>1),PLAYERY-h-13,srcx,srcy,w,h);
  }
}

/* Render.
 */
 
static void _cpr_render(struct battle *battle) {

  const int groundy=PLAYERY+NS_sys_tilesize-1;
  graf_fill_rect(g_graf,0,0,FBW,FBH,0x8c979bff);
  graf_fill_rect(g_graf,0,groundy,FBW,FBH-groundy,0x789a7bff);
  graf_fill_rect(g_graf,0,groundy,FBW,1,0x000000ff);

  graf_set_image(g_graf,RID_image_battle_fractia);
  player_render_tiles(battle,BATTLE->playerv+0);
  player_render_tiles(battle,BATTLE->playerv+1);
  player_render_over(battle,BATTLE->playerv+0);
  player_render_over(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
static const struct battle_input cpr_input[]={
  {1,EGG_BTN_DOWN},
  {3,0},
  {0},
};
 
const struct battle_type battle_type_cpr={
  .name="cpr",
  .objlen=sizeof(struct battle_cpr),
  .id=37,
  .strix_name=163,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=cpr_input,
  .del=_cpr_del,
  .init=_cpr_init,
  .update=_cpr_update,
  .render=_cpr_render,
};

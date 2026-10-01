/* battle_chanting.c
 */

#include "game/batsup/battle_internal.h"
#include "game/batsup/batsup_visbits.h"

#define WORD_LIMIT 16
#define MELT_SPEED_LIMIT 0.200 /* candle/sec */

struct battle_chanting {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t homenote; // MIDI note. Girls are higher, etc.
    double candle; // 0..1, counts down.
    double canimclock;
    double meltlo,melthi; // How much candle to melt per word.
    double meltrange; // s, how close for the lo melt score. <0.5
    double melt_pending; // >=0, will remove so much from (candle) over time.
    int canimframe;
    int mouth;
    int blackout;
    int noteid;
    double cpuclock;
    double cpuerrlo,cpuerrhi; // s
  } playerv[2];
  
  double ballrate; // word/sec
  
  int strix;
  const char *text;
  int textc;
  int textw;
  struct word {
    int ap,zp; // Start and end positions in (text), (zp) exclusive.
    int ballx; // Horizontal framebuffer position of center.
  } wordv[WORD_LIMIT];
  int wordc;
  int wordc_past;
  double ballp; // 0..1, at 1 it wraps and the next word gets focussed.
};

#define BATTLE ((struct battle_chanting*)battle)

/* Delete.
 */
 
static void _chanting_del(struct battle *battle) {
  battle_unsong();
}

/* Load a psalm, take some measurements, reset the karaoke state.
 * We won't load the same string twice in a row, but might repeat within a session.
 */
 
static void chanting_next_psalm(struct battle *battle) {

  // Random string, but not the one currently loaded.
  int strix;
  for (;;) {
    strix=341+rand()%8;
    if (strix!=BATTLE->strix) break;
  }
  BATTLE->strix=strix;
  BATTLE->textc=text_get_string(&BATTLE->text,RID_strings_battle,strix);
  while (BATTLE->textc&&((unsigned char)BATTLE->text[BATTLE->textc-1]<=0x20)) BATTLE->textc--;
  while (BATTLE->textc&&((unsigned char)BATTLE->text[0]<=0x20)) { BATTLE->text++; BATTLE->textc--; }
  BATTLE->textw=monkish_render(0,0,BATTLE->text,BATTLE->textc,1);
  
  // Split on space.
  int lx=(FBW>>1)-(BATTLE->textw>>1);
  BATTLE->wordc=0;
  int textp=0;
  while (textp<BATTLE->textc) {
    if ((unsigned char)BATTLE->text[textp]<=0x20) { textp++; continue; }
    if (BATTLE->wordc>=WORD_LIMIT) {
      fprintf(stderr,"%s: strix %d, too many words\n",__func__,BATTLE->strix);
      break;
    }
    struct word *word=BATTLE->wordv+BATTLE->wordc++;
    word->ap=textp;
    while ((textp<BATTLE->textc)&&((unsigned char)BATTLE->text[textp]>0x20)) textp++;
    word->zp=textp;
    int ax=lx+monkish_render(0,0,BATTLE->text,word->ap,1);
    int zx=ax+monkish_render(0,0,BATTLE->text+word->ap,word->zp-word->ap,1);
    word->ballx=(ax+zx)>>1;
  }
  
  // Karaoke ball all the way left.
  BATTLE->wordc_past=0;
  BATTLE->ballp=0.0;
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  
  player->candle=1.0;
  player->meltrange=(1.0-player->skill)*0.150+player->skill*0.250;
  player->melthi=(1.0-player->skill)*0.070+player->skill*0.100;
  player->meltlo=(1.0-player->skill)*0.010+player->skill*0.020;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->melthi*=0.900;
    player->meltlo*=0.800;
    player->cpuclock=1.0/BATTLE->ballrate;
    player->cpuerrhi=(1.0-player->skill)*0.170+player->skill*0.080;
    player->cpuerrlo=(1.0-player->skill)*0.090+player->skill*0.030;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x46260fff;
        player->tileid=0x90;
        player->homenote=0x38;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x30;
        player->homenote=0x40;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x60;
        player->homenote=0x48;
      } break;
  }
}

/* New.
 */
 
static int _chanting_init(struct battle *battle) {
  battle_song(RID_song_chanting);
  BATTLE->ballrate=2.000;
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  chanting_next_psalm(battle);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->mouth=(input&EGG_BTN_SOUTH);
  }
  //TODO
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->cpuclock-=elapsed)<=0.0) {
    if (player->mouth) {
      player->mouth=0;
      double waittime=(1.0-BATTLE->ballp)/BATTLE->ballrate;
      double err=(rand()&0xffff)/65535.0;
      err=(1.0-err)*player->cpuerrlo+err*player->cpuerrhi;
      player->cpuclock+=waittime+err;
    } else {
      if (
        ((BATTLE->wordc_past==0)&&(BATTLE->ballp<0.5))||
        ((BATTLE->wordc_past>=BATTLE->wordc)&&(BATTLE->ballp>0.5))
      ) { // Don't sing at the fake transition word.
        double waittime=(1.0-BATTLE->ballp)/BATTLE->ballrate;
        double err=(rand()&0xffff)/65535.0;
        err=(1.0-err)*player->cpuerrlo+err*player->cpuerrhi;
        player->cpuclock+=waittime+err;
      } else {
        player->mouth=1;
        player->cpuclock+=0.150+((rand()&0xffff)*0.100)/65535.0; // sing time
      }
    }
  }
}

/* Note started. Adjust score accordingly.
 */
 
static void chanting_score_note(struct battle *battle,struct player *player) {
  double d=BATTLE->ballp;
  if (d>0.5) d=1.0-d;
  if (d<0.0) d=-d;
  if (d>player->meltrange) return;
  d/=player->meltrange;
  double melt=(1.0-d)*player->melthi+d*player->meltlo;
  player->melt_pending+=melt;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Animate flame.
  if ((player->canimclock-=elapsed)<=0.0) {
    player->canimclock+=0.150;
    if (++(player->canimframe)>=4) player->canimframe=0;
  }
  
  if (battle->outcome>-2) {
    if (player->noteid) {
      egg_song_event_note_off(1,player->who,player->noteid);
      player->noteid=0;
    }
    player->mouth=0;
    return;
  }
  
  /* If my mouth is open, ensure a note is playing, and assess timing.
   * And if not, not.
   */
  if (player->mouth) {
    if (!player->noteid) {
      chanting_score_note(battle,player);
      player->noteid=player->homenote;
      if (!(rand()%3)) player->noteid-=5;
      player->noteid-=player->who; // Shouldn't be two of the same faces, but if so offset one's notes.
      uint8_t velocity=0x01+rand()%126;
      egg_song_event_note_on(1,player->who,player->noteid,velocity);
    }
  } else {
    if (player->noteid) {
      egg_song_event_note_off(1,player->who,player->noteid);
      player->noteid=0;
    }
  }
  
  /* Melt the candle.
   */
  if (player->melt_pending>0.0) {
    double limit=MELT_SPEED_LIMIT*elapsed;
    double rm=player->melt_pending;
    if (rm>limit) rm=limit;
    player->melt_pending-=rm;
    if ((player->candle-=rm)<0.0) player->candle=0.0;
  }
}

/* Update.
 */
 
static void _chanting_update(struct battle *battle,double elapsed) {
  
  BATTLE->ballp+=BATTLE->ballrate*elapsed;
  if (BATTLE->ballp>=1.0) {
    if (BATTLE->wordc_past>=BATTLE->wordc) {
      if (battle->outcome>-2) {
        BATTLE->wordc=0;
        BATTLE->wordc_past=0;
        BATTLE->text="";
        BATTLE->textc=0;
        BATTLE->textw=0;
      } else {
        chanting_next_psalm(battle);
      }
    } else {
      BATTLE->ballp-=1.0;
      BATTLE->wordc_past++;
    }
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Game is over when a candle burns out. Ties are possible.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    int ldone=(l->candle<=0.0);
    int rdone=(r->candle<=0.0);
    if (ldone&&rdone) battle->outcome=0;
    else if (ldone) battle->outcome=1;
    else if (rdone) battle->outcome=-1;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  /* Hero.
   */
  uint8_t herotileid=player->tileid;
  if (player->mouth) herotileid+=2;
  uint8_t heroxform;
  int backx,frontx;
  if (player->who) {
    backx=FBW-90;
    frontx=backx-NS_sys_tilesize;
    heroxform=EGG_XFORM_XREV;
  } else {
    backx=90;
    frontx=backx+NS_sys_tilesize;
    heroxform=0;
  }
  int topy=80;
  int midy=topy+NS_sys_tilesize;
  int lowy=midy+NS_sys_tilesize;
  if (player->candle<=0.0) graf_set_tint(g_graf,0x00000080);
  graf_tile(g_graf,backx ,topy,herotileid+0x00,heroxform);
  graf_tile(g_graf,frontx,topy,herotileid+0x01,heroxform);
  graf_tile(g_graf,backx ,midy,herotileid+0x10,heroxform);
  graf_tile(g_graf,frontx,midy,herotileid+0x11,heroxform);
  graf_tile(g_graf,backx ,lowy,herotileid+0x20,heroxform);
  graf_tile(g_graf,frontx,lowy,herotileid+0x21,heroxform);

  /* Candle.
   */
  int candlex=140;
  if (player->who) candlex=FBW-candlex;
  int candleh=(int)(player->candle*80.0); // How much shaft. Zero is ok.
  int candley=120;
  graf_tile(g_graf,candlex,candley,0x22,0);
  candley-=NS_sys_tilesize>>1;
  while (candleh>=NS_sys_tilesize) {
    graf_tile(g_graf,candlex,candley-(NS_sys_tilesize>>1),0x21,0);
    candley-=NS_sys_tilesize;
    candleh-=NS_sys_tilesize;
  }
  if (candleh>0) {
    candley-=candleh;
    int srcx=NS_sys_tilesize;
    int srcy=NS_sys_tilesize*3-candleh;
    graf_decal(g_graf,candlex-(NS_sys_tilesize>>1),candley,srcx,srcy,NS_sys_tilesize,candleh);
  }
  uint8_t captileid=0x11;
  uint8_t flametileid=0x01;
  uint8_t flamexform=0;
  if (player->candle>0.0) {
    switch (player->canimframe) {
      case 0: break;
      case 1: flametileid+=1; break;
      case 2: captileid+=1; flamexform=EGG_XFORM_XREV; break;
      case 3: captileid+=1; flametileid+=1; flamexform=EGG_XFORM_XREV; break;
    }
  }
  graf_tile(g_graf,candlex,candley,captileid,0);
  if (player->candle>0.0) graf_tile(g_graf,candlex,candley-10,flametileid,flamexform);
  graf_set_tint(g_graf,0);
}

/* Render.
 */
 
static void _chanting_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,0x000000ff);
  
  graf_set_image(g_graf,RID_image_battle_chanting);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  if (!BATTLE->wordc) return;
  
  /* Lyrics render in up to two passes.
   * Different color for those already sung.
   */
  int x=(FBW>>1)-(BATTLE->textw>>1);
  int y=FBH-16;
  int splitp;
  if (BATTLE->wordc_past>=BATTLE->wordc) {
    splitp=BATTLE->textc;
  } else if (BATTLE->wordc_past>0) {
    splitp=BATTLE->wordv[BATTLE->wordc_past-1].zp;
  } else {
    splitp=0;
  }
  if (splitp>0) {
    graf_set_tint(g_graf,0xe0a060ff);
    x+=monkish_render(x,y,BATTLE->text,splitp,0);
  }
  if (splitp<BATTLE->textc) {
    graf_set_tint(g_graf,0xc07030ff);
    x+=monkish_render(x,y,BATTLE->text+splitp,BATTLE->textc-splitp,0);
  }
  graf_set_tint(g_graf,0);
  
  /* Karaoke ball bouncing from word to word.
   */
  graf_set_image(g_graf,RID_image_battle_chanting);
  double ax,zx;
  if (BATTLE->wordc_past<=0) ax=-BATTLE->wordv[0].ballx;
  else ax=BATTLE->wordv[BATTLE->wordc_past-1].ballx;
  if (BATTLE->wordc_past>=BATTLE->wordc) zx=FBW+(FBW-BATTLE->wordv[BATTLE->wordc-1].ballx);
  else zx=BATTLE->wordv[BATTLE->wordc_past].ballx;
  double balevation=(BATTLE->ballp-0.5)*2.0;
  balevation=1.0-balevation*balevation;
  int ballx=(int)(ax*(1.0-BATTLE->ballp)+zx*BATTLE->ballp);
  int bally=y-17-(int)(balevation*30.0);
  graf_fancy(g_graf,ballx,bally,0x00,0,0,NS_sys_tilesize,0,0xffffffff);
}

/* Type definition.
 */
 
const struct battle_type battle_type_chanting={
  .name="chanting",
  .objlen=sizeof(struct battle_chanting),
  .id=105,
  .strix_name=340,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .imageid_default=0,
  .del=_chanting_del,
  .init=_chanting_init,
  .update=_chanting_update,
  .render=_chanting_render,
};

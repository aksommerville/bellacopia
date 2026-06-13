/* battle_sumohorse.c
 * Metronome. One player dances with only Left and Right, then the other player must repeat it.
 */

#include "game/bellacopia.h"

#define STAGE_RDANCE  1
#define STAGE_LREPEAT 2
#define STAGE_LDANCE  3
#define STAGE_RREPEAT 4
#define STAGE_REPORT  5

#define STAGE_LEAD_TIME 1.000
#define METRONOME_PERIOD 0.750
#define STEP_LIMIT 6

struct step {
  int lean; // -1,0,1
  int visited; // For CPU players only, so we can distinguish intentional (lean==0).
  double offset; // -1..1, how close to the beat.
};

struct battle_sumohorse {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    int srcy;
    uint8_t xform;
    int dstx; // Center of main 48x48 decal.
    int lean; // -1,0,1 = left,neutral,right. Absolute direction regardless of xform. Controller sets.
    int pvlean; // For generic controller only.
    double leanhold; // Nonzero lean has a minimum uptime.
    int captiontexid;
    int captionw,captionh;
    int active; // Nonzero if I can dance in this stage.
    int freshsteps; // Nonzero if I'm active and overwriting (stepv). ie a "DANCE" stage.
    int plan[STEP_LIMIT]; // For CPU players, decide in advance which repeat steps will be correct (1) and which incorrect (0).
    double failage,failagelo,failagehi; // Deliberate delay after a beat, to make us imperfect.
  } playerv[2];
  
  int stage;
  double stageclock; // Counts up.
  double metroclock; // Counts down.
  int readyset; // Counts down beats after the metronome starts, before we start encoding.
  // Record all four dances so we can analyze at our leisure after the festivities.
  struct step rdance[STEP_LIMIT];
  struct step lrepeat[STEP_LIMIT];
  struct step ldance[STEP_LIMIT];
  struct step rrepeat[STEP_LIMIT];
  int stepc;
  int reportc;
};

#define BATTLE ((struct battle_sumohorse*)battle)

/* Delete.
 */
 
static void _sumohorse_del(struct battle *battle) {
  if (modal_get_topmost(&modal_type_pvp)) bm_song_gently(RID_song_death_rattle);
  else bm_song_gently(bm_song_for_outerworld());
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    egg_texture_del(player->captiontexid);
  }
}

/* Set caption.
 */
 
static void sumohorse_set_caption(struct battle *battle,struct player *player,int strix) {
  egg_texture_del(player->captiontexid);
  if (strix) {
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_battle,strix);
    player->captiontexid=font_render_to_texture(0,g.font,src,srcc,FBW,FBH,0xffffffff);
    egg_texture_get_size(&player->captionw,&player->captionh,player->captiontexid);
  } else {
    player->captiontexid=0;
    player->captionw=0;
    player->captionh=0;
  }
}

/* Init player.
 */
 
static void sumohorse_random_correct(int *v) {
  int candidatec=0;
  int i=STEP_LIMIT;
  while (i-->0) if (!v[i]) candidatec++;
  if (candidatec<1) return;
  int candidatep=rand()%candidatec;
  for (i=STEP_LIMIT;i-->0;) {
    if (v[i]) continue;
    if (!candidatep--) {
      v[i]=1;
      return;
    }
  }
}
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->dstx=FBW/3;
  } else { // Right.
    player->who=1;
    player->dstx=(FBW*2)/3;
    player->xform=EGG_XFORM_XREV;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    /* Select a count of correct steps per skill.
     */
    int correctc=STEP_LIMIT;
         if (player->skill>=0.900) correctc=6;
    else if (player->skill>=0.600) correctc=5;
    else if (player->skill>=0.400) correctc=4; // <-- the typical case
    else if (player->skill>=0.200) correctc=3; // <-- princess here if we do that
    else if (player->skill>=0.100) correctc=2;
    else correctc=1;
    if (correctc>=STEP_LIMIT) memset(player->plan,0xff,sizeof(player->plan));
    else while (correctc-->0) sumohorse_random_correct(player->plan);
    /* Prepare failage: How far after the beat will I act?
     */
    player->failagehi=0.050+(1.0-player->skill)*0.200;
    player->failagelo=player->failagehi*0.5;
    player->failage=(rand()&0xffff)/65535.0;
    player->failage=player->failage*player->failagelo+(1.0-player->failage)*player->failagehi;
  }
  switch (face) {
    case NS_face_monster: {
        player->srcy=96;
        player->color=0xaa8a58ff;
      } break;
    case NS_face_dot: {
        player->srcy=0;
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->srcy=48;
        player->color=0x0d3ac1ff;
      } break;
  }
}

/* Begin stage.
 */
 
static void sumohorse_begin_stage(struct battle *battle,int stage) {
  BATTLE->stageclock=0.0;
  BATTLE->stage=stage;
  BATTLE->metroclock=0.0;
  BATTLE->readyset=5; // The initial value will never be displayed; starts at (n-1).
  BATTLE->stepc=0;
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  l->active=r->active=0;
  l->freshsteps=r->freshsteps=0;
  switch (stage) {
    case STAGE_RDANCE: {
        sumohorse_set_caption(battle,l,221);
        sumohorse_set_caption(battle,r,222);
        r->active=1;
        r->freshsteps=1;
      } break;
    case STAGE_LREPEAT: {
        sumohorse_set_caption(battle,l,223);
        sumohorse_set_caption(battle,r,0);
        l->active=1;
      } break;
    case STAGE_LDANCE: {
        sumohorse_set_caption(battle,l,222);
        sumohorse_set_caption(battle,r,221);
        l->active=1;
        l->freshsteps=1;
      } break;
    case STAGE_RREPEAT: {
        sumohorse_set_caption(battle,l,0);
        sumohorse_set_caption(battle,r,223);
        r->active=1;
      } break;
    case STAGE_REPORT: {
        sumohorse_set_caption(battle,l,0);
        sumohorse_set_caption(battle,r,0);
        BATTLE->readyset=0;
        BATTLE->stepc=STEP_LIMIT;
      } break;
  }
}

static void sumohorse_next_stage(struct battle *battle) {
  switch (BATTLE->stage) {
    case STAGE_RDANCE:  sumohorse_begin_stage(battle,STAGE_LREPEAT); break;
    case STAGE_LREPEAT: sumohorse_begin_stage(battle,STAGE_LDANCE); break;
    case STAGE_LDANCE:  sumohorse_begin_stage(battle,STAGE_RREPEAT); break;
    case STAGE_RREPEAT: sumohorse_begin_stage(battle,STAGE_REPORT); break;
  }
}

static struct step *sumohorse_get_active_dance(struct battle *battle) {
  switch (BATTLE->stage) {
    case STAGE_RDANCE: return BATTLE->rdance;
    case STAGE_LREPEAT: return BATTLE->lrepeat;
    case STAGE_LDANCE: return BATTLE->ldance;
    case STAGE_RREPEAT: return BATTLE->rrepeat;
  }
  return 0;
}

/* New.
 */
 
static int _sumohorse_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  bm_song_force(0);
  sumohorse_begin_stage(battle,STAGE_RDANCE);
  return 0;
}

/* A significant beat has elapsed just now.
 */
 
static void sumohorse_commit_beat(struct battle *battle) {
  // Not sure we do anything with this. player_update_common() does the real work.
  if (BATTLE->stepc<STEP_LIMIT) {
    BATTLE->stepc++;
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->lean=-1;  break;
    case EGG_BTN_RIGHT: player->lean=1; break;
    default: player->lean=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  /* When (metroclock) is high and (stepc>0), a beat has just elapsed.
   */
  player->lean=0;
  if ((BATTLE->metroclock>METRONOME_PERIOD-0.300)&&(BATTLE->metroclock<METRONOME_PERIOD-player->failage)&&(BATTLE->stepc>0)) {
    struct step *stepv=sumohorse_get_active_dance(battle);
    if (stepv) {
      int stepp=BATTLE->stepc-1;
      if ((stepp>=0)&&(stepp<STEP_LIMIT)&&!stepv[stepp].visited) {
        // Haven't encoded this beat yet. Opportunity to do so.
        stepv[stepp].visited=1;
        if (player->freshsteps) { // Make something up.
          // Never skip the first beat, it's awkward.
          if (stepp==0) {
            player->lean=(rand()&1)?-1:1;
          } else switch (rand()%3) {
            case 0: player->lean=-1; break;
            case 1: player->lean=1; break;
          }
        } else { // Do what the opponent did, or fail to.
          const struct step *ref=stepv; // Don't let it be null (it won't be but hey)
          switch (BATTLE->stage) {
            case STAGE_LREPEAT: ref=BATTLE->rdance; break;
            case STAGE_RREPEAT: ref=BATTLE->ldance; break;
          }
          if (player->plan[stepp]) { // Get it right. Easy.
            player->lean=ref[stepp].lean;
          } else { // Get it wrong. Two options.
            if (ref[stepp].lean<0) player->lean=(rand()&1)?0:1;
            else if (ref[stepp].lean>0) player->lean=(rand()&1)?-1:0;
            else player->lean=(rand()&1)?-1:1;
          }
        }
        player->failage=(rand()&0xffff)/65535.0;
        player->failage=player->failage*player->failagelo+(1.0-player->failage)*player->failagehi;
      }
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->lean!=player->pvlean) {
    if (player->pvlean=player->lean) {
      // Changed to a nonzero lean. Encode a step.
      player->leanhold=0.333;
      struct step *stepv=sumohorse_get_active_dance(battle);
      if (stepv&&(BATTLE->stageclock>STAGE_LEAD_TIME-METRONOME_PERIOD)) { // Ignore strokes way before start time.
        const double halfperiod=METRONOME_PERIOD*0.5;
        int stepp;
        double offset;
        if (BATTLE->stageclock<STAGE_LEAD_TIME) { // Before the first bell, it binds to [0] but offset is computed per (stageclock).
          stepp=0;
          offset=(BATTLE->stageclock-STAGE_LEAD_TIME)/halfperiod;
        } else if (BATTLE->metroclock>=halfperiod) { // High (metroclock), bind to (stepc-1), and we're late.
          stepp=BATTLE->stepc-1;
          offset=(METRONOME_PERIOD-BATTLE->metroclock)/halfperiod;
        } else { // Low (metroclock), bind to (stepc), and we're early.
          stepp=BATTLE->stepc;
          offset=-BATTLE->metroclock/halfperiod;
        }
        if (stepp<0) stepp=0; else if (stepp>=STEP_LIMIT) stepp=STEP_LIMIT-1;
        if (stepv[stepp].lean) {
          // Already encoded something here. Retain the first stroke and ignore this new one.
        } else {
          if (offset<-1.0) offset=-1.0; else if (offset>1.0) offset=1.0;
          stepv[stepp].lean=player->lean;
          stepv[stepp].offset=offset;
        }
      }
    }
  }
}

/* Decide the final outcome. All step lists are final.
 * First we consider discrete correct steps. If that yields a winner, that's all.
 * If the correct step counts are a tie, we then consider RMS of the timing offsets.
 * But among two humans, timing is a fair contest.
 */
 
static void sumohorse_decide_outcome(struct battle *battle) {
  int lc=0,rc=0; // Count of correct steps.
  int i=STEP_LIMIT;
  while (i-->0) {
    if (BATTLE->lrepeat[i].lean==BATTLE->rdance[i].lean) lc++;
    if (BATTLE->rrepeat[i].lean==BATTLE->ldance[i].lean) rc++;
  }
  if (lc>rc) { battle->outcome=1; return; }
  if (lc<rc) { battle->outcome=-1; return; }
  double lss=0.0,rss=0.0; // Sum of squared offsets, for only the Repeat stage. We do record for Dance stage, but the scoreboard doesn't show it, so meh.
  for (i=STEP_LIMIT;i-->0;) {
    lss+=BATTLE->lrepeat[i].offset*BATTLE->lrepeat[i].offset;
    rss+=BATTLE->rrepeat[i].offset*BATTLE->rrepeat[i].offset;
  }
  double lrms=sqrt(lss/(STEP_LIMIT*1.0));
  double rrms=sqrt(rss/(STEP_LIMIT*1.0));
  if (lrms<rrms) { battle->outcome=1; return; }
  if (lrms>rrms) { battle->outcome=-1; return; }
  // Ties are possible but very unlikely.
  battle->outcome=0;
}

/* Update.
 */
 
static void _sumohorse_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* Update global clocks.
   * Play the metronome and advance beat if we're in any stage but REPORT, and beyond the lead time.
   */
  BATTLE->stageclock+=elapsed;
  if ((BATTLE->stageclock>=STAGE_LEAD_TIME)&&(BATTLE->stage!=STAGE_REPORT)) {
    if ((BATTLE->metroclock-=elapsed)<=0.0) {
      if (BATTLE->stepc>=STEP_LIMIT) {
        sumohorse_next_stage(battle);
      } else {
        BATTLE->metroclock+=METRONOME_PERIOD;
        bm_sound(RID_sound_cowbell);
        if (BATTLE->readyset>0) BATTLE->readyset--;
        if (!BATTLE->readyset) { // NB Do run this bit on the stroke that goes from 1 to 0.
          sumohorse_commit_beat(battle);
        }
      }
    }
  }
  
  /* In STAGE_REPORT, we borrow (stepc) and (metroclock) to pay out display of the final scores.
   */
  if ((BATTLE->stage==STAGE_REPORT)&&(BATTLE->reportc<STEP_LIMIT)) {
    if ((BATTLE->metroclock-=elapsed)<=0.0) {
      BATTLE->metroclock+=0.200;
      BATTLE->reportc++;
      if (BATTLE->reportc>=STEP_LIMIT) {
        sumohorse_decide_outcome(battle);
      }
    }
  }
  
  /* Update players.
   * When their (leanhold) is set, we count it down automatically and do not update them.
   * Also, we don't update them when inactive.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->leanhold>0.0) {
      player->leanhold-=elapsed;
    } else if (!player->active) {
      player->lean=0;
    } else {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
    player_update_common(battle,player,elapsed);
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int decalw=48,decalh=48;
  int srcx=0; // 0,1,2 = neutral,left,right. For the untransformed orientation.
  if (player->lean<0) srcx=player->xform?2:1;
  else if (player->lean>0) srcx=player->xform?1:2;
  srcx*=decalw;
  graf_decal_xform(&g.graf,player->dstx-(decalw>>1),(FBH>>1)-(decalh>>1),srcx,player->srcy,decalw,decalh,player->xform);
  
  // Word bubble when leaning.
  if (player->lean) {
    int dstx=player->dstx-NS_sys_tilesize+player->lean*2;
    int dsty=(FBH>>1)-(decalh>>1)-NS_sys_tilesize*2;
    graf_decal(&g.graf,dstx,dsty,0,decalh*3,NS_sys_tilesize*2,NS_sys_tilesize*2);
    uint8_t xform=(player->lean<0)?0:EGG_XFORM_XREV;
    graf_decal_xform(&g.graf,dstx+8,dsty+6,NS_sys_tilesize*2,NS_sys_tilesize*9,NS_sys_tilesize,NS_sys_tilesize,xform);
  }
}

/* Scoreboard.
 * We display the other guy's Dance and our own Repeat.
 */
 
static void scoreboard_render(
  struct battle *battle,
  struct player *player,
  uint32_t othercolor,
  const struct step *dance,
  const struct step *repeat
) {
  if (BATTLE->reportc<1) return;
  int boardw=NS_sys_tilesize*STEP_LIMIT;
  int boardx=player->dstx-(boardw>>1);
  int boardy=(FBH>>1)+30;
  int dstx=boardx+(NS_sys_tilesize>>1);
  int dsty=boardy+(NS_sys_tilesize>>1); // For top row.
  
  // For the left player only, draw the bell off to the left of the bottom row, so players understand that row is timing.
  if (!player->who) {
    graf_fancy(&g.graf,dstx-NS_sys_tilesize-4,dsty+NS_sys_tilesize*2,0x94,0,0,NS_sys_tilesize,0,0x808080ff);
  }
  
  int i=0;
  for (;i<BATTLE->reportc;i++,dstx+=NS_sys_tilesize) {
    uint8_t dtile=0xa5;
    uint8_t dxform=0;
    uint8_t rtile=0xa5;
    uint8_t rxform=0;
    uint32_t rcolor;
    uint8_t ttile=0x96; // +0,1,2 = poop,check,star
    if (dance[i].lean<0) dtile=0xa2; 
    else if (dance[i].lean>0) { dtile=0xa2; dxform=EGG_XFORM_XREV; }
    if (repeat[i].lean<0) rtile=0xa2;
    else if (repeat[i].lean>0) { rtile=0xa2; rxform=EGG_XFORM_XREV; }
    double timing=repeat[i].offset;
    if (timing<0.0) timing=-timing;
         if (repeat[i].offset<0.080) ttile+=2;
    else if (repeat[i].offset<0.200) ttile+=1;
    if (dance[i].lean==repeat[i].lean) rcolor=0x00ff00ff;
    else rcolor=0xff0000ff;
    graf_fancy(&g.graf,dstx,dsty,dtile,dxform,0,NS_sys_tilesize,0,othercolor);
    graf_fancy(&g.graf,dstx,dsty+NS_sys_tilesize,rtile,rxform,0,NS_sys_tilesize,0,rcolor);
    graf_fancy(&g.graf,dstx,dsty+NS_sys_tilesize*2,ttile,0,0,NS_sys_tilesize,0,0x808080ff);
  }
}

/* Render.
 */
 
static void _sumohorse_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_athletes);
  
  // Referee.
  {
    int refx=FBW>>1;
    int refy=(FBH>>1)-25; // Position of lower tile, he's 1x2.
    uint8_t xform=0;
    switch (BATTLE->stage) {
      case STAGE_LDANCE:
      case STAGE_LREPEAT:
        xform=EGG_XFORM_XREV;
        break;
    }
    int bellx=refx+(xform?-NS_sys_tilesize:NS_sys_tilesize);
    int belly=refy-NS_sys_tilesize-3;
    int army=refy-12;
    int armx=refx;
    if ((BATTLE->stageclock>=STAGE_LEAD_TIME)&&(BATTLE->stage!=STAGE_REPORT)) {
      double d=(BATTLE->metroclock/METRONOME_PERIOD)*2.0;
      if (d>=1.0) d=2.0-d;
      d=1.0-d;
      armx+=(int)((bellx-armx)*d);
    }
    graf_tile(&g.graf,armx,army,0xa4,xform);
    graf_tile(&g.graf,refx,refy,0xa3,xform);
    graf_tile(&g.graf,refx,refy-NS_sys_tilesize,0x93,xform);
    uint8_t belltile=0x94;
    if (BATTLE->metroclock>METRONOME_PERIOD-0.150) belltile++;
    graf_tile(&g.graf,bellx,belly,belltile,xform);
  }
  
  // Players, just the image:battle_athletes bits.
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // Scoreboards, in STAGE_REPORT.
  if (BATTLE->stage==STAGE_REPORT) {
    scoreboard_render(battle,BATTLE->playerv+0,BATTLE->playerv[1].color,BATTLE->rdance,BATTLE->lrepeat);
    scoreboard_render(battle,BATTLE->playerv+1,BATTLE->playerv[0].color,BATTLE->ldance,BATTLE->rrepeat);
  }
  
  // Captions.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (!player->captionw) continue;
    graf_set_input(&g.graf,player->captiontexid);
    int dstx=player->dstx-(player->captionw>>1);
    int dsty=(FBH>>1)+30;
    graf_decal(&g.graf,dstx,dsty,0,0,player->captionw,player->captionh);
  }
  
  // Lead-in counter.
  if (BATTLE->readyset&&(BATTLE->stageclock>=STAGE_LEAD_TIME)) {
    int alpha=(BATTLE->metroclock*255.0)/METRONOME_PERIOD;
    if (alpha>0) {
      if (alpha>0xff) alpha=0xff;
      graf_set_image(&g.graf,RID_image_fonttiles);
      graf_set_alpha(&g.graf,alpha);
      graf_tile(&g.graf,FBW>>1,FBH>>1,'0'+BATTLE->readyset,0);
      graf_set_alpha(&g.graf,0xff);
    }
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_sumohorse={
  .name="sumohorse",
  .objlen=sizeof(struct battle_sumohorse),
  .id=NS_battle_sumohorse,
  .strix_name=158,
  .no_article=1,
  .no_contest=1,
  .support_pvp=1,
  .support_cvc=1,
  .del=_sumohorse_del,
  .init=_sumohorse_init,
  .update=_sumohorse_update,
  .render=_sumohorse_render,
};

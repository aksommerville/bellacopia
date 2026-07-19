/* battle_snaring.c
 */

#include "game/bellacopia.h"

#define NOTE_LIMIT 32

#define STAGE_INTRO     1 /* Introduce the reference riff. */
#define STAGE_REF       2 /* Play the reference riff. */
#define STAGE_INTERLUDE 3 /* Get players ready. */
#define STAGE_PLAY      4 /* Take input from players. */
#define STAGE_FINAL     5 /* Wrap up. */

struct battle_snaring {
  struct battle hdr;
  double notev[NOTE_LIMIT]; // The reference riff, in beats so a range of 0..4. Ordered.
  int notec;
  double refarm;
  double clock; // Counts up per stage.
  int stage;
  int metronome; // The next beat to sound.
  int notep; // In STAGE_REF, which note is next.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // 1x2 tiles, with a loose arm at +1
    uint8_t xform;
    int x; // Center of sprite in framebuffer pixels.
    double arm; // 0..1 = idle..whack, position of the arm
    double notev[NOTE_LIMIT];
    int notec;
    double planv[NOTE_LIMIT]; // CPU players decide in advance when they're going to strike.
    int planc,planp;
    double score; // 0..1
  } playerv[2];
};

#define BATTLE ((struct battle_snaring*)battle)

/* Delete.
 */
 
static void _snaring_del(struct battle *battle) {
  battle_unsong();
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW/3;
  } else { // Right.
    player->who=1;
    player->x=(FBW*2)/3;
    player->xform=EGG_XFORM_XREV;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xc80e0eff;
        player->tileid=0x60;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x20;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x40;
      } break;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
  
    /* With a penalty range of 0.250, the balanced CPU player is a match for me, we both win sometimes. (disregarding penalties)
     * At 0.100 he's really good. Don't let it go down to zero; that player would be perfect and totally unfair.
     */
    double fuzzlimit=player->skill*0.100+(1.0-player->skill)*0.250;
  
    /* Every stroke I'm going to make is planned in advance, ie right now.
     * We'll deliberately miss a fixed count of strokes per skill.
     * We never make up a false stroke; missing is just ignoring a reference stroke.
     * And each stroke we hit, we'll fuzz the timing a little, again per skill.
     */
    int missc;
         if (player->skill>0.750) missc=0;
    else if (player->skill>0.600) missc=1;
    else if (player->skill>0.450) missc=2; // The usual case.
    else if (player->skill>0.200) missc=3;
    else missc=4;
    if (missc>=BATTLE->notec) missc=BATTLE->notec-1; // Always hit at least one.
    int candidatev[NOTE_LIMIT];
    int candidatec=0;
    while (candidatec<BATTLE->notec) {
      candidatev[candidatec]=candidatec;
      candidatec++;
    }
    int missv[NOTE_LIMIT]; // index of missed notes, in (BATTLE->notev).
    int i=0;
    for (;i<missc;i++) {
      int cp=rand()%candidatec;
      missv[i]=candidatev[cp];
      candidatec--;
      memmove(candidatev+cp,candidatev+cp+1,sizeof(int)*(candidatec-cp));
    }
    for (i=0;i<BATTLE->notec;i++) {
      int miss=0;
      int j=missc; while (j-->0) {
        if (missv[j]==i) {
          miss=1;
          break;
        }
      }
      if (miss) continue; // Deliberate miss; skip this note.
      double err=((rand()&0xffff)*fuzzlimit)/65535.0-fuzzlimit*0.5;
      player->planv[player->planc++]=BATTLE->notev[i]+err;
    }
  
  }
}

/* New.
 */
 
static int _snaring_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  battle_song(0);
  BATTLE->stage=STAGE_INTRO;
  BATTLE->clock=0.0;
  BATTLE->metronome=-1;
  
  /* Generate the song.
   * I think 1/4 beats are reasonable.
   * The more off beats are less likely.
   */
  int notec=4+(rand()%4); // 4..8. Always at least 4 notes and at least 8 gaps.
  uint8_t selections[16]={0}; // 0,1, indexed by time position.
  struct candidate {
    int p;
    int weight;
  } candidatev[16]={
    { 0,128},
    { 1,  1},
    { 2,  4},
    { 3,  1},
    { 4, 16},
    { 5,  1},
    { 6,  4},
    { 7,  1},
    { 8, 16},
    { 9,  1},
    {10,  4},
    {11,  1},
    {12, 16},
    {13,  1},
    {14,  4},
    {15,  1},
  };
  int candidatec=16;
  if (notec>candidatec) return -1;
  struct candidate *candidate=candidatev;
  int i=candidatec;
  int wsum=0;
  for (;i-->0;candidate++) wsum+=candidate->weight;
  while (notec-->0) {
    int choice=rand()%wsum;
    candidate=candidatev;
    for (i=candidatec;i-->0;candidate++) {
      if ((choice-=candidate->weight)<0) break;
    }
    if (i<0) candidate=candidatev; // oops
    wsum-=candidate->weight;
    int p=candidate-candidatev;
    selections[candidate->p]=1;
    candidatec--;
    memmove(candidate,candidate+1,sizeof(struct candidate)*(candidatec-p));
  }
  for (i=0;i<16;i++) {
    if (!selections[i]) continue;
    if (BATTLE->notec>NOTE_LIMIT) return -1;
    BATTLE->notev[BATTLE->notec++]=i*0.250;
  }
  
  /* Initialize players after reference riff, so the CPU ones can arrange their plan.
   */
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  return 0;
}

/* Commit a stroke.
 */
 
static void player_whack(struct battle *battle,struct player *player) {
  player->arm=1.0;
  // Use two slightly different sounds, and pan fully, since they're likely to play at the same time.
  if (player->who) {
    bm_sound_pan(RID_sound_snare2,1.0);
  } else {
    bm_sound_pan(RID_sound_snare,-1.0);
  }
  if (player->notec<NOTE_LIMIT) {
    player->notev[player->notec++]=BATTLE->clock;
  }
}

/* Updating players turns out to be trivial.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    player_whack(battle,player);
  }
}
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->planp>=player->planc) return;
  if (BATTLE->clock<player->planv[player->planp]) return;
  player->planp++;
  player_whack(battle,player);
}
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if ((player->arm-=elapsed*3.000)<=0.0) player->arm=0.0;
}

/* Calculate one player's score.
 */
 
static void snaring_rate(struct battle *battle,struct player *player) {
  
  /* Match each member of each list to its likeliest partner in the other.
   * Both sides may contain gaps.
   * Each note matches no more than one in the other list.
   * Since both are sorted, we can do this with an FSM, no overall per-note state.
   */
  int refp=0,actp=0,refskipc=0,actskipc=0,matchc=0;
  double sqsum=0.0;
  while ((refp<BATTLE->notec)||(actp<BATTLE->notec)) {
  
    /* First, if one list is exhausted, skip one in the other.
     * er actually, skip em all, we can do that.
     */
    if (refp>=BATTLE->notec) {
      actskipc+=player->notec-actp;
      break;
    }
    if (actp>=player->notec) {
      refskipc+=BATTLE->notec-refp;
      break;
    }
    
    /* Take the positive difference between these two notes.
     * Rather than flipping sign, square it -- we want to compute the standard deviation anyway.
     */
    double d2=BATTLE->notev[refp]-player->notev[actp];
    d2*=d2;
    
    /* If there's another beyond either readhead, compare that to current position.
     * If not, give it an impossibly high delta.
     */
    double dref2=999.999,dact2=999.999;
    if (refp+1<BATTLE->notec) {
      dref2=BATTLE->notev[refp+1]-player->notev[actp];
      dref2*=dref2;
    }
    if (actp+1<player->notec) {
      dact2=BATTLE->notev[refp]-player->notev[actp+1];
      dact2*=dact2;
    }
    
    /* Comparing these three squared deltas tells us whether to consume partners here or skip one side.
     * If we choose to skip, don't record it yet, just take the one step on the skip side.
     */
    if ((d2<=dref2)&&(d2<=dact2)) { // Match.
      refp++;
      actp++;
      matchc++;
      sqsum+=d2;
    } else if (dref2<dact2) { // Skip reference.
      refp++;
      refskipc++;
    } else { // Skip actual.
      actp++;
      actskipc++;
    }
  }
  
  // Initial stab at the score is RMS of the deltas. High is bad. Expect a range of 0..1/8 or so.
  // If no notes were matched, expect the penalty stage to cover it, but start it off high.
  if (matchc>0) {
    player->score=sqrt(sqsum/matchc);
  } else {
    player->score=0.250;
  }
  
  // Add a huge penalty for every mismatched note.
  // Could distinguish "missed" from "extra" but I don't think it's helpful to.
  player->score+=refskipc*0.250;
  player->score+=actskipc*0.250;
  
  // When CPU are playing, let their plans drive the bias. Man vs man, apply an artificial bias right here.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->human&&r->human) {
    if (battle->args.bias>=0x80) { // Favor (r) by increasing (l).
      if (l==player) {
        player->score+=(battle->args.bias-0x80)/500.0;
      }
    } else { // Favor (l) by increasing (r).
      if (r==player) {
        player->score+=(0x80-battle->args.bias)/500.0;
      }
    }
  }
  
  // Now flip it, so we have something close to 1.0, with high good.
  player->score=1.0-player->score;
  
  // Clamp to 0..1 and we're done.
  if (player->score<0.0) player->score=0.0;
  else if (player->score>1.0) player->score=1.0;
}

/* Update during INTRO and INTERLUDE stages: Metronome plays, and advance stage at 4.0.
 */
 
static void snaring_update_INTRO(struct battle *battle,double elapsed) {
  if (BATTLE->clock>=4.0) {
    BATTLE->stage++;
    BATTLE->clock=0.0;
    BATTLE->metronome=-1;
  } else {
    int metnext=(int)BATTLE->clock;
    if (metnext>BATTLE->metronome) {
      bm_sound(RID_sound_cowbell);
      BATTLE->metronome=metnext;
    }
  }
}
 
static void snaring_update_INTERLUDE(struct battle *battle,double elapsed) {
  if (BATTLE->clock>=4.0) {
    BATTLE->stage++;
    BATTLE->clock=0.0;
    BATTLE->metronome=-1;
    // STAGE_PLAY is starting. Player might have struck early. If their button is still down, commit it.
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (!player->human) continue;
      if (g.input[player->human]&EGG_BTN_SOUTH) player_whack(battle,player);
    }
  } else {
    int metnext=(int)BATTLE->clock;
    if (metnext>BATTLE->metronome) {
      bm_sound(RID_sound_cowbell);
      BATTLE->metronome=metnext;
    }
  }
}

/* Update during REF stage: Play the reference riff, which has already been arranged.
 */
 
static void snaring_update_REF(struct battle *battle,double elapsed) {
  if (BATTLE->clock>=4.0) {
    BATTLE->stage=STAGE_INTERLUDE;
    BATTLE->clock=0.0;
    BATTLE->metronome=-1;
  } else {
    if (BATTLE->notep<BATTLE->notec) {
      if (BATTLE->clock>=BATTLE->notev[BATTLE->notep]) {
        bm_sound(RID_sound_snare);
        BATTLE->notep++;
        BATTLE->refarm=1.0;
      }
    }
    // Debatable, but I think a metronome is still helpful during REF.
    int metnext=(int)BATTLE->clock;
    if (metnext>BATTLE->metronome) {
      bm_sound(RID_sound_cowbell);
      BATTLE->metronome=metnext;
    }
  }
}

/* Update during PLAY stage: Metronome and termination just like INTRO.
 */
 
static void snaring_update_PLAY(struct battle *battle,double elapsed) {
  if (BATTLE->clock>=4.0) {
    BATTLE->stage=STAGE_FINAL;
    BATTLE->clock=0.0;
    BATTLE->metronome=-1;
    snaring_rate(battle,BATTLE->playerv+0);
    snaring_rate(battle,BATTLE->playerv+1);
  } else {
    int metnext=(int)BATTLE->clock;
    if (metnext>BATTLE->metronome) {
      bm_sound(RID_sound_cowbell);
      BATTLE->metronome=metnext;
    }
  }
}

/* Update during FINAL stage: After a tasteful interval, terminate the battle.
 */

static void snaring_update_FINAL(struct battle *battle,double elapsed) {
  if (BATTLE->clock>1.0) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Update.
 */
 
static void _snaring_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (BATTLE->stage==STAGE_PLAY) { // Only update controller if it's my turn.
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
    player_update_common(battle,player,elapsed);
  }
  
  /* Update the drum machine.
   */
  if ((BATTLE->refarm-=3.0*elapsed)<=0.0) BATTLE->refarm=0.0;
  BATTLE->clock+=elapsed;
  switch (BATTLE->stage) {
    case STAGE_INTRO: snaring_update_INTRO(battle,elapsed); break;
    case STAGE_REF: snaring_update_REF(battle,elapsed); break;
    case STAGE_INTERLUDE: snaring_update_INTERLUDE(battle,elapsed); break;
    case STAGE_PLAY: snaring_update_PLAY(battle,elapsed); break;
    case STAGE_FINAL: snaring_update_FINAL(battle,elapsed); break;
  }
}

/* Render a chart.
 * This uses tiles, caller is expected to load the image.
 */
 
static void snaring_render_chart(struct battle *battle,int midx,int midy,double playhead,const double *notev,int notec) {
  // Each measure is two tiles, and there's a cap on each end.
  int fullw=(1+4*2+1)*NS_sys_tilesize;
  int xl=midx-(fullw>>1);
  int x=xl+(NS_sys_tilesize>>1);
  graf_tile(&g.graf,x,midy,0x22,0); x+=NS_sys_tilesize;
  int i=4; while (i-->0) {
    graf_tile(&g.graf,x,midy,0x23,0); x+=NS_sys_tilesize;
    graf_tile(&g.graf,x,midy,0x24,0); x+=NS_sys_tilesize;
  }
  graf_tile(&g.graf,x,midy,0x25,0);
  int time0x=xl+NS_sys_tilesize+(NS_sys_tilesize>>2); // The extra quarter-tile is because dots don't appear at the measure line, they're offset a bit.
  int timezx=xl+fullw-NS_sys_tilesize;
  // If there's a playhead, it goes between the lines and the dots.
  if ((playhead>=0.0)&&(playhead<=4.0)) {
    int phx=time0x+(int)(playhead*NS_sys_tilesize*2.0);
    if (phx<timezx) graf_tile(&g.graf,phx,midy,0x27,0);
  }
  // Then the notes.
  for (;notec-->0;notev++) {
    if ((*notev<-0.5)||(*notev>4.5)) continue; // A little affordance for early and late notes, they can render off the chart a little.
    int nx=time0x+(int)((*notev)*NS_sys_tilesize*2.0);
    graf_tile(&g.graf,nx,midy,0x26,0);
  }
}

/* Render drum machine. It's a lot like a player.
 */
 
static void drum_machine_render(struct battle *battle) {
  int x=FBW>>1;
  int y=30;
  int armx=x-(NS_sys_tilesize>>1);
  uint8_t tileid=0x31;
  if (BATTLE->refarm>0.600) tileid=0x51;
  graf_set_image(&g.graf,RID_image_battle_war);
  int army=y-1;
  army-=(int)((1.0-BATTLE->refarm)*12.0);
  graf_set_image(&g.graf,RID_image_battle_war);
  graf_tile(&g.graf,x,y,tileid,0);
  graf_tile(&g.graf,armx,army,0x71,0);
  double playhead=-1.0;
  if (BATTLE->stage==STAGE_REF) playhead=BATTLE->clock;
  snaring_render_chart(battle,x,y+20,playhead,BATTLE->notev,BATTLE->notep);
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int y=130; // Lower tile.
  int armx=player->x;
  int drumx=player->x;
  if (player->xform) {
    armx-=NS_sys_tilesize>>1;
    drumx-=NS_sys_tilesize;
  } else {
    armx+=NS_sys_tilesize>>1;
    drumx+=NS_sys_tilesize;
  }
  int army=y-1;
  army-=(int)((1.0-player->arm)*12.0);
  uint8_t drumtile=0x31;
  if (player->arm>0.600) drumtile=0x51;
  graf_set_image(&g.graf,RID_image_battle_war);
  graf_tile(&g.graf,drumx,y,drumtile,player->xform);
  graf_tile(&g.graf,armx,army,player->tileid+0x01,player->xform);
  graf_tile(&g.graf,player->x,y-NS_sys_tilesize,player->tileid,player->xform);
  graf_tile(&g.graf,player->x,y,player->tileid+0x10,player->xform);
  double playhead=-1.0;
  if (BATTLE->stage==STAGE_PLAY) playhead=BATTLE->clock;
  int chartx=(FBW*(player->who?3:1))>>2;
  snaring_render_chart(battle,chartx,y+20,playhead,player->notev,player->notec);
}

/* Render.
 */
 
static void _snaring_render(struct battle *battle) {
  const int groundy=138;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,groundy,FBW,FBH-groundy,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,groundy,FBW,1,0x000000ff);
  
  // During STAGE_FINAL, animate our scores growing.
  if (BATTLE->stage==STAGE_FINAL) {
    double n=BATTLE->clock;
    if (n<0.0) n=0.0; else if (n>1.0) n=1.0;
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    int barh=60;
    int lh=(int)(l->score*n*barh);
    int rh=(int)(r->score*n*barh);
    graf_fill_rect(&g.graf,(FBW>>1)-4,groundy-lh,4,lh,l->color);
    graf_fill_rect(&g.graf,(FBW>>1),groundy-rh,4,rh,r->color);
  }
  
  drum_machine_render(battle);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // When the metronome is counting us in, display it.
  if ((BATTLE->stage==STAGE_INTRO)||(BATTLE->stage==STAGE_INTERLUDE)) {
    int s=(int)(5.0-BATTLE->clock);
    if (s<1) s=1; else if (s>4) s=4;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_tile(&g.graf,FBW>>1,80,'0'+s,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_snaring={
  .name="snaring",
  .objlen=sizeof(struct battle_snaring),
  .id=NS_battle_snaring,
  .strix_name=288,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_a,
  .del=_snaring_del,
  .init=_snaring_init,
  .update=_snaring_update,
  .render=_snaring_render,
};

/* battle_morsecode.c
 * "Morse" is French for "walrus", hence we have a walrus doing Morse Code.
 * I'm here all week, folks.
 * Ooh ooh! Also, because this battle only happens in the far north, let's decorate with "Northern Union" branding.
 *
 * I've never used Morse Code before. So here is literally everything I know about it:
 * Summarizing the rules from https://en.wikipedia.org/wiki/Morse_code
 *  - Dot duration is one unit, let's call it "beat".
 *  - Dash duration is three beats.
 *  - Each dot or dash is followed by a silence of one beat.
 *  - Three beats between letters. (presumably including the one beat after the last pulse).
 *  - Seven beats between words. (presumably including the three beats after the last letter).
 *
 * We must use relative timing, not a fixed beat. That's kind of the whole point, how fast can you encode morse?
 * So I think we need to capture the whole input sequence before attempting to decode anything.
 * We can print it in real time as a square wave or something.
 *
 * There will be a global timeout, and also we'll stop capturing after both players have gone nonzero and then zero for some long interval.
 * The global timeout *does* include initial idle time, but for all other purposes things start happening at your first key-down.
 */

#include "game/bellacopia.h"

#define MESSAGE_LIMIT 32
#define SIGNAL_LIMIT 256
#define OUTPUT_LIMIT 64

/* From the diagram of International Morse Code found at https://en.wikipedia.org/wiki/Morse_code
 */
static const char *morse_letters[26]={
  ".-"  ,"-...","-.-.", // ABC
  "-.." ,"."   ,"..-.", // DEF
  "--." ,"....",".."  , // GHI
  ".---","-.-" ,".-..", // JKL
  "--"  ,"-."  ,"---" , // MNO
  ".--.","--.-",".-." , // PQR
  "..." ,"-"   ,"..-" , // STU
  "...-",".--" ,"-..-", // VWX
  "-.--","--..",        // YZ
};
static const char *morse_digits[10]={ // We don't use them, but just for the sake of completeness.
  "-----", // 0
  ".----", // 1
  "..---", // 2
  "...--", // 3
  "....-", // 4
  ".....", // 5
  "-....", // 6
  "--...", // 7
  "---..", // 8
  "----.", // 9
};

struct battle_morsecode {
  struct battle hdr;
  int songon;
  int started; // Goes nonzero at the first ON state for either player.
  double silent_time; // Counts up when (started) and both states OFF.
  double panic_time; // Counts down always. Races the global timeout.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid;
    char message[MESSAGE_LIMIT]; // A-Z and space only.
    int messagec;
    int state; // 0,1 = idle,ON. Controller sets only this.
    int pvstate;
    double signalclock;
    double signal[SIGNAL_LIMIT]; // Absolute time of state changes. [0] is an ON event, and should be at time zero.
    int signalc;
    char output[OUTPUT_LIMIT]; // Derived from (signal), for comparison to (message). It can be longer.
    int outputc;
    uint32_t *sigimg_rgba;
    int sigimg_texid;
    int sigimg_w;
    int sigimg_h;
    int siggen_x,siggen_y; // (0,0) until the first stroke. (y) will never be zero again once running.
    double siggen_clock;
    double score;
    
    // For humans:
    int blackout;
    
    // For CPU:
    double beatlen; // Const, established at init.
    double beatclock; // Counts down to the next state change.
    int holdc; // Retain current state for so many beats. Counts down.
    const char *glyph; // [.-]+, what we're encoding next. Head advances, NUL terminated.
    int messagep; // Next thing to glyph up.
  } playerv[2];
};

#define BATTLE ((struct battle_morsecode*)battle)

/* Delete.
 */
 
static void player_cleanup(struct player *player) {
  if (player->sigimg_rgba) free(player->sigimg_rgba);
  egg_texture_del(player->sigimg_texid);
}
 
static void _morsecode_del(struct battle *battle) {
  egg_play_song(3,0,0,0.0,0.0);
  player_cleanup(BATTLE->playerv+0);
  player_cleanup(BATTLE->playerv+1);
}

/* Init player.
 * Message is not established here; see _morsecode_init().
 */
 
static int player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->beatlen=0.200; // Painfully slow but for one like myself that doesn't even know Morse code, it's actually pretty hard to beat this.
    double adjust=((rand()&0xffff)/65535.0);
    adjust=0.900*adjust+1.100*(1.0-adjust);
    player->beatlen*=adjust; // +- 10% just to keep it spicy.
    player->beatclock=1.000+player->beatlen; // Some initial delay.
    player->glyph=""; // Must not be null, ever.
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x02;
      } break;
    case NS_face_dot: {
        player->tileid=0x06;
      } break;
    case NS_face_princess: {
        player->tileid=0x0a;
      } break;
  }
  
  /* Prepare buffer and texture for the signal image.
   */
  player->sigimg_w=(FBW>>1)-10;
  player->sigimg_h=40;
  if (!(player->sigimg_rgba=calloc(player->sigimg_w<<2,player->sigimg_h))) return -1;
  //uint32_t *p=player->sigimg_rgba; int i=player->sigimg_w*player->sigimg_h; for (;i-->0;p++) *p=rand();//XXX noise to check sizing
  if ((player->sigimg_texid=egg_texture_new())<1) return -1;
  if (egg_texture_load_raw(player->sigimg_texid,player->sigimg_w,player->sigimg_h,player->sigimg_w<<2,player->sigimg_rgba,player->sigimg_w*player->sigimg_h*4)<0) return -1;
  
  return 0;
}

/* Set message in player.
 */
 
static int player_set_message(struct battle *battle,struct player *player,int strix) {
  const char *src;
  int srcc=text_get_string(&src,RID_strings_battle,strix);
  if ((srcc<1)||(srcc>MESSAGE_LIMIT)) {
    fprintf(stderr,"morsecode invalid string %d (len %d, must be 1..%d)\n",strix,srcc,MESSAGE_LIMIT);
    return -1;
  }
  if ((src[0]==0x20)||(src[srcc-1]==0x20)) {
    fprintf(stderr,"Please remove leading or trailing space from strings:battle:%d, it will confuse morsecode.\n",strix);
    return -1;
  }
  memcpy(player->message,src,srcc);
  player->messagec=srcc;
  for (;srcc-->0;src++) {
    if ((*src>='A')&&(*src<='Z')) continue;
    if (*src==' ') continue;
    fprintf(stderr,"morsecode invalid character '%c' 0x%02x in string %d\n",*src,(uint8_t)(*src),strix);
    return -1;
  }
  return 0;
}

/* New.
 */
 
static int _morsecode_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  if (player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface)<0) return -1;
  if (player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface)<0) return -1;
  
  /* Select a message for each player.
   * Our messages increase in length, one character each step.
   * But the total length doesn't matter so much as the difference in length player to player.
   * So we'll select a delta, then position it randomly.
   * Delta at least one: Don't give both players the same message.
   */
  const int strixlo=239;
  const int strixhi=257; // inclusive
  const int strixc=strixhi-strixlo+1;
  int delta=battle->args.bias-0x80;
  if (delta<0) delta=-delta;
  delta=(delta*strixc)/120;
  if (delta<1) delta=1; // Don't show them the same message, even at balanced bias.
  else if (delta>=strixc-1) delta=strixc-1; // At the limit, we show the first and last messages.
  struct player *shorty=BATTLE->playerv+0;
  struct player *longy=BATTLE->playerv+1;
  if (battle->args.bias<0x80) ; // Dot short, Walrus long, as our default says.
  else if (battle->args.bias>0x80) { // Dot long, Walrus short.
    shorty=BATTLE->playerv+1;
    longy=BATTLE->playerv+0;
  } else { // Balanced bias is extremely common, so don't lean one way or the other: Pick randomly.
    if (rand()&1) {
      shorty=BATTLE->playerv+1;
      longy=BATTLE->playerv+0;
    }
  }
  int sstrix=strixlo+rand()%(strixc-delta);
  int lstrix=sstrix+delta;
  // The formula above should have yielded valid strix. But I'm not as smart as I think I am, so let's make sure:
  if (sstrix<strixlo) sstrix=strixlo; else if (sstrix>strixhi) sstrix=strixhi;
  if (lstrix<strixlo) lstrix=strixlo; else if (lstrix>strixhi) lstrix=strixhi;
  if (player_set_message(battle,shorty,sstrix)<0) return -1;
  if (player_set_message(battle,longy,lstrix)<0) return -1;
   
  egg_play_song(3,RID_song_morsecode,1,1.0,0.0);
  BATTLE->songon=1;
  BATTLE->panic_time=40.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->state=(input&EGG_BTN_SOUTH)?1:0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Pay out (beatclock) and (holdc).
   * The remainder only runs at state changes.
   */
  if ((player->beatclock-=elapsed)>0.0) return;
  player->beatclock+=player->beatlen;
  if ((player->holdc)-->0) return;
  
  /* If our state was ON, we now go OFF for 1, 3, or 7 beats.
   */
  if (player->state) {
    player->state=0;
    if (player->glyph[0]) player->holdc=0; // Between ticks.
    else if (player->messagep>=player->messagec) { // End of message.
      player->beatclock=999.999;
    } else if (player->message[player->messagep]==0x20) { // Between words. Consume space.
      player->holdc=6;
      player->messagep++;
    } else { // Between letters.
      player->holdc=2;
    }
    return;
  }
  
  /* We're about to go ON. If we don't have a glyph queued up, grab the next one.
   */
  if (!player->glyph[0]) {
    if (player->messagep>=player->messagec) {
      player->beatclock=999.999;
      return;
    }
    char ch=player->message[player->messagep++];
    if ((ch<'A')||(ch>'Z')) {
      // Spaces should have been consumed by the ON=>OFF block. We must have double spaces. I dunno, just skip a beat.
      // Same for anything that isn't a letter.
      return;
    }
    player->glyph=morse_letters[ch-'A'];
    if (!player->glyph[0]) return; // Won't happen but would be devastating if it does, so get out.
  }
  
  /* Schedule and consume the next chit in the glyph.
   */
  switch (*(player->glyph++)) {
    case '.': player->holdc=0; break; // Dots are one beat.
    case '-': player->holdc=2; break; // Dashes are three beats.
    default: return; // Huh? I dunno, stay off for a beat.
  }
  player->state=1;
}

/* siggen: Generate the signal image on the fly.
 * (sigimg_*) all get initialized at the start, but (siggen_*) are zero until the first ON state change.
 */
 
#define SIGGEN_S_PER_PX 0.080

static void player_siggen_next(struct battle *battle,struct player *player) {
  if (player->siggen_x>=player->sigimg_w) {
    player->siggen_x=0;
    player->siggen_y+=5;
  }
  if (player->siggen_y>=player->sigimg_h) return;
  player->sigimg_rgba[player->siggen_y*player->sigimg_w+player->siggen_x]=0x80808080; // Control line.
  if (player->state) { // ON highlight.
    player->sigimg_rgba[(player->siggen_y-1)*player->sigimg_w+player->siggen_x]=0xff000030;
  }
  player->siggen_x++;
  egg_texture_load_raw(player->sigimg_texid,player->sigimg_w,player->sigimg_h,player->sigimg_w<<2,player->sigimg_rgba,player->sigimg_w*player->sigimg_h*4);
}

static void player_siggen_init(struct battle *battle,struct player *player) {
  player->siggen_x=0;
  player->siggen_y=3;
  player->siggen_clock=SIGGEN_S_PER_PX;
  player_siggen_next(battle,player);
}

static void player_siggen_update(struct battle *battle,struct player *player,double elapsed) {
  for (;;) {
    player->siggen_clock-=elapsed;
    if (player->siggen_clock>0.0) return;
    player->siggen_clock+=SIGGEN_S_PER_PX;
    player_siggen_next(battle,player);
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (player->state!=player->pvstate) {
    uint8_t noteid=(player->human?0x41:0x3f)+player->who;
    if (player->pvstate=player->state) { // Press.
      if (!player->siggen_y) player_siggen_init(battle,player);
      uint8_t velocity=player->human?0x7f:0x00;
      egg_song_event_note_on(3,player->who,noteid,velocity);
    } else { // Release.
      egg_song_event_note_off(3,player->who,noteid);
    }
    if (player->signalc<SIGNAL_LIMIT) {
      player->signal[player->signalc++]=player->signalclock;
    }
  }
  
  /* Signal clock starts ticking when the player starts encoding.
   */
  if (player->signalc||player->state) {
    player->signalclock+=elapsed;
  }
  
  if (player->siggen_y) player_siggen_update(battle,player,elapsed);
}

/* Decode a signal into plain text for comparison to message.
 * Always returns in (0..dsta).
 * (src) is composed of absolute timestamps. Its even indices are ON events and odd indices are OFF.
 */
 
static int morsecode_decode(char *dst,int dsta,const double *src,int srcc) {
  if (dsta<1) return 0; // come on man
  int dstc=0;
  
  /* First convert (src) to a more tractable "on+off" struct with durations instead of absolute timestamps.
   * Each record is an ON and the following OFF.
   * The last record's OFF time will not be used.
   * If we end up with zero cycles, we're done.
   */
  #define cyclea (SIGNAL_LIMIT>>1)
  struct cycle {
    double on,off;
    char stroke; // '.' or '-', per (on).
    int post; // (0,1,2) = (stroke break, letter break, word break)
  } cyclev[cyclea];
  int cyclec=0;
  int srcp=0;
  for (;srcp<srcc;) {
    if (cyclec>=cyclea) break;
    struct cycle *cycle=cyclev+cyclec++;
    double ta=src[srcp++];
    double tb,tc;
    if (srcp<srcc) tb=src[srcp++]; else tb=ta;
    if (srcp<srcc) tc=src[srcp]; else tc=tb;
    if ((cycle->on=tb-ta)<0.0) cycle->on=0.0;
    if ((cycle->off=tc-tb)<0.0) cycle->off=0.0;
  }
  if (cyclec<1) return 0;
  
  /* Now the real dicey part: How long is a beat?
   * The ON times should all be N or N*3.
   * I guess we can take the average of all ON times, and split them above/below.
   */
  double onsum=0.0;
  struct cycle *cycle=cyclev;
  int i=cyclec;
  for (;i-->0;cycle++) onsum+=cycle->on;
  double onavg=onsum/cyclec;
  
  /* Assign (stroke) based on (onavg).
   * Take the average of all dots now, so we can use that judge OFF times.
   * In the edge case where we have just one cycle, we must call it dot, not dash.
   * (so here it's strictly "<=onavg" for dots, not "<").
   */
  double dotsum=0.0;
  int dotc=0;
  for (cycle=cyclev,i=cyclec;i-->0;cycle++) {
    if (cycle->on<=onavg) {
      cycle->stroke='.';
      dotsum+=cycle->on;
      dotc++;
    } else {
      cycle->stroke='-';
    }
  }
  
  /* Populate (post) in cycles.
   * The spec says it's based on dot timing (1x, 3x, 7x) and that's easy to do.
   * But I think it would make more sense to examine just the OFF times and pick our two thresholds from those.
   * ...confirmed, I'm getting much better results this way.
   */
  cyclev[cyclec-1].off=0.0;
  if (cyclec>1) {
    // First take the average of off times.
    double offsum=0.0;
    for (cycle=cyclev,i=cyclec;i-->0;cycle++) offsum+=cycle->off;
    double offavg=offsum/cyclec;
    /* Then for each off time below that average but greater than zero,
     * find the sum of distances to the nearer of 1x, 3x, and 7x.
     * Whichever yields the smallest sum is the best beat time.
     */
    double beattime=0.0,beatsum=999.0;
    for (cycle=cyclev,i=cyclec;i-->0;cycle++) {
      if ((cycle->off>0.0)&&(cycle->off<offavg)) {
        double c1=cycle->off;
        double c3=c1*3.0;
        double c7=c1*7.0;
        double dsum=0.0;
        const struct cycle *other=cyclev;
        int otheri=cyclec;
        for (;otheri-->0;other++) {
          double o1=other->off-c1; if (o1<0.0) o1=-o1;
          double o3=other->off-c3; if (o3<0.0) o3=-o3;
          double o7=other->off-c7; if (o7<0.0) o7=-o7;
          if ((o1<=o3)&&(o1<=o7)) dsum+=o1;
          else if (o3<=o7) dsum+=o3;
          else dsum+=o7;
        }
        if (dsum<beatsum) {
          beattime=cycle->off;
          beatsum=dsum;
        }
      }
    }
    /* Now can can set thresholds at 2x and 5x of (beattime) and classify events accordingly.
     */
    double threshlo=beattime*2.0;
    double threshhi=beattime*5.0;
    for (cycle=cyclev,i=cyclec-1;i-->0;cycle++) {
      if (cycle->off<threshlo) cycle->post=0;
      else if (cycle->off<threshhi) cycle->post=1;
      else cycle->post=2;
    }
  }
  cyclev[cyclec-1].post=2; // The final stroke is a word break no matter what.
  
  /* Read cycles in batches, where each but the last member of each batch has (post==0).
   * ie each batch is one glyph.
   */
  for (cycle=cyclev,i=0;i<cyclec;) {
    int batchlen=1;
    while ((i+batchlen<cyclec)&&!cycle[batchlen-1].post) batchlen++;
    char ch=0;
    char glyph[8];
    if (batchlen<=sizeof(glyph)) {
      int j=0;
      for (;j<batchlen;j++) glyph[j]=cycle[j].stroke;
      for (j=0;j<26;j++) if (!memcmp(morse_letters[j],glyph,batchlen)&&!morse_letters[j][batchlen]) {
        ch='A'+j;
        break;
      }
    }
    if (!ch) ch='?';
    if (dstc<dsta) dst[dstc++]=ch;
    if (cycle[batchlen-1].post==2) { // word break.
      if (dstc<dsta) dst[dstc++]=' ';
    }
    cycle+=batchlen;
    i+=batchlen;
  }
  
  /* And we've probably emitted a trailing space, drop it.
   */
  if ((dstc>0)&&(dstc<=dsta)&&(dst[dstc-1]==' ')) dstc--;
  
  #undef cyclea
  return dstc;
}

/* Compare reference text (ref) to user input (q) and return a score.
 * Higher is better and both positive and negative are possible.
 */
 
static int morsecode_score(const char *ref,int refc,const char *q,int qc) {
  int score=0,refp=0,qp=0;
  for (;;) {
  
    /* If both strings exhausted, we're done.
     * Otherwise, penalize based on the count remaining on the longer string.
     */
    if (refp>=refc) {
      score-=5*(qc-qp); // Collapses to zero if matched.
      break;
    } else if (qp>=qc) {
      score-=5*(refc-refp);
      break;
    }
    
    /* If the next characters match, give a bonus and consume both.
     */
    if (ref[refp]==q[qp]) {
      score+=5;
      refp++;
      qp++;
      continue;
    }
    
    /* Discard an errant space in (q), and penalize only gently for it.
     * This happens when you wait 7 beats instead of 3 between letters, I'm doing it a lot (first time morse-coder here).
     */
    if ((q[qp]==' ')&&(qp<=qc-2)&&(q[qp+1]==ref[refp])) {
      score+=4;
      refp++;
      qp+=2;
      continue;
    }
    
    /* Here's where it gets tricky, we have a real mismatch.
     * If there's a match in the near future, say within 5 spaces in both strings, skip up to that match.
     * Seems the likelier mode of mismatch is excess space in (q), so let's weight (q) skips a bit lower than (ref) skips.
     * Better to skip more of (q) and less of (ref).
     */
    const int maxdiscard=5;
    int disrefc=99,disqc=99,disscore=99;
    int ri=0; for (;ri<maxdiscard;ri++) {
      if (refp+ri>=refc) break;
      int qi=0; for (;qi<maxdiscard;qi++) {
        if (qp+qi>=qc) break;
        if (ref[refp+ri]==q[qp+qi]) { // Got a match here.
          int ckscore=ri*2+qi;
          if (ckscore<disscore) {
            disrefc=ri;
            disqc=qi;
            disscore=ckscore;
            break;
          }
        }
      }
    }
    if (disscore<99) {
      refp+=disrefc;
      qp+=disqc;
      if (disrefc>disqc) score-=5*disrefc;
      else score-=5*disqc;
      continue;
    }
    
    /* Couldn't find a future match. Discard one from both strings and penalize.
     */
    refp++;
    qp++;
    score-=5;
  }
  return score;
}

/* Establish outcome.
 */
 
static void morsecode_finish(struct battle *battle) {
  
  /* First decode both signals and compare to the input message.
   * This yields a signed integer score with a max of N*5 and unlimited min.
   * Normalize against the input length.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  l->outputc=morsecode_decode(l->output,OUTPUT_LIMIT,l->signal,l->signalc);
  r->outputc=morsecode_decode(r->output,OUTPUT_LIMIT,r->signal,r->signalc);
  int llexi=morsecode_score(l->message,l->messagec,l->output,l->outputc);
  int rlexi=morsecode_score(r->message,r->messagec,r->output,r->outputc);
  double llex=(double)llexi/(double)l->messagec;
  double rlex=(double)rlexi/(double)r->messagec;
  
  /* Then a time bonus based on relative end times.
   * Because we read this off (signal), initial delay doesn't count. I think that's good?
   */
  double ltime=999.999,rtime=999.999;
  if (l->signalc>1) ltime=l->signal[l->signalc-1];
  if (r->signalc>1) rtime=r->signal[r->signalc-1];
  struct player *fast,*slow;
  double timebonus; // >=1
  if (ltime<rtime) {
    fast=l;
    slow=r;
    timebonus=rtime/ltime;
  } else {
    fast=r;
    slow=l;
    timebonus=ltime/rtime;
  }
  if (timebonus>3.0) timebonus=3.0;
  
  /* Aaaand I think that's it. Normalized lexical score times time bonus.
   */
  l->score=llex;
  r->score=rlex;
  fast->score*=timebonus;
   
  if (l->score>r->score) battle->outcome=1;
  else if (l->score<r->score) battle->outcome=-1;
  else battle->outcome=0;
}

/* Update.
 */
 
static void _morsecode_update(struct battle *battle,double elapsed) {

  /* Try to stop the "music" when the report begins.
   * We'll do it again at cleanup, to be on the safe side.
   */
  if (battle->outcome>-2) {
    if (BATTLE->songon) {
      egg_play_song(3,0,0,0.0,0.0);
      BATTLE->songon=0;
    }
    BATTLE->playerv[0].state=0;
    BATTLE->playerv[1].state=0;
    return;
  }
  
  /* panic_time is always running, and when it expires, we stop cold.
   */
  if ((BATTLE->panic_time-=elapsed)<=0.0) {
    morsecode_finish(battle);
    return;
  }
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Normal termination happens when somebody started, and then both are silent for a few seconds.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (BATTLE->started) {
    if (l->state||r->state) {
      BATTLE->silent_time=0.0;
    } else {
      BATTLE->silent_time+=elapsed;
      if (BATTLE->silent_time>=3.000) {
        morsecode_finish(battle);
      }
    }
  } else {
    if (l->state||r->state) {
      BATTLE->started=1;
    }
  }
}

/* Render player.
 */
 
static void player_render(
  struct battle *battle,struct player *player,
  int fullx,int fully,int fullw,int fullh,int align
) {

  /* Hero. Decorative.
   */
  const int xclearance=30;
  graf_set_image(&g.graf,RID_image_battle_tundra);
  // Desk, not flopped (contains text).
  if (align>0) {
    graf_tile(&g.graf,fullx+xclearance,fully+fullh-8,0x10,0);
    graf_tile(&g.graf,fullx+xclearance+16,fully+fullh-8,0x11,0);
  } else {
    graf_tile(&g.graf,fullx+fullw-xclearance,fully+fullh-8,0x11,0);
    graf_tile(&g.graf,fullx+fullw-xclearance-16,fully+fullh-8,0x10,0);
  }
  // Hero.
  uint8_t player_tileid=player->tileid;
  if (player->state) player_tileid+=2;
  if (align>0) {
    graf_tile(&g.graf,fullx+xclearance-16,fully+fullh-24,player_tileid,0);
    graf_tile(&g.graf,fullx+xclearance,fully+fullh-24,player_tileid+1,0);
    graf_tile(&g.graf,fullx+xclearance-16,fully+fullh-8,player_tileid+0x10,0);
    graf_tile(&g.graf,fullx+xclearance,fully+fullh-8,player_tileid+0x11,0);
  } else {
    graf_tile(&g.graf,fullx+fullw-xclearance,fully+fullh-24,player_tileid+1,EGG_XFORM_XREV);
    graf_tile(&g.graf,fullx+fullw-xclearance+16,fully+fullh-24,player_tileid,EGG_XFORM_XREV);
    graf_tile(&g.graf,fullx+fullw-xclearance,fully+fullh-8,player_tileid+0x11,EGG_XFORM_XREV);
    graf_tile(&g.graf,fullx+fullw-xclearance+16,fully+fullh-8,player_tileid+0x10,EGG_XFORM_XREV);
  }
  // Clicker.
  uint8_t clicker_tileid=player->state?0x01:0x00;
  if (align>0) {
    graf_tile(&g.graf,fullx+xclearance,fully+fullh-24,clicker_tileid,0);
  } else {
    graf_tile(&g.graf,fullx+fullw-xclearance,fully+fullh-24,clicker_tileid,EGG_XFORM_XREV);
  }
  // Pole and wires.
  int polex,poledx;
  uint8_t poletileid=0x0e;
  if (player->state) poletileid+=1;
  if (align>0) {
    polex=fullx+xclearance+32;
    poledx=16;
  } else {
    polex=fullx+fullw-xclearance-32;
    poledx=-16;
  }
  for (;(polex>=-8)&&(polex<FBW+8);polex+=poledx) {
    graf_tile(&g.graf,polex,fully+fullh-24,poletileid,0);
    graf_tile(&g.graf,polex,fully+fullh-8,poletileid+0x10,0);
  }

  /* Message high and left-aligned.
   * Always left aligned, regardless of the player's alignment.
   */
  int dstx=fullx+10;
  int dsty=fully+10;
  graf_set_image(&g.graf,RID_image_tinyfonttiles);
  graf_set_tint(&g.graf,0x000000ff);
  const char *src=player->message;
  int i=player->messagec;
  for (;i-->0;src++,dstx+=6) {
    if (*src<=0x20) continue;
    graf_tile(&g.graf,dstx,dsty,*src,0);
  }
  graf_set_tint(&g.graf,0);
  
  /* Signal below message.
   */
  graf_set_input(&g.graf,player->sigimg_texid);
  graf_decal(&g.graf,fullx+(fullw>>1)-(player->sigimg_w>>1),dsty+10,0,0,player->sigimg_w,player->sigimg_h);
  
  /* If game over, show the final telegram.
   */
  if (battle->outcome>-2) {
    int srcx=9*NS_sys_tilesize;
    int srcy=2*NS_sys_tilesize;
    int srcw=7*NS_sys_tilesize;
    int srch=3*NS_sys_tilesize;
    dstx=fullx+(fullw>>1)-(srcw>>1);
    dsty=fully=(fullh>>1)-(srch>>1);
    graf_set_image(&g.graf,RID_image_battle_tundra);
    graf_decal(&g.graf,dstx,dsty,srcx,srcy,srcw,srch);
    graf_set_image(&g.graf,RID_image_tinyfonttiles);
    graf_set_tint(&g.graf,0x111d45ff);
    int textxa=dstx+8;
    int textxz=dstx+srcw-8;
    int texty=dsty+15;
    int textx=textxa;
    for (src=player->output,i=player->outputc;i-->0;src++,textx+=6) {
      if (*src<=0x20) continue;
      if (textx>=textxz) {
        textx=textxa;
        texty+=6;
      }
      graf_tile(&g.graf,textx,texty,*src,0);
    }
    graf_set_tint(&g.graf,0xfff8f0ff);
    int nscore=(int)(player->score*100.0);
    if (nscore<-999) nscore=-999;
    else if (nscore>999) nscore=999;
    textx=textxa;
    texty=dsty+srch-5;
    if (nscore<0) {
      graf_tile(&g.graf,textx,texty,'-',0);
      textx+=6;
      nscore=-nscore;
    }
    graf_tile(&g.graf,textx,texty,'0'+nscore/100,0); textx+=8;
    graf_tile(&g.graf,textx,texty,'0'+(nscore/10)%10,0); textx+=8;
    graf_tile(&g.graf,textx,texty,'0'+nscore%10,0);
    graf_set_tint(&g.graf,0);
  }
}

/* Render.
 */
 
static void _morsecode_render(struct battle *battle) {
  const int GROUNDY=113;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  
  player_render(battle,BATTLE->playerv+0,0,0,FBW>>1,GROUNDY,-1);
  player_render(battle,BATTLE->playerv+1,FBW>>1,0,FBW>>1,GROUNDY,1);
  
  /* Static Morse Code key.
   */
  graf_set_image(&g.graf,RID_image_battle_tundra);
  int srcx=0,srcy=NS_sys_tilesize*2;
  int srcw=NS_sys_tilesize*9,srch=NS_sys_tilesize*4;
  graf_decal(&g.graf,(FBW>>1)-(srcw>>1),FBH-srch-1,srcx,srcy,srcw,srch);
}

/* Type definition.
 */
 
static const struct battle_input morsecode_input[]={
  {1,EGG_BTN_SOUTH},
  {1,0},
  {1,EGG_BTN_SOUTH},
  {1,0},
  {1,EGG_BTN_SOUTH},
  {3,0},
  {3,EGG_BTN_SOUTH},
  {1,0},
  {3,EGG_BTN_SOUTH},
  {1,0},
  {3,EGG_BTN_SOUTH},
  {3,0},
  {1,EGG_BTN_SOUTH},
  {1,0},
  {1,EGG_BTN_SOUTH},
  {1,0},
  {1,EGG_BTN_SOUTH},
  {3,0},
  {0},
};
 
const struct battle_type battle_type_morsecode={
  .name="morsecode",
  .objlen=sizeof(struct battle_morsecode),
  .id=NS_battle_morsecode,
  .strix_name=238,
  .no_article=0,
  .no_contest=0,
  .no_timeout=1, // We do our own hard timeout, but it's longer than the default.
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=morsecode_input,
  .del=_morsecode_del,
  .init=_morsecode_init,
  .update=_morsecode_update,
  .render=_morsecode_render,
};

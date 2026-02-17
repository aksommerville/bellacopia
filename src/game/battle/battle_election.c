/* battle_election.c
 * Stuff a ballot box against Mayor Cat, with help from your endorsers.
 */

#include "game/bellacopia.h"

#define PLAYER_LIMIT 8
#define MAYOR_PERIOD      0.500
#define ENDORSER_PERIOD   2.500
#define PERIOD_FUZZ       1.125 /* >1. */
#define ARM_TIME          0.100
#define BALLOT_LIMIT 32
#define BALLOT_TIME 0.500
#define BALLOT_HEIGHT 50.0
#define SPINNER_DT_MIN 3.000
#define SPINNER_DT_MAX 6.000
#define SPINNER_RANGE_MIN 0.500
#define SPINNER_RANGE_MAX 2.000
#define SCORE_LIMIT 100 /* Stop producing ballots when the combined score reaches this. Ones in flight will still count, so this is not the final range. */

struct battle_election {
  struct battle hdr;
  int choice;
  int scorev[2];
  
  /* Unlike most battles, our (playerv) includes others, the endorsers.
   */
  struct player {
    int who; // My index in this list.
    int party; // 0,1 = left,right
    int human; // 0 for CPU partisan, >0 for the input index, or <0 for CPU endorsers.
    double skill; // 0..1, reverse of each other.
    int dstx,dsty;
    uint8_t tileid;
    uint8_t xform;
    double periodlo,periodhi; // CPU only.
    double throwclock; // CPU only, counts down.
    int pvinput; // Human only.
    double armclock; // Counts down while showing arm in the air.
    double spinnert; // Radians. Only relevant for (human>=0).
    double spinnerdt; // Rad/sec. ''
    double spinnerrange; // Radians.
    double spinnerhighlight;
    uint32_t color;
    uint32_t highlight;
  } playerv[PLAYER_LIMIT];
  int playerc;
  
  struct ballot {
    double startx,starty;
    double ttl;
    int party;
  } ballotv[BALLOT_LIMIT];
  int ballotc;
};

#define BATTLE ((struct battle_election*)battle)

/* Delete.
 */
 
static void _election_del(struct battle *battle) {
}

/* Count members of a party.
 */
 
static int election_count_party(const struct battle *battle,int party) {
  const struct player *player=BATTLE->playerv;
  int i=BATTLE->playerc,c=0;
  for (;i-->0;player++) if (player->party==party) c++;
  return c;
}

/* Init player.
 */
 
static struct player *player_add(struct battle *battle,int human,int face,int party) {
  if (BATTLE->playerc>=PLAYER_LIMIT) return 0;
  struct player *player=BATTLE->playerv+BATTLE->playerc++;
  player->who=BATTLE->playerc-1;
  
  if (player->party=party) { // Right.
    player->skill=(double)battle->args.bias/255.0;
    player->dstx=(FBW*2)/3;
    player->dsty=100;
    player->xform=EGG_XFORM_XREV;
  } else { // Left.
    player->skill=1.0-(double)battle->args.bias/255.0;
    player->dstx=FBW/3;
    player->dsty=100;
  }
  
  player->human=human;
  if (player->human>0) { // Human.
    player->periodlo=ENDORSER_PERIOD; // Won't be used, just don't let it be zero.
  } else if (player->human<0) { // Endorser.
    player->periodlo=ENDORSER_PERIOD;
    switch (election_count_party(battle,party)) { // Me and the Principal both count, so the minimum is 2.
      case 2: player->dstx-=20; player->dsty-=10; break;
      case 3: player->dstx+=20; player->dsty-=10; break;
      case 4:                   player->dsty-=20; break;
      case 5: player->dstx-=20; player->dsty+=10; break;
      case 6: player->dstx+=20; player->dsty+=10; break;
      case 7:                   player->dsty+=20; break;
    }
  } else { // CPU player.
    player->periodlo=MAYOR_PERIOD;
  }
  player->periodhi=player->periodlo*PERIOD_FUZZ;
  player->periodlo/=PERIOD_FUZZ;
  player->throwclock=((rand()&0xffff)*player->periodhi)/65535.0; // Random phase.
  player->pvinput=EGG_BTN_SOUTH;
  player->spinnert=((rand()&0xffff)*M_PI*2.0)/65535.0; // Random phase.
  player->spinnerdt=SPINNER_DT_MIN*(1.0-player->skill)+SPINNER_DT_MAX*player->skill;
  player->spinnerrange=SPINNER_RANGE_MIN*player->skill+SPINNER_RANGE_MAX*(1.0-player->skill);
  
  player->color=0x808080ff;
  switch (face) {
    case NS_face_dot: player->tileid=0x00; player->color=0x411775ff; break;
    case NS_face_monster: // We don't have a Princess face, she doesn't play this game, so give her Mayor Cat instead.
    case NS_face_princess: player->tileid=0x20; player->color=0xc76f15ff; break;
    case NS_fld_endorse_food: player->tileid=0x40; break;
    case NS_fld_endorse_public: player->tileid=0x60; break;
    case NS_fld_endorse_athlete: player->tileid=0x80; break;
    case NS_fld_endorse_hospital: player->tileid=0xa0; break;
    case NS_fld_endorse_casino: player->tileid=0xc0; break;
    case NS_fld_endorse_labor: player->tileid=0xe0; break;
  }
  {
    uint8_t r=player->color>>24,g=player->color>>16,b=player->color>>8;
    r+=(0xff-r)>>1;
    g+=(0xff-g)>>1;
    b+=(0xff-b)>>1;
    player->highlight=(r<<24)|(g<<16)|(b<<8)|0xff;
  }
  
  return player;
}

/* Render order for players.
 */
 
static int election_player_cmp(const void *a,const void *b) {
  const struct player *A=a,*B=b;
  if (A->dsty<B->dsty) return -1;
  if (A->dsty>B->dsty) return 1;
  return 0; // Same row, don't care.
}

/* New.
 */
 
static int _election_init(struct battle *battle) {

  // Add the principals first.
  player_add(battle,battle->args.lctl,battle->args.lface,0);
  player_add(battle,battle->args.rctl,battle->args.rface,1);
  
  /* In the usual man-vs-cpu battle, endorsements come from global flags.
   */
  if ((battle->args.lctl==1)&&(battle->args.rctl==0)) {
    player_add(battle,-1,NS_fld_endorse_food    ,store_get_fld(NS_fld_endorse_food    )?0:1);
    player_add(battle,-1,NS_fld_endorse_public  ,store_get_fld(NS_fld_endorse_public  )?0:1);
    player_add(battle,-1,NS_fld_endorse_athlete ,store_get_fld(NS_fld_endorse_athlete )?0:1);
    player_add(battle,-1,NS_fld_endorse_hospital,store_get_fld(NS_fld_endorse_hospital)?0:1);
    player_add(battle,-1,NS_fld_endorse_casino  ,store_get_fld(NS_fld_endorse_casino  )?0:1);
    player_add(battle,-1,NS_fld_endorse_labor   ,store_get_fld(NS_fld_endorse_labor   )?0:1);
    
  /* Any other arrangement, apportion endorsers per bias.
   * The counts come from bias and the selections are random.
   */
  } else {
    // Don't overthink the order. Start with any order, then randomly swap pairs like a hundred times.
    #define ENDORSER_COUNT 6
    int facev[ENDORSER_COUNT]={
      NS_fld_endorse_food,
      NS_fld_endorse_public,
      NS_fld_endorse_athlete,
      NS_fld_endorse_hospital,
      NS_fld_endorse_casino,
      NS_fld_endorse_labor,
    };
    int randi=100;
    while (randi-->0) {
      int a=rand()%ENDORSER_COUNT;
      int b=rand()%ENDORSER_COUNT;
      if (a!=b) {
        int tmp=facev[a];
        facev[a]=facev[b];
        facev[b]=tmp;
      }
    }
    int rec=(battle->args.bias*(ENDORSER_COUNT+1))/255;
    if (rec<0) rec=0; else if (rec>ENDORSER_COUNT) rec=ENDORSER_COUNT;
    int lec=ENDORSER_COUNT-rec;
    int i;
    const int *face=facev;
    for (i=lec;i-->0;face++) player_add(battle,-1,*face,0);
    for (i=rec;i-->0;face++) player_add(battle,-1,*face,1);
  }
  
  /* Now sort (playerv) in render order.
   */
  qsort(BATTLE->playerv,BATTLE->playerc,sizeof(struct player),election_player_cmp);
  
  return 0;
}

/* Throw a ballot.
 */
 
static void player_throw(struct battle *battle,struct player *player) {

  // Stop when the score limit is reached.
  if (BATTLE->scorev[0]+BATTLE->scorev[1]>=SCORE_LIMIT) {
    return;
  }
  
  // Humans are subject to a spinner.
  if (player->human>0) {
    if ((player->spinnert<player->spinnerrange*-0.5)||(player->spinnert>player->spinnerrange*0.5)) {
      bm_sound(RID_sound_reject);
      return;
    }
    bm_sound_pan(RID_sound_throw,player->who?0.250:-0.250);
  }
  
  if (BATTLE->ballotc>=BALLOT_LIMIT) return;
  struct ballot *ballot=BATTLE->ballotv+BATTLE->ballotc++;
  
  ballot->ttl=BALLOT_TIME;
  ballot->party=player->party;
  ballot->startx=player->dstx;
  ballot->starty=player->dsty-8.0;
  
  if (player->xform) ballot->startx-=16.0;
  else ballot->startx+=16.0;

  player->armclock=ARM_TIME;
  player->spinnerhighlight=0.500;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (input!=player->pvinput) {
    if ((input&EGG_BTN_SOUTH)&&!(player->pvinput&EGG_BTN_SOUTH)) {
      player_throw(battle,player);
    }
    player->pvinput=input;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->throwclock-=elapsed)<=0.0) {
    //if ((player->spinnert>player->spinnerrange*-0.5)&&(player->spinnert<player->spinnerrange*0.5)) {
      player->throwclock=player->periodlo+((rand()&0xffff)*(player->periodhi-player->periodlo))/65535.0;
      player_throw(battle,player);
    //}
  }
}

/* Update endorser.
 */
 
static void player_update_endorser(struct battle *battle,struct player *player,double elapsed) {
  if ((player->throwclock-=elapsed)<=0.0) {
    player->throwclock=player->periodlo+((rand()&0xffff)*(player->periodhi-player->periodlo))/65535.0;
    player_throw(battle,player);
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->armclock>0.0) {
    player->armclock-=elapsed;
  }
  if (player->spinnerhighlight>0.0) {
    player->spinnerhighlight-=elapsed;
  }
  if (player->human>0) { // Humans and CPU players have a spinner. Endorsers don't.
    player->spinnert+=player->spinnerdt*elapsed;
    if (player->spinnert>M_PI) player->spinnert-=M_PI*2.0;
  }
}

/* Update one ballot.
 * Return >0 if still running or 0 to eliminate.
 */
 
static int ballot_update(struct battle *battle,struct ballot *ballot,double elapsed) {
  if ((ballot->ttl-=elapsed)<=0.0) {
    bm_sound(RID_sound_vote);
    if ((ballot->party>=0)&&(ballot->party<2)) {
      BATTLE->scorev[ballot->party]++;
    }
    return 0;
  }
  return 1;
}

/* Update.
 */
 
static void _election_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=BATTLE->playerc;
  for (;i-->0;player++) {
    if (player->human>0) player_update_man(battle,player,elapsed,g.input[player->human]);
    else if (player->human<0) player_update_endorser(battle,player,elapsed);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  struct ballot *ballot=BATTLE->ballotv+BATTLE->ballotc-1;
  for (i=BATTLE->ballotc;i-->0;ballot--) {
    if (!ballot_update(battle,ballot,elapsed)) {
      BATTLE->ballotc--;
      memmove(ballot,ballot+1,sizeof(struct ballot)*(BATTLE->ballotc-i));
    }
  }
  
  if (!BATTLE->ballotc&&(BATTLE->scorev[0]+BATTLE->scorev[1]>=SCORE_LIMIT)) {
    if (BATTLE->scorev[0]<BATTLE->scorev[1]) {
      battle->outcome=-1;
    } else if (BATTLE->scorev[0]>BATTLE->scorev[1]) {
      battle->outcome=1;
    } else {
      battle->outcome=0;
    }
  }
}

/* Render player or endorser.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int ht=NS_sys_tilesize>>1;
  int frame=0;
  if (player->armclock>0.0) frame=1;
  uint8_t tileidnw=player->tileid+frame*2;
  uint8_t tileidne,tileidsw,tileidse;
  if (player->xform) {
    tileidne=tileidnw;
    tileidnw=tileidne+1;
  } else {
    tileidne=tileidnw+1;
  }
  tileidsw=tileidnw+0x10;
  tileidse=tileidne+0x10;
  graf_tile(&g.graf,player->dstx-ht,player->dsty-ht,tileidnw,player->xform);
  graf_tile(&g.graf,player->dstx+ht,player->dsty-ht,tileidne,player->xform);
  graf_tile(&g.graf,player->dstx-ht,player->dsty+ht,tileidsw,player->xform);
  graf_tile(&g.graf,player->dstx+ht,player->dsty+ht,tileidse,player->xform);
}

/* Spinner, under certain players.
 */
 
static void spinner_render(struct battle *battle,struct player *player) {
  int x=player->dstx;
  int y=player->dsty+50;
  uint32_t bgtint=0;
  if (player->spinnerhighlight>0.0) {
    bgtint=(int)((player->spinnerhighlight*255.0)/0.500);
    if (bgtint>0xff) bgtint=0xff;
    bgtint|=0x00ff0000;
  }
  uint32_t bgprimary=player->color;
  uint32_t thumbprimary=0x808080ff;
  if ((player->spinnert>player->spinnerrange*-0.5)&&(player->spinnert<player->spinnerrange*0.5)) {
    bgprimary=player->highlight;
    thumbprimary=0xffff00ff;
  }
  graf_fancy(&g.graf,x,y,0x2a,0,0,NS_sys_tilesize,bgtint,bgprimary);
  graf_fancy(&g.graf,x,y,0x2b,0,(int8_t)((player->spinnert*128.0)/M_PI),NS_sys_tilesize,0,thumbprimary);
}

/* One flying ballot.
 */
 
static void ballot_render(struct battle *battle,struct ballot *ballot) {
  double t=ballot->ttl/BALLOT_TIME; // 0..1 = box..start
  if (t<0.0) t=0.0; else if (t>1.0) t=1.0;
  
  const double midx=FBW*0.5;
  double x=midx*(1.0-t)+ballot->startx*t;
  
  const double midy=FBH*0.5;
  const double peaky=midy-BALLOT_HEIGHT;
  double y;
  if (t>=0.5) { // peak..start
    double subt=(t-0.5)*2.0;
    y=peaky+(subt*subt)*(ballot->starty-peaky);
  } else { // box..peak
    double subt=1.0-t*2.0;
    y=peaky+(subt*subt)*(midy-peaky);
  }
  
  graf_tile(&g.graf,(int)x,(int)y,ballot->party?0x29:0x28,0);
}

/* Render.
 */
 
static void _election_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  
  /* Scoreboard.
   */
  int scorescale=BATTLE->scorev[0]+BATTLE->scorev[1];
  if (scorescale<SCORE_LIMIT) scorescale=SCORE_LIMIT;
  int scorew=(FBW*3)/4;
  int scoreh=30;
  int scorex=(FBW>>1)-(scorew>>1);
  int scorey=20;
  int lw=(BATTLE->scorev[0]*scorew)/scorescale;
  int rw=(BATTLE->scorev[1]*scorew)/scorescale;
  graf_fill_rect(&g.graf,scorex,scorey,scorew,scoreh,0x606060ff);
  graf_fill_rect(&g.graf,scorex,scorey,lw,scoreh,0x411775ff);
  graf_fill_rect(&g.graf,scorex+scorew-rw,scorey,rw,scoreh,0xc76f15ff);
  graf_fill_rect(&g.graf,FBW>>1,scorey,1,scoreh,0x80808080);
  
  // Players.
  graf_set_image(&g.graf,RID_image_election);
  struct player *player=BATTLE->playerv;
  int i=BATTLE->playerc;
  for (;i-->0;player++) {
    player_render(battle,player);
  }
  
  // Ballot box.
  graf_tile(&g.graf,(FBW>>1)-(NS_sys_tilesize>>1),(FBH>>1)-(NS_sys_tilesize>>1)+8,0x08,0);
  graf_tile(&g.graf,(FBW>>1)+(NS_sys_tilesize>>1),(FBH>>1)-(NS_sys_tilesize>>1)+8,0x09,0);
  graf_tile(&g.graf,(FBW>>1)-(NS_sys_tilesize>>1),(FBH>>1)+(NS_sys_tilesize>>1)+8,0x18,0);
  graf_tile(&g.graf,(FBW>>1)+(NS_sys_tilesize>>1),(FBH>>1)+(NS_sys_tilesize>>1)+8,0x19,0);
  
  // Ballots.
  struct ballot *ballot=BATTLE->ballotv;
  for (i=BATTLE->ballotc;i-->0;ballot++) {
    ballot_render(battle,ballot);
  }
  
  // Spinners for human and cpu.
  // Don't render these during the players pass because endorsers might occlude it, and also it would break the tile batch.
  for (player=BATTLE->playerv,i=BATTLE->playerc;i-->0;player++) {
    if (player->human<=0) continue;
    spinner_render(battle,player);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_election={
  .name="election",
  .objlen=sizeof(struct battle_election),
  .strix_name=112,
  .no_article=0,
  .no_contest=1,
  .support_pvp=1,
  .support_cvc=1,
  .del=_election_del,
  .init=_election_init,
  .update=_election_update,
  .render=_election_render,
};

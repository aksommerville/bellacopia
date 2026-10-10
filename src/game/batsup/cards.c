#include "game/bellacopia.h"
#include "cards.h"

/* Shuffle deck.
 */
 
void deck_shuffle(struct deck *deck) {
  uint8_t src[52];
  int i=0;
  for (;i<52;i++) src[i]=i;
  for (i=52;i-->0;) {
    int p=rand()%(i+1);
    deck->cardidv[i]=src[p];
    memmove(src+p,src+p+1,(i+1)-p);
  }
}

/* Draw one card.
 */

int deck_draw(struct deck *deck) {
  if ((deck->cardidp<0)||(deck->cardidp>=52)) return -1;
  return deck->cardidv[deck->cardidp++];
}

/* Count remaining cards in deck.
 */
 
int deck_remaining(const struct deck *deck) {
  return 52-deck->cardidp;
}

/* Render card.
 */

void card_render(int dstx,int dsty,uint8_t cardid) {
  graf_set_image(&g.graf,RID_image_battle_casino);
  if (cardid>=52) { // Backside, a simple decal.
    graf_decal(&g.graf,dstx,dsty,NS_sys_tilesize*13,NS_sys_tilesize*4,NS_sys_tilesize*3,NS_sys_tilesize*4);
  } else {
    int rank=RANK_FROM_CARDID(cardid);
    int suit=SUIT_FROM_CARDID(cardid);
    uint32_t color=COLOR_FROM_CARDID(cardid);
    graf_decal(&g.graf,dstx,dsty,NS_sys_tilesize*13,0,NS_sys_tilesize*3,NS_sys_tilesize*4);
    int x1=dstx+6;
    int y1=dsty+7;
    graf_fancy(&g.graf,x1,y1,0x00+rank,0,0,NS_sys_tilesize,color,0x808080ff);
    x1+=7;
    graf_fancy(&g.graf,x1,y1,0x10+suit,0,0,NS_sys_tilesize,color,0x808080ff);
    switch (rank) {
      /* Common ranks show the suit in a fixed pattern.
       * There are three columns always in the same places.
       * Seven rows -- the odd rows are spaced halfway. Don't mix odd and even rows.
       * Aside from Ace, this arrangement matches a very normal-looking Bicycle deck I had laying around.
       */
      #define _(col,row) { \
        int X=dstx+11+col*12; \
        int Y=dsty+20+row*5; \
        graf_fancy(&g.graf,X,Y,0x10+suit,0,0,NS_sys_tilesize,color,0x808080ff); \
      }
      case 0: _(1,3) break;
      case 1: _(1,0) _(1,6) break;
      case 2: _(1,0) _(1,3) _(1,6) break;
      case 3: _(0,0) _(2,0) _(0,6) _(2,6) break;
      case 4: _(0,0) _(2,0) _(0,6) _(2,6) _(1,3) break;
      case 5: _(0,0) _(2,0) _(0,6) _(2,6) _(0,3) _(2,3) break;
      case 6: _(0,0) _(2,0) _(0,6) _(2,6) _(0,3) _(2,3) _(1,1) break;
      case 7: _(0,0) _(2,0) _(0,6) _(2,6) _(0,3) _(2,3) _(1,1) _(1,5) break;
      case 8: _(0,0) _(0,2) _(0,4) _(0,6) _(2,0) _(2,2) _(2,4) _(2,6) _(1,3) break;
      case 9: _(0,0) _(0,2) _(0,4) _(0,6) _(2,0) _(2,2) _(2,4) _(2,6) _(1,1) _(1,5) break;
      #undef _
      // Face cards are a 3x3 decal, and don't have variations or color:
      case 10: graf_decal(&g.graf,dstx,dsty+NS_sys_tilesize,NS_sys_tilesize*4,NS_sys_tilesize,NS_sys_tilesize*3,NS_sys_tilesize*3); break;
      case 11: graf_decal(&g.graf,dstx,dsty+NS_sys_tilesize,NS_sys_tilesize*7,NS_sys_tilesize,NS_sys_tilesize*3,NS_sys_tilesize*3); break;
      case 12: graf_decal(&g.graf,dstx,dsty+NS_sys_tilesize,NS_sys_tilesize*10,NS_sys_tilesize,NS_sys_tilesize*3,NS_sys_tilesize*3); break;
    }
  }
}

/* Score a poker hand.
 */
 
static int suits_match(const uint8_t *suitv) {
  int i=5;
  while (i-->0) if (suitv[i]!=suitv[0]) return 0;
  return 1;
}

static int rankcmp(const void *a,const void *b) {
  const uint8_t *A=a,*B=b;
  return (*A)-(*B);
}

static int is_sequential(const uint8_t *v,int c) {
  int i=c;
  while (i-->0) if (v[i]!=v[0]+i) return 0;
  return 1;
}

// 0=no, 1=with a low ace, 2=with no ace, 3=with a high ace
static int ranks_ordered(const uint8_t *rankv) {
  uint8_t sorted[5];
  memcpy(sorted,rankv,5);
  qsort(sorted,5,1,rankcmp);
  if (is_sequential(sorted,5)) return sorted[0]?2:1;
  if (sorted[0]) return 0;
  if ((sorted[1]==9)&&is_sequential(sorted+1,4)) return 3;
  return 0;
}

static void poker_hand_pick_top(struct poker_hand *hand,const uint8_t *suitv,const uint8_t *rankv,int acehigh) {
  hand->toprank=hand->topsuit=0;
  int i=5;
  while (i-->0) {
    int effrank=rankv[i];
    if (acehigh&&!effrank) effrank=13;
    if ((effrank>hand->toprank)||((effrank==hand->toprank)&&(suitv[i]>hand->topsuit))) {
      hand->toprank=effrank;
      hand->topsuit=suitv[i];
    }
  }
}

static uint8_t poker_best_suit_of_rank(const uint8_t *rankv,const uint8_t *suitv,uint8_t rank) {
  uint8_t best=0;
  int i=5;
  while (i-->0) {
    if (rankv[i]!=rank) continue;
    if (suitv[i]>best) best=suitv[i];
  }
  return best;
}

static int poker_count_ranks(const uint8_t *rankv,uint8_t rank) {
  int c=0,i=5;
  while (i-->0) if (rankv[i]==rank) c++;
  return c;
}
 
void poker_hand_analyze(struct poker_hand *hand,const uint8_t *cardidv) {

  // Extract suit and rank to ease further analysis.
  uint8_t suitv[5],rankv[5];
  int i=5; while (i-->0) {
    suitv[i]=SUIT_FROM_CARDID(cardidv[i]);
    rankv[i]=RANK_FROM_CARDID(cardidv[i]);
  }
  int flush=suits_match(suitv);
  int straight=ranks_ordered(rankv);
  
  // Royal flush?
  if (flush&&(straight==3)) {
    hand->handid=HANDID_ROYALFLUSH;
    hand->toprank=13;
    hand->topsuit=suitv[0];
    return;
  }
  
  // Straight flush?
  if (flush&&straight) {
    hand->handid=HANDID_STRFLUSH;
    poker_hand_pick_top(hand,suitv,rankv,(straight==1)?0:1);
    return;
  }
  
  // Four of a kind?
  int c1=poker_count_ranks(rankv,rankv[0]);
  if (c1>=4) {
    hand->handid=HANDID_FOUR;
    hand->toprank=rankv[0];
    hand->topsuit=3;
    return;
  }
  if (poker_count_ranks(rankv,rankv[1])>=4) {
    hand->handid=HANDID_FOUR;
    hand->toprank=rankv[1];
    hand->topsuit=3;
    return;
  }
  
  // Full house?
  if (c1>=3) {
    for (i=5;i-->0;) {
      if ((rankv[i]!=rankv[0])&&(poker_count_ranks(rankv,rankv[i])>=2)) {
        hand->handid=HANDID_FULLHOUSE;
        hand->toprank=rankv[0];
        hand->topsuit=poker_best_suit_of_rank(rankv,suitv,rankv[0]);
        return;
      }
    }
  } else if (c1>=2) {
    for (i=5;i-->0;) {
      if ((rankv[i]!=rankv[0])&&(poker_count_ranks(rankv,rankv[i])>=3)) {
        hand->handid=HANDID_FULLHOUSE;
        hand->toprank=rankv[i];
        hand->topsuit=poker_best_suit_of_rank(rankv,suitv,rankv[i]);
        return;
      }
    }
  }
  
  // Flush?
  if (flush) {
    hand->handid=HANDID_FLUSH;
    poker_hand_pick_top(hand,suitv,rankv,1);
    return;
  }
  
  // Straight?
  if (straight) {
    hand->handid=HANDID_STRAIGHT;
    poker_hand_pick_top(hand,suitv,rankv,(straight==1)?0:1);
    return;
  }
  
  // Three of a kind?
  if (c1>=3) {
    hand->handid=HANDID_THREE;
    hand->toprank=rankv[0];
    hand->topsuit=poker_best_suit_of_rank(rankv,suitv,rankv[0]);
    return;
  }
  if (poker_count_ranks(rankv,rankv[1])>=3) {
    hand->handid=HANDID_THREE;
    hand->toprank=rankv[1];
    hand->topsuit=poker_best_suit_of_rank(rankv,suitv,rankv[1]);
    return;
  }
  if (poker_count_ranks(rankv,rankv[2])>=3) {
    hand->handid=HANDID_THREE;
    hand->toprank=rankv[2];
    hand->topsuit=poker_best_suit_of_rank(rankv,suitv,rankv[2]);
    return;
  }
  
  // Pair or two pair?
  int p1p=-1,p2p=-1;
  uint8_t p1rank=0xff,p2rank=0xff;
  for (i=5;i-->0;) {
    if ((rankv[i]==p1rank)||(rankv[i]==p2rank)) continue;
    if (poker_count_ranks(rankv,rankv[i])>=2) {
      if (p1p<0) {
        p1p=i;
        p1rank=rankv[i];
      } else {
        p2p=i;
        p2rank=rankv[i];
        break;
      }
    }
  }
  if (p2p>=0) {
    hand->handid=HANDID_TWOPAIR;
    if (p2rank>p1rank) hand->toprank=p2rank;
    else hand->toprank=p1rank;
    hand->topsuit=poker_best_suit_of_rank(rankv,suitv,hand->toprank);
    return;
  }
  if (p1p>=0) {
    hand->handid=HANDID_PAIR;
    hand->toprank=p1rank;
    hand->topsuit=poker_best_suit_of_rank(rankv,suitv,p1rank);
    return;
  }
  
  // Nothing fancy, just the high card.
  hand->handid=HANDID_CARD;
  poker_hand_pick_top(hand,suitv,rankv,(straight==1)?0:1);
}


/* Compare two poker hands.
 */
 
int poker_hand_compare(const uint8_t *a,const uint8_t *b) {
  struct poker_hand ahand,bhand;
  poker_hand_analyze(&ahand,a);
  poker_hand_analyze(&bhand,b);
  if (ahand.handid>bhand.handid) return -1;
  if (ahand.handid<bhand.handid) return 1;
  int arank=ahand.toprank; if (!arank) arank=13; // Aces always high for scoring purposes.
  int brank=bhand.toprank; if (!brank) brank=13;
  if (arank>brank) return -1;
  if (arank<brank) return 1;
  //TODO When tied at a pair or two pair, I think we're supposed to look at the rest of the hand before comparing suits.
  // For that matter, I'm not even certain that comparing suits is a real thing, or that we use the right order or them.
  // Is there such a thing as ties in Poker?
  if (ahand.topsuit>bhand.topsuit) return -1;
  if (ahand.topsuit<bhand.topsuit) return 1;
  return 0;
}

/* Short description of analyzed hand.
 */
 
int poker_hand_repr(char *dst,int dsta,const struct poker_hand *hand) {
  
  /* Hands above pair, I think we can just print the static name, eg "Two pair", no need to say which ranks.
   */
  if (hand->handid>HANDID_PAIR) {
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_battle,363+hand->handid);
    if ((srcc>0)&&(srcc<=dsta)) memcpy(dst,src,srcc);
    return srcc;
  }
  
  /* "Pair of eights". We perform the pluralization right here.
   */
  int rankid=hand->toprank;
  if (rankid>=13) rankid=0; // High aces are still just "aces" for naming purposes.
  if (hand->handid==HANDID_PAIR) {
    const char *tmpl=0;
    int tmplc=text_get_string(&tmpl,RID_strings_battle,373);
    const char *rankstem=0;
    int rankstemc=text_get_string(&rankstem,RID_strings_battle,204+rankid);
    char rank[16];
    int rankc=0;
    if (rankid==5) { // "Six" is the only one that pluralizes different.
      if (rankstemc<=sizeof(rank)-2) {
        memcpy(rank,rankstem,rankstemc);
        rank[rankstemc]='e';
        rank[rankstemc+1]='s';
        rankc=rankstemc+2;
      }
    } else { // Other ranks append one "s".
      if (rankstemc<=sizeof(rank)-1) {
        memcpy(rank,rankstem,rankstemc);
        rank[rankstemc]='s';
        rankc=rankstemc+1;
      }
    }
    struct text_insertion ins={.mode='s',.s={.v=rank,.c=rankc}};
    return text_format(dst,dsta,tmpl,tmplc,&ins,1);
  }
  
  /* "King of Hearts". Suits are pre-pluralized.
   */
  const char *tmpl=0;
  int tmplc=text_get_string(&tmpl,RID_strings_battle,374);
  struct text_insertion insv[]={
    {.mode='r',.r={.rid=RID_strings_battle,204+rankid}},
    {.mode='r',.r={.rid=RID_strings_battle,200+hand->topsuit}},
  };
  return text_format(dst,dsta,tmpl,tmplc,insv,2);
}

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

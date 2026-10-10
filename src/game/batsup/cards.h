/* cards.h
 * Helpers for battles using a 52-card poker deck: slapping, poker, blackjack.
 *
 * Suit names start at strings:battle:200.
 * Rank names start at strings:battle:204. (Ace is low and is called "Ace")
 * Poker hand names start at strings:battle:363.
 */
 
#ifndef CARDS_H
#define CARDS_H

/* Rank in 0..12 and suit in 0..3.
 * Suits 0,1 are red and 2,3 black.
 * The tiles are arranged so.
 * cardid zero is a valid card; it's the Ace of Hearts.
 */
#define RANK_FROM_CARDID(cardid) ((cardid)>>2)
#define SUIT_FROM_CARDID(cardid) ((cardid)&3)
#define COLOR_FROM_CARDID(cardid) (((cardid)&2)?0x000000ff:0xc00010ff)

struct deck {
  uint8_t cardidv[52];
  int cardidp;
};

/* Populates (cardidv) with a random order of all 52 cards and sets (cardidp) zero.
 */
void deck_shuffle(struct deck *deck);

/* Drawing returns the next card or <0 if exhausted.
 */
int deck_draw(struct deck *deck);
int deck_remaining(const struct deck *deck);

/* (dstx,dsty) is the top left corner.
 * Covers 3x4 tiles.
 * If (cardid>=52), renders the back side.
 */
void card_render(int dstx,int dsty,uint8_t cardid);

#define HANDID_CARD         0
#define HANDID_PAIR         1
#define HANDID_TWOPAIR      2
#define HANDID_THREE        3
#define HANDID_STRAIGHT     4
#define HANDID_FLUSH        5
#define HANDID_FULLHOUSE    6
#define HANDID_FOUR         7
#define HANDID_STRFLUSH     8
#define HANDID_ROYALFLUSH   9

struct poker_hand {
  int handid;
  int toprank; // 13 for an Ace playing high.
  int topsuit;
};
void poker_hand_analyze(struct poker_hand *hand,const uint8_t *cardidv/*5*/);
int poker_hand_compare(const uint8_t *a,const uint8_t *b);

int poker_hand_repr(char *dst,int dsta,const struct poker_hand *hand);

#endif

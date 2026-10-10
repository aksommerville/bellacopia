/* cards.h
 * Helpers for battles using a 52-card poker deck: slapping, poker, blackjack
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

#endif

#ifndef CHESS_INTERNAL_H
#define CHESS_INTERNAL_H

#include "game/bellacopia.h"
#include "chess.h"

struct chesspiece {
  uint8_t piece,x,y;
};

int chesspiecev_search(struct chesspiece *v,int c,uint8_t piece);
struct chesspiece *chesspiece_get(struct chesspiece *v,int c,uint8_t piece);

#endif

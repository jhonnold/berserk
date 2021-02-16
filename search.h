#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

int Search(Board* board, Move* bestMove);
int negamax(int alpha, int beta, int depth, int ply, Board* board);
int quiesce(int alpha, int beta, Board* board);

#endif
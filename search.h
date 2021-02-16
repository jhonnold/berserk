#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

extern const int CHECKMATE;
extern const int MATE_BOUND;

void Search(Board* board, SearchParams* params);
int negamax(int alpha, int beta, int depth, int ply, Board* board, SearchParams* params);
int quiesce(int alpha, int beta, Board* board, SearchParams* params);

#endif
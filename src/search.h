#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

extern const int CHECKMATE;
extern const int MATE_BOUND;

void Search(Board* board, SearchParams* params, SearchData* data);
int negamax(int alpha, int beta, int depth, int ply, int canNull, Board* board, SearchParams* params, SearchData* data);
int quiesce(int alpha, int beta, int ply, Board* board, SearchParams* params, SearchData* data);

#endif
#ifndef PERFT_H
#define PERFT_H

#include "types.h"

int64_t perft(int depth, SearchData* data);
void PerftTest(int depth, Board* board);

#endif
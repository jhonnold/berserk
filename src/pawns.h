#ifndef PAWNS_H
#define PAWNS_H

#include "types.h"

PawnHashEntry* TTPawnProbe(uint64_t hash, ThreadData* thread);
void TTPawnPut(uint64_t hash, Score s, BitBoard passedPawns, ThreadData* thread);

Score PawnEval(Board* board, EvalData* data, int side);
Score PasserEval(Board* board, EvalData* data, int side);

#endif
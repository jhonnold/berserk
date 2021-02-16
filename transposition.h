#ifndef TRANSPOSITION_H
#define TRANSPOSITION_H

#include <inttypes.h>

#include "types.h"

extern const int TT_EXACT;
extern const int TT_LOWER;
extern const int TT_UPPER;

int ttFlag(TTValue value);
Move ttMove(TTValue value);
int ttDepth(TTValue value);
int ttScore(TTValue value, int ply);
TTValue ttProbe(uint64_t hash);
void ttPut(uint64_t hash, int depth, int score, int flag, Move move);
void ttClear();

#endif
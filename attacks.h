#ifndef ATTACKS_H
#define ATTACKS_H

#include "types.h"

void initAttacks();

bb_t shift(bb_t bb, int dir);

bb_t getInBetween(int from, int to);
bb_t getPinnedMoves(int p, int k);

bb_t getPawnAttacks(int sq, int color);
bb_t getKnightAttacks(int sq);
bb_t getBishopAttacks(int sq, bb_t occupancy);
bb_t getRookAttacks(int sq, bb_t occupancy);
bb_t getQueenAttacks(int sq, bb_t occupancy);
bb_t getKingAttacks(int sq);

#endif

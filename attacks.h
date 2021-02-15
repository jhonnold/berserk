#ifndef ATTACKS_H
#define ATTACKS_H

#include "types.h"

void initAttacks();

BitBoard shift(BitBoard bb, int dir);

BitBoard getInBetween(int from, int to);
BitBoard getPinnedMoves(int p, int k);

BitBoard getPawnAttacks(int sq, int color);
BitBoard getKnightAttacks(int sq);
BitBoard getBishopAttacks(int sq, BitBoard occupancy);
BitBoard getRookAttacks(int sq, BitBoard occupancy);
BitBoard getQueenAttacks(int sq, BitBoard occupancy);
BitBoard getKingAttacks(int sq);

#endif

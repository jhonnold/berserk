#ifndef ATTACKS_H
#define ATTACKS_H

#include "types.h"

extern const BitBoard sides[];
extern const BitBoard aFile;
extern const BitBoard columnMasks[];
extern const BitBoard centerFour;

void initAttacks();
void initPawnSpans();

BitBoard shift(BitBoard bb, int dir);
BitBoard fill(BitBoard initial, int direction);

BitBoard getInBetween(int from, int to);
BitBoard getPinnedMoves(int p, int k);
BitBoard getPawnSpans(BitBoard pawns, int side);
BitBoard getPawnSpan(int sq, int side);

BitBoard getPawnAttacks(int sq, int color);
BitBoard getKnightAttacks(int sq);
BitBoard getBishopAttacks(int sq, BitBoard occupancy);
BitBoard getRookAttacks(int sq, BitBoard occupancy);
BitBoard getQueenAttacks(int sq, BitBoard occupancy);
BitBoard getKingAttacks(int sq);

#endif

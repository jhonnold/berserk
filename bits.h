#ifndef BITS_H
#define BITS_H

#include "types.h"

#define setBit(bb, sq) (bb |= 1UL << (sq))
#define getBit(bb, sq) (bb & (1UL << (sq)))
#define popBit(bb, sq) (bb &= ~(1UL << (sq)))
#define popLsb(bb) (bb &= bb - 1)
#define bits(bb) __builtin_popcountll(bb)
#define lsb(bb) (__builtin_ctzll(bb))

void printBB(bb_t bb);

#endif
#ifndef BITS_H
#define BITS_H

#include "types.h"

#define setBit(bb, sq) (bb |= 1ULL << (sq))
#define getBit(bb, sq) (bb & (1ULL << (sq)))
#define popBit(bb, sq) (bb &= ~(1ULL << (sq)))
#define popLsb(bb) (bb &= bb - 1)
#define lsb(bb) (__builtin_ctzll(bb))
#define msb(bb) (63 ^ __builtin_clzll(bb))

#ifndef POPCOUNT
inline int bits(BitBoard bb) {
  int c;
  for (c = 0; bb; bb &= bb - 1)
    c++;
  return c;
}
#else
#define bits(bb) (__builtin_popcountll(bb))
#endif

void printBB(BitBoard bb);

#endif
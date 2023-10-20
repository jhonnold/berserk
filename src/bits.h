// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2023 Jay Honnold

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef BITS_H
#define BITS_H

#include <stdio.h>

#include "types.h"
#include "util.h"

#define ALL -1

#define A_FILE 0x0101010101010101ULL
#define B_FILE 0x0202020202020202ULL
#define C_FILE 0x0404040404040404ULL
#define D_FILE 0x0808080808080808ULL
#define E_FILE 0x1010101010101010ULL
#define F_FILE 0x2020202020202020ULL
#define G_FILE 0x4040404040404040ULL
#define H_FILE 0x8080808080808080ULL

#define RANK_1 0xFF00000000000000ULL
#define RANK_2 0x00FF000000000000ULL
#define RANK_3 0x0000FF0000000000ULL
#define RANK_4 0x000000FF00000000ULL
#define RANK_5 0x00000000FF000000ULL
#define RANK_6 0x0000000000FF0000ULL
#define RANK_7 0x000000000000FF00ULL
#define RANK_8 0x00000000000000FFULL

#define DARK_SQS 0x55AA55AA55AA55AAULL

#define Bit(sq)                (1ULL << (sq))
#define BitCount(bb)           (__builtin_popcountll(bb))
#define SetBit(bb, sq)         ((bb) |= Bit(sq))
#define GetBit(bb, sq)         ((bb) &Bit(sq))
#define PopBit(bb, sq)         ((bb) &= ~Bit(sq))
#define FlipBit(bb, sq)        ((bb) ^= Bit(sq))
#define FlipBits(bb, sq1, sq2) ((bb) ^= Bit(sq1) ^ Bit(sq2))
#define LSB(bb)                (__builtin_ctzll(bb))
#define MSB(bb)                (63 ^ __builtin_clzll(bb))

#define ShiftN(bb)  ((bb) >> 8)
#define ShiftS(bb)  ((bb) << 8)
#define ShiftNN(bb) ((bb) >> 16)
#define ShiftSS(bb) ((bb) << 16)
#define ShiftW(bb)  (((bb) & ~A_FILE) >> 1)
#define ShiftE(bb)  (((bb) & ~H_FILE) << 1)
#define ShiftNE(bb) (((bb) & ~H_FILE) >> 7)
#define ShiftSW(bb) (((bb) & ~A_FILE) << 7)
#define ShiftNW(bb) (((bb) & ~A_FILE) >> 9)
#define ShiftSE(bb) (((bb) & ~H_FILE) << 9)

INLINE BitBoard ShiftPawnDir(BitBoard bb, const int c) {
  return c == WHITE ? ShiftN(bb) : ShiftS(bb);
}

INLINE BitBoard ShiftPawnCapW(BitBoard bb, const int c) {
  return c == WHITE ? ShiftNW(bb) : ShiftSW(bb);
}

INLINE BitBoard ShiftPawnCapE(BitBoard bb, const int c) {
  return c == WHITE ? ShiftNE(bb) : ShiftSE(bb);
}

INLINE int PopLSB(BitBoard* bb) {
  int sq = LSB(*bb);
  *bb &= *bb - 1;
  return sq;
}

INLINE void PrintBB(BitBoard bitboard) {
  for (int i = 0; i < 64; i++) {
    if ((i & 7) == 0)
      printf(" %d ", 8 - (i >> 3));

    printf(" %d", GetBit(bitboard, i) ? 1 : 0);

    if ((i & 7) == 7)
      printf("\n");
  }

  printf("\n    a b c d e f g h\n\n");
  printf(" Value: %" PRIu64 "\n\n", bitboard);
}

#endif
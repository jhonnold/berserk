// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

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

#include "types.h"

extern const BitBoard FILE_MASKS[8];
extern const BitBoard RANK_MASKS[8];
extern const BitBoard ADJACENT_FILE_MASKS[8];
extern const BitBoard FORWARD_RANK_MASKS[2][8];
extern const BitBoard BOARD_SIDE[8];
extern const BitBoard MY_SIDE[2];

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

#define bit(sq) (1ULL << (sq))
#define setBit(bb, sq) (bb |= bit(sq))
#define getBit(bb, sq) (bb & bit(sq))
#define popBit(bb, sq) (bb &= ~bit(sq))
#define popLsb(bb) (bb &= bb - 1)
#define lsb(bb) (__builtin_ctzll(bb))
#define msb(bb) (63 ^ __builtin_clzll(bb))

#ifndef POPCOUNT
int bits(BitBoard bb);
#else
#define bits(bb) (__builtin_popcountll(bb))
#endif

BitBoard shift(BitBoard bb, int dir);
BitBoard fill(BitBoard initial, int direction);
void printBB(BitBoard bb);

#endif
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

#include <inttypes.h>
#include <stdio.h>

#include "bits.h"
#include "board.h"
#include "types.h"

const BitBoard FILE_MASKS[8] = {A_FILE, B_FILE, C_FILE, D_FILE, E_FILE, F_FILE, G_FILE, H_FILE};
const BitBoard RANK_MASKS[8] = {RANK_8, RANK_7, RANK_6, RANK_5, RANK_4, RANK_3, RANK_2, RANK_1};
const BitBoard ADJACENT_FILE_MASKS[8] = {B_FILE,          A_FILE | C_FILE, B_FILE | D_FILE, C_FILE | E_FILE,
                                         D_FILE | F_FILE, E_FILE | G_FILE, F_FILE | H_FILE, G_FILE};
const BitBoard BOARD_SIDE[8] = {A_FILE | B_FILE | C_FILE,          A_FILE | B_FILE | C_FILE | D_FILE,
                                A_FILE | B_FILE | C_FILE | D_FILE, C_FILE | D_FILE | E_FILE | F_FILE,
                                C_FILE | D_FILE | E_FILE | F_FILE, E_FILE | F_FILE | G_FILE | H_FILE,
                                E_FILE | F_FILE | G_FILE | H_FILE, F_FILE | G_FILE | H_FILE};
const BitBoard MY_SIDE[2] = {RANK_1 | RANK_2 | RANK_3 | RANK_4, RANK_5 | RANK_6 | RANK_7 | RANK_8};

const BitBoard FORWARD_RANK_MASKS[2][8] = {{
                                               0ULL,
                                               RANK_8,
                                               RANK_8 | RANK_7,
                                               RANK_8 | RANK_7 | RANK_6,
                                               RANK_8 | RANK_7 | RANK_6 | RANK_5,
                                               RANK_8 | RANK_7 | RANK_6 | RANK_5 | RANK_4,
                                               RANK_8 | RANK_7 | RANK_6 | RANK_5 | RANK_4 | RANK_3,
                                               RANK_8 | RANK_7 | RANK_6 | RANK_5 | RANK_4 | RANK_3 | RANK_2,
                                           },
                                           {
                                               RANK_1 | RANK_2 | RANK_3 | RANK_4 | RANK_5 | RANK_6 | RANK_7,
                                               RANK_1 | RANK_2 | RANK_3 | RANK_4 | RANK_5 | RANK_6,
                                               RANK_1 | RANK_2 | RANK_3 | RANK_4 | RANK_5,
                                               RANK_1 | RANK_2 | RANK_3 | RANK_4,
                                               RANK_1 | RANK_2 | RANK_3,
                                               RANK_1 | RANK_2,
                                               RANK_1,
                                               0ULL,
                                           }};

#ifndef POPCOUNT
inline int bits(BitBoard bb) {
  int c;
  for (c = 0; bb; bb &= bb - 1)
    c++;
  return c;
}
#endif

inline BitBoard shift(BitBoard bb, int dir) {
  switch (dir) {
  case N:
    return bb >> 8;
  case S:
    return bb << 8;
  case (N + N):
    return bb >> 16;
  case (S + S):
    return bb << 16;
  case W:
    return (bb & ~A_FILE) >> 1;
  case E:
    return (bb & ~H_FILE) << 1;
  case NE:
    return (bb & ~H_FILE) >> 7;
  case SW:
    return (bb & ~A_FILE) << 7;
  case NW:
    return (bb & ~A_FILE) >> 9;
  case SE:
    return (bb & ~H_FILE) << 9;
  default:
    return 0;
  }
}

inline BitBoard fill(BitBoard initial, int direction) {
  switch (direction) {
  case S:
    initial |= (initial << 8);
    initial |= (initial << 16);
    return initial | (initial << 32);
  case N:
    initial |= (initial >> 8);
    initial |= (initial >> 16);
    return initial | (initial >> 32);
  default:
    return initial;
  }
}

void printBB(BitBoard bitboard) {
  for (int i = 0; i < 64; i++) {
    if (file(i) == 0)
      printf(" %d ", 8 - rank(i));

    printf(" %d", getBit(bitboard, i) ? 1 : 0);

    if (file(i) == 7)
      printf("\n");
  }

  printf("\n    a b c d e f g h\n\n");
  printf(" Value: %" PRIu64 "\n\n", bitboard);
}
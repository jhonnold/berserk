// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

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

#include "bits.h"

// clang-format off
const BitBoard FILE_MASKS[8] = {A_FILE, B_FILE, C_FILE, D_FILE, E_FILE, F_FILE, G_FILE, H_FILE};
const BitBoard RANK_MASKS[8] = {RANK_8, RANK_7, RANK_6, RANK_5, RANK_4, RANK_3, RANK_2, RANK_1};
const BitBoard ADJACENT_FILE_MASKS[8] = {B_FILE,          A_FILE | C_FILE, B_FILE | D_FILE, C_FILE | E_FILE,
                                         D_FILE | F_FILE, E_FILE | G_FILE, F_FILE | H_FILE, G_FILE};

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

const BitBoard CENTER_SQS = (D_FILE | E_FILE) & (RANK_4 | RANK_5);
// clang-format on

inline BitBoard Fill(BitBoard initial, int direction) {
  switch (direction) {
  case S:
    initial |= (initial << 8);
    initial |= (initial << 16);
    return initial | (initial << 32);
  case N:
    initial |= (initial >> 8);
    initial |= (initial >> 16);
    return initial | (initial >> 32);
  default: return initial;
  }
}

inline BitBoard FileFill(BitBoard initial) {
  return Fill(initial, N) | Fill(initial, S);
}

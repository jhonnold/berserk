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

#include "zobrist.h"
#include "random.h"

uint64_t ZOBRIST_PIECES[12][64];
uint64_t ZOBRIST_EP_KEYS[64];
uint64_t ZOBRIST_CASTLE_KEYS[16];
uint64_t ZOBRIST_SIDE_KEY;

void initZobristKeys() {
  for (int i = 0; i < 12; i++)
    for (int j = 0; j < 64; j++)
      ZOBRIST_PIECES[i][j] = randomLong();

  for (int i = 0; i < 64; i++)
    ZOBRIST_EP_KEYS[i] = randomLong();

  for (int i = 0; i < 16; i++)
    ZOBRIST_CASTLE_KEYS[i] = randomLong();

  ZOBRIST_SIDE_KEY = randomLong();
}
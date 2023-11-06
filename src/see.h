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

#ifndef SEE_H
#define SEE_H

#include "types.h"
#include "util.h"

extern const int SEE_VALUE[7];
extern const int NET_VALUE[7];

int SEEV(Board* board, Move move, int threshold, const int* values);

INLINE int SEE(Board* board, Move move, int threshold) {
  return SEEV(board, move, threshold, SEE_VALUE);
}

INLINE int SEEReal(Board* board, Move move, int threshold) {
  return SEEV(board, move, threshold, NET_VALUE);
}

#endif
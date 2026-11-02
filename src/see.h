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

#ifndef SEE_H
#define SEE_H

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "move.h"
#include "types.h"
#include "util.h"

extern const int SEE_VALUE[7];

// Static exchange evaluation using The Swap Algorithm -
// https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
int SEE(Board* board, Move move, int threshold);

#endif
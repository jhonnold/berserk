// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2026 Jay Honnold

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

#ifdef SPSA
extern int SEE_VALUE[7];
void InitSeeValues(void);
#else
extern const int SEE_VALUE[7];
#endif

// Static exchange evaluation using The Swap Algorithm -
// https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
Bool SEE(Board* board, Move move, Score threshold);

#endif
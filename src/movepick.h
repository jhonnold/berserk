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

#ifndef MOVEPICK_H
#define MOVEPICK_H

#include "types.h"

void InitAllMoves(MoveList* moves, Move hashMove, SearchData* data);
void InitTacticalMoves(MoveList* moves, SearchData* data, int cutoff);
void InitPerftMoves(MoveList* moves, Board* board);
Move NextMove(MoveList* moves, Board* board, int skipQuiets);

void PrintMoves(Board* board, ThreadData* thread);

#endif
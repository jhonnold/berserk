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

#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"

extern const int MVV_LVA[12][12];

extern const BitBoard PROMOTION_RANKS[];
extern const BitBoard HOME_RANKS[];
extern const BitBoard THIRD_RANKS[];
extern const int PAWN_DIRECTIONS[];

extern const int HASH_MOVE_SCORE;
extern const int GOOD_CAPTURE_SCORE;
extern const int BAD_CAPTURE_SCORE;
extern const int KILLER1_SCORE;
extern const int KILLER2_SCORE;
extern const int COUNTER_SCORE;

void AppendMove(Move* arr, uint8_t* n, Move move);
void GenerateAllMoves(MoveList* moveList, Board* board);
void GenerateQuietMoves(MoveList* moveList, Board* board);
void GenerateTacticalMoves(MoveList* moveList, Board* board);

#endif

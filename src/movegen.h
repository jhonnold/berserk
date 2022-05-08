// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2022 Jay Honnold

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

#define PawnDir(c) ((c) == WHITE ? N : S)
#define PromoRank(c) ((c) == WHITE ? RANK_7 : RANK_2)
#define DPRank(c) ((c) == WHITE ? RANK_3 : RANK_6)

#define ALL -1ULL

#define NO_PROMO 0
#define QUIET 0b0000
#define CAPTURE 0b0001
#define EP 0b0101
#define DP 0b0010
#define CASTLE 0b1000

#define WHITE_KS 0x8
#define WHITE_QS 0x4
#define BLACK_KS 0x2
#define BLACK_QS 0x1

#define CanCastle(dir) (board->castling & (dir))

void GenerateQuietMoves(MoveList* moveList, Board* board);
void GenerateTacticalMoves(MoveList* moveList, Board* board);
void GenerateAllMoves(MoveList* moveList, Board* board);

#endif

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

#ifndef MOVE_H
#define MOVE_H

#include "types.h"

#define NULL_MOVE 0

extern const int CHAR_TO_PIECE[];
extern const char* PIECE_TO_CHAR;
extern const char* PROMOTION_TO_CHAR;
extern const char* SQ_TO_COORD[64];
extern const int CASTLE_ROOK_DEST[64];
extern const int CASTLING_ROOK[64];

#define BuildMove(from, to, piece, promo, flags) \
  (from) | ((to) << 6) | ((piece) << 12) | ((promo) << 16) | ((flags) << 20)
#define From(move)   ((int) (move) &0x3f)
#define To(move)     (((int) (move) &0xfc0) >> 6)
#define Moving(move) (((int) (move) &0xf000) >> 12)
#define Promo(move)  (((int) (move) &0xf0000) >> 16)
#define IsCap(move)  (((int) (move) &0x100000) >> 20)
#define IsEP(move)   (((int) (move) &0x400000) >> 22)
#define IsCas(move)  (((int) (move) &0x800000) >> 23)
// just mask the from/to bits into a single int for indexing butterfly tables
#define FromTo(move) ((int) (move) &0xfff)

#define IsTactical(move) (((int) (move) &0x1f0000) >> 16)

Move ParseMove(char* moveStr, Board* board);
char* MoveToStr(Move move, Board* board);
int IsRecapture(SearchStack* ss, Move move);

#endif

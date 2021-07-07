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

#ifndef MOVE_H
#define MOVE_H

#include "types.h"

#define NULL_MOVE 0

extern const int CHAR_TO_PIECE[];
extern const char* PIECE_TO_CHAR;
extern const char* PROMOTION_TO_CHAR;
extern const char* SQ_TO_COORD[];

#define BuildMove(start, end, piece, promo, cap, dub, ep, castle)                                                      \
  (start) | ((end) << 6) | ((piece) << 12) | ((promo) << 16) | ((cap) << 20) | ((dub) << 21) | ((ep) << 22) |          \
      ((castle) << 23)
#define MoveStart(move) ((int)(move)&0x3f)
#define MoveEnd(move) (((int)(move)&0xfc0) >> 6)
#define MovePiece(move) (((int)(move)&0xf000) >> 12)
#define MovePromo(move) (((int)(move)&0xf0000) >> 16)
#define MoveCapture(move) (((int)(move)&0x100000) >> 20)
#define MoveDoublePush(move) (((int)(move)&0x200000) >> 21)
#define MoveEP(move) (((int)(move)&0x400000) >> 22)
#define MoveCastle(move) (((int)(move)&0x800000) >> 23)
// just mask the start/end bits into a single int for indexing butterfly tables
#define MoveStartEnd(move) ((int)(move)&0xfff)

#define Tactical(move) (((int)(move)&0x1f0000) >> 16)

Move ParseMove(char* moveStr, Board* board);
char* MoveToStr(Move move);
int IsRecapture(SearchData* data, Move move);

#endif
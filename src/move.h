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

#define buildMove(start, end, piece, promo, cap, dub, ep, castle)                                                      \
  (start) | ((end) << 6) | ((piece) << 12) | ((promo) << 16) | ((cap) << 20) | ((dub) << 21) | ((ep) << 22) |          \
      ((castle) << 23)
#define moveStart(move) ((move)&0x3f)
#define moveEnd(move) (((move)&0xfc0) >> 6)
#define movePiece(move) (((move)&0xf000) >> 12)
#define movePromo(move) (((move)&0xf0000) >> 16)
#define moveCapture(move) (((move)&0x100000) >> 20)
#define moveDouble(move) (((move)&0x200000) >> 21)
#define moveEP(move) (((move)&0x400000) >> 22)
#define moveCastle(move) (((move)&0x800000) >> 23)
#define moveSE(move) ((move)&0xfff)

Move parseMove(char* moveStr, Board* board);
char* moveStr(Move move);

#endif
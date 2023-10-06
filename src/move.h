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

#ifndef MOVE_H
#define MOVE_H

#include "types.h"

#define NULL_MOVE 0

#define QUIET_FLAG        0b0000
#define CASTLE_FLAG       0b0001
#define CAPTURE_FLAG      0b0100
#define EP_FLAG           0b0110
#define PROMO_FLAG        0b1000
#define KNIGHT_PROMO_FLAG 0b1000
#define BISHOP_PROMO_FLAG 0b1001
#define ROOK_PROMO_FLAG   0b1010
#define QUEEN_PROMO_FLAG  0b1011

extern const int CHAR_TO_PIECE[];
extern const char* PIECE_TO_CHAR;
extern const char* PROMOTION_TO_CHAR;
extern const char* SQ_TO_COORD[64];
extern const int CASTLE_ROOK_DEST[64];
extern const int CASTLING_ROOK[64];

#define BuildMove(from, to, piece, flags) (from) | ((to) << 6) | ((piece) << 12) | ((flags) << 16)
#define FromTo(move)                      (((int) (move) &0x00fff) >> 0)
#define From(move)                        (((int) (move) &0x0003f) >> 0)
#define To(move)                          (((int) (move) &0x00fc0) >> 6)
#define Moving(move)                      (((int) (move) &0x0f000) >> 12)
#define Flags(move)                       (((int) (move) &0xf0000) >> 16)

#define IsCap(move)           (!!(Flags(move) & CAPTURE_FLAG))
#define IsEP(move)            (Flags(move) == EP_FLAG)
#define IsCas(move)           (Flags(move) == CASTLE_FLAG)

#define IsPromo(move)         (!!(Flags(move) & PROMO_FLAG))
#define PromoPT(move)         ((Flags(move) & 0x3) + KNIGHT)
#define PromoPiece(move, stm) (Piece(PromoPT(move), stm))

Move ParseMove(char* moveStr, Board* board);
char* MoveToStr(Move move, Board* board);

#endif

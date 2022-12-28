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

#include <stdio.h>

#include "board.h"
#include "types.h"
#include "util.h"

#define NULL_MOVE 0

#define NORMAL2 (0 << 12)
#define CASTLE2 (1 << 12)
#define EP2     (2 << 12)
#define PROMO   (3 << 12)

#define KNIGHT_PROMO (3 << 12)
#define BISHOP_PROMO (7 << 12)
#define ROOK_PROMO   (11 << 12)
#define QUEEN_PROMO  (15 << 12)

extern const int CHAR_TO_PIECE[];
extern const char* PIECE_TO_CHAR;
extern const char* PROMOTION_TO_CHAR;
extern const char* SQ_TO_COORD[64];
extern const int CASTLE_ROOK_DEST[64];
extern const int CASTLING_ROOK[64];

#define BuildMove(from, to, newFlags, promo, flags) \
  (from) | ((to) << 6) | (newFlags) | ((promo) << 16) | ((flags) << 20)
#define From(move)  ((int) (move) &0x3f)
#define To(move)    (((int) (move) &0xfc0) >> 6)
#define Promo(move) (((int) (move) &0xf0000) >> 16)
// #define IsEP(move)   (((int) (move) &0x400000) >> 22)
// #define IsCas(move)  (((int) (move) &0x800000) >> 23)
// just mask the from/to bits into a single int for indexing butterfly tables
#define FromTo(move) ((int) (move) &0xfff)

INLINE int IsEP(Move move) {
  return (move & 0x3000) == EP2;
}

INLINE int IsCas(Move move) {
  return (move & 0x3000) == CASTLE2;
}

INLINE int IsCapture(Move move, Board* board) {
  return (!IsCas(move) && board->squares[To(move)] != NO_PIECE) || IsEP(move);
}

INLINE int IsTactical(Move move, Board* board) {
  return IsCapture(move, board) || Promo(move);
}

Move ParseMove(char* moveStr, Board* board);
char* MoveToStr(Move move, Board* board);
int IsRecapture(SearchStack* ss, Board* board, Move move);

#endif

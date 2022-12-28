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

#define NORMAL (0 << 12)
#define CASTLE (1 << 12)
#define EP     (2 << 12)
#define PROMO  (3 << 12)

#define NORMAL_CAPTURE (4 << 12)

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

#define BuildMove(from, to, flags) (from) | ((to) << 6) | (flags)
#define From(move)                 ((int) (move) &0x3f)
#define To(move)                   (((int) (move) &0xfc0) >> 6)
// just mask the from/to bits into a single int for indexing butterfly tables
#define FromTo(move) ((int) (move) &0xfff)

INLINE int IsNormal(Move move) {
  return (move & 0x3000) == NORMAL;
}

INLINE int IsNormalCapture(Move move) {
  return (move & 0xF000) == NORMAL_CAPTURE;
}

INLINE int IsPromo(Move move) {
  return (move & 0x3000) == PROMO;
}

INLINE int PromoPiece(Move move, int color) {
  return Piece(((move & 0xc000) >> 14) + 1, color);
}

INLINE int IsEP(Move move) {
  return (move & 0x3000) == EP;
}

INLINE int IsCas(Move move) {
  return (move & 0x3000) == CASTLE;
}

INLINE int IsCapture(Move move, Board* board) {
  return IsNormalCapture(move) || IsEP(move) || (board->squares[To(move)] != NO_PIECE && !IsCas(move));
}

INLINE int IsTactical(Move move, Board* board) {
  return IsCapture(move, board) || IsPromo(move);
}

Move ParseMove(char* moveStr, Board* board);
char* MoveToStr(Move move, Board* board);
int IsRecapture(SearchStack* ss, Board* board, Move move);

#endif

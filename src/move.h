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

#ifndef MOVE_H
#define MOVE_H

#include "board.h"
#include "types.h"
#include "util.h"

typedef struct {
  int count;
  Move moves[MAX_MOVES];
} SimpleMoveList;

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
extern const char* SQ_TO_COORD[64];
extern const int CASTLE_ROOK_DEST[64];
extern const int CASTLING_ROOK[64];

// from | to << 6 | piece << 12 | flags << 16
INLINE Move BuildMove(Square from, Square to, Piece piece, int flags) {
  return from | (to << 6) | (piece << 12) | (flags << 16);
}

INLINE int FromTo(Move move) {
  return (int) move & 0x00fff;
}

INLINE Square From(Move move) {
  return (int) move & 0x0003f;
}

INLINE Square To(Move move) {
  return ((int) move & 0x00fc0) >> 6;
}

INLINE Piece Moving(Move move) {
  return ((int) move & 0x0f000) >> 12;
}

INLINE int Flags(Move move) {
  return ((int) move & 0xf0000) >> 16;
}

INLINE Bool IsCap(Move move) {
  return !!(Flags(move) & CAPTURE_FLAG);
}

INLINE Bool IsEP(Move move) {
  return Flags(move) == EP_FLAG;
}

INLINE Bool IsCas(Move move) {
  return Flags(move) == CASTLE_FLAG;
}

INLINE Bool IsPromo(Move move) {
  return !!(Flags(move) & PROMO_FLAG);
}

INLINE PieceType PromoPT(Move move) {
  return (Flags(move) & 0x3) + KNIGHT;
}

INLINE Piece PromoPiece(Move move, Color stm) {
  return MakePiece(PromoPT(move), stm);
}

Move ParseMove(char* moveStr, Board* board);
char* MoveToStr(Move move, Board* board);

#endif

// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

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

#ifndef TYPES_H
#define TYPES_H

#include <inttypes.h>
#include <limits.h>

#define MAX_SEARCH_PLY 201 // effective max depth 250
#define MAX_MOVES      128

#define ALIGN_ON 64
#define ALIGN    __attribute__((aligned(ALIGN_ON)))

typedef uint64_t BitBoard;
typedef uint32_t Move;

// defined in nn/defs.h and thread.h; Board and the search only hold pointers
typedef struct Accumulator Accumulator;
typedef struct AccumulatorKingState AccumulatorKingState;
typedef struct ThreadData ThreadData;

typedef struct {
  int castling;
  int ep;
  int fmr;
  int nullply;
  uint64_t zobrist;
  uint64_t pawnZobrist;
  uint64_t nonPawnZobrist[2];
  BitBoard checkers;
  BitBoard pinned;
  BitBoard threatened;
  BitBoard threatenedBy[6];
  int capture;
} BoardHistory;

typedef struct {
  // The below are in order of BoardHistory above for copies
  int castling; // castling mask e.g. 1111 = KQkq, 1001 = Kq
  int epSquare; // en passant square (a8 or 0 is not valid so that marks no
                // active ep)
  int fmr;      // half move count for 50 move rule
  int nullply;  // distance from last nullmove

  uint64_t zobrist;     // zobrist hash of the position
  uint64_t pawnZobrist; // pawn zobrist hash of the position (pawns + stm)
  uint64_t nonPawnZobrist[2]; // per color non-pawn zobrist hash (pieces + stm)

  BitBoard checkers; // checking piece squares
  BitBoard pinned;   // pinned pieces

  BitBoard threatened; // opponent "threatening" these squares
  BitBoard threatenedBy[6];

  int stm;     // side to move
  int xstm;    // not side to move
  int histPly; // ply for historical state
  int moveNo;  // game move number
  int phase;   // efficiently updated phase for scaling

  uint64_t piecesCounts; // "material key" - pieces left on the board

  uint8_t squares[64];     // piece per square
  BitBoard occupancies[3]; // 0 - white pieces, 1 - black pieces, 2 - both
  BitBoard pieces[12];     // individual piece data

  int cr[4];
  int castlingRights[64];

  BoardHistory history[MAX_SEARCH_PLY + 100];

  Accumulator* accumulators;
  AccumulatorKingState* refreshTable;
} Board;

typedef struct {
  int count;
  Move moves[MAX_MOVES];
} SimpleMoveList;

// Tracking the principal variation
typedef struct {
  int count;
  Move moves[MAX_SEARCH_PLY];
} PV;

enum {
  WHITE,
  BLACK,
  BOTH
};

enum {
  A8,
  B8,
  C8,
  D8,
  E8,
  F8,
  G8,
  H8,
  A7,
  B7,
  C7,
  D7,
  E7,
  F7,
  G7,
  H7,
  A6,
  B6,
  C6,
  D6,
  E6,
  F6,
  G6,
  H6,
  A5,
  B5,
  C5,
  D5,
  E5,
  F5,
  G5,
  H5,
  A4,
  B4,
  C4,
  D4,
  E4,
  F4,
  G4,
  H4,
  A3,
  B3,
  C3,
  D3,
  E3,
  F3,
  G3,
  H3,
  A2,
  B2,
  C2,
  D2,
  E2,
  F2,
  G2,
  H2,
  A1,
  B1,
  C1,
  D1,
  E1,
  F1,
  G1,
  H1,
};

enum {
  N  = -8,
  E  = 1,
  S  = 8,
  W  = -1,
  NE = -7,
  SE = 9,
  SW = 7,
  NW = -9
};

enum {
  WHITE_PAWN,
  BLACK_PAWN,
  WHITE_KNIGHT,
  BLACK_KNIGHT,
  WHITE_BISHOP,
  BLACK_BISHOP,
  WHITE_ROOK,
  BLACK_ROOK,
  WHITE_QUEEN,
  BLACK_QUEEN,
  WHITE_KING,
  BLACK_KING
};

enum {
  PAWN,
  KNIGHT,
  BISHOP,
  ROOK,
  QUEEN,
  KING
};

#endif

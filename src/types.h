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

#ifndef TYPES_H
#define TYPES_H

#include <inttypes.h>
#include <setjmp.h>

#define MAX_SEARCH_PLY 100
#define MAX_MOVES 128

#define N_FEATURES (8 * 12 * 64)
#define N_HIDDEN 512
#define N_OUTPUT 1

#if defined(__AVX512F__)
#define ALIGN_ON 64
#elif defined(__AVX2__)
#define ALIGN_ON 32
#else
#define ALIGN_ON 16
#endif

typedef struct {
  int na, nr;
  int additions[2];
  int removals[2];
} NNUpdate;

typedef int16_t Accumulator[N_HIDDEN] __attribute__((aligned(ALIGN_ON)));

typedef int Score;
typedef uint64_t BitBoard;
typedef uint32_t Move;

typedef struct {
  BitBoard pcs, sqs;
} Threat;

typedef struct {
  int stm, xstm;  // stm to move
  int epSquare;   // en passant square (a8 or 0 is not valid so that marks no active ep)
  int castling;   // castling mask e.g. 1111 = KQkq, 1001 = Kq
  int histPly;
  int moveNo;    
  int halfMove;   // half move count for 50 move rule
  int ply;
  int phase;

  BitBoard checkers;      // checking piece squares
  BitBoard pinned;        // pinned pieces
  uint64_t piecesCounts;  // "material key" - pieces left on the board
  uint64_t zobrist;       // zobrist hash of the position

  Accumulator* accumulators[2];

  int squares[64];          // piece per square
  BitBoard occupancies[3];  // 0 - white pieces, 1 - black pieces, 2 - both
  BitBoard pieces[13];      // individual piece data

  int cr[4];
  int castlingRights[64];

  // data that is hard to track, so it is "remembered" when search undoes moves
  int castlingHistory[MAX_SEARCH_PLY + 100];
  int epSquareHistory[MAX_SEARCH_PLY + 100];
  int captureHistory[MAX_SEARCH_PLY + 100];
  int halfMoveHistory[MAX_SEARCH_PLY + 100];
  uint64_t zobristHistory[MAX_SEARCH_PLY + 100];
  BitBoard checkersHistory[MAX_SEARCH_PLY + 100];
  BitBoard pinnedHistory[MAX_SEARCH_PLY + 100];
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

// A general data object for use during search
typedef struct {
  int window;

  Board* board;  // reference to board
  int ply;       // ply depth of active search

  // TODO: Put depth here as well? Just cause
  uint64_t nodes;  // node count
  uint64_t tbhits;
  int seldepth;  // seldepth count

  Move* moves;

  int contempt[2];

  int de[MAX_SEARCH_PLY]; // double extensions

  Move skipMove[MAX_SEARCH_PLY];         // moves to skip during singular search
  int evals[MAX_SEARCH_PLY];             // static evals at ply stack
  Move searchMoves[MAX_SEARCH_PLY + 2];  // moves for ply stack

  Move killers[MAX_SEARCH_PLY][2];  // killer moves, 2 per ply
  Move counters[64 * 64];           // counter move butterfly table
  int hh[2][2][64 * 64];           // history heuristic butterfly table (stm / threatened)
  int ch[12][64][12][64];           // continuation move history table

  int th[6][64][6];  // tactical (capture) history

  int64_t tm[64 * 64];
} SearchData;

typedef struct {
  long start;
  int alloc;
  int max;

  uint64_t nodes;
  int hitrate;

  int timeset;
  int depth;
  int movesToGo;
  int stopped;
  int quit;
  int multiPV;
  int searchMoves;
  SimpleMoveList searchable;
} SearchParams;

typedef struct {
  int depth;
  Score scores[MAX_SEARCH_PLY];
  Move bestMoves[MAX_SEARCH_PLY];
  Move ponderMoves[MAX_SEARCH_PLY];
} SearchResults;

typedef struct ThreadData ThreadData;

struct ThreadData {
  int count, idx, multiPV, depth;

  Accumulator* accumulators[2];

  ThreadData* threads;
  jmp_buf exit;

  SearchParams* params;
  SearchResults* results;
  SearchData data;

  Board board;

  Score scores[MAX_MOVES];
  Move bestMoves[MAX_MOVES];
  PV pvs[MAX_MOVES];
};

typedef struct {
  Board* board;
  SearchParams* params;
  ThreadData* threads;
} SearchArgs;

// Move generation storage
// moves/scores idx's match
enum { ALL_MOVES, TACTICAL_MOVES };

enum {
  HASH_MOVE,
  GEN_TACTICAL_MOVES,
  PLAY_GOOD_TACTICAL,
  PLAY_KILLER_1,
  PLAY_KILLER_2,
  PLAY_COUNTER,
  GEN_QUIET_MOVES,
  PLAY_QUIETS,
  PLAY_BAD_TACTICAL,
  NO_MORE_MOVES,
  PERFT_MOVES,
};

typedef struct {
  SearchData* data;
  Move hashMove, killer1, killer2, counter;
  int seeCutoff;
  uint8_t type, phase, nTactical, nQuiets, nBadTactical;

  BitBoard threats;

  Move tactical[MAX_MOVES];
  Move quiet[MAX_MOVES];
  int sTactical[MAX_MOVES];
  int sQuiet[MAX_MOVES];
} MoveList;

enum { WHITE, BLACK, BOTH };

// clang-format off
enum {
  A8, B8, C8, D8, E8, F8, G8, H8,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A1, B1, C1, D1, E1, F1, G1, H1,
};
// clang-format on

enum { N = -8, E = 1, S = 8, W = -1, NE = -7, SE = 9, SW = 7, NW = -9 };

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

enum { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

enum { MG, EG };

#endif

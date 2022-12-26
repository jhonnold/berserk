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
#include <limits.h>
#include <setjmp.h>

#define MAX_SEARCH_PLY (INT8_MAX + 1)
#define MAX_MOVES      128

#define N_KING_BUCKETS 16

#define N_FEATURES (N_KING_BUCKETS * 12 * 64)
#define N_HIDDEN   512
#define N_OUTPUT   1

#define ALIGN_ON 64
#define ALIGN    __attribute__((aligned(ALIGN_ON)))

typedef int Score;
typedef uint64_t BitBoard;
typedef uint32_t Move;

enum { SUB = 0, ADD = 1 };

typedef int16_t Accumulator[N_HIDDEN] ALIGN;

typedef struct {
  BitBoard pcs[12];
  Accumulator values;
} ALIGN AccumulatorKingState;

typedef struct {
  BitBoard pcs, sqs;
} Threat;

typedef struct {
  int stm;      // side to move
  int xstm;     // not side to move
  int acc;      // board ply for accumulator access
  int histPly;  // ply for historical state
  int moveNo;   // game move number
  int halfMove; // half move count for 50 move rule
  int castling; // castling mask e.g. 1111 = KQkq, 1001 = Kq
  int phase;    // efficiently updated phase for scaling
  int epSquare; // en passant square (a8 or 0 is not valid so that marks no active ep)

  BitBoard checkers;     // checking piece squares
  BitBoard pinned;       // pinned pieces
  uint64_t piecesCounts; // "material key" - pieces left on the board
  uint64_t zobrist;      // zobrist hash of the position

  Accumulator* accumulators[2];
  AccumulatorKingState* refreshTable[2];

  int squares[64];         // piece per square
  BitBoard occupancies[3]; // 0 - white pieces, 1 - black pieces, 2 - both
  BitBoard pieces[12];     // individual piece data

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

typedef int PieceTo[12][64];

typedef struct {
  int ply, staticEval, de;
  PieceTo* ch;
  Move move, skip;
  Move killers[2];
} SearchStack;

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
  Score prevScore;
  Score scores[MAX_SEARCH_PLY];
  Move bestMoves[MAX_SEARCH_PLY];
  Move ponderMoves[MAX_SEARCH_PLY];
} SearchResults;

typedef struct ThreadData ThreadData;

struct ThreadData {
  int count, idx, multiPV, depth, seldepth;
  uint64_t nodes, tbhits;

  Accumulator* accumulators[2];
  AccumulatorKingState* refreshTable[2];

  jmp_buf exit;

  SearchParams* params;
  SearchResults results;
  Board board;

  uint64_t nodeCounts[64 * 64];

  int contempt[2];

  Score scores[MAX_MOVES];
  Move bestMoves[MAX_MOVES];
  PV pvs[MAX_MOVES];

  Move counters[12][64];    // counter move butterfly table
  int hh[2][2][2][64 * 64]; // history heuristic butterfly table (stm / threatened)
  int ch[12][64][12][64];   // continuation move history table
  int th[6][64][7];         // tactical (capture) history
};

typedef struct {
  Board* board;
  SearchParams* params;
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
  ThreadData* thread;
  SearchStack* ss;
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

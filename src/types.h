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

#ifndef TYPES_H
#define TYPES_H

#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <setjmp.h>

#define MAX_SEARCH_PLY 251 // effective max depth 250
#define MAX_MOVES      128

#define N_KING_BUCKETS 16

#define N_FEATURES (N_KING_BUCKETS * 12 * 64)
#define N_HIDDEN   768
#define N_L1       (2 * N_HIDDEN)
#define N_L2       8
#define N_L3       32
#define N_OUTPUT   1

#define ALIGN_ON 64
#define ALIGN    __attribute__((aligned(ALIGN_ON)))

typedef int Score;
typedef uint64_t BitBoard;
typedef uint16_t Move;

enum {
  SUB = 0,
  ADD = 1
};

typedef int16_t acc_t;

typedef struct {
  uint8_t correct[2];
  uint16_t captured;
  int moving;
  Move move;
  acc_t values[2][N_HIDDEN] ALIGN;
} Accumulator;

typedef struct {
  acc_t values[N_HIDDEN] ALIGN;
  BitBoard pcs[12];
} AccumulatorKingState;

typedef struct {
  BitBoard pcs, sqs;
} Threat;

typedef struct {
  int capture;
  int castling;
  int ep;
  int fmr;
  int nullply;
  uint64_t zobrist;
  BitBoard checkers;
  BitBoard pinned;
} BoardHistory;

typedef struct {
  int stm;      // side to move
  int xstm;     // not side to move
  int histPly;  // ply for historical state
  int moveNo;   // game move number
  int fmr;      // half move count for 50 move rule
  int nullply;  // distance from last nullmove
  int castling; // castling mask e.g. 1111 = KQkq, 1001 = Kq
  int phase;    // efficiently updated phase for scaling
  int epSquare; // en passant square (a8 or 0 is not valid so that marks no
                // active ep)

  BitBoard checkers;     // checking piece squares
  BitBoard pinned;       // pinned pieces
  uint64_t piecesCounts; // "material key" - pieces left on the board
  uint64_t zobrist;      // zobrist hash of the position

  int squares[64];         // piece per square
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

typedef int16_t PieceTo[12][64];

typedef struct {
  int ply, staticEval, de;
  PieceTo* ch;
  Move move, skip;
  Threat oppThreat, ownThreat;
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
  int mate;
  int movesToGo;
  int stopped;
  int quit;
  int multiPV;
  int infinite;
  int searchMoves;
  SimpleMoveList searchable;
} SearchParams;

typedef struct {
  Move move;
  int seldepth;
  int score, previousScore;
  uint64_t nodes;
  PV pv;
} RootMove;

enum {
  THREAD_SLEEP,
  THREAD_SEARCH,
  THREAD_TT_CLEAR,
  THREAD_SEARCH_CLEAR,
  THREAD_EXIT,
  THREAD_RESUME
};

typedef struct ThreadData ThreadData;

struct ThreadData {
  int idx, multiPV, depth, seldepth;
  uint64_t nodes, tbhits;

  Accumulator* accumulators;
  AccumulatorKingState* refreshTable;

  Board board;

  int contempt[2];
  int previousScore;
  int numRootMoves;
  RootMove rootMoves[MAX_MOVES];

  Move counters[12][64];         // counter move butterfly table
  int16_t hh[2][2][2][64 * 64];  // history heuristic butterfly table (stm / threatened)
  int16_t ch[2][12][64][12][64]; // continuation move history table
  int16_t caph[12][64][7];       // capture history

  int action, calls;
  pthread_t nativeThread;
  pthread_mutex_t mutex;
  pthread_cond_t sleep;
  jmp_buf exit;
};

typedef struct {
  Board* board;
} SearchArgs;

// Move generation storage
// moves/scores idx's match
enum {
  ALL_MOVES,
  NOISY_MOVES
};

enum {
  HASH_MOVE,
  GEN_NOISY_MOVES,
  PLAY_GOOD_NOISY,
  PLAY_KILLER_1,
  PLAY_KILLER_2,
  PLAY_COUNTER,
  GEN_QUIET_MOVES,
  PLAY_QUIETS,
  PLAY_BAD_NOISY,
  // ProbCut
  PC_GEN_NOISY_MOVES,
  PC_PLAY_GOOD_NOISY,
  PC_PLAY_BAD_NOISY,
  // QSearch
  QS_GEN_NOISY_MOVES,
  QS_PLAY_NOISY_MOVES,
  // QSearch Evasions
  QS_EVASION_HASH_MOVE,
  QS_GEN_EVASIONS,
  QS_PLAY_EVASIONS,
  // ...
  NO_MORE_MOVES,
  PERFT_MOVES,
};

typedef struct {
  int score;
  Move move;
} ScoredMove;

typedef struct {
  ThreadData* thread;
  SearchStack* ss;
  Move hashMove, killer1, killer2, counter;
  int seeCutoff, phase;

  ScoredMove *current, *end, *endBad;
  ScoredMove moves[MAX_MOVES];
} MovePicker;

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

enum {
  MG,
  EG
};

#endif

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
#include <pthread.h>
#include <setjmp.h>
#include <stdatomic.h>

typedef uint64_t BitBoard;
typedef uint32_t Move;
typedef int32_t Score;

#define MAX_SEARCH_PLY 201 // effective max depth 200
#define MAX_MOVES      128

// search specific score evals
#define UNKNOWN      32257 // this must be higher than CHECKMATE
#define CHECKMATE    32200
#define MATE_BOUND   (32200 - MAX_SEARCH_PLY)
#define TB_WIN_SCORE MATE_BOUND
#define TB_WIN_BOUND (TB_WIN_SCORE - MAX_SEARCH_PLY)

// eval unknown value (in-check)
#define EVAL_UNKNOWN 2046

// NN network sizes
#define N_KING_BUCKETS    16
#define N_FEATURES        (N_KING_BUCKETS * 12 * 64)
#define N_HIDDEN          1024
#define N_L1              (2 * N_HIDDEN)
#define N_L2              16
#define N_L3              32
#define N_OUTPUT          1
#define SPARSE_CHUNK_SIZE 4

#define ALIGN __attribute__((aligned(64)))

typedef int16_t acc_t;

typedef struct {
  uint8_t correct[2];
  uint16_t captured;
  Move move;
  acc_t values[2][N_HIDDEN] ALIGN;
} Accumulator;

typedef struct {
  acc_t values[N_HIDDEN] ALIGN;
  BitBoard pcs[12];
} AccumulatorKingState;

// Move + Move Types
#define NULL_MOVE         0
#define QUIET_FLAG        0b0000
#define CASTLE_FLAG       0b0001
#define CAPTURE_FLAG      0b0100
#define EP_FLAG           0b0110
#define PROMO_FLAG        0b1000
#define KNIGHT_PROMO_FLAG 0b1000
#define BISHOP_PROMO_FLAG 0b1001
#define ROOK_PROMO_FLAG   0b1010
#define QUEEN_PROMO_FLAG  0b1011

// Note: Any usage of & that matches 1 or 2 will match 3
//       thus making GT_LEGAL generate all.
enum {
  GT_QUIET   = 0b01,
  GT_CAPTURE = 0b10,
  GT_LEGAL   = 0b11,
};

// Castling rights
#define WHITE_KS 0x8
#define WHITE_QS 0x4
#define BLACK_KS 0x2
#define BLACK_QS 0x1

// TT
#define NO_ENTRY    0ULL
#define MEGABYTE    (1024ull * 1024ull)
#define BUCKET_SIZE 3
#define BOUND_MASK  (0x3)
#define PV_MASK     (0x4)
#define AGE_MASK    (0xF8)
#define AGE_INC     (0x8)
#define AGE_CYCLE   (255 + AGE_INC)

typedef struct {
  int capture;
  int castling;
  int ep;
  int fmr;
  int nullply;
  uint64_t zobrist;
  BitBoard checkers;
  BitBoard pinned;
  BitBoard threatened;
  BitBoard easyCapture;
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

  BitBoard checkers; // checking piece squares
  BitBoard pinned;   // pinned pieces

  BitBoard threatened;  // opponent "threatening" these squares
  BitBoard easyCapture; // opponent capturing these is a guarantee SEE > 0

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
  atomic_uint_fast64_t nodes, tbhits;

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
  int16_t caph[12][64][2][7];    // capture history (piece - to - defeneded - captured_type)

  int action, calls;
  pthread_t nativeThread;
  pthread_mutex_t mutex;
  pthread_cond_t sleep;
  jmp_buf exit;
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
  // QSearch
  QS_GEN_NOISY_MOVES,
  QS_PLAY_NOISY_MOVES,
  QS_GEN_QUIET_CHECKS,
  QS_PLAY_QUIET_CHECKS,
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
  int seeCutoff, phase, genChecks;

  ScoredMove *current, *end, *endBad;
  ScoredMove moves[MAX_MOVES];
} MovePicker;

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
  WHITE,
  BLACK,
  BOTH
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
  BLACK_KING,
  NO_PIECE,
};

enum {
  PAWN,
  KNIGHT,
  BISHOP,
  ROOK,
  QUEEN,
  KING
};

typedef struct __attribute__((packed)) {
  uint16_t hash;
  uint8_t depth;
  uint8_t agePvBound;
  uint32_t evalAndMove;
  int16_t score;
} TTEntry;

typedef struct {
  TTEntry entries[BUCKET_SIZE];
  uint16_t padding;
} TTBucket;

typedef struct {
  void* mem;
  TTBucket* buckets;
  uint64_t count;
  uint8_t age;
} TTTable;

enum {
  BOUND_UNKNOWN = 0,
  BOUND_LOWER   = 1,
  BOUND_UPPER   = 2,
  BOUND_EXACT   = 3
};

#endif

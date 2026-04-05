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
#include <pthread.h>
#include <setjmp.h>
#include <stdatomic.h>

#define MAX_SEARCH_PLY 201 // effective max depth 250
#define MAX_MOVES      128

#define CORRECTION_GRAIN 256

#define PAWN_CORRECTION_SIZE 131072
#define PAWN_CORRECTION_MASK (PAWN_CORRECTION_SIZE - 1)

typedef int Score;
typedef uint64_t BitBoard;
typedef uint32_t Move;

typedef struct {
  BitBoard passedPawns;
  BitBoard openFiles;
  int kingSq[2];
  BitBoard kingArea[2];
  BitBoard attacks[2][6];
  BitBoard allAttacks[2];
  BitBoard twoAttacks[2];
  Score ksAttackWeight[2];
  int ksAttackerCount[2];
  BitBoard mobilitySquares[2];
  BitBoard outposts[2];
} EvalData;

typedef struct {
  Score s;
  uint64_t hash;
  BitBoard passedPawns;
} PawnHashEntry;

#ifdef TUNE
#define PAWN_TABLE_MASK (0x1)
#define PAWN_TABLE_SIZE (1ULL << 1)
#else
#define PAWN_TABLE_MASK (0xFFFF)
#define PAWN_TABLE_SIZE (1ULL << 16)
#endif

typedef struct {
  int castling;
  int ep;
  int fmr;
  int nullply;
  uint64_t zobrist;
  uint64_t pawnZobrist;
  Score mat;
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

  Score mat;           // incremental material + PSQT score

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

  int squares[64];         // piece per square
  BitBoard occupancies[3]; // 0 - white pieces, 1 - black pieces, 2 - both
  BitBoard pieces[12];     // individual piece data

  int cr[4];
  int castlingRights[64];

  BoardHistory history[MAX_SEARCH_PLY + 100];
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
  int reduction;
  PieceTo* ch;
  PieceTo* cont;
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
  int score, previousScore, avgScore;
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

  int nmpMinPly, npmColor;

  Board board;

  int contempt[2];
  int previousScore;
  int numRootMoves;
  RootMove rootMoves[MAX_MOVES];

  Move counters[12][64];         // counter move butterfly table
  int16_t hh[2][2][2][64 * 64];  // history heuristic butterfly table (stm / threatened)
  int16_t ch[2][12][64][12][64]; // continuation move history table
  int16_t caph[12][64][2][7];    // capture history (piece - to - defeneded - captured_type)

  int16_t pawnCorrection[PAWN_CORRECTION_SIZE];
  int16_t contCorrection[12][64][12][64];

  PawnHashEntry pawnHashTable[PAWN_TABLE_SIZE];

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
  // QSearch
  QS_GEN_NOISY_MOVES,
  QS_PLAY_NOISY_MOVES,
  QS_GEN_QUIET_CHECKS,
  QS_PLAY_QUIET_CHECKS,
  // QSearch Evasions
  QS_EVASION_HASH_MOVE,
  QS_EVASION_GEN_NOISY,
  QS_EVASION_PLAY_NOISY,
  QS_EVASION_GEN_QUIET,
  QS_EVASION_PLAY_QUIET,
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

typedef struct {
  int8_t pieces[5];
  int8_t psqt[6][2][32];
  int8_t bishopPair;

  int8_t knightPostPsqt[12];
  int8_t bishopPostPsqt[12];

  int8_t knightMobilities[9];
  int8_t bishopMobilities[14];
  int8_t rookMobilities[15];
  int8_t queenMobilities[28];
  int8_t kingMobilities[9];

  int8_t minorBehindPawn;
  int8_t knightPostReachable;
  int8_t bishopPostReachable;
  int8_t bishopTrapped;
  int8_t rookTrapped;
  int8_t badBishopPawns;
  int8_t dragonBishop;
  int8_t rookOpenFileOffset;
  int8_t rookOpenFile;
  int8_t rookSemiOpen;
  int8_t rookToOpen;
  int8_t queenOppositeRook;
  int8_t queenRookBattery;

  int8_t defendedPawns;
  int8_t doubledPawns;
  int8_t isolatedPawns[4];
  int8_t openIsolatedPawns;
  int8_t backwardsPawns;
  int8_t connectedPawn[4][8];
  int8_t candidatePasser[8];
  int8_t candidateEdgeDistance;

  int8_t passedPawn[8];
  int8_t passedPawnEdgeDistance;
  int8_t passedPawnKingProximity;
  int8_t passedPawnAdvance[5];
  int8_t passedPawnEnemySliderBehind;
  int8_t passedPawnSqRule;
  int8_t passedPawnUnsupported;
  int8_t passedPawnOutsideVKnight;

  int8_t knightThreats[6];
  int8_t bishopThreats[6];
  int8_t rookThreats[6];
  int8_t kingThreat;
  int8_t pawnThreat;
  int8_t pawnPushThreat;
  int8_t pawnPushThreatPinned;
  int8_t hangingThreat;
  int8_t knightCheckQueen;
  int8_t bishopCheckQueen;
  int8_t rookCheckQueen;

  int16_t space;

  int16_t imbalance[5][5];

  int8_t pawnShelter[4][8];
  int8_t pawnStorm[4][8];
  int8_t blockedPawnStorm[8];
  int8_t castlingRights;

  int8_t complexPawns;
  int8_t complexPawnsBothSides;
  int8_t complexOffset;

  int ks;
  int danger[2];
  int8_t ksAttackerCount[2];
  int8_t ksAttackerWeights[2][5];
  int8_t ksWeakSqs[2];
  int8_t ksPinned[2];
  int8_t ksKnightCheck[2];
  int8_t ksBishopCheck[2];
  int8_t ksRookCheck[2];
  int8_t ksQueenCheck[2];
  int8_t ksUnsafeCheck[2];
  int8_t ksEnemyQueen[2];
  int8_t ksKnightDefense[2];

  int8_t ss;
} EvalCoeffs;

#endif

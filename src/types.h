#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#define MAX_DEPTH 64

typedef uint64_t BitBoard;

typedef uint64_t TTValue;

typedef int Move;

typedef struct {
  Move moves[256];
  int scores[256];
  int count;
} MoveList;

typedef struct {
  // pieces
  BitBoard pieces[12];
  BitBoard occupancies[3];
  int squares[64];
  BitBoard checkers;
  BitBoard pinners;
  uint64_t piecesCounts;

  // Game state
  int side;
  int xside;
  int epSquare;
  int castling;
  int moveNo;
  int halfMove;

  uint64_t zobrist;
  uint64_t zobristHistory[512];

  // history
  int castlingHistory[512];
  int epSquareHistory[512];
  int captureHistory[512];
  int halfMoveHistory[512];
} Board;

typedef struct {
  int count;
  Move moves[MAX_DEPTH];
} PV;

typedef struct {
  int score;
  Move bestMove;

  Board* board;
  int ply;

  int nodes;
  int seldepth;

  int evals[MAX_DEPTH];
  Move moves[MAX_DEPTH];
  Move killers[MAX_DEPTH][2];
  Move counters[64 * 64];
  int hh[2][64 * 64];
  int bf[2][64 * 64];
} SearchData;

typedef struct {
  long startTime;
  long endTime;
  int depth;
  int timeset;
  int movesToGo;
  int stopped;
  int quit;
} SearchParams;

typedef struct {
  int material[2];
  int pawns[2];
  int knights[2];
  int bishops[2];
  int rooks[2];
  int queens[2];
  int kings[2];

  int mobility[2];
  int kingSafety[2];
  int threats[2];

  BitBoard attacks[6];
  BitBoard allAttacks;
  BitBoard attacks2;
  int attackWeight;
  int attackCount;
  int attackers;
} EvalData;

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

enum {
  N = -8,
  E = 1,
  S = 8,
  W = -1,
  NE = -7,
  SE = 9,
  SW = 7,
  NW = -9
};
// clang-format on

#endif

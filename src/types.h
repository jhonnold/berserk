#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

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
  BitBoard checkers;
  BitBoard pinners;
  uint64_t piecesCounts;

  // Game state
  int side;
  int xside;
  int epSquare;
  int castling;
  int moveNo;

  uint64_t zobrist;
  uint64_t zobristHistory[512];

  // history
  int castlingHistory[512];
  int epSquareHistory[512];
  int captureHistory[512];

  // movegen
  Move gameMoves[512];
  Move killers[64][2];
  Move counters[64 * 64];
  int historyHeuristic[2][64 * 64];
  int bfHeuristic[2][64 * 64];
} Board;

typedef struct {
  int nodes;
  int seldepth;
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
  int material;
  int pawns;
  int knights;
  int bishops;
  int rooks;
  int queens;
  int kings;

  int mobility;

  int attacking;
  int kingSafety;

  int threats;
  BitBoard attacks[6];
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

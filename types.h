#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef uint64_t BitBoard;

typedef int Move;

typedef uint64_t TTValue;

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
} Board;

typedef struct {
  long startTime;
  long endTime;
  int depth;
  int timeset;
  int movesToGo;

  long nodes;

  int stopped;
  int quit;
} SearchParams;

extern const BitBoard EMPTY;

extern const int PAWN[];
extern const int KNIGHT[];
extern const int BISHOP[];
extern const int ROOK[];
extern const int QUEEN[];
extern const int KING[];

enum { WHITE, BLACK, BOTH };

#endif

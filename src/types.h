// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

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

#include <setjmp.h>
#include <stdint.h>

#ifdef TUNE
#define MAX_SEARCH_PLY 16
#define MAX_MOVES 256
#define MAX_GAME_PLY 32
#else
#define MAX_SEARCH_PLY 128
#define MAX_MOVES 256
#define MAX_GAME_PLY 1024
#endif

typedef int Score;

typedef uint64_t BitBoard;

typedef uint64_t TTValue;

typedef int Move;

// Move generation storage
// moves/scores idx's match
typedef struct {
  int count;
  Move moves[MAX_MOVES];
  int scores[MAX_MOVES];
} MoveList;

typedef struct {
  BitBoard pieces[12];     // individual piece data
  BitBoard occupancies[3]; // 0 - white pieces, 1 - black pieces, 2 - both
  int squares[64];         // piece per square
  BitBoard checkers;       // checking piece squares
  BitBoard pinned;        // pinned pieces
  uint64_t piecesCounts;   // "material key" - pieces left on the board

  int side;     // side to move
  int xside;    // side not to move
  int epSquare; // en passant square (a8 or 0 is not valid so that marks no active ep)
  int castling; // castling mask e.g. 1111 = KQkq, 1001 = Kq
  int moveNo;   // current game move number TODO: Is this still used?
  int halfMove; // half move count for 50 move rule

  uint64_t zobrist; // zobrist hash of the position

  // data that is hard to track, so it is "remembered" when search undoes moves
  uint64_t zobristHistory[MAX_GAME_PLY];
  int castlingHistory[MAX_GAME_PLY];
  int epSquareHistory[MAX_GAME_PLY];
  int captureHistory[MAX_GAME_PLY];
  int halfMoveHistory[MAX_GAME_PLY];
  BitBoard checkersHistory[MAX_GAME_PLY];
  BitBoard pinnedHistory[MAX_GAME_PLY];
} Board;

// Tracking the principal variation
typedef struct {
  int count;
  Move moves[MAX_SEARCH_PLY];
} PV;

// A general data object for use during search
typedef struct {
  int score;     // analysis score result, from perspective of stm
  Move bestMove; // best move from analysis

  Board* board; // reference to board
  int ply;      // ply depth of active search

  // TODO: Put depth here as well? Just cause
  uint64_t nodes; // node count
  int seldepth;   // seldepth count

  Move skipMove[MAX_SEARCH_PLY]; // moves to skip during singular search
  int evals[MAX_SEARCH_PLY];     // static evals at ply stack
  Move moves[MAX_SEARCH_PLY];    // moves for ply stack

  Move killers[MAX_SEARCH_PLY][2]; // killer moves, 2 per ply
  Move counters[64 * 64];          // counter move butterfly table
  int hh[2][64 * 64];              // history heuristic butterfly table (side)
  int bf[2][64 * 64];              // butterfly heuristic butterfly table (side)
} SearchData;

typedef struct {
  int8_t pieces[2][5];
  int8_t psqt[2][6][32];
  int8_t bishopPair[2];
  int8_t bishopTrapped[2];
  int8_t knightPostPsqt[2][32];
  int8_t bishopPostPsqt[2][32];
  int8_t knightPostReachable[2];
  int8_t bishopPostReachable[2];
  int8_t knightMobilities[2][9];
  int8_t bishopMobilities[2][14];
  int8_t rookMobilities[2][15];
  int8_t queenMobilities[2][28];
  int8_t doubledPawns[2];
  int8_t opposedIsolatedPawns[2];
  int8_t openIsolatedPawns[2];
  int8_t backwardsPawns[2];
  int8_t connectedPawn[2][8];
  int8_t candidatePasser[2][8];
  int8_t passedPawn[2][8];
  int8_t passedPawnAdvance[2];
  int8_t passedPawnEdgeDistance[2];
  int8_t passedPawnKingProximity[2];
  int8_t rookOpenFile[2];
  int8_t rookSemiOpen[2];
  int8_t rookOppositeKing[2];
  int8_t rookAdjacentKing[2];
  int8_t rookTrapped[2];
  int8_t knightThreats[2][6];
  int8_t bishopThreats[2][6];
  int8_t rookThreats[2][6];
  int8_t kingThreats[2][6];
  int8_t hangingThreat[2];
  int ks[2];

  int8_t ss;
} EvalCoeffs;

typedef struct {
  long startTime;
  long endTime;
  long maxTime;
  int timeToSpend;

  int depth;
  int timeset;
  int movesToGo;
  int stopped;
  int quit;
} SearchParams;

typedef struct {
  // these are general data objects, for buildup during eval
  int kingSq[2];
  BitBoard kingArea[2];
  BitBoard attacks[2][6];  // attacks by piece type
  BitBoard allAttacks[2];  // all attacks
  BitBoard twoAttacks[2];  // squares attacked twice
  Score ksAttackWeight[2]; // king safety attackers weight
  int ksSqAttackCount[2];  // king safety sq attack count
  int ksAttackerCount[2];  // attackers

  BitBoard passedPawns[2];
  BitBoard mobilitySquares[2];
  BitBoard outposts[2];

} EvalData;

typedef struct ThreadData ThreadData;

struct ThreadData {
  int count, idx;
  ThreadData* threads;
  jmp_buf exit;

  SearchParams* params;
  SearchData data;

  Board board;
  PV pv;
};

typedef struct {
  Board* board;
  SearchParams* params;
  ThreadData* threads;
} SearchArgs;

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
  PAWN_WHITE,
  PAWN_BLACK,
  KNIGHT_WHITE,
  KNIGHT_BLACK,
  BISHOP_WHITE,
  BISHOP_BLACK,
  ROOK_WHITE,
  ROOK_BLACK,
  QUEEN_WHITE,
  QUEEN_BLACK,
  KING_WHITE,
  KING_BLACK
};

enum { PAWN_TYPE, KNIGHT_TYPE, BISHOP_TYPE, ROOK_TYPE, QUEEN_TYPE, KING_TYPE };

enum { MG, EG };

#endif

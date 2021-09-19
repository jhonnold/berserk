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

#include <inttypes.h>
#include <setjmp.h>

#ifdef TUNE
#define MAX_SEARCH_PLY 16
#define MAX_MOVES 256
#define MAX_GAME_PLY 32
#else
#define MAX_SEARCH_PLY INT8_MAX
#define MAX_MOVES 256
#define MAX_GAME_PLY 1024
#endif

#ifdef TUNE
#define PAWN_TABLE_MASK (0x1)
#define PAWN_TABLE_SIZE (1ULL << 1)
#else
#define PAWN_TABLE_MASK (0xFFFF)
#define PAWN_TABLE_SIZE (1ULL << 16)
#endif

typedef int Score;

typedef uint64_t BitBoard;

typedef uint32_t Move;

typedef struct {
  BitBoard pieces[12];     // individual piece data
  BitBoard occupancies[3]; // 0 - white pieces, 1 - black pieces, 2 - both
  int squares[64];         // piece per square
  BitBoard checkers;       // checking piece squares
  BitBoard pinned;         // pinned pieces
  uint64_t piecesCounts;   // "material key" - pieces left on the board

  Score mat; // material+psqt score updated incrementally

  int side;     // side to move
  int xside;    // side not to move
  int epSquare; // en passant square (a8 or 0 is not valid so that marks no active ep)
  int castling; // castling mask e.g. 1111 = KQkq, 1001 = Kq
  int moveNo;   // current game move number TODO: Is this still used?
  int halfMove; // half move count for 50 move rule

  uint64_t zobrist; // zobrist hash of the position
  uint64_t pawnHash;

  int castlingRights[64];
  int castleRooks[4];
  
  // data that is hard to track, so it is "remembered" when search undoes moves
  int castlingHistory[MAX_GAME_PLY];
  int epSquareHistory[MAX_GAME_PLY];
  int captureHistory[MAX_GAME_PLY];
  int halfMoveHistory[MAX_GAME_PLY];
  Score materialHistory[MAX_GAME_PLY];
  uint64_t zobristHistory[MAX_GAME_PLY];
  uint64_t pawnHashHistory[MAX_GAME_PLY];
  BitBoard checkersHistory[MAX_GAME_PLY];
  BitBoard pinnedHistory[MAX_GAME_PLY];
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
  Score contempt;

  Board* board; // reference to board
  int ply;      // ply depth of active search

  // TODO: Put depth here as well? Just cause
  uint64_t nodes; // node count
  uint64_t tbhits;
  int seldepth; // seldepth count

  Move skipMove[MAX_SEARCH_PLY]; // moves to skip during singular search
  int evals[MAX_SEARCH_PLY];     // static evals at ply stack
  Move moves[MAX_SEARCH_PLY];    // moves for ply stack

  Move killers[MAX_SEARCH_PLY][2]; // killer moves, 2 per ply
  Move counters[64 * 64];          // counter move butterfly table
  int hh[2][64 * 64];              // history heuristic butterfly table (side)
  int ch[6][64][6][64];            // counter move history table
  int fh[6][64][6][64];            // follow up history table
} SearchData;

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
  int8_t complexPawnsOffset;
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

typedef struct {
  long start;
  int alloc;
  int max;

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

typedef struct {
  BitBoard passedPawns;
  BitBoard openFiles;

  // these are general data objects, for buildup during eval
  int kingSq[2];
  BitBoard kingArea[2];
  BitBoard attacks[2][6];  // attacks by piece type
  BitBoard allAttacks[2];  // all attacks
  BitBoard twoAttacks[2];  // squares attacked twice
  Score ksAttackWeight[2]; // king safety attackers weight
  int ksAttackerCount[2];  // attackers

  BitBoard mobilitySquares[2];
  BitBoard outposts[2];
} EvalData;

typedef struct {
  Score s;
  uint64_t hash;
  BitBoard passedPawns;
} PawnHashEntry;

typedef struct ThreadData ThreadData;

struct ThreadData {
  int count, idx, multiPV, depth;

  Score scores[MAX_MOVES];
  Move bestMoves[MAX_MOVES];
  PV pvs[MAX_MOVES];

  ThreadData* threads;
  jmp_buf exit;

  SearchParams* params;
  SearchResults* results;
  SearchData data;

  PawnHashEntry pawnHashTable[PAWN_TABLE_SIZE];

  Board board;
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

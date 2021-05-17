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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "endgame.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "pawns.h"
#include "string.h"
#include "tune.h"
#include "types.h"
#include "util.h"

#ifdef TUNE
#define T 1
#else
#define T 0
#endif

// a utility for texel tuning
// berserk uses a coeff based tuner, ethereal's design
EvalCoeffs C;

#define S(mg, eg) (makeScore((mg), (eg)))

const int MAX_PHASE = 24;
const int PHASE_MULTIPLIERS[5] = {0, 1, 1, 2, 4};
const int MAX_SCALE = 128;

const int STATIC_MATERIAL_VALUE[7] = {93, 310, 323, 548, 970, 30000, 0};
const int SHELTER_STORM_FILES[8][2] = {{0, 2}, {0, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6}, {5, 7}, {5, 7}};

// clang-format off
const Score MATERIAL_VALUES[7] = { S(85, 137), S(340, 360), S(350, 364), S(514, 664), S(1071, 1248), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(25, 57);

const Score PAWN_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  63, 140), S(  15, 160), S(  29, 133), S(  51, 107),
 S( -12,  24), S( -18,  23), S(   5,   1), S(  19, -26),
 S( -21,  -1), S( -10, -13), S( -14, -20), S(   4, -34),
 S( -22, -15), S( -19, -18), S(  -8, -26), S(   2, -30),
 S( -19, -19), S(  -5, -21), S(  -6, -23), S(   5, -21),
 S( -17, -13), S(   2, -18), S(  -8, -17), S(   5, -14),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S(-134, -43), S( -85,  -8), S( -51,   1), S( -16,   5),
 S( -19, -10), S( -12,  14), S(  33,   8), S(  21,  17),
 S(  28, -17), S(  23,   6), S(  13,  28), S(  36,  20),
 S(  23,  12), S(  14,  21), S(  22,  27), S(  30,  32),
 S(  20,  20), S(  23,  16), S(  27,  29), S(  29,  36),
 S(  -1,  17), S(  13,  17), S(  10,  23), S(  21,  37),
 S(  12,  27), S(  13,  14), S(   9,  21), S(  16,  25),
 S( -13,  28), S(   5,  14), S(   9,  21), S(  13,  28),
};

const Score BISHOP_PSQT[32] = {
 S( -31,  29), S( -13,  33), S( -46,  36), S( -65,  41),
 S(  -8,  25), S( -13,  32), S(   2,  33), S(   7,  30),
 S(  29,  24), S(   6,  29), S(  11,  30), S(  17,  27),
 S(  -9,  38), S(  22,  27), S(  15,  31), S(  24,  36),
 S(  32,  19), S(  10,  26), S(  16,  33), S(  23,  30),
 S(  18,  25), S(  32,  34), S(  25,  35), S(  21,  42),
 S(  30,  23), S(  35,  22), S(  29,  23), S(  15,  36),
 S(  26,  24), S(  31,  30), S(  16,  37), S(  16,  36),
};

const Score ROOK_PSQT[32] = {
 S(   3,  50), S(  -9,  54), S( -22,  64), S( -28,  64),
 S(   4,  38), S(  -7,  52), S(  10,  48), S(  13,  40),
 S(  -6,  37), S(  34,  32), S(  25,  36), S(  29,  27),
 S( -15,  45), S(   2,  38), S(  14,  39), S(  13,  35),
 S( -19,  43), S(   0,  36), S( -11,  43), S(  -3,  40),
 S( -14,  38), S(   5,  30), S(   3,  33), S(   1,  34),
 S( -17,  45), S(   1,  33), S(   4,  32), S(   7,  33),
 S(  -2,  35), S(  -1,  36), S(   0,  36), S(   5,  30),
};

const Score QUEEN_PSQT[32] = {
 S(  10,  27), S(  14,  34), S(  46,  31), S(  36,  50),
 S(  -8,  40), S( -37,  73), S( -16, 101), S( -17,  88),
 S(  23,  25), S(  19,  58), S(   3,  80), S(  13,  81),
 S(   4,  71), S(   3,  94), S(   1,  82), S(  -9, 109),
 S(   6,  65), S(   4,  89), S(   0,  90), S(  -9, 109),
 S(   5,  56), S(  12,  59), S(   1,  81), S(   0,  76),
 S(   9,  35), S(  14,  17), S(  15,  33), S(  11,  49),
 S(  14,  15), S(  -1,  31), S(  -3,  32), S(   6,  25),
};

const Score KING_PSQT[32] = {
 S( -96, -86), S(  81, -26), S(  36,   2), S( -12,  -9),
 S( -13,   4), S(  87,  39), S(  96,  37), S(  77,  22),
 S(  -4,  10), S( 127,  35), S( 110,  34), S(  19,  35),
 S( -41,   7), S(  88,  24), S(  10,  37), S( -24,  39),
 S( -92,   7), S(  16,  16), S(  22,  17), S( -20,  24),
 S( -13,  -5), S(  42,   7), S(  21,   7), S(   2,  11),
 S(   2, -10), S(   8,  12), S( -11,   8), S( -39,   6),
 S(   2, -44), S(   7, -10), S(  -3, -17), S( -15, -35),
};

const Score KNIGHT_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( -22,  31), S(  15,  29), S(  42,  20), S(  34,  40),
 S(  21,  16), S(  46,  19), S(  42,  32), S(  53,  31),
 S(  26,   2), S(  35,  20), S(  23,  24), S(  26,  32),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score BISHOP_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   6,  18), S(  32,  13), S(  51,  18), S(  53,  -3),
 S(  19,  11), S(  30,  17), S(  39,   8), S(  44,   2),
 S(  -6,  23), S(  37,  12), S(  33,   7), S(  41,  16),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -41, -65), S( -16, -14), S(  -1,  15), S(   9,  26),
 S(  19,  34), S(  26,  39), S(  33,  40), S(  42,  37),
 S(  50,  28),};

const Score BISHOP_MOBILITIES[14] = {
 S( -28, -33), S(  -9, -14), S(   5,   2), S(  12,  21),
 S(  23,  30), S(  29,  39), S(  32,  45), S(  33,  50),
 S(  35,  54), S(  35,  58), S(  41,  55), S(  59,  57),
 S(  62,  62), S(  62,  63),};

const Score ROOK_MOBILITIES[15] = {
 S( -56,-136), S( -18, -14), S(  -7,   5), S(   1,   3),
 S(  -3,  24), S(  -3,  33), S(  -6,  40), S(  -2,  41),
 S(   4,  42), S(   7,  46), S(  10,  48), S(  11,  51),
 S(  14,  55), S(  21,  56), S(  30,  50),};

const Score QUEEN_MOBILITIES[28] = {
 S(-333,-354), S(-152,-143), S(  -1,-217), S(  -9, -58),
 S(  -7,  27), S(  -4,  32), S(  -5,  56), S(  -2,  71),
 S(   0,  86), S(   3,  87), S(   5,  90), S(   6,  95),
 S(   7,  99), S(   9,  98), S(   9, 102), S(  12,  94),
 S(   9,  94), S(  10,  94), S(   6,  87), S(  14,  73),
 S(  24,  47), S(  38,  27), S(  51, -10), S(  41, -18),
 S(  54, -49), S( 183,-150), S(  88,-170), S( -59, -88),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(7, 18);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 4);

const Score BISHOP_TRAPPED = S(-152, -101);

const Score ROOK_TRAPPED = S(-53, -20);

const Score ROOK_OPEN_FILE = S(23, 14);

const Score ROOK_SEMI_OPEN = S(7, 10);

const Score DOUBLED_PAWN = S(-6, -27);

const Score OPPOSED_ISOLATED_PAWN = S(-6, -10);

const Score OPEN_ISOLATED_PAWN = S(-14, -13);

const Score BACKWARDS_PAWN = S(-9, -12);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  75,  37), S(  30,  27), S(  16,   9),
 S(   8,   2), S(   3,   1), S(   1,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -14, 125), S(  -5,  35),
 S( -13,  25), S( -19,   6), S( -29,   2), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  39, 140), S(  14, 185), S(   4,  98),
 S( -13,  50), S(  -7,  28), S(  -2,  24), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-5, -7);

const Score PASSED_PAWN_KING_PROXIMITY = S(-6, 20);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(23, 21);

const Score KNIGHT_THREATS[6] = { S(-1, 18), S(0, 5), S(37, 26), S(71, -5), S(42, -61), S(184, -1),};

const Score BISHOP_THREATS[6] = { S(4, 17), S(29, 35), S(10, 5), S(43, 7), S(46, 75), S(99, 83),};

const Score ROOK_THREATS[6] = { S(4, 24), S(30, 32), S(34, 40), S(-1, 19), S(75, 6), S(161, 14),};

const Score KING_THREATS[6] = { S(16, 27), S(24, 35), S(-3, 29), S(17, 22), S(-141, -278), S(0, 0),};

const Score PAWN_THREAT = S(57, 12);

const Score HANGING_THREAT = S(4, 11);

const Score PAWN_SHELTER[4][8] = {
 { S(-24, 3), S(40, 0), S(-32, 42), S(-6, 7), S(4, -7), S(24, -17), S(22, -34), S(0, 0),},
 { S(-40, 2), S(-42, 36), S(-9, 26), S(-25, 15), S(-15, 1), S(19, -13), S(24, -20), S(0, 0),},
 { S(-20, -7), S(-8, 24), S(-21, 24), S(-6, 4), S(-10, -2), S(-7, -2), S(23, -11), S(0, 0),},
 { S(-19, 4), S(9, 36), S(-51, 25), S(-19, 20), S(-12, 8), S(-9, 3), S(-9, 6), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-24, -18), S(-11, -9), S(-10, -11), S(-18, -2), S(-39, 29), S(-3, 125), S(141, 157), S(0, 0),},
 { S(-12, -8), S(2, -11), S(2, -6), S(-8, 3), S(-18, 22), S(-103, 144), S(27, 196), S(0, 0),},
 { S(13, -8), S(20, -17), S(16, -9), S(9, -2), S(-15, 23), S(-131, 128), S(-90, 177), S(0, 0),},
 { S(4, -2), S(7, -21), S(13, -10), S(7, -15), S(-14, 8), S(-43, 90), S(-85, 153), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-15, 12), S(52, -58), S(16, -33), S(15, -26), S(4, -18), S(-34, -17), S(0, 0), S(0, 0),
};

const Score KS_KING_FILE[4] = { S(11, -2), S(6, 0), S(0, 6), S(-31, -1),};

const Score KS_ATTACKER_WEIGHTS[5] = {0, 28, 18, 15, 4};

const Score KS_ATTACK = 24;

const Score KS_WEAK_SQS = 63;

const Score KS_SAFE_CHECK = 200;

const Score KS_UNSAFE_CHECK = 51;

const Score KS_ENEMY_QUEEN = 300;

const Score KS_KNIGHT_DEFENSE = 20;

const Score TEMPO = 20;
// clang-format on

Score PSQT[12][64];
Score KNIGHT_POSTS[2][64];
Score BISHOP_POSTS[2][64];

void InitPSQT() {
  for (int sq = 0; sq < 64; sq++) {
    PSQT[PAWN_WHITE][sq] = PSQT[PAWN_BLACK][MIRROR[sq]] = PAWN_PSQT[psqtIdx(sq)] + MATERIAL_VALUES[PAWN_TYPE];
    PSQT[KNIGHT_WHITE][sq] = PSQT[KNIGHT_BLACK][MIRROR[sq]] = KNIGHT_PSQT[psqtIdx(sq)] + MATERIAL_VALUES[KNIGHT_TYPE];
    PSQT[BISHOP_WHITE][sq] = PSQT[BISHOP_BLACK][MIRROR[sq]] = BISHOP_PSQT[psqtIdx(sq)] + MATERIAL_VALUES[BISHOP_TYPE];
    PSQT[ROOK_WHITE][sq] = PSQT[ROOK_BLACK][MIRROR[sq]] = ROOK_PSQT[psqtIdx(sq)] + MATERIAL_VALUES[ROOK_TYPE];
    PSQT[QUEEN_WHITE][sq] = PSQT[QUEEN_BLACK][MIRROR[sq]] = QUEEN_PSQT[psqtIdx(sq)] + MATERIAL_VALUES[QUEEN_TYPE];
    PSQT[KING_WHITE][sq] = PSQT[KING_BLACK][MIRROR[sq]] = KING_PSQT[psqtIdx(sq)];

    KNIGHT_POSTS[WHITE][sq] = KNIGHT_POSTS[BLACK][MIRROR[sq]] = KNIGHT_POST_PSQT[psqtIdx(sq)];
    BISHOP_POSTS[WHITE][sq] = BISHOP_POSTS[BLACK][MIRROR[sq]] = BISHOP_POST_PSQT[psqtIdx(sq)];
  }
}

// Get the phase of the game
inline int GetPhase(Board* board) {
  int phase = 0;
  for (int i = KNIGHT_WHITE; i <= QUEEN_BLACK; i++)
    phase += PHASE_MULTIPLIERS[PIECE_TYPE[i]] * bits(board->pieces[i]);

  phase = min(MAX_PHASE, phase);
  return (phase * 128 + MAX_PHASE / 2) / MAX_PHASE;
}

// Get a scalar for the given board for the stronger side (out of 100)
// two scalars are well defined as no mating material = 0
// opposite colored bishops + pawns = 50
// scale down the score quadratically based on strong sides remaining pawns
// i.e. no pawns = scalar of 52 / 100
inline int Scale(Board* board, int ss) {
  if (bits(board->occupancies[ss]) == 2 && (board->pieces[KNIGHT[ss]] | board->pieces[BISHOP[ss]]))
    return 0;

  if (IsOCB(board))
    return 64;

  int ssPawns = bits(board->pieces[PAWN[ss]]);
  if (ssPawns < 4)
    return MAX_SCALE - 4 * (4 - ssPawns) * (4 - ssPawns);

  return MAX_SCALE;
}

// preload a bunch of important evalution data
void InitEvalData(EvalData* data, Board* board) {
  BitBoard whitePawns = board->pieces[PAWN_WHITE];
  BitBoard blackPawns = board->pieces[PAWN_BLACK];
  BitBoard whitePawnAttacks = Shift(whitePawns, NE) | Shift(whitePawns, NW);
  BitBoard blackPawnAttacks = Shift(blackPawns, SE) | Shift(blackPawns, SW);

  data->allAttacks[WHITE] = data->attacks[WHITE][PAWN_TYPE] = whitePawnAttacks;
  data->allAttacks[BLACK] = data->attacks[BLACK][PAWN_TYPE] = blackPawnAttacks;
  data->twoAttacks[WHITE] = Shift(whitePawns, NE) & Shift(whitePawns, NW);
  data->twoAttacks[BLACK] = Shift(blackPawns, SE) & Shift(blackPawns, SW);

  data->outposts[WHITE] =
      ~Fill(blackPawnAttacks, S) & (whitePawnAttacks | Shift(whitePawns | blackPawns, S)) & (RANK_4 | RANK_5 | RANK_6);
  data->outposts[BLACK] =
      ~Fill(whitePawnAttacks, N) & (blackPawnAttacks | Shift(whitePawns | blackPawns, N)) & (RANK_5 | RANK_4 | RANK_3);

  BitBoard inTheWayWhitePawns = (Shift(board->occupancies[BOTH], S) | RANK_2 | RANK_3) & whitePawns;
  BitBoard inTheWayBlackPawns = (Shift(board->occupancies[BOTH], N) | RANK_7 | RANK_6) & blackPawns;

  data->mobilitySquares[WHITE] = ~(inTheWayWhitePawns | blackPawnAttacks);
  data->mobilitySquares[BLACK] = ~(inTheWayBlackPawns | whitePawnAttacks);

  data->kingSq[WHITE] = lsb(board->pieces[KING_WHITE]);
  data->kingSq[BLACK] = lsb(board->pieces[KING_BLACK]);

  data->kingArea[WHITE] = GetKingAttacks(data->kingSq[WHITE]);
  data->kingArea[BLACK] = GetKingAttacks(data->kingSq[BLACK]);
}

// Material + PSQT value
// TODO: Perhaps a/b lazy eval at this point? Would require eval saving to occur
// separate from search
Score MaterialValue(Board* board, int side) {
  Score s = 0;

  for (int pc = PAWN[side]; pc <= KING[side]; pc += 2) {
    BitBoard pieces = board->pieces[pc];

    if (T)
      C.pieces[side][PIECE_TYPE[pc]] = bits(pieces);

    while (pieces) {
      int sq = lsb(pieces);
      s += PSQT[pc][sq];

      popLsb(pieces);

      if (T)
        C.psqt[side][PIECE_TYPE[pc]][psqtIdx(side == WHITE ? sq : MIRROR[sq])]++;
    }
  }

  return s;
}

// Mobility + various piece specific mobility bonus
// Note: This method also builds up attack data critical for
// threat and king safety calculations
Score PieceEval(Board* board, EvalData* data, int side) {
  Score s = 0;

  int xside = side ^ 1;
  BitBoard enemyKingArea = data->kingArea[xside];
  BitBoard mob = data->mobilitySquares[side];
  BitBoard outposts = data->outposts[side];
  BitBoard myPawns = board->pieces[PAWN[side]];
  BitBoard opponentPawns = board->pieces[PAWN[xside]];

  // bishop pair bonus first
  if (bits(board->pieces[BISHOP[side]]) > 1) {
    s += BISHOP_PAIR;

    if (T)
      C.bishopPair[side] = 1;
  }

  for (int p = KNIGHT[side]; p <= KING[side]; p += 2) {
    BitBoard pieces = board->pieces[p];
    int pieceType = PIECE_TYPE[p];

    while (pieces) {
      BitBoard bb = pieces & -pieces;
      int sq = lsb(pieces);

      // Calculate a mobility bonus - bishops/rooks can see through queen/rook
      // because batteries shouldn't "limit" mobility
      // TODO: Consider queen behind rook acceptable?
      BitBoard movement = EMPTY;
      switch (pieceType) {
      case KNIGHT_TYPE:
        movement = GetKnightAttacks(sq);
        s += KNIGHT_MOBILITIES[bits(movement & mob)];

        if (T)
          C.knightMobilities[side][bits(movement & mob)]++;
        break;
      case BISHOP_TYPE:
        movement = GetBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]);
        s += BISHOP_MOBILITIES[bits(movement & mob)];

        if (T)
          C.bishopMobilities[side][bits(movement & mob)]++;
        break;
      case ROOK_TYPE:
        movement =
            GetRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[ROOK[side]] ^ board->pieces[QUEEN[side]]);
        s += ROOK_MOBILITIES[bits(movement & mob)];

        if (T)
          C.rookMobilities[side][bits(movement & mob)]++;
        break;
      case QUEEN_TYPE:
        movement = GetQueenAttacks(sq, board->occupancies[BOTH]);
        s += QUEEN_MOBILITIES[bits(movement & mob)];

        if (T)
          C.queenMobilities[side][bits(movement & mob)]++;
        break;
      case KING_TYPE:
        movement = GetKingAttacks(sq) & ~enemyKingArea;
      }

      // Update attack/king safety data
      data->twoAttacks[side] |= (movement & data->allAttacks[side]);
      data->attacks[side][pieceType] |= movement;
      data->allAttacks[side] |= movement;

      if (movement & enemyKingArea) {
        data->ksAttackWeight[side] += KS_ATTACKER_WEIGHTS[pieceType];
        data->ksSqAttackCount[side] += bits(movement & enemyKingArea);
        data->ksAttackerCount[side]++;
      }

      // Piece specific bonus'
      if (pieceType == KNIGHT_TYPE) {
        if (outposts & bb) {
          s += KNIGHT_POSTS[side][sq];

          if (T)
            C.knightPostPsqt[side][psqtIdx(side == WHITE ? sq : MIRROR[sq])]++;
        } else if (movement & outposts) {
          s += KNIGHT_OUTPOST_REACHABLE;

          if (T)
            C.knightPostReachable[side]++;
        }
      } else if (pieceType == BISHOP_TYPE) {
        if (outposts & bb) {
          s += BISHOP_POSTS[side][sq];

          if (T)
            C.bishopPostPsqt[side][psqtIdx(side == WHITE ? sq : MIRROR[sq])]++;
        } else if (movement & outposts) {
          s += BISHOP_OUTPOST_REACHABLE;

          if (T)
            C.bishopPostReachable[side]++;
        }

        if (side == WHITE) {
          if ((sq == A7 || sq == B8) && getBit(opponentPawns, B6) && getBit(opponentPawns, C7)) {
            s += BISHOP_TRAPPED;

            if (T)
              C.bishopTrapped[side]++;
          } else if ((sq == H7 || sq == G8) && getBit(opponentPawns, F7) && getBit(opponentPawns, G6)) {
            s += BISHOP_TRAPPED;

            if (T)
              C.bishopTrapped[side]++;
          }
        } else {
          if ((sq == A2 || sq == B1) && getBit(opponentPawns, B3) && getBit(opponentPawns, C2)) {
            s += BISHOP_TRAPPED;

            if (T)
              C.bishopTrapped[side]++;
          } else if ((sq == H2 || sq == G1) && getBit(opponentPawns, G3) && getBit(opponentPawns, F2)) {
            s += BISHOP_TRAPPED;

            if (T)
              C.bishopTrapped[side]++;
          }
        }
      } else if (pieceType == ROOK_TYPE) {
        if (!(FILE_MASKS[file(sq)] & myPawns)) {
          if (!(FILE_MASKS[file(sq)] & opponentPawns)) {
            s += ROOK_OPEN_FILE;

            if (T)
              C.rookOpenFile[side]++;
          } else {
            s += ROOK_SEMI_OPEN;

            if (T)
              C.rookSemiOpen[side]++;
          }
        }

        if (side == WHITE) {
          if ((sq == A1 || sq == A2 || sq == B1) && (data->kingSq[side] == C1 || data->kingSq[side] == B1)) {
            s += ROOK_TRAPPED;

            if (T)
              C.rookTrapped[side]++;
          } else if ((sq == H1 || sq == H2 || sq == G1) && (data->kingSq[side] == F1 || data->kingSq[side] == G1)) {
            s += ROOK_TRAPPED;

            if (T)
              C.rookTrapped[side]++;
          }
        } else {
          if ((sq == A8 || sq == A7 || sq == B8) && (data->kingSq[side] == B8 || data->kingSq[side] == C8)) {
            s += ROOK_TRAPPED;

            if (T)
              C.rookTrapped[side]++;
          } else if ((sq == H8 || sq == H7 || sq == G8) && (data->kingSq[side] == F8 || data->kingSq[side] == G8)) {
            s += ROOK_TRAPPED;

            if (T)
              C.rookTrapped[side]++;
          }
        }
      }

      popLsb(pieces);
    }
  }

  return s;
}

// Threats bonus (piece attacks piece)
// TODO: Make this vary more based on piece type
Score Threats(Board* board, EvalData* data, int side) {
  Score s = 0;

  int xside = side ^ 1;
  BitBoard weak = board->occupancies[xside] &
                  ~(data->attacks[xside][PAWN_TYPE] | (data->twoAttacks[xside] & ~data->twoAttacks[side])) &
                  data->allAttacks[side];

  for (int piece = KNIGHT_TYPE; piece <= KING_TYPE; piece++) {
    if (piece == QUEEN_TYPE)
      continue;

    BitBoard threats = weak & data->attacks[side][piece];
    while (threats) {
      int sq = lsb(threats);
      int attacked = PIECE_TYPE[board->squares[sq]];

      switch (piece) {
      case KNIGHT_TYPE:
        s += KNIGHT_THREATS[attacked];

        if (T)
          C.knightThreats[side][attacked]++;
        break;
      case BISHOP_TYPE:
        s += BISHOP_THREATS[attacked];

        if (T)
          C.bishopThreats[side][attacked]++;
        break;
      case ROOK_TYPE:
        s += ROOK_THREATS[attacked];

        if (T)
          C.rookThreats[side][attacked]++;
        break;
      case KING_TYPE:
        s += KING_THREATS[attacked];

        if (T)
          C.kingThreats[side][attacked]++;
        break;
      }

      popLsb(threats);
    }
  }

  BitBoard pawnThreats = board->occupancies[xside] & ~board->pieces[PAWN[xside]] & data->attacks[side][PAWN_TYPE];
  s += bits(pawnThreats) * PAWN_THREAT;

  BitBoard hangingPieces = board->occupancies[xside] & ~data->allAttacks[xside] & data->allAttacks[side];
  s += bits(hangingPieces) * HANGING_THREAT;

  if (T) {
    C.pawnThreat[side] += bits(pawnThreats);
    C.hangingThreat[side] += bits(hangingPieces);
  }

  return s;
}

// King safety is a quadratic based - there is a summation of smaller values
// into a single score, then that value is squared and diveded by 1000
// This fit for KS is strong as it can ignore a single piece attacking, but will
// spike on a secondary piece joining.
// It is heavily influenced by Toga, Rebel, SF, and Ethereal
Score KingSafety(Board* board, EvalData* data, int side) {
  Score s = S(0, 0);
  Score shelter = S(0, 0);

  int xside = side ^ 1;

  BitBoard ourPawns = board->pieces[PAWN[side]] & ~data->attacks[xside][PAWN_TYPE] &
                      ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];
  BitBoard opponentPawns = board->pieces[PAWN[xside]] & ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];

  // pawn shelter includes, pawns in front of king/enemy pawn storm (blocked/moving)
  for (int file = SHELTER_STORM_FILES[file(data->kingSq[side])][0];
       file <= SHELTER_STORM_FILES[file(data->kingSq[side])][1]; file++) {
    int adjustedFile = file > 3 ? 7 - file : file;

    BitBoard ourPawnFile = ourPawns & FILE_MASKS[file];
    int pawnRank = ourPawnFile ? (side ? 7 - rank(lsb(ourPawnFile)) : rank(msb(ourPawnFile))) : 0;
    shelter += PAWN_SHELTER[adjustedFile][pawnRank];
    if (T)
      C.pawnShelter[side][adjustedFile][pawnRank]++;

    BitBoard opponentPawnFile = opponentPawns & FILE_MASKS[file];
    int theirRank = opponentPawnFile ? (side ? 7 - rank(lsb(opponentPawnFile)) : rank(msb(opponentPawnFile))) : 0;
    if (pawnRank && pawnRank == theirRank + 1) {
      shelter += BLOCKED_PAWN_STORM[theirRank];

      if (T)
        C.blockedPawnStorm[side][theirRank]++;
    } else {
      shelter += PAWN_STORM[adjustedFile][theirRank];

      if (T)
        C.pawnStorm[side][adjustedFile][theirRank]++;
    }

    // the quality of the file the king is on is recorded
    if (file == file(data->kingSq[side])) {
      int idx = 2 * !(board->pieces[PAWN[side]] & FILE_MASKS[file]) + !(board->pieces[PAWN[xside]] & FILE_MASKS[file]);
      shelter += KS_KING_FILE[idx];

      if (T)
        C.kingFile[side][idx]++;
    }
  }

  BitBoard kingArea = data->kingArea[side];
  BitBoard weak = data->allAttacks[xside] & ~data->twoAttacks[side] &
                  (~data->allAttacks[side] | data->attacks[side][QUEEN_TYPE] | data->attacks[side][KING_TYPE]);
  BitBoard vulnerable = (~data->allAttacks[side] | (weak & data->twoAttacks[xside])) & ~board->occupancies[xside];

  BitBoard possibleKnightChecks =
      GetKnightAttacks(data->kingSq[side]) & data->attacks[xside][KNIGHT_TYPE] & ~board->occupancies[xside];
  BitBoard possibleBishopChecks = GetBishopAttacks(data->kingSq[side], board->occupancies[BOTH]) &
                                  data->attacks[xside][BISHOP_TYPE] & ~board->occupancies[xside];
  BitBoard possibleRookChecks = GetRookAttacks(data->kingSq[side], board->occupancies[BOTH]) &
                                data->attacks[xside][ROOK_TYPE] & ~board->occupancies[xside];
  BitBoard possibleQueenChecks = GetQueenAttacks(data->kingSq[side], board->occupancies[BOTH]) &
                                 data->attacks[xside][QUEEN_TYPE] & ~board->occupancies[xside];

  // all possible checks are used
  int safeChecks = bits(possibleKnightChecks & vulnerable) + bits(possibleBishopChecks & vulnerable) +
                   bits(possibleRookChecks & vulnerable) + bits(possibleQueenChecks & vulnerable);
  int unsafeChecks = bits(possibleKnightChecks & ~vulnerable) + bits(possibleBishopChecks & ~vulnerable) +
                     bits(possibleRookChecks & ~vulnerable);

  Score danger = data->ksAttackWeight[xside] * data->ksAttackerCount[xside] // Similar concept to toga's weight attack
                 + (KS_SAFE_CHECK * safeChecks)                             //
                 + (KS_UNSAFE_CHECK * unsafeChecks)                         //
                 + (KS_WEAK_SQS * bits(weak & kingArea))                    // weak sqs makes you vulnerable
                 + (KS_ATTACK * data->ksSqAttackCount[xside])               // general pieces aimed
                 - (KS_ENEMY_QUEEN * !board->pieces[QUEEN[xside]])          //
                 - (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT_TYPE] & kingArea)); // knight f8 = no m8

  // only include this if in danger
  if (danger > 0)
    s += S(-danger * danger / 1000, -danger / 30);

  // TODO: Utilize Texel tuning for these values
  if (T)
    C.ks[side] = s;

  s += shelter;
  return s;
}

// Main evalution method
Score Evaluate(Board* board, ThreadData* thread) {
  if (IsMaterialDraw(board))
    return 0;

  Score eval = UNKNOWN;
  if (bits(board->occupancies[BOTH]) == 3) {
    eval = EvaluateKXK(board);
  } else if (!(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK])) {
    eval = EvaluateMaterialOnlyEndgame(board);
  }

  // A specific endgame calculation returned a score
  if (eval != UNKNOWN)
    return eval;

  EvalData data = {0};
  InitEvalData(&data, board);

  Score s;
  if (!T) {
    s = board->side == WHITE ? board->mat : -board->mat;
  } else {
    s = MaterialValue(board, WHITE) - MaterialValue(board, BLACK);
  }

  s += PieceEval(board, &data, WHITE) - PieceEval(board, &data, BLACK);

  if (!T) {
    PawnHashEntry* pawnEntry = TTPawnProbe(board->pawnHash, thread);
    if (pawnEntry != NULL) {
      s += pawnEntry->s;
      data.passedPawns = pawnEntry->passedPawns;
    } else {
      Score pawnS = PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);
      TTPawnPut(board->pawnHash, pawnS, data.passedPawns, thread);
      s += pawnS;
    }
  } else {
    s += PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);
  }

  s += PasserEval(board, &data, WHITE) - PasserEval(board, &data, BLACK);
  s += Threats(board, &data, WHITE) - Threats(board, &data, BLACK);
  s += KingSafety(board, &data, WHITE) - KingSafety(board, &data, BLACK);

  // taper
  int phase = GetPhase(board);
  Score res = (phase * scoreMG(s) + (128 - phase) * scoreEG(s)) / 128;

  if (T)
    C.ss = res >= 0 ? WHITE : BLACK;

  // scale the score
  res = (res * Scale(board, res >= 0 ? WHITE : BLACK) + MAX_SCALE / 2) / MAX_SCALE;
  return TEMPO + (board->side == WHITE ? res : -res);
}

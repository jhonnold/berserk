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
const int cs[2] = {1, -1};

#define S(mg, eg) (makeScore((mg), (eg)))

const int MAX_PHASE = 24;
const int PHASE_MULTIPLIERS[5] = {0, 1, 1, 2, 4};
const int MAX_SCALE = 128;

const int STATIC_MATERIAL_VALUE[7] = {100, 565, 565, 705, 1000, 30000, 0};
const int SHELTER_STORM_FILES[8][2] = {{0, 2}, {0, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6}, {5, 7}, {5, 7}};

// clang-format off
const Score MATERIAL_VALUES[7] = { S(77, 166), S(366, 423), S(394, 441), S(575, 873), S(1196, 1648), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(29, 89);

const Score PAWN_PSQT[32] = {
 S(   0,  -1), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  79, 196), S(  54, 215), S(  30, 206), S(  67, 178),
 S(   8,  40), S(  -3,  50), S(  31,  14), S(  44, -20),
 S( -11,  14), S(  -7,  -2), S(  -5, -10), S(  11, -29),
 S( -20,  -5), S( -18,  -7), S(  -7, -15), S(  10, -23),
 S( -24, -12), S( -17, -15), S( -10, -12), S(   1,  -9),
 S( -17,  -3), S(  -2,  -7), S(  -4,  -4), S(   6,   6),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S(-141, -26), S(-106,  41), S(-100,  58), S( -35,  57),
 S( -11,  35), S(  11,  60), S(  45,  52), S(  45,  61),
 S(  45,  30), S(  46,  47), S(  58,  62), S(  60,  68),
 S(  36,  64), S(  45,  68), S(  50,  79), S(  44,  87),
 S(  43,  67), S(  28,  69), S(  60,  80), S(  49,  92),
 S(  19,  57), S(  35,  55), S(  38,  67), S(  44,  86),
 S(  28,  61), S(  29,  57), S(  27,  65), S(  31,  68),
 S(  -8,  72), S(  18,  58), S(  28,  62), S(  30,  74),
};

const Score BISHOP_PSQT[32] = {
 S( -36,  48), S(  -1,  75), S( -65,  78), S( -78,  83),
 S( -16,  66), S( -29,  60), S(  14,  73), S(  23,  67),
 S(  42,  65), S(  24,  73), S(   8,  56), S(  39,  71),
 S(   5,  77), S(  46,  71), S(  39,  77), S(  39,  89),
 S(  53,  51), S(  37,  73), S(  44,  83), S(  46,  81),
 S(  32,  63), S(  51,  74), S(  31,  65), S(  34,  89),
 S(  39,  61), S(  36,  42), S(  44,  64), S(  31,  77),
 S(  24,  36), S(  42,  71), S(  25,  80), S(  33,  73),
};

const Score ROOK_PSQT[32] = {
 S(  20,  92), S(  -6, 106), S( -20, 112), S( -25, 110),
 S(  12,  79), S(   2,  93), S(  13,  92), S(  22,  80),
 S( -10,  82), S(  35,  78), S(  19,  77), S(  27,  72),
 S(   2,  88), S(  31,  78), S(  34,  77), S(  39,  71),
 S( -10,  84), S(  11,  82), S(   8,  80), S(  24,  75),
 S(  -8,  72), S(  13,  65), S(  11,  67), S(  17,  70),
 S(  -8,  72), S(  15,  61), S(  16,  64), S(  22,  64),
 S(   8,  69), S(   7,  71), S(  10,  69), S(  20,  62),
};

const Score QUEEN_PSQT[32] = {
 S(  19, 108), S(  59,  64), S(  50,  99), S(  56, 112),
 S(   2, 108), S( -28, 129), S( -15, 163), S( -34, 178),
 S(  14,  88), S(  13,  90), S(   7, 118), S(  11, 141),
 S(   2, 132), S(  15, 147), S(  14, 132), S(  -2, 184),
 S(  26, 101), S(  25, 141), S(  15, 148), S(  -3, 191),
 S(  13,  92), S(  24, 111), S(  16, 129), S(   7, 147),
 S(  24,  55), S(  26,  62), S(  28,  73), S(  25,  98),
 S(  11,  71), S(   5,  71), S(   7,  69), S(  17,  79),
};

const Score KING_PSQT[32] = {
 S( 180,-119), S(  70, -16), S(  53, -11), S(  47, -24),
 S( -12,  18), S(  41,  54), S(  18,  54), S(  71,  19),
 S(  42,  16), S( 117,  49), S( 103,  51), S(   4,  53),
 S( -28,  19), S(  50,  43), S( -13,  58), S( -73,  56),
 S( -56,   6), S( -18,  30), S( -10,  34), S( -78,  43),
 S(  -5,  -7), S(  16,  13), S(  -8,  17), S( -36,  18),
 S(  10, -10), S(  -3,  16), S( -33,  11), S( -61,   5),
 S(  14, -60), S(  16, -17), S(  -3, -21), S( -15, -46),
};

const Score KNIGHT_POST_PSQT[12] = {
 S( -45,  36), S(   5,  39), S(  32,  29), S(  50,  39),
 S(  18,  17), S(  40,  26), S(  37,  35), S(  46,  40),
 S(  17,   7), S(  42,  19), S(  16,  28), S(  26,  30),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -7,  34), S(  26,  19), S(  57,  32), S(  54,  10),
 S(   5,  13), S(  21,  22), S(  32,  14), S(  46,  15),
 S( -22,  41), S(  40,  14), S(  26,  16), S(  39,  28),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -42, -66), S(  -5,  14), S(  15,  56), S(  28,  71),
 S(  40,  82), S(  48,  93), S(  58,  94), S(  67,  94),
 S(  76,  83),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -8,  -9), S(  11,  25), S(  25,  50), S(  32,  74),
 S(  40,  84), S(  46,  95), S(  48, 101), S(  52, 106),
 S(  53, 109), S(  55, 112), S(  63, 107), S(  78, 105),
 S(  77, 111), S(  80, 106),};

const Score ROOK_MOBILITIES[15] = {
 S( -31,-174), S( -16,  -7), S(  -1,  25), S(  10,  28),
 S(   7,  55), S(   9,  67), S(   4,  78), S(   8,  80),
 S(  15,  83), S(  21,  88), S(  24,  92), S(  23,  97),
 S(  25, 103), S(  37, 105), S(  53,  99),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1379,-1423), S( -58,-555), S(  10,-202), S(   0,  15),
 S(   4,  98), S(   9, 116), S(   6, 153), S(   7, 182),
 S(   9, 202), S(  12, 207), S(  14, 216), S(  17, 217),
 S(  17, 221), S(  20, 219), S(  20, 220), S(  27, 209),
 S(  26, 203), S(  27, 197), S(  33, 176), S(  59, 142),
 S(  80, 101), S(  91,  62), S( 132,  20), S( 175, -63),
 S( 194, -93), S( 346,-242), S( 272,-257), S( 211,-298),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(11, 22);

const Score BISHOP_OUTPOST_REACHABLE = S(8, 9);

const Score BISHOP_TRAPPED = S(-140, -213);

const Score ROOK_TRAPPED = S(-63, -23);

const Score BAD_BISHOP_PAWNS = S(-1, -6);

const Score DRAGON_BISHOP = S(28, 19);

const Score ROOK_OPEN_FILE = S(29, 18);

const Score ROOK_SEMI_OPEN = S(7, 14);

const Score DEFENDED_PAWN = S(13, 10);

const Score DOUBLED_PAWN = S(10, -31);

const Score OPPOSED_ISOLATED_PAWN = S(-5, -10);

const Score OPEN_ISOLATED_PAWN = S(-11, -18);

const Score BACKWARDS_PAWN = S(-12, -17);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  72,  53), S(  28,  37), S(  13,  12),
 S(   8,   2), S(   6,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -25, 268), S(  11,  69),
 S( -11,  44), S( -25,  19), S( -34,   4), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  62, 213), S(  -4, 269), S(  -6, 143),
 S( -22,  74), S(  -5,  45), S(  -5,  40), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(0, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 27);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(29, 32);

const Score KNIGHT_THREATS[6] = { S(0, 20), S(-1, 2), S(42, 38), S(97, 5), S(53, -60), S(264, 8),};

const Score BISHOP_THREATS[6] = { S(0, 19), S(34, 38), S(12, 26), S(62, 14), S(69, 58), S(125, 103),};

const Score ROOK_THREATS[6] = { S(3, 29), S(41, 42), S(44, 50), S(10, 14), S(86, 8), S(270, -17),};

const Score KING_THREAT = S(-22, 37);

const Score PAWN_THREAT = S(75, 26);

const Score PAWN_PUSH_THREAT = S(23, 2);

const Score HANGING_THREAT = S(7, 11);

const Score SPACE[17] = {
 S(   0,   0), S(   0,   0), S( 256,  -8), S( 131, -12), S(  53,  -8),
 S(  17,  -4), S(   1,   0), S(  -8,   2), S( -12,   5), S( -12,   7),
 S( -11,   9), S(  -8,  13), S(  -5,  14), S(  -2,  20), S(   2,  21),
 S(   7,  13), S(  12,-197),};

const Score PAWN_SHELTER[4][8] = {
 { S(-23, 5), S(45, 54), S(-12, 58), S(-12, 12), S(4, 0), S(32, -19), S(28, -42), S(0, 0),},
 { S(-53, 1), S(19, 45), S(-30, 39), S(-38, 19), S(-31, 6), S(22, -16), S(30, -28), S(0, 0),},
 { S(-27, -7), S(36, 57), S(-46, 38), S(-11, 10), S(-17, 1), S(-6, -1), S(29, -13), S(0, 0),},
 { S(-30, 5), S(26, 65), S(-75, 38), S(-25, 25), S(-23, 16), S(-13, 6), S(-14, 5), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-33, -23), S(-22, -9), S(-19, -11), S(-34, 6), S(-66, 51), S(6, 174), S(252, 206), S(0, 0),},
 { S(-13, -11), S(2, -10), S(2, -4), S(-11, 11), S(-32, 38), S(-131, 203), S(149, 279), S(0, 0),},
 { S(24, -15), S(27, -18), S(23, -10), S(7, 4), S(-19, 34), S(-137, 169), S(-76, 241), S(0, 0),},
 { S(-2, -2), S(5, -10), S(18, -7), S(5, -8), S(-25, 17), S(-52, 120), S(-99, 226), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-32, -8), S(52, -72), S(22, -41), S(17, -32), S(-2, -16), S(-23, -37), S(0, 0), S(0, 0),
};

const Score KS_ATTACKER_WEIGHTS[5] = {0, 29, 19, 16, 5};

const Score KS_ATTACK = 25;

const Score KS_WEAK_SQS = 65;

const Score KS_PINNED = 32;

const Score KS_SAFE_CHECK = 204;

const Score KS_UNSAFE_CHECK = 52;

const Score KS_ENEMY_QUEEN = -307;

const Score KS_KNIGHT_DEFENSE = -21;

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

    if (sq >= A6 && sq <= H4) {
      KNIGHT_POSTS[WHITE][sq] = KNIGHT_POSTS[BLACK][MIRROR[sq]] = KNIGHT_POST_PSQT[psqtIdx(sq) - 8];
      BISHOP_POSTS[WHITE][sq] = BISHOP_POSTS[BLACK][MIRROR[sq]] = BISHOP_POST_PSQT[psqtIdx(sq) - 8];
    }
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
  BitBoard whitePawnAttacks = ShiftNE(whitePawns) | ShiftNW(whitePawns);
  BitBoard blackPawnAttacks = ShiftSE(blackPawns) | ShiftSW(blackPawns);

  data->allAttacks[WHITE] = data->attacks[WHITE][PAWN_TYPE] = whitePawnAttacks;
  data->allAttacks[BLACK] = data->attacks[BLACK][PAWN_TYPE] = blackPawnAttacks;
  data->twoAttacks[WHITE] = ShiftNE(whitePawns) & ShiftNW(whitePawns);
  data->twoAttacks[BLACK] = ShiftSE(blackPawns) & ShiftSW(blackPawns);

  data->outposts[WHITE] =
      ~Fill(blackPawnAttacks, S) & (whitePawnAttacks | ShiftS(whitePawns | blackPawns)) & (RANK_4 | RANK_5 | RANK_6);
  data->outposts[BLACK] =
      ~Fill(whitePawnAttacks, N) & (blackPawnAttacks | ShiftN(whitePawns | blackPawns)) & (RANK_5 | RANK_4 | RANK_3);

  BitBoard inTheWayWhitePawns = (ShiftS(board->occupancies[BOTH]) | RANK_2 | RANK_3) & whitePawns;
  BitBoard inTheWayBlackPawns = (ShiftN(board->occupancies[BOTH]) | RANK_7 | RANK_6) & blackPawns;

  data->mobilitySquares[WHITE] = ~(inTheWayWhitePawns | blackPawnAttacks);
  data->mobilitySquares[BLACK] = ~(inTheWayBlackPawns | whitePawnAttacks);

  data->kingSq[WHITE] = lsb(board->pieces[KING_WHITE]);
  data->kingSq[BLACK] = lsb(board->pieces[KING_BLACK]);

  int whiteKingF = max(1, min(6, file(data->kingSq[WHITE])));
  int whiteKingR = max(1, min(6, rank(data->kingSq[WHITE])));
  int blackKingF = max(1, min(6, file(data->kingSq[BLACK])));
  int blackKingR = max(1, min(6, rank(data->kingSq[BLACK])));

  data->kingArea[WHITE] = GetKingAttacks(sq(whiteKingR, whiteKingF)) | bit(sq(whiteKingR, whiteKingF));
  data->kingArea[BLACK] = GetKingAttacks(sq(blackKingR, blackKingF)) | bit(sq(blackKingR, blackKingF));
}

// Material + PSQT value
// TODO: Perhaps a/b lazy eval at this point? Would require eval saving to occur
// separate from search
Score MaterialValue(Board* board, int side) {
  Score s = 0;

  for (int pc = PAWN[side]; pc <= KING[side]; pc += 2) {
    BitBoard pieces = board->pieces[pc];

    if (T)
      C.pieces[PIECE_TYPE[pc]] += cs[side] * bits(pieces);

    while (pieces) {
      int sq = lsb(pieces);
      s += PSQT[pc][sq];

      popLsb(pieces);

      if (T)
        C.psqt[PIECE_TYPE[pc]][psqtIdx(side == WHITE ? sq : MIRROR[sq])] += cs[side];
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
      C.bishopPair += cs[side];
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
          C.knightMobilities[bits(movement & mob)] += cs[side];
        break;
      case BISHOP_TYPE:
        movement = GetBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]);
        s += BISHOP_MOBILITIES[bits(movement & mob)];

        if (T)
          C.bishopMobilities[bits(movement & mob)] += cs[side];
        break;
      case ROOK_TYPE:
        movement =
            GetRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[ROOK[side]] ^ board->pieces[QUEEN[side]]);
        s += ROOK_MOBILITIES[bits(movement & mob)];

        if (T)
          C.rookMobilities[bits(movement & mob)] += cs[side];
        break;
      case QUEEN_TYPE:
        movement = GetQueenAttacks(sq, board->occupancies[BOTH]);
        s += QUEEN_MOBILITIES[bits(movement & mob)];

        if (T)
          C.queenMobilities[bits(movement & mob)] += cs[side];
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
            C.knightPostPsqt[psqtIdx(side == WHITE ? sq : MIRROR[sq]) - 8] += cs[side];
        } else if (movement & outposts) {
          s += KNIGHT_OUTPOST_REACHABLE;

          if (T)
            C.knightPostReachable += cs[side];
        }
      } else if (pieceType == BISHOP_TYPE) {
        BitBoard bishopSquares = (bb & DARK_SQS) ? DARK_SQS : ~DARK_SQS;
        BitBoard inTheWayPawns = board->pieces[PAWN[side]] & bishopSquares;
        BitBoard blockedInTheWayPawns =
            (side == WHITE ? ShiftS(board->occupancies[BOTH]) : ShiftN(board->occupancies[BOTH])) &
            board->pieces[PAWN[side]] & ~(A_FILE | B_FILE | G_FILE | H_FILE);

        int scalar = bits(inTheWayPawns) * bits(blockedInTheWayPawns);
        s += BAD_BISHOP_PAWNS * scalar;

        if (T)
          C.badBishopPawns += cs[side] * scalar;

        if (!(CENTER_SQS & bb) && bits(CENTER_SQS & GetBishopAttacks(sq, (myPawns | opponentPawns))) > 1) {
          s += DRAGON_BISHOP;

          if (T)
            C.dragonBishop += cs[side];
        }

        if (outposts & bb) {
          s += BISHOP_POSTS[side][sq];

          if (T)
            C.bishopPostPsqt[psqtIdx(side == WHITE ? sq : MIRROR[sq]) - 8] += cs[side];
        } else if (movement & outposts) {
          s += BISHOP_OUTPOST_REACHABLE;

          if (T)
            C.bishopPostReachable += cs[side];
        }

        if (side == WHITE) {
          if ((sq == A7 || sq == B8) && getBit(opponentPawns, B6) && getBit(opponentPawns, C7)) {
            s += BISHOP_TRAPPED;

            if (T)
              C.bishopTrapped += cs[side];
          } else if ((sq == H7 || sq == G8) && getBit(opponentPawns, F7) && getBit(opponentPawns, G6)) {
            s += BISHOP_TRAPPED;

            if (T)
              C.bishopTrapped += cs[side];
          }
        } else {
          if ((sq == A2 || sq == B1) && getBit(opponentPawns, B3) && getBit(opponentPawns, C2)) {
            s += BISHOP_TRAPPED;

            if (T)
              C.bishopTrapped += cs[side];
          } else if ((sq == H2 || sq == G1) && getBit(opponentPawns, G3) && getBit(opponentPawns, F2)) {
            s += BISHOP_TRAPPED;

            if (T)
              C.bishopTrapped += cs[side];
          }
        }
      } else if (pieceType == ROOK_TYPE) {
        if (!(FILE_MASKS[file(sq)] & myPawns)) {
          if (!(FILE_MASKS[file(sq)] & opponentPawns)) {
            s += ROOK_OPEN_FILE;

            if (T)
              C.rookOpenFile += cs[side];
          } else {
            s += ROOK_SEMI_OPEN;

            if (T)
              C.rookSemiOpen += cs[side];
          }
        }

        if (side == WHITE) {
          if ((sq == A1 || sq == A2 || sq == B1) && (data->kingSq[side] == C1 || data->kingSq[side] == B1)) {
            s += ROOK_TRAPPED;

            if (T)
              C.rookTrapped += cs[side];
          } else if ((sq == H1 || sq == H2 || sq == G1) && (data->kingSq[side] == F1 || data->kingSq[side] == G1)) {
            s += ROOK_TRAPPED;

            if (T)
              C.rookTrapped += cs[side];
          }
        } else {
          if ((sq == A8 || sq == A7 || sq == B8) && (data->kingSq[side] == B8 || data->kingSq[side] == C8)) {
            s += ROOK_TRAPPED;

            if (T)
              C.rookTrapped += cs[side];
          } else if ((sq == H8 || sq == H7 || sq == G8) && (data->kingSq[side] == F8 || data->kingSq[side] == G8)) {
            s += ROOK_TRAPPED;

            if (T)
              C.rookTrapped += cs[side];
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

  for (int piece = KNIGHT_TYPE; piece <= ROOK_TYPE; piece++) {
    BitBoard threats = weak & data->attacks[side][piece];
    while (threats) {
      int sq = lsb(threats);
      int attacked = PIECE_TYPE[board->squares[sq]];

      switch (piece) {
      case KNIGHT_TYPE:
        s += KNIGHT_THREATS[attacked];

        if (T)
          C.knightThreats[attacked] += cs[side];
        break;
      case BISHOP_TYPE:
        s += BISHOP_THREATS[attacked];

        if (T)
          C.bishopThreats[attacked] += cs[side];
        break;
      case ROOK_TYPE:
        s += ROOK_THREATS[attacked];

        if (T)
          C.rookThreats[attacked] += cs[side];
        break;
      }

      popLsb(threats);
    }
  }

  BitBoard kingThreats = weak & data->attacks[side][KING_TYPE];
  if (kingThreats) {
    s += KING_THREAT;

    if (T)
      C.kingThreat += cs[side];
  }

  BitBoard pawnThreats = board->occupancies[xside] & ~board->pieces[PAWN[xside]] & data->attacks[side][PAWN_TYPE];
  s += bits(pawnThreats) * PAWN_THREAT;

  BitBoard hangingPieces = board->occupancies[xside] & ~data->allAttacks[xside] & data->allAttacks[side];
  s += bits(hangingPieces) * HANGING_THREAT;

  BitBoard pawnPushes = ~board->occupancies[BOTH] &
                        (side == WHITE ? ShiftN(board->pieces[PAWN_WHITE]) : ShiftS(board->pieces[PAWN_BLACK]));
  pawnPushes |= ~board->occupancies[BOTH] & (side == WHITE ? ShiftN(pawnPushes & RANK_3) : ShiftS(pawnPushes & RANK_6));
  BitBoard pawnPushAttacks =
      (side == WHITE ? ShiftNE(pawnPushes) | ShiftNW(pawnPushes) : ShiftSE(pawnPushes) | ShiftSW(pawnPushes));
  pawnPushAttacks &= (board->occupancies[xside] & ~board->pieces[PAWN[xside]]);

  s += bits(pawnPushAttacks) * PAWN_PUSH_THREAT;

  if (T) {
    C.pawnThreat += cs[side] * bits(pawnThreats);
    C.hangingThreat += cs[side] * bits(hangingPieces);
    C.pawnPushThreat += cs[side] * bits(pawnPushAttacks);
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
      C.pawnShelter[adjustedFile][pawnRank] += cs[side];

    BitBoard opponentPawnFile = opponentPawns & FILE_MASKS[file];
    int theirRank = opponentPawnFile ? (side ? 7 - rank(lsb(opponentPawnFile)) : rank(msb(opponentPawnFile))) : 0;
    if (pawnRank && pawnRank == theirRank + 1) {
      shelter += BLOCKED_PAWN_STORM[theirRank];

      if (T)
        C.blockedPawnStorm[theirRank] += cs[side];
    } else {
      shelter += PAWN_STORM[adjustedFile][theirRank];

      if (T)
        C.pawnStorm[adjustedFile][theirRank] += cs[side];
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
                 + (KS_PINNED * bits(board->pinned & board->occupancies[side]))           //
                 + (KS_ENEMY_QUEEN * !board->pieces[QUEEN[xside]])                        //
                 + (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT_TYPE] & kingArea)); // knight f8 = no m8

  // only include this if in danger
  if (danger > 0)
    s += S(-danger * danger / 1024, -danger / 32);

  // TODO: Utilize Texel tuning for these values
  if (T)
    C.ks += s * cs[side];

  s += shelter;
  return s;
}

Score Space(Board* board, EvalData* data, int side) {
  Score s = S(0, 0);
  int xside = side ^ 1;

  BitBoard space = side == WHITE ? ShiftS(board->pieces[PAWN[side]]) : ShiftN(board->pieces[PAWN[side]]);
  space |= side == WHITE ? (ShiftS(space) | ShiftSS(space)) : (ShiftN(space) | ShiftNN(space));

  s +=
      SPACE[bits(board->occupancies[side])] *
      bits(space & ~board->pieces[PAWN[side]] & ~data->attacks[xside][PAWN_TYPE] & (C_FILE | D_FILE | E_FILE | F_FILE));

  if (T)
    C.space[bits(board->occupancies[side])] +=
        cs[side] * bits(space & ~board->pieces[PAWN[side]] & ~data->attacks[xside][PAWN_TYPE] &
                        (C_FILE | D_FILE | E_FILE | F_FILE));

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
  s += Space(board, &data, WHITE) - Space(board, &data, BLACK);

  // taper
  int phase = GetPhase(board);
  Score res = (phase * scoreMG(s) + (128 - phase) * scoreEG(s)) / 128;

  if (T)
    C.ss = res >= 0 ? WHITE : BLACK;

  // scale the score
  res = (res * Scale(board, res >= 0 ? WHITE : BLACK) + MAX_SCALE / 2) / MAX_SCALE;
  return TEMPO + (board->side == WHITE ? res : -res);
}

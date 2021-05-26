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

const int STATIC_MATERIAL_VALUE[7] = {100, 325, 325, 550, 975, 30000, 0};
const int SHELTER_STORM_FILES[8][2] = {{0, 2}, {0, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6}, {5, 7}, {5, 7}};

// clang-format off
const Score MATERIAL_VALUES[7] = { S(79, 139), S(351, 374), S(366, 383), S(531, 706), S(1075, 1360), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(25, 62);

const Score PAWN_PSQT[32] = {
 S(   0,  -1), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  63, 151), S(  16, 174), S(  29, 148), S(  54, 122),
 S(  -1,  21), S( -11,  23), S(  14,   0), S(  31, -29),
 S( -14,  -1), S(  -6, -13), S(  -6, -18), S(   6, -29),
 S( -18, -15), S( -17, -16), S(  -4, -24), S(   7, -26),
 S( -20, -20), S( -12, -22), S(  -7, -21), S(  -1, -18),
 S( -15, -14), S(   1, -18), S(  -4, -15), S(   1, -10),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S(-141, -38), S( -87,   1), S( -68,  16), S( -23,  17),
 S( -23,   0), S( -16,  26), S(  34,  17), S(  23,  26),
 S(  31,  -9), S(  24,  15), S(  15,  37), S(  41,  28),
 S(  29,  22), S(  28,  30), S(  38,  34), S(  38,  42),
 S(  31,  29), S(  41,  23), S(  44,  37), S(  41,  46),
 S(   0,  26), S(  16,  24), S(  18,  32), S(  24,  46),
 S(  11,  36), S(  14,  23), S(  10,  28), S(  15,  33),
 S( -15,  37), S(   3,  23), S(   8,  30), S(  14,  38),
};

const Score BISHOP_PSQT[32] = {
 S( -52,  21), S(  -7,  40), S( -43,  45), S( -76,  52),
 S( -10,  32), S( -34,  30), S(   5,  44), S(   9,  41),
 S(  35,  30), S(  11,  41), S(  -6,  30), S(  24,  39),
 S(   2,  46), S(  39,  37), S(  30,  43), S(  33,  52),
 S(  43,  25), S(  28,  37), S(  35,  48), S(  39,  44),
 S(  21,  33), S(  38,  42), S(  20,  36), S(  27,  54),
 S(  31,  28), S(  26,  19), S(  33,  33), S(  19,  47),
 S(  15,  21), S(  33,  37), S(  17,  46), S(  21,  45),
};

const Score ROOK_PSQT[32] = {
 S(  -2,  58), S( -13,  62), S( -27,  73), S( -34,  73),
 S(   1,  46), S(  -8,  59), S(   9,  54), S(  12,  47),
 S(  -7,  44), S(  34,  38), S(  25,  42), S(  29,  33),
 S( -11,  51), S(  11,  44), S(  23,  43), S(  16,  40),
 S( -14,  49), S(   9,  41), S(   0,  48), S(   4,  46),
 S( -14,  43), S(   8,  36), S(   8,  38), S(   5,  40),
 S( -17,  50), S(   1,  37), S(   6,  37), S(   9,  39),
 S(  -2,  40), S(  -2,  41), S(   0,  42), S(   6,  35),
};

const Score QUEEN_PSQT[32] = {
 S(   6,  36), S(   8,  45), S(  42,  42), S(  33,  62),
 S( -14,  48), S( -41,  81), S( -18, 110), S( -16,  93),
 S(  22,  32), S(  15,  66), S(   0,  91), S(  10,  95),
 S(   9,  82), S(  12, 101), S(   8,  93), S( -10, 122),
 S(  14,  72), S(  17,  95), S(  10,  99), S(  -3, 122),
 S(   4,  62), S(  12,  68), S(   6,  90), S(   1,  88),
 S(   6,  41), S(  12,  26), S(  14,  43), S(  10,  60),
 S(  13,  22), S(  -2,  39), S(  -5,  42), S(   4,  34),
};

const Score KING_PSQT[32] = {
 S( -81, -95), S(  99, -31), S(  19,   5), S( -27,  -9),
 S( -13,   4), S(  96,  41), S(  93,  40), S(  70,  24),
 S(  -8,  11), S( 132,  37), S( 114,  37), S(   8,  38),
 S( -43,   8), S(  93,  26), S(   8,  41), S( -31,  42),
 S( -95,   8), S(  19,  18), S(  26,  20), S( -23,  26),
 S( -15,  -5), S(  42,   8), S(  22,   9), S(  -1,  12),
 S(   5, -10), S(  10,  13), S( -13,   9), S( -43,   6),
 S(   1, -47), S(   7, -11), S(  -4, -16), S( -18, -37),
};

const Score KNIGHT_POST_PSQT[12] = {
 S( -29,  34), S(  14,  28), S(  42,  14), S(  34,  37),
 S(  14,  14), S(  36,  18), S(  28,  30), S(  44,  26),
 S(  13,   2), S(  25,  21), S(  13,  21), S(  14,  30),
};

const Score BISHOP_POST_PSQT[12] = {
 S(   5,  20), S(  30,  16), S(  60,  20), S(  52,   1),
 S(  11,  14), S(  17,  20), S(  28,   9), S(  34,   7),
 S( -15,  27), S(  27,  15), S(  19,   8), S(  31,  18),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -39, -70), S( -12, -13), S(   4,  18), S(  14,  30),
 S(  24,  40), S(  31,  47), S(  38,  49), S(  46,  47),
 S(  53,  39),};

const Score BISHOP_MOBILITIES[14] = {
 S( -17, -28), S(  -3,  -1), S(  11,  15), S(  17,  35),
 S(  25,  43), S(  31,  49), S(  33,  54), S(  35,  59),
 S(  37,  60), S(  38,  63), S(  45,  58), S(  63,  58),
 S(  71,  64), S(  61,  65),};

const Score ROOK_MOBILITIES[15] = {
 S( -64,-123), S( -19, -14), S(  -7,   7), S(   2,   8),
 S(  -1,  29), S(  -1,  38), S(  -5,  46), S(  -1,  47),
 S(   5,  48), S(   9,  53), S(  13,  55), S(  13,  58),
 S(  17,  61), S(  24,  63), S(  36,  57),};

const Score QUEEN_MOBILITIES[28] = {
 S(-647,-669), S(-162, -56), S(  -1,-218), S( -10, -47),
 S(  -7,  39), S(  -3,  43), S(  -5,  67), S(  -2,  86),
 S(   0,  99), S(   3, 101), S(   5, 105), S(   6, 110),
 S(   6, 114), S(   9, 113), S(  10, 117), S(  13, 108),
 S(  10, 107), S(  11, 108), S(   4, 101), S(  17,  84),
 S(  28,  56), S(  41,  37), S(  56,  -3), S(  39,  -9),
 S(  58, -46), S( 196,-154), S( 287,-288), S(-322,  43),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(6, 17);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-162, -109);

const Score ROOK_TRAPPED = S(-55, -22);

const Score BAD_BISHOP_PAWNS = S(-1, -5);

const Score DRAGON_BISHOP = S(23, 14);

const Score ROOK_OPEN_FILE = S(23, 15);

const Score ROOK_SEMI_OPEN = S(6, 10);

const Score DEFENDED_PAWN = S(10, 8);

const Score DOUBLED_PAWN = S(4, -23);

const Score OPPOSED_ISOLATED_PAWN = S(-3, -8);

const Score OPEN_ISOLATED_PAWN = S(-11, -12);

const Score BACKWARDS_PAWN = S(-8, -14);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  80,  36), S(  27,  28), S(  12,   9),
 S(   7,   2), S(   5,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -55, 201), S(   2,  46),
 S( -12,  30), S( -17,  12), S( -30,   2), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  43, 155), S(   2, 212), S(   2, 111),
 S( -12,  58), S(  -7,  36), S(  -3,  30), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-4, -9);

const Score PASSED_PAWN_KING_PROXIMITY = S(-7, 21);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(19, 24);

const Score KNIGHT_THREATS[6] = { S(0, 16), S(5, 7), S(39, 25), S(78, -6), S(47, -69), S(206, -1),};

const Score BISHOP_THREATS[6] = { S(2, 14), S(28, 32), S(12, 13), S(46, 6), S(49, 69), S(111, 86),};

const Score ROOK_THREATS[6] = { S(3, 24), S(33, 34), S(36, 42), S(1, 22), S(76, 13), S(198, 6),};

const Score KING_THREATS[6] = { S(11, 27), S(26, 37), S(-6, 33), S(14, 24), S(-115, -518), S(0, 0),};

const Score PAWN_THREAT = S(66, 13);

const Score PAWN_PUSH_THREAT = S(18, 1);

const Score HANGING_THREAT = S(3, 11);

const Score PAWN_SHELTER[4][8] = {
 { S(-26, 5), S(52, 16), S(-24, 42), S(-8, 10), S(3, -5), S(23, -17), S(23, -35), S(0, 0),},
 { S(-40, 2), S(-37, 52), S(-1, 26), S(-27, 16), S(-15, 1), S(19, -15), S(26, -22), S(0, 0),},
 { S(-21, -6), S(2, 39), S(-16, 24), S(-7, 6), S(-11, 0), S(-8, -1), S(24, -12), S(0, 0),},
 { S(-20, 5), S(13, 54), S(-53, 28), S(-20, 22), S(-14, 10), S(-11, 6), S(-10, 8), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-27, -18), S(-13, -9), S(-12, -10), S(-21, 1), S(-44, 34), S(-1, 139), S(150, 173), S(0, 0),},
 { S(-13, -8), S(-1, -10), S(0, -5), S(-10, 4), S(-24, 26), S(-112, 155), S(34, 212), S(0, 0),},
 { S(13, -9), S(22, -17), S(17, -9), S(9, -1), S(-15, 24), S(-128, 135), S(-98, 190), S(0, 0),},
 { S(4, -3), S(9, -21), S(15, -10), S(7, -15), S(-15, 9), S(-42, 96), S(-93, 167), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-29, 0), S(46, -55), S(17, -34), S(17, -27), S(1, -17), S(-34, -24), S(0, 0), S(0, 0),
};

const Score KS_KING_FILE[4] = { S(11, -2), S(8, 1), S(0, 5), S(-31, -1),};

const Score KS_ATTACKER_WEIGHTS[5] = {0, 28, 18, 15, 4};

const Score KS_ATTACK = 24;

const Score KS_WEAK_SQS = 63;

const Score KS_PINNED = 31;

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
        BitBoard blockedInTheWayPawns = Shift(board->occupancies[BOTH], PAWN_DIRECTIONS[xside]) &
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
      case KING_TYPE:
        s += KING_THREATS[attacked];

        if (T)
          C.kingThreats[attacked] += cs[side];
        break;
      }

      popLsb(threats);
    }
  }

  BitBoard pawnThreats = board->occupancies[xside] & ~board->pieces[PAWN[xside]] & data->attacks[side][PAWN_TYPE];
  s += bits(pawnThreats) * PAWN_THREAT;

  BitBoard hangingPieces = board->occupancies[xside] & ~data->allAttacks[xside] & data->allAttacks[side];
  s += bits(hangingPieces) * HANGING_THREAT;

  BitBoard pawnPushes = ~board->occupancies[BOTH] & Shift(board->pieces[PAWN[side]], PAWN_DIRECTIONS[side]);
  pawnPushes |= ~board->occupancies[BOTH] & Shift(pawnPushes & THIRD_RANKS[side], PAWN_DIRECTIONS[side]);
  BitBoard pawnPushAttacks =
      (Shift(pawnPushes, PAWN_DIRECTIONS[side] + E) | Shift(pawnPushes, PAWN_DIRECTIONS[side] + W)) &
      (board->occupancies[xside] & ~board->pieces[PAWN[xside]]);
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

    // the quality of the file the king is on is recorded
    if (file == file(data->kingSq[side])) {
      int idx = 2 * !(board->pieces[PAWN[side]] & FILE_MASKS[file]) + !(board->pieces[PAWN[xside]] & FILE_MASKS[file]);
      shelter += KS_KING_FILE[idx];

      if (T)
        C.kingFile[idx] += cs[side];
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
                 - (KS_ENEMY_QUEEN * !board->pieces[QUEEN[xside]])                        //
                 - (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT_TYPE] & kingArea)); // knight f8 = no m8

  // only include this if in danger
  if (danger > 0)
    s += S(-danger * danger / 1000, -danger / 30);

  // TODO: Utilize Texel tuning for these values
  if (T)
    C.ks += s * cs[side];

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

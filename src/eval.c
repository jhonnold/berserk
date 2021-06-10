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
const Score MATERIAL_VALUES[7] = { S(89, 146), S(374, 386), S(394, 397), S(597, 746), S(1191, 1443), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(28, 68);

const Score PAWN_PSQT[32] = {
 S(   0,  -1), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  84, 163), S(  23, 189), S(  35, 162), S(  66, 133),
 S(   3,  27), S(  -6,  29), S(  25,   2), S(  40, -29),
 S( -10,   4), S(  -3,  -9), S(  -3, -16), S(   9, -28),
 S( -15, -12), S( -14, -14), S(  -1, -22), S(  11, -25),
 S( -19, -17), S( -12, -19), S(  -6, -18), S(   2, -15),
 S( -13, -10), S(   2, -14), S(  -4, -11), S(   2,  -6),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S(-152, -33), S( -76,   6), S( -92,  29), S( -34,  29),
 S( -28,  12), S(  -5,  35), S(  40,  27), S(  21,  40),
 S(  34,   0), S(  33,  26), S(  34,  47), S(  52,  39),
 S(  28,  37), S(  41,  41), S(  43,  48), S(  40,  56),
 S(  43,  39), S(  23,  42), S(  57,  49), S(  46,  62),
 S(  19,  34), S(  37,  32), S(  40,  41), S(  46,  56),
 S(  31,  44), S(  38,  30), S(  29,  37), S(  34,  41),
 S(   2,  48), S(  22,  31), S(  27,  38), S(  33,  46),
};

const Score BISHOP_PSQT[32] = {
 S( -51,  31), S(   6,  50), S( -32,  53), S( -71,  61),
 S(  -6,  41), S( -30,  40), S(   7,  56), S(  16,  51),
 S(  46,  40), S(  15,  52), S(   6,  39), S(  33,  49),
 S(   3,  58), S(  48,  46), S(  36,  54), S(  38,  63),
 S(  53,  34), S(  40,  47), S(  45,  59), S(  46,  56),
 S(  34,  42), S(  52,  51), S(  37,  43), S(  37,  63),
 S(  42,  37), S(  38,  26), S(  44,  42), S(  34,  55),
 S(  27,  28), S(  37,  49), S(  28,  55), S(  36,  52),
};

const Score ROOK_PSQT[32] = {
 S(  11,  68), S(  -3,  73), S( -32,  87), S( -31,  86),
 S(   6,  56), S(  -4,  70), S(  11,  66), S(  18,  58),
 S( -10,  56), S(  32,  50), S(  14,  56), S(  22,  47),
 S(  -6,  64), S(  21,  54), S(  29,  54), S(  28,  49),
 S(  -7,  60), S(  11,  54), S(   6,  59), S(  18,  54),
 S( -10,  54), S(   7,  49), S(  11,  49), S(  17,  48),
 S(  -8,  61), S(  12,  46), S(  14,  46), S(  23,  47),
 S(  10,  50), S(   9,  51), S(  11,  48), S(  20,  43),
};

const Score QUEEN_PSQT[32] = {
 S(   4,  56), S(  18,  63), S(  45,  49), S(  12,  85),
 S(  -9,  70), S( -37, 105), S( -23, 120), S( -14, 104),
 S(  14,  46), S(  -9,  92), S(  -1,  97), S(   7,  96),
 S(  -2, 103), S(  14, 117), S(  10, 103), S(  -8, 143),
 S(  25,  82), S(  19, 121), S(  19, 112), S(   3, 141),
 S(   8,  81), S(  25,  81), S(  14, 107), S(  12, 108),
 S(  20,  55), S(  25,  46), S(  27,  58), S(  24,  78),
 S(  28,  29), S(   9,  54), S(   9,  53), S(  19,  56),
};

const Score KING_PSQT[32] = {
 S( -45,-107), S( 149, -40), S(  83,   3), S(  40, -16),
 S( -26,   8), S(  60,  50), S(  79,  46), S(  35,  32),
 S(   7,  10), S( 106,  45), S( 112,  42), S( -14,  44),
 S( -19,   4), S(  75,  29), S(  -9,  45), S( -56,  47),
 S( -73,   4), S( -13,  23), S(   7,  23), S( -47,  30),
 S(   6, -10), S(  20,  10), S(   5,  10), S( -22,  14),
 S(   8, -11), S(  -4,  14), S( -29,  12), S( -61,   7),
 S(  18, -53), S(  14, -14), S(  -1, -16), S( -17, -40),
};

const Score KNIGHT_POST_PSQT[12] = {
 S( -49,  45), S(  14,  29), S(  39,  17), S(  38,  41),
 S(  29,  10), S(  41,  19), S(  42,  28), S(  44,  29),
 S(  18,   1), S(  58,  12), S(  17,  22), S(  22,  29),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -3,  26), S(  29,  19), S(  58,  22), S(  54,   1),
 S(  15,  13), S(  23,  21), S(  36,   7), S(  36,   7),
 S( -17,  30), S(  32,  16), S(  16,   9), S(  32,  18),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -28, -67), S(   0,  -7), S(  19,  27), S(  30,  41),
 S(  41,  51), S(  48,  58), S(  56,  61), S(  64,  58),
 S(  72,  48),};

const Score BISHOP_MOBILITIES[14] = {
 S(   2, -23), S(  16,   6), S(  30,  24), S(  36,  46),
 S(  44,  55), S(  50,  61), S(  51,  67), S(  53,  72),
 S(  54,  74), S(  54,  77), S(  62,  71), S(  82,  71),
 S(  84,  77), S(  67,  81),};

const Score ROOK_MOBILITIES[15] = {
 S( -60,-123), S( -10, -10), S(   2,  13), S(  11,  12),
 S(   8,  36), S(   9,  47), S(   4,  56), S(   8,  57),
 S(  14,  58), S(  18,  63), S(  20,  67), S(  17,  71),
 S(  20,  75), S(  29,  77), S(  48,  68),};

const Score QUEEN_MOBILITIES[28] = {
 S(-984,-1061), S(-151, -38), S(  11,-196), S(  -1, -12),
 S(   4,  74), S(   8,  83), S(   6, 104), S(   9, 123),
 S(  12, 137), S(  15, 139), S(  17, 144), S(  17, 148),
 S(  17, 152), S(  20, 150), S(  19, 158), S(  23, 146),
 S(  19, 144), S(  22, 141), S(  13, 134), S(  31, 112),
 S(  43,  82), S(  47,  70), S(  66,  22), S(  61,   3),
 S(  50, -34), S( 307,-188), S( 333,-318), S(-316,  32),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(7, 18);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 8);

const Score BISHOP_TRAPPED = S(-198, -107);

const Score ROOK_TRAPPED = S(-59, -24);

const Score BAD_BISHOP_PAWNS = S(-1, -5);

const Score DRAGON_BISHOP = S(25, 14);

const Score ROOK_OPEN_FILE = S(28, 14);

const Score ROOK_SEMI_OPEN = S(6, 11);

const Score DEFENDED_PAWN = S(11, 9);

const Score DOUBLED_PAWN = S(5, -26);

const Score OPPOSED_ISOLATED_PAWN = S(-4, -9);

const Score OPEN_ISOLATED_PAWN = S(-13, -13);

const Score BACKWARDS_PAWN = S(-9, -15);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  93,  38), S(  32,  30), S(  13,  10),
 S(   8,   2), S(   6,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -70, 237), S(   6,  50),
 S( -12,  34), S( -19,  13), S( -37,   4), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  57, 170), S(   4, 231), S(   4, 121),
 S( -13,  64), S(  -6,  39), S(  -4,  34), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-4, -10);

const Score PASSED_PAWN_KING_PROXIMITY = S(-7, 23);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(23, 25);

const Score KNIGHT_THREATS[6] = { S(0, 19), S(3, 14), S(40, 29), S(92, -10), S(50, -77), S(199, 1),};

const Score BISHOP_THREATS[6] = { S(1, 15), S(30, 35), S(14, 14), S(53, 5), S(51, 77), S(105, 92),};

const Score ROOK_THREATS[6] = { S(2, 27), S(35, 38), S(40, 47), S(3, 25), S(81, 10), S(171, 11),};

const Score KING_THREAT = S(14, 31);

const Score PAWN_THREAT = S(70, 16);

const Score PAWN_PUSH_THREAT = S(21, 2);

const Score HANGING_THREAT = S(4, 10);

const Score PAWN_SHELTER[4][8] = {
 { S(-21, 4), S(41, 35), S(-21, 44), S(-13, 13), S(0, -5), S(27, -18), S(25, -39), S(0, 0),},
 { S(-44, 3), S(-45, 70), S(-8, 30), S(-31, 18), S(-21, 3), S(24, -16), S(26, -27), S(0, 0),},
 { S(-23, -6), S(4, 56), S(-33, 29), S(-11, 8), S(-13, 0), S(-6, -3), S(26, -14), S(0, 0),},
 { S(-30, 6), S(13, 74), S(-60, 30), S(-23, 25), S(-20, 12), S(-13, 6), S(-13, 6), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-29, -20), S(-17, -7), S(-16, -9), S(-28, 4), S(-53, 41), S(7, 152), S(189, 182), S(0, 0),},
 { S(-17, -9), S(-2, -9), S(-1, -4), S(-12, 7), S(-31, 33), S(-113, 170), S(55, 228), S(0, 0),},
 { S(19, -12), S(24, -17), S(19, -8), S(9, 1), S(-14, 28), S(-120, 144), S(-94, 204), S(0, 0),},
 { S(4, -3), S(12, -21), S(20, -8), S(8, -12), S(-15, 14), S(-31, 104), S(-115, 185), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-29, -15), S(48, -58), S(23, -38), S(18, -28), S(3, -17), S(-15, -33), S(0, 0), S(0, 0),
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

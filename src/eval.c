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
const Score MATERIAL_VALUES[7] = { S(88, 143), S(350, 372), S(365, 381), S(532, 704), S(1091, 1345), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(25, 62);

const Score PAWN_PSQT[32] = {
 S(   0,  -1), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  61, 151), S(  12, 172), S(  24, 146), S(  54, 118),
 S(  -9,  23), S( -20,  24), S(   5,   1), S(  24, -28),
 S( -19,   0), S( -12, -13), S( -12, -19), S(   0, -32),
 S( -22, -14), S( -23, -16), S(  -9, -24), S(   4, -24),
 S( -19, -18), S(  -6, -20), S(  -3, -20), S(   6, -17),
 S( -17, -12), S(   2, -17), S(  -5, -14), S(   3, -11),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S(-140, -39), S( -85,   2), S( -65,  15), S( -18,  17),
 S( -23,   0), S( -16,  27), S(  37,  17), S(  24,  26),
 S(  27,  -8), S(  22,  15), S(  12,  37), S(  38,  28),
 S(  30,  23), S(  26,  29), S(  34,  34), S(  35,  41),
 S(  32,  29), S(  39,  22), S(  43,  36), S(  39,  45),
 S(  -1,  26), S(  16,  24), S(  18,  30), S(  24,  45),
 S(  12,  37), S(  15,  24), S(  10,  28), S(  15,  33),
 S( -16,  38), S(   3,  23), S(   8,  31), S(  15,  38),
};

const Score BISHOP_PSQT[32] = {
 S( -50,  16), S(  -6,  41), S( -46,  46), S( -67,  51),
 S( -10,  33), S( -35,  26), S(   4,  45), S(  10,  42),
 S(  35,  33), S(  12,  41), S(  -6,  23), S(  24,  41),
 S(   2,  47), S(  42,  39), S(  30,  43), S(  34,  52),
 S(  46,  25), S(  28,  38), S(  36,  48), S(  39,  44),
 S(  23,  34), S(  40,  43), S(  19,  31), S(  28,  53),
 S(  35,  29), S(  25,  16), S(  34,  34), S(  19,  47),
 S(  15,  18), S(  34,  37), S(  18,  46), S(  20,  46),
};

const Score ROOK_PSQT[32] = {
 S(   3,  57), S(  -7,  62), S( -21,  72), S( -32,  70),
 S(   1,  46), S(  -8,  60), S(   8,  54), S(  12,  46),
 S(  -7,  44), S(  35,  38), S(  25,  42), S(  28,  33),
 S( -10,  53), S(  11,  43), S(  22,  42), S(  13,  41),
 S( -14,  49), S(  13,  42), S(  -1,  50), S(   3,  46),
 S( -12,  45), S(   9,  35), S(   7,  38), S(   4,  40),
 S( -17,  51), S(   2,  39), S(   5,  37), S(   8,  40),
 S(  -2,  40), S(   0,  43), S(   0,  42), S(   6,  36),
};

const Score QUEEN_PSQT[32] = {
 S(  11,  35), S(  13,  43), S(  51,  38), S(  37,  60),
 S( -13,  50), S( -42,  84), S( -19, 114), S( -20,  98),
 S(  22,  33), S(  19,  65), S(  -1,  91), S(   9,  94),
 S(  10,  79), S(  14, 102), S(   8,  93), S( -10, 122),
 S(  15,  72), S(  17,  96), S(   9,  98), S(  -5, 121),
 S(   5,  63), S(  14,  70), S(   6,  88), S(   0,  87),
 S(   8,  46), S(  12,  25), S(  13,  42), S(  10,  58),
 S(  14,  24), S(  -3,  41), S(  -5,  41), S(   4,  32),
};

const Score KING_PSQT[32] = {
 S( -85, -93), S(  99, -32), S(  30,   4), S( -24,  -8),
 S(  -7,   3), S(  91,  41), S(  94,  39), S(  75,  24),
 S(  -5,  10), S( 135,  36), S( 114,  36), S(  13,  38),
 S( -43,   7), S(  92,  25), S(  10,  38), S( -26,  41),
 S( -91,   8), S(  23,  17), S(  27,  19), S( -20,  26),
 S( -12,  -5), S(  43,   7), S(  22,   8), S(  -1,  12),
 S(   2, -11), S(   9,  13), S( -14,   7), S( -44,   6),
 S(   2, -49), S(   8, -11), S(  -3, -19), S( -17, -37),
};

const Score KNIGHT_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( -24,  34), S(  16,  27), S(  45,  15), S(  35,  37),
 S(  18,  15), S(  38,  19), S(  30,  30), S(  47,  27),
 S(  14,   1), S(  20,  19), S(  12,  24), S(  13,  31),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score BISHOP_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   5,  21), S(  31,  16), S(  59,  23), S(  52,  -1),
 S(  11,  14), S(  16,  21), S(  28,   9), S(  37,   6),
 S( -19,  26), S(  23,  13), S(  19,   6), S(  27,  18),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -37, -70), S( -11, -12), S(   5,  19), S(  14,  31),
 S(  24,  40), S(  32,  47), S(  39,  50), S(  47,  48),
 S(  54,  39),};

const Score BISHOP_MOBILITIES[14] = {
 S( -19, -27), S(  -4,  -1), S(  11,  15), S(  17,  35),
 S(  26,  42), S(  32,  48), S(  34,  54), S(  36,  58),
 S(  38,  59), S(  38,  63), S(  45,  57), S(  62,  56),
 S(  68,  63), S(  65,  62),};

const Score ROOK_MOBILITIES[15] = {
 S( -70,-123), S( -19, -13), S(  -7,   7), S(   3,   6),
 S(  -1,  29), S(  -1,  38), S(  -5,  46), S(  -1,  47),
 S(   5,  48), S(  10,  53), S(  13,  55), S(  13,  57),
 S(  17,  61), S(  23,  61), S(  34,  57),};

const Score QUEEN_MOBILITIES[28] = {
 S(-475,-497), S(-165, -88), S(  -2,-214), S(  -9, -56),
 S(  -8,  35), S(  -4,  41), S(  -5,  65), S(  -2,  83),
 S(   0,  98), S(   4, 100), S(   6, 104), S(   7, 110),
 S(   7, 112), S(   9, 112), S(  10, 116), S(  14, 107),
 S(  10, 107), S(  11, 107), S(   5,  99), S(  16,  84),
 S(  27,  56), S(  41,  41), S(  55,  -3), S(  43,  -9),
 S(  57, -41), S( 213,-158), S( 208,-238), S(-172, -39),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(7, 17);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-162, -114);

const Score ROOK_TRAPPED = S(-56, -22);

const Score BAD_BISHOP_PAWNS = S(-1, -5);

const Score DRAGON_BISHOP = S(25, 20);

const Score ROOK_OPEN_FILE = S(24, 14);

const Score ROOK_SEMI_OPEN = S(7, 10);

const Score DOUBLED_PAWN = S(-1, -25);

const Score OPPOSED_ISOLATED_PAWN = S(-6, -11);

const Score OPEN_ISOLATED_PAWN = S(-15, -15);

const Score BACKWARDS_PAWN = S(-8, -12);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  80,  39), S(  32,  28), S(  16,  10),
 S(   8,   2), S(   3,   1), S(   1,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -61, 196), S(  -9,  38),
 S( -14,  25), S( -21,   7), S( -31,   2), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  40, 152), S(   5, 206), S(   0, 108),
 S( -12,  54), S( -11,  32), S(  -5,  27), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-5, -8);

const Score PASSED_PAWN_KING_PROXIMITY = S(-7, 21);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(18, 23);

const Score KNIGHT_THREATS[6] = { S(1, 16), S(2, 9), S(38, 25), S(74, -6), S(45, -66), S(197, -2),};

const Score BISHOP_THREATS[6] = { S(4, 15), S(29, 37), S(12, 13), S(45, 7), S(49, 68), S(117, 84),};

const Score ROOK_THREATS[6] = { S(5, 25), S(33, 34), S(35, 41), S(1, 22), S(77, 14), S(191, 6),};

const Score KING_THREATS[6] = { S(11, 26), S(24, 36), S(-6, 31), S(13, 24), S(-150, -415), S(0, 0),};

const Score PAWN_THREAT = S(62, 13);

const Score PAWN_PUSH_THREAT = S(17, 1);

const Score HANGING_THREAT = S(3, 11);

const Score PAWN_SHELTER[4][8] = {
 { S(-27, 4), S(43, 5), S(-30, 42), S(-11, 9), S(3, -5), S(23, -17), S(22, -35), S(0, 0),},
 { S(-42, 3), S(-47, 43), S(-3, 27), S(-27, 15), S(-15, 1), S(20, -13), S(25, -21), S(0, 0),},
 { S(-22, -6), S(-5, 29), S(-17, 24), S(-7, 5), S(-11, -1), S(-8, -1), S(24, -11), S(0, 0),},
 { S(-20, 5), S(6, 47), S(-54, 28), S(-20, 22), S(-13, 9), S(-10, 4), S(-10, 7), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-26, -18), S(-12, -8), S(-11, -10), S(-20, 0), S(-40, 33), S(0, 136), S(149, 173), S(0, 0),},
 { S(-12, -8), S(0, -11), S(1, -7), S(-8, 4), S(-20, 26), S(-104, 154), S(36, 211), S(0, 0),},
 { S(12, -10), S(21, -18), S(17, -9), S(9, -1), S(-14, 25), S(-131, 134), S(-101, 191), S(0, 0),},
 { S(3, -3), S(8, -22), S(13, -10), S(8, -13), S(-14, 9), S(-42, 95), S(-91, 165), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-22, 10), S(49, -59), S(17, -34), S(16, -27), S(2, -17), S(-33, -20), S(0, 0), S(0, 0),
};

const Score KS_KING_FILE[4] = { S(11, -2), S(6, 0), S(0, 4), S(-33, -1),};

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
        BitBoard bishopSquares = (bb & DARK_SQS) ? DARK_SQS : ~DARK_SQS;
        BitBoard inTheWayPawns = board->pieces[PAWN[side]] & bishopSquares;
        BitBoard blockedInTheWayPawns = Shift(board->occupancies[BOTH], PAWN_DIRECTIONS[xside]) &
                                        board->pieces[PAWN[side]] & ~(A_FILE | B_FILE | G_FILE | H_FILE);
        int scalar = bits(inTheWayPawns) * bits(blockedInTheWayPawns);

        s += BAD_BISHOP_PAWNS * scalar;

        if (T)
          C.badBishopPawns[side] += scalar;

        if (!(CENTER_SQS & bb) && bits(CENTER_SQS & GetBishopAttacks(sq, (myPawns | opponentPawns))) > 1) {
          s += DRAGON_BISHOP;

          if (T)
            C.dragonBishop[side]++;
        }

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

  BitBoard pawnPushes = ~board->occupancies[BOTH] & Shift(board->pieces[PAWN[side]], PAWN_DIRECTIONS[side]);
  pawnPushes |= ~board->occupancies[BOTH] & Shift(pawnPushes & THIRD_RANKS[side], PAWN_DIRECTIONS[side]);
  BitBoard pawnPushAttacks =
      (Shift(pawnPushes, PAWN_DIRECTIONS[side] + E) | Shift(pawnPushes, PAWN_DIRECTIONS[side] + W)) &
      (board->occupancies[xside] & ~board->pieces[PAWN[xside]]);
  s += bits(pawnPushAttacks) * PAWN_PUSH_THREAT;

  if (T)
    C.pawnPushThreat[side] += bits(pawnPushAttacks);

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

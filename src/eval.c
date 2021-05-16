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
const Score MATERIAL_VALUES[7] = { S(91, 139), S(399, 395), S(426, 427), S(616, 814), S(1323, 1414), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(30, 70);

const Score PAWN_PSQT[32] = {
 S(   0,  -1), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  81, 206), S(  28, 229), S(  38, 202), S(  68, 170),
 S(  -2,  49), S(  -9,  49), S(  18,  22), S(  35, -11),
 S( -18,  26), S(  -5,  11), S( -11,   4), S(  11, -12),
 S( -21,   9), S( -15,   5), S(  -4,  -3), S(   8,  -6),
 S( -16,   4), S(   1,   1), S(  -1,   2), S(  11,   5),
 S( -14,  11), S(  10,   6), S(  -3,   9), S(  12,  13),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S(-127, -42), S( -75,   7), S( -45,  23), S(  14,  24),
 S(   4,   6), S(   9,  36), S(  68,  27), S(  54,  38),
 S(  61,  -4), S(  55,  25), S(  43,  54), S(  71,  43),
 S(  58,  32), S(  47,  44), S(  57,  51), S(  66,  56),
 S(  50,  42), S(  58,  38), S(  60,  55), S(  62,  63),
 S(  27,  39), S(  42,  39), S(  40,  46), S(  53,  64),
 S(  42,  50), S(  44,  35), S(  38,  43), S(  46,  49),
 S(  12,  50), S(  34,  35), S(  38,  44), S(  43,  54),
};

const Score BISHOP_PSQT[32] = {
 S( -43,  35), S( -22,  41), S( -66,  44), S( -93,  52),
 S( -21,  30), S( -27,  39), S(  -9,  42), S(  -4,  37),
 S(  24,  30), S(  -1,  34), S(   3,  37), S(  11,  32),
 S( -19,  45), S(  17,  34), S(   7,  38), S(  19,  45),
 S(  27,  24), S(   1,  33), S(  10,  42), S(  20,  38),
 S(  12,  32), S(  29,  42), S(  19,  43), S(  16,  52),
 S(  26,  29), S(  33,  27), S(  26,  30), S(   9,  44),
 S(  21,  31), S(  29,  39), S(  10,  46), S(   9,  46),
};

const Score ROOK_PSQT[32] = {
 S(  27,  57), S(   9,  64), S(  -8,  77), S( -16,  78),
 S(  24,  44), S(  11,  60), S(  30,  56), S(  32,  47),
 S(  12,  42), S(  57,  38), S(  48,  42), S(  51,  33),
 S(   3,  52), S(  18,  47), S(  33,  48), S(  30,  43),
 S(  -3,  49), S(  15,  44), S(   2,  55), S(  12,  52),
 S(   2,  44), S(  22,  38), S(  20,  43), S(  16,  45),
 S(  -2,  53), S(  15,  41), S(  22,  42), S(  25,  44),
 S(  15,  43), S(  14,  44), S(  17,  46), S(  23,  38),
};

const Score QUEEN_PSQT[32] = {
 S( -32,  93), S( -36, 109), S(   6, 104), S(  -4, 124),
 S( -57, 105), S( -94, 148), S( -69, 185), S( -71, 171),
 S( -19,  90), S( -24, 126), S( -44, 159), S( -36, 163),
 S( -45, 146), S( -47, 173), S( -50, 163), S( -62, 194),
 S( -44, 140), S( -46, 167), S( -52, 170), S( -61, 192),
 S( -44, 124), S( -36, 131), S( -50, 158), S( -50, 152),
 S( -42, 100), S( -35,  83), S( -33, 100), S( -37, 118),
 S( -34,  79), S( -52,  99), S( -55, 101), S( -44,  90),
};

const Score KING_PSQT[32] = {
 S(-153,-218), S(  41,-147), S( -31, -86), S(-100, -80),
 S( -76,-106), S(  26, -61), S(  29, -41), S(   6, -42),
 S( -77, -97), S(  65, -66), S(  61, -47), S( -60, -28),
 S(-118,-102), S(  23, -81), S( -60, -43), S(-105, -24),
 S(-175,-100), S( -57, -90), S( -42, -67), S( -97, -42),
 S( -79,-116), S( -16,-103), S( -34, -81), S( -60, -60),
 S( -57,-123), S( -51, -98), S( -63, -83), S( -96, -69),
 S( -56,-165), S( -49,-127), S( -50,-113), S( -62,-119),
};

const Score KNIGHT_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( -25,  40), S(  20,  36), S(  55,  23), S(  42,  47),
 S(  25,  20), S(  54,  24), S(  47,  40), S(  63,  36),
 S(  30,   2), S(  41,  25), S(  28,  28), S(  31,  38),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score BISHOP_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  10,  22), S(  37,  18), S(  63,  21), S(  64,  -3),
 S(  23,  14), S(  36,  20), S(  48,   9), S(  55,   3),
 S(  -7,  28), S(  45,  15), S(  41,  10), S(  49,  21),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -85, -52), S( -55,  13), S( -37,  49), S( -27,  61),
 S( -14,  70), S(  -6,  77), S(   4,  78), S(  14,  74),
 S(  23,  64),};

const Score BISHOP_MOBILITIES[14] = {
 S( -48, -23), S( -26,   3), S( -10,  22), S(  -2,  45),
 S(  11,  57), S(  19,  67), S(  23,  75), S(  24,  81),
 S(  27,  86), S(  26,  91), S(  33,  86), S(  56,  88),
 S(  61,  97), S(  59,  98),};

const Score ROOK_MOBILITIES[15] = {
 S(-110,-157), S( -70, -15), S( -56,   7), S( -46,   5),
 S( -50,  30), S( -50,  41), S( -54,  50), S( -50,  51),
 S( -42,  51), S( -39,  58), S( -35,  59), S( -34,  63),
 S( -30,  68), S( -22,  70), S(  -9,  62),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1021,-930), S(-278, -21), S( -88,-138), S( -97,  40),
 S( -94, 138), S( -90, 141), S( -91, 171), S( -88, 191),
 S( -85, 205), S( -81, 206), S( -78, 210), S( -77, 216),
 S( -76, 220), S( -74, 220), S( -73, 225), S( -70, 215),
 S( -73, 215), S( -72, 215), S( -77, 207), S( -66, 192),
 S( -52, 159), S( -31, 136), S( -23, 100), S( -35,  89),
 S( -35,  62), S( 153, -71), S( 225,-206), S(-446, 153),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(8, 21);

const Score BISHOP_OUTPOST_REACHABLE = S(7, 5);

const Score BISHOP_TRAPPED = S(-174, -121);

const Score ROOK_TRAPPED = S(-59, -28);

const Score ROOK_OPEN_FILE = S(24, 23);

const Score ROOK_SEMI_OPEN = S(1, 19);

const Score ROOK_OPPOSITE_KING = S(42, -20);

const Score ROOK_ADJACENT_KING = S(17, -29);

const Score DOUBLED_PAWN = S(-6, -32);

const Score OPPOSED_ISOLATED_PAWN = S(-6, -13);

const Score OPEN_ISOLATED_PAWN = S(-17, -16);

const Score BACKWARDS_PAWN = S(-10, -15);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  88,  44), S(  36,  33), S(  19,  11),
 S(   9,   2), S(   3,   1), S(   1,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -50, 223), S(  -4,  43),
 S( -17,  30), S( -23,   7), S( -34,   2), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  29, 166), S(  -2, 239), S(  -3, 124),
 S( -21,  65), S( -12,  37), S(  -5,  32), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-5, -9);

const Score PASSED_PAWN_KING_PROXIMITY = S(-9, 25);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(27, 25);

const Score KNIGHT_THREATS[6] = { S(-2, 21), S(2, 8), S(44, 30), S(82, -6), S(48, -72), S(218, -3),};

const Score BISHOP_THREATS[6] = { S(6, 20), S(34, 43), S(13, 11), S(50, 8), S(52, 86), S(118, 103),};

const Score ROOK_THREATS[6] = { S(6, 28), S(35, 39), S(39, 47), S(0, 30), S(85, 12), S(203, 10),};

const Score KING_THREATS[6] = { S(21, 33), S(24, 42), S(-5, 35), S(10, 14), S(-88, -655), S(0, 0),};

const Score PAWN_THREAT = S(66, 13);

const Score HANGING_THREAT = S(3, 10);

const Score PAWN_SHELTER[4][8] = {
 { S(-18, 21), S(73, 24), S(-20, 64), S(3, 27), S(16, 11), S(39, -2), S(37, -22), S(0, 0),},
 { S(-46, 14), S(-43, 66), S(-6, 42), S(-31, 30), S(-17, 14), S(23, -3), S(29, -11), S(0, 0),},
 { S(-14, -1), S(11, 43), S(-9, 33), S(3, 12), S(-1, 5), S(3, 5), S(39, -6), S(0, 0),},
 { S(-23, 0), S(13, 50), S(-62, 26), S(-24, 22), S(-15, 7), S(-11, 0), S(-12, 4), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-32, -36), S(-21, -21), S(-20, -23), S(-30, -11), S(-54, 25), S(-18, 145), S(150, 181), S(0, 0),},
 { S(-19, -23), S(-6, -23), S(-6, -18), S(-17, -7), S(-30, 17), S(-126, 163), S(20, 228), S(0, 0),},
 { S(12, -25), S(18, -31), S(13, -22), S(4, -13), S(-23, 16), S(-154, 142), S(-122, 206), S(0, 0),},
 { S(0, -17), S(2, -34), S(8, -22), S(1, -27), S(-23, -2), S(-56, 99), S(-111, 176), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-35, -10), S(48, -73), S(13, -51), S(12, -42), S(-3, -32), S(-42, -39), S(0, 0), S(0, 0),
};

const Score KS_KING_FILE[4] = { S(16, -4), S(16, -1), S(7, 3), S(-29, -2),};

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

          if (file(sq) == file(data->kingSq[xside])) {
            s += ROOK_OPPOSITE_KING;

            if (T)
              C.rookOppositeKing[side]++;
          } else if (abs(file(sq) - file(data->kingSq[xside])) == 1) {
            s += ROOK_ADJACENT_KING;

            if (T)
              C.rookAdjacentKing[side]++;
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

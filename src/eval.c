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
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "string.h"
#include "types.h"
#include "util.h"

#ifdef TUNE
#include "tune.h"

EvalCoeffs C;
#endif

#define S(mg, eg) (makeScore((mg), (eg)))
#define rel(sq, side) ((side) ? MIRROR[(sq)] : (sq))
#define distance(a, b) max(abs(rank(a) - rank(b)), abs(file(a) - file(b)))

const Score UNKNOWN = INT32_MAX;
const int SHELTER_STORM_FILES[8][2] = {{0, 2}, {0, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6}, {5, 7}, {5, 7}};
const uint8_t kpkResults[2 * 64 * 64 * 24 / 8] = {
#include "kpk.h"
};

// Phase generation calculation is standard from CPW
const Score PHASE_MULTIPLIERS[5] = {0, 1, 1, 2, 4};
const Score MAX_PHASE = 24;

const int STATIC_MATERIAL_VALUE[7] = {93, 310, 323, 548, 970, 30000, 0};

// clang-format off
const Score MATERIAL_VALUES[7] = { S(60, 93), S(310, 310), S(323, 323), S(435, 548), S(910, 970), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(7, 20);

const Score PAWN_PSQT[32] = {
 S(   2,  -1), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  21,  99), S(  10,  97), S(  15,  94), S(  22,  89),
 S(   7,  25), S(  10,  21), S(  17,   8), S(  16,   3),
 S(   2,  17), S(   5,  14), S(   4,  10), S(   9,  11),
 S(   0,  12), S(   2,  13), S(   4,  10), S(   6,  14),
 S(   2,  10), S(   8,  11), S(   5,  12), S(   7,  15),
 S(   2,  11), S(   8,  12), S(   4,  13), S(   7,  18),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S(  21, -16), S(  38,  -5), S(  45,   0), S(  49,  -3),
 S(  27,   6), S(  27,  18), S(  40,   8), S(  32,  12),
 S(  37,   4), S(  31,  12), S(  25,  19), S(  36,  10),
 S(  35,  14), S(  31,  11), S(  35,  10), S(  36,  12),
 S(  35,  22), S(  36,  13), S(  36,  21), S(  37,  21),
 S(  29,  20), S(  33,  19), S(  31,  22), S(  36,  26),
 S(  32,  30), S(  33,  22), S(  32,  21), S(  33,  24),
 S(  26,  27), S(  31,  22), S(  33,  20), S(  33,  27),
};

const Score BISHOP_PSQT[32] = {
 S(  15,  10), S(  14,  16), S(  15,  12), S(  10,   8),
 S(   6,  20), S(   1,  26), S(   7,  24), S(   4,  23),
 S(   8,  20), S(   5,  20), S(   3,  23), S(  12,  16),
 S(   4,  19), S(  12,  17), S(   7,  18), S(  13,  18),
 S(  15,  14), S(   8,  16), S(  10,  19), S(  14,  16),
 S(  11,  17), S(  15,  21), S(  13,  22), S(  12,  25),
 S(  15,  18), S(  17,  20), S(  15,  18), S(  10,  24),
 S(  14,  20), S(  16,  18), S(  12,  24), S(  11,  24),
};

const Score ROOK_PSQT[32] = {
 S(  38,  44), S(  30,  47), S(  10,  53), S(  11,  52),
 S(  46,  39), S(  42,  44), S(  46,  44), S(  42,  41),
 S(  38,  42), S(  56,  40), S(  53,  42), S(  54,  40),
 S(  39,  49), S(  42,  46), S(  46,  48), S(  46,  46),
 S(  37,  50), S(  40,  47), S(  39,  51), S(  41,  52),
 S(  37,  51), S(  41,  45), S(  42,  49), S(  41,  50),
 S(  34,  56), S(  41,  50), S(  41,  52), S(  42,  52),
 S(  40,  56), S(  40,  53), S(  40,  54), S(  42,  51),
};

const Score QUEEN_PSQT[32] = {
 S(  34,  94), S(  43,  91), S(  39, 103), S(  25, 114),
 S(  19, 112), S(  11, 127), S(  18, 131), S(  12, 126),
 S(  28, 107), S(  24, 118), S(  22, 122), S(  23, 127),
 S(  22, 119), S(  21, 128), S(  21, 123), S(  18, 132),
 S(  23, 118), S(  22, 124), S(  20, 125), S(  19, 131),
 S(  22, 116), S(  25, 110), S(  21, 120), S(  21, 116),
 S(  25, 102), S(  27,  94), S(  26, 101), S(  25, 106),
 S(  25,  98), S(  20, 103), S(  20, 102), S(  23,  96),
};

const Score KING_PSQT[32] = {
 S( -66,-155), S(  37,-125), S( -57, -95), S( -40,-106),
 S( -55,-107), S(  35, -97), S(  24, -92), S(  16, -91),
 S( -47,-105), S(  14, -98), S(  35, -94), S( -22, -88),
 S( -56,-104), S(   8, -97), S(  -6, -86), S( -27, -81),
 S( -83,-101), S( -33, -93), S( -10, -88), S( -20, -84),
 S( -56,-106), S( -36, -94), S( -25, -89), S( -26, -86),
 S( -53,-115), S( -53,-101), S( -53, -91), S( -61, -86),
 S( -54,-131), S( -52,-119), S( -54,-107), S( -54,-110),
};

const Score KNIGHT_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  -5,   0), S(  10,   5), S(  21,  -1), S(  20,  10),
 S(  11,  -1), S(  16,  12), S(  15,  17), S(  18,  13),
 S(   7,  -1), S(   8,  10), S(  10,   6), S(   8,  18),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score BISHOP_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   8,   0), S(  11,   5), S(  22,   0), S(  19,   0),
 S(   4,   6), S(  10,  12), S(  19,   3), S(  16,   3),
 S(  -1,  11), S(  12,   4), S(   8,   7), S(  14,   9),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -42,  -7), S( -34,  16), S( -29,  28), S( -26,  33),
 S( -22,  35), S( -20,  38), S( -18,  39), S( -16,  38),
 S( -14,  36),};

const Score BISHOP_MOBILITIES[14] = {
 S( -16,  11), S(  -9,  19), S(  -5,  25), S(  -3,  32),
 S(   0,  35), S(   2,  39), S(   3,  39), S(   3,  41),
 S(   4,  40), S(   4,  41), S(   6,  40), S(   9,  40),
 S(  14,  44), S(   3,  41),};

const Score ROOK_MOBILITIES[15] = {
 S( -55, -33), S( -40,  35), S( -36,  43), S( -33,  40),
 S( -34,  45), S( -34,  49), S( -35,  49), S( -35,  53),
 S( -33,  52), S( -32,  52), S( -30,  52), S( -29,  49),
 S( -28,  49), S( -26,  48), S( -24,  45),};

const Score QUEEN_MOBILITIES[28] = {
 S(-128,-104), S( -47,  81), S( -12,  -8), S( -12,  92),
 S( -11, 127), S( -11, 136), S( -11, 145), S(  -9, 147),
 S(  -9, 152), S(  -7, 152), S(  -7, 154), S(  -6, 155),
 S(  -5, 158), S(  -5, 157), S(  -5, 160), S(  -4, 157),
 S(  -5, 156), S(  -6, 160), S(  -7, 155), S(  -3, 150),
 S(  -1, 142), S(  -1, 137), S(  17, 109), S(  21, 102),
 S(  38,  75), S( 116,  16), S(  73,  14), S( 144,   5),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(1, 7);

const Score BISHOP_OUTPOST_REACHABLE = S(1, 2);

const Score BISHOP_TRAPPED = S(-67, -68);

const Score ROOK_TRAPPED = S(-13, -16);

const Score ROOK_OPEN_FILE = S(6, 10);

const Score ROOK_SEMI_OPEN = S(0, 10);

const Score ROOK_OPPOSITE_KING = S(11, -13);

const Score ROOK_ADJACENT_KING = S(7, -29);

const Score DOUBLED_PAWN = S(-3, -10);

const Score OPPOSED_ISOLATED_PAWN = S(-1, -5);

const Score OPEN_ISOLATED_PAWN = S(-3, -5);

const Score BACKWARDS_PAWN = S(-2, -6);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  42,   2), S(  10,  10), S(   4,   5),
 S(   2,   2), S(   1,   1), S(   0,  -1), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  -5,  58), S( -10,  96), S(  -6,  52),
 S(  -7,  26), S(  -2,  13), S(   0,  11), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-1, -4);

const Score PASSED_PAWN_KING_PROXIMITY = S(-4, 12);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(7, 10);

const Score KNIGHT_THREATS[6] = { S(0, 7), S(4, -2), S(13, 13), S(23, 13), S(3, 43), S(19, 21),};

const Score BISHOP_THREATS[6] = { S(2, 6), S(10, 15), S(1, 11), S(17, 10), S(8, 73), S(40, 41),};

const Score ROOK_THREATS[6] = { S(2, 9), S(8, 17), S(10, 19), S(-6, 26), S(7, 62), S(19, 11),};

const Score KING_THREATS[6] = { S(-1, 34), S(-9, 37), S(-1, 23), S(1, 26), S(-14, 61), S(0, 0),};

const Score PAWN_SHELTER[4][8] = {
    {S(-2,0), S(12,0), S(12,0), S(15,0), S(22,0), S(38,0), S(34,0), S(0,0)},
    {S(-18,0), S(-18,0), S(-5,0), S(-12,0), S(-21,0), S(14,0), S(28,0), S(0,0)},
    {S(-5,0), S(-15,0), S(3,0), S(13,0), S(-2,0), S(10,0), S(32,0), S(0,0)},
    {S(-16,0), S(-50,0), S(-24,0), S(-16,0), S(-21,0), S(-14,0), S(-5,0), S(0,0)},
};

const Score PAWN_STORM[4][8] = {
    {S(-36,0), S(-20,0), S(-20,0), S(-20,0), S(-40,0), S(70,0), S(119,0), S(0,0)},
    {S(-18,0), S(-9,0), S(4,0), S(-14,0), S(-20,0), S(-50,0), S(10,0), S(0,0)},
    {S(3,0), S(4,0), S(5,0), S(1,0), S(-14,0), S(-65,0), S(-21,0), S(0,0)},
    {S(6,0), S(12,0), S(6,0), S(-2,0), S(0,0), S(-45,0), S(5,0), S(0,0)},
};

const Score BLOCKED_PAWN_STORM[8] = {S(0, 0), S(0, 0), S(2, -2), S(2, -2), S(2, -4), S(-31, -32), S(0, 0), S(0, 0),};

const Score KS_KING_FILE[4] = {S(9, -4), S(4, 0), S(0, 0), S(-4, 2)};

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

inline Score GetPhase(Board* board) {
  Score phase = 0;
  for (int i = KNIGHT_WHITE; i <= QUEEN_BLACK; i++)
    phase += PHASE_MULTIPLIERS[PIECE_TYPE[i]] * bits(board->pieces[i]);

  phase = min(MAX_PHASE, phase);
  return phase;
}

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

Score MaterialValue(Board* board, int side) {
  Score s = 0;

  for (int pc = PAWN[side]; pc <= KING[side]; pc += 2) {
    BitBoard pieces = board->pieces[pc];

#ifdef TUNE
    C.pieces[side][PIECE_TYPE[pc]] = bits(pieces);
#endif

    while (pieces) {
      int sq = lsb(pieces);
      s += PSQT[pc][sq];

      popLsb(pieces);

#ifdef TUNE
      C.psqt[side][PIECE_TYPE[pc]][psqtIdx(side == WHITE ? sq : MIRROR[sq])]++;
#endif
    }
  }

  if (bits(board->pieces[BISHOP[side]]) > 1) {
    s += BISHOP_PAIR;

#ifdef TUNE
    C.bishopPair[side]++;
#endif
  }

  return s;
}

Score PieceEval(Board* board, EvalData* data, int side) {
  Score s = 0;

  int xside = side ^ 1;
  BitBoard enemyKingArea = data->kingArea[xside];
  BitBoard mob = data->mobilitySquares[side];
  BitBoard outposts = data->outposts[side];
  BitBoard myPawns = board->pieces[PAWN[side]];
  BitBoard opponentPawns = board->pieces[PAWN[xside]];

  for (int p = KNIGHT[side]; p <= KING[side]; p += 2) {
    BitBoard pieces = board->pieces[p];
    int pieceType = PIECE_TYPE[p];

    while (pieces) {
      BitBoard bb = pieces & -pieces;
      int sq = lsb(pieces);

      BitBoard movement = EMPTY;
      switch (pieceType) {
      case KNIGHT_TYPE:
        movement = GetKnightAttacks(sq);
        s += KNIGHT_MOBILITIES[bits(movement & mob)];
#ifdef TUNE
        C.knightMobilities[side][bits(movement & mob)]++;
#endif
        break;
      case BISHOP_TYPE:
        movement = GetBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]);
        s += BISHOP_MOBILITIES[bits(movement & mob)];
#ifdef TUNE
        C.bishopMobilities[side][bits(movement & mob)]++;
#endif
        break;
      case ROOK_TYPE:
        movement =
            GetRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[ROOK[side]] ^ board->pieces[QUEEN[side]]);
        s += ROOK_MOBILITIES[bits(movement & mob)];
#ifdef TUNE
        C.rookMobilities[side][bits(movement & mob)]++;
#endif
        break;
      case QUEEN_TYPE:
        movement = GetQueenAttacks(sq, board->occupancies[BOTH]);
        s += QUEEN_MOBILITIES[bits(movement & mob)];
#ifdef TUNE
        C.queenMobilities[side][bits(movement & mob)]++;
#endif
        break;
      case KING_TYPE:
        movement = GetKingAttacks(sq) & ~enemyKingArea;
      }

      data->twoAttacks[side] |= (movement & data->allAttacks[side]);
      data->attacks[side][pieceType] |= movement;
      data->allAttacks[side] |= movement;

      if (movement & enemyKingArea) {
        data->ksAttackWeight[side] += KS_ATTACKER_WEIGHTS[pieceType];
        data->ksSqAttackCount[side] += bits(movement & enemyKingArea);
        data->ksAttackerCount[side]++;
      }

      if (pieceType == KNIGHT_TYPE) {
        if (outposts & bb) {
          s += KNIGHT_POSTS[side][sq];
#ifdef TUNE
          C.knightPostPsqt[side][psqtIdx(side == WHITE ? sq : MIRROR[sq])]++;
#endif
        } else if (movement & outposts) {
          s += KNIGHT_OUTPOST_REACHABLE;
#ifdef TUNE
          C.knightPostReachable[side]++;
#endif
        }
      } else if (pieceType == BISHOP_TYPE) {
        if (outposts & bb) {
          s += BISHOP_POSTS[side][sq];
#ifdef TUNE
          C.bishopPostPsqt[side][psqtIdx(side == WHITE ? sq : MIRROR[sq])]++;
#endif
        } else if (movement & outposts) {
          s += BISHOP_OUTPOST_REACHABLE;
#ifdef TUNE
          C.bishopPostReachable[side]++;
#endif
        }

        if (side == WHITE) {
          if ((sq == A7 || sq == B8) && getBit(opponentPawns, B6) && getBit(opponentPawns, C7)) {
            s += BISHOP_TRAPPED;
#ifdef TUNE
            C.bishopTrapped[side]++;
#endif
          } else if ((sq == H7 || sq == G8) && getBit(opponentPawns, F7) && getBit(opponentPawns, G6)) {
            s += BISHOP_TRAPPED;
#ifdef TUNE
            C.bishopTrapped[side]++;
#endif
          }
        } else {
          if ((sq == A2 || sq == B1) && getBit(opponentPawns, B3) && getBit(opponentPawns, C2)) {
            s += BISHOP_TRAPPED;
#ifdef TUNE
            C.bishopTrapped[side]++;
#endif
          } else if ((sq == H2 || sq == G1) && getBit(opponentPawns, G3) && getBit(opponentPawns, F2)) {
            s += BISHOP_TRAPPED;
#ifdef TUNE
            C.bishopTrapped[side]++;
#endif
          }
        }
      } else if (pieceType == ROOK_TYPE) {
        if (!(FILE_MASKS[file(sq)] & myPawns)) {
          if (!(FILE_MASKS[file(sq)] & opponentPawns)) {
            s += ROOK_OPEN_FILE;
#ifdef TUNE
            C.rookOpenFile[side]++;
#endif
          } else {
            s += ROOK_SEMI_OPEN;
#ifdef TUNE
            C.rookSemiOpen[side]++;
#endif
          }

          // king threats bonus - these tuned really negative in the EG which is both surprising but not
          if (file(sq) == file(data->kingSq[xside])) {
            s += ROOK_OPPOSITE_KING;
#ifdef TUNE
            C.rookOppositeKing[side]++;
#endif
          } else if (abs(file(sq) - file(data->kingSq[xside])) == 1) {
            s += ROOK_ADJACENT_KING;
#ifdef TUNE
            C.rookAdjacentKing[side]++;
#endif
          }
        }

        if (side == WHITE) {
          if ((sq == A1 || sq == A2 || sq == B1) && (data->kingSq[side] == C1 || data->kingSq[side] == B1)) {
            s += ROOK_TRAPPED;
#ifdef TUNE
            C.rookTrapped[side]++;
#endif
          } else if ((sq == H1 || sq == H2 || sq == G1) && (data->kingSq[side] == F1 || data->kingSq[side] == G1)) {
            s += ROOK_TRAPPED;
#ifdef TUNE
            C.rookTrapped[side]++;
#endif
          }
        } else {
          if ((sq == A8 || sq == A7 || sq == B8) && (data->kingSq[side] == B8 || data->kingSq[side] == C8)) {
            s += ROOK_TRAPPED;
#ifdef TUNE
            C.rookTrapped[side]++;
#endif
          } else if ((sq == H8 || sq == H7 || sq == G8) && (data->kingSq[side] == F8 || data->kingSq[side] == G8)) {
            s += ROOK_TRAPPED;
#ifdef TUNE
            C.rookTrapped[side]++;
#endif
          }
        }
      }

      popLsb(pieces);
    }
  }

  return s;
}

Score PawnEval(Board* board, EvalData* data, int side) {
  Score s = 0;

  int xside = side ^ 1;
  BitBoard pawns = board->pieces[PAWN[side]];

  while (pawns) {
    BitBoard bb = pawns & -pawns;
    int sq = lsb(pawns);

    int file = file(sq);
    int rank = rank(sq);
    int adjustedRank = side ? 7 - rank : rank;
    int adjustedFile = file > 3 ? 7 - file : file; // 0-3

    BitBoard opposed = board->pieces[PAWN[xside]] & FILE_MASKS[file] & FORWARD_RANK_MASKS[side][rank];
    BitBoard doubled = board->pieces[PAWN[side]] & Shift(bb, PAWN_DIRECTIONS[xside]);
    BitBoard neighbors = board->pieces[PAWN[side]] & ADJACENT_FILE_MASKS[file];
    BitBoard connected = neighbors & RANK_MASKS[rank];
    BitBoard defenders = board->pieces[PAWN[side]] & GetPawnAttacks(sq, xside);
    BitBoard forwardLever = board->pieces[PAWN[xside]] & GetPawnAttacks(sq + PAWN_DIRECTIONS[side], side);
    int backwards = !(neighbors & FORWARD_RANK_MASKS[xside][rank(sq + PAWN_DIRECTIONS[side])]) && forwardLever;
    int passed = !(board->pieces[PAWN[xside]] & FORWARD_RANK_MASKS[side][rank] &
                   (ADJACENT_FILE_MASKS[file] | FILE_MASKS[file])) &&
                 !(board->pieces[PAWN[side]] & FORWARD_RANK_MASKS[side][rank] & FILE_MASKS[file]);

    if (doubled) {
      s += DOUBLED_PAWN;
#ifdef TUNE
      C.doubledPawns[side]++;
#endif
    }

    if (!neighbors) {
      s += opposed ? OPPOSED_ISOLATED_PAWN : OPEN_ISOLATED_PAWN;
#ifdef TUNE
      if (opposed)
        C.opposedIsolatedPawns[side]++;
      else
        C.openIsolatedPawns[side]++;
#endif
    } else if (backwards) {
      s += BACKWARDS_PAWN;
#ifdef TUNE
      C.backwardsPawns[side]++;
#endif
    } else if (defenders | connected) {
      int scalar = 2 + !!connected - !!opposed;
      s += CONNECTED_PAWN[adjustedRank] * scalar;
#ifdef TUNE
      C.connectedPawn[side][adjustedRank] += scalar;
#endif
    }

    if (passed) {
      s += PASSED_PAWN[adjustedRank] + PASSED_PAWN_EDGE_DISTANCE * adjustedFile;
#ifdef TUNE
      C.passedPawn[side][adjustedRank]++;
      C.passedPawnEdgeDistance[side] += adjustedFile;
#endif

      int advSq = sq + PAWN_DIRECTIONS[side];
      BitBoard advance = bit(advSq);

      if (adjustedRank <= 4) {
        int myDistance = distance(advSq, data->kingSq[side]);
        int opponentDistance = distance(advSq, data->kingSq[xside]);

        s += PASSED_PAWN_KING_PROXIMITY * min(4, max(opponentDistance - myDistance, -4));
#ifdef TUNE
        C.passedPawnKingProximity[side] += min(4, max(opponentDistance - myDistance, -4));
#endif

        if (!(board->occupancies[xside] & advance)) {
          BitBoard pusher = GetRookAttacks(sq, board->occupancies[BOTH]) & FILE_MASKS[file] &
                            FORWARD_RANK_MASKS[xside][rank] & (board->pieces[ROOK[side]] | board->pieces[QUEEN[side]]);

          if (pusher | (data->allAttacks[side] & advance)) {
            s += PASSED_PAWN_ADVANCE_DEFENDED;
#ifdef TUNE
            C.passedPawnAdvance[side]++;
#endif
          }
        }
      }
    }

    popLsb(pawns);
  }

  return s;
}

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
#ifdef TUNE
        C.knightThreats[side][attacked]++;
#endif
        break;
      case BISHOP_TYPE:
        s += BISHOP_THREATS[attacked];
#ifdef TUNE
        C.bishopThreats[side][attacked]++;
#endif
        break;
      case ROOK_TYPE:
        s += ROOK_THREATS[attacked];
#ifdef TUNE
        C.rookThreats[side][attacked]++;
#endif
        break;
      case KING_TYPE:
        s += KING_THREATS[attacked];
#ifdef TUNE
        C.kingThreats[side][attacked]++;
#endif
        break;
      }

      popLsb(threats);
    }
  }

  return s;
}

Score KingSafety(Board* board, EvalData* data, int side) {
  Score s = 0;
  Score shelter = S(2, 2);

  int xside = side ^ 1;

  BitBoard ourPawns = board->pieces[PAWN[side]] & ~data->attacks[xside][PAWN_TYPE] &
                      ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];
  BitBoard opponentPawns = board->pieces[PAWN[xside]] & ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];

  for (int file = SHELTER_STORM_FILES[file(data->kingSq[side])][0];
       file <= SHELTER_STORM_FILES[file(data->kingSq[side])][1]; file++) {
    int adjustedFile = file > 3 ? 7 - file : file;

    BitBoard ourPawnFile = ourPawns & FILE_MASKS[file];
    int pawnRank = ourPawnFile ? (side ? 7 - rank(lsb(ourPawnFile)) : rank(msb(ourPawnFile))) : 0;
    shelter += PAWN_SHELTER[adjustedFile][pawnRank];

    BitBoard opponentPawnFile = opponentPawns & FILE_MASKS[file];
    int theirRank = opponentPawnFile ? (side ? 7 - rank(lsb(opponentPawnFile)) : rank(msb(opponentPawnFile))) : 0;
    if (pawnRank && pawnRank == theirRank + 1) {
      shelter += BLOCKED_PAWN_STORM[theirRank];
    } else {
      shelter += PAWN_STORM[adjustedFile][theirRank];
    }

    if (file == file(data->kingSq[side])) {
      int idx = 2 * !(board->pieces[PAWN[side]] & FILE_MASKS[file]) + !(board->pieces[PAWN[xside]] & FILE_MASKS[file]);
      shelter += KS_KING_FILE[idx];
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

  int safeChecks = bits(possibleKnightChecks & vulnerable) + bits(possibleBishopChecks & vulnerable) +
                   bits(possibleRookChecks & vulnerable) + bits(possibleQueenChecks & vulnerable);
  int unsafeChecks = bits(possibleKnightChecks & ~vulnerable) + bits(possibleBishopChecks & ~vulnerable) +
                     bits(possibleRookChecks & ~vulnerable);

  Score danger = data->ksAttackWeight[xside] * data->ksAttackerCount[xside]              //
                 + (KS_SAFE_CHECK * safeChecks)                                          //
                 + (KS_UNSAFE_CHECK * unsafeChecks)                                      //
                 + (KS_WEAK_SQS * bits(weak & kingArea))                                 //
                 + (KS_ATTACK * data->ksSqAttackCount[xside])                            //
                 + (KS_ENEMY_QUEEN * !board->pieces[QUEEN[xside]])                       //
                 + (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT_TYPE] & kingArea)) //
                 - scoreMG(shelter) / 2;                                                 //

  if (danger > 0)
    s += S(-danger * danger / 1000, -danger / 30);

  s += shelter;

#ifdef TUNE
  C.ks[side] = s;
#endif

  return s;
}

Score EvaluateKXK(Board* board) {
  if (board->pieces[QUEEN_WHITE] | board->pieces[QUEEN_BLACK] | board->pieces[ROOK_WHITE] | board->pieces[ROOK_BLACK]) {
    Score eval = 10000;
    int ss = (board->pieces[QUEEN_WHITE] | board->pieces[ROOK_WHITE]) ? WHITE : BLACK;

    int ssKingSq = lsb(board->pieces[KING[ss]]);
    int wsKingSq = lsb(board->pieces[KING[1 - ss]]);

    eval -= distance(ssKingSq, wsKingSq) * 25; // move king towards

    int wsKingRank = rank(wsKingSq) > 3 ? 7 - rank(wsKingSq) : rank(wsKingSq);
    eval -= wsKingRank * 15;

    int wsKingFile = file(wsKingSq) > 3 ? 7 - file(wsKingSq) : file(wsKingSq);
    eval -= wsKingFile * 15;

    return ss == board->side ? eval : -eval;
  } else if (board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK]) {
    Score eval = 9000;
    int ss = board->pieces[PAWN_WHITE] ? WHITE : BLACK;

    int ssKingSq = lsb(board->pieces[KING[ss]]);
    int wsKingSq = lsb(board->pieces[KING[1 - ss]]);
    int pawnSq = lsb(board->pieces[PAWN[ss]]);

    if (KPKDraw(ss, ssKingSq, wsKingSq, pawnSq, board->side)) {
      return 0;
    } else {
      int queenSq = ss == WHITE ? file(pawnSq) : file(pawnSq) + A1;

      eval -= 10 * distance(pawnSq, queenSq);
      eval -= distance(ssKingSq, queenSq);

      return ss == board->side ? eval : -eval;
    }
  }

  return UNKNOWN;
}

// The following KPK code is modified for my use from Cheng (as is the dataset)
uint8_t GetKPKBit(int bit) { return (uint8_t)(kpkResults[bit >> 3] & (1U << (bit & 7))); }

int KPKIndex(int ssKing, int wsKing, int p, int stm) {
  int file = file(p);
  int x = file > 3 ? 7 : 0;

  ssKing ^= x;
  wsKing ^= x;
  p ^= x;
  file ^= x;

  int pawn = (((p & 0x38) - 8) >> 1) | file;

  return ((unsigned)pawn << 13) | ((unsigned)stm << 12) | ((unsigned)wsKing << 6) | ((unsigned)ssKing);
}

int KPKDraw(int ss, int ssKing, int wsKing, int p, int stm) {
  int x = ss == WHITE ? 0 : 0x38;
  int idx = KPKIndex(ssKing ^ x, wsKing ^ x, p ^ x, stm ^ x);

  return GetKPKBit(idx);
}
//---

Score StaticMaterialScore(int side, Board* board) {
  Score s = 0;
  for (int p = PAWN[side]; p <= QUEEN[side]; p += 2)
    s += bits(board->pieces[p]) * STATIC_MATERIAL_VALUE[PIECE_TYPE[p]];

  return s;
}

Score EvaluateMaterialOnlyEndgame(Board* board) {
  Score whiteMaterial = StaticMaterialScore(WHITE, board);
  int whitePieceCount = bits(board->occupancies[WHITE]);

  Score blackMaterial = StaticMaterialScore(BLACK, board);
  int blackPieceCount = bits(board->occupancies[BLACK]);

  int ss = whiteMaterial >= blackMaterial ? WHITE : BLACK;

  if (!whiteMaterial || !blackMaterial) {
    // TODO: Clean this up - This is a repeat of KRK and KQK

    // We set eval to 10000 since we now know one side is completely winning
    // (they have to have mating material and the other side can't defend with no pieces)
    Score eval = 10000;

    int ssKingSq = lsb(board->pieces[KING[ss]]);
    int wsKingSq = lsb(board->pieces[KING[1 - ss]]);

    eval -= distance(ssKingSq, wsKingSq) * 25;

    int wsKingRank = rank(wsKingSq) > 3 ? 7 - rank(wsKingSq) : rank(wsKingSq);
    eval -= wsKingRank * 15;

    int wsKingFile = file(wsKingSq) > 3 ? 7 - file(wsKingSq) : file(wsKingSq);
    eval -= wsKingFile * 15;

    return ss == board->side ? eval : -eval;
  } else {
    int ssMaterial = ss == WHITE ? whiteMaterial : blackMaterial;
    int wsMaterial = ss == WHITE ? blackMaterial : whiteMaterial;
    int ssPieceCount = ss == WHITE ? whitePieceCount : blackPieceCount;
    int wsPieceCount = ss == WHITE ? blackPieceCount : whitePieceCount;

    int materialDiff = ssMaterial - wsMaterial;
    Score eval = materialDiff;

    int ssKingSq = lsb(board->pieces[KING[ss]]);
    int wsKingSq = lsb(board->pieces[KING[1 - ss]]);

    // I keep doing this...
    eval -= distance(ssKingSq, wsKingSq) * 25;

    int wsKingRank = rank(wsKingSq) > 3 ? 7 - rank(wsKingSq) : rank(wsKingSq);
    eval -= wsKingRank * 15;

    int wsKingFile = file(wsKingSq) > 3 ? 7 - file(wsKingSq) : file(wsKingSq);
    eval -= wsKingFile * 15;

    if (ssPieceCount <= wsPieceCount + 1 && materialDiff <= STATIC_MATERIAL_VALUE[BISHOP_TYPE]) {
      if (ssMaterial >= STATIC_MATERIAL_VALUE[ROOK_TYPE] + STATIC_MATERIAL_VALUE[BISHOP_TYPE])
        eval /= 4;
      else
        eval /= 8;
    }

    return ss == board->side ? eval : -eval;
  }
}

// Main evalution method
Score Evaluate(Board* board) {
  if (IsMaterialDraw(board))
    return 0;

  Score eval;
  if (bits(board->occupancies[BOTH]) == 3) {
    eval = EvaluateKXK(board);
    if (eval != UNKNOWN)
      return eval;
  } else if (!(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK])) {
    eval = EvaluateMaterialOnlyEndgame(board);
    if (eval != UNKNOWN)
      return eval;
  }

  EvalData data = {0};
  InitEvalData(&data, board);

  Score s = 0;

  s += MaterialValue(board, WHITE) - MaterialValue(board, BLACK);
  s += PieceEval(board, &data, WHITE) - PieceEval(board, &data, BLACK);
  s += PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);
  s += Threats(board, &data, WHITE) - Threats(board, &data, BLACK);
  s += KingSafety(board, &data, WHITE) - KingSafety(board, &data, BLACK);

  int phase = GetPhase(board);
  s = (phase * scoreMG(s) + (MAX_PHASE - phase) * scoreEG(s)) / MAX_PHASE;
  s = s * Scale(board, eval > 0 ? board->side : board->xside) / 100;

  return TEMPO + (board->side == WHITE ? s : -s);
}

inline int Scale(Board* board, int ss) {
  if (bits(board->occupancies[ss]) == 2 && (board->pieces[KNIGHT[ss]] | board->pieces[BISHOP[ss]]))
    return 0;

  if (IsOCB(board))
    return 50;

  int ssPawns = bits(board->pieces[PAWN[ss]]);
  if (ssPawns < 4)
    return 100 - 3 * (4 - ssPawns) * (4 - ssPawns);

  return 100;
}
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
const Score MATERIAL_VALUES[7] = { S(73, 104), S(338, 331), S(352, 348), S(485, 608), S(1039, 1036), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(20, 49);

const Score PAWN_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  49, 150), S(  14, 162), S(  57, 138), S(  79, 125),
 S(   4,  20), S(  -5,  17), S(  22, -10), S(  21, -22),
 S( -14,   5), S(   0,  -4), S(  -7, -11), S(  11, -14),
 S( -17,  -6), S( -12,  -6), S(  -3, -11), S(   2,  -6),
 S( -16,  -9), S(   6, -10), S(  -5,  -6), S(   4,  -2),
 S( -14,  -6), S(   2,  -6), S( -10,   0), S(   2,   8),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S(-100, -55), S( -51, -21), S( -39, -10), S(   1,  -7),
 S(  -4, -18), S(  -1,   5), S(  40,  -4), S(  33,   4),
 S(  21, -22), S(  24,  -4), S(  19,  17), S(  44,   6),
 S(  29,  -1), S(  20,   6), S(  31,  11), S(  37,  15),
 S(  29,   5), S(  32,   1), S(  34,  15), S(  36,  19),
 S(  12,   3), S(  24,   3), S(  20,  10), S(  32,  21),
 S(  21,  16), S(  27,   2), S(  22,   7), S(  26,  11),
 S(  -1,  14), S(  15,   1), S(  21,   6), S(  24,  13),
};

const Score BISHOP_PSQT[32] = {
 S( -36,   2), S( -26,   9), S( -65,  11), S( -78,  14),
 S( -25,   4), S( -28,   9), S( -14,  10), S(  -9,   6),
 S( -12,   5), S( -22,   6), S( -16,   8), S(  -4,   3),
 S( -26,  10), S(  -1,   2), S(  -8,   6), S(   4,   9),
 S(   9,  -6), S( -10,   1), S(  -4,   8), S(   4,   6),
 S(   0,   1), S(  13,   8), S(   3,  10), S(   2,  15),
 S(  11,  -1), S(  17,  -1), S(  10,  -1), S(  -3,  11),
 S(   8,   0), S(  11,   4), S(   0,  10), S(  -2,  11),
};

const Score ROOK_PSQT[32] = {
 S(  11,  29), S(   2,  32), S(  -9,  41), S( -17,  42),
 S(  13,  17), S(   6,  27), S(  22,  22), S(  23,  17),
 S(   3,  16), S(  40,  10), S(  35,  14), S(  37,   8),
 S(  -1,  22), S(  10,  18), S(  24,  18), S(  23,  15),
 S(  -4,  20), S(   8,  16), S(   2,  24), S(   9,  22),
 S(  -1,  16), S(  13,  11), S(  13,  16), S(  12,  16),
 S(  -2,  22), S(  11,  15), S(  17,  14), S(  19,  16),
 S(  11,  14), S(  10,  15), S(  11,  17), S(  17,  11),
};

const Score QUEEN_PSQT[32] = {
 S( -40,  40), S( -34,  43), S( -14,  49), S( -26,  70),
 S( -63,  67), S( -88,  97), S( -66, 117), S( -65,  97),
 S( -39,  47), S( -46,  84), S( -54,  97), S( -46, 100),
 S( -56,  91), S( -55, 109), S( -57, 102), S( -65, 125),
 S( -51,  84), S( -53, 103), S( -58, 108), S( -64, 126),
 S( -52,  76), S( -45,  78), S( -57,  98), S( -56,  94),
 S( -47,  53), S( -42,  41), S( -44,  58), S( -46,  71),
 S( -40,  34), S( -56,  56), S( -57,  56), S( -51,  50),
};

const Score KING_PSQT[32] = {
 S(-165,-180), S(  33,-139), S(   0,-101), S( -52,-104),
 S( -38,-113), S(  45, -82), S(  63, -77), S(  32, -81),
 S( -42,-106), S(  74, -90), S(  53, -79), S( -31, -71),
 S( -76,-103), S(  41, -94), S( -34, -71), S( -78, -63),
 S(-119, -95), S( -23, -90), S( -16, -79), S( -57, -71),
 S( -59,-102), S( -10, -94), S( -10, -88), S( -23, -82),
 S( -61,-112), S( -50, -97), S( -53, -86), S( -70, -80),
 S( -63,-144), S( -57,-120), S( -52,-103), S( -55,-109),
};

const Score KNIGHT_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  -7,  25), S(  21,  24), S(  40,  15), S(  28,  33),
 S(  20,  13), S(  45,  18), S(  36,  28), S(  47,  27),
 S(  21,   3), S(  32,  18), S(  24,  19), S(  25,  30),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score BISHOP_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  24,  11), S(  37,  11), S(  48,  16), S(  44,   1),
 S(  18,  10), S(  30,  15), S(  38,   8), S(  41,   5),
 S(  -3,  19), S(  35,  12), S(  29,  10), S(  39,  14),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -87, -63), S( -67, -20), S( -55,   7), S( -47,  15),
 S( -39,  21), S( -33,  26), S( -27,  27), S( -21,  24),
 S( -16,  16),};

const Score BISHOP_MOBILITIES[14] = {
 S( -54, -39), S( -38, -22), S( -28,  -8), S( -22,   8),
 S( -12,  16), S(  -7,  23), S(  -4,  28), S(  -3,  32),
 S(   0,  35), S(   0,  37), S(   6,  34), S(  21,  35),
 S(  25,  43), S(  21,  42),};

const Score ROOK_MOBILITIES[15] = {
 S(-117,-129), S( -69, -23), S( -59,  -8), S( -52, -10),
 S( -55,   7), S( -55,  15), S( -58,  21), S( -55,  22),
 S( -49,  22), S( -46,  26), S( -44,  27), S( -44,  29),
 S( -41,  32), S( -37,  33), S( -27,  26),};

const Score QUEEN_MOBILITIES[28] = {
 S(-291,-211), S(-194, -34), S( -96,-138), S(-101,   2),
 S( -98,  74), S( -97,  79), S( -98, 104), S( -95, 115),
 S( -94, 128), S( -91, 127), S( -89, 131), S( -88, 134),
 S( -87, 137), S( -85, 135), S( -84, 139), S( -81, 129),
 S( -83, 128), S( -81, 127), S( -84, 118), S( -77, 104),
 S( -66,  78), S( -49,  54), S( -41,  22), S( -40,   7),
 S( -30, -22), S( 105,-122), S( 150,-232), S(-108, -65),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(4, 16);

const Score BISHOP_OUTPOST_REACHABLE = S(5, 4);

const Score BISHOP_TRAPPED = S(-132, -84);

const Score ROOK_TRAPPED = S(-43, -18);

const Score ROOK_OPEN_FILE = S(18, 16);

const Score ROOK_SEMI_OPEN = S(2, 11);

const Score ROOK_OPPOSITE_KING = S(27, -14);

const Score ROOK_ADJACENT_KING = S(10, -24);

const Score DOUBLED_PAWN = S(-6, -20);

const Score OPPOSED_ISOLATED_PAWN = S(-6, -9);

const Score OPEN_ISOLATED_PAWN = S(-13, -11);

const Score BACKWARDS_PAWN = S(-8, -11);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  77,   6), S(  28,  17), S(  12,  10),
 S(   6,   3), S(   2,   1), S(   0,  -2), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  29, 105), S(  12, 167), S(   0,  93),
 S( -12,  48), S(  -6,  27), S(  -2,  23), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-4, -6);

const Score PASSED_PAWN_KING_PROXIMITY = S(-6, 22);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(23, 17);

const Score KNIGHT_THREATS[6] = { S(0, 19), S(1, 8), S(35, 30), S(67, 7), S(43, -43), S(159, 9),};

const Score BISHOP_THREATS[6] = { S(2, 17), S(26, 34), S(11, 10), S(45, 15), S(47, 75), S(96, 81),};

const Score ROOK_THREATS[6] = { S(3, 23), S(26, 32), S(29, 38), S(-1, 28), S(71, 17), S(152, 17),};

const Score KING_THREATS[6] = { S(18, 56), S(21, 45), S(1, 36), S(25, 20), S(-103, -347), S(0, 0),};

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
    C.bishopPair[side] = 1;
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
                 - (KS_ENEMY_QUEEN * !board->pieces[QUEEN[xside]])                       //
                 - (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT_TYPE] & kingArea)) //
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

#ifdef TUNE
  C.ss = s >= 0 ? WHITE : BLACK;
#endif

  s = s * Scale(board, s >= 0 ? WHITE : BLACK) / 100;

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
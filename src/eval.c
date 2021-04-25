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
const Score MATERIAL_VALUES[7] = { S(88, 126), S(311, 319), S(315, 314), S(524, 549), S(1030, 1053), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(17, 21);

const Score PAWN_PSQT[32] = {
 S(  -2,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  23,  43), S(  21,  40), S(  29,  37), S(  25,  24),
 S(  11,  25), S(  18,  23), S(  20,  16), S(   7,   6),
 S( -14,   8), S(  -6,  -4), S(  -4, -15), S(  -1, -23),
 S( -16,  -6), S( -18,  -7), S(  -8, -20), S(  -1, -20),
 S(  -8,  -4), S(  -2,  -6), S(  -4, -13), S(   1, -16),
 S( -13,  -2), S(  -8,  -2), S(  14,  -8), S(   1,  -6),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S( -32, -29), S( -30, -27), S( -30, -21), S( -29, -13),
 S( -14,  -8), S(  -5,   0), S(  28,  -5), S(   0,  -1),
 S(   0,  -9), S(  18,  -7), S(  -1,   6), S(  16,   6),
 S(  10,  -4), S(   0,   0), S(  11,   7), S(  14,  10),
 S(   3,   8), S(   6,   3), S(   9,   7), S(  11,  13),
 S(  -6,  -1), S(   3,  -1), S(   4,   2), S(   6,   9),
 S(  -5,  13), S(   0,   5), S(   3,   0), S(   7,   6),
 S( -19, -15), S(  -5,  -5), S(   5,  -2), S(   3,  -1),
};

const Score BISHOP_PSQT[32] = {
 S( -27,  -7), S( -27,  22), S( -35,  14), S( -32,   5),
 S(  -4,  -1), S( -19,  15), S(   4,  10), S( -12,  14),
 S(   9,   7), S(   6,   7), S( -16,  12), S(  11,   3),
 S(  -9,  16), S(   7,   9), S(   0,   5), S(   8,  10),
 S(  15,   3), S(  -1,   8), S(   2,  13), S(   5,   7),
 S(  10,   2), S(  13,  12), S(  13,  12), S(   7,  18),
 S(  11,   6), S(  18,   3), S(  16,   3), S(   9,  12),
 S(  12,   9), S(  25,  -1), S(  15,  20), S(   9,  14),
};

const Score ROOK_PSQT[32] = {
 S( -10,  13), S( -39,  24), S( -41,  27), S( -38,  22),
 S(   6,  -1), S( -16,  11), S(  -2,  12), S(   0,   6),
 S(   5,   2), S(  17,  -1), S(   7,   5), S(  12,  -4),
 S(  -2,   9), S(  -5,   8), S(  -7,   5), S(   0,   2),
 S( -19,  13), S( -11,   7), S( -29,  13), S( -10,   8),
 S( -14,   2), S( -10,   0), S( -17,   3), S( -11,   6),
 S( -15,   3), S( -14,   4), S( -14,   1), S(  -3,   1),
 S( -11,  -2), S(  -5,  -1), S(   0,  -4), S(   5, -12),
};

const Score QUEEN_PSQT[32] = {
 S(   6,  -7), S(   1,   8), S(  42,  -8), S(  23,   7),
 S(  -3,  -6), S( -18,   6), S( -10,  40), S( -20,  41),
 S(  23,  -2), S(  26,   4), S(   6,  24), S(  11,  27),
 S(   5,  16), S(  -5,  29), S(  -4,  23), S(  -7,  35),
 S(   0,  12), S(  -4,  21), S( -11,  25), S( -13,  33),
 S(   3,  -6), S(   6,  -2), S(  -7,  12), S(  -8,  10),
 S(   7, -23), S(  -1, -21), S(   9, -28), S(  -2,  -5),
 S(   9, -43), S(  -1, -17), S( -10, -19), S(  -3, -23),
};

const Score KING_PSQT[32] = {
 S( -41, -36), S(  21,  -4), S( -63,  23), S( -50,   6),
 S( -21,  -2), S(  15,  24), S(   7,  15), S(  21,  22),
 S( -45,  -2), S(   6,  17), S(  19,  20), S(  17,  21),
 S( -43,  -3), S(  -5,   9), S(   5,  18), S(  12,  20),
 S( -33, -15), S( -45,   4), S( -13,   8), S(   0,  12),
 S( -27,  -9), S( -18,   1), S( -20,   2), S( -15,   4),
 S(  14, -12), S(   9,  -2), S( -24,   4), S( -41,   9),
 S(   2, -21), S(  31, -20), S(  -6,  -6), S( -22, -13),
};

const Score KNIGHT_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( -12,  17), S(   7,  15), S(  24,  15), S(  20,  18),
 S(  23,   6), S(  29,  22), S(  24,  20), S(  27,  22),
 S(  16,  -7), S(  28,   9), S(  19,  13), S(  15,  18),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score BISHOP_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   6,   4), S(  11,  15), S(  31,  20), S(  19,   1),
 S(  25,  -4), S(  16,  10), S(  22,   8), S(  21,  10),
 S(  11,   4), S(  21,  11), S(  16,   7), S(  24,  17),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -25, -26), S( -15, -19), S(  -4,  -4), S(   0,   5),
 S(   2,  10), S(   6,   9), S(  11,   9), S(  17,  10),
 S(  20,   3),};

const Score BISHOP_MOBILITIES[14] = {
 S( -24, -24), S( -15, -16), S(  -5,  -9), S(  -1,   5),
 S(   8,   6), S(  13,   8), S(  15,  13), S(  15,  16),
 S(  17,  17), S(  16,  18), S(  18,  15), S(  25,  19),
 S(  26,  23), S(  28,  23),};

const Score ROOK_MOBILITIES[15] = {
 S( -40, -44), S( -16, -18), S( -12, -15), S(  -9, -14),
 S(  -2,  -8), S(  -4,  -1), S(  -4,  11), S(  -7,  14),
 S(  -8,  15), S(  -5,  16), S(  -1,  14), S(  -1,  13),
 S(   5,  14), S(   9,  18), S(   3,  21),};

const Score QUEEN_MOBILITIES[28] = {
 S( -50,   0), S( -29, -44), S( -29, -33), S( -26, -24),
 S( -19, -16), S( -15, -15), S(  -9,  -6), S(  -4,  -5),
 S(   2,   2), S(   7,   0), S(  10,   8), S(  10,  17),
 S(  12,  15), S(  13,  18), S(  14,  22), S(  13,  25),
 S(  11,  26), S(  13,  28), S(  17,  17), S(  14,  26),
 S(  18,  12), S(  26,  15), S(   9,  23), S(  10,  -7),
 S( -30,  39), S(  63, -21), S(  12, -27), S( -27,  16),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(3, 9);

const Score BISHOP_OUTPOST_REACHABLE = S(2, 4);

const Score BISHOP_TRAPPED = S(-40, -32);

const Score ROOK_TRAPPED = S(-20, -17);

const Score ROOK_OPEN_FILE = S(13, 10);

const Score ROOK_SEMI_OPEN = S(-5, 7);

const Score ROOK_OPPOSITE_KING = S(34, -4);

const Score ROOK_ADJACENT_KING = S(5, -9);

const Score DOUBLED_PAWN = S(-9, -19);

const Score OPPOSED_ISOLATED_PAWN = S(-3, 0);

const Score OPEN_ISOLATED_PAWN = S(-13, 4);

const Score BACKWARDS_PAWN = S(-3, -5);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  20,  22), S(  14,  14), S(  10,   5),
 S(   4,   3), S(   3,   1), S(   3,  -1), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  23,  40), S(  14,  23), S(   0,  21),
 S(  -5,   6), S(  -3,  -6), S(   2,  -7), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-6, 6);

const Score PASSED_PAWN_KING_PROXIMITY = S(-11, 12);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(16, 16);

const Score KNIGHT_THREATS[6] = { S(-4, 9), S(7, 14), S(24, 24), S(29, 25), S(20, 18), S(40, 15),};

const Score BISHOP_THREATS[6] = { S(-2, 11), S(19, 21), S(3, 18), S(25, 23), S(29, 29), S(38, 30),};

const Score ROOK_THREATS[6] = { S(1, 14), S(19, 22), S(24, 25), S(0, 13), S(28, 27), S(11, 32),};

const Score KING_THREATS[6] = { S(21, 24), S(-17, 27), S(10, 24), S(-6, 15), S(-53, -50), S(0, 0),};

const Score PAWN_SHELTER[4][8] = {
  { S(57, 54), S(-29, -44), S(-46, -34), S(28, 13), S(50, 41), S(-57, -49), S(-57, -31), S(0, 0),},
  { S(53, 53), S(-39, -43), S(-27, -24), S(52, 46), S(50, 48), S(-53, -41), S(-55, -57), S(0, 0),},
  { S(56, 64), S(-40, -32), S(-46, -19), S(52, 54), S(47, 47), S(41, 40), S(-59, -59), S(0, 0),},
  { S(-7, 7), S(0, 3), S(5, 6), S(6, 4), S(-9, -7), S(-7, -6), S(-6, -4), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
  { S(32, 42), S(-54, -59), S(-57, -4), S(-49, -59), S(49, 37), S(-42, -25), S(-47, -13), S(0, 0),},
  { S(38, 55), S(-58, -30), S(-56, -28), S(-54, -60), S(32, 22), S(41, 31), S(-53, -27), S(0, 0),},
  { S(-55, -32), S(36, 48), S(-53, -51), S(-56, -55), S(33, -5), S(43, 34), S(39, 32), S(0, 0),},
  { S(-8, 7), S(0, 4), S(-8, -6), S(-6, 9), S(8, 10), S(7, -6), S(-4, -7), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = { S(-40, -42), S(-50, -18), S(48, 49), S(24, 28), S(10, 7), S(32, 28), S(0, 0), S(0, 0),};

const Score KS_KING_FILE[4] = { S(-59, -42), S(26, 14), S(54, 57), S(48, 62),};

const Score KS_ATTACKER_WEIGHTS[5] = { S(0, 0), S(41, 37), S(24, 68), S(23, 38), S(33, 33),};

const Score KS_SAFE_CHECK = S(105, 138);

const Score KS_UNSAFE_CHECK = S(21, 35);

const Score KS_WEAK_SQS = S(40, 41);

const Score KS_ATTACK = S(15, 25);

const Score KS_ENEMY_QUEEN = S(-213, -230);

const Score KS_KNIGHT_DEFENSE = S(-52, -44);

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

#ifdef TUNE
        C.ksAttacker[xside][pieceType]++;
#endif
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
  Score shelter = S(0, 0);

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
#ifdef TUNE
    C.ksShelter[side][adjustedFile][pawnRank]++;
#endif

    BitBoard opponentPawnFile = opponentPawns & FILE_MASKS[file];
    int theirRank = opponentPawnFile ? (side ? 7 - rank(lsb(opponentPawnFile)) : rank(msb(opponentPawnFile))) : 0;
    if (pawnRank && pawnRank == theirRank + 1) {
      shelter += BLOCKED_PAWN_STORM[theirRank];
#ifdef TUNE
      C.ksBlockedStorm[side][theirRank]++;
#endif
    } else {
      shelter += PAWN_STORM[adjustedFile][theirRank];
#ifdef TUNE
      C.ksPawnStorm[side][adjustedFile][theirRank]++;
#endif
    }

    if (file == file(data->kingSq[side])) {
      int idx = 2 * !(board->pieces[PAWN[side]] & FILE_MASKS[file]) + !(board->pieces[PAWN[xside]] & FILE_MASKS[file]);
      shelter += KS_KING_FILE[idx];

#ifdef TUNE
      C.ksFile[side][idx]++;
#endif
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

  Score danger = data->ksAttackWeight[xside]                                             // weight of attacks
                 + (KS_SAFE_CHECK * safeChecks)                                          // safe checks
                 + (KS_UNSAFE_CHECK * unsafeChecks)                                      // unsafe checks
                 + (KS_WEAK_SQS * bits(weak & kingArea))                                 // weak sqs
                 + (KS_ATTACK * data->ksSqAttackCount[xside])                            // aimed pieces
                 + (KS_ENEMY_QUEEN * !board->pieces[QUEEN[xside]])                       // enemy has queen?
                 + (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT_TYPE] & kingArea)) // knight on f8 = no m8
                 - scoreMG(shelter) / 2;                                                 // house

  double mg = scoreMG(danger);
  double eg = scoreEG(danger);
  s += S(-mg * max(0, mg) / 1000, -max(0, eg) / 30);

#ifdef TUNE
  C.ksSafeChecks[side] = safeChecks;
  C.ksUnsafeChecks[side] = unsafeChecks;
  C.ksWeakSqs[side] = bits(weak & kingArea);
  C.ksSquareAttackCount[side] = data->ksSqAttackCount[xside];
  C.ksEnemyQueen[side] = !board->pieces[QUEEN[xside]];
  C.ksKnightDefense[side] = !!(data->attacks[side][KNIGHT_TYPE] & kingArea);
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
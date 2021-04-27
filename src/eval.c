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
const int STATIC_MATERIAL_VALUE[7] = {93, 310, 323, 548, 970, 30000, 0};
const int SHELTER_STORM_FILES[8][2] = {{0, 2}, {0, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6}, {5, 7}, {5, 7}};

// clang-format off
const Score MATERIAL_VALUES[7] = { S(84, 117), S(363, 359), S(383, 383), S(539, 696), S(1165, 1182), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(24, 59);

const Score PAWN_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  43, 177), S(  12, 188), S(  61, 160), S(  87, 143),
 S(   7,  29), S(   0,  25), S(  31,  -5), S(  30, -22),
 S( -14,  11), S(   0,   1), S(  -6,  -7), S(  13, -11),
 S( -19,   0), S( -13,  -1), S(  -3,  -6), S(   3,  -2),
 S( -16,  -5), S(   6,  -6), S(  -5,  -1), S(   6,   3),
 S( -14,  -2), S(   4,  -2), S(  -7,   5), S(   5,  14),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S(-111, -49), S( -59,  -9), S( -44,   5), S(  14,   6),
 S(   0,  -7), S(   2,  19), S(  50,  10), S(  44,  18),
 S(  33, -14), S(  36,   8), S(  29,  32), S(  54,  21),
 S(  41,  11), S(  29,  22), S(  43,  27), S(  49,  31),
 S(  36,  21), S(  42,  15), S(  44,  31), S(  45,  37),
 S(  17,  18), S(  31,  18), S(  27,  25), S(  40,  39),
 S(  28,  33), S(  33,  17), S(  28,  22), S(  34,  27),
 S(  -1,  33), S(  22,  15), S(  28,  21), S(  31,  29),
};

const Score BISHOP_PSQT[32] = {
 S( -38,  16), S( -20,  23), S( -68,  26), S( -81,  29),
 S( -24,  17), S( -29,  22), S( -12,  23), S(  -6,  20),
 S(  -3,  18), S( -17,  19), S( -15,  22), S(   0,  16),
 S( -24,  25), S(   4,  16), S(  -5,  20), S(   9,  24),
 S(  16,   8), S(  -8,  15), S(   1,  22), S(  10,  18),
 S(   5,  15), S(  19,  23), S(   9,  24), S(   7,  30),
 S(  17,  13), S(  24,  11), S(  16,  12), S(   1,  25),
 S(  13,  14), S(  18,  18), S(   5,  25), S(   2,  25),
};

const Score ROOK_PSQT[32] = {
 S(  18,  43), S(   6,  48), S(  -4,  57), S( -13,  59),
 S(  18,  31), S(   6,  43), S(  23,  39), S(  26,  32),
 S(   6,  29), S(  45,  24), S(  39,  28), S(  41,  20),
 S(   0,  36), S(  13,  31), S(  26,  32), S(  26,  28),
 S(  -4,  34), S(  10,  29), S(   0,  39), S(   9,  36),
 S(   0,  28), S(  16,  23), S(  14,  29), S(  13,  30),
 S(  -3,  36), S(  11,  28), S(  18,  26), S(  22,  29),
 S(  13,  28), S(  12,  28), S(  14,  30), S(  20,  23),
};

const Score QUEEN_PSQT[32] = {
 S( -38,  71), S( -34,  77), S(  -6,  79), S( -13,  97),
 S( -60,  87), S( -95, 126), S( -69, 149), S( -67, 131),
 S( -28,  66), S( -36, 104), S( -51, 127), S( -41, 131),
 S( -50, 120), S( -52, 140), S( -53, 132), S( -64, 157),
 S( -47, 111), S( -49, 134), S( -55, 139), S( -64, 158),
 S( -48, 102), S( -40, 104), S( -53, 127), S( -53, 121),
 S( -43,  76), S( -37,  62), S( -38,  78), S( -41,  93),
 S( -35,  56), S( -52,  77), S( -54,  78), S( -46,  69),
};

const Score KING_PSQT[32] = {
 S(-166,-198), S(  50,-146), S( -13, -98), S( -74,-101),
 S( -40,-114), S(  36, -76), S(  62, -70), S(  21, -74),
 S( -52,-106), S(  70, -87), S(  55, -73), S( -44, -63),
 S( -95,-102), S(  34, -92), S( -54, -63), S( -98, -55),
 S(-145, -92), S( -39, -86), S( -28, -74), S( -77, -64),
 S( -68,-101), S( -16, -92), S( -19, -84), S( -33, -77),
 S( -61,-113), S( -50, -96), S( -58, -83), S( -79, -76),
 S( -62,-151), S( -53,-124), S( -52,-104), S( -59,-110),
};

const Score KNIGHT_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  -9,  30), S(  21,  31), S(  47,  19), S(  34,  39),
 S(  22,  16), S(  50,  20), S(  39,  34), S(  54,  32),
 S(  24,   3), S(  34,  22), S(  28,  22), S(  28,  34),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score BISHOP_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  24,  14), S(  40,  13), S(  61,  17), S(  52,   0),
 S(  22,  11), S(  35,  17), S(  45,   8), S(  48,   5),
 S(  -3,  22), S(  41,  13), S(  34,  10), S(  45,  17),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -84, -58), S( -61,  -6), S( -47,  26), S( -38,  36),
 S( -28,  44), S( -22,  49), S( -15,  50), S(  -7,  47),
 S(  -2,  39),};

const Score BISHOP_MOBILITIES[14] = {
 S( -52, -33), S( -34, -12), S( -21,   4), S( -14,  23),
 S(  -3,  33), S(   4,  42), S(   7,  48), S(   9,  52),
 S(  12,  56), S(  13,  60), S(  19,  56), S(  38,  57),
 S(  43,  67), S(  41,  66),};

const Score ROOK_MOBILITIES[15] = {
 S(-122,-143), S( -69, -17), S( -58,   1), S( -50,  -3),
 S( -53,  17), S( -53,  27), S( -56,  35), S( -53,  36),
 S( -46,  35), S( -43,  40), S( -40,  41), S( -41,  45),
 S( -37,  49), S( -30,  49), S( -21,  43),};

const Score QUEEN_MOBILITIES[28] = {
 S(-507,-431), S(-253,  30), S( -91,-148), S( -99,  28),
 S( -95, 107), S( -93, 113), S( -94, 140), S( -91, 153),
 S( -89, 168), S( -85, 167), S( -83, 171), S( -81, 174),
 S( -80, 178), S( -77, 177), S( -77, 182), S( -74, 172),
 S( -76, 171), S( -75, 171), S( -79, 163), S( -72, 150),
 S( -59, 121), S( -41,  98), S( -32,  64), S( -37,  50),
 S( -20,  14), S( 131, -95), S( 143,-208), S(-348,  78),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(5, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 5);

const Score BISHOP_TRAPPED = S(-148, -100);

const Score ROOK_TRAPPED = S(-50, -22);

const Score ROOK_OPEN_FILE = S(21, 19);

const Score ROOK_SEMI_OPEN = S(2, 14);

const Score ROOK_OPPOSITE_KING = S(36, -17);

const Score ROOK_ADJACENT_KING = S(14, -26);

const Score DOUBLED_PAWN = S(-8, -24);

const Score OPPOSED_ISOLATED_PAWN = S(-6, -11);

const Score OPEN_ISOLATED_PAWN = S(-15, -13);

const Score BACKWARDS_PAWN = S(-9, -13);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  90,   7), S(  31,  18), S(  14,   9),
 S(   8,   2), S(   2,   1), S(   0,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -12, 103), S(   1,  33),
 S( -12,  25), S( -20,   7), S( -25,   6), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  31, 130), S(   5, 199), S(  -3, 110),
 S( -16,  57), S(  -8,  32), S(  -3,  27), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-6, -7);

const Score PASSED_PAWN_KING_PROXIMITY = S(-9, 26);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(25, 20);

const Score KNIGHT_THREATS[6] = { S(0, 18), S(0, 8), S(38, 26), S(70, -3), S(40, -61), S(187, -1),};

const Score BISHOP_THREATS[6] = { S(3, 18), S(29, 36), S(13, 7), S(45, 8), S(45, 75), S(104, 86),};

const Score ROOK_THREATS[6] = { S(5, 23), S(30, 33), S(34, 39), S(-4, 30), S(72, 6), S(174, 7),};

const Score KING_THREATS[6] = { S(24, 58), S(16, 42), S(-7, 34), S(15, 13), S(-93, -486), S(0, 0),};

const Score HANGING_THREAT = S(11, 9);

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

// Get the phase of the game
inline int GetPhase(Board* board) {
  int phase = 0;
  for (int i = KNIGHT_WHITE; i <= QUEEN_BLACK; i++)
    phase += PHASE_MULTIPLIERS[PIECE_TYPE[i]] * bits(board->pieces[i]);

  phase = min(MAX_PHASE, phase);
  return phase;
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
    return 50;

  int ssPawns = bits(board->pieces[PAWN[ss]]);
  if (ssPawns < 4)
    return 100 - 3 * (4 - ssPawns) * (4 - ssPawns);

  return 100;
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

// Standard pawn and passer evaluation
// TODO: Pawn hash table
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
    BitBoard levers = board->pieces[PAWN[xside]] & GetPawnAttacks(sq, side);
    BitBoard forwardLevers = board->pieces[PAWN[xside]] & GetPawnAttacks(sq + PAWN_DIRECTIONS[side], side);
    int backwards = !(neighbors & FORWARD_RANK_MASKS[xside][rank(sq + PAWN_DIRECTIONS[side])]) && forwardLevers;
    BitBoard passerSpan = FORWARD_RANK_MASKS[side][rank] & (ADJACENT_FILE_MASKS[file] | FILE_MASKS[file]);
    BitBoard antiPassers = board->pieces[xside] & passerSpan;
    int passed = !antiPassers &&
                 // make sure we don't double count passers
                 !(board->pieces[PAWN[side]] & FORWARD_RANK_MASKS[side][rank] & FILE_MASKS[file]);

    if (doubled) {
      s += DOUBLED_PAWN;

      if (T)
        C.doubledPawns[side]++;
    }

    if (!neighbors) {
      s += opposed ? OPPOSED_ISOLATED_PAWN : OPEN_ISOLATED_PAWN;

      if (T) {
        if (opposed)
          C.opposedIsolatedPawns[side]++;
        else
          C.openIsolatedPawns[side]++;
      }
    } else if (backwards) {
      s += BACKWARDS_PAWN;

      if (T)
        C.backwardsPawns[side]++;
    } else if (defenders | connected) {
      int scalar = 2 + !!connected - !!opposed;
      s += CONNECTED_PAWN[adjustedRank] * scalar;

      if (T)
        C.connectedPawn[side][adjustedRank] += scalar;

      // candidate passers are either in tension right now (and a push is all they need)
      // or a pawn 2 ranks down is stopping them, but our pawns can support it through
      if (!passed) {
        int onlyInTension = !(antiPassers ^ levers);
        int enoughSupport = !(antiPassers ^ forwardLevers) && bits(connected) >= bits(forwardLevers);

        if (onlyInTension | enoughSupport) {
          s += CANDIDATE_PASSER[adjustedRank];

          if (T)
            C.candidatePasser[side][adjustedRank]++;
        }
      }
    }

    if (passed) {
      s += PASSED_PAWN[adjustedRank] + PASSED_PAWN_EDGE_DISTANCE * adjustedFile;

      if (T) {
        C.passedPawn[side][adjustedRank]++;
        C.passedPawnEdgeDistance[side] += adjustedFile;
      }

      int advSq = sq + PAWN_DIRECTIONS[side];
      BitBoard advance = bit(advSq);

      if (adjustedRank <= 4) {
        int myDistance = distance(advSq, data->kingSq[side]);
        int opponentDistance = distance(advSq, data->kingSq[xside]);

        s += PASSED_PAWN_KING_PROXIMITY * min(4, max(opponentDistance - myDistance, -4));

        if (T)
          C.passedPawnKingProximity[side] += min(4, max(opponentDistance - myDistance, -4));

        if (!(board->occupancies[xside] & advance)) {
          BitBoard pusher = GetRookAttacks(sq, board->occupancies[BOTH]) & FILE_MASKS[file] &
                            FORWARD_RANK_MASKS[xside][rank] & (board->pieces[ROOK[side]] | board->pieces[QUEEN[side]]);

          if (pusher | (data->allAttacks[side] & advance)) {
            s += PASSED_PAWN_ADVANCE_DEFENDED;

            if (T)
              C.passedPawnAdvance[side]++;
          }
        }
      }
    }

    popLsb(pawns);
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

  BitBoard hangingPieces = board->occupancies[xside] & ~data->allAttacks[xside] & data->allAttacks[side];
  s += bits(hangingPieces) * HANGING_THREAT;

  if (T)
    C.hangingThreat[side] += bits(hangingPieces);

  return s;
}

// King safety is a quadratic based - there is a summation of smaller values
// into a single score, then that value is squared and diveded by 1000
// This fit for KS is strong as it can ignore a single piece attacking, but will
// spike on a secondary piece joining.
// It is heavily influenced by Toga, Rebel, SF, and Ethereal
Score KingSafety(Board* board, EvalData* data, int side) {
  Score s = 0;
  Score shelter = S(2, 2);

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

    BitBoard opponentPawnFile = opponentPawns & FILE_MASKS[file];
    int theirRank = opponentPawnFile ? (side ? 7 - rank(lsb(opponentPawnFile)) : rank(msb(opponentPawnFile))) : 0;
    if (pawnRank && pawnRank == theirRank + 1) {
      shelter += BLOCKED_PAWN_STORM[theirRank];
    } else {
      shelter += PAWN_STORM[adjustedFile][theirRank];
    }

    // the quality of the file the king is on is recorded
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
                 - (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT_TYPE] & kingArea)) // knight f8 = no m8
                 - scoreMG(shelter) / 2; // the quality of shelter is important

  // only include this if in danger
  if (danger > 0)
    s += S(-danger * danger / 1000, -danger / 30);

  s += shelter;

  // TODO: Utilize Texel tuning for these values
  if (T)
    C.ks[side] = s;

  return s;
}

// Main evalution method
Score Evaluate(Board* board) {
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
  s += PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);
  s += Threats(board, &data, WHITE) - Threats(board, &data, BLACK);
  s += KingSafety(board, &data, WHITE) - KingSafety(board, &data, BLACK);

  // taper
  int phase = GetPhase(board);
  s = (phase * scoreMG(s) + (MAX_PHASE - phase) * scoreEG(s)) / MAX_PHASE;

  if (T)
    C.ss = s >= 0 ? WHITE : BLACK;

  // scale the score
  s = s * Scale(board, s >= 0 ? WHITE : BLACK) / 100;
  return TEMPO + (board->side == WHITE ? s : -s);
}

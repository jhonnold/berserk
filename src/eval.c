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
const Score MATERIAL_VALUES[7] = { S(88, 134), S(392, 388), S(419, 419), S(600, 788), S(1287, 1361), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(28, 67);

const Score PAWN_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  80, 199), S(  29, 219), S(  41, 194), S(  69, 164),
 S(  -2,  47), S(  -9,  47), S(  18,  20), S(  34, -11),
 S( -18,  25), S(  -5,  10), S( -10,   3), S(  10, -12),
 S( -20,   8), S( -15,   4), S(  -4,  -3), S(   7,  -6),
 S( -16,   3), S(   1,   1), S(  -1,   1), S(  10,   5),
 S( -14,  10), S(  10,   5), S(  -3,   8), S(  11,  12),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_PSQT[32] = {
 S(-124, -44), S( -68,   3), S( -45,  17), S(  12,  19),
 S(   2,   2), S(   8,  31), S(  63,  22), S(  50,  33),
 S(  57,  -8), S(  52,  20), S(  39,  49), S(  67,  37),
 S(  54,  27), S(  44,  39), S(  53,  45), S(  62,  50),
 S(  47,  37), S(  54,  32), S(  56,  49), S(  59,  57),
 S(  25,  33), S(  40,  34), S(  37,  41), S(  50,  58),
 S(  39,  45), S(  41,  30), S(  36,  38), S(  43,  43),
 S(   8,  46), S(  31,  30), S(  35,  38), S(  40,  47),
};

const Score BISHOP_PSQT[32] = {
 S( -45,  30), S( -22,  36), S( -68,  39), S( -88,  45),
 S( -23,  26), S( -29,  35), S( -10,  36), S(  -6,  32),
 S(  22,  25), S(  -3,  30), S(   1,  33), S(   8,  28),
 S( -20,  40), S(  14,  29), S(   5,  33), S(  16,  40),
 S(  24,  20), S(  -1,  28), S(   7,  37), S(  17,  33),
 S(  10,  27), S(  26,  37), S(  16,  38), S(  14,  46),
 S(  23,  24), S(  30,  23), S(  23,  25), S(   6,  39),
 S(  18,  26), S(  25,  33), S(   8,  41), S(   7,  40),
};

const Score ROOK_PSQT[32] = {
 S(  26,  53), S(   8,  61), S(  -9,  74), S( -16,  75),
 S(  23,  42), S(   9,  57), S(  29,  53), S(  31,  44),
 S(  12,  40), S(  55,  35), S(  46,  40), S(  48,  31),
 S(   3,  49), S(  17,  44), S(  32,  46), S(  30,  40),
 S(  -3,  46), S(  15,  42), S(   2,  53), S(  12,  49),
 S(   2,  41), S(  21,  35), S(  19,  41), S(  16,  42),
 S(  -2,  50), S(  14,  39), S(  21,  39), S(  25,  41),
 S(  15,  41), S(  14,  41), S(  16,  43), S(  23,  35),
};

const Score QUEEN_PSQT[32] = {
 S( -34,  90), S( -34, 102), S(   6,  97), S(  -8, 120),
 S( -57, 102), S( -94, 144), S( -68, 177), S( -70, 164),
 S( -21,  86), S( -26, 122), S( -45, 153), S( -36, 157),
 S( -45, 141), S( -48, 169), S( -51, 158), S( -62, 186),
 S( -45, 137), S( -46, 161), S( -52, 164), S( -61, 186),
 S( -45, 122), S( -37, 128), S( -51, 154), S( -51, 147),
 S( -43,  97), S( -36,  81), S( -34,  97), S( -39, 115),
 S( -34,  77), S( -52,  96), S( -55,  98), S( -44,  87),
};

const Score KING_PSQT[32] = {
 S(-169,-212), S(  44,-147), S( -42, -84), S( -99, -81),
 S( -69,-108), S(  19, -62), S(  39, -45), S(   3, -43),
 S( -69,-101), S(  69, -71), S(  63, -51), S( -59, -31),
 S(-118,-102), S(  26, -84), S( -58, -46), S( -98, -28),
 S(-169,-101), S( -56, -91), S( -39, -68), S( -93, -45),
 S( -77,-116), S( -16,-104), S( -32, -82), S( -56, -62),
 S( -58,-122), S( -52, -98), S( -62, -83), S( -93, -70),
 S( -57,-163), S( -50,-126), S( -50,-111), S( -62,-118),
};

const Score KNIGHT_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( -24,  37), S(  18,  35), S(  54,  22), S(  40,  45),
 S(  24,  18), S(  53,  23), S(  45,  39), S(  60,  35),
 S(  29,   3), S(  38,  24), S(  27,  27), S(  30,  36),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score BISHOP_POST_PSQT[32] = {
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   8,  22), S(  36,  18), S(  60,  21), S(  61,  -3),
 S(  22,  14), S(  36,  19), S(  46,   9), S(  52,   4),
 S(  -7,  26), S(  43,  15), S(  40,   9), S(  48,  19),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -86, -54), S( -57,  10), S( -39,  44), S( -29,  56),
 S( -17,  65), S(  -9,  71), S(   0,  72), S(  10,  68),
 S(  18,  58),};

const Score BISHOP_MOBILITIES[14] = {
 S( -50, -26), S( -29,  -2), S( -13,  17), S(  -6,  39),
 S(   7,  51), S(  15,  60), S(  18,  68), S(  19,  74),
 S(  22,  78), S(  21,  83), S(  29,  78), S(  49,  81),
 S(  57,  89), S(  53,  90),};

const Score ROOK_MOBILITIES[15] = {
 S(-107,-156), S( -70, -16), S( -57,   5), S( -47,   3),
 S( -51,  27), S( -51,  37), S( -55,  46), S( -51,  47),
 S( -44,  48), S( -41,  53), S( -36,  55), S( -36,  59),
 S( -32,  63), S( -24,  66), S( -12,  58),};

const Score QUEEN_MOBILITIES[28] = {
 S(-921,-830), S(-273, -11), S( -88,-144), S( -97,  38),
 S( -94, 131), S( -90, 135), S( -92, 167), S( -88, 184),
 S( -86, 199), S( -82, 200), S( -79, 204), S( -78, 210),
 S( -77, 213), S( -75, 213), S( -74, 218), S( -71, 208),
 S( -74, 209), S( -72, 206), S( -78, 199), S( -68, 185),
 S( -54, 154), S( -35, 131), S( -24,  93), S( -36,  82),
 S( -33,  54), S( 140, -69), S( 227,-211), S(-446, 145),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(8, 20);

const Score BISHOP_OUTPOST_REACHABLE = S(7, 5);

const Score BISHOP_TRAPPED = S(-174, -114);

const Score ROOK_TRAPPED = S(-57, -27);

const Score ROOK_OPEN_FILE = S(23, 22);

const Score ROOK_SEMI_OPEN = S(1, 18);

const Score ROOK_OPPOSITE_KING = S(40, -19);

const Score ROOK_ADJACENT_KING = S(16, -28);

const Score DOUBLED_PAWN = S(-6, -31);

const Score OPPOSED_ISOLATED_PAWN = S(-6, -13);

const Score OPEN_ISOLATED_PAWN = S(-16, -16);

const Score BACKWARDS_PAWN = S(-10, -14);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  88,  39), S(  35,  32), S(  18,  11),
 S(   9,   2), S(   3,   1), S(   1,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -29, 190), S(  -5,  43),
 S( -16,  30), S( -23,   7), S( -34,   2), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  31, 158), S(  -2, 231), S(  -4, 120),
 S( -20,  63), S( -12,  36), S(  -5,  31), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-5, -9);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 24);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(26, 24);

const Score KNIGHT_THREATS[6] = { S(-2, 21), S(1, 8), S(42, 30), S(80, -5), S(46, -68), S(213, -3),};

const Score BISHOP_THREATS[6] = { S(5, 20), S(32, 42), S(13, 8), S(49, 8), S(51, 83), S(117, 100),};

const Score ROOK_THREATS[6] = { S(6, 27), S(34, 38), S(38, 45), S(-1, 30), S(84, 10), S(199, 9),};

const Score KING_THREATS[6] = { S(14, 35), S(23, 40), S(-4, 34), S(13, 13), S(-98, -603), S(0, 0),};

const Score PAWN_THREAT = S(64, 13);

const Score HANGING_THREAT = S(3, 10);

const Score PAWN_SHELTER[4][8] = {
 { S(-16, 20), S(68, 20), S(-17, 61), S(5, 25), S(17, 10), S(39, -1), S(37, -20), S(0, 0),},
 { S(-43, 14), S(-40, 55), S(-1, 39), S(-28, 28), S(-16, 13), S(23, -3), S(28, -10), S(0, 0),},
 { S(-12, 0), S(17, 37), S(-8, 31), S(5, 12), S(1, 5), S(5, 5), S(39, -5), S(0, 0),},
 { S(-22, 0), S(0, 45), S(-58, 24), S(-22, 21), S(-14, 7), S(-10, 0), S(-11, 4), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-32, -35), S(-21, -21), S(-19, -22), S(-29, -12), S(-52, 23), S(-11, 133), S(157, 158), S(0, 0),},
 { S(-18, -24), S(-6, -23), S(-6, -18), S(-16, -8), S(-28, 14), S(-115, 149), S(43, 190), S(0, 0),},
 { S(11, -24), S(17, -30), S(12, -22), S(3, -13), S(-23, 14), S(-149, 132), S(-90, 178), S(0, 0),},
 { S(0, -17), S(2, -34), S(8, -22), S(0, -27), S(-22, -3), S(-54, 92), S(-74, 153), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-34, -7), S(45, -70), S(12, -49), S(11, -41), S(-3, -32), S(-41, -38), S(0, 0), S(0, 0),
};

const Score KS_KING_FILE[4] = { S(15, -4), S(15, -1), S(6, 3), S(-27, -2),};

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
  s = (phase * scoreMG(s) + (MAX_PHASE - phase) * scoreEG(s)) / MAX_PHASE;

  if (T)
    C.ss = s >= 0 ? WHITE : BLACK;

  // scale the score
  s = s * Scale(board, s >= 0 ? WHITE : BLACK) / MAX_SCALE;
  return TEMPO + (board->side == WHITE ? s : -s);
}

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
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "endgame.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "pawns.h"
#include "search.h"
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
const Score MATERIAL_VALUES[7] = { S(100, 100), S(325, 325), S(325, 325), S(550, 550), S(1000, 1000), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(20, 67);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  32, 198), S(  24, 216), S(  15, 188), S(  50, 158),
 S( -38,  77), S( -52,  82), S( -23,  35), S( -15,   7),
 S( -42,  58), S( -40,  39), S( -36,  25), S( -35,  16),
 S( -47,  38), S( -48,  34), S( -38,  21), S( -29,  19),
 S( -52,  31), S( -51,  28), S( -46,  26), S( -40,  29),
 S( -45,  35), S( -39,  33), S( -41,  31), S( -37,  33),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  83, 162), S(   0, 153), S(  48, 162), S(  31, 156),
 S( -36,  38), S( -22,  25), S(  -4,  11), S( -11,  -3),
 S( -52,  28), S( -34,  17), S( -43,  17), S( -18,   8),
 S( -64,  23), S( -44,  20), S( -49,  19), S( -21,  13),
 S( -73,  20), S( -43,  15), S( -52,  26), S( -34,  25),
 S( -66,  29), S( -30,  24), S( -48,  31), S( -33,  37),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-109,  35), S( -69,  44), S( -53,  60), S( -29,  45),
 S( -24,  56), S( -15,  65), S(  16,  53), S(  17,  56),
 S(  22,  40), S(  21,  56), S(  19,  70), S(  12,  71),
 S(  18,  65), S(  21,  67), S(  34,  69), S(  21,  72),
 S(  17,  67), S(  22,  64), S(  32,  77), S(  26,  75),
 S(   0,  52), S(   5,  58), S(  14,  68), S(  11,  76),
 S( -12,  57), S(   3,  51), S(   4,  61), S(   9,  62),
 S( -36,  65), S(   1,  52), S(  -2,  54), S(   5,  65),
},{
 S(-137, -97), S(-125,  16), S( -88,  23), S(   9,  39),
 S( -13,   2), S(  63,  25), S(  42,  40), S(   9,  56),
 S(  28,  15), S(   2,  30), S(  45,  22), S(  44,  49),
 S(  14,  47), S(  24,  49), S(  26,  60), S(  25,  68),
 S(  17,  62), S(   3,  58), S(  33,  68), S(  18,  81),
 S(  10,  52), S(  25,  49), S(  18,  62), S(  23,  72),
 S(  13,  49), S(  23,  46), S(  10,  53), S(   9,  59),
 S(   2,  54), S(  -4,  53), S(  10,  50), S(  12,  61),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -17,  79), S( -14,  93), S( -24,  84), S( -36,  94),
 S(  -5,  89), S( -14,  78), S(  21,  81), S(  13,  88),
 S(  20,  87), S(  19,  89), S(  -3,  75), S(  21,  84),
 S(  13,  91), S(  32,  86), S(  22,  88), S(   9, 104),
 S(  27,  78), S(  21,  86), S(  25,  90), S(  34,  87),
 S(  24,  74), S(  36,  86), S(  26,  73), S(  28,  86),
 S(  25,  75), S(  27,  51), S(  35,  68), S(  23,  82),
 S(  19,  55), S(  33,  70), S(  24,  83), S(  29,  82),
},{
 S( -42,  47), S(  38,  69), S( -28,  78), S( -50,  77),
 S(  -9,  53), S( -23,  70), S(  -8,  90), S(  20,  73),
 S(  32,  75), S(  -1,  82), S(   7,  79), S(  16,  84),
 S(   3,  77), S(  32,  80), S(  29,  86), S(  26,  88),
 S(  40,  59), S(  33,  78), S(  36,  89), S(  26,  89),
 S(  33,  68), S(  48,  74), S(  34,  72), S(  29,  94),
 S(  35,  56), S(  35,  52), S(  37,  77), S(  32,  82),
 S(  30,  46), S(  27,  95), S(  21,  86), S(  37,  79),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -53, 161), S( -42, 163), S( -42, 166), S( -68, 170),
 S( -35, 147), S( -42, 155), S( -23, 153), S( -10, 140),
 S( -45, 152), S( -14, 147), S( -24, 147), S( -23, 142),
 S( -40, 162), S( -26, 157), S( -14, 156), S( -10, 142),
 S( -47, 161), S( -37, 161), S( -34, 160), S( -23, 147),
 S( -48, 155), S( -38, 146), S( -32, 148), S( -30, 143),
 S( -47, 150), S( -38, 147), S( -28, 146), S( -24, 139),
 S( -39, 150), S( -38, 148), S( -36, 152), S( -27, 138),
},{
 S(  -2, 151), S( -45, 167), S(-109, 179), S( -48, 162),
 S( -32, 139), S( -22, 147), S( -34, 143), S( -31, 143),
 S( -52, 138), S(   0, 131), S( -19, 118), S(   3, 122),
 S( -44, 142), S(  -7, 131), S( -10, 123), S(  -6, 133),
 S( -57, 142), S( -36, 139), S( -31, 132), S( -17, 139),
 S( -47, 124), S( -31, 123), S( -27, 122), S( -22, 132),
 S( -48, 135), S( -19, 117), S( -25, 121), S( -21, 130),
 S( -30, 128), S( -33, 135), S( -30, 130), S( -23, 131),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -55, 335), S(   3, 259), S(  54, 236), S(  25, 274),
 S(  -7, 281), S( -10, 270), S(   9, 289), S(   8, 284),
 S(  15, 265), S(  24, 255), S(  11, 285), S(  27, 280),
 S(  18, 291), S(  21, 307), S(  24, 304), S(   4, 320),
 S(  26, 272), S(  14, 318), S(  16, 314), S(   7, 331),
 S(  21, 266), S(  28, 271), S(  23, 288), S(  20, 291),
 S(  26, 240), S(  30, 241), S(  35, 244), S(  34, 254),
 S(  22, 249), S(  20, 243), S(  23, 249), S(  29, 252),
},{
 S(  62, 221), S( 112, 192), S( -49, 303), S(  36, 238),
 S(  37, 201), S(  20, 209), S(   6, 274), S(  -6, 288),
 S(  44, 205), S(  16, 199), S(  27, 227), S(  21, 271),
 S(  18, 252), S(  30, 250), S(  41, 227), S(   8, 300),
 S(  37, 222), S(  31, 258), S(  31, 267), S(  13, 320),
 S(  32, 207), S(  39, 232), S(  34, 263), S(  24, 288),
 S(  53, 187), S(  50, 187), S(  46, 210), S(  37, 254),
 S(  52, 193), S(  25, 219), S(  27, 208), S(  31, 234),
}};

const Score KING_PSQT[2][32] = {{
 S( 125, -86), S( 142, -42), S(  23, -13), S( 107, -32),
 S( -36,  30), S(  39,  36), S(  58,  31), S(  83,   6),
 S(  59,  15), S(  91,  30), S(  97,  31), S(  50,  31),
 S(  76,  13), S(  76,  29), S(  30,  36), S(  -5,  34),
 S(  -7,  11), S(  39,  19), S(  46,  19), S( -25,  24),
 S( -17,  -3), S(   8,  11), S(   2,   9), S( -17,   6),
 S(   2, -18), S( -15,  10), S( -31,   3), S( -47,  -3),
 S(   7, -66), S(  15, -28), S(  -7, -24), S( -29, -34),
},{
 S(  72,-106), S(-187,  40), S( -37,  -6), S(  -6, -21),
 S(-116,   4), S(   0,  48), S(   9,  44), S( 159,   7),
 S(  11,   3), S( 140,  43), S( 253,  30), S( 121,  29),
 S( -33,   3), S(  66,  32), S(  47,  44), S(   4,  41),
 S( -50,  -2), S( -20,  23), S(  13,  26), S( -14,  30),
 S(  -2, -10), S(  24,   9), S(   8,  12), S(  -2,  11),
 S(   5,  -8), S(   1,  14), S( -25,  10), S( -46,   3),
 S(   4, -47), S(   8,  -8), S( -14, -12), S( -12, -37),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -36,  17), S(   8,  21), S(  26,  19), S(  50,  28),
 S(  13,   1), S(  34,  13), S(  25,  22), S(  36,  28),
 S(  16,  -3), S(  29,   6), S(  17,  11), S(  16,  18),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -7,   9), S(  24,   4), S(  46,  12), S(  54,   3),
 S(   1,  -2), S(  21,  10), S(  36,   4), S(  45,   8),
 S(  -5,  18), S(  37,   6), S(  29,   5), S(  31,  16),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -43, -30), S( -16,  38), S(  -1,  69), S(   9,  80),
 S(  17,  87), S(  23,  94), S(  31,  94), S(  37,  92),
 S(  45,  81),};

const Score BISHOP_MOBILITIES[14] = {
 S( -10,  17), S(   4,  44), S(  15,  62), S(  20,  80),
 S(  26,  87), S(  29,  95), S(  31, 100), S(  34, 103),
 S(  35, 104), S(  38, 106), S(  44, 101), S(  59,  97),
 S(  65,  99), S(  65,  96),};

const Score ROOK_MOBILITIES[15] = {
 S( -58, -35), S( -44,  88), S( -32, 117), S( -25, 120),
 S( -27, 139), S( -25, 148), S( -29, 155), S( -25, 156),
 S( -21, 158), S( -16, 160), S( -13, 162), S( -13, 164),
 S( -10, 166), S(  -1, 167), S(  18, 158),};

const Score QUEEN_MOBILITIES[28] = {
 S(-609,-641), S( -56,-227), S(  23,  22), S(  11, 202),
 S(  15, 259), S(  19, 275), S(  18, 296), S(  18, 316),
 S(  19, 331), S(  21, 335), S(  23, 339), S(  24, 339),
 S(  24, 340), S(  27, 339), S(  26, 339), S(  31, 327),
 S(  30, 320), S(  30, 318), S(  36, 291), S(  56, 258),
 S(  74, 217), S(  91, 172), S( 134, 122), S( 166,  43),
 S( 210, -18), S( 287,-112), S( 165,-130), S(  99,-163),
};

const Score MINOR_BEHIND_PAWN = S(4, 15);

const Score KNIGHT_OUTPOST_REACHABLE = S(9, 16);

const Score BISHOP_OUTPOST_REACHABLE = S(5, 6);

const Score BISHOP_TRAPPED = S(-113, -178);

const Score ROOK_TRAPPED = S(-51, -18);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(19, 14);

const Score ROOK_OPEN_FILE = S(22, 14);

const Score ROOK_SEMI_OPEN = S(5, 14);

const Score DEFENDED_PAWN = S(10, 9);

const Score DOUBLED_PAWN = S(7, -26);

const Score OPPOSED_ISOLATED_PAWN = S(-3, -7);

const Score OPEN_ISOLATED_PAWN = S(-8, -15);

const Score BACKWARDS_PAWN = S(-8, -14);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  81,  44), S(  27,  33), S(  10,  10),
 S(   5,   2), S(   4,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 107, 107), S(  15,  44),
 S(  -4,  32), S( -14,  15), S( -27,   2), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  26, 191), S(  13, 234), S(  -1, 119),
 S( -17,  64), S(  -6,  39), S(  -6,  36), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-7, 22);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(21, 26);

const Score KNIGHT_THREATS[6] = { S(0, 18), S(-2, 9), S(26, 31), S(71, 2), S(45, -49), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 15), S(19, 27), S(-46, 70), S(51, 15), S(54, 78), S(0, 0),};

const Score ROOK_THREATS[6] = { S(3, 23), S(30, 34), S(29, 40), S(6, 16), S(64, 12), S(0, 0),};

const Score KING_THREAT = S(14, 27);

const Score PAWN_THREAT = S(63, 23);

const Score PAWN_PUSH_THREAT = S(16, 14);

const Score HANGING_THREAT = S(6, 8);

const Score SPACE[15] = {
 S( 713, -14), S( 129, -10), S(  53,  -5), S(  20,  -2), S(   5,   0),
 S(  -3,   2), S(  -6,   3), S(  -8,   5), S(  -7,   5), S(  -5,   7),
 S(  -3,   8), S(  -1,  11), S(   1,  10), S(   3,   5), S(   7,-270),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-21, -5), S(34, 44), S(-5, 41), S(-15, 9), S(2, -7), S(25, -24), S(23, -43), S(0, 0),},
 { S(-32, -5), S(26, 39), S(-10, 38), S(-23, 15), S(-20, 0), S(21, -20), S(26, -31), S(0, 0),},
 { S(-17, -8), S(16, 47), S(-33, 36), S(-10, 10), S(-12, -2), S(-2, -6), S(27, -17), S(0, 0),},
 { S(-24, 8), S(48, 41), S(-48, 33), S(-15, 20), S(-15, 13), S(-8, 6), S(-11, 6), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-16, -13), S(-20, -4), S(-19, -9), S(-28, 1), S(-49, 28), S(26, 136), S(217, 167), S(0, 0),},
 { S(-15, -4), S(2, -8), S(4, -7), S(-11, 4), S(-25, 19), S(-53, 141), S(72, 200), S(0, 0),},
 { S(30, -15), S(27, -16), S(17, -8), S(4, -2), S(-16, 17), S(-61, 119), S(-28, 181), S(0, 0),},
 { S(0, -8), S(3, -18), S(15, -13), S(5, -14), S(-16, 7), S(-26, 90), S(-97, 184), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-12, -6), S(29, -51), S(16, -37), S(14, -30), S(4, -22), S(1, -61), S(0, 0), S(0, 0),
};

const Score KS_ATTACKER_WEIGHTS[5] = {
 0, 32, 31, 18, 24
};

const Score KS_ATTACK = 4;

const Score KS_WEAK_SQS = 78;

const Score KS_PINNED = 74;

const Score KS_SAFE_CHECK = 248;

const Score KS_UNSAFE_CHECK = 57;

const Score KS_ENEMY_QUEEN = -190;

const Score KS_KNIGHT_DEFENSE = -87;

const Score TEMPO = 20;
// clang-format on

Score PSQT[12][2][64];
Score KNIGHT_POSTS[2][64];
Score BISHOP_POSTS[2][64];

void InitPSQT() {
  for (int sq = 0; sq < 64; sq++) {
    for (int i = 0; i < 2; i++) {
      PSQT[PAWN_WHITE][i][sq] = PSQT[PAWN_BLACK][i][MIRROR[sq]] =
          PAWN_PSQT[i][psqtIdx(sq)] + MATERIAL_VALUES[PAWN_TYPE];
      PSQT[KNIGHT_WHITE][i][sq] = PSQT[KNIGHT_BLACK][i][MIRROR[sq]] =
          KNIGHT_PSQT[i][psqtIdx(sq)] + MATERIAL_VALUES[KNIGHT_TYPE];
      PSQT[BISHOP_WHITE][i][sq] = PSQT[BISHOP_BLACK][i][MIRROR[sq]] =
          BISHOP_PSQT[i][psqtIdx(sq)] + MATERIAL_VALUES[BISHOP_TYPE];
      PSQT[ROOK_WHITE][i][sq] = PSQT[ROOK_BLACK][i][MIRROR[sq]] =
          ROOK_PSQT[i][psqtIdx(sq)] + MATERIAL_VALUES[ROOK_TYPE];
      PSQT[QUEEN_WHITE][i][sq] = PSQT[QUEEN_BLACK][i][MIRROR[sq]] =
          QUEEN_PSQT[i][psqtIdx(sq)] + MATERIAL_VALUES[QUEEN_TYPE];
      PSQT[KING_WHITE][i][sq] = PSQT[KING_BLACK][i][MIRROR[sq]] = KING_PSQT[i][psqtIdx(sq)];
    }

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
  return MAX_SCALE - (8 - ssPawns) * (8 - ssPawns);
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

Score NonPawnMaterialValue(Board* board) {
  Score s = 0;

  for (int pc = KNIGHT_WHITE; pc <= QUEEN_BLACK; pc++)
    s += bits(board->pieces[pc]) * scoreMG(MATERIAL_VALUES[PIECE_TYPE[pc]]);

  return s;
}

// Material + PSQT value
// Berserk uses reflected PSQTs in 2 states (same side as enemy king or not)
// A similar idea has been implemented in koi and is the inspiration for this
// simpler implementation
Score MaterialValue(Board* board, int side) {
  Score s = S(0, 0);

  uint8_t eks = SQ_SIDE[lsb(board->pieces[KING[side ^ 1]])];

  for (int pc = PAWN[side]; pc <= KING[side]; pc += 2) {
    BitBoard pieces = board->pieces[pc];

    if (T)
      C.pieces[PIECE_TYPE[pc]] += cs[side] * bits(pieces);

    while (pieces) {
      int sq = lsb(pieces);

      s += PSQT[pc][eks == SQ_SIDE[sq]][sq];

      popLsb(pieces);

      if (T)
        C.psqt[PIECE_TYPE[pc]][eks == SQ_SIDE[sq]][psqtIdx(side == WHITE ? sq : MIRROR[sq])] += cs[side];
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
  BitBoard allPawns = myPawns | opponentPawns;

  // bishop pair bonus first
  if (bits(board->pieces[BISHOP[side]]) > 1) {
    s += BISHOP_PAIR;

    if (T)
      C.bishopPair += cs[side];
  }

  BitBoard minorsBehindPawns = (board->pieces[KNIGHT[side]] | board->pieces[BISHOP[side]]) &
                               (side == WHITE ? ShiftS(allPawns) : ShiftN(allPawns));
  s += bits(minorsBehindPawns) * MINOR_BEHIND_PAWN;

  if (T)
    C.minorBehindPawn += bits(minorsBehindPawns) * cs[side];

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

        if (T) {
          C.ksAttackerCount[xside]++;
          C.ksAttackerWeights[xside][pieceType]++;
          C.ksAttack[xside] += bits(movement & enemyKingArea);
        }
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
  Score s = S(0, 0);

  int xside = side ^ 1;

  BitBoard nonPawnEnemies = board->occupancies[xside] & ~board->pieces[PAWN[xside]];
  BitBoard weak =
      ~(data->attacks[xside][PAWN_TYPE] | (data->twoAttacks[xside] & ~data->twoAttacks[side])) & data->allAttacks[side];

  for (int piece = KNIGHT_TYPE; piece <= ROOK_TYPE; piece++) {
    BitBoard threats = board->occupancies[xside] & data->attacks[side][piece];
    if (piece == KNIGHT_TYPE || piece == BISHOP_TYPE)
      threats &= nonPawnEnemies | weak;
    else
      threats &= weak;

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

  BitBoard kingThreats = weak & data->attacks[side][KING_TYPE] & board->occupancies[xside];
  if (kingThreats) {
    s += KING_THREAT;

    if (T)
      C.kingThreat += cs[side];
  }

  BitBoard pawnThreats = nonPawnEnemies & data->attacks[side][PAWN_TYPE];
  s += bits(pawnThreats) * PAWN_THREAT;

  BitBoard hangingPieces = board->occupancies[xside] & ~data->allAttacks[xside] & data->allAttacks[side];
  s += bits(hangingPieces) * HANGING_THREAT;

  BitBoard pawnPushes = ~board->occupancies[BOTH] &
                        (side == WHITE ? ShiftN(board->pieces[PAWN_WHITE]) : ShiftS(board->pieces[PAWN_BLACK]));
  pawnPushes |= ~board->occupancies[BOTH] & (side == WHITE ? ShiftN(pawnPushes & RANK_3) : ShiftS(pawnPushes & RANK_6));
  pawnPushes &= (data->allAttacks[side] | ~data->allAttacks[xside]);
  BitBoard pawnPushAttacks =
      (side == WHITE ? ShiftNE(pawnPushes) | ShiftNW(pawnPushes) : ShiftSE(pawnPushes) | ShiftSW(pawnPushes));
  pawnPushAttacks &= nonPawnEnemies;

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
  if (T) {
    C.ks += s * cs[side];
    C.danger[side] = danger;

    C.ksSafeCheck[side] = safeChecks;
    C.ksUnsafeCheck[side] = unsafeChecks;
    C.ksWeakSqs[side] = bits(weak & kingArea);
    C.ksPinned[side] = bits(board->pinned & board->occupancies[side]);
    C.ksEnemyQueen[side] = !board->pieces[QUEEN[xside]];
    C.ksKnightDefense[side] = !!(data->attacks[side][KNIGHT_TYPE] & kingArea);
  }

  s += shelter;
  return s;
}

// Space evaluation
// space is determined by # of pieces (scalar) and number of squares in the center
// that are behind pawns and not occupied by allied pawns or attacked by enemy
// pawns.
// https://www.chessprogramming.org/Space, Laser, Chess22k, and SF as references
Score Space(Board* board, EvalData* data, int side) {
  Score s = S(0, 0);
  int xside = side ^ 1;

  static const BitBoard CENTER_FILES = (C_FILE | D_FILE | E_FILE | F_FILE);

  BitBoard space = side == WHITE ? ShiftS(board->pieces[PAWN[side]]) : ShiftN(board->pieces[PAWN[side]]);
  space |= side == WHITE ? (ShiftS(space) | ShiftSS(space)) : (ShiftN(space) | ShiftNN(space));
  space &= ~(board->pieces[PAWN[side]] | data->attacks[xside][PAWN_TYPE]);
  space &= CENTER_FILES;

  s += SPACE[bits(board->occupancies[side]) - 2] * bits(space);

  if (T)
    C.space[bits(board->occupancies[side]) - 2] += cs[side] * bits(space);

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

  if (T || abs(scoreMG(s) + scoreEG(s)) / 2 < 640) {
    s += PieceEval(board, &data, WHITE) - PieceEval(board, &data, BLACK);
    s += PasserEval(board, &data, WHITE) - PasserEval(board, &data, BLACK);
    s += Threats(board, &data, WHITE) - Threats(board, &data, BLACK);
    s += KingSafety(board, &data, WHITE) - KingSafety(board, &data, BLACK);
    s += Space(board, &data, WHITE) - Space(board, &data, BLACK);
  }

  // taper
  int phase = GetPhase(board);
  Score res = (phase * scoreMG(s) + (128 - phase) * scoreEG(s)) / 128;

  if (T)
    C.ss = res >= 0 ? WHITE : BLACK;

  // scale the score
  res = (res * Scale(board, res >= 0 ? WHITE : BLACK)) / MAX_SCALE;
  return TEMPO + (board->side == WHITE ? res : -res);
}

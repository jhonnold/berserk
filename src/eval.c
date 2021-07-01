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

const Score BISHOP_PAIR = S(20, 80);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  19, 226), S(  34, 241), S(  -1, 232), S(  30, 203),
 S( -41,  98), S( -47, 113), S( -20,  57), S( -15,  28),
 S( -47,  78), S( -42,  62), S( -38,  44), S( -39,  34),
 S( -54,  56), S( -49,  56), S( -43,  42), S( -36,  41),
 S( -58,  46), S( -53,  48), S( -51,  44), S( -47,  51),
 S( -49,  50), S( -40,  55), S( -45,  49), S( -39,  55),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  97, 179), S(  39, 177), S(  60, 187), S(  20, 205),
 S( -30,  53), S( -12,  47), S(   2,  22), S( -15,  20),
 S( -51,  43), S( -32,  36), S( -50,  31), S( -19,  26),
 S( -65,  36), S( -47,  40), S( -57,  36), S( -26,  34),
 S( -79,  33), S( -50,  34), S( -58,  42), S( -39,  46),
 S( -72,  44), S( -35,  44), S( -52,  47), S( -37,  62),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S( -99,  48), S(-112,  77), S( -64,  87), S( -27,  70),
 S( -16,  84), S(  -5,  91), S(  11,  82), S(  14,  83),
 S(  24,  68), S(  23,  81), S(  31,  93), S(  11,  99),
 S(  16,  95), S(  17,  95), S(  32,  99), S(  17, 104),
 S(  10,  98), S(  18,  89), S(  28, 110), S(  24, 103),
 S(  -6,  76), S(  -1,  83), S(   7,  97), S(   4, 107),
 S( -20,  79), S(  -5,  80), S(  -2,  90), S(   2,  90),
 S( -46,  81), S(  -5,  80), S(  -7,  78), S(  -2,  94),
},{
 S(-138, -97), S(-203,  60), S( -57,  46), S(  14,  66),
 S(  23,  17), S(  92,  45), S(  27,  75), S(  32,  75),
 S(  33,  43), S(  -4,  63), S(  41,  45), S(  42,  79),
 S(  15,  75), S(  22,  78), S(  25,  91), S(  24,  99),
 S(  13,  94), S(   0,  88), S(  32,  99), S(  14, 113),
 S(  12,  77), S(  20,  77), S(  13,  91), S(  18, 104),
 S(   8,  73), S(  14,  71), S(   4,  82), S(   4,  88),
 S(  -5,  87), S(  -6,  79), S(   5,  76), S(   8,  90),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -16, 108), S( -24, 128), S( -32, 112), S( -45, 128),
 S(  -4, 116), S( -15, 108), S(  25, 106), S(   4, 116),
 S(  12, 121), S(  20, 120), S(  -7, 103), S(  20, 115),
 S(  13, 119), S(  29, 115), S(  23, 119), S(   9, 139),
 S(  23, 107), S(  21, 120), S(  20, 122), S(  32, 119),
 S(  18, 110), S(  33, 114), S(  22, 100), S(  23, 119),
 S(  21, 104), S(  23,  74), S(  34,  97), S(  20, 111),
 S(  12,  82), S(  33,  98), S(  21, 115), S(  23, 111),
},{
 S( -32,  72), S(  29, 101), S( -40, 108), S( -39, 100),
 S(  -9,  78), S( -33, 106), S(   0, 120), S(  28, 101),
 S(  25, 111), S(   1, 113), S(   6, 105), S(  22, 115),
 S(   3, 107), S(  27, 114), S(  29, 118), S(  23, 120),
 S(  38,  88), S(  27, 114), S(  33, 121), S(  21, 125),
 S(  33,  92), S(  45, 106), S(  27, 104), S(  25, 126),
 S(  30,  90), S(  31,  78), S(  34, 108), S(  27, 112),
 S(  27,  62), S(  28, 129), S(  20, 115), S(  35, 107),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -67, 227), S( -55, 229), S( -66, 236), S( -93, 236),
 S( -51, 210), S( -51, 215), S( -32, 213), S( -21, 200),
 S( -61, 215), S( -26, 211), S( -36, 208), S( -32, 204),
 S( -55, 227), S( -37, 221), S( -27, 221), S( -21, 203),
 S( -63, 226), S( -51, 226), S( -45, 220), S( -38, 212),
 S( -65, 217), S( -55, 206), S( -47, 208), S( -46, 206),
 S( -64, 208), S( -56, 210), S( -41, 207), S( -39, 198),
 S( -55, 213), S( -55, 210), S( -52, 214), S( -42, 201),
},{
 S(  -1, 211), S( -65, 238), S(-126, 245), S( -58, 226),
 S( -41, 199), S( -33, 210), S( -49, 211), S( -51, 206),
 S( -60, 199), S(   1, 191), S( -35, 177), S(  -7, 183),
 S( -50, 202), S( -11, 193), S( -19, 180), S( -13, 194),
 S( -72, 200), S( -52, 206), S( -44, 192), S( -29, 199),
 S( -57, 183), S( -39, 182), S( -40, 181), S( -35, 194),
 S( -55, 187), S( -26, 172), S( -38, 180), S( -34, 187),
 S( -45, 186), S( -47, 196), S( -45, 191), S( -36, 191),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-130, 498), S( -20, 370), S( -21, 401), S( -13, 411),
 S( -45, 412), S( -40, 386), S( -24, 410), S( -53, 447),
 S( -20, 387), S( -18, 377), S( -22, 396), S( -13, 413),
 S( -19, 409), S( -21, 428), S( -14, 429), S( -37, 447),
 S( -17, 396), S( -26, 442), S( -29, 441), S( -35, 462),
 S( -16, 375), S( -15, 395), S( -17, 409), S( -22, 415),
 S( -13, 351), S( -13, 363), S(  -5, 361), S(  -7, 374),
 S( -26, 375), S( -23, 362), S( -19, 367), S( -14, 379),
},{
 S(  54, 351), S( 139, 307), S(-143, 467), S(  -8, 371),
 S(  11, 355), S(  11, 329), S( -31, 394), S( -47, 438),
 S(  -1, 355), S( -11, 315), S( -11, 363), S( -11, 398),
 S( -18, 378), S(  -8, 384), S(   6, 354), S( -21, 425),
 S(  -3, 349), S(  -8, 382), S( -12, 396), S( -33, 456),
 S(  -8, 333), S(  -1, 355), S(  -4, 383), S( -21, 422),
 S(  10, 309), S(  14, 299), S(   6, 324), S(  -3, 375),
 S(   4, 341), S( -23, 347), S( -17, 331), S( -10, 356),
}};

const Score KING_PSQT[2][32] = {{
 S( 213,-100), S( 117, -38), S( -15, -14), S( 103, -38),
 S( -23,  33), S(  36,  38), S(  30,  30), S(  77,  -1),
 S(  60,  26), S( 104,  32), S( 107,  33), S(  50,  35),
 S(  84,  23), S(  64,  37), S(  42,  37), S(   8,  32),
 S(  19,  13), S(  32,  26), S(  30,  25), S( -56,  32),
 S( -12,  -3), S(   5,  15), S(  -7,  13), S( -28,   8),
 S(   9, -18), S( -17,  15), S( -34,   2), S( -47,  -6),
 S(   6, -68), S(  19, -30), S(  -8, -28), S( -25, -43),
},{
 S(  64,-102), S(-226,  58), S(  46, -36), S( -60, -20),
 S(-128,  15), S(  19,  54), S( -88,  60), S( 164,   0),
 S( -10,   9), S( 142,  50), S( 231,  33), S( 132,  31),
 S( -46,   9), S(  33,  46), S(  28,  57), S(  -1,  47),
 S( -55,   2), S( -30,  32), S(   8,  31), S( -31,  36),
 S( -12,  -8), S(  23,  13), S(  -7,  16), S( -13,  10),
 S(  10,  -8), S(   7,  14), S( -27,   7), S( -47,  -3),
 S(   9, -53), S(  15, -11), S( -15, -19), S( -10, -44),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -39,  12), S(   6,  24), S(  20,  27), S(  60,  28),
 S(   8,   3), S(  34,  17), S(  23,  25), S(  35,  34),
 S(  19,  -1), S(  26,   7), S(  21,  10), S(  17,  20),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -9,  12), S(  22,   2), S(  47,  17), S(  49,  13),
 S(  -5,  -5), S(  21,   9), S(  34,   8), S(  48,  13),
 S(  -6,  18), S(  42,   3), S(  36,   8), S(  35,  20),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -53, -10), S( -25,  73), S(  -9, 109), S(   3, 120),
 S(  11, 128), S(  19, 138), S(  28, 135), S(  34, 135),
 S(  44, 122),};

const Score BISHOP_MOBILITIES[14] = {
 S( -21,  49), S(  -1,  78), S(   9, 100), S(  15, 119),
 S(  21, 127), S(  24, 137), S(  26, 143), S(  31, 145),
 S(  30, 147), S(  35, 147), S(  42, 145), S(  52, 140),
 S(  48, 144), S(  64, 134),};

const Score ROOK_MOBILITIES[15] = {
 S( -17, -55), S( -58, 139), S( -44, 175), S( -37, 181),
 S( -38, 202), S( -35, 211), S( -41, 220), S( -36, 220),
 S( -31, 224), S( -26, 226), S( -22, 229), S( -21, 232),
 S( -18, 234), S(  -9, 236), S(   8, 227),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1282,-1144), S( -83,-300), S( -73, 260), S( -47, 365),
 S( -44, 418), S( -38, 433), S( -38, 457), S( -39, 485),
 S( -37, 501), S( -34, 505), S( -33, 514), S( -30, 511),
 S( -30, 513), S( -27, 512), S( -29, 510), S( -22, 500),
 S( -22, 493), S( -21, 488), S(  -8, 457), S(  12, 425),
 S(  34, 381), S(  54, 332), S( 103, 288), S( 161, 187),
 S( 192, 150), S( 237,  62), S(  37, 101), S( 195, -34),
};

const Score MINOR_BEHIND_PAWN = S(4, 18);

const Score KNIGHT_OUTPOST_REACHABLE = S(11, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-59, -261);

const Score ROOK_TRAPPED = S(-49, -19);

const Score BAD_BISHOP_PAWNS = S(-1, -5);

const Score DRAGON_BISHOP = S(21, 17);

const Score ROOK_OPEN_FILE = S(24, 16);

const Score ROOK_SEMI_OPEN = S(6, 15);

const Score DEFENDED_PAWN = S(11, 10);

const Score DOUBLED_PAWN = S(10, -31);

const Score ISOLATED_PAWN[4] = {
 S(   4,  -6), S(   0, -13), S(  -6,  -6), S(   0, -13),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -9);

const Score BACKWARDS_PAWN = S(-10, -15);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  65,  48), S(  24,  38), S(  10,  11),
 S(   6,   2), S(   4,   1), S(   1,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 113, 145), S(  10,  75),
 S( -13,  57), S( -22,  36), S( -29,  10), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -9);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  23, 233), S(   5, 269), S(  -9, 137),
 S( -26,  72), S( -12,  45), S( -12,  41), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 25);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(23, 31);

const Score KNIGHT_THREATS[6] = { S(0, 19), S(-3, 41), S(27, 35), S(71, 6), S(51, -62), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(0, 18), S(19, 30), S(-34, 39), S(58, 22), S(76, 47), S(0, 0),};

const Score ROOK_THREATS[6] = { S(3, 24), S(29, 37), S(27, 44), S(6, 15), S(69, 9), S(0, 0),};

const Score KING_THREAT = S(13, 29);

const Score PAWN_THREAT = S(69, 33);

const Score PAWN_PUSH_THREAT = S(17, 17);

const Score HANGING_THREAT = S(8, 8);

const Score SPACE[15] = {
 S( 700, -27), S( 156, -16), S(  62,  -9), S(  22,  -5), S(   4,  -1),
 S(  -5,   1), S(  -9,   3), S( -10,   5), S( -10,   6), S(  -7,   8),
 S(  -3,   8), S(  -1,  10), S(   2,   8), S(   5,  -3), S(   7,-264),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-20, -5), S(-7, 74), S(-4, 47), S(-18, 10), S(4, -6), S(29, -25), S(26, -47), S(0, 0),},
 { S(-37, -4), S(13, 39), S(-30, 45), S(-30, 18), S(-26, 3), S(21, -19), S(29, -30), S(0, 0),},
 { S(-19, -10), S(1, 67), S(-39, 41), S(-10, 10), S(-13, -4), S(-2, -8), S(28, -18), S(0, 0),},
 { S(-25, 13), S(14, 53), S(-54, 40), S(-14, 24), S(-16, 20), S(-7, 10), S(-14, 10), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-17, -15), S(-25, -3), S(-23, -10), S(-27, 1), S(-54, 34), S(18, 157), S(255, 176), S(0, 0),},
 { S(-9, -5), S(3, -9), S(4, -8), S(-11, 6), S(-20, 21), S(-40, 156), S(128, 235), S(0, 0),},
 { S(39, -14), S(33, -15), S(25, -10), S(8, -2), S(-16, 20), S(-51, 125), S(-1, 200), S(0, 0),},
 { S(-1, -8), S(3, -14), S(15, -15), S(6, -13), S(-18, 8), S(-38, 101), S(-119, 218), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(26, -21), S(32, -54), S(18, -41), S(16, -34), S(4, -23), S(7, -72), S(0, 0), S(0, 0),
};

const Score KS_ATTACKER_WEIGHTS[5] = {
 0, 32, 31, 18, 24
};

const Score KS_ATTACK = 4;

const Score KS_WEAK_SQS = 78;

const Score KS_PINNED = 74;

const Score KS_KNIGHT_CHECK = 279;

const Score KS_BISHOP_CHECK = 311;

const Score KS_ROOK_CHECK = 272;

const Score KS_QUEEN_CHECK = 213;

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

  int unsafeChecks = bits((possibleKnightChecks | possibleBishopChecks | possibleRookChecks) & ~vulnerable);

  Score danger = data->ksAttackWeight[xside] * data->ksAttackerCount[xside] // Similar concept to toga's weight attack
                 + (KS_KNIGHT_CHECK * bits(possibleKnightChecks & vulnerable))  //
                 + (KS_BISHOP_CHECK * bits(possibleBishopChecks & vulnerable))  //
                 + (KS_ROOK_CHECK * bits(possibleRookChecks & vulnerable))      //
                 + (KS_QUEEN_CHECK * bits(possibleQueenChecks & vulnerable))    //
                 + (KS_UNSAFE_CHECK * unsafeChecks)                             //
                 + (KS_WEAK_SQS * bits(weak & kingArea))                        // weak sqs makes you vulnerable
                 + (KS_ATTACK * data->ksSqAttackCount[xside])                   // general pieces aimed
                 + (KS_PINNED * bits(board->pinned & board->occupancies[side])) //
                 + (KS_ENEMY_QUEEN * !board->pieces[QUEEN[xside]])              //
                 + (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT_TYPE] & kingArea)); // knight f8 = no m8

  // only include this if in danger
  if (danger > 0)
    s += S(-danger * danger / 1024, -danger / 32);

  // TODO: Utilize Texel tuning for these values
  if (T) {
    C.ks += s * cs[side];
    C.danger[side] = danger;

    C.ksKnightCheck[side] = bits(possibleKnightChecks & vulnerable);
    C.ksBishopCheck[side] = bits(possibleBishopChecks & vulnerable);
    C.ksRookCheck[side] = bits(possibleRookChecks & vulnerable);
    C.ksQueenCheck[side] = bits(possibleQueenChecks & vulnerable);
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

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

const Score BISHOP_PAIR = S(22, 87);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 109, 196), S( 103, 219), S(  84, 211), S( 120, 183),
 S(  69, 130), S(  55, 146), S(  85,  92), S(  93,  69),
 S(  67, 112), S(  68,  98), S(  72,  78), S(  70,  67),
 S(  62,  87), S(  61,  90), S(  68,  74), S(  78,  72),
 S(  56,  79), S(  57,  82), S(  62,  77), S(  65,  84),
 S(  65,  83), S(  71,  88), S(  67,  84), S(  74,  88),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 150, 152), S(  52, 154), S( 117, 149), S( 100, 184),
 S(  81,  77), S(  94,  68), S( 113,  45), S(  98,  57),
 S(  56,  74), S(  74,  68), S(  62,  61), S(  93,  56),
 S(  47,  66), S(  66,  71), S(  55,  68), S(  88,  64),
 S(  31,  66), S(  63,  67), S(  56,  74), S(  74,  80),
 S(  40,  76), S(  79,  78), S(  63,  83), S(  75,  99),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-191,  42), S(-199,  66), S(-167,  74), S(-125,  55),
 S(-118,  67), S(-100,  73), S( -73,  60), S( -70,  62),
 S( -63,  50), S( -65,  60), S( -68,  71), S( -76,  72),
 S( -72,  76), S( -67,  72), S( -50,  69), S( -63,  72),
 S( -72,  72), S( -65,  67), S( -53,  80), S( -59,  77),
 S( -93,  52), S( -85,  54), S( -73,  61), S( -78,  75),
 S(-104,  53), S( -88,  50), S( -86,  58), S( -79,  57),
 S(-129,  56), S( -89,  50), S( -92,  52), S( -82,  63),
},{
 S(-245,-107), S(-295,  44), S(-149,  27), S( -73,  53),
 S( -94,   4), S( -24,  28), S( -59,  58), S( -78,  64),
 S( -50,  22), S( -91,  41), S( -36,  21), S( -45,  51),
 S( -67,  54), S( -58,  52), S( -53,  60), S( -56,  67),
 S( -68,  68), S( -78,  61), S( -49,  73), S( -66,  86),
 S( -74,  47), S( -60,  46), S( -68,  54), S( -63,  72),
 S( -74,  49), S( -65,  48), S( -78,  51), S( -78,  54),
 S( -89,  55), S( -94,  52), S( -78,  48), S( -75,  61),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -28, 102), S( -44, 124), S( -31, 106), S( -50, 123),
 S(  -9, 106), S( -21,  95), S(  19, 100), S(   5, 112),
 S(  11, 109), S(  15, 111), S( -15,  94), S(  20, 102),
 S(   7, 108), S(  32, 104), S(  22, 108), S(  12, 125),
 S(  27,  95), S(  18, 107), S(  27, 108), S(  36, 105),
 S(  24,  88), S(  40, 101), S(  26,  86), S(  30, 101),
 S(  27,  84), S(  27,  58), S(  39,  76), S(  24,  96),
 S(  20,  61), S(  38,  77), S(  29,  94), S(  30,  95),
},{
 S( -67,  65), S(  36,  90), S( -42,  98), S( -69, 101),
 S(   2,  60), S( -30,  93), S( -10, 111), S(  18,  93),
 S(  30,  96), S(   1, 100), S(   3,  95), S(  18, 103),
 S(   2,  93), S(  34,  99), S(  33, 103), S(  31, 104),
 S(  45,  71), S(  33,  97), S(  39, 106), S(  29, 107),
 S(  36,  78), S(  53,  89), S(  34,  87), S(  31, 111),
 S(  39,  60), S(  39,  61), S(  40,  90), S(  33,  96),
 S(  34,  40), S(  30, 108), S(  23, 100), S(  38,  93),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-114, 221), S(-107, 226), S(-107, 229), S(-134, 232),
 S(-105, 214), S(-111, 225), S( -90, 223), S( -75, 208),
 S(-119, 221), S( -83, 215), S( -93, 217), S( -91, 210),
 S(-112, 234), S( -96, 230), S( -82, 227), S( -79, 211),
 S(-116, 230), S(-109, 231), S(-103, 227), S( -94, 216),
 S(-119, 220), S(-108, 208), S(-102, 211), S(-100, 208),
 S(-115, 205), S(-110, 210), S( -97, 206), S( -93, 198),
 S(-114, 216), S(-108, 208), S(-105, 212), S( -95, 198),
},{
 S( -49, 205), S(-105, 231), S(-163, 241), S(-106, 222),
 S( -82, 194), S( -68, 207), S( -93, 211), S(-103, 214),
 S(-109, 199), S( -54, 195), S( -83, 188), S( -60, 188),
 S(-102, 206), S( -66, 203), S( -74, 191), S( -72, 202),
 S(-119, 204), S( -99, 206), S(-100, 201), S( -86, 206),
 S(-107, 180), S( -89, 181), S( -93, 184), S( -89, 195),
 S(-101, 181), S( -78, 170), S( -92, 180), S( -88, 189),
 S(-111, 196), S( -92, 189), S( -97, 190), S( -90, 190),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-120, 384), S( -43, 290), S(  -6, 290), S( -18, 317),
 S( -53, 319), S( -45, 297), S( -29, 322), S( -37, 325),
 S( -25, 295), S( -17, 285), S( -27, 317), S(  -6, 310),
 S( -21, 316), S( -14, 331), S( -10, 330), S( -29, 343),
 S(  -8, 292), S( -21, 338), S( -18, 331), S( -24, 351),
 S( -12, 272), S(  -5, 286), S( -10, 304), S( -13, 303),
 S(  -4, 241), S(  -3, 249), S(   3, 250), S(   3, 262),
 S( -14, 259), S( -14, 251), S( -11, 257), S(  -4, 260),
},{
 S(  34, 263), S(  89, 245), S(-115, 371), S(  -6, 282),
 S(  18, 261), S(  -6, 257), S( -19, 300), S( -43, 335),
 S(  19, 257), S(  -7, 252), S(   6, 274), S( -11, 311),
 S(  -7, 288), S(   2, 293), S(  12, 266), S( -23, 337),
 S(   6, 258), S(   1, 287), S(  -1, 295), S( -21, 346),
 S(   2, 227), S(   9, 253), S(   4, 283), S( -10, 304),
 S(  22, 197), S(  21, 191), S(  13, 219), S(   6, 261),
 S(  16, 223), S( -14, 240), S( -12, 225), S(  -7, 246),
}};

const Score KING_PSQT[2][32] = {{
 S( 231,-143), S( 139, -62), S( -41, -14), S(  80, -40),
 S( -85,  48), S( -34,  66), S( -31,  57), S(   6,  25),
 S(  22,  36), S(  -8,  73), S(   8,  64), S( -44,  70),
 S(  32,  32), S(   9,  59), S( -20,  62), S( -48,  57),
 S( -25,  21), S(  12,  38), S(  23,  31), S( -49,  37),
 S( -25,  -3), S(   0,  14), S(  -7,  11), S( -18,   6),
 S(   6, -24), S( -18,   9), S( -25,  -5), S( -29, -14),
 S(  12, -81), S(  20, -39), S(   9, -39), S( -47, -38),
},{
 S(   0,-138), S(-282,  56), S(  -5, -29), S( -74, -29),
 S(-175,  12), S( -83,  86), S( -96,  73), S( 100,  20),
 S( -92,  27), S(  42,  85), S( 106,  74), S(  40,  64),
 S( -83,  17), S(   7,  60), S( -17,  75), S( -29,  63),
 S( -77,   6), S( -53,  41), S(  -2,  39), S( -27,  41),
 S(  -7, -11), S(  22,  14), S(   3,  14), S(   6,  10),
 S(  12, -13), S(   8,  12), S( -10,   2), S( -21,  -9),
 S(  14, -61), S(  18, -14), S(   2, -24), S( -33, -34),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -45,  11), S(   7,  20), S(  29,  24), S(  60,  38),
 S(  14,  -2), S(  39,  20), S(  30,  27), S(  42,  38),
 S(  21,  -2), S(  33,  11), S(  22,  14), S(  21,  24),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -3,   9), S(  24,   6), S(  59,  14), S(  61,  10),
 S(   2,   1), S(  25,  13), S(  40,   6), S(  52,  12),
 S(  -7,  25), S(  44,   8), S(  30,  13), S(  38,  21),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-159, -35), S(-118,  38), S(-100,  76), S( -87,  89),
 S( -78, 101), S( -70, 112), S( -61, 114), S( -53, 116),
 S( -45, 108),};

const Score BISHOP_MOBILITIES[14] = {
 S( -20,  47), S(   5,  71), S(  17,  92), S(  24, 112),
 S(  31, 119), S(  36, 129), S(  38, 135), S(  42, 138),
 S(  43, 140), S(  46, 141), S(  54, 138), S(  68, 132),
 S(  70, 139), S(  77, 126),};

const Score ROOK_MOBILITIES[15] = {
 S( -92, -79), S(-107, 135), S( -92, 170), S( -86, 177),
 S( -88, 199), S( -84, 207), S( -88, 217), S( -84, 218),
 S( -80, 224), S( -74, 228), S( -71, 231), S( -74, 237),
 S( -70, 238), S( -61, 242), S( -45, 234),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1778,-1353), S(-123,-413), S( -82,  98), S( -60, 231),
 S( -49, 275), S( -44, 289), S( -44, 319), S( -42, 341),
 S( -40, 357), S( -37, 360), S( -35, 366), S( -32, 366),
 S( -32, 372), S( -29, 371), S( -31, 374), S( -25, 365),
 S( -28, 365), S( -29, 364), S( -22, 342), S(  -5, 313),
 S(  13, 278), S(  28, 239), S(  68, 200), S( 110, 113),
 S(  71, 126), S( 197,  -9), S(  49,  -2), S(  36, -35),
};

const Score MINOR_BEHIND_PAWN = S(5, 14);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-114, -240);

const Score ROOK_TRAPPED = S(-43, -33);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(23, 17);

const Score ROOK_OPEN_FILE = S(28, 15);

const Score ROOK_SEMI_OPEN = S(15, 6);

const Score DEFENDED_PAWN = S(11, 11);

const Score DOUBLED_PAWN = S(10, -32);

const Score ISOLATED_PAWN[4] = {
 S(   1,  -6), S(  -1, -14), S(  -8,  -6), S(  -2, -12),
};

const Score OPEN_ISOLATED_PAWN = S(-4, -11);

const Score BACKWARDS_PAWN = S(-10, -17);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  83,  44), S(  28,  39), S(  11,  12),
 S(   6,   3), S(   5,   2), S(   2,   0), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 126, 162), S(  10,  76),
 S( -13,  61), S( -23,  38), S( -36,  14), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -11);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 113, 185), S(  39, 183), S(   7, 115),
 S( -16,  77), S( -12,  44), S( -11,  40), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-6, 23);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S(  74, 239), S(  15, 136), S(   9,  47), S(  13,  10),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(26, -108);

const Score PASSED_PAWN_SQ_RULE = S(0, 357);

const Score KNIGHT_THREATS[6] = { S(1, 19), S(-1, 44), S(29, 32), S(76, -1), S(51, -57), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 19), S(21, 33), S(-60, 86), S(58, 10), S(63, 85), S(0, 0),};

const Score ROOK_THREATS[6] = { S(0, 20), S(30, 40), S(30, 52), S(3, 22), S(76, -7), S(0, 0),};

const Score KING_THREAT = S(12, 30);

const Score PAWN_THREAT = S(71, 31);

const Score PAWN_PUSH_THREAT = S(18, 19);

const Score HANGING_THREAT = S(5, 9);

const Score SPACE[15] = {
 S( 504, -25), S( 144, -16), S(  59, -10), S(  20,  -5), S(   2,  -2),
 S(  -6,   1), S( -10,   3), S( -11,   6), S(  -9,   9), S(  -7,  13),
 S(  -4,  15), S(  -2,  20), S(   1,  18), S(   3,  13), S(   5,-342),
};

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(11, 26), S(0, 0),},
 { S(6, 22), S(-9, -29), S(0, 0),},
 { S(1, 37), S(-31, -19), S(-3, 1), S(0, 0),},
 { S(51, 50), S(-58, 18), S(-6, 81), S(-200, 156), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-23, -4), S(8, 97), S(-5, 49), S(-13, 8), S(6, -4), S(35, -25), S(32, -48), S(0, 0),},
 { S(-42, 0), S(-8, 76), S(-16, 42), S(-28, 19), S(-25, 6), S(25, -18), S(33, -32), S(0, 0),},
 { S(-23, -12), S(-1, 86), S(-37, 34), S(-12, 8), S(-15, -4), S(-5, -8), S(28, -19), S(0, 0),},
 { S(-37, 15), S(-7, 84), S(-59, 35), S(-28, 30), S(-27, 22), S(-16, 13), S(-25, 21), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-22, -10), S(-24, -3), S(-25, -8), S(-29, 3), S(-56, 32), S(59, 80), S(279, 76), S(0, 0),},
 { S(-14, 0), S(3, -6), S(3, -5), S(-11, 9), S(-27, 21), S(-28, 85), S(107, 131), S(0, 0),},
 { S(36, -8), S(33, -10), S(23, -5), S(5, 5), S(-16, 19), S(-45, 75), S(20, 101), S(0, 0),},
 { S(-4, -3), S(1, -10), S(13, -8), S(3, -11), S(-19, 4), S(-13, 45), S(-34, 116), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(4, -49), S(38, -51), S(17, -36), S(15, -31), S(3, -24), S(6, -85), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(43, -19);

const Score KS_ATTACKER_WEIGHTS[5] = {
 0, 33, 32, 19, 25
};

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
  data->passedPawns = 0ULL;

  BitBoard whitePawns = board->pieces[PAWN_WHITE];
  BitBoard blackPawns = board->pieces[PAWN_BLACK];
  BitBoard whitePawnAttacks = ShiftNE(whitePawns) | ShiftNW(whitePawns);
  BitBoard blackPawnAttacks = ShiftSE(blackPawns) | ShiftSW(blackPawns);

  data->allAttacks[WHITE] = data->attacks[WHITE][PAWN_TYPE] = whitePawnAttacks;
  data->allAttacks[BLACK] = data->attacks[BLACK][PAWN_TYPE] = blackPawnAttacks;
  data->attacks[WHITE][KNIGHT_TYPE] = data->attacks[BLACK][KNIGHT_TYPE] = 0ULL;
  data->attacks[WHITE][BISHOP_TYPE] = data->attacks[BLACK][BISHOP_TYPE] = 0ULL;
  data->attacks[WHITE][ROOK_TYPE] = data->attacks[BLACK][ROOK_TYPE] = 0ULL;
  data->attacks[WHITE][QUEEN_TYPE] = data->attacks[BLACK][QUEEN_TYPE] = 0ULL;
  data->attacks[WHITE][KING_TYPE] = data->attacks[BLACK][KING_TYPE] = 0ULL;
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
  data->ksAttackWeight[WHITE] = data->ksAttackWeight[BLACK] = 0;
  data->ksAttackerCount[WHITE] = data->ksAttackerCount[BLACK] = 0;

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
  Score s = S(0, 0);

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
        data->ksAttackerCount[side]++;

        if (T) {
          C.ksAttackerCount[xside]++;
          C.ksAttackerWeights[xside][pieceType]++;
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
            if (!(movement & FORWARD_RANK_MASKS[side][rank(sq)] & opponentPawns & data->attacks[xside][PAWN_TYPE])) {
              s += ROOK_SEMI_OPEN;

              if (T)
                C.rookSemiOpen += cs[side];
            }
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

  uint8_t rights = side == WHITE ? (board->castling & 0xC) : (board->castling & 0x3);

  shelter += CAN_CASTLE * bits((uint64_t)rights);
  if (T)
    C.castlingRights += cs[side] * bits((uint64_t)rights);

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

Score Imbalance(Board* board, int side) {
  Score s = S(0, 0);
  int xside = side ^ 1;

  for (int i = KNIGHT_TYPE; i < KING_TYPE; i++) {
    for (int j = PAWN_TYPE; j < i; j++) {
      s += IMBALANCE[i][j] * bits(board->pieces[2 * i + side]) * bits(board->pieces[2 * j + xside]);

      if (T)
        C.imbalance[i][j] += bits(board->pieces[2 * i + side]) * bits(board->pieces[2 * j + xside]) * cs[side];
    }
  }

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

  EvalData data;
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

  s += Imbalance(board, WHITE) - Imbalance(board, BLACK);

  if (T || abs(scoreMG(s) + scoreEG(s)) / 2 < 1024) {
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

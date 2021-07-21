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

const Score BISHOP_PAIR = S(20, 89);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 102, 216), S( 101, 239), S(  65, 235), S( 108, 196),
 S(  64, 153), S(  44, 168), S(  59, 112), S(  65,  85),
 S(  63, 133), S(  62, 116), S(  45, 100), S(  42,  85),
 S(  58, 106), S(  48, 107), S(  49,  91), S(  56,  86),
 S(  49,  97), S(  49, 100), S(  48,  92), S(  50,  97),
 S(  55, 100), S(  63, 102), S(  62,  98), S(  65,  94),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 144, 166), S(  60, 166), S( 164, 161), S(  89, 207),
 S(  81,  96), S(  88,  86), S( 103,  59), S(  72,  73),
 S(  54,  95), S(  69,  87), S(  50,  78), S(  67,  75),
 S(  42,  87), S(  54,  90), S(  50,  82), S(  67,  78),
 S(  23,  84), S(  54,  85), S(  52,  87), S(  60,  92),
 S(  28,  95), S(  71,  94), S(  61,  96), S(  62, 106),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-180,  67), S(-191,  94), S(-164, 106), S(-121,  83),
 S(-111, 101), S( -93, 105), S( -67,  90), S( -63,  91),
 S( -53,  82), S( -60,  91), S( -65, 102), S( -75, 103),
 S( -64, 107), S( -59, 101), S( -47, 100), S( -58, 104),
 S( -65, 102), S( -59,  99), S( -45, 107), S( -53, 109),
 S( -84,  80), S( -75,  81), S( -66,  88), S( -69, 105),
 S( -96,  82), S( -80,  79), S( -75,  85), S( -70,  84),
 S(-118,  88), S( -81,  81), S( -81,  82), S( -73,  95),
},{
 S(-225, -94), S(-254,  57), S(-132,  51), S( -57,  75),
 S( -85,  34), S( -18,  60), S( -52,  86), S( -69,  93),
 S( -39,  50), S( -82,  74), S( -26,  46), S( -44,  83),
 S( -55,  85), S( -51,  82), S( -45,  90), S( -53, 101),
 S( -59,  98), S( -68,  90), S( -42, 103), S( -59, 117),
 S( -65,  75), S( -50,  73), S( -60,  81), S( -55, 100),
 S( -65,  77), S( -55,  78), S( -68,  79), S( -69,  82),
 S( -75,  84), S( -87,  83), S( -69,  78), S( -64,  92),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -24, 137), S( -45, 155), S( -30, 134), S( -53, 155),
 S(  -3, 135), S( -16, 120), S(  22, 127), S(   5, 142),
 S(  14, 141), S(  16, 140), S( -12, 119), S(  19, 128),
 S(   8, 137), S(  33, 130), S(  21, 135), S(  13, 154),
 S(  29, 122), S(  17, 135), S(  27, 134), S(  36, 131),
 S(  24, 113), S(  42, 128), S(  27, 112), S(  30, 128),
 S(  29, 111), S(  27,  84), S(  42, 103), S(  24, 122),
 S(  21,  88), S(  40, 106), S(  28, 126), S(  32, 123),
},{
 S( -57,  92), S(  53, 112), S( -37, 124), S( -69, 132),
 S(   3,  88), S( -33, 122), S(  -7, 135), S(  18, 122),
 S(  34, 125), S(   5, 128), S(   8, 121), S(  18, 131),
 S(   5, 122), S(  34, 126), S(  37, 130), S(  33, 130),
 S(  44, 100), S(  36, 122), S(  38, 131), S(  32, 132),
 S(  38, 104), S(  53, 117), S(  36, 113), S(  30, 138),
 S(  40,  85), S(  39,  87), S(  40, 118), S(  33, 123),
 S(  39,  68), S(  29, 139), S(  23, 131), S(  39, 121),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-151, 248), S(-149, 257), S(-142, 257), S(-173, 260),
 S(-136, 241), S(-144, 253), S(-127, 252), S(-109, 232),
 S(-154, 251), S(-121, 246), S(-131, 246), S(-131, 240),
 S(-147, 263), S(-133, 261), S(-121, 258), S(-117, 240),
 S(-149, 258), S(-144, 259), S(-137, 255), S(-131, 246),
 S(-152, 247), S(-142, 235), S(-138, 239), S(-136, 235),
 S(-147, 229), S(-142, 235), S(-130, 232), S(-127, 224),
 S(-148, 237), S(-141, 231), S(-139, 236), S(-131, 223),
},{
 S( -92, 234), S(-156, 267), S(-208, 274), S(-148, 253),
 S(-118, 222), S(-108, 237), S(-129, 241), S(-139, 240),
 S(-141, 227), S( -92, 224), S(-114, 213), S( -95, 215),
 S(-135, 235), S(-101, 231), S(-111, 221), S(-107, 229),
 S(-150, 230), S(-131, 231), S(-133, 227), S(-119, 233),
 S(-138, 204), S(-121, 205), S(-126, 209), S(-123, 221),
 S(-133, 206), S(-109, 195), S(-124, 203), S(-121, 213),
 S(-144, 217), S(-124, 212), S(-132, 213), S(-123, 213),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -87, 451), S(  -1, 347), S(  38, 347), S(  23, 379),
 S(  -7, 378), S(  19, 337), S(  27, 372), S(  23, 374),
 S(  22, 353), S(  31, 342), S(  28, 365), S(  44, 364),
 S(  18, 375), S(  24, 391), S(  30, 388), S(  21, 390),
 S(  25, 354), S(  12, 398), S(  16, 390), S(   9, 412),
 S(  18, 337), S(  20, 354), S(  17, 370), S(  12, 372),
 S(  21, 305), S(  19, 316), S(  24, 321), S(  23, 336),
 S(   6, 330), S(   7, 324), S(   8, 331), S(  11, 334),
},{
 S(  67, 329), S( 113, 329), S( -70, 444), S(  39, 341),
 S(  56, 318), S(  41, 320), S(  39, 356), S(  17, 388),
 S(  53, 324), S(  39, 308), S(  55, 340), S(  43, 365),
 S(  20, 362), S(  31, 369), S(  51, 330), S(  29, 388),
 S(  29, 335), S(  26, 361), S(  33, 361), S(  14, 404),
 S(  26, 301), S(  32, 327), S(  32, 351), S(  18, 371),
 S(  42, 270), S(  41, 268), S(  35, 293), S(  28, 332),
 S(  34, 290), S(   3, 317), S(   9, 302), S(  16, 316),
}};

const Score KING_PSQT[2][32] = {{
 S( 213,-149), S( 125, -67), S(  -4, -25), S( 122, -56),
 S(-123,  69), S( -84,  90), S( -31,  67), S(  13,  30),
 S( -14,  53), S( -34,  87), S(   1,  76), S( -45,  81),
 S(  -8,  47), S(   5,  65), S( -26,  68), S( -43,  61),
 S( -31,  25), S(  10,  40), S(  31,  32), S( -37,  37),
 S( -26,  -4), S(   0,  13), S(  -4,  11), S( -19,   5),
 S(   6, -27), S( -22,   9), S( -25,  -7), S( -32, -15),
 S(  14, -87), S(  18, -42), S(   6, -41), S( -46, -40),
},{
 S(-100,-122), S(-277,  54), S( -14, -35), S( -35, -43),
 S(-202,  15), S(-116, 102), S(-124,  90), S( 132,  16),
 S( -99,  28), S(  23,  99), S( 155,  75), S(  43,  79),
 S( -73,  12), S(  19,  63), S(  13,  78), S(   1,  64),
 S( -75,   5), S( -42,  39), S(  10,  39), S( -11,  39),
 S(  -4, -14), S(  24,  12), S(   7,  14), S(  12,   8),
 S(  13, -15), S(   8,  11), S(  -8,   1), S( -23,  -9),
 S(  17, -65), S(  17, -17), S(   2, -25), S( -31, -35),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -49,  11), S(   7,  18), S(  28,  26), S(  58,  40),
 S(  14,  -2), S(  37,  22), S(  28,  27), S(  39,  37),
 S(  20,  -2), S(  33,  10), S(  19,  16), S(  20,  21),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -6,   6), S(  23,   1), S(  56,  16), S(  60,   6),
 S(   4,  -2), S(  24,  12), S(  41,   3), S(  50,   9),
 S(  -5,  22), S(  41,   8), S(  30,  13), S(  38,  21),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-147, -12), S(-108,  66), S( -91, 105), S( -79, 119),
 S( -70, 131), S( -63, 144), S( -56, 147), S( -48, 150),
 S( -39, 142),};

const Score BISHOP_MOBILITIES[14] = {
 S( -12,  72), S(  11, 104), S(  23, 124), S(  30, 145),
 S(  36, 154), S(  39, 164), S(  41, 171), S(  44, 175),
 S(  44, 179), S(  47, 181), S(  54, 178), S(  65, 174),
 S(  68, 177), S(  80, 162),};

const Score ROOK_MOBILITIES[15] = {
 S(-115, -49), S(-128, 172), S(-114, 205), S(-108, 206),
 S(-112, 225), S(-108, 231), S(-113, 239), S(-109, 239),
 S(-105, 245), S(-101, 249), S( -99, 254), S(-103, 260),
 S(-100, 263), S( -94, 268), S( -87, 262),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1894,-1387), S( -94,-252), S( -42, 210), S( -22, 343),
 S( -10, 382), S(  -4, 392), S(  -4, 423), S(  -3, 443),
 S(   0, 459), S(   3, 463), S(   6, 469), S(  10, 469),
 S(  10, 474), S(  14, 474), S(  14, 476), S(  21, 469),
 S(  19, 470), S(  18, 473), S(  26, 451), S(  45, 419),
 S(  61, 387), S(  67, 356), S( 106, 316), S( 159, 212),
 S( 134, 214), S( 179, 136), S(  37, 137), S( 292, -74),
};

const Score MINOR_BEHIND_PAWN = S(5, 13);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-109, -253);

const Score ROOK_TRAPPED = S(-37, -28);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(22, 18);

const Score ROOK_OPEN_FILE_OFFSET = S(17, 7);

const Score ROOK_OPEN_FILE = S(26, 16);

const Score ROOK_SEMI_OPEN = S(16, 7);

const Score ROOK_TO_OPEN = S(16, 18);

const Score QUEEN_OPPOSITE_ROOK = S(-14, 3);

const Score QUEEN_ROOK_BATTERY = S(3, 54);

const Score DEFENDED_PAWN = S(12, 8);

const Score DOUBLED_PAWN = S(16, -36);

const Score ISOLATED_PAWN[4] = {
 S(   0, -10), S(   0, -15), S(  -6,  -6), S(   1,  -8),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -10);

const Score BACKWARDS_PAWN = S(-8, -16);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(66, 17), S(0, 27), S(0, 8), S(0, -1), S(3, 1), S(4, -3), S(0, 0),},
 { S(0, 0), S(75, 29), S(17, 38), S(5, 12), S(9, 2), S(6, -1), S(0, 1), S(0, 0),},
 { S(0, 0), S(89, 70), S(32, 40), S(13, 12), S(6, 3), S(5, 3), S(1, -3), S(0, 0),},
 { S(0, 0), S(59, 85), S(28, 46), S(10, 17), S(6, 8), S(5, 5), S(5, 4), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 211, 145), S(  16,  85),
 S( -11,  71), S( -20,  48), S( -35,  24), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(3, -17);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 113, 206), S(  46, 198), S(  15, 123),
 S( -12,  84), S( -13,  45), S( -11,  40), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -12);

const Score PASSED_PAWN_KING_PROXIMITY = S(-6, 25);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S(  94, 274), S(  11, 146), S(   5,  51), S(  10,  11),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(29, -121);

const Score PASSED_PAWN_SQ_RULE = S(0, 386);

const Score PASSED_PAWN_UNSUPPORTED = S(-25, -5);

const Score KNIGHT_THREATS[6] = { S(-1, 20), S(-3, 43), S(34, 38), S(84, 12), S(70, -48), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(1, 20), S(22, 37), S(-66, 81), S(71, 21), S(60, 35), S(96, 1446),};

const Score ROOK_THREATS[6] = { S(-1, 23), S(30, 44), S(32, 56), S(7, 22), S(49, -37), S(199, 1054),};

const Score KING_THREAT = S(11, 38);

const Score PAWN_THREAT = S(83, 40);

const Score PAWN_PUSH_THREAT = S(18, 19);

const Score PAWN_PUSH_THREAT_PINNED = S(52, 130);

const Score HANGING_THREAT = S(11, 22);

const Score KNIGHT_CHECK_QUEEN = S(13, 1);

const Score BISHOP_CHECK_QUEEN = S(22, 23);

const Score ROOK_CHECK_QUEEN = S(19, 7);

const Score SPACE = 104;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(6, 24), S(0, 0),},
 { S(3, 19), S(-9, -29), S(0, 0),},
 { S(2, 35), S(-30, -18), S(-3, 1), S(0, 0),},
 { S(45, 42), S(-75, 26), S(-23, 86), S(-228, 168), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-21, -5), S(22, 109), S(-5, 55), S(-17, 7), S(7, -6), S(38, -26), S(33, -49), S(0, 0),},
 { S(-44, -1), S(-15, 88), S(-22, 50), S(-33, 20), S(-28, 6), S(24, -20), S(32, -33), S(0, 0),},
 { S(-22, -13), S(-2, 94), S(-39, 38), S(-14, 7), S(-15, -4), S(-5, -8), S(28, -19), S(0, 0),},
 { S(-38, 18), S(29, 80), S(-60, 37), S(-32, 29), S(-30, 23), S(-18, 14), S(-27, 24), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-19, -12), S(-24, -2), S(-24, -7), S(-29, 5), S(-59, 32), S(59, 81), S(268, 77), S(0, 0),},
 { S(-12, -1), S(3, -6), S(3, -5), S(-12, 9), S(-28, 22), S(-34, 89), S(96, 137), S(0, 0),},
 { S(24, -6), S(25, -10), S(23, -6), S(8, 3), S(-14, 17), S(-44, 77), S(62, 112), S(0, 0),},
 { S(-2, -5), S(4, -15), S(15, -13), S(3, -14), S(-19, 3), S(-25, 51), S(-47, 133), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-6, -52), S(37, -56), S(20, -38), S(17, -32), S(5, -26), S(10, -88), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(41, -18);

const Score COMPLEXITY_PAWNS = 6;

const Score COMPLEXITY_PAWNS_OFFSET = 5;

const Score COMPLEXITY_OFFSET = -73;

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

const Score TEMPO = 37;
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

  data->openFiles = ~(Fill(whitePawns | blackPawns, N) | Fill(whitePawns | blackPawns, S));

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
inline Score MaterialValue(Board* board, const int side) {
  Score s = S(0, 0);

  const int xside = side ^ 1;

  uint8_t eks = SQ_SIDE[lsb(board->pieces[KING[xside]])];

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

INLINE Score PieceEval(Board* board, EvalData* data, const int piece) {
  Score s = S(0, 0);

  const int side = piece & 1;
  const int xside = side ^ 1;
  const int pieceType = PIECE_TYPE[piece];
  const BitBoard enemyKingArea = data->kingArea[xside];
  const BitBoard mob = data->mobilitySquares[side];
  const BitBoard outposts = data->outposts[side];
  const BitBoard myPawns = board->pieces[PAWN[side]];
  const BitBoard opponentPawns = board->pieces[PAWN[xside]];
  const BitBoard allPawns = myPawns | opponentPawns;

  if (pieceType == BISHOP_TYPE) {
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
  }

  BitBoard pieces = board->pieces[piece];
  while (pieces) {
    BitBoard bb = pieces & -pieces;
    int sq = lsb(pieces);

    BitBoard movement = EMPTY;
    if (pieceType == KNIGHT_TYPE) {
      movement = GetKnightAttacks(sq);
      s += KNIGHT_MOBILITIES[bits(movement & mob)];

      if (T)
        C.knightMobilities[bits(movement & mob)] += cs[side];
    } else if (pieceType == BISHOP_TYPE) {
      movement =
          GetBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]] ^ board->pieces[QUEEN[xside]]);
      s += BISHOP_MOBILITIES[bits(movement & mob)];

      if (T)
        C.bishopMobilities[bits(movement & mob)] += cs[side];
    } else if (pieceType == ROOK_TYPE) {
      movement = GetRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[ROOK[side]] ^ board->pieces[QUEEN[side]] ^
                                        board->pieces[QUEEN[xside]]);
      s += ROOK_MOBILITIES[bits(movement & mob)];

      if (T)
        C.rookMobilities[bits(movement & mob)] += cs[side];
    } else if (pieceType == QUEEN_TYPE) {
      movement = GetQueenAttacks(sq, board->occupancies[BOTH]);
      s += QUEEN_MOBILITIES[bits(movement & mob)];

      if (T)
        C.queenMobilities[bits(movement & mob)] += cs[side];
    } else if (pieceType == KING_TYPE) {
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
      int numOpenFiles = 8 - bits(Fill(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK], N) & 0xFFULL);
      s += ROOK_OPEN_FILE_OFFSET * numOpenFiles;

      if (T)
        C.rookOpenFileOffset += cs[side] * numOpenFiles;

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

      if (movement & data->openFiles) {
        s += ROOK_TO_OPEN;

        if (T)
          C.rookToOpen += cs[side];
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
    } else if (pieceType == QUEEN_TYPE) {
      if (FILE_MASKS[file(sq)] & board->pieces[ROOK[xside]]) {
        s += QUEEN_OPPOSITE_ROOK;

        if (T)
          C.queenOppositeRook += cs[side];
      }

      if ((FILE_MASKS[file(sq)] & FORWARD_RANK_MASKS[side][rank(sq)] & board->pieces[ROOK[side]]) &&
          !(FILE_MASKS[file(sq)] & myPawns)) {
        s += QUEEN_ROOK_BATTERY;

        if (T)
          C.queenRookBattery += cs[side];
      }
    }

    popLsb(pieces);
  }

  return s;
}

// Threats bonus (piece attacks piece)
// TODO: Make this vary more based on piece type
INLINE Score Threats(Board* board, EvalData* data, const int side) {
  Score s = S(0, 0);

  const int xside = side ^ 1;

  const BitBoard covered = data->attacks[xside][PAWN_TYPE] | (data->twoAttacks[xside] & ~data->twoAttacks[side]);
  const BitBoard nonPawnEnemies = board->occupancies[xside] & ~board->pieces[PAWN[xside]];
  const BitBoard weak = ~covered & data->allAttacks[side];

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

  s += bits(pawnPushAttacks) * PAWN_PUSH_THREAT + bits(pawnPushAttacks & board->pinned) * PAWN_PUSH_THREAT_PINNED;

  if (T) {
    C.pawnThreat += cs[side] * bits(pawnThreats);
    C.hangingThreat += cs[side] * bits(hangingPieces);
    C.pawnPushThreat += cs[side] * bits(pawnPushAttacks);
    C.pawnPushThreatPinned += cs[side] * bits(pawnPushAttacks & board->pinned);
  }

  if (board->pieces[QUEEN[xside]]) {
    int oppQueenSquare = lsb(board->pieces[QUEEN[xside]]);

    BitBoard knightQueenHits =
        GetKnightAttacks(oppQueenSquare) & data->attacks[side][KNIGHT_TYPE] & ~board->pieces[PAWN[side]] & ~covered;
    s += bits(knightQueenHits) * KNIGHT_CHECK_QUEEN;

    BitBoard bishopQueenHits = GetBishopAttacks(oppQueenSquare, board->occupancies[BOTH]) &
                               data->attacks[side][BISHOP_TYPE] & ~board->pieces[PAWN[side]] & ~covered &
                               data->twoAttacks[side];
    s += bits(bishopQueenHits) * BISHOP_CHECK_QUEEN;

    BitBoard rookQueenHits = GetRookAttacks(oppQueenSquare, board->occupancies[BOTH]) & data->attacks[side][ROOK_TYPE] &
                             ~board->pieces[PAWN[side]] & ~covered & data->twoAttacks[side];
    s += bits(rookQueenHits) * ROOK_CHECK_QUEEN;

    if (T) {
      C.knightCheckQueen += cs[side] * bits(knightQueenHits);
      C.bishopCheckQueen += cs[side] * bits(bishopQueenHits);
      C.rookCheckQueen += cs[side] * bits(rookQueenHits);
    }
  }

  return s;
}

// King safety is a quadratic based - there is a summation of smaller values
// into a single score, then that value is squared and diveded by 1000
// This fit for KS is strong as it can ignore a single piece attacking, but will
// spike on a secondary piece joining.
// It is heavily influenced by Toga, Rebel, SF, and Ethereal
INLINE Score KingSafety(Board* board, EvalData* data, const int side) {
  Score s = S(0, 0);
  Score shelter = S(0, 0);

  const int xside = side ^ 1;

  const BitBoard ourPawns = board->pieces[PAWN[side]] & ~data->attacks[xside][PAWN_TYPE] &
                            ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];
  const BitBoard opponentPawns = board->pieces[PAWN[xside]] & ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];

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
INLINE Score Space(Board* board, EvalData* data, const int side) {
  static const BitBoard CENTER_FILES = (C_FILE | D_FILE | E_FILE | F_FILE) & ~(RANK_1 | RANK_8);

  Score s = S(0, 0);
  const int xside = side ^ 1;

  BitBoard space = side == WHITE ? ShiftS(board->pieces[PAWN[side]]) : ShiftN(board->pieces[PAWN[side]]);
  space |= side == WHITE ? (ShiftS(space) | ShiftSS(space)) : (ShiftN(space) | ShiftNN(space));
  space &= ~(board->pieces[PAWN[side]] | data->attacks[xside][PAWN_TYPE] |
             (data->twoAttacks[xside] & ~data->twoAttacks[side]));
  space &= CENTER_FILES;

  int pieces = bits(board->occupancies[side]);
  int openFiles = 8 - bits(Fill(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK], N) & 0xFULL);
  int scalar = bits(space) * max(0, pieces - openFiles) * max(0, pieces - openFiles);

  s += S((SPACE * scalar) / 1024, 0);

  if (T)
    C.space += cs[side] * scalar;

  return s;
}

INLINE Score Imbalance(Board* board, const int side) {
  Score s = S(0, 0);
  const int xside = side ^ 1;

  for (int i = KNIGHT_TYPE; i < KING_TYPE; i++) {
    for (int j = PAWN_TYPE; j < i; j++) {
      s += IMBALANCE[i][j] * bits(board->pieces[2 * i + side]) * bits(board->pieces[2 * j + xside]);

      if (T)
        C.imbalance[i][j] += bits(board->pieces[2 * i + side]) * bits(board->pieces[2 * j + xside]) * cs[side];
    }
  }

  return s;
}

INLINE Score Complexity(Board* board, EvalData* data, Score eg) {
  int sign = (eg > 0) - (eg < 0);
  if (!sign)
    return S(0, 0);

  int pawns = bits(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK]);
  int pawnsOffset = bits(0xFFULL & (FileFill(board->pieces[PAWN_WHITE]) ^ FileFill(board->pieces[PAWN_BLACK])));

  Score complexity = pawns * COMPLEXITY_PAWNS                //
                     + pawnsOffset * COMPLEXITY_PAWNS_OFFSET //
                     + COMPLEXITY_OFFSET;

  if (T) {
    C.complexPawns = pawns;
    C.complexPawnsOffset = pawnsOffset;
    C.complexOffset = 1;
  }

  int bound = sign * max(complexity, -abs(eg));

  return S(0, bound);
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
    s += PieceEval(board, &data, KNIGHT_WHITE) - PieceEval(board, &data, KNIGHT_BLACK);
    s += PieceEval(board, &data, BISHOP_WHITE) - PieceEval(board, &data, BISHOP_BLACK);
    s += PieceEval(board, &data, ROOK_WHITE) - PieceEval(board, &data, ROOK_BLACK);
    s += PieceEval(board, &data, QUEEN_WHITE) - PieceEval(board, &data, QUEEN_BLACK);
    s += PieceEval(board, &data, KING_WHITE) - PieceEval(board, &data, KING_BLACK);

    s += PasserEval(board, &data, WHITE) - PasserEval(board, &data, BLACK);
    s += Threats(board, &data, WHITE) - Threats(board, &data, BLACK);
    s += KingSafety(board, &data, WHITE) - KingSafety(board, &data, BLACK);
    s += Space(board, &data, WHITE) - Space(board, &data, BLACK);
  }

  s += thread->data.contempt;
  s += Complexity(board, &data, scoreEG(s));

  // taper
  int phase = GetPhase(board);
  Score res = (phase * scoreMG(s) + (128 - phase) * scoreEG(s)) / 128;

  if (T)
    C.ss = res >= 0 ? WHITE : BLACK;

  // scale the score
  res = (res * Scale(board, res >= 0 ? WHITE : BLACK)) / MAX_SCALE;
  return TEMPO + (board->side == WHITE ? res : -res);
}

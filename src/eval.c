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
const Score MATERIAL_VALUES[7] = { S(200, 200), S(650, 650), S(650, 650), S(1100, 1100), S(2000, 2000), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(40, 191);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 195, 396), S( 208, 487), S( 149, 464), S( 221, 411),
 S( 128, 321), S(  96, 339), S( 118, 234), S( 131, 191),
 S(  95, 276), S( 116, 243), S(  78, 211), S(  72, 174),
 S(  73, 212), S(  59, 202), S(  66, 179), S(  71, 152),
 S(  72, 205), S(  53, 182), S(  65, 176), S(  65, 186),
 S(  61, 197), S(  88, 191), S( 114, 199), S(  93, 181),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 280, 310), S( 108, 347), S( 414, 296), S( 202, 430),
 S( 160, 211), S( 176, 185), S( 199, 141), S( 146, 166),
 S(  82, 200), S( 128, 192), S(  90, 169), S( 115, 157),
 S(  42, 175), S(  62, 174), S(  62, 159), S(  84, 139),
 S(  22, 183), S(  54, 157), S(  60, 169), S(  82, 178),
 S(   9, 188), S( 102, 173), S( 109, 194), S(  83, 197),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-328, 181), S(-364, 248), S(-300, 278), S(-228, 235),
 S(-184, 251), S(-145, 272), S( -96, 247), S( -83, 249),
 S( -58, 204), S( -74, 238), S( -85, 263), S(-107, 273),
 S( -83, 271), S( -75, 262), S( -50, 263), S( -71, 274),
 S( -84, 259), S( -76, 255), S( -48, 277), S( -61, 283),
 S(-128, 217), S(-108, 218), S( -87, 231), S( -92, 266),
 S(-149, 225), S(-116, 206), S(-110, 228), S( -96, 224),
 S(-192, 224), S(-118, 220), S(-118, 223), S( -96, 241),
},{
 S(-424,-145), S(-484, 170), S(-225, 152), S( -73, 209),
 S(-123, 120), S(  18, 177), S( -57, 233), S(-101, 247),
 S( -25, 156), S(-108, 209), S(  -3, 152), S( -43, 229),
 S( -60, 231), S( -58, 228), S( -42, 246), S( -61, 271),
 S( -73, 260), S( -90, 246), S( -36, 270), S( -70, 301),
 S( -85, 206), S( -55, 202), S( -75, 221), S( -65, 258),
 S( -88, 220), S( -65, 210), S( -93, 212), S( -95, 219),
 S(-105, 231), S(-132, 228), S( -96, 211), S( -81, 241),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -72, 327), S(-109, 357), S( -80, 322), S(-136, 367),
 S( -31, 315), S( -52, 284), S(  26, 307), S( -10, 338),
 S(   9, 326), S(  18, 325), S( -46, 289), S(  21, 313),
 S(   2, 325), S(  50, 313), S(  27, 326), S(  11, 359),
 S(  42, 299), S(  18, 323), S(  41, 321), S(  61, 314),
 S(  33, 277), S(  74, 305), S(  41, 270), S(  50, 303),
 S(  45, 269), S(  40, 208), S(  71, 253), S(  34, 290),
 S(  27, 220), S(  69, 263), S(  43, 299), S(  50, 296),
},{
 S(-142, 234), S(  66, 288), S(-102, 302), S(-156, 310),
 S( -14, 220), S( -83, 289), S( -36, 322), S(  23, 289),
 S(  54, 301), S(  -8, 316), S(  -4, 294), S(  19, 313),
 S(  -6, 301), S(  54, 306), S(  59, 317), S(  49, 317),
 S(  75, 251), S(  56, 297), S(  64, 316), S(  53, 315),
 S(  61, 255), S(  93, 281), S(  58, 269), S(  48, 325),
 S(  67, 212), S(  65, 211), S(  63, 278), S(  53, 290),
 S(  64, 171), S(  50, 315), S(  30, 307), S(  65, 289),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-295, 541), S(-286, 559), S(-277, 566), S(-343, 574),
 S(-268, 525), S(-282, 553), S(-250, 552), S(-214, 513),
 S(-307, 549), S(-237, 538), S(-263, 542), S(-254, 527),
 S(-284, 565), S(-264, 564), S(-236, 556), S(-230, 524),
 S(-287, 548), S(-282, 557), S(-264, 541), S(-255, 531),
 S(-295, 529), S(-275, 506), S(-264, 509), S(-264, 508),
 S(-284, 488), S(-277, 506), S(-251, 497), S(-244, 484),
 S(-290, 509), S(-273, 496), S(-270, 507), S(-254, 482),
},{
 S(-160, 508), S(-322, 594), S(-390, 592), S(-280, 551),
 S(-218, 485), S(-206, 516), S(-232, 515), S(-268, 522),
 S(-268, 503), S(-161, 499), S(-229, 482), S(-187, 478),
 S(-259, 519), S(-198, 514), S(-216, 496), S(-210, 504),
 S(-286, 498), S(-253, 509), S(-256, 500), S(-232, 508),
 S(-268, 447), S(-226, 441), S(-242, 458), S(-238, 481),
 S(-253, 441), S(-206, 422), S(-238, 442), S(-232, 462),
 S(-279, 465), S(-236, 458), S(-257, 461), S(-235, 460),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-159, 803), S(  11, 603), S(  75, 626), S(  57, 682),
 S(  -7, 657), S(  45, 584), S(  63, 658), S(  60, 658),
 S(  52, 610), S(  74, 585), S(  65, 640), S( 100, 643),
 S(  49, 651), S(  58, 691), S(  75, 679), S(  47, 698),
 S(  61, 609), S(  34, 710), S(  41, 685), S(  29, 734),
 S(  45, 577), S(  50, 608), S(  44, 648), S(  34, 652),
 S(  48, 511), S(  46, 538), S(  58, 546), S(  56, 578),
 S(  20, 556), S(  20, 559), S(  26, 564), S(  31, 573),
},{
 S( 142, 597), S( 260, 564), S(-119, 794), S( 109, 578),
 S( 119, 574), S(  88, 574), S(  87, 618), S(  47, 690),
 S( 122, 560), S( 101, 521), S( 125, 590), S(  97, 639),
 S(  51, 647), S(  74, 651), S( 112, 586), S(  68, 699),
 S(  69, 583), S(  63, 638), S(  78, 638), S(  40, 727),
 S(  61, 512), S(  76, 562), S(  77, 613), S(  45, 654),
 S(  89, 446), S(  90, 441), S(  80, 486), S(  67, 570),
 S(  71, 502), S(   6, 556), S(  22, 516), S(  40, 539),
}};

const Score KING_PSQT[2][32] = {{
 S( 519,-403), S( 189,-157), S(  33,-125), S( 283,-162),
 S(-248, 102), S(-296, 236), S(-148, 154), S( -32,  91),
 S( -19,  66), S(-143, 205), S( -35, 158), S(-123, 181),
 S(   4,  53), S( -30, 141), S( -73, 128), S( -84, 124),
 S( -73,  20), S(  -3,  79), S(  43,  49), S( -64,  68),
 S( -37, -47), S(  -4,  19), S(   1,   2), S( -37,  13),
 S(  24, -83), S( -35,  25), S( -38, -15), S( -45, -14),
 S(  40,-210), S(  49,-113), S(  32,-118), S( -70,-101),
},{
 S(-125,-290), S(-452,  53), S( -31,-115), S( -21,-124),
 S(-412,  23), S(-184, 211), S(-280, 188), S( 342,  17),
 S(-181,  42), S(  73, 207), S( 319, 142), S( 101, 180),
 S(-142,  19), S(  31, 135), S(  48, 150), S(  14, 143),
 S(-144,  -3), S( -97,  90), S(  23,  72), S( -18,  92),
 S( -13, -34), S(  30,  41), S(   8,  31), S(  15,  38),
 S(  24, -33), S(  12,  54), S( -17,  24), S( -42,  19),
 S(  33,-136), S(  30, -36), S(   7, -61), S( -53, -67),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S(-102,  23), S(   9,  44), S(  58,  58), S( 124,  90),
 S(  34,  -6), S(  79,  46), S(  59,  58), S(  79,  86),
 S(  42,   1), S(  73,  27), S(  46,  36), S(  39,  46),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -6,  17), S(  50,   3), S( 117,  27), S( 125,  14),
 S(   9,  -6), S(  50,  25), S(  88,   5), S( 105,  19),
 S(  -8,  37), S(  91,  16), S(  64,  24), S(  73,  47),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-255, -61), S(-170, 112), S(-134, 195), S(-108, 225),
 S( -90, 252), S( -74, 279), S( -58, 287), S( -41, 292),
 S( -27, 276),};

const Score BISHOP_MOBILITIES[14] = {
 S( -44, 131), S(   5, 196), S(  33, 243), S(  47, 288),
 S(  60, 307), S(  67, 329), S(  71, 346), S(  79, 354),
 S(  79, 364), S(  86, 367), S( 101, 359), S( 120, 355),
 S( 118, 368), S( 150, 334),};

const Score ROOK_MOBILITIES[15] = {
 S(-258, -95), S(-283, 369), S(-253, 445), S(-241, 448),
 S(-245, 489), S(-237, 503), S(-247, 520), S(-238, 521),
 S(-231, 533), S(-222, 543), S(-219, 555), S(-229, 572),
 S(-223, 578), S(-212, 590), S(-201, 581),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2243,-2393), S(-151,-399), S( -39, 573), S(   1, 868),
 S(  26, 948), S(  38, 975), S(  38,1040), S(  43,1081),
 S(  49,1114), S(  55,1123), S(  61,1135), S(  69,1136),
 S(  69,1151), S(  77,1151), S(  77,1153), S(  92,1137),
 S(  90,1140), S(  86,1148), S(  98,1111), S( 135,1052),
 S( 166, 985), S( 187, 914), S( 247, 856), S( 303, 696),
 S( 274, 680), S( 295, 574), S(  23, 560), S( 769, -54),
};

const Score KING_MOBILITIES[9] = {
 S(   9, -29), S(   5,  41), S(   1,  48),
 S(  -4,  47), S(  -3,  29), S(  -4,   5),
 S(  -9,   0), S(  -6, -23), S(  -1, -91),
};

const Score MINOR_BEHIND_PAWN = S(10, 30);

const Score KNIGHT_OUTPOST_REACHABLE = S(20, 43);

const Score BISHOP_OUTPOST_REACHABLE = S(13, 14);

const Score BISHOP_TRAPPED = S(-231, -540);

const Score ROOK_TRAPPED = S(-76, -58);

const Score BAD_BISHOP_PAWNS = S(-2, -9);

const Score DRAGON_BISHOP = S(46, 39);

const Score ROOK_OPEN_FILE_OFFSET = S(27, 13);

const Score ROOK_OPEN_FILE = S(58, 34);

const Score ROOK_SEMI_OPEN = S(34, 16);

const Score ROOK_TO_OPEN = S(32, 40);

const Score QUEEN_OPPOSITE_ROOK = S(-30, 6);

const Score QUEEN_ROOK_BATTERY = S(7, 110);

const Score DEFENDED_PAWN = S(23, 16);

const Score DOUBLED_PAWN = S(29, -82);

const Score ISOLATED_PAWN[4] = {
 S(  -1, -34), S(  -3, -31), S( -12, -17), S(   2, -21),
};

const Score OPEN_ISOLATED_PAWN = S(-9, -24);

const Score BACKWARDS_PAWN = S(-16, -35);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(118, 47), S(-4, 57), S(0, 20), S(2, 1), S(5, 0), S(6, -10), S(0, 0),},
 { S(0, 0), S(164, 66), S(42, 76), S(11, 26), S(19, 5), S(13, -4), S(1, 4), S(0, 0),},
 { S(0, 0), S(149, 141), S(49, 82), S(28, 29), S(13, 10), S(9, 4), S(-1, -11), S(0, 0),},
 { S(0, 0), S(101, 140), S(52, 86), S(19, 34), S(11, 15), S(8, 5), S(10, 3), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 392, 329), S(  17, 157),
 S( -32, 139), S( -46, 100), S( -59,  54), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(11, -33);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 248, 450), S( 123, 407), S(  20, 247),
 S( -25, 166), S( -23,  88), S( -16,  75), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(4, -22);

const Score PASSED_PAWN_KING_PROXIMITY = S(-16, 51);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 176, 558), S(  21, 293), S(  15, 102), S(  21,  23),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(58, -252);

const Score PASSED_PAWN_SQ_RULE = S(0, 809);

const Score PASSED_PAWN_UNSUPPORTED = S(-47, -11);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(-24, 179);

const Score KNIGHT_THREATS[6] = { S(-1, 45), S(-6, 80), S(73, 82), S(178, 25), S(145, -104), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(4, 44), S(47, 81), S(0, 0), S(147, 45), S(128, 66), S(222, 2952),};

const Score ROOK_THREATS[6] = { S(-1, 49), S(65, 94), S(70, 116), S(16, 44), S(104, -81), S(426, 2157),};

const Score KING_THREAT = S(23, 74);

const Score PAWN_THREAT = S(174, 87);

const Score PAWN_PUSH_THREAT = S(37, 42);

const Score PAWN_PUSH_THREAT_PINNED = S(109, 272);

const Score HANGING_THREAT = S(23, 45);

const Score KNIGHT_CHECK_QUEEN = S(27, 0);

const Score BISHOP_CHECK_QUEEN = S(47, 45);

const Score ROOK_CHECK_QUEEN = S(41, 12);

const Score SPACE = 227;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(18, 59), S(0, 0),},
 { S(11, 48), S(-6, -45), S(0, 0),},
 { S(10, 81), S(-52, -3), S(-15, 8), S(0, 0),},
 { S(99, 104), S(-113, 102), S(-59, 190), S(-485, 361), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-40, -19), S(67, 189), S(-4, 98), S(-30, 4), S(21, -25), S(87, -76), S(73, -123), S(0, 0),},
 { S(-94, 13), S(-31, 168), S(-71, 118), S(-69, 56), S(-56, 24), S(53, -40), S(71, -67), S(0, 0),},
 { S(-63, 6), S(-14, 201), S(-107, 104), S(-49, 45), S(-43, 22), S(-21, 9), S(43, -21), S(0, 0),},
 { S(-80, 32), S(63, 126), S(-107, 55), S(-67, 57), S(-58, 43), S(-32, 19), S(-53, 39), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-38, -21), S(-40, -4), S(-40, -14), S(-54, 12), S(-110, 70), S(141, 170), S(591, 179), S(0, 0),},
 { S(-17, 2), S(17, -11), S(17, -5), S(-19, 22), S(-49, 50), S(-36, 173), S(209, 288), S(0, 0),},
 { S(54, -6), S(57, -16), S(47, -6), S(24, 7), S(-16, 35), S(-92, 160), S(175, 187), S(0, 0),},
 { S(-3, -4), S(9, -27), S(32, -19), S(13, -24), S(-27, 7), S(-45, 107), S(-123, 276), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-25, -87), S(81, -121), S(47, -75), S(43, -68), S(21, -56), S(14, -176), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(86, -40);

const Score COMPLEXITY_PAWNS = 4;

const Score COMPLEXITY_PAWNS_BOTH_SIDES = 148;

const Score COMPLEXITY_OFFSET = -195;

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

const Score TEMPO = 78;
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
      int numMoves = bits(movement & ~data->allAttacks[xside] & ~board->occupancies[side]);

      s += KING_MOBILITIES[numMoves];

      if (T)
        C.kingMobilities[numMoves] += cs[side];
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
    s += 2 * S(-danger * danger / 1024, -danger / 32);

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

INLINE Score Complexity(Board* board, Score eg) {
  int sign = (eg > 0) - (eg < 0);
  if (!sign)
    return S(0, 0);

  BitBoard allPawns = board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK];
  int pawns = bits(allPawns);
  int pawnsBothSides =
      (allPawns & (A_FILE | B_FILE | C_FILE | D_FILE)) && (allPawns & (E_FILE | F_FILE | G_FILE | H_FILE));

  Score complexity = pawns * COMPLEXITY_PAWNS                       //
                     + pawnsBothSides * COMPLEXITY_PAWNS_BOTH_SIDES //
                     + COMPLEXITY_OFFSET;

  if (T) {
    C.complexPawns = pawns;
    C.complexPawnsBothSides = pawnsBothSides;
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

      float netEval = NetworkEval(board->pieces[PAWN_WHITE], board->pieces[PAWN_BLACK], PAWN_NET);
      Score rounded = (Score)netEval;
      pawnS += S(rounded, rounded);

      TTPawnPut(board->pawnHash, pawnS, data.passedPawns, thread);
      s += pawnS;
    }
  } else {
    s += PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);

    float netEval = NetworkEval(board->pieces[PAWN_WHITE], board->pieces[PAWN_BLACK], PAWN_NET);
    Score rounded = (Score)netEval;
    s += S(rounded, rounded);
  }

  s += Imbalance(board, WHITE) - Imbalance(board, BLACK);

  if (T || abs(scoreMG(s) + scoreEG(s)) / 2 < 2048) {
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

  s += thread->data.contempt * 2;
  s += Complexity(board, scoreEG(s));

  // taper
  int phase = GetPhase(board);
  Score res = (phase * scoreMG(s) + (128 - phase) * scoreEG(s)) / 128;

  if (T)
    C.ss = res >= 0 ? WHITE : BLACK;

  // scale the score
  res = (res * Scale(board, res >= 0 ? WHITE : BLACK)) / MAX_SCALE;
  return TEMPO + (board->side == WHITE ? res : -res);
}

Score EvaluateScaled(Board* board, ThreadData* thread) { return Evaluate(board, thread) / 2; }
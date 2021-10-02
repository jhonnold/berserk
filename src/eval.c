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

// clang-format off
const Score MATERIAL_VALUES[7] = { S(200, 200), S(650, 650), S(650, 650), S(1100, 1100), S(2000, 2000), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(41, 188);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 180, 412), S( 222, 511), S( 171, 496), S( 237, 430),
 S( 138, 339), S(  98, 356), S( 137, 268), S( 154, 211),
 S( 100, 279), S( 115, 249), S(  88, 225), S(  83, 191),
 S(  73, 208), S(  62, 204), S(  70, 181), S(  76, 157),
 S(  67, 195), S(  53, 178), S(  64, 169), S(  66, 180),
 S(  64, 194), S(  89, 186), S(  98, 178), S(  93, 179),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 281, 279), S( 107, 326), S( 433, 317), S( 223, 436),
 S( 169, 221), S( 174, 180), S( 218, 156), S( 164, 174),
 S(  79, 200), S( 128, 188), S(  98, 177), S( 123, 158),
 S(  44, 174), S(  66, 176), S(  70, 168), S(  91, 144),
 S(  20, 182), S(  59, 157), S(  69, 172), S(  84, 182),
 S(  16, 191), S( 103, 176), S( 100, 184), S(  84, 197),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-322, 179), S(-367, 259), S(-303, 278), S(-214, 228),
 S(-183, 260), S(-147, 277), S( -94, 251), S( -81, 251),
 S( -61, 218), S( -79, 244), S( -85, 269), S(-107, 278),
 S( -83, 277), S( -74, 266), S( -50, 269), S( -70, 278),
 S( -83, 265), S( -73, 262), S( -47, 282), S( -59, 287),
 S(-126, 223), S(-107, 223), S( -87, 237), S( -92, 271),
 S(-149, 233), S(-115, 215), S(-106, 232), S( -94, 229),
 S(-192, 237), S(-115, 227), S(-114, 226), S( -95, 246),
},{
 S(-417,-145), S(-479, 170), S(-237, 156), S( -79, 214),
 S(-127, 114), S(  21, 172), S( -61, 241), S( -99, 247),
 S( -28, 151), S(-108, 208), S(  -9, 155), S( -41, 233),
 S( -59, 230), S( -57, 228), S( -40, 247), S( -60, 273),
 S( -71, 261), S( -92, 250), S( -36, 271), S( -70, 306),
 S( -85, 212), S( -55, 208), S( -74, 225), S( -63, 262),
 S( -88, 222), S( -64, 213), S( -92, 217), S( -93, 224),
 S(-106, 236), S(-130, 232), S( -96, 216), S( -80, 245),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -61, 323), S(-112, 364), S( -93, 334), S(-138, 373),
 S( -28, 324), S( -52, 294), S(  26, 312), S( -10, 342),
 S(   9, 336), S(  16, 330), S( -42, 297), S(  22, 318),
 S(   1, 333), S(  52, 318), S(  28, 329), S(  10, 363),
 S(  44, 303), S(  19, 329), S(  44, 326), S(  63, 319),
 S(  36, 278), S(  74, 311), S(  44, 277), S(  50, 308),
 S(  47, 272), S(  43, 213), S(  71, 261), S(  35, 296),
 S(  31, 224), S(  71, 265), S(  45, 304), S(  52, 300),
},{
 S(-145, 240), S(  62, 293), S(-103, 305), S(-160, 316),
 S( -16, 216), S( -82, 289), S( -34, 324), S(  25, 295),
 S(  54, 300), S(  -7, 313), S(  -4, 299), S(  16, 320),
 S(  -7, 305), S(  55, 307), S(  59, 318), S(  50, 320),
 S(  76, 255), S(  57, 301), S(  65, 318), S(  53, 320),
 S(  62, 259), S(  93, 286), S(  60, 275), S(  49, 330),
 S(  65, 217), S(  66, 218), S(  64, 285), S(  53, 297),
 S(  64, 177), S(  53, 315), S(  30, 311), S(  66, 296),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-296, 552), S(-278, 563), S(-274, 571), S(-337, 577),
 S(-267, 536), S(-283, 559), S(-247, 557), S(-212, 519),
 S(-304, 559), S(-239, 547), S(-259, 547), S(-250, 531),
 S(-286, 578), S(-265, 573), S(-234, 563), S(-227, 532),
 S(-287, 557), S(-281, 567), S(-266, 553), S(-253, 540),
 S(-295, 538), S(-274, 514), S(-263, 518), S(-264, 517),
 S(-284, 498), S(-276, 512), S(-249, 505), S(-242, 491),
 S(-288, 516), S(-273, 503), S(-269, 515), S(-254, 489),
},{
 S(-181, 524), S(-345, 605), S(-411, 604), S(-280, 556),
 S(-233, 491), S(-215, 516), S(-239, 525), S(-267, 528),
 S(-272, 499), S(-163, 494), S(-230, 484), S(-187, 481),
 S(-264, 519), S(-202, 516), S(-219, 503), S(-210, 511),
 S(-287, 500), S(-256, 509), S(-257, 504), S(-235, 518),
 S(-269, 453), S(-228, 446), S(-240, 464), S(-238, 489),
 S(-256, 446), S(-206, 426), S(-237, 448), S(-231, 470),
 S(-279, 470), S(-238, 463), S(-256, 469), S(-234, 467),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-160, 818), S(  12, 614), S(  76, 635), S(  72, 677),
 S(  -6, 666), S(  47, 590), S(  64, 665), S(  60, 663),
 S(  56, 615), S(  70, 600), S(  66, 646), S( 100, 655),
 S(  48, 662), S(  60, 696), S(  75, 689), S(  52, 703),
 S(  63, 615), S(  36, 717), S(  43, 689), S(  31, 740),
 S(  49, 576), S(  53, 610), S(  47, 652), S(  36, 656),
 S(  52, 512), S(  49, 541), S(  61, 551), S(  57, 585),
 S(  23, 559), S(  22, 562), S(  28, 568), S(  33, 578),
},{
 S( 143, 583), S( 251, 563), S(-110, 790), S( 103, 596),
 S( 112, 565), S(  82, 563), S(  88, 623), S(  53, 690),
 S( 124, 554), S( 100, 523), S( 129, 585), S(  98, 644),
 S(  54, 641), S(  74, 654), S( 114, 589), S(  71, 700),
 S(  72, 583), S(  65, 637), S(  79, 642), S(  42, 730),
 S(  62, 513), S(  77, 567), S(  78, 617), S(  47, 659),
 S(  93, 434), S(  93, 443), S(  81, 498), S(  67, 578),
 S(  73, 502), S(  13, 553), S(  24, 519), S(  43, 546),
}};

const Score KING_PSQT[2][32] = {{
 S( 486,-424), S( 235,-131), S(  30,-115), S( 230,-197),
 S(-227, 116), S(-251, 225), S(-103, 149), S( -26,  61),
 S( -14,  52), S(-142, 187), S(  -2, 137), S( -91, 145),
 S(   8,  53), S( -51, 121), S( -81, 105), S( -52,  84),
 S( -51,  31), S( -10,  92), S(  38,  38), S( -78,  63),
 S(  -6, -39), S(   5,  28), S(  20,  25), S( -10,  34),
 S(  55, -78), S( -19,  26), S( -21,   1), S( -26,  -5),
 S(  55,-196), S(  56,-111), S(  10,-131), S( -91,-104),
},{
 S(-132,-320), S(-485,  74), S(  -9,-142), S( -62,-162),
 S(-436,  38), S(-220, 230), S(-307, 161), S( 303,   6),
 S(-211,  56), S(  29, 192), S( 264, 108), S(  64, 133),
 S(-180,  37), S( -15, 115), S(   7, 107), S( -20,  95),
 S(-167,  27), S(-119,  89), S(  18,  41), S( -11,  64),
 S(   0, -23), S(  27,  45), S(  19,  41), S(  27,  46),
 S(  47, -23), S(  11,  53), S( -10,  30), S( -44,  23),
 S(  42,-121), S(  33, -34), S( -20, -72), S( -75, -75),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S(-100,  30), S(  12,  43), S(  57,  55), S( 123,  88),
 S(  30,  -3), S(  77,  51), S(  60,  59), S(  82,  86),
 S(  42,   1), S(  74,  23), S(  45,  37), S(  40,  48),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -4,  14), S(  50,   3), S( 117,  26), S( 126,  13),
 S(   8,  -9), S(  48,  26), S(  87,   8), S( 104,  18),
 S(  -9,  42), S(  90,  14), S(  62,  25), S(  71,  41),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-254, -42), S(-168, 125), S(-130, 206), S(-105, 235),
 S( -87, 262), S( -71, 289), S( -55, 296), S( -39, 302),
 S( -24, 286),};

const Score BISHOP_MOBILITIES[14] = {
 S( -45, 139), S(   8, 205), S(  35, 253), S(  49, 297),
 S(  62, 316), S(  70, 338), S(  73, 354), S(  81, 362),
 S(  81, 371), S(  88, 375), S( 102, 369), S( 123, 364),
 S( 121, 374), S( 149, 343),};

const Score ROOK_MOBILITIES[15] = {
 S(-260, -82), S(-283, 380), S(-252, 454), S(-241, 455),
 S(-246, 498), S(-236, 512), S(-245, 530), S(-238, 530),
 S(-230, 542), S(-221, 552), S(-218, 563), S(-227, 581),
 S(-223, 588), S(-214, 599), S(-196, 585),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2269,-2396), S(-147,-369), S( -39, 608), S(   3, 887),
 S(  27, 966), S(  42, 983), S(  41,1051), S(  46,1093),
 S(  52,1125), S(  59,1135), S(  65,1145), S(  73,1144),
 S(  73,1157), S(  82,1157), S(  82,1161), S(  97,1145),
 S(  95,1146), S(  92,1153), S( 102,1120), S( 141,1058),
 S( 170, 993), S( 192, 923), S( 252, 864), S( 307, 701),
 S( 274, 696), S( 263, 597), S(   0, 596), S( 799, -82),
};

const Score KING_MOBILITIES[9] = {
 S(   9, -34), S(   6,  36), S(   1,  46),
 S(  -5,  46), S(  -5,  30), S(  -9,   8),
 S( -13,   3), S(  -6, -20), S(   1, -81),
};

const Score MINOR_BEHIND_PAWN = S(11, 30);

const Score KNIGHT_OUTPOST_REACHABLE = S(20, 43);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 14);

const Score BISHOP_TRAPPED = S(-228, -538);

const Score ROOK_TRAPPED = S(-77, -56);

const Score BAD_BISHOP_PAWNS = S(-2, -9);

const Score DRAGON_BISHOP = S(43, 39);

const Score ROOK_OPEN_FILE_OFFSET = S(28, 15);

const Score ROOK_OPEN_FILE = S(57, 34);

const Score ROOK_SEMI_OPEN = S(34, 15);

const Score ROOK_TO_OPEN = S(29, 42);

const Score QUEEN_OPPOSITE_ROOK = S(-29, 6);

const Score QUEEN_ROOK_BATTERY = S(8, 107);

const Score DEFENDED_PAWN = S(23, 14);

const Score DOUBLED_PAWN = S(28, -83);

const Score ISOLATED_PAWN[4] = {
 S(  -3, -32), S(  -3, -31), S( -13, -18), S(   0, -20),
};

const Score OPEN_ISOLATED_PAWN = S(-7, -24);

const Score BACKWARDS_PAWN = S(-16, -35);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(112, 22), S(3, 61), S(5, 22), S(3, -1), S(5, -1), S(8, -9), S(0, 0),},
 { S(0, 0), S(158, 66), S(39, 88), S(13, 27), S(20, 5), S(12, -5), S(-1, 2), S(0, 0),},
 { S(0, 0), S(151, 148), S(55, 85), S(30, 30), S(13, 8), S(10, 3), S(0, -10), S(0, 0),},
 { S(0, 0), S(114, 162), S(51, 93), S(20, 37), S(12, 16), S(11, 7), S(9, 4), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 365, 297), S(  18, 155),
 S( -28, 138), S( -47,  99), S( -58,  54), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(10, -33);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 272, 472), S( 120, 401), S(  27, 243),
 S( -22, 163), S( -22,  86), S( -17,  75), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -21);

const Score PASSED_PAWN_KING_PROXIMITY = S(-13, 51);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 172, 560), S(  22, 293), S(  14, 103), S(  22,  22),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(55, -249);

const Score PASSED_PAWN_SQ_RULE = S(0, 813);

const Score PASSED_PAWN_UNSUPPORTED = S(-45, -11);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(-34, 173);

const Score KNIGHT_THREATS[6] = { S(-1, 46), S(-9, 103), S(73, 82), S(176, 27), S(146, -105), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 43), S(47, 82), S(0, 0), S(147, 47), S(128, 64), S(210, 2999),};

const Score ROOK_THREATS[6] = { S(-1, 49), S(64, 95), S(69, 116), S(16, 45), S(100, -73), S(411, 2185),};

const Score KING_THREAT = S(24, 69);

const Score PAWN_THREAT = S(174, 86);

const Score PAWN_PUSH_THREAT = S(37, 40);

const Score PAWN_PUSH_THREAT_PINNED = S(108, 271);

const Score HANGING_THREAT = S(23, 45);

const Score KNIGHT_CHECK_QUEEN = S(27, -1);

const Score BISHOP_CHECK_QUEEN = S(46, 45);

const Score ROOK_CHECK_QUEEN = S(42, 10);

const Score SPACE = 223;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(17, 57), S(0, 0),},
 { S(10, 46), S(-6, -44), S(0, 0),},
 { S(8, 78), S(-51, -2), S(-16, 9), S(0, 0),},
 { S(95, 100), S(-114, 110), S(-62, 195), S(-496, 373), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-33, -15), S(63, 195), S(-21, 93), S(-30, -5), S(20, -31), S(89, -76), S(75, -119), S(0, 0),},
 { S(-86, 16), S(-46, 157), S(-68, 103), S(-61, 50), S(-54, 26), S(52, -38), S(73, -67), S(0, 0),},
 { S(-60, 8), S(-17, 204), S(-99, 94), S(-52, 36), S(-47, 20), S(-24, 10), S(45, -21), S(0, 0),},
 { S(-85, 29), S(71, 136), S(-107, 69), S(-77, 44), S(-71, 33), S(-43, 14), S(-53, 36), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-32, -16), S(-38, 0), S(-37, -8), S(-53, 12), S(-113, 69), S(110, 156), S(575, 163), S(0, 0),},
 { S(-14, 3), S(15, -10), S(17, -6), S(-21, 23), S(-53, 48), S(-54, 157), S(186, 279), S(0, 0),},
 { S(58, -8), S(57, -17), S(47, -6), S(21, 8), S(-21, 29), S(-73, 140), S(161, 194), S(0, 0),},
 { S(-3, -8), S(6, -24), S(26, -21), S(5, -26), S(-33, 2), S(-45, 93), S(-129, 275), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-25, -76), S(81, -109), S(41, -74), S(36, -68), S(15, -61), S(12, -188), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(80, -51);

const Score COMPLEXITY_PAWNS = 4;

const Score COMPLEXITY_PAWNS_BOTH_SIDES = 150;

const Score COMPLEXITY_OFFSET = -196;

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

const Score TEMPO = 77;
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
  const int xside = side ^ 1;

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

  uint8_t rights = side == WHITE ? (board->castling & 0xC) : (board->castling & 0x3);
  s += CAN_CASTLE * bits((uint64_t)rights);
  if (T)
    C.castlingRights += cs[side] * bits((uint64_t)rights);

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
      pawnS += KPNetworkScore(board);

      TTPawnPut(board->pawnHash, pawnS, data.passedPawns, thread);
      s += pawnS;
    }
  } else {
    s += PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);
    s += KPNetworkScore(board);
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
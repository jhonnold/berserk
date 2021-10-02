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

const Score BISHOP_PAIR = S(41, 187);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 193, 400), S( 213, 495), S( 145, 464), S( 217, 408),
 S( 121, 318), S(  93, 338), S( 111, 238), S( 132, 189),
 S( 102, 277), S( 110, 237), S(  78, 209), S(  71, 175),
 S(  83, 217), S(  66, 209), S(  73, 185), S(  78, 157),
 S(  70, 204), S(  59, 187), S(  71, 184), S(  68, 188),
 S(  70, 205), S(  96, 198), S( 109, 201), S(  97, 182),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 282, 304), S( 111, 345), S( 411, 293), S( 203, 427),
 S( 157, 206), S( 175, 178), S( 200, 140), S( 146, 166),
 S(  82, 202), S( 123, 187), S(  86, 162), S( 118, 158),
 S(  49, 181), S(  66, 179), S(  67, 163), S(  92, 143),
 S(  18, 181), S(  56, 160), S(  66, 175), S(  85, 180),
 S(  13, 193), S( 108, 182), S( 108, 199), S(  87, 201),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-322, 187), S(-348, 247), S(-296, 278), S(-218, 238),
 S(-177, 259), S(-141, 273), S( -93, 248), S( -82, 252),
 S( -51, 213), S( -71, 242), S( -83, 263), S(-107, 273),
 S( -84, 276), S( -74, 266), S( -49, 264), S( -69, 275),
 S( -86, 261), S( -73, 258), S( -45, 278), S( -62, 281),
 S(-127, 219), S(-108, 217), S( -88, 233), S( -94, 266),
 S(-152, 225), S(-117, 212), S(-107, 229), S( -96, 224),
 S(-195, 231), S(-121, 224), S(-120, 222), S(-102, 245),
},{
 S(-420,-133), S(-497, 187), S(-219, 154), S( -78, 216),
 S(-117, 128), S(  14, 178), S( -59, 236), S(-106, 250),
 S( -24, 162), S(-109, 211), S( -10, 163), S( -42, 231),
 S( -61, 237), S( -56, 233), S( -42, 248), S( -59, 270),
 S( -73, 261), S( -90, 251), S( -38, 272), S( -71, 301),
 S( -85, 209), S( -55, 203), S( -75, 220), S( -65, 259),
 S( -90, 216), S( -66, 209), S( -94, 215), S( -95, 218),
 S(-110, 235), S(-133, 229), S( -97, 212), S( -83, 239),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -62, 325), S(-102, 358), S( -78, 325), S(-131, 366),
 S( -22, 324), S( -46, 289), S(  30, 306), S( -10, 342),
 S(  16, 329), S(  22, 326), S( -41, 291), S(  23, 312),
 S(   4, 323), S(  52, 315), S(  28, 326), S(  13, 358),
 S(  43, 297), S(  20, 322), S(  43, 320), S(  60, 314),
 S(  34, 278), S(  73, 305), S(  40, 271), S(  48, 303),
 S(  45, 269), S(  41, 211), S(  72, 253), S(  34, 290),
 S(  29, 223), S(  69, 262), S(  42, 302), S(  50, 294),
},{
 S(-131, 234), S(  76, 288), S(-100, 306), S(-153, 316),
 S(  -6, 228), S( -77, 293), S( -33, 325), S(  25, 295),
 S(  59, 305), S(  -3, 316), S(   0, 300), S(  18, 317),
 S(  -5, 303), S(  55, 308), S(  60, 318), S(  53, 316),
 S(  75, 254), S(  58, 299), S(  64, 315), S(  53, 316),
 S(  63, 256), S(  94, 281), S(  58, 270), S(  47, 325),
 S(  66, 214), S(  65, 214), S(  64, 279), S(  53, 290),
 S(  65, 175), S(  48, 321), S(  30, 307), S(  65, 291),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-300, 546), S(-288, 559), S(-282, 564), S(-348, 572),
 S(-267, 529), S(-282, 553), S(-251, 553), S(-217, 514),
 S(-304, 552), S(-238, 541), S(-258, 543), S(-255, 528),
 S(-286, 568), S(-262, 565), S(-233, 557), S(-227, 527),
 S(-289, 551), S(-284, 558), S(-264, 544), S(-256, 531),
 S(-299, 530), S(-280, 505), S(-267, 510), S(-269, 510),
 S(-288, 491), S(-280, 505), S(-252, 498), S(-246, 485),
 S(-291, 511), S(-275, 496), S(-271, 506), S(-256, 481),
},{
 S(-168, 515), S(-329, 594), S(-395, 593), S(-283, 550),
 S(-220, 489), S(-208, 516), S(-235, 517), S(-268, 521),
 S(-271, 504), S(-169, 498), S(-223, 489), S(-185, 481),
 S(-265, 520), S(-200, 517), S(-215, 501), S(-207, 507),
 S(-293, 501), S(-257, 510), S(-258, 502), S(-233, 512),
 S(-271, 447), S(-232, 444), S(-246, 462), S(-241, 483),
 S(-260, 443), S(-210, 425), S(-242, 447), S(-234, 465),
 S(-282, 467), S(-240, 459), S(-258, 463), S(-236, 460),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-156, 797), S(  11, 601), S(  77, 618), S(  56, 674),
 S(  -1, 665), S(  48, 589), S(  63, 657), S(  62, 648),
 S(  57, 614), S(  76, 589), S(  68, 641), S( 102, 637),
 S(  49, 654), S(  58, 690), S(  75, 681), S(  50, 697),
 S(  58, 611), S(  32, 705), S(  41, 683), S(  27, 725),
 S(  42, 576), S(  48, 604), S(  41, 643), S(  31, 647),
 S(  48, 507), S(  44, 538), S(  56, 543), S(  54, 573),
 S(  17, 553), S(  19, 551), S(  23, 561), S(  29, 570),
},{
 S( 142, 593), S( 258, 564), S(-130, 790), S( 102, 575),
 S( 124, 570), S(  91, 574), S(  85, 612), S(  49, 681),
 S( 119, 553), S(  97, 518), S( 126, 581), S(  96, 637),
 S(  48, 644), S(  72, 650), S( 111, 583), S(  69, 690),
 S(  67, 579), S(  62, 630), S(  76, 632), S(  37, 722),
 S(  59, 508), S(  74, 556), S(  73, 608), S(  42, 649),
 S(  86, 440), S(  89, 436), S(  79, 481), S(  64, 566),
 S(  68, 494), S(   6, 551), S(  20, 510), S(  39, 535),
}};

const Score KING_PSQT[2][32] = {{
 S( 516,-400), S( 207,-156), S(  41,-114), S( 269,-162),
 S(-236, 107), S(-285, 233), S(-133, 153), S( -33,  91),
 S(  -8,  64), S(-139, 206), S( -25, 164), S(-116, 183),
 S(  17,  53), S( -22, 146), S( -74, 125), S( -76, 118),
 S( -60,  11), S(   9,  73), S(  48,  43), S( -71,  62),
 S( -23, -39), S(  -1,  17), S(  -3,   4), S( -35,  15),
 S(  27, -84), S( -34,  22), S( -39, -17), S( -43, -17),
 S(  42,-209), S(  52,-109), S(  29,-125), S( -74,-106),
},{
 S(-117,-307), S(-482,  56), S( -39,-128), S( -28,-127),
 S(-423,  23), S(-211, 205), S(-306, 173), S( 334,  23),
 S(-192,  42), S(  60, 202), S( 295, 140), S(  92, 168),
 S(-151,  16), S(  23, 137), S(  36, 143), S(  -1, 135),
 S(-155,  -5), S(-101,  81), S(  19,  67), S( -23,  85),
 S(  -7, -31), S(  32,  40), S(   9,  31), S(  21,  41),
 S(  28, -29), S(  12,  53), S( -12,  21), S( -45,  14),
 S(  30,-137), S(  35, -33), S(   7, -64), S( -60, -74),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S(-110,  25), S(   5,  42), S(  56,  60), S( 119,  87),
 S(  29,  -6), S(  75,  43), S(  59,  58), S(  80,  83),
 S(  44,   1), S(  68,  23), S(  45,  34), S(  40,  45),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -13,  10), S(  44,   1), S( 118,  25), S( 122,  14),
 S(   7,  -6), S(  47,  24), S(  86,   7), S( 103,  20),
 S(  -9,  42), S(  85,  14), S(  65,  27), S(  75,  45),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-250, -49), S(-168, 118), S(-132, 198), S(-107, 228),
 S( -89, 254), S( -74, 281), S( -59, 289), S( -42, 295),
 S( -25, 279),};

const Score BISHOP_MOBILITIES[14] = {
 S( -40, 138), S(   6, 201), S(  33, 247), S(  47, 290),
 S(  60, 308), S(  67, 330), S(  71, 347), S(  78, 355),
 S(  77, 364), S(  84, 368), S(  98, 362), S( 119, 356),
 S( 120, 369), S( 148, 338),};

const Score ROOK_MOBILITIES[15] = {
 S(-247, -81), S(-279, 378), S(-251, 449), S(-240, 450),
 S(-248, 491), S(-238, 504), S(-249, 521), S(-240, 522),
 S(-234, 534), S(-225, 545), S(-222, 555), S(-231, 571),
 S(-225, 578), S(-212, 590), S(-199, 582),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2252,-2394), S(-136,-389), S( -38, 580), S(   0, 869),
 S(  25, 948), S(  38, 969), S(  37,1034), S(  41,1077),
 S(  46,1109), S(  53,1118), S(  59,1129), S(  67,1130),
 S(  67,1142), S(  75,1142), S(  75,1146), S(  90,1129),
 S(  87,1133), S(  84,1139), S(  96,1105), S( 134,1046),
 S( 165, 983), S( 185, 914), S( 248, 850), S( 300, 700),
 S( 265, 687), S( 283, 583), S(  24, 573), S( 782, -55),
};

const Score KING_MOBILITIES[9] = {
 S(   6, -24), S(   5,  40), S(   1,  47),
 S(  -4,  45), S(  -3,  28), S(  -4,   5),
 S(  -8,  -1), S(  -2, -22), S(   5, -88),
};

const Score MINOR_BEHIND_PAWN = S(11, 27);

const Score KNIGHT_OUTPOST_REACHABLE = S(19, 42);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 13);

const Score BISHOP_TRAPPED = S(-217, -534);

const Score ROOK_TRAPPED = S(-78, -55);

const Score BAD_BISHOP_PAWNS = S(-2, -8);

const Score DRAGON_BISHOP = S(45, 37);

const Score ROOK_OPEN_FILE_OFFSET = S(29, 14);

const Score ROOK_OPEN_FILE = S(55, 32);

const Score ROOK_SEMI_OPEN = S(32, 13);

const Score ROOK_TO_OPEN = S(33, 38);

const Score QUEEN_OPPOSITE_ROOK = S(-29, 5);

const Score QUEEN_ROOK_BATTERY = S(9, 103);

const Score DEFENDED_PAWN = S(28, 21);

const Score DOUBLED_PAWN = S(41, -70);

const Score ISOLATED_PAWN[4] = {
 S(   0, -27), S(  -1, -27), S( -15, -16), S(   2, -16),
};

const Score OPEN_ISOLATED_PAWN = S(-2, -18);

const Score BACKWARDS_PAWN = S(-16, -32);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(130, 37), S(-11, 58), S(0, 17), S(2, 0), S(6, 3), S(10, -6), S(0, 0),},
 { S(0, 0), S(160, 58), S(29, 84), S(8, 26), S(20, 5), S(17, 0), S(1, 4), S(0, 0),},
 { S(0, 0), S(158, 154), S(54, 86), S(26, 28), S(12, 8), S(11, 8), S(3, -5), S(0, 0),},
 { S(0, 0), S(121, 154), S(51, 100), S(20, 37), S(14, 19), S(12, 10), S(13, 8), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 389, 309), S(  30, 172),
 S( -31, 143), S( -45, 101), S( -61,  53), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(9, -33);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 247, 450), S( 115, 405), S(  27, 250),
 S( -19, 169), S( -16,  94), S(  -8,  82), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -21);

const Score PASSED_PAWN_KING_PROXIMITY = S(-11, 50);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 185, 564), S(  19, 303), S(  18, 103), S(  26,  25),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(60, -248);

const Score PASSED_PAWN_SQ_RULE = S(0, 805);

const Score PASSED_PAWN_UNSUPPORTED = S(-47, -9);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(-33, 179);

const Score KNIGHT_THREATS[6] = { S(-2, 42), S(-6, 90), S(71, 78), S(175, 25), S(145, -102), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 40), S(45, 78), S(0, 0), S(146, 42), S(126, 66), S(208, 2962),};

const Score ROOK_THREATS[6] = { S(-1, 46), S(64, 92), S(68, 113), S(16, 45), S(101, -76), S(415, 2161),};

const Score KING_THREAT = S(24, 71);

const Score PAWN_THREAT = S(173, 86);

const Score PAWN_PUSH_THREAT = S(37, 43);

const Score PAWN_PUSH_THREAT_PINNED = S(106, 266);

const Score HANGING_THREAT = S(23, 43);

const Score KNIGHT_CHECK_QUEEN = S(27, 0);

const Score BISHOP_CHECK_QUEEN = S(45, 48);

const Score ROOK_CHECK_QUEEN = S(41, 13);

const Score SPACE = 234;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(12, 52), S(0, 0),},
 { S(5, 42), S(-7, -47), S(0, 0),},
 { S(2, 72), S(-54, -6), S(-17, 5), S(0, 0),},
 { S(90, 96), S(-118, 93), S(-64, 180), S(-502, 351), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-46, -23), S(72, 188), S(-10, 98), S(-25, -2), S(24, -27), S(87, -75), S(76, -121), S(0, 0),},
 { S(-93, 10), S(-27, 165), S(-63, 118), S(-70, 53), S(-55, 22), S(55, -38), S(69, -63), S(0, 0),},
 { S(-60, 4), S(-19, 212), S(-99, 111), S(-49, 44), S(-43, 16), S(-22, 8), S(42, -16), S(0, 0),},
 { S(-81, 30), S(58, 132), S(-115, 66), S(-68, 57), S(-61, 43), S(-35, 16), S(-53, 39), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-31, -18), S(-43, -4), S(-40, -14), S(-54, 11), S(-111, 65), S(130, 168), S(587, 167), S(0, 0),},
 { S(-14, 5), S(18, -8), S(15, -7), S(-19, 24), S(-47, 45), S(-49, 166), S(210, 291), S(0, 0),},
 { S(54, -5), S(61, -16), S(45, -10), S(20, 5), S(-16, 29), S(-80, 159), S(178, 199), S(0, 0),},
 { S(-1, -5), S(9, -26), S(33, -19), S(9, -26), S(-31, 8), S(-38, 99), S(-120, 280), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-21, -77), S(85, -114), S(44, -76), S(36, -67), S(19, -55), S(23, -175), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(83, -40);

const Score COMPLEXITY_PAWNS = 5;

const Score COMPLEXITY_PAWNS_BOTH_SIDES = 150;

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
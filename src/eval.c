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

const Score BISHOP_PAIR = S(40, 192);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 166, 400), S( 252, 540), S( 200, 512), S( 262, 451),
 S( 157, 355), S( 121, 362), S( 172, 300), S( 181, 229),
 S( 110, 280), S( 130, 248), S( 107, 224), S(  88, 179),
 S(  78, 205), S(  62, 195), S(  78, 175), S(  75, 145),
 S(  65, 185), S(  48, 166), S(  61, 161), S(  63, 170),
 S(  62, 184), S(  79, 171), S(  83, 162), S(  92, 168),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 287, 253), S( 120, 312), S( 446, 303), S( 250, 442),
 S( 190, 223), S( 196, 164), S( 244, 156), S( 193, 182),
 S(  92, 188), S( 140, 176), S( 114, 172), S( 122, 144),
 S(  50, 163), S(  65, 162), S(  76, 160), S(  88, 131),
 S(  19, 166), S(  53, 143), S(  65, 158), S(  81, 172),
 S(  16, 175), S(  95, 157), S(  84, 164), S(  77, 185),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-330, 181), S(-375, 257), S(-309, 280), S(-220, 230),
 S(-191, 266), S(-153, 282), S( -96, 247), S( -83, 247),
 S( -68, 223), S( -84, 248), S( -89, 269), S(-112, 279),
 S( -88, 280), S( -79, 269), S( -54, 269), S( -74, 279),
 S( -89, 267), S( -79, 264), S( -52, 284), S( -65, 288),
 S(-132, 224), S(-112, 225), S( -91, 237), S( -97, 272),
 S(-153, 232), S(-121, 216), S(-111, 231), S( -99, 229),
 S(-194, 236), S(-122, 228), S(-119, 227), S(-101, 248),
},{
 S(-427,-147), S(-495, 179), S(-245, 158), S( -85, 214),
 S(-130, 113), S(  16, 173), S( -63, 236), S(-105, 246),
 S( -35, 152), S(-114, 208), S( -10, 150), S( -46, 234),
 S( -65, 231), S( -62, 228), S( -45, 248), S( -65, 273),
 S( -77, 261), S( -97, 250), S( -40, 273), S( -75, 305),
 S( -91, 212), S( -59, 207), S( -79, 224), S( -68, 264),
 S( -93, 222), S( -70, 215), S( -97, 218), S( -99, 225),
 S(-109, 231), S(-136, 232), S(-101, 217), S( -86, 247),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -62, 319), S(-118, 367), S( -95, 334), S(-141, 372),
 S( -31, 326), S( -54, 295), S(  22, 316), S( -14, 344),
 S(   3, 343), S(  13, 335), S( -44, 294), S(  19, 321),
 S(  -2, 335), S(  48, 322), S(  24, 334), S(   7, 366),
 S(  40, 305), S(  15, 331), S(  40, 329), S(  58, 323),
 S(  32, 280), S(  71, 313), S(  41, 276), S(  47, 311),
 S(  44, 275), S(  40, 212), S(  67, 263), S(  32, 298),
 S(  28, 225), S(  67, 268), S(  42, 305), S(  48, 302),
},{
 S(-149, 231), S(  57, 293), S(-105, 306), S(-163, 317),
 S( -23, 217), S( -88, 290), S( -39, 330), S(  18, 300),
 S(  50, 303), S( -12, 314), S(  -8, 299), S(  14, 318),
 S(  -9, 302), S(  51, 309), S(  55, 321), S(  47, 324),
 S(  73, 256), S(  54, 300), S(  61, 322), S(  49, 321),
 S(  58, 261), S(  90, 287), S(  57, 275), S(  46, 333),
 S(  62, 217), S(  63, 217), S(  61, 288), S(  49, 299),
 S(  63, 171), S(  51, 313), S(  26, 314), S(  63, 297),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-301, 560), S(-291, 576), S(-286, 585), S(-344, 586),
 S(-275, 548), S(-293, 572), S(-256, 569), S(-219, 529),
 S(-314, 572), S(-244, 556), S(-268, 558), S(-259, 542),
 S(-296, 592), S(-273, 585), S(-242, 574), S(-234, 542),
 S(-296, 568), S(-289, 576), S(-272, 561), S(-260, 548),
 S(-305, 551), S(-282, 525), S(-273, 533), S(-273, 527),
 S(-293, 507), S(-284, 523), S(-257, 515), S(-250, 500),
 S(-297, 526), S(-281, 513), S(-277, 526), S(-262, 499),
},{
 S(-187, 533), S(-343, 612), S(-407, 609), S(-290, 569),
 S(-236, 498), S(-217, 523), S(-246, 536), S(-278, 541),
 S(-277, 507), S(-172, 506), S(-234, 490), S(-198, 494),
 S(-268, 527), S(-211, 526), S(-228, 515), S(-219, 522),
 S(-296, 510), S(-263, 517), S(-266, 515), S(-243, 527),
 S(-275, 460), S(-235, 454), S(-248, 474), S(-247, 500),
 S(-264, 456), S(-215, 435), S(-245, 458), S(-240, 480),
 S(-287, 480), S(-246, 472), S(-264, 478), S(-243, 477),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-164, 825), S(  11, 616), S(  71, 642), S(  70, 680),
 S(  -6, 672), S(  46, 596), S(  66, 664), S(  64, 659),
 S(  58, 611), S(  67, 611), S(  67, 647), S(  99, 658),
 S(  47, 667), S(  59, 702), S(  74, 694), S(  49, 710),
 S(  62, 618), S(  34, 722), S(  42, 693), S(  28, 748),
 S(  48, 578), S(  52, 613), S(  46, 656), S(  35, 657),
 S(  50, 515), S(  48, 544), S(  60, 551), S(  56, 587),
 S(  24, 557), S(  22, 561), S(  27, 569), S(  31, 581),
},{
 S( 134, 600), S( 250, 565), S( -98, 782), S(  83, 617),
 S( 106, 579), S(  75, 577), S(  91, 619), S(  50, 697),
 S( 123, 554), S(  99, 524), S( 128, 586), S(  98, 647),
 S(  54, 642), S(  74, 654), S( 113, 589), S(  69, 704),
 S(  72, 580), S(  64, 638), S(  78, 645), S(  40, 734),
 S(  61, 515), S(  77, 568), S(  78, 618), S(  45, 666),
 S(  93, 429), S(  91, 445), S(  80, 498), S(  66, 581),
 S(  76, 500), S(  11, 562), S(  24, 519), S(  42, 548),
}};

const Score KING_PSQT[2][32] = {{
 S( 462,-403), S( 235,-124), S(  13,-136), S( 198,-224),
 S(-241, 139), S(-221, 215), S( -94, 113), S( -59,  34),
 S( -15,  26), S(-172, 156), S(  -9, 112), S(-115, 119),
 S( -20,  50), S( -67, 104), S(-109,  87), S( -80,  57),
 S( -48,  48), S(  -3,  91), S(  44,  64), S( -83,  76),
 S(  -5, -27), S(   2,  28), S(  22,  44), S(  -7,  56),
 S(  73, -55), S( -21,  32), S( -14,  14), S( -37,   5),
 S(  63,-189), S(  73, -95), S(   7,-123), S( -98, -95),
},{
 S(-121,-318), S(-507, 106), S(  20,-127), S( -92,-188),
 S(-402,  60), S(-238, 232), S(-299, 141), S( 272,  -2),
 S(-238,  37), S(   0, 164), S( 238,  81), S(  41, 107),
 S(-206,  27), S( -39, 101), S( -11,  94), S( -49,  67),
 S(-162,  37), S(-137,  83), S(  22,  62), S( -21,  68),
 S(   1, -18), S(  26,  39), S(  25,  55), S(  33,  64),
 S(  61,  -6), S(   6,  52), S(  -9,  38), S( -54,  27),
 S(  42,-122), S(  43, -23), S( -24, -67), S( -87, -72),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -98,  28), S(  12,  42), S(  54,  61), S( 124,  89),
 S(  33,  -5), S(  77,  51), S(  61,  59), S(  80,  90),
 S(  42,   1), S(  76,  21), S(  46,  37), S(  41,  49),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -3,  12), S(  50,   1), S( 115,  30), S( 126,  18),
 S(   7,  -8), S(  49,  26), S(  88,   7), S( 104,  18),
 S(  -8,  41), S(  91,  14), S(  63,  25), S(  72,  42),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-263, -51), S(-180, 124), S(-142, 206), S(-117, 237),
 S( -99, 264), S( -83, 291), S( -67, 298), S( -51, 304),
 S( -36, 288),};

const Score BISHOP_MOBILITIES[14] = {
 S( -52, 141), S(   0, 207), S(  27, 256), S(  42, 301),
 S(  55, 320), S(  63, 342), S(  66, 358), S(  74, 365),
 S(  74, 375), S(  82, 378), S(  95, 371), S( 117, 366),
 S( 117, 375), S( 143, 343),};

const Score ROOK_MOBILITIES[15] = {
 S(-270, -70), S(-290, 390), S(-260, 466), S(-249, 468),
 S(-254, 512), S(-244, 526), S(-254, 544), S(-245, 544),
 S(-238, 556), S(-229, 567), S(-227, 578), S(-236, 597),
 S(-232, 603), S(-222, 614), S(-204, 600),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2283,-2399), S(-150,-342), S( -38, 591), S(  -1, 897),
 S(  24, 972), S(  39, 992), S(  38,1060), S(  43,1102),
 S(  49,1133), S(  56,1141), S(  62,1153), S(  71,1152),
 S(  70,1165), S(  80,1163), S(  80,1164), S(  94,1150),
 S(  93,1151), S(  90,1158), S( 100,1126), S( 141,1060),
 S( 170, 995), S( 190, 928), S( 249, 870), S( 311, 697),
 S( 283, 687), S( 299, 567), S( -39, 630), S( 802,-104),
};

const Score KING_MOBILITIES[9] = {
 S(   8, -37), S(   6,  35), S(   1,  44),
 S(  -4,  45), S(  -4,  30), S(  -8,   9),
 S( -14,   4), S(  -9, -17), S(   3, -78),
};

const Score MINOR_BEHIND_PAWN = S(11, 30);

const Score KNIGHT_OUTPOST_REACHABLE = S(20, 44);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 14);

const Score BISHOP_TRAPPED = S(-236, -536);

const Score ROOK_TRAPPED = S(-78, -56);

const Score BAD_BISHOP_PAWNS = S(-2, -9);

const Score DRAGON_BISHOP = S(43, 41);

const Score ROOK_OPEN_FILE_OFFSET = S(28, 13);

const Score ROOK_OPEN_FILE = S(58, 33);

const Score ROOK_SEMI_OPEN = S(34, 15);

const Score ROOK_TO_OPEN = S(28, 42);

const Score QUEEN_OPPOSITE_ROOK = S(-30, 7);

const Score QUEEN_ROOK_BATTERY = S(9, 106);

const Score DEFENDED_PAWN = S(22, 14);

const Score DOUBLED_PAWN = S(27, -87);

const Score ISOLATED_PAWN[4] = {
 S(  -4, -32), S(  -3, -31), S( -13, -18), S(  -3, -22),
};

const Score OPEN_ISOLATED_PAWN = S(-7, -24);

const Score BACKWARDS_PAWN = S(-17, -36);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(93, 35), S(1, 61), S(3, 22), S(3, 0), S(4, -2), S(8, -10), S(0, 0),},
 { S(0, 0), S(164, 82), S(39, 86), S(11, 27), S(20, 6), S(12, -5), S(-1, 3), S(0, 0),},
 { S(0, 0), S(129, 145), S(49, 82), S(27, 30), S(13, 8), S(10, 3), S(-1, -12), S(0, 0),},
 { S(0, 0), S(118, 159), S(46, 91), S(19, 37), S(12, 15), S(11, 7), S(10, 4), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 386, 270), S(  14, 152),
 S( -30, 138), S( -47,  97), S( -56,  53), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(10, -32);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 302, 484), S( 130, 389), S(  23, 238),
 S( -22, 157), S( -21,  85), S( -14,  75), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(5, -21);

const Score PASSED_PAWN_KING_PROXIMITY = S(-12, 49);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 184, 547), S(  22, 292), S(  14, 107), S(  21,  29),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(55, -249);

const Score PASSED_PAWN_SQ_RULE = S(0, 833);

const Score PASSED_PAWN_UNSUPPORTED = S(-43, -11);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(-7, 172);

const Score KNIGHT_THREATS[6] = { S(-1, 47), S(-7, 105), S(73, 84), S(177, 26), S(145, -103), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 44), S(47, 82), S(0, 0), S(148, 47), S(128, 66), S(208, 3031),};

const Score ROOK_THREATS[6] = { S(-1, 49), S(65, 97), S(69, 118), S(14, 50), S(100, -71), S(406, 2218),};

const Score KING_THREAT = S(27, 70);

const Score PAWN_THREAT = S(174, 88);

const Score PAWN_PUSH_THREAT = S(37, 41);

const Score PAWN_PUSH_THREAT_PINNED = S(110, 272);

const Score HANGING_THREAT = S(24, 45);

const Score KNIGHT_CHECK_QUEEN = S(27, 0);

const Score BISHOP_CHECK_QUEEN = S(46, 47);

const Score ROOK_CHECK_QUEEN = S(41, 13);

const Score SPACE = 202;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(19, 60), S(0, 0),},
 { S(11, 49), S(-6, -46), S(0, 0),},
 { S(12, 80), S(-54, -4), S(-17, 7), S(0, 0),},
 { S(100, 107), S(-118, 106), S(-64, 195), S(-497, 380), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-31, -11), S(36, 218), S(-18, 97), S(-32, -4), S(22, -27), S(94, -73), S(78, -114), S(0, 0),},
 { S(-85, 22), S(-69, 174), S(-77, 108), S(-65, 54), S(-52, 30), S(57, -34), S(77, -61), S(0, 0),},
 { S(-61, 10), S(-13, 189), S(-96, 92), S(-51, 36), S(-49, 21), S(-25, 9), S(47, -21), S(0, 0),},
 { S(-94, 21), S(99, 114), S(-95, 49), S(-80, 37), S(-80, 25), S(-51, 7), S(-70, 28), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-40, -14), S(-39, 1), S(-38, -8), S(-53, 14), S(-110, 73), S(124, 161), S(558, 165), S(0, 0),},
 { S(-22, -1), S(15, -11), S(16, -8), S(-18, 22), S(-47, 45), S(-41, 144), S(174, 259), S(0, 0),},
 { S(57, -8), S(59, -15), S(50, -5), S(26, 10), S(-14, 29), S(-82, 115), S(139, 176), S(0, 0),},
 { S(4, -4), S(12, -21), S(32, -17), S(8, -25), S(-28, 5), S(-44, 100), S(-145, 274), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-26, -81), S(85, -116), S(42, -72), S(40, -68), S(22, -61), S(11, -199), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(81, -50);

const Score COMPLEXITY_PAWNS = 4;

const Score COMPLEXITY_PAWNS_BOTH_SIDES = 149;

const Score COMPLEXITY_OFFSET = -198;

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
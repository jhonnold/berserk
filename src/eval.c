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
 S( 165, 400), S( 254, 540), S( 203, 513), S( 263, 451),
 S( 158, 355), S( 122, 362), S( 181, 301), S( 191, 231),
 S( 110, 280), S( 130, 246), S( 108, 224), S(  87, 177),
 S(  78, 205), S(  62, 195), S(  78, 174), S(  76, 142),
 S(  66, 185), S(  47, 163), S(  61, 160), S(  63, 168),
 S(  63, 183), S(  79, 170), S(  82, 160), S(  91, 166),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 287, 252), S( 121, 312), S( 446, 301), S( 254, 443),
 S( 192, 223), S( 198, 163), S( 247, 157), S( 201, 183),
 S(  92, 188), S( 140, 175), S( 114, 170), S( 122, 142),
 S(  50, 162), S(  65, 161), S(  76, 158), S(  88, 129),
 S(  19, 165), S(  52, 140), S(  65, 158), S(  81, 170),
 S(  16, 174), S(  94, 155), S(  83, 162), S(  77, 184),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-331, 181), S(-376, 258), S(-309, 280), S(-220, 229),
 S(-192, 266), S(-153, 281), S( -96, 247), S( -83, 248),
 S( -69, 222), S( -84, 248), S( -89, 269), S(-112, 278),
 S( -88, 280), S( -79, 268), S( -54, 269), S( -74, 279),
 S( -89, 267), S( -79, 263), S( -52, 284), S( -65, 288),
 S(-132, 223), S(-112, 225), S( -91, 237), S( -98, 271),
 S(-153, 231), S(-122, 215), S(-112, 231), S(-100, 230),
 S(-195, 237), S(-122, 228), S(-119, 227), S(-102, 248),
},{
 S(-427,-147), S(-495, 179), S(-245, 158), S( -85, 214),
 S(-130, 112), S(  16, 173), S( -63, 236), S(-104, 247),
 S( -35, 152), S(-115, 207), S( -10, 150), S( -46, 232),
 S( -65, 232), S( -62, 228), S( -45, 248), S( -65, 273),
 S( -77, 262), S( -97, 251), S( -41, 274), S( -75, 306),
 S( -91, 213), S( -59, 207), S( -79, 224), S( -68, 264),
 S( -93, 222), S( -69, 215), S( -97, 218), S( -99, 225),
 S(-109, 231), S(-136, 232), S(-101, 218), S( -86, 246),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -62, 319), S(-118, 368), S( -95, 333), S(-141, 371),
 S( -32, 326), S( -54, 294), S(  22, 316), S( -14, 344),
 S(   3, 342), S(  13, 335), S( -44, 294), S(  19, 320),
 S(  -3, 336), S(  48, 321), S(  24, 334), S(   7, 365),
 S(  40, 305), S(  15, 331), S(  39, 329), S(  58, 323),
 S(  32, 280), S(  71, 314), S(  41, 277), S(  47, 311),
 S(  43, 276), S(  40, 212), S(  67, 263), S(  31, 298),
 S(  28, 225), S(  67, 268), S(  42, 305), S(  48, 302),
},{
 S(-149, 229), S(  58, 293), S(-105, 306), S(-163, 317),
 S( -23, 217), S( -88, 291), S( -40, 329), S(  18, 299),
 S(  49, 304), S( -12, 315), S(  -8, 299), S(  14, 318),
 S(  -9, 302), S(  51, 309), S(  54, 321), S(  47, 324),
 S(  73, 256), S(  53, 300), S(  61, 321), S(  49, 321),
 S(  57, 261), S(  90, 287), S(  57, 275), S(  46, 333),
 S(  62, 216), S(  63, 218), S(  61, 289), S(  49, 299),
 S(  63, 171), S(  50, 313), S(  26, 313), S(  63, 297),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-301, 560), S(-291, 576), S(-286, 584), S(-344, 588),
 S(-275, 548), S(-293, 571), S(-257, 569), S(-219, 529),
 S(-314, 572), S(-245, 556), S(-268, 559), S(-260, 542),
 S(-297, 592), S(-273, 586), S(-242, 574), S(-234, 541),
 S(-296, 569), S(-288, 575), S(-272, 561), S(-260, 549),
 S(-306, 551), S(-283, 524), S(-273, 532), S(-274, 528),
 S(-293, 508), S(-284, 522), S(-258, 516), S(-251, 501),
 S(-297, 526), S(-281, 514), S(-278, 526), S(-263, 500),
},{
 S(-187, 533), S(-342, 611), S(-407, 611), S(-291, 569),
 S(-237, 499), S(-217, 523), S(-245, 535), S(-278, 541),
 S(-278, 508), S(-172, 507), S(-235, 491), S(-198, 494),
 S(-268, 527), S(-210, 526), S(-228, 515), S(-219, 522),
 S(-296, 511), S(-263, 517), S(-266, 516), S(-244, 527),
 S(-276, 460), S(-236, 455), S(-249, 474), S(-247, 500),
 S(-264, 456), S(-215, 436), S(-246, 458), S(-241, 480),
 S(-288, 480), S(-246, 472), S(-265, 479), S(-243, 477),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-164, 826), S(  11, 616), S(  70, 643), S(  70, 680),
 S(  -7, 673), S(  47, 594), S(  66, 663), S(  63, 659),
 S(  57, 612), S(  67, 610), S(  67, 646), S(  99, 659),
 S(  47, 667), S(  59, 702), S(  74, 694), S(  50, 710),
 S(  62, 617), S(  34, 722), S(  42, 693), S(  28, 748),
 S(  47, 579), S(  51, 612), S(  46, 656), S(  35, 658),
 S(  50, 515), S(  48, 543), S(  60, 551), S(  56, 587),
 S(  24, 558), S(  22, 562), S(  27, 569), S(  31, 581),
},{
 S( 134, 600), S( 250, 565), S( -99, 781), S(  83, 617),
 S( 106, 579), S(  75, 579), S(  91, 618), S(  50, 697),
 S( 123, 554), S(  99, 524), S( 128, 587), S(  97, 646),
 S(  54, 641), S(  74, 653), S( 113, 589), S(  69, 704),
 S(  72, 580), S(  65, 638), S(  78, 646), S(  40, 734),
 S(  61, 516), S(  77, 568), S(  78, 618), S(  45, 667),
 S(  93, 430), S(  91, 445), S(  80, 499), S(  66, 580),
 S(  76, 500), S(  11, 562), S(  24, 519), S(  41, 546),
}};

const Score KING_PSQT[2][32] = {{
 S( 461,-400), S( 235,-126), S(  13,-136), S( 193,-225),
 S(-240, 140), S(-218, 216), S( -94, 111), S( -59,  33),
 S( -15,  25), S(-174, 154), S(  -9, 110), S(-116, 118),
 S( -22,  50), S( -68, 103), S(-108,  87), S( -82,  55),
 S( -49,  49), S(  -3,  91), S(  44,  65), S( -83,  76),
 S(  -6, -27), S(   2,  30), S(  22,  44), S(  -8,  57),
 S(  73, -55), S( -20,  32), S( -14,  13), S( -36,   6),
 S(  63,-190), S(  73, -94), S(   8,-123), S( -98, -95),
},{
 S(-120,-316), S(-507, 107), S(  21,-126), S( -94,-190),
 S(-400,  61), S(-239, 232), S(-299, 138), S( 270,  -3),
 S(-241,  36), S(  -4, 163), S( 236,  81), S(  38, 106),
 S(-210,  24), S( -43, 100), S( -12,  94), S( -51,  63),
 S(-162,  37), S(-138,  82), S(  22,  64), S( -21,  67),
 S(   1, -17), S(  27,  40), S(  24,  56), S(  33,  65),
 S(  61,  -5), S(   6,  53), S(  -9,  38), S( -54,  27),
 S(  42,-122), S(  43, -23), S( -24, -67), S( -87, -73),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -99,  25), S(  12,  43), S(  54,  61), S( 124,  90),
 S(  33,  -6), S(  77,  51), S(  61,  60), S(  80,  90),
 S(  42,   1), S(  76,  22), S(  46,  37), S(  41,  49),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -4,  11), S(  50,   0), S( 116,  29), S( 126,  17),
 S(   7,  -8), S(  49,  26), S(  88,   7), S( 104,  18),
 S(  -8,  42), S(  91,  13), S(  63,  24), S(  73,  42),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-263, -51), S(-180, 123), S(-142, 206), S(-118, 236),
 S( -99, 264), S( -84, 291), S( -67, 298), S( -51, 304),
 S( -36, 288),};

const Score BISHOP_MOBILITIES[14] = {
 S( -52, 139), S(   0, 208), S(  27, 256), S(  42, 301),
 S(  55, 320), S(  62, 342), S(  66, 358), S(  74, 365),
 S(  74, 375), S(  82, 378), S(  95, 371), S( 117, 365),
 S( 117, 375), S( 142, 342),};

const Score ROOK_MOBILITIES[15] = {
 S(-270, -70), S(-291, 391), S(-260, 466), S(-249, 469),
 S(-255, 512), S(-244, 526), S(-254, 544), S(-246, 545),
 S(-239, 556), S(-229, 567), S(-227, 579), S(-237, 597),
 S(-232, 603), S(-222, 615), S(-205, 600),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2283,-2399), S(-151,-342), S( -38, 591), S(  -1, 896),
 S(  24, 972), S(  38, 992), S(  38,1060), S(  43,1102),
 S(  49,1133), S(  56,1141), S(  62,1153), S(  70,1152),
 S(  70,1164), S(  79,1163), S(  80,1164), S(  94,1150),
 S(  93,1151), S(  89,1159), S(  99,1126), S( 140,1061),
 S( 170, 995), S( 190, 928), S( 249, 870), S( 312, 697),
 S( 283, 687), S( 300, 566), S( -42, 630), S( 802,-104),
};

const Score KING_MOBILITIES[9] = {
 S(   8, -37), S(   6,  35), S(   1,  44),
 S(  -4,  45), S(  -4,  30), S(  -8,   9),
 S( -15,   4), S(  -9, -17), S(   2, -79),
};

const Score MINOR_BEHIND_PAWN = S(11, 30);

const Score KNIGHT_OUTPOST_REACHABLE = S(20, 44);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 14);

const Score BISHOP_TRAPPED = S(-236, -536);

const Score ROOK_TRAPPED = S(-78, -56);

const Score BAD_BISHOP_PAWNS = S(-2, -9);

const Score DRAGON_BISHOP = S(44, 41);

const Score ROOK_OPEN_FILE_OFFSET = S(28, 13);

const Score ROOK_OPEN_FILE = S(58, 33);

const Score ROOK_SEMI_OPEN = S(34, 16);

const Score ROOK_TO_OPEN = S(29, 42);

const Score QUEEN_OPPOSITE_ROOK = S(-30, 8);

const Score QUEEN_ROOK_BATTERY = S(9, 106);

const Score DEFENDED_PAWN = S(22, 14);

const Score DOUBLED_PAWN = S(27, -87);

const Score ISOLATED_PAWN[4] = {
 S(  -5, -32), S(  -3, -31), S( -13, -18), S(  -3, -22),
};

const Score OPEN_ISOLATED_PAWN = S(-7, -24);

const Score BACKWARDS_PAWN = S(-17, -36);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(93, 35), S(1, 61), S(3, 22), S(3, -1), S(5, -2), S(7, -10), S(0, 0),},
 { S(0, 0), S(163, 82), S(39, 85), S(11, 27), S(20, 6), S(13, -4), S(-1, 3), S(0, 0),},
 { S(0, 0), S(129, 146), S(48, 82), S(27, 30), S(13, 8), S(10, 3), S(-1, -11), S(0, 0),},
 { S(0, 0), S(118, 160), S(45, 92), S(20, 36), S(12, 15), S(11, 7), S(10, 4), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 387, 268), S(  15, 152),
 S( -30, 137), S( -47,  98), S( -56,  53), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(10, -32);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 305, 484), S( 129, 389), S(  24, 238),
 S( -21, 156), S( -20,  85), S( -14,  76), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(5, -21);

const Score PASSED_PAWN_KING_PROXIMITY = S(-12, 49);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 184, 545), S(  22, 293), S(  14, 107), S(  21,  29),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(54, -249);

const Score PASSED_PAWN_SQ_RULE = S(0, 833);

const Score PASSED_PAWN_UNSUPPORTED = S(-43, -11);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(-6, 172);

const Score KNIGHT_THREATS[6] = { S(-1, 47), S(-6, 106), S(72, 84), S(177, 27), S(145, -103), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 45), S(47, 83), S(0, 0), S(148, 46), S(128, 66), S(207, 3032),};

const Score ROOK_THREATS[6] = { S(-1, 49), S(65, 97), S(69, 118), S(14, 49), S(100, -70), S(406, 2220),};

const Score KING_THREAT = S(27, 70);

const Score PAWN_THREAT = S(173, 88);

const Score PAWN_PUSH_THREAT = S(37, 41);

const Score PAWN_PUSH_THREAT_PINNED = S(110, 272);

const Score HANGING_THREAT = S(23, 45);

const Score KNIGHT_CHECK_QUEEN = S(27, 0);

const Score BISHOP_CHECK_QUEEN = S(46, 47);

const Score ROOK_CHECK_QUEEN = S(41, 13);

const Score SPACE = 197;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(19, 60), S(0, 0),},
 { S(11, 49), S(-6, -46), S(0, 0),},
 { S(13, 81), S(-54, -4), S(-17, 6), S(0, 0),},
 { S(101, 108), S(-118, 105), S(-65, 194), S(-498, 379), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-31, -11), S(34, 220), S(-17, 98), S(-31, -3), S(22, -27), S(94, -73), S(78, -114), S(0, 0),},
 { S(-85, 21), S(-69, 173), S(-76, 109), S(-65, 55), S(-52, 30), S(57, -33), S(77, -60), S(0, 0),},
 { S(-61, 9), S(-14, 189), S(-97, 93), S(-51, 36), S(-49, 21), S(-24, 9), S(47, -21), S(0, 0),},
 { S(-94, 20), S(99, 114), S(-96, 48), S(-81, 37), S(-80, 25), S(-51, 7), S(-70, 28), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-40, -14), S(-40, 1), S(-39, -8), S(-53, 14), S(-110, 74), S(125, 161), S(559, 165), S(0, 0),},
 { S(-23, 0), S(15, -11), S(16, -7), S(-18, 22), S(-47, 44), S(-42, 144), S(175, 258), S(0, 0),},
 { S(57, -7), S(59, -16), S(50, -5), S(26, 10), S(-14, 28), S(-83, 114), S(139, 176), S(0, 0),},
 { S(5, -4), S(13, -21), S(33, -17), S(9, -24), S(-27, 5), S(-44, 99), S(-144, 275), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-27, -80), S(85, -116), S(42, -72), S(40, -68), S(22, -61), S(12, -199), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(81, -50);

const Score COMPLEXITY_PAWNS = 4;

const Score COMPLEXITY_PAWNS_BOTH_SIDES = 150;

const Score COMPLEXITY_OFFSET = -197;

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
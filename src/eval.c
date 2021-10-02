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

const Score BISHOP_PAIR = S(42, 185);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 187, 410), S( 219, 504), S( 158, 485), S( 227, 423),
 S( 132, 332), S(  93, 348), S( 126, 256), S( 146, 205),
 S(  94, 272), S( 110, 245), S(  82, 222), S(  78, 189),
 S(  76, 211), S(  62, 206), S(  69, 183), S(  78, 157),
 S(  70, 199), S(  55, 181), S(  68, 176), S(  67, 182),
 S(  67, 198), S(  94, 192), S( 105, 186), S(  95, 180),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 280, 287), S( 114, 336), S( 424, 310), S( 219, 438),
 S( 164, 217), S( 169, 179), S( 215, 158), S( 157, 172),
 S(  77, 199), S( 125, 187), S(  94, 176), S( 120, 159),
 S(  46, 179), S(  67, 178), S(  69, 167), S(  91, 145),
 S(  19, 184), S(  58, 160), S(  68, 174), S(  84, 182),
 S(  15, 193), S( 105, 180), S( 103, 190), S(  85, 198),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-319, 181), S(-356, 252), S(-300, 276), S(-216, 231),
 S(-181, 258), S(-146, 276), S( -93, 249), S( -84, 254),
 S( -58, 214), S( -78, 243), S( -84, 266), S(-107, 274),
 S( -83, 275), S( -74, 267), S( -50, 267), S( -70, 276),
 S( -84, 263), S( -74, 262), S( -47, 282), S( -60, 285),
 S(-126, 221), S(-106, 221), S( -87, 235), S( -92, 268),
 S(-149, 232), S(-116, 216), S(-106, 231), S( -94, 227),
 S(-195, 242), S(-116, 226), S(-115, 225), S( -96, 246),
},{
 S(-415,-139), S(-488, 174), S(-234, 160), S( -80, 217),
 S(-126, 123), S(  18, 176), S( -58, 239), S( -98, 245),
 S( -29, 156), S(-108, 208), S( -10, 158), S( -42, 233),
 S( -60, 233), S( -58, 230), S( -41, 245), S( -61, 272),
 S( -72, 261), S( -92, 250), S( -36, 270), S( -70, 305),
 S( -85, 211), S( -55, 207), S( -74, 223), S( -64, 261),
 S( -88, 221), S( -64, 212), S( -93, 217), S( -93, 222),
 S(-108, 239), S(-130, 231), S( -96, 215), S( -81, 243),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -61, 324), S(-110, 362), S( -92, 332), S(-134, 369),
 S( -26, 320), S( -51, 290), S(  26, 310), S(  -9, 342),
 S(  11, 333), S(  16, 328), S( -41, 294), S(  22, 316),
 S(   2, 330), S(  52, 317), S(  29, 327), S(  11, 360),
 S(  43, 303), S(  20, 327), S(  44, 324), S(  62, 318),
 S(  36, 277), S(  74, 309), S(  44, 275), S(  50, 306),
 S(  46, 271), S(  43, 214), S(  71, 259), S(  35, 294),
 S(  31, 224), S(  71, 265), S(  45, 303), S(  51, 299),
},{
 S(-140, 238), S(  64, 291), S(-101, 306), S(-159, 316),
 S( -14, 219), S( -80, 289), S( -33, 323), S(  24, 295),
 S(  55, 301), S(  -5, 313), S(  -5, 299), S(  17, 318),
 S(  -5, 302), S(  55, 306), S(  59, 317), S(  50, 319),
 S(  75, 255), S(  57, 300), S(  64, 316), S(  53, 318),
 S(  62, 258), S(  92, 284), S(  60, 274), S(  49, 327),
 S(  66, 216), S(  66, 218), S(  64, 283), S(  53, 294),
 S(  63, 177), S(  53, 314), S(  30, 309), S(  65, 295),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-295, 548), S(-287, 564), S(-276, 568), S(-338, 574),
 S(-267, 531), S(-285, 557), S(-248, 554), S(-214, 516),
 S(-305, 554), S(-239, 542), S(-260, 544), S(-251, 528),
 S(-286, 573), S(-266, 569), S(-234, 559), S(-227, 528),
 S(-287, 554), S(-284, 566), S(-265, 550), S(-254, 536),
 S(-296, 536), S(-274, 510), S(-263, 515), S(-264, 514),
 S(-285, 496), S(-277, 510), S(-249, 502), S(-242, 488),
 S(-289, 513), S(-273, 500), S(-269, 511), S(-254, 486),
},{
 S(-182, 522), S(-344, 603), S(-404, 599), S(-281, 553),
 S(-230, 488), S(-213, 514), S(-240, 524), S(-268, 526),
 S(-273, 498), S(-163, 492), S(-231, 482), S(-188, 481),
 S(-265, 517), S(-203, 514), S(-219, 500), S(-210, 508),
 S(-288, 499), S(-256, 507), S(-258, 503), S(-234, 516),
 S(-269, 452), S(-230, 446), S(-242, 464), S(-238, 486),
 S(-257, 444), S(-208, 426), S(-238, 447), S(-231, 467),
 S(-279, 468), S(-239, 461), S(-257, 467), S(-234, 465),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-159, 812), S(   8, 615), S(  78, 628), S(  68, 676),
 S(  -7, 662), S(  45, 587), S(  63, 660), S(  63, 655),
 S(  54, 613), S(  70, 595), S(  66, 640), S( 101, 648),
 S(  47, 657), S(  58, 696), S(  73, 687), S(  51, 698),
 S(  62, 614), S(  35, 713), S(  43, 686), S(  30, 735),
 S(  47, 576), S(  52, 606), S(  46, 648), S(  35, 653),
 S(  51, 509), S(  48, 540), S(  60, 549), S(  56, 582),
 S(  21, 558), S(  22, 559), S(  27, 566), S(  32, 576),
},{
 S( 143, 582), S( 249, 562), S(-121, 795), S( 106, 588),
 S( 115, 563), S(  82, 560), S(  87, 620), S(  50, 689),
 S( 120, 558), S(  98, 527), S( 126, 587), S(  97, 642),
 S(  52, 643), S(  72, 653), S( 112, 588), S(  70, 696),
 S(  70, 584), S(  64, 634), S(  77, 639), S(  42, 726),
 S(  61, 514), S(  77, 563), S(  77, 614), S(  46, 655),
 S(  90, 439), S(  93, 441), S(  80, 495), S(  67, 575),
 S(  73, 499), S(  10, 555), S(  23, 518), S(  41, 544),
}};

const Score KING_PSQT[2][32] = {{
 S( 495,-413), S( 231,-140), S(  41,-108), S( 241,-187),
 S(-222, 109), S(-261, 240), S(-114, 154), S( -15,  76),
 S(  -4,  60), S(-132, 198), S(  -8, 146), S( -94, 158),
 S(  19,  56), S( -42, 128), S( -69, 111), S( -55,  96),
 S( -46,  25), S(   4,  88), S(  45,  46), S( -73,  64),
 S(  -7, -41), S(   5,  26), S(  13,  16), S( -18,  28),
 S(  45, -89), S( -21,  24), S( -23,  -2), S( -29,  -7),
 S(  48,-204), S(  56,-113), S(  14,-133), S( -88,-110),
},{
 S(-128,-311), S(-491,  65), S( -25,-148), S( -52,-157),
 S(-430,  37), S(-213, 223), S(-322, 161), S( 316,  14),
 S(-204,  53), S(  40, 203), S( 273, 121), S(  69, 145),
 S(-169,  30), S(  -5, 125), S(  14, 118), S(  -4, 109),
 S(-169,  21), S(-114,  87), S(  30,  50), S(  -9,  68),
 S(  -2, -27), S(  28,  44), S(  10,  32), S(  22,  40),
 S(  42, -30), S(  14,  54), S( -10,  29), S( -42,  23),
 S(  36,-124), S(  35, -34), S( -12, -73), S( -71, -79),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S(-102,  30), S(  12,  42), S(  57,  55), S( 123,  88),
 S(  32,  -7), S(  77,  48), S(  59,  59), S(  82,  84),
 S(  42,   2), S(  71,  25), S(  45,  36), S(  40,  46),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -7,  14), S(  49,   1), S( 116,  27), S( 126,  15),
 S(   7,  -7), S(  47,  27), S(  86,   9), S( 103,  18),
 S(  -8,  41), S(  87,  17), S(  62,  26), S(  70,  41),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-252, -42), S(-168, 123), S(-130, 203), S(-105, 233),
 S( -87, 259), S( -71, 286), S( -55, 294), S( -39, 300),
 S( -24, 284),};

const Score BISHOP_MOBILITIES[14] = {
 S( -45, 137), S(   8, 204), S(  35, 251), S(  49, 295),
 S(  62, 313), S(  70, 335), S(  73, 351), S(  80, 359),
 S(  80, 369), S(  88, 373), S( 101, 367), S( 122, 361),
 S( 122, 371), S( 143, 344),};

const Score ROOK_MOBILITIES[15] = {
 S(-255, -88), S(-282, 378), S(-252, 451), S(-241, 452),
 S(-246, 495), S(-236, 509), S(-246, 527), S(-238, 526),
 S(-231, 538), S(-221, 549), S(-219, 559), S(-228, 576),
 S(-224, 583), S(-214, 595), S(-197, 582),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2263,-2396), S(-140,-378), S( -40, 599), S(   3, 880),
 S(  26, 963), S(  40, 979), S(  40,1045), S(  44,1087),
 S(  50,1119), S(  57,1129), S(  63,1139), S(  71,1139),
 S(  71,1151), S(  80,1150), S(  80,1154), S(  95,1139),
 S(  93,1142), S(  89,1149), S( 100,1115), S( 137,1056),
 S( 168, 990), S( 188, 922), S( 247, 864), S( 298, 707),
 S( 267, 695), S( 263, 601), S(   7, 594), S( 794, -76),
};

const Score KING_MOBILITIES[9] = {
 S(   9, -33), S(   6,  38), S(   1,  47),
 S(  -5,  47), S(  -4,  30), S(  -7,   7),
 S( -11,   1), S(  -4, -21), S(   5, -84),
};

const Score MINOR_BEHIND_PAWN = S(10, 29);

const Score KNIGHT_OUTPOST_REACHABLE = S(20, 42);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 13);

const Score BISHOP_TRAPPED = S(-225, -536);

const Score ROOK_TRAPPED = S(-78, -56);

const Score BAD_BISHOP_PAWNS = S(-2, -9);

const Score DRAGON_BISHOP = S(44, 38);

const Score ROOK_OPEN_FILE_OFFSET = S(29, 14);

const Score ROOK_OPEN_FILE = S(57, 34);

const Score ROOK_SEMI_OPEN = S(34, 15);

const Score ROOK_TO_OPEN = S(30, 40);

const Score QUEEN_OPPOSITE_ROOK = S(-29, 7);

const Score QUEEN_ROOK_BATTERY = S(7, 108);

const Score DEFENDED_PAWN = S(23, 16);

const Score DOUBLED_PAWN = S(30, -79);

const Score ISOLATED_PAWN[4] = {
 S(  -1, -30), S(  -4, -32), S( -14, -18), S(   0, -18),
};

const Score OPEN_ISOLATED_PAWN = S(-7, -23);

const Score BACKWARDS_PAWN = S(-17, -35);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(120, 22), S(-3, 60), S(3, 22), S(3, 0), S(5, -1), S(8, -7), S(0, 0),},
 { S(0, 0), S(151, 58), S(36, 88), S(12, 27), S(19, 5), S(13, -5), S(-1, 2), S(0, 0),},
 { S(0, 0), S(148, 146), S(52, 82), S(29, 30), S(13, 8), S(10, 4), S(0, -9), S(0, 0),},
 { S(0, 0), S(118, 160), S(51, 92), S(19, 37), S(12, 16), S(11, 8), S(9, 4), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 371, 311), S(  20, 157),
 S( -27, 140), S( -45, 101), S( -60,  54), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(9, -34);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 262, 466), S( 117, 406), S(  29, 246),
 S( -21, 166), S( -20,  89), S( -14,  77), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(2, -22);

const Score PASSED_PAWN_KING_PROXIMITY = S(-14, 51);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 174, 565), S(  18, 295), S(  14, 102), S(  23,  21),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(57, -250);

const Score PASSED_PAWN_SQ_RULE = S(0, 808);

const Score PASSED_PAWN_UNSUPPORTED = S(-47, -10);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(-39, 177);

const Score KNIGHT_THREATS[6] = { S(-1, 45), S(-7, 98), S(73, 80), S(175, 26), S(145, -105), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 43), S(46, 80), S(0, 0), S(146, 47), S(128, 62), S(206, 2985),};

const Score ROOK_THREATS[6] = { S(-1, 48), S(65, 92), S(69, 114), S(16, 44), S(101, -74), S(407, 2174),};

const Score KING_THREAT = S(26, 69);

const Score PAWN_THREAT = S(174, 85);

const Score PAWN_PUSH_THREAT = S(37, 40);

const Score PAWN_PUSH_THREAT_PINNED = S(107, 267);

const Score HANGING_THREAT = S(23, 45);

const Score KNIGHT_CHECK_QUEEN = S(27, -2);

const Score BISHOP_CHECK_QUEEN = S(46, 44);

const Score ROOK_CHECK_QUEEN = S(42, 10);

const Score SPACE = 228;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(16, 55), S(0, 0),},
 { S(8, 44), S(-7, -43), S(0, 0),},
 { S(6, 75), S(-52, -3), S(-16, 8), S(0, 0),},
 { S(93, 98), S(-116, 105), S(-64, 190), S(-500, 367), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-38, -17), S(64, 188), S(-20, 97), S(-28, -3), S(21, -32), S(88, -76), S(75, -120), S(0, 0),},
 { S(-88, 15), S(-39, 158), S(-68, 105), S(-62, 51), S(-52, 24), S(52, -39), S(71, -67), S(0, 0),},
 { S(-58, 8), S(-14, 211), S(-97, 97), S(-47, 38), S(-45, 19), S(-23, 9), S(42, -22), S(0, 0),},
 { S(-83, 31), S(67, 139), S(-111, 72), S(-74, 47), S(-69, 35), S(-40, 15), S(-51, 38), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-31, -16), S(-39, -2), S(-39, -10), S(-55, 9), S(-108, 64), S(111, 154), S(581, 164), S(0, 0),},
 { S(-13, 4), S(15, -9), S(17, -6), S(-22, 19), S(-54, 45), S(-55, 161), S(193, 281), S(0, 0),},
 { S(58, -7), S(58, -15), S(47, -7), S(20, 6), S(-21, 28), S(-70, 153), S(170, 195), S(0, 0),},
 { S(-4, -7), S(7, -24), S(26, -22), S(6, -26), S(-32, 3), S(-43, 96), S(-122, 277), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-22, -73), S(83, -110), S(42, -76), S(37, -68), S(18, -61), S(15, -185), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(80, -48);

const Score COMPLEXITY_PAWNS = 5;

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
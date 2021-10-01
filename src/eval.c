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

const Score BISHOP_PAIR = S(39, 179);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 205, 402), S( 180, 471), S( 119, 451), S( 191, 385),
 S( 103, 303), S(  63, 335), S(  87, 230), S(  99, 177),
 S(  99, 267), S(  95, 235), S(  58, 206), S(  53, 176),
 S(  88, 213), S(  69, 215), S(  67, 187), S(  83, 174),
 S(  70, 195), S(  71, 200), S(  67, 187), S(  70, 197),
 S(  82, 202), S(  99, 206), S(  96, 199), S( 101, 191),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 297, 304), S( 108, 321), S( 377, 280), S( 162, 403),
 S( 135, 195), S( 152, 176), S( 178, 125), S( 114, 155),
 S(  77, 196), S( 106, 186), S(  69, 161), S( 102, 158),
 S(  54, 179), S(  76, 187), S(  69, 169), S( 103, 162),
 S(  14, 174), S(  77, 176), S(  72, 180), S(  89, 191),
 S(  25, 194), S( 111, 191), S(  93, 195), S(  96, 216),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-311, 177), S(-338, 238), S(-288, 271), S(-210, 229),
 S(-174, 252), S(-139, 265), S( -90, 239), S( -81, 243),
 S( -52, 204), S( -71, 231), S( -80, 253), S(-104, 264),
 S( -82, 267), S( -71, 256), S( -49, 257), S( -70, 267),
 S( -81, 253), S( -71, 250), S( -43, 269), S( -59, 273),
 S(-121, 212), S(-103, 211), S( -84, 225), S( -90, 257),
 S(-145, 218), S(-111, 205), S(-103, 221), S( -91, 216),
 S(-187, 223), S(-114, 214), S(-115, 213), S( -97, 236),
},{
 S(-400,-141), S(-497, 184), S(-214, 149), S( -78, 211),
 S(-120, 126), S(  12, 177), S( -57, 231), S(-103, 243),
 S( -29, 157), S(-112, 203), S( -12, 155), S( -39, 222),
 S( -63, 229), S( -55, 224), S( -44, 241), S( -60, 262),
 S( -71, 252), S( -90, 240), S( -38, 264), S( -71, 292),
 S( -83, 203), S( -53, 196), S( -72, 212), S( -61, 250),
 S( -86, 209), S( -63, 202), S( -90, 207), S( -91, 211),
 S(-106, 229), S(-126, 218), S( -93, 204), S( -79, 231),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -61, 315), S( -95, 344), S( -74, 314), S(-119, 352),
 S( -19, 310), S( -44, 277), S(  33, 294), S(  -6, 331),
 S(  18, 317), S(  23, 314), S( -37, 280), S(  25, 301),
 S(   5, 310), S(  53, 304), S(  30, 313), S(  13, 346),
 S(  45, 287), S(  23, 312), S(  43, 309), S(  60, 303),
 S(  36, 268), S(  72, 296), S(  42, 261), S(  49, 294),
 S(  45, 261), S(  43, 205), S(  71, 245), S(  36, 280),
 S(  30, 216), S(  69, 252), S(  44, 290), S(  53, 283),
},{
 S(-127, 228), S(  78, 279), S( -89, 293), S(-149, 306),
 S(  -9, 221), S( -73, 282), S( -32, 316), S(  26, 283),
 S(  58, 294), S(  -2, 303), S(  -1, 291), S(  20, 305),
 S(  -3, 293), S(  55, 297), S(  60, 308), S(  53, 304),
 S(  75, 244), S(  59, 289), S(  65, 303), S(  52, 307),
 S(  64, 247), S(  94, 272), S(  60, 262), S(  49, 314),
 S(  66, 208), S(  66, 208), S(  66, 269), S(  55, 281),
 S(  65, 171), S(  51, 308), S(  33, 297), S(  65, 280),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-286, 521), S(-278, 536), S(-269, 540), S(-328, 544),
 S(-259, 505), S(-273, 529), S(-241, 528), S(-207, 490),
 S(-294, 527), S(-228, 516), S(-249, 518), S(-248, 506),
 S(-275, 542), S(-250, 541), S(-224, 532), S(-218, 502),
 S(-277, 528), S(-270, 534), S(-255, 522), S(-245, 509),
 S(-285, 507), S(-265, 482), S(-255, 488), S(-256, 488),
 S(-275, 470), S(-266, 482), S(-242, 476), S(-236, 463),
 S(-279, 489), S(-263, 475), S(-260, 485), S(-245, 461),
},{
 S(-161, 492), S(-313, 569), S(-395, 573), S(-277, 529),
 S(-215, 466), S(-205, 496), S(-234, 499), S(-260, 499),
 S(-265, 482), S(-165, 475), S(-221, 467), S(-176, 457),
 S(-255, 496), S(-192, 494), S(-212, 478), S(-202, 485),
 S(-282, 478), S(-249, 489), S(-252, 483), S(-225, 490),
 S(-259, 427), S(-224, 426), S(-238, 442), S(-231, 462),
 S(-247, 425), S(-203, 406), S(-233, 426), S(-224, 443),
 S(-270, 448), S(-231, 440), S(-248, 443), S(-228, 441),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-159, 781), S(  -2, 594), S(  77, 596), S(  53, 652),
 S( -11, 647), S(  40, 569), S(  58, 638), S(  49, 641),
 S(  46, 597), S(  62, 580), S(  59, 623), S(  92, 624),
 S(  40, 636), S(  53, 670), S(  64, 668), S(  46, 670),
 S(  53, 598), S(  27, 691), S(  36, 665), S(  23, 709),
 S(  38, 568), S(  43, 595), S(  38, 628), S(  28, 632),
 S(  45, 499), S(  40, 527), S(  52, 531), S(  50, 560),
 S(  16, 544), S(  17, 539), S(  20, 551), S(  26, 558),
},{
 S( 131, 572), S( 239, 551), S(-139, 781), S(  99, 553),
 S( 110, 543), S(  84, 545), S(  74, 602), S(  39, 669),
 S( 110, 540), S(  84, 506), S( 112, 567), S(  88, 617),
 S(  43, 623), S(  65, 632), S( 103, 564), S(  61, 671),
 S(  61, 566), S(  56, 613), S(  71, 612), S(  33, 702),
 S(  53, 497), S(  68, 541), S(  68, 592), S(  39, 631),
 S(  86, 428), S(  84, 426), S(  72, 472), S(  59, 554),
 S(  71, 472), S(   9, 531), S(  17, 501), S(  34, 523),
}};

const Score KING_PSQT[2][32] = {{
 S( 531,-396), S( 184,-147), S(  31,-110), S( 287,-148),
 S(-246, 105), S(-286, 232), S(-127, 153), S(   4,  81),
 S( -35,  74), S(-127, 198), S( -13, 152), S( -97, 172),
 S(   6,  52), S( -31, 138), S( -81, 127), S( -69, 119),
 S( -51,  13), S(   1,  76), S(  60,  43), S( -60,  64),
 S( -31, -49), S(   2,  15), S(  -2,   1), S( -37,  10),
 S(  23, -83), S( -37,  22), S( -41, -16), S( -52, -15),
 S(  37,-201), S(  45,-107), S(  26,-117), S( -75, -96),
},{
 S(-133,-282), S(-445,  58), S( -39,-100), S(  11,-119),
 S(-428,  31), S(-194, 209), S(-261, 181), S( 322,  31),
 S(-197,  48), S(  56, 203), S( 298, 146), S(  81, 185),
 S(-146,  20), S(  22, 136), S(  48, 146), S(  23, 138),
 S(-137,  -4), S( -82,  82), S(  32,  67), S( -12,  87),
 S( -10, -37), S(  38,  35), S(  15,  26), S(  21,  34),
 S(  25, -34), S(  12,  49), S( -14,  20), S( -45,  17),
 S(  31,-131), S(  32, -36), S(   9, -60), S( -55, -63),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S(-101,  27), S(  11,  40), S(  55,  58), S( 117,  82),
 S(  29,  -4), S(  74,  44), S(  56,  55), S(  78,  78),
 S(  40,   1), S(  65,  23), S(  37,  31), S(  40,  42),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -13,  12), S(  45,   4), S( 113,  27), S( 119,  16),
 S(   6,   1), S(  47,  25), S(  81,   8), S( 100,  22),
 S(  -8,  38), S(  82,  14), S(  60,  25), S(  76,  42),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-241, -45), S(-162, 110), S(-126, 188), S(-102, 216),
 S( -85, 242), S( -71, 268), S( -56, 275), S( -41, 281),
 S( -24, 265),};

const Score BISHOP_MOBILITIES[14] = {
 S( -34, 132), S(  12, 192), S(  37, 234), S(  50, 276),
 S(  62, 294), S(  70, 315), S(  73, 331), S(  80, 338),
 S(  79, 348), S(  85, 351), S(  99, 345), S( 118, 341),
 S( 121, 349), S( 148, 318),};

const Score ROOK_MOBILITIES[15] = {
 S(-238, -98), S(-271, 362), S(-242, 428), S(-230, 430),
 S(-238, 469), S(-229, 481), S(-240, 498), S(-231, 498),
 S(-225, 510), S(-216, 520), S(-213, 529), S(-222, 545),
 S(-216, 551), S(-205, 562), S(-195, 556),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2195,-2363), S(-141,-372), S( -45, 588), S(  -5, 862),
 S(  18, 939), S(  32, 958), S(  31,1021), S(  35,1063),
 S(  40,1096), S(  46,1104), S(  52,1114), S(  60,1115),
 S(  60,1127), S(  68,1126), S(  68,1128), S(  82,1113),
 S(  80,1116), S(  77,1121), S(  87,1089), S( 121,1032),
 S( 153, 965), S( 172, 895), S( 232, 835), S( 301, 666),
 S( 267, 649), S( 299, 536), S(   9, 545), S( 705, -40),
};

const Score KING_MOBILITIES[9] = {
 S(   7, -25), S(   5,  40), S(   1,  46),
 S(  -4,  45), S(  -3,  28), S(  -4,   5),
 S(  -8,  -1), S(   0, -23), S(   4, -85),
};

const Score MINOR_BEHIND_PAWN = S(11, 27);

const Score KNIGHT_OUTPOST_REACHABLE = S(19, 40);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 13);

const Score BISHOP_TRAPPED = S(-216, -510);

const Score ROOK_TRAPPED = S(-77, -53);

const Score BAD_BISHOP_PAWNS = S(-2, -8);

const Score DRAGON_BISHOP = S(43, 36);

const Score ROOK_OPEN_FILE_OFFSET = S(29, 14);

const Score ROOK_OPEN_FILE = S(53, 33);

const Score ROOK_SEMI_OPEN = S(32, 13);

const Score ROOK_TO_OPEN = S(31, 35);

const Score QUEEN_OPPOSITE_ROOK = S(-28, 5);

const Score QUEEN_ROOK_BATTERY = S(8, 104);

const Score DEFENDED_PAWN = S(23, 18);

const Score DOUBLED_PAWN = S(34, -74);

const Score ISOLATED_PAWN[4] = {
 S(   1, -21), S(   0, -30), S( -11, -13), S(   3, -16),
};

const Score OPEN_ISOLATED_PAWN = S(-10, -20);

const Score BACKWARDS_PAWN = S(-17, -32);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(140, 34), S(-2, 58), S(0, 17), S(0, -1), S(5, 2), S(8, -5), S(0, 0),},
 { S(0, 0), S(142, 65), S(32, 81), S(11, 25), S(19, 5), S(12, -1), S(-1, 2), S(0, 0),},
 { S(0, 0), S(176, 143), S(63, 82), S(26, 25), S(11, 7), S(10, 6), S(3, -5), S(0, 0),},
 { S(0, 0), S(124, 167), S(55, 93), S(19, 35), S(11, 16), S(11, 9), S(11, 8), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 367, 313), S(  34, 168),
 S( -22, 142), S( -40,  96), S( -69,  49), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(7, -34);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 217, 423), S(  97, 388), S(  34, 240),
 S( -20, 162), S( -23,  87), S( -18,  76), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(0, -22);

const Score PASSED_PAWN_KING_PROXIMITY = S(-12, 49);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 202, 545), S(  24, 292), S(  11, 101), S(  22,  20),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(53, -238);

const Score PASSED_PAWN_SQ_RULE = S(0, 774);

const Score PASSED_PAWN_UNSUPPORTED = S(-49, -9);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(4, 171);

const Score KNIGHT_THREATS[6] = { S(-1, 41), S(-4, 80), S(69, 76), S(168, 24), S(138, -97), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 40), S(44, 76), S(0, 0), S(141, 41), S(121, 65), S(186, 2925),};

const Score ROOK_THREATS[6] = { S(-1, 45), S(61, 89), S(66, 108), S(15, 43), S(96, -72), S(396, 2118),};

const Score KING_THREAT = S(25, 70);

const Score PAWN_THREAT = S(166, 81);

const Score PAWN_PUSH_THREAT = S(35, 41);

const Score PAWN_PUSH_THREAT_PINNED = S(104, 256);

const Score HANGING_THREAT = S(22, 42);

const Score KNIGHT_CHECK_QUEEN = S(26, 0);

const Score BISHOP_CHECK_QUEEN = S(43, 46);

const Score ROOK_CHECK_QUEEN = S(39, 13);

const Score SPACE = 218;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(10, 46), S(0, 0),},
 { S(3, 37), S(-8, -44), S(0, 0),},
 { S(-1, 64), S(-50, -7), S(-13, 3), S(0, 0),},
 { S(81, 76), S(-109, 83), S(-59, 171), S(-473, 337), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-37, -23), S(75, 185), S(6, 90), S(-25, -1), S(23, -27), S(84, -74), S(72, -119), S(0, 0),},
 { S(-87, 9), S(-10, 164), S(-38, 111), S(-62, 52), S(-54, 22), S(51, -35), S(66, -63), S(0, 0),},
 { S(-59, 3), S(-16, 209), S(-90, 105), S(-43, 44), S(-42, 18), S(-21, 7), S(41, -16), S(0, 0),},
 { S(-77, 30), S(75, 129), S(-120, 68), S(-63, 54), S(-59, 39), S(-34, 15), S(-55, 36), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-32, -20), S(-41, 0), S(-42, -11), S(-51, 12), S(-112, 67), S(125, 168), S(564, 163), S(0, 0),},
 { S(-19, 4), S(12, -9), S(12, -6), S(-17, 23), S(-48, 46), S(-52, 164), S(206, 275), S(0, 0),},
 { S(53, -5), S(56, -17), S(51, -6), S(22, 9), S(-18, 29), S(-72, 144), S(208, 183), S(0, 0),},
 { S(1, -5), S(13, -23), S(37, -20), S(14, -24), S(-28, 4), S(-39, 96), S(-98, 265), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-19, -81), S(76, -107), S(45, -71), S(40, -63), S(17, -52), S(29, -175), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(82, -35);

const Score COMPLEXITY_PAWNS = 6;

const Score COMPLEXITY_PAWNS_OFFSET = 3;

const Score COMPLEXITY_PAWNS_BOTH_SIDES = 139;

const Score COMPLEXITY_OFFSET = -208;

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

const Score TEMPO = 73;
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
  int pawnsOffset = bits(0xFFULL & (FileFill(board->pieces[PAWN_WHITE]) ^ FileFill(board->pieces[PAWN_BLACK])));

  Score complexity = pawns * COMPLEXITY_PAWNS                       //
                     + pawnsOffset * COMPLEXITY_PAWNS_OFFSET        //
                     + pawnsBothSides * COMPLEXITY_PAWNS_BOTH_SIDES //
                     + COMPLEXITY_OFFSET;

  if (T) {
    C.complexPawns = pawns;
    C.complexPawnsOffset = pawnsOffset;
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
      TTPawnPut(board->pawnHash, pawnS, data.passedPawns, thread);
      s += pawnS;
    }
  } else {
    s += PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);
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
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
const Score MATERIAL_VALUES[7] = { S(200, 200), S(650, 650), S(650, 650), S(1100, 1100), S(2000, 2000), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(38, 177);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 194, 405), S( 179, 463), S( 109, 454), S( 186, 381),
 S(  85, 301), S(  42, 336), S(  70, 226), S(  81, 175),
 S(  81, 262), S(  77, 232), S(  42, 200), S(  37, 171),
 S(  71, 208), S(  51, 212), S(  51, 182), S(  66, 170),
 S(  53, 190), S(  53, 196), S(  50, 181), S(  53, 191),
 S(  64, 198), S(  81, 202), S(  79, 193), S(  84, 185),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 298, 295), S(  99, 319), S( 341, 286), S( 168, 386),
 S( 119, 189), S( 131, 177), S( 162, 118), S(  97, 153),
 S(  62, 189), S(  90, 179), S(  54, 152), S(  87, 152),
 S(  39, 172), S(  60, 181), S(  54, 159), S(  87, 156),
 S(  -2, 168), S(  61, 170), S(  58, 169), S(  73, 183),
 S(   9, 188), S(  95, 185), S(  77, 186), S(  79, 208),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-298, 175), S(-332, 239), S(-276, 267), S(-190, 221),
 S(-159, 244), S(-126, 259), S( -78, 237), S( -65, 235),
 S( -42, 203), S( -60, 229), S( -70, 253), S( -92, 260),
 S( -69, 262), S( -59, 254), S( -36, 252), S( -57, 262),
 S( -69, 249), S( -57, 242), S( -31, 266), S( -47, 269),
 S(-109, 209), S( -90, 208), S( -71, 221), S( -78, 254),
 S(-133, 215), S( -98, 200), S( -90, 216), S( -79, 213),
 S(-179, 230), S(-102, 211), S(-102, 209), S( -85, 233),
},{
 S(-387,-144), S(-447, 160), S(-205, 148), S( -59, 202),
 S(-110, 123), S(  21, 175), S( -44, 222), S( -89, 237),
 S( -17, 154), S(-101, 203), S(   2, 150), S( -28, 222),
 S( -49, 223), S( -43, 222), S( -31, 236), S( -47, 256),
 S( -58, 247), S( -77, 234), S( -25, 258), S( -58, 287),
 S( -70, 200), S( -40, 192), S( -59, 209), S( -49, 247),
 S( -74, 203), S( -51, 197), S( -77, 202), S( -78, 207),
 S( -93, 222), S(-114, 216), S( -81, 200), S( -67, 228),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -62, 305), S(-102, 340), S( -76, 305), S(-128, 352),
 S( -26, 305), S( -49, 272), S(  26, 290), S( -11, 323),
 S(  10, 312), S(  16, 310), S( -43, 274), S(  18, 296),
 S(  -2, 307), S(  47, 298), S(  23, 307), S(   7, 339),
 S(  39, 278), S(  16, 307), S(  36, 303), S(  54, 298),
 S(  30, 263), S(  65, 291), S(  35, 256), S(  43, 288),
 S(  39, 255), S(  36, 201), S(  65, 238), S(  29, 275),
 S(  24, 211), S(  62, 248), S(  37, 286), S(  46, 278),
},{
 S(-137, 227), S(  90, 262), S( -95, 287), S(-153, 299),
 S( -17, 214), S( -80, 276), S( -36, 305), S(  17, 280),
 S(  51, 288), S(  -9, 298), S(  -7, 285), S(  13, 301),
 S(  -9, 288), S(  49, 292), S(  54, 301), S(  47, 299),
 S(  68, 240), S(  53, 283), S(  58, 299), S(  45, 301),
 S(  58, 241), S(  87, 266), S(  53, 256), S(  42, 308),
 S(  61, 201), S(  59, 203), S(  60, 262), S(  48, 275),
 S(  58, 169), S(  42, 309), S(  26, 292), S(  59, 273),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-225, 501), S(-216, 515), S(-220, 527), S(-276, 530),
 S(-198, 486), S(-212, 509), S(-178, 506), S(-147, 469),
 S(-234, 506), S(-168, 496), S(-186, 495), S(-185, 483),
 S(-218, 525), S(-189, 518), S(-164, 512), S(-159, 482),
 S(-218, 509), S(-211, 514), S(-196, 504), S(-185, 489),
 S(-226, 489), S(-205, 463), S(-196, 469), S(-196, 468),
 S(-215, 453), S(-207, 465), S(-181, 456), S(-175, 443),
 S(-219, 469), S(-203, 456), S(-200, 466), S(-185, 441),
},{
 S( -90, 466), S(-238, 540), S(-349, 558), S(-224, 514),
 S(-163, 451), S(-145, 476), S(-181, 484), S(-201, 480),
 S(-205, 461), S(-112, 460), S(-161, 446), S(-115, 436),
 S(-195, 475), S(-130, 470), S(-150, 455), S(-142, 466),
 S(-223, 460), S(-190, 470), S(-191, 462), S(-165, 470),
 S(-199, 408), S(-165, 409), S(-177, 422), S(-170, 441),
 S(-188, 408), S(-143, 389), S(-173, 406), S(-164, 424),
 S(-210, 428), S(-171, 421), S(-188, 423), S(-168, 422),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-194, 787), S( -23, 584), S(  48, 595), S(  13, 664),
 S( -41, 649), S(  11, 570), S(  29, 638), S(  20, 642),
 S(  17, 598), S(  36, 575), S(  30, 623), S(  61, 627),
 S(  12, 634), S(  22, 674), S(  35, 669), S(  14, 675),
 S(  24, 599), S(  -3, 691), S(   6, 668), S(  -7, 710),
 S(   9, 569), S(  14, 594), S(   8, 629), S(  -1, 631),
 S(  17, 498), S(  12, 526), S(  23, 533), S(  21, 560),
 S( -13, 544), S( -13, 540), S(  -9, 551), S(  -3, 558),
},{
 S(  98, 578), S( 205, 552), S(-205, 813), S(  42, 583),
 S(  83, 535), S(  57, 540), S(  47, 598), S(   7, 675),
 S(  83, 535), S(  57, 501), S(  87, 558), S(  60, 615),
 S(  14, 620), S(  37, 629), S(  76, 559), S(  32, 671),
 S(  32, 564), S(  27, 613), S(  42, 613), S(   2, 702),
 S(  25, 495), S(  39, 541), S(  39, 592), S(  10, 629),
 S(  58, 429), S(  56, 426), S(  43, 474), S(  30, 554),
 S(  47, 460), S( -19, 529), S( -10, 494), S(   5, 524),
}};

const Score KING_PSQT[2][32] = {{
 S( 546,-384), S( 306,-183), S(   4, -90), S( 264,-135),
 S(-220, 101), S(-177, 195), S(-102, 142), S(   3,  79),
 S( -17,  74), S( -89, 187), S(  28, 136), S( -89, 166),
 S(   6,  57), S(  -4, 131), S( -67, 123), S( -69, 116),
 S( -47,  16), S(  22,  70), S(  69,  39), S( -63,  64),
 S( -37, -43), S(  -1,  18), S(  -3,   0), S( -34,   9),
 S(  20, -78), S( -39,  24), S( -40, -18), S( -52, -17),
 S(  35,-196), S(  44,-104), S(  25,-114), S( -76, -94),
},{
 S( -91,-282), S(-540,  90), S( -12, -93), S( -38, -96),
 S(-409,  28), S(-272, 228), S(-266, 182), S( 304,  39),
 S(-176,  44), S(  68, 198), S( 293, 145), S( 115, 163),
 S(-127,  14), S(  29, 131), S(  34, 143), S(  13, 135),
 S(-142,  -3), S( -93,  83), S(  25,  66), S( -17,  86),
 S(  -9, -38), S(  39,  35), S(  13,  24), S(  20,  32),
 S(  24, -31), S(  12,  48), S( -13,  17), S( -44,  13),
 S(  30,-129), S(  32, -34), S(   8, -60), S( -55, -63),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S(-100,  24), S(  12,  38), S(  57,  53), S( 118,  79),
 S(  28,  -4), S(  74,  43), S(  56,  56), S(  78,  77),
 S(  40,   2), S(  63,  24), S(  37,  32), S(  39,  45),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -13,  13), S(  45,   4), S( 113,  26), S( 122,  11),
 S(   7,  -2), S(  47,  25), S(  81,   8), S( 100,  22),
 S( -10,  43), S(  83,  14), S(  59,  27), S(  76,  42),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-227, -47), S(-148, 108), S(-113, 185), S( -89, 213),
 S( -72, 238), S( -58, 263), S( -43, 271), S( -27, 276),
 S( -11, 262),};

const Score BISHOP_MOBILITIES[14] = {
 S( -43, 128), S(   3, 187), S(  28, 229), S(  41, 271),
 S(  53, 288), S(  61, 309), S(  64, 325), S(  71, 333),
 S(  70, 341), S(  76, 344), S(  90, 339), S( 110, 333),
 S( 114, 343), S( 142, 309),};

const Score ROOK_MOBILITIES[15] = {
 S(-193,-103), S(-221, 340), S(-193, 406), S(-181, 408),
 S(-189, 447), S(-180, 458), S(-191, 474), S(-182, 475),
 S(-176, 486), S(-167, 496), S(-164, 506), S(-173, 519),
 S(-167, 526), S(-155, 537), S(-141, 527),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1992,-2162), S(-198,-335), S(-100, 605), S( -59, 867),
 S( -36, 948), S( -23, 967), S( -23,1029), S( -20,1069),
 S( -15,1100), S(  -8,1108), S(  -2,1119), S(   5,1119),
 S(   5,1131), S(  14,1130), S(  13,1134), S(  28,1117),
 S(  26,1121), S(  22,1126), S(  36,1087), S(  76,1021),
 S( 103, 962), S( 126, 889), S( 188, 828), S( 289, 628),
 S( 245, 626), S( 352, 460), S( 216, 350), S( 480,  97),
};

const Score KING_MOBILITIES[9] = {
 S(  10, -29), S(   7,  38), S(   3,  44),
 S(  -2,  43), S(  -1,  26), S(  -2,   5),
 S(  -7,  -1), S(  -2, -20), S( -36, -72),
};

const Score MINOR_BEHIND_PAWN = S(10, 27);

const Score KNIGHT_OUTPOST_REACHABLE = S(19, 40);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 13);

const Score BISHOP_TRAPPED = S(-219, -498);

const Score ROOK_TRAPPED = S(-76, -53);

const Score BAD_BISHOP_PAWNS = S(-2, -8);

const Score DRAGON_BISHOP = S(43, 36);

const Score ROOK_OPEN_FILE_OFFSET = S(25, 16);

const Score ROOK_OPEN_FILE = S(53, 32);

const Score ROOK_SEMI_OPEN = S(32, 12);

const Score ROOK_TO_OPEN = S(31, 38);

const Score QUEEN_OPPOSITE_ROOK = S(-28, 5);

const Score QUEEN_ROOK_BATTERY = S(7, 106);

const Score DEFENDED_PAWN = S(23, 18);

const Score DOUBLED_PAWN = S(34, -72);

const Score ISOLATED_PAWN[4] = {
 S(   1, -20), S(   0, -30), S( -11, -12), S(   3, -17),
};

const Score OPEN_ISOLATED_PAWN = S(-10, -19);

const Score BACKWARDS_PAWN = S(-17, -32);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(132, 36), S(-2, 55), S(1, 16), S(0, -1), S(5, 2), S(8, -5), S(0, 0),},
 { S(0, 0), S(141, 66), S(33, 79), S(11, 24), S(19, 4), S(12, -1), S(-1, 2), S(0, 0),},
 { S(0, 0), S(178, 138), S(63, 79), S(26, 25), S(12, 7), S(10, 7), S(3, -5), S(0, 0),},
 { S(0, 0), S(123, 168), S(55, 92), S(19, 34), S(11, 16), S(11, 9), S(11, 8), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 440, 238), S(  32, 171),
 S( -22, 141), S( -40,  94), S( -69,  49), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(7, -34);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 208, 417), S(  99, 384), S(  34, 240),
 S( -20, 163), S( -23,  87), S( -19,  78), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(0, -22);

const Score PASSED_PAWN_KING_PROXIMITY = S(-12, 49);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 199, 543), S(  24, 291), S(  11, 100), S(  22,  20),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(56, -240);

const Score PASSED_PAWN_SQ_RULE = S(0, 764);

const Score PASSED_PAWN_UNSUPPORTED = S(-49, -9);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(153, 157);

const Score KNIGHT_THREATS[6] = { S(-1, 40), S(-4, 80), S(69, 75), S(169, 23), S(140, -102), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 40), S(43, 75), S(0, 0), S(140, 42), S(121, 66), S(189, 2894),};

const Score ROOK_THREATS[6] = { S(-1, 45), S(61, 88), S(65, 110), S(15, 43), S(100, -83), S(396, 2116),};

const Score KING_THREAT = S(21, 71);

const Score PAWN_THREAT = S(166, 81);

const Score PAWN_PUSH_THREAT = S(35, 41);

const Score PAWN_PUSH_THREAT_PINNED = S(105, 250);

const Score HANGING_THREAT = S(22, 43);

const Score KNIGHT_CHECK_QUEEN = S(25, 1);

const Score BISHOP_CHECK_QUEEN = S(43, 46);

const Score ROOK_CHECK_QUEEN = S(39, 14);

const Score SPACE = 214;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(9, 43), S(0, 0),},
 { S(3, 34), S(-5, -40), S(0, 0),},
 { S(-7, 63), S(-56, -6), S(-25, -1), S(0, 0),},
 { S(80, 67), S(-86, 78), S(-50, 156), S(-436, 355), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-37, -24), S(57, 200), S(2, 93), S(-26, 0), S(22, -27), S(84, -73), S(72, -119), S(0, 0),},
 { S(-87, 9), S(-29, 188), S(-38, 112), S(-63, 54), S(-54, 21), S(51, -34), S(66, -63), S(0, 0),},
 { S(-59, 3), S(-35, 232), S(-90, 105), S(-43, 44), S(-42, 18), S(-21, 7), S(41, -15), S(0, 0),},
 { S(-77, 28), S(48, 158), S(-118, 65), S(-63, 52), S(-59, 37), S(-35, 14), S(-55, 33), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-31, -23), S(-41, -2), S(-41, -13), S(-51, 10), S(-112, 66), S(128, 158), S(575, 149), S(0, 0),},
 { S(-18, 3), S(12, -9), S(12, -6), S(-17, 22), S(-48, 44), S(-55, 165), S(204, 273), S(0, 0),},
 { S(52, -2), S(55, -14), S(51, -5), S(22, 9), S(-19, 32), S(-74, 142), S(175, 197), S(0, 0),},
 { S(2, -4), S(14, -23), S(37, -19), S(14, -25), S(-29, 3), S(-37, 92), S(-78, 248), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(1, -103), S(78, -109), S(46, -72), S(40, -61), S(17, -50), S(27, -171), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(82, -36);

const Score COMPLEXITY_PAWNS = 12;

const Score COMPLEXITY_PAWNS_OFFSET = 10;

const Score COMPLEXITY_OFFSET = -144;

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

const Score TEMPO = 74;
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
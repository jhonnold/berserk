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

const Score BISHOP_PAIR = S(38, 177);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 193, 406), S( 180, 463), S( 109, 453), S( 188, 380),
 S(  85, 301), S(  42, 337), S(  70, 227), S(  81, 175),
 S(  82, 262), S(  78, 231), S(  42, 200), S(  38, 171),
 S(  71, 208), S(  51, 212), S(  51, 182), S(  66, 170),
 S(  53, 190), S(  54, 196), S(  51, 181), S(  54, 191),
 S(  64, 198), S(  81, 202), S(  80, 193), S(  84, 184),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 299, 296), S(  97, 321), S( 339, 288), S( 170, 384),
 S( 120, 189), S( 131, 178), S( 161, 119), S(  97, 154),
 S(  63, 188), S(  90, 179), S(  55, 152), S(  87, 152),
 S(  39, 172), S(  60, 182), S(  55, 159), S(  88, 156),
 S(  -1, 168), S(  61, 170), S(  59, 169), S(  74, 183),
 S(   9, 188), S(  96, 185), S(  78, 187), S(  80, 208),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-300, 176), S(-331, 237), S(-275, 267), S(-192, 222),
 S(-160, 245), S(-127, 260), S( -79, 238), S( -67, 236),
 S( -43, 204), S( -61, 230), S( -71, 253), S( -93, 260),
 S( -70, 263), S( -60, 254), S( -37, 252), S( -58, 263),
 S( -70, 249), S( -58, 243), S( -32, 266), S( -48, 269),
 S(-110, 210), S( -91, 208), S( -72, 222), S( -79, 254),
 S(-134, 215), S( -99, 201), S( -91, 217), S( -80, 213),
 S(-180, 231), S(-103, 211), S(-103, 209), S( -86, 233),
},{
 S(-389,-141), S(-450, 161), S(-204, 146), S( -63, 205),
 S(-110, 123), S(  21, 174), S( -45, 223), S( -90, 238),
 S( -19, 154), S(-102, 204), S(   1, 151), S( -29, 222),
 S( -50, 223), S( -44, 223), S( -33, 237), S( -48, 257),
 S( -60, 248), S( -78, 236), S( -26, 259), S( -59, 287),
 S( -71, 200), S( -41, 193), S( -60, 209), S( -50, 248),
 S( -74, 203), S( -51, 197), S( -78, 202), S( -79, 208),
 S( -93, 221), S(-115, 217), S( -82, 201), S( -68, 228),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -62, 307), S(-102, 341), S( -77, 306), S(-127, 352),
 S( -25, 306), S( -49, 272), S(  27, 290), S( -10, 323),
 S(  11, 313), S(  17, 310), S( -42, 275), S(  19, 296),
 S(  -2, 307), S(  47, 298), S(  24, 308), S(   8, 339),
 S(  40, 279), S(  16, 308), S(  36, 304), S(  54, 299),
 S(  30, 263), S(  66, 291), S(  36, 256), S(  43, 288),
 S(  40, 256), S(  37, 201), S(  66, 239), S(  30, 275),
 S(  24, 212), S(  62, 249), S(  37, 286), S(  47, 278),
},{
 S(-135, 227), S(  88, 264), S( -93, 287), S(-153, 301),
 S( -16, 215), S( -79, 276), S( -34, 304), S(  19, 280),
 S(  52, 288), S(  -8, 299), S(  -6, 286), S(  14, 301),
 S(  -9, 288), S(  49, 293), S(  54, 302), S(  47, 299),
 S(  69, 240), S(  54, 283), S(  58, 299), S(  46, 301),
 S(  58, 241), S(  88, 267), S(  54, 256), S(  43, 308),
 S(  61, 201), S(  60, 203), S(  60, 263), S(  49, 276),
 S(  59, 169), S(  43, 309), S(  27, 292), S(  60, 273),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-227, 501), S(-219, 515), S(-221, 526), S(-279, 530),
 S(-201, 486), S(-214, 509), S(-181, 507), S(-149, 469),
 S(-237, 507), S(-171, 496), S(-189, 495), S(-188, 483),
 S(-220, 526), S(-191, 518), S(-167, 512), S(-161, 482),
 S(-221, 509), S(-213, 514), S(-198, 504), S(-188, 490),
 S(-229, 489), S(-207, 463), S(-198, 469), S(-199, 469),
 S(-218, 453), S(-209, 464), S(-184, 456), S(-178, 443),
 S(-221, 469), S(-206, 456), S(-202, 466), S(-188, 441),
},{
 S( -94, 467), S(-241, 541), S(-354, 559), S(-229, 516),
 S(-165, 451), S(-148, 476), S(-182, 483), S(-204, 481),
 S(-207, 461), S(-115, 460), S(-163, 446), S(-118, 437),
 S(-197, 475), S(-133, 471), S(-153, 456), S(-144, 466),
 S(-225, 459), S(-192, 470), S(-194, 462), S(-167, 470),
 S(-202, 409), S(-168, 409), S(-180, 423), S(-173, 441),
 S(-191, 409), S(-146, 389), S(-175, 406), S(-167, 424),
 S(-213, 428), S(-173, 421), S(-191, 423), S(-170, 422),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-192, 787), S( -22, 584), S(  49, 595), S(  14, 664),
 S( -39, 646), S(  12, 569), S(  30, 638), S(  20, 644),
 S(  19, 597), S(  37, 575), S(  32, 623), S(  63, 626),
 S(  14, 632), S(  24, 673), S(  36, 670), S(  16, 674),
 S(  25, 598), S(  -1, 690), S(   7, 668), S(  -5, 710),
 S(  10, 569), S(  16, 593), S(   9, 628), S(   0, 631),
 S(  19, 496), S(  13, 525), S(  24, 533), S(  22, 560),
 S( -12, 544), S( -11, 539), S(  -7, 550), S(  -2, 558),
},{
 S(  99, 578), S( 206, 552), S(-206, 816), S(  46, 580),
 S(  85, 534), S(  58, 541), S(  51, 594), S(   8, 675),
 S(  84, 536), S(  59, 499), S(  89, 558), S(  61, 616),
 S(  16, 620), S(  39, 627), S(  78, 557), S(  33, 671),
 S(  33, 565), S(  28, 613), S(  43, 612), S(   4, 701),
 S(  27, 495), S(  40, 542), S(  40, 591), S(  11, 629),
 S(  59, 428), S(  57, 424), S(  45, 474), S(  31, 554),
 S(  49, 459), S( -18, 529), S(  -9, 494), S(   7, 523),
}};

const Score KING_PSQT[2][32] = {{
 S( 560,-390), S( 319,-189), S(   9, -92), S( 266,-137),
 S(-217,  99), S(-190, 199), S(-103, 143), S(   3,  80),
 S( -10,  71), S( -93, 189), S(  26, 137), S( -92, 167),
 S(   5,  57), S(  -7, 132), S( -68, 123), S( -69, 116),
 S( -45,  15), S(  23,  69), S(  71,  39), S( -62,  63),
 S( -36, -44), S(  -2,  19), S(  -3,   1), S( -34,   9),
 S(  20, -78), S( -39,  24), S( -41, -17), S( -53, -16),
 S(  36,-196), S(  44,-104), S(  25,-114), S( -76, -94),
},{
 S(-106,-280), S(-545,  90), S( -27, -92), S( -30, -99),
 S(-408,  27), S(-272, 229), S(-277, 185), S( 307,  37),
 S(-175,  44), S(  72, 197), S( 293, 144), S( 113, 163),
 S(-126,  14), S(  30, 131), S(  33, 144), S(  15, 134),
 S(-141,  -4), S( -93,  83), S(  26,  66), S( -15,  85),
 S(  -8, -39), S(  40,  34), S(  14,  24), S(  21,  32),
 S(  24, -32), S(  12,  49), S( -14,  18), S( -45,  14),
 S(  31,-130), S(  32, -34), S(   9, -60), S( -55, -63),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S(-100,  25), S(  12,  38), S(  57,  54), S( 118,  79),
 S(  28,  -4), S(  74,  43), S(  55,  56), S(  78,  78),
 S(  39,   2), S(  63,  24), S(  37,  33), S(  39,  45),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -13,  13), S(  45,   4), S( 113,  26), S( 121,  12),
 S(   7,  -2), S(  47,  24), S(  81,   8), S(  99,  22),
 S( -10,  44), S(  83,  14), S(  59,  27), S(  76,  42),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-228, -47), S(-149, 109), S(-114, 185), S( -90, 213),
 S( -73, 239), S( -59, 264), S( -44, 271), S( -28, 277),
 S( -12, 262),};

const Score BISHOP_MOBILITIES[14] = {
 S( -42, 129), S(   4, 188), S(  29, 229), S(  42, 271),
 S(  54, 288), S(  62, 310), S(  65, 325), S(  72, 333),
 S(  71, 342), S(  77, 344), S(  91, 339), S( 111, 334),
 S( 114, 344), S( 144, 310),};

const Score ROOK_MOBILITIES[15] = {
 S(-196,-100), S(-223, 340), S(-195, 406), S(-183, 408),
 S(-191, 447), S(-182, 458), S(-193, 474), S(-184, 475),
 S(-178, 486), S(-169, 496), S(-166, 506), S(-175, 520),
 S(-169, 527), S(-157, 538), S(-143, 528),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2018,-2187), S(-196,-335), S( -97, 605), S( -57, 871),
 S( -33, 948), S( -20, 967), S( -21,1029), S( -17,1068),
 S( -12,1100), S(  -5,1108), S(   0,1118), S(   8,1119),
 S(   8,1130), S(  17,1129), S(  16,1133), S(  31,1116),
 S(  28,1120), S(  25,1125), S(  39,1086), S(  78,1021),
 S( 105, 963), S( 128, 889), S( 190, 829), S( 293, 626),
 S( 247, 626), S( 348, 464), S( 216, 353), S( 506,  79),
};

const Score KING_MOBILITIES[9] = {
 S(   7, -25), S(   5,  40), S(   1,  46),
 S(  -3,  45), S(  -2,  27), S(  -4,   5),
 S(  -8,  -1), S(  -3, -20), S( -10, -82),
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

const Score QUEEN_OPPOSITE_ROOK = S(-28, 6);

const Score QUEEN_ROOK_BATTERY = S(6, 107);

const Score DEFENDED_PAWN = S(23, 18);

const Score DOUBLED_PAWN = S(34, -72);

const Score ISOLATED_PAWN[4] = {
 S(   1, -20), S(   0, -30), S( -11, -12), S(   3, -17),
};

const Score OPEN_ISOLATED_PAWN = S(-9, -19);

const Score BACKWARDS_PAWN = S(-17, -32);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(134, 34), S(-1, 55), S(1, 16), S(0, -1), S(5, 2), S(8, -5), S(0, 0),},
 { S(0, 0), S(143, 65), S(33, 78), S(11, 24), S(19, 4), S(12, -1), S(-1, 2), S(0, 0),},
 { S(0, 0), S(176, 140), S(63, 79), S(26, 25), S(12, 7), S(10, 7), S(3, -5), S(0, 0),},
 { S(0, 0), S(121, 169), S(55, 92), S(19, 34), S(11, 16), S(11, 10), S(11, 8), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 441, 237), S(  33, 171),
 S( -22, 141), S( -40,  93), S( -69,  49), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(7, -33);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 208, 417), S(  99, 384), S(  34, 240),
 S( -20, 163), S( -23,  87), S( -19,  77), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(0, -22);

const Score PASSED_PAWN_KING_PROXIMITY = S(-12, 49);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 200, 543), S(  24, 290), S(  10, 100), S(  23,  19),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(56, -240);

const Score PASSED_PAWN_SQ_RULE = S(0, 766);

const Score PASSED_PAWN_UNSUPPORTED = S(-49, -9);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(135, 159);

const Score KNIGHT_THREATS[6] = { S(-1, 40), S(-4, 79), S(69, 75), S(169, 23), S(141, -103), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 40), S(43, 75), S(0, 0), S(140, 42), S(121, 67), S(190, 2894),};

const Score ROOK_THREATS[6] = { S(-1, 45), S(61, 88), S(65, 110), S(15, 43), S(100, -82), S(395, 2117),};

const Score KING_THREAT = S(21, 71);

const Score PAWN_THREAT = S(166, 81);

const Score PAWN_PUSH_THREAT = S(35, 41);

const Score PAWN_PUSH_THREAT_PINNED = S(105, 251);

const Score HANGING_THREAT = S(22, 43);

const Score KNIGHT_CHECK_QUEEN = S(25, 1);

const Score BISHOP_CHECK_QUEEN = S(43, 46);

const Score ROOK_CHECK_QUEEN = S(39, 13);

const Score SPACE = 214;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(9, 43), S(0, 0),},
 { S(3, 34), S(-5, -41), S(0, 0),},
 { S(-7, 63), S(-56, -6), S(-25, -1), S(0, 0),},
 { S(80, 67), S(-87, 78), S(-50, 157), S(-437, 354), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-38, -23), S(58, 199), S(2, 94), S(-26, 0), S(22, -26), S(84, -73), S(72, -119), S(0, 0),},
 { S(-87, 9), S(-29, 188), S(-39, 112), S(-63, 54), S(-54, 22), S(51, -34), S(66, -63), S(0, 0),},
 { S(-59, 3), S(-36, 233), S(-90, 104), S(-43, 44), S(-42, 18), S(-21, 7), S(41, -16), S(0, 0),},
 { S(-77, 29), S(48, 158), S(-119, 65), S(-63, 53), S(-59, 37), S(-35, 14), S(-55, 32), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-31, -23), S(-41, -2), S(-41, -12), S(-51, 10), S(-112, 66), S(128, 158), S(576, 148), S(0, 0),},
 { S(-18, 3), S(12, -9), S(12, -6), S(-17, 22), S(-48, 44), S(-55, 165), S(202, 274), S(0, 0),},
 { S(52, -2), S(55, -14), S(51, -5), S(22, 9), S(-19, 32), S(-74, 142), S(172, 199), S(0, 0),},
 { S(1, -4), S(14, -23), S(37, -20), S(14, -25), S(-29, 3), S(-38, 93), S(-77, 246), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(1, -103), S(78, -109), S(46, -73), S(40, -61), S(17, -50), S(27, -171), S(0, 0), S(0, 0),
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
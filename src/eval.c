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

const Score BISHOP_PAIR = S(38, 176);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 186, 408), S( 173, 465), S( 108, 451), S( 181, 382),
 S(  77, 302), S(  34, 337), S(  64, 224), S(  77, 171),
 S(  74, 263), S(  71, 232), S(  37, 198), S(  32, 170),
 S(  63, 211), S(  44, 213), S(  45, 182), S(  60, 171),
 S(  45, 192), S(  46, 198), S(  43, 183), S(  46, 193),
 S(  57, 199), S(  74, 204), S(  72, 196), S(  76, 186),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 282, 304), S( 100, 313), S( 368, 281), S( 155, 395),
 S( 112, 187), S( 124, 172), S( 154, 117), S(  92, 145),
 S(  56, 187), S(  86, 172), S(  48, 154), S(  82, 149),
 S(  32, 171), S(  55, 178), S(  46, 164), S(  82, 155),
 S(  -8, 167), S(  55, 169), S(  50, 173), S(  67, 184),
 S(   3, 188), S(  90, 186), S(  69, 190), S(  72, 211),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-284, 169), S(-314, 230), S(-255, 252), S(-169, 206),
 S(-146, 240), S(-112, 252), S( -61, 223), S( -52, 224),
 S( -29, 201), S( -46, 223), S( -54, 243), S( -75, 247),
 S( -54, 255), S( -44, 245), S( -20, 241), S( -41, 250),
 S( -54, 243), S( -43, 238), S( -15, 256), S( -31, 260),
 S( -95, 203), S( -76, 203), S( -57, 217), S( -64, 251),
 S(-118, 210), S( -84, 198), S( -75, 212), S( -64, 210),
 S(-165, 226), S( -87, 204), S( -88, 207), S( -70, 229),
},{
 S(-375,-151), S(-422, 143), S(-188, 139), S( -45, 191),
 S( -92, 103), S(  42, 154), S( -29, 213), S( -65, 228),
 S(   0, 137), S( -87, 186), S(  21, 135), S( -13, 207),
 S( -34, 211), S( -28, 208), S( -15, 220), S( -32, 243),
 S( -43, 237), S( -61, 221), S(  -9, 245), S( -42, 274),
 S( -55, 195), S( -26, 187), S( -44, 204), S( -35, 242),
 S( -57, 199), S( -34, 194), S( -61, 201), S( -64, 205),
 S( -77, 215), S( -99, 209), S( -64, 198), S( -53, 225),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -73, 302), S(-116, 335), S( -86, 294), S(-133, 337),
 S( -39, 302), S( -62, 269), S(  15, 281), S( -20, 312),
 S(  -1, 307), S(   2, 308), S( -55, 267), S(   7, 284),
 S( -14, 302), S(  36, 289), S(  12, 296), S(  -3, 331),
 S(  28, 270), S(   5, 296), S(  25, 296), S(  42, 291),
 S(  18, 254), S(  54, 284), S(  23, 251), S(  31, 284),
 S(  28, 248), S(  25, 197), S(  53, 234), S(  18, 271),
 S(  12, 205), S(  50, 240), S(  25, 280), S(  34, 275),
},{
 S(-142, 212), S(  85, 249), S(-104, 278), S(-165, 292),
 S( -25, 207), S( -92, 267), S( -44, 300), S(   7, 270),
 S(  37, 279), S( -21, 283), S( -18, 275), S(   5, 291),
 S( -19, 270), S(  38, 279), S(  44, 286), S(  36, 287),
 S(  57, 230), S(  42, 274), S(  46, 292), S(  33, 294),
 S(  46, 235), S(  76, 261), S(  41, 254), S(  30, 305),
 S(  51, 197), S(  49, 203), S(  50, 263), S(  37, 273),
 S(  47, 165), S(  28, 309), S(  16, 289), S(  47, 270),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-192, 486), S(-188, 502), S(-184, 509), S(-232, 507),
 S(-163, 472), S(-176, 494), S(-145, 494), S(-111, 457),
 S(-198, 493), S(-133, 484), S(-152, 482), S(-151, 468),
 S(-186, 519), S(-158, 512), S(-132, 505), S(-125, 470),
 S(-188, 505), S(-179, 508), S(-166, 500), S(-154, 482),
 S(-195, 484), S(-174, 460), S(-165, 465), S(-163, 459),
 S(-184, 448), S(-175, 461), S(-151, 454), S(-143, 438),
 S(-187, 464), S(-172, 452), S(-168, 463), S(-153, 437),
},{
 S( -72, 457), S(-188, 516), S(-293, 531), S(-190, 499),
 S(-126, 435), S(-102, 463), S(-143, 469), S(-167, 469),
 S(-170, 442), S( -74, 440), S(-122, 421), S( -80, 419),
 S(-162, 463), S( -93, 453), S(-114, 434), S(-106, 450),
 S(-191, 449), S(-154, 455), S(-157, 446), S(-130, 457),
 S(-167, 399), S(-132, 400), S(-144, 410), S(-138, 433),
 S(-156, 402), S(-109, 380), S(-138, 397), S(-132, 417),
 S(-179, 422), S(-138, 414), S(-154, 416), S(-136, 416),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-213, 788), S( -51, 593), S(  18, 603), S(  15, 637),
 S( -57, 647), S(  -6, 568), S(  11, 639), S(   5, 637),
 S(  -1, 601), S(  18, 577), S(  12, 625), S(  44, 623),
 S(  -9, 647), S(   4, 678), S(  17, 667), S(  -4, 679),
 S(   6, 603), S( -20, 691), S( -13, 675), S( -26, 716),
 S( -10, 572), S(  -4, 601), S( -11, 634), S( -20, 636),
 S(   0, 500), S(  -6, 529), S(   5, 537), S(   3, 564),
 S( -31, 548), S( -30, 540), S( -27, 554), S( -21, 562),
},{
 S(  83, 563), S( 199, 528), S(-167, 764), S(  25, 584),
 S(  67, 531), S(  39, 534), S(  41, 595), S(  -8, 667),
 S(  62, 543), S(  33, 513), S(  70, 564), S(  40, 628),
 S(  -2, 612), S(  20, 626), S(  59, 553), S(  16, 668),
 S(  15, 560), S(   9, 613), S(  24, 615), S( -16, 702),
 S(   8, 493), S(  20, 547), S(  21, 595), S(  -9, 634),
 S(  45, 417), S(  40, 429), S(  26, 482), S(  12, 559),
 S(  25, 474), S( -37, 530), S( -26, 496), S( -12, 528),
}};

const Score KING_PSQT[2][32] = {{
 S( 406,-302), S( 258,-147), S(  -8, -65), S( 277,-120),
 S(-244, 126), S(-152, 163), S( -69, 117), S(  29,  60),
 S(  -5,  86), S( -53, 158), S(  21, 128), S( -77, 158),
 S( -14,  82), S(  12, 118), S( -40, 115), S( -80, 121),
 S( -61,  41), S(  38,  63), S(  73,  44), S( -60,  69),
 S( -48, -20), S(   8,  12), S(   7,   0), S( -25,   7),
 S(  19, -67), S( -39,   6), S( -38, -33), S( -48, -34),
 S(  34,-188), S(  43, -97), S(  24,-102), S( -77, -83),
},{
 S( -64,-284), S(-581, 121), S(  71, -97), S( -57, -69),
 S(-393,  34), S(-241, 209), S(-193, 163), S( 327,  30),
 S(-189,  58), S(  39, 204), S( 334, 142), S(  88, 173),
 S(-134,  26), S(  37, 131), S(  36, 152), S(  13, 141),
 S(-152,  17), S( -83,  82), S(  19,  78), S( -21,  95),
 S( -11, -21), S(  45,  31), S(  19,  27), S(  30,  31),
 S(  24, -23), S(  13,  28), S( -12,   1), S( -40,  -3),
 S(  31,-123), S(  31, -27), S(   8, -50), S( -56, -53),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S(-100,  23), S(  12,  39), S(  56,  51), S( 118,  76),
 S(  28,  -6), S(  74,  41), S(  56,  54), S(  78,  74),
 S(  40,  -1), S(  64,  24), S(  37,  32), S(  40,  43),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -13,  16), S(  45,   4), S( 114,  25), S( 120,  12),
 S(   6,  -1), S(  48,  22), S(  81,   8), S( 101,  19),
 S( -10,  43), S(  82,  17), S(  60,  24), S(  76,  41),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-209, -53), S(-131, 101), S( -96, 178), S( -72, 206),
 S( -55, 231), S( -41, 255), S( -26, 262), S( -10, 268),
 S(   7, 253),};

const Score BISHOP_MOBILITIES[14] = {
 S( -56, 121), S( -11, 183), S(  14, 225), S(  27, 266),
 S(  39, 283), S(  46, 304), S(  49, 319), S(  56, 326),
 S(  56, 334), S(  62, 337), S(  77, 330), S(  99, 323),
 S( 102, 331), S( 133, 297),};

const Score ROOK_MOBILITIES[15] = {
 S(-163,-110), S(-188, 330), S(-160, 396), S(-149, 399),
 S(-156, 437), S(-147, 448), S(-158, 464), S(-150, 465),
 S(-143, 477), S(-134, 486), S(-131, 494), S(-139, 507),
 S(-133, 512), S(-121, 522), S(-107, 511),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1882,-2050), S(-233,-322), S(-134, 614), S( -94, 882),
 S( -70, 957), S( -57, 977), S( -58,1037), S( -55,1077),
 S( -49,1108), S( -43,1116), S( -37,1127), S( -30,1127),
 S( -29,1138), S( -21,1136), S( -22,1140), S(  -6,1121),
 S(  -9,1127), S( -12,1129), S(   5,1086), S(  43,1022),
 S(  74, 957), S( 101, 879), S( 169, 811), S( 269, 612),
 S( 239, 597), S( 349, 428), S( 233, 300), S( 343, 143),
};

const Score MINOR_BEHIND_PAWN = S(11, 26);

const Score KNIGHT_OUTPOST_REACHABLE = S(19, 40);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 13);

const Score BISHOP_TRAPPED = S(-218, -506);

const Score ROOK_TRAPPED = S(-75, -55);

const Score BAD_BISHOP_PAWNS = S(-2, -8);

const Score DRAGON_BISHOP = S(44, 35);

const Score ROOK_OPEN_FILE_OFFSET = S(22, 17);

const Score ROOK_OPEN_FILE = S(53, 31);

const Score ROOK_SEMI_OPEN = S(32, 14);

const Score ROOK_TO_OPEN = S(31, 37);

const Score QUEEN_OPPOSITE_ROOK = S(-28, 6);

const Score QUEEN_ROOK_BATTERY = S(5, 110);

const Score DEFENDED_PAWN = S(23, 17);

const Score DOUBLED_PAWN = S(33, -72);

const Score ISOLATED_PAWN[4] = {
 S(   1, -20), S(   0, -30), S( -11, -13), S(   3, -17),
};

const Score OPEN_ISOLATED_PAWN = S(-9, -19);

const Score BACKWARDS_PAWN = S(-17, -32);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(133, 37), S(-1, 56), S(0, 16), S(0, -1), S(6, 2), S(9, -6), S(0, 0),},
 { S(0, 0), S(154, 60), S(36, 74), S(11, 23), S(19, 4), S(12, -1), S(-1, 3), S(0, 0),},
 { S(0, 0), S(182, 137), S(63, 80), S(26, 25), S(12, 7), S(10, 6), S(3, -6), S(0, 0),},
 { S(0, 0), S(137, 157), S(56, 92), S(19, 34), S(11, 16), S(11, 9), S(10, 8), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 422, 273), S(  33, 170),
 S( -22, 141), S( -41,  94), S( -69,  48), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(7, -34);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 204, 419), S(  97, 388), S(  33, 241),
 S( -21, 163), S( -23,  88), S( -20,  79), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(0, -23);

const Score PASSED_PAWN_KING_PROXIMITY = S(-12, 50);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 185, 552), S(  23, 292), S(  10, 101), S(  21,  22),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(55, -238);

const Score PASSED_PAWN_SQ_RULE = S(0, 775);

const Score PASSED_PAWN_UNSUPPORTED = S(-49, -11);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(118, 119);

const Score KNIGHT_THREATS[6] = { S(-1, 40), S(-4, 72), S(69, 75), S(169, 24), S(141, -104), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 40), S(44, 75), S(0, 0), S(141, 42), S(121, 64), S(189, 2884),};

const Score ROOK_THREATS[6] = { S(-2, 47), S(62, 88), S(65, 109), S(14, 45), S(99, -80), S(409, 2098),};

const Score KING_THREAT = S(22, 77);

const Score PAWN_THREAT = S(166, 79);

const Score PAWN_PUSH_THREAT = S(36, 38);

const Score PAWN_PUSH_THREAT_PINNED = S(106, 253);

const Score HANGING_THREAT = S(21, 44);

const Score KNIGHT_CHECK_QUEEN = S(25, 2);

const Score BISHOP_CHECK_QUEEN = S(43, 46);

const Score ROOK_CHECK_QUEEN = S(39, 14);

const Score SPACE = 205;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(9, 43), S(0, 0),},
 { S(2, 34), S(0, -36), S(0, 0),},
 { S(-9, 62), S(-55, -7), S(-35, -10), S(0, 0),},
 { S(80, 65), S(-67, 80), S(-51, 138), S(-407, 359), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-37, -27), S(57, 197), S(2, 91), S(-27, -3), S(20, -29), S(82, -71), S(73, -116), S(0, 0),},
 { S(-86, 5), S(-23, 178), S(-37, 103), S(-63, 49), S(-55, 19), S(50, -33), S(67, -59), S(0, 0),},
 { S(-59, 0), S(-27, 217), S(-87, 98), S(-43, 39), S(-44, 17), S(-23, 9), S(42, -12), S(0, 0),},
 { S(-76, 26), S(54, 147), S(-117, 61), S(-63, 48), S(-59, 35), S(-35, 18), S(-54, 38), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-32, -19), S(-42, 1), S(-42, -9), S(-52, 14), S(-114, 71), S(124, 170), S(563, 153), S(0, 0),},
 { S(-18, 3), S(12, -9), S(12, -6), S(-17, 22), S(-49, 46), S(-58, 176), S(207, 271), S(0, 0),},
 { S(56, -9), S(57, -17), S(52, -8), S(21, 11), S(-20, 35), S(-80, 153), S(192, 202), S(0, 0),},
 { S(2, -6), S(14, -26), S(36, -21), S(13, -24), S(-32, 10), S(-41, 102), S(-80, 262), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(2, -103), S(75, -106), S(46, -73), S(41, -60), S(17, -47), S(27, -172), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(82, -36);

const Score COMPLEXITY_PAWNS = 12;

const Score COMPLEXITY_PAWNS_OFFSET = 10;

const Score COMPLEXITY_OFFSET = -142;

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
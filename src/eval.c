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

const Score BISHOP_PAIR = S(38, 168);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 170, 380), S( 160, 431), S(  98, 418), S( 173, 350),
 S(  59, 274), S(  23, 299), S(  47, 197), S(  58, 148),
 S(  56, 238), S(  54, 206), S(  18, 177), S(  13, 152),
 S(  45, 189), S(  27, 189), S(  26, 161), S(  41, 151),
 S(  28, 171), S(  29, 175), S(  25, 163), S(  28, 173),
 S(  39, 177), S(  56, 181), S(  53, 175), S(  57, 167),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 254, 288), S(  95, 285), S( 370, 255), S( 134, 377),
 S(  92, 169), S( 109, 147), S( 132, 105), S(  74, 123),
 S(  36, 171), S(  67, 153), S(  27, 141), S(  60, 132),
 S(  13, 155), S(  37, 158), S(  28, 147), S(  61, 137),
 S( -26, 151), S(  37, 150), S(  30, 158), S(  46, 166),
 S( -15, 170), S(  70, 166), S(  49, 172), S(  53, 190),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-263, 142), S(-287, 199), S(-236, 220), S(-170, 188),
 S(-131, 211), S(-100, 224), S( -49, 193), S( -43, 197),
 S( -20, 175), S( -33, 191), S( -42, 212), S( -65, 216),
 S( -44, 227), S( -33, 213), S( -11, 211), S( -32, 218),
 S( -44, 214), S( -33, 209), S(  -5, 224), S( -22, 227),
 S( -83, 173), S( -64, 174), S( -46, 188), S( -52, 219),
 S(-105, 177), S( -73, 169), S( -65, 184), S( -53, 180),
 S(-142, 176), S( -76, 176), S( -78, 180), S( -60, 201),
},{
 S(-362,-154), S(-460, 147), S(-172, 114), S( -42, 169),
 S( -84,  87), S(  54, 131), S( -15, 179), S( -53, 197),
 S(   6, 114), S( -77, 158), S(  26, 107), S(  -3, 174),
 S( -28, 184), S( -18, 173), S(  -7, 189), S( -22, 211),
 S( -34, 206), S( -53, 190), S(   0, 212), S( -33, 240),
 S( -44, 164), S( -16, 159), S( -34, 175), S( -25, 211),
 S( -47, 170), S( -25, 168), S( -51, 172), S( -53, 176),
 S( -64, 180), S( -88, 181), S( -53, 169), S( -42, 194),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -93, 287), S(-118, 304), S(-103, 277), S(-144, 311),
 S( -53, 279), S( -79, 250), S(  -3, 258), S( -38, 292),
 S( -18, 284), S( -15, 284), S( -73, 248), S( -13, 264),
 S( -31, 280), S(  16, 267), S(  -7, 273), S( -23, 308),
 S(   8, 252), S( -13, 272), S(   5, 273), S(  23, 267),
 S(  -1, 235), S(  34, 260), S(   4, 231), S(  11, 262),
 S(   8, 230), S(   5, 179), S(  32, 216), S(  -1, 250),
 S(  -8, 191), S(  30, 219), S(   6, 257), S(  14, 254),
},{
 S(-155, 192), S(  46, 239), S(-113, 253), S(-172, 268),
 S( -43, 190), S(-108, 246), S( -63, 281), S( -12, 249),
 S(  18, 254), S( -41, 262), S( -38, 252), S( -14, 268),
 S( -39, 251), S(  18, 257), S(  22, 266), S(  17, 263),
 S(  37, 208), S(  23, 251), S(  27, 267), S(  13, 270),
 S(  26, 215), S(  56, 239), S(  21, 234), S(  10, 282),
 S(  30, 183), S(  29, 185), S(  29, 243), S(  17, 252),
 S(  25, 153), S(  12, 275), S(  -3, 266), S(  27, 250),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-166, 412), S(-163, 428), S(-143, 425), S(-206, 433),
 S(-134, 397), S(-149, 420), S(-119, 421), S( -88, 386),
 S(-169, 418), S(-107, 408), S(-128, 411), S(-126, 398),
 S(-154, 440), S(-129, 437), S(-104, 429), S( -99, 398),
 S(-158, 430), S(-149, 432), S(-136, 424), S(-125, 406),
 S(-166, 412), S(-145, 388), S(-136, 393), S(-135, 388),
 S(-155, 377), S(-146, 388), S(-122, 382), S(-115, 366),
 S(-158, 394), S(-143, 381), S(-139, 391), S(-125, 366),
},{
 S( -58, 387), S(-183, 452), S(-264, 455), S(-153, 417),
 S( -98, 357), S( -80, 389), S(-117, 392), S(-136, 392),
 S(-146, 368), S( -53, 366), S(-101, 350), S( -56, 348),
 S(-137, 391), S( -70, 382), S( -91, 366), S( -79, 376),
 S(-164, 378), S(-129, 382), S(-129, 373), S(-104, 386),
 S(-140, 329), S(-107, 329), S(-117, 340), S(-110, 361),
 S(-129, 331), S( -85, 312), S(-112, 328), S(-105, 346),
 S(-151, 355), S(-112, 344), S(-126, 346), S(-108, 346),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-226, 764), S( -70, 573), S(  22, 554), S( -16, 623),
 S( -77, 629), S( -30, 556), S( -11, 616), S( -18, 616),
 S( -22, 581), S(  -5, 558), S( -13, 609), S(  21, 600),
 S( -30, 626), S( -17, 656), S(  -5, 646), S( -24, 651),
 S( -17, 587), S( -43, 675), S( -34, 652), S( -46, 691),
 S( -33, 559), S( -26, 584), S( -32, 614), S( -42, 620),
 S( -23, 487), S( -29, 517), S( -18, 524), S( -19, 550),
 S( -54, 539), S( -53, 530), S( -49, 540), S( -43, 550),
},{
 S(  60, 536), S( 164, 518), S(-241, 796), S(   6, 556),
 S(  42, 501), S(  12, 516), S(  13, 572), S( -28, 639),
 S(  39, 514), S(   7, 490), S(  42, 538), S(  19, 598),
 S( -25, 591), S(  -2, 599), S(  34, 533), S(  -7, 641),
 S(  -8, 541), S( -13, 593), S(   1, 592), S( -38, 684),
 S( -15, 477), S(  -3, 531), S(  -2, 578), S( -31, 618),
 S(  17, 420), S(  14, 421), S(   3, 468), S( -10, 545),
 S(   2, 462), S( -58, 515), S( -48, 487), S( -34, 517),
}};

const Score KING_PSQT[2][32] = {{
 S( 458,-323), S( 225,-137), S(  26, -76), S( 223, -98),
 S(-266, 131), S(-227, 186), S( -82, 124), S(  19,  65),
 S( -32,  92), S( -93, 167), S( -10, 140), S( -72, 156),
 S( -21,  81), S(  -1, 118), S( -38, 115), S( -51, 114),
 S( -38,  30), S(  30,  64), S(  74,  46), S( -34,  62),
 S( -41, -21), S(  12,  10), S(   8,   5), S( -17,   6),
 S(  19, -66), S( -35,   4), S( -36, -27), S( -45, -31),
 S(  32,-180), S(  41, -93), S(  24, -94), S( -76, -76),
},{
 S( -78,-273), S(-516, 104), S( -34, -74), S( -69, -67),
 S(-401,  38), S(-224, 201), S(-189, 157), S( 292,  37),
 S(-199,  59), S(  67, 189), S( 320, 145), S(  96, 176),
 S(-148,  32), S(  40, 126), S(  59, 147), S(  32, 136),
 S(-134,  14), S( -71,  79), S(  42,  71), S(   1,  86),
 S(  -7, -22), S(  44,  29), S(  29,  25), S(  37,  29),
 S(  23, -24), S(  13,  26), S( -10,   4), S( -37,  -1),
 S(  28,-118), S(  29, -27), S(   7, -43), S( -56, -49),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -96,  22), S(  11,  36), S(  54,  48), S( 116,  71),
 S(  28,  -7), S(  73,  38), S(  56,  49), S(  78,  67),
 S(  41,  -6), S(  64,  20), S(  37,  29), S(  39,  40),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -13,  14), S(  43,   5), S( 111,  24), S( 117,  10),
 S(   7,  -4), S(  47,  22), S(  79,   5), S(  99,  17),
 S(  -9,  39), S(  80,  13), S(  58,  21), S(  74,  37),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-201, -68), S(-123,  76), S( -88, 150), S( -64, 177),
 S( -47, 201), S( -34, 225), S( -19, 230), S(  -4, 235),
 S(  12, 220),};

const Score BISHOP_MOBILITIES[14] = {
 S( -80, 103), S( -36, 163), S( -12, 203), S(   1, 243),
 S(  13, 260), S(  20, 280), S(  23, 294), S(  30, 300),
 S(  29, 307), S(  35, 310), S(  49, 304), S(  69, 298),
 S(  66, 311), S(  87, 283),};

const Score ROOK_MOBILITIES[15] = {
 S(-129,-159), S(-153, 266), S(-126, 331), S(-114, 333),
 S(-121, 370), S(-113, 382), S(-123, 397), S(-115, 397),
 S(-108, 409), S( -99, 417), S( -97, 424), S(-105, 437),
 S(-100, 441), S( -89, 451), S( -79, 443),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1686,-1652), S(-230,-461), S(-141, 495), S( -99, 749),
 S( -77, 827), S( -64, 844), S( -64, 904), S( -61, 945),
 S( -56, 976), S( -50, 983), S( -44, 993), S( -37, 993),
 S( -37,1004), S( -29,1003), S( -29,1004), S( -16, 990),
 S( -17, 988), S( -20, 993), S(  -6, 954), S(  26, 900),
 S(  59, 830), S(  68, 775), S( 145, 691), S( 216, 524),
 S( 158, 526), S( 209, 402), S( -91, 415), S( 684,-212),
};

const Score MINOR_BEHIND_PAWN = S(11, 24);

const Score KNIGHT_OUTPOST_REACHABLE = S(19, 37);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 11);

const Score BISHOP_TRAPPED = S(-219, -472);

const Score ROOK_TRAPPED = S(-74, -53);

const Score BAD_BISHOP_PAWNS = S(-2, -8);

const Score DRAGON_BISHOP = S(43, 32);

const Score ROOK_OPEN_FILE_OFFSET = S(8, 27);

const Score ROOK_OPEN_FILE = S(52, 29);

const Score ROOK_SEMI_OPEN = S(31, 14);

const Score ROOK_TO_OPEN = S(30, 33);

const Score QUEEN_OPPOSITE_ROOK = S(-27, 5);

const Score QUEEN_ROOK_BATTERY = S(7, 101);

const Score DEFENDED_PAWN = S(23, 16);

const Score DOUBLED_PAWN = S(32, -70);

const Score ISOLATED_PAWN[4] = {
 S(   1, -19), S(   0, -29), S( -11, -13), S(   3, -15),
};

const Score OPEN_ISOLATED_PAWN = S(-9, -19);

const Score BACKWARDS_PAWN = S(-17, -30);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(139, 32), S(2, 51), S(1, 16), S(0, -1), S(5, 1), S(8, -6), S(0, 0),},
 { S(0, 0), S(159, 52), S(35, 72), S(11, 23), S(18, 4), S(11, -1), S(-1, 3), S(0, 0),},
 { S(0, 0), S(167, 139), S(62, 76), S(25, 23), S(11, 7), S(10, 6), S(3, -5), S(0, 0),},
 { S(0, 0), S(131, 152), S(54, 87), S(19, 32), S(11, 15), S(11, 9), S(10, 8), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 290, 362), S(  34, 158),
 S( -21, 134), S( -39,  90), S( -67,  44), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(6, -32);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 193, 397), S(  94, 374), S(  30, 233),
 S( -22, 159), S( -24,  85), S( -19,  73), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(2, -24);

const Score PASSED_PAWN_KING_PROXIMITY = S(-11, 47);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 200, 524), S(  25, 277), S(  11,  95), S(  20,  21),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(49, -227);

const Score PASSED_PAWN_SQ_RULE = S(0, 735);

const Score PASSED_PAWN_UNSUPPORTED = S(-48, -9);

const Score KNIGHT_THREATS[6] = { S(-1, 39), S(-4, 73), S(67, 71), S(163, 21), S(137, -103), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 38), S(43, 70), S(0, 0), S(137, 36), S(118, 56), S(191, 2748),};

const Score ROOK_THREATS[6] = { S(-1, 44), S(60, 83), S(63, 103), S(15, 41), S(93, -73), S(393, 2017),};

const Score KING_THREAT = S(29, 69);

const Score PAWN_THREAT = S(163, 73);

const Score PAWN_PUSH_THREAT = S(35, 35);

const Score PAWN_PUSH_THREAT_PINNED = S(104, 241);

const Score HANGING_THREAT = S(21, 39);

const Score KNIGHT_CHECK_QUEEN = S(25, -1);

const Score BISHOP_CHECK_QUEEN = S(43, 41);

const Score ROOK_CHECK_QUEEN = S(38, 12);

const Score SPACE = 211;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(8, 39), S(0, 0),},
 { S(3, 28), S(5, -34), S(0, 0),},
 { S(-14, 59), S(-51, -5), S(-42, -11), S(0, 0),},
 { S(74, 55), S(-55, 76), S(-47, 109), S(-373, 325), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-34, -23), S(71, 177), S(11, 85), S(-25, 0), S(22, -26), S(82, -65), S(72, -107), S(0, 0),},
 { S(-83, 4), S(3, 149), S(-30, 95), S(-60, 44), S(-53, 17), S(49, -33), S(65, -58), S(0, 0),},
 { S(-57, 4), S(0, 185), S(-85, 98), S(-42, 41), S(-43, 20), S(-22, 12), S(41, -10), S(0, 0),},
 { S(-73, 27), S(96, 111), S(-111, 59), S(-61, 48), S(-59, 37), S(-35, 19), S(-53, 39), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-30, -15), S(-39, 5), S(-40, -4), S(-49, 17), S(-108, 68), S(125, 162), S(530, 154), S(0, 0),},
 { S(-17, 4), S(13, -6), S(13, -4), S(-15, 24), S(-46, 47), S(-50, 173), S(208, 257), S(0, 0),},
 { S(56, -11), S(57, -18), S(52, -7), S(24, 10), S(-19, 36), S(-71, 150), S(213, 187), S(0, 0),},
 { S(6, -6), S(15, -23), S(38, -19), S(14, -21), S(-30, 13), S(-36, 99), S(-77, 255), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-20, -76), S(70, -97), S(45, -66), S(41, -57), S(18, -43), S(28, -162), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(80, -36);

const Score COMPLEXITY_PAWNS = 7;

const Score COMPLEXITY_PAWNS_BOTH_SIDES = 128;

const Score COMPLEXITY_OFFSET = -193;

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
    return MAX_SCALE / 2;

  int ssPawns = bits(board->pieces[PAWN[ss]]);
  return min(MAX_SCALE, MAX_SCALE / 2 + MAX_SCALE / 8 * ssPawns);
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

  BitBoard allPawns = board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK];

  int pawns = bits(allPawns);
  int bothSides = (allPawns & K_SIDE) && (allPawns & Q_SIDE);

  Score complexity = pawns * COMPLEXITY_PAWNS //
                     + bothSides * COMPLEXITY_PAWNS_BOTH_SIDES + COMPLEXITY_OFFSET;

  if (T) {
    C.complexPawns = pawns;
    C.complexPawnsBothSides = bothSides;
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

  s += thread->data.contempt;
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

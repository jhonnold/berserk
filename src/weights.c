// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

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

#include "board.h"
#include "weights.h"

// clang-format off
const int MIRROR[] = {
  56, 57, 58, 59, 60, 61, 62, 63,
  48, 49, 50, 51, 52, 53, 54, 55,
  40, 41, 42, 43, 44, 45, 46, 47,
  32, 33, 34, 35, 36, 37, 38, 39,
  24, 25, 26, 27, 28, 29, 30, 31,
  16, 17, 18, 19, 20, 21, 22, 23,
   8,  9, 10, 11, 12, 13, 14, 15,
   0,  1,  2,  3,  4,  5,  6,  7
};

const Score MATERIAL_VALUES[7] = { S(100, 100), S(325, 325), S(325, 325), S(550, 550), S(1000, 1000), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(21, 103);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 126, 226), S( 112, 264), S(  77, 257), S( 114, 216),
 S( 108, 184), S(  83, 203), S(  97, 146), S( 101, 114),
 S( 105, 163), S( 103, 146), S(  80, 129), S(  76, 114),
 S(  98, 133), S(  89, 134), S(  84, 120), S(  96, 112),
 S(  90, 123), S(  89, 127), S(  89, 119), S(  90, 127),
 S(  96, 128), S( 105, 130), S( 104, 127), S( 106, 121),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 179, 169), S(  68, 183), S( 183, 167), S( 100, 223),
 S( 126, 126), S( 131, 114), S( 147,  86), S( 111, 104),
 S(  93, 123), S( 111, 120), S(  87, 105), S( 105, 103),
 S(  80, 114), S(  92, 120), S(  87, 109), S( 107, 106),
 S(  56, 112), S(  95, 114), S(  92, 114), S(  99, 122),
 S(  63, 122), S( 112, 121), S( 103, 124), S( 104, 136),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-186,  83), S(-201, 115), S(-171, 134), S(-117, 109),
 S(-105, 123), S( -86, 129), S( -57, 117), S( -50, 117),
 S( -38,  99), S( -48, 112), S( -54, 125), S( -64, 131),
 S( -53, 128), S( -46, 125), S( -33, 125), S( -42, 134),
 S( -53, 124), S( -49, 122), S( -31, 134), S( -40, 136),
 S( -77,  98), S( -66, 100), S( -53, 106), S( -58, 125),
 S( -89, 100), S( -70,  97), S( -65, 105), S( -56, 105),
 S(-111, 102), S( -73, 102), S( -70, 104), S( -63, 114),
},{
 S(-227, -93), S(-259,  80), S(-122,  68), S( -48, 100),
 S( -68,  52), S(  -2,  82), S( -34, 112), S( -60, 117),
 S( -16,  73), S( -65,  95), S(  -2,  72), S( -29, 107),
 S( -32, 111), S( -33, 108), S( -24, 117), S( -36, 131),
 S( -44, 123), S( -49, 118), S( -25, 133), S( -42, 146),
 S( -52,  95), S( -38,  91), S( -46,  99), S( -43, 120),
 S( -56,  98), S( -42,  96), S( -57, 100), S( -57, 102),
 S( -67, 106), S( -78, 105), S( -58,  98), S( -52, 112),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -29, 167), S( -44, 182), S( -33, 168), S( -57, 186),
 S(   0, 165), S( -15, 148), S(  27, 155), S(   5, 177),
 S(  19, 168), S(  25, 164), S( -12, 147), S(  25, 157),
 S(  13, 163), S(  43, 159), S(  29, 164), S(  28, 183),
 S(  36, 148), S(  24, 164), S(  39, 160), S(  47, 158),
 S(  32, 140), S(  54, 153), S(  33, 136), S(  39, 154),
 S(  41, 135), S(  36, 105), S(  50, 127), S(  28, 145),
 S(  26, 110), S(  52, 130), S(  37, 153), S(  41, 151),
},{
 S( -62, 117), S(  52, 149), S( -37, 154), S( -74, 163),
 S(  10, 114), S( -28, 146), S(  -4, 166), S(  24, 148),
 S(  49, 153), S(  18, 157), S(  15, 150), S(  26, 161),
 S(  14, 153), S(  41, 155), S(  48, 162), S(  45, 162),
 S(  54, 126), S(  43, 152), S(  48, 162), S(  43, 163),
 S(  49, 131), S(  64, 141), S(  45, 137), S(  41, 167),
 S(  48, 106), S(  49, 106), S(  48, 140), S(  40, 146),
 S(  50,  84), S(  37, 162), S(  31, 157), S(  48, 149),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-129, 294), S(-122, 301), S(-117, 301), S(-150, 303),
 S(-122, 285), S(-131, 299), S(-114, 297), S( -91, 278),
 S(-143, 297), S(-107, 292), S(-118, 292), S(-114, 286),
 S(-131, 306), S(-120, 303), S(-100, 303), S(-105, 281),
 S(-133, 298), S(-129, 300), S(-123, 293), S(-115, 289),
 S(-139, 286), S(-127, 272), S(-122, 275), S(-121, 275),
 S(-132, 263), S(-129, 269), S(-117, 266), S(-112, 259),
 S(-132, 277), S(-124, 268), S(-123, 272), S(-115, 261),
},{
 S( -53, 274), S(-134, 317), S(-179, 317), S(-115, 293),
 S( -85, 262), S( -85, 276), S( -98, 277), S(-121, 278),
 S(-114, 268), S( -56, 264), S( -88, 263), S( -74, 259),
 S(-113, 278), S( -74, 277), S( -91, 272), S( -89, 276),
 S(-131, 269), S(-108, 270), S(-115, 273), S(-102, 278),
 S(-117, 240), S( -91, 238), S(-110, 248), S(-104, 262),
 S(-112, 241), S( -86, 224), S(-107, 239), S(-104, 247),
 S(-128, 251), S(-102, 246), S(-118, 248), S(-105, 247),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -83, 496), S(   5, 400), S(  50, 398), S(  40, 429),
 S(  -4, 429), S(  25, 380), S(  37, 419), S(  29, 427),
 S(  30, 396), S(  41, 382), S(  39, 406), S(  56, 413),
 S(  26, 416), S(  35, 431), S(  43, 429), S(  37, 433),
 S(  35, 389), S(  22, 440), S(  28, 426), S(  19, 451),
 S(  26, 374), S(  31, 392), S(  29, 409), S(  22, 410),
 S(  30, 340), S(  28, 353), S(  35, 355), S(  34, 371),
 S(  14, 361), S(  12, 360), S(  17, 364), S(  20, 368),
},{
 S(  89, 391), S( 157, 371), S( -54, 492), S(  65, 385),
 S(  75, 379), S(  61, 381), S(  55, 401), S(  26, 442),
 S(  78, 371), S(  71, 350), S(  80, 386), S(  63, 411),
 S(  33, 414), S(  49, 420), S(  66, 386), S(  44, 434),
 S(  41, 387), S(  43, 402), S(  47, 404), S(  25, 448),
 S(  38, 348), S(  51, 368), S(  45, 391), S(  27, 406),
 S(  50, 306), S(  53, 299), S(  48, 323), S(  38, 367),
 S(  48, 323), S(   8, 361), S(  14, 341), S(  24, 348),
}};

const Score KING_PSQT[2][32] = {{
 S( 250,-196), S(  78, -65), S(  -4, -46), S( 126, -77),
 S(-136,  67), S(-155, 133), S( -86,  96), S( -31,  49),
 S(  -8,  43), S( -77, 117), S( -24,  96), S( -80,  98),
 S(  -9,  36), S( -25,  85), S( -56,  82), S( -62,  69),
 S( -32,  14), S( -11,  47), S(   9,  35), S( -53,  37),
 S( -20, -20), S(  -4,  17), S( -14,  12), S( -35,   6),
 S(  11, -42), S( -22,  19), S( -27,   0), S( -34,  -9),
 S(  25,-106), S(  26, -55), S(  14, -59), S( -42, -56),
},{
 S( -42,-169), S(-246,  30), S(  -8, -54), S( -29, -72),
 S(-230,  14), S(-105, 116), S(-153, 100), S( 155,   9),
 S(-112,  24), S(  27, 112), S( 139,  89), S(  38,  94),
 S( -86,   9), S(   5,  74), S(   6,  85), S( -14,  72),
 S( -82,  -5), S( -55,  45), S(   2,  41), S( -26,  41),
 S(  -5, -21), S(  19,  20), S(  -2,  19), S(   0,  13),
 S(  15, -23), S(   8,  22), S( -12,  10), S( -28,  -2),
 S(  23, -75), S(  21, -23), S(   4, -35), S( -31, -49),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -54,  15), S(   5,  22), S(  32,  34), S(  67,  45),
 S(  15,  -1), S(  42,  22), S(  33,  32), S(  47,  45),
 S(  24,  -4), S(  37,  13), S(  21,  18), S(  22,  23),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -10,   4), S(  26,   3), S(  66,  16), S(  68,   8),
 S(   4,  -1), S(  25,  14), S(  46,   5), S(  57,  12),
 S(  -5,  20), S(  46,   8), S(  34,  13), S(  44,  24),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-133,  26), S( -87, 115), S( -68, 160), S( -53, 175),
 S( -44, 190), S( -35, 203), S( -26, 209), S( -17, 211),
 S(  -7, 203),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -3, 117), S(  21, 150), S(  39, 177), S(  45, 200),
 S(  54, 209), S(  58, 225), S(  60, 232), S(  63, 236),
 S(  62, 241), S(  68, 242), S(  76, 241), S(  85, 236),
 S(  87, 244), S( 101, 227),};

const Score ROOK_MOBILITIES[15] = {
 S( -90, -29), S(-113, 235), S( -92, 273), S( -88, 274),
 S( -91, 297), S( -85, 305), S( -92, 313), S( -88, 313),
 S( -84, 320), S( -78, 326), S( -75, 330), S( -81, 340),
 S( -76, 344), S( -70, 352), S( -65, 348),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1894,-1387), S( -93,-254), S( -31, 223), S(  -9, 383),
 S(   6, 427), S(  16, 441), S(  13, 477), S(  16, 503),
 S(  19, 521), S(  24, 528), S(  26, 534), S(  33, 536),
 S(  30, 544), S(  36, 543), S(  34, 548), S(  42, 540),
 S(  41, 546), S(  38, 549), S(  46, 538), S(  58, 511),
 S(  71, 478), S(  78, 454), S( 108, 422), S( 126, 354),
 S( 102, 355), S( 161, 267), S(  35, 252), S( 325,   3),
};

const Score KING_MOBILITIES[9] = {
 S(   5, -22), S(   2,  11), S(  -1,  16),
 S(  -1,  15), S(  -1,   8), S(  -2,  -6),
 S(  -1,  -9), S(   2, -21), S(   4, -58),
};

const Score MINOR_BEHIND_PAWN = S(6, 14);

const Score KNIGHT_OUTPOST_REACHABLE = S(11, 22);

const Score BISHOP_OUTPOST_REACHABLE = S(7, 7);

const Score BISHOP_TRAPPED = S(-121, -289);

const Score ROOK_TRAPPED = S(-43, -30);

const Score BAD_BISHOP_PAWNS = S(-1, -5);

const Score DRAGON_BISHOP = S(25, 21);

const Score ROOK_OPEN_FILE_OFFSET = S(11, 11);

const Score ROOK_OPEN_FILE = S(30, 18);

const Score ROOK_SEMI_OPEN = S(18, 7);

const Score ROOK_TO_OPEN = S(18, 23);

const Score QUEEN_OPPOSITE_ROOK = S(-16, 2);

const Score QUEEN_ROOK_BATTERY = S(4, 57);

const Score DEFENDED_PAWN = S(13, 10);

const Score DOUBLED_PAWN = S(19, -42);

const Score ISOLATED_PAWN[4] = {
 S(   1, -12), S(  -1, -17), S(  -6,  -7), S(   2, -10),
};

const Score OPEN_ISOLATED_PAWN = S(-6, -12);

const Score BACKWARDS_PAWN = S(-8, -18);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(70, 20), S(-1, 34), S(0, 9), S(-1, -1), S(3, 2), S(4, -4), S(0, 0),},
 { S(0, 0), S(77, 31), S(19, 43), S(6, 15), S(12, 3), S(8, 0), S(0, 2), S(0, 0),},
 { S(0, 0), S(83, 85), S(35, 48), S(14, 15), S(6, 4), S(6, 4), S(1, -3), S(0, 0),},
 { S(0, 0), S(60, 92), S(32, 54), S(12, 18), S(7, 10), S(6, 6), S(8, 5), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 224, 168), S(  20,  96),
 S( -13,  80), S( -23,  55), S( -38,  27), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -18);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 157, 251), S(  55, 217), S(  19, 133),
 S( -12,  89), S( -15,  45), S( -10,  43), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(-1, -12);

const Score PASSED_PAWN_KING_PROXIMITY = S(-9, 26);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S(  73, 309), S(  10, 163), S(   6,  56), S(  11,  10),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(29, -135);

const Score PASSED_PAWN_SQ_RULE = S(0, 440);

const Score PASSED_PAWN_UNSUPPORTED = S(-28, -5);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(-8, 105);

const Score KNIGHT_THREATS[6] = { S(0, 22), S(-5, 54), S(38, 44), S(94, 16), S(80, -53), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(4, 23), S(26, 42), S(-66, 81), S(78, 25), S(69, 25), S(121, 1596),};

const Score ROOK_THREATS[6] = { S(0, 26), S(34, 49), S(38, 63), S(5, 21), S(56, -50), S(261, 1128),};

const Score KING_THREAT = S(14, 39);

const Score PAWN_THREAT = S(95, 45);

const Score PAWN_PUSH_THREAT = S(21, 23);

const Score PAWN_PUSH_THREAT_PINNED = S(21, 127);

const Score HANGING_THREAT = S(11, 24);

const Score KNIGHT_CHECK_QUEEN = S(15, -1);

const Score BISHOP_CHECK_QUEEN = S(24, 24);

const Score ROOK_CHECK_QUEEN = S(22, 6);

const Score SPACE = 131;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(10, 29), S(0, 0),},
 { S(6, 23), S(-6, -29), S(0, 0),},
 { S(5, 44), S(-34, -10), S(-8, 5), S(0, 0),},
 { S(57, 65), S(-71, 69), S(-28, 128), S(-263, 247), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-25, -3), S(33, 107), S(-5, 62), S(-16, 9), S(9, -6), S(42, -33), S(38, -59), S(0, 0),},
 { S(-51, 1), S(-10, 83), S(-23, 57), S(-36, 24), S(-30, 7), S(29, -23), S(38, -38), S(0, 0),},
 { S(-26, -12), S(-2, 100), S(-42, 46), S(-16, 11), S(-15, -4), S(-5, -10), S(32, -22), S(0, 0),},
 { S(-45, 22), S(19, 80), S(-72, 44), S(-38, 38), S(-32, 29), S(-20, 16), S(-32, 26), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-25, -13), S(-28, -3), S(-26, -7), S(-32, 6), S(-66, 36), S(65, 89), S(318, 89), S(0, 0),},
 { S(-16, 2), S(3, -6), S(3, -5), S(-13, 10), S(-32, 23), S(-33, 91), S(111, 153), S(0, 0),},
 { S(27, -5), S(29, -12), S(25, -5), S(9, 3), S(-12, 16), S(-42, 78), S(71, 113), S(0, 0),},
 { S(-4, -6), S(5, -14), S(18, -13), S(5, -13), S(-19, 1), S(-29, 53), S(-61, 150), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-11, -46), S(43, -58), S(21, -41), S(20, -35), S(6, -31), S(7, -100), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(47, -20);

const Score COMPLEXITY_PAWNS = 2;

const Score COMPLEXITY_PAWNS_BOTH_SIDES = 83;

const Score COMPLEXITY_OFFSET = -106;

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

const Score TEMPO = 43;
// clang-format on

Score PSQT[12][2][64];
Score KNIGHT_POSTS[2][64];
Score BISHOP_POSTS[2][64];

void InitPSQT() {
  for (int sq = 0; sq < 64; sq++) {
    for (int i = 0; i < 2; i++) {
      PSQT[WHITE_PAWN][i][sq] = PSQT[BLACK_PAWN][i][MIRROR[sq]] =
          PAWN_PSQT[i][psqtIdx(sq)] + MATERIAL_VALUES[PAWN];
      PSQT[WHITE_KNIGHT][i][sq] = PSQT[BLACK_KNIGHT][i][MIRROR[sq]] =
          KNIGHT_PSQT[i][psqtIdx(sq)] + MATERIAL_VALUES[KNIGHT];
      PSQT[WHITE_BISHOP][i][sq] = PSQT[BLACK_BISHOP][i][MIRROR[sq]] =
          BISHOP_PSQT[i][psqtIdx(sq)] + MATERIAL_VALUES[BISHOP];
      PSQT[WHITE_ROOK][i][sq] = PSQT[BLACK_ROOK][i][MIRROR[sq]] =
          ROOK_PSQT[i][psqtIdx(sq)] + MATERIAL_VALUES[ROOK];
      PSQT[WHITE_QUEEN][i][sq] = PSQT[BLACK_QUEEN][i][MIRROR[sq]] =
          QUEEN_PSQT[i][psqtIdx(sq)] + MATERIAL_VALUES[QUEEN];
      PSQT[WHITE_KING][i][sq] = PSQT[BLACK_KING][i][MIRROR[sq]] = KING_PSQT[i][psqtIdx(sq)];
    }

    if (sq >= A6 && sq <= H4) {
      KNIGHT_POSTS[WHITE][sq] = KNIGHT_POSTS[BLACK][MIRROR[sq]] = KNIGHT_POST_PSQT[psqtIdx(sq) - 8];
      BISHOP_POSTS[WHITE][sq] = BISHOP_POSTS[BLACK][MIRROR[sq]] = BISHOP_POST_PSQT[psqtIdx(sq) - 8];
    }
  }
}

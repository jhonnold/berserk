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
 S( 205, 404), S( 181, 471), S( 121, 450), S( 192, 384),
 S( 105, 302), S(  63, 335), S(  88, 230), S( 100, 176),
 S(  99, 267), S(  96, 235), S(  59, 206), S(  54, 177),
 S(  89, 213), S(  69, 215), S(  68, 188), S(  83, 175),
 S(  71, 195), S(  72, 201), S(  67, 187), S(  71, 198),
 S(  82, 203), S(  99, 206), S(  97, 199), S( 101, 192),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 297, 305), S( 106, 325), S( 381, 278), S( 163, 403),
 S( 137, 195), S( 152, 176), S( 180, 123), S( 115, 155),
 S(  78, 197), S( 107, 186), S(  70, 160), S( 103, 159),
 S(  54, 179), S(  77, 187), S(  69, 169), S( 104, 163),
 S(  15, 175), S(  77, 177), S(  73, 180), S(  90, 192),
 S(  26, 195), S( 112, 191), S(  93, 196), S(  96, 216),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-311, 177), S(-338, 238), S(-292, 275), S(-208, 228),
 S(-174, 253), S(-139, 265), S( -90, 240), S( -81, 244),
 S( -53, 206), S( -71, 231), S( -81, 254), S(-105, 265),
 S( -82, 267), S( -72, 257), S( -49, 257), S( -70, 267),
 S( -82, 254), S( -71, 250), S( -43, 270), S( -60, 275),
 S(-122, 213), S(-103, 212), S( -84, 226), S( -90, 257),
 S(-145, 220), S(-111, 205), S(-103, 221), S( -91, 217),
 S(-187, 223), S(-114, 214), S(-115, 215), S( -97, 236),
},{
 S(-401,-140), S(-497, 184), S(-215, 151), S( -78, 212),
 S(-120, 125), S(  11, 178), S( -56, 230), S(-102, 242),
 S( -29, 158), S(-112, 204), S( -13, 157), S( -40, 224),
 S( -63, 229), S( -55, 225), S( -44, 241), S( -60, 262),
 S( -71, 252), S( -90, 242), S( -38, 264), S( -71, 292),
 S( -83, 203), S( -53, 197), S( -72, 213), S( -62, 250),
 S( -87, 209), S( -64, 203), S( -90, 207), S( -91, 212),
 S(-106, 229), S(-126, 219), S( -93, 205), S( -79, 231),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -59, 314), S( -95, 344), S( -73, 314), S(-117, 352),
 S( -19, 310), S( -44, 277), S(  34, 294), S(  -7, 333),
 S(  18, 317), S(  24, 313), S( -37, 281), S(  26, 301),
 S(   5, 311), S(  53, 304), S(  30, 314), S(  14, 346),
 S(  45, 288), S(  23, 313), S(  43, 309), S(  61, 304),
 S(  37, 269), S(  72, 296), S(  42, 262), S(  50, 294),
 S(  46, 261), S(  43, 205), S(  71, 245), S(  36, 280),
 S(  31, 217), S(  69, 253), S(  44, 291), S(  53, 284),
},{
 S(-124, 224), S(  76, 281), S( -91, 295), S(-149, 307),
 S(  -9, 221), S( -72, 281), S( -32, 317), S(  26, 284),
 S(  59, 294), S(  -1, 303), S(   0, 291), S(  21, 306),
 S(  -2, 293), S(  56, 298), S(  60, 308), S(  54, 304),
 S(  75, 244), S(  60, 290), S(  65, 304), S(  52, 307),
 S(  64, 248), S(  94, 272), S(  60, 262), S(  49, 314),
 S(  67, 208), S(  66, 208), S(  66, 270), S(  55, 281),
 S(  65, 172), S(  52, 307), S(  33, 297), S(  66, 280),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-288, 523), S(-280, 538), S(-271, 542), S(-329, 546),
 S(-260, 507), S(-274, 531), S(-242, 530), S(-209, 492),
 S(-296, 530), S(-230, 518), S(-250, 520), S(-249, 507),
 S(-276, 544), S(-252, 543), S(-225, 534), S(-220, 504),
 S(-278, 530), S(-271, 535), S(-256, 524), S(-247, 511),
 S(-287, 509), S(-265, 483), S(-257, 491), S(-258, 490),
 S(-276, 472), S(-267, 484), S(-243, 478), S(-237, 465),
 S(-280, 491), S(-264, 478), S(-261, 487), S(-247, 463),
},{
 S(-161, 492), S(-312, 570), S(-394, 574), S(-277, 530),
 S(-216, 467), S(-206, 498), S(-236, 501), S(-261, 501),
 S(-264, 482), S(-166, 476), S(-223, 469), S(-178, 459),
 S(-256, 498), S(-194, 496), S(-213, 481), S(-203, 487),
 S(-284, 482), S(-250, 491), S(-253, 484), S(-226, 491),
 S(-261, 429), S(-226, 428), S(-239, 444), S(-232, 463),
 S(-248, 427), S(-204, 408), S(-235, 428), S(-226, 445),
 S(-271, 450), S(-232, 441), S(-250, 445), S(-229, 443),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-155, 775), S(  -1, 594), S(  77, 598), S(  55, 652),
 S( -11, 649), S(  41, 569), S(  59, 638), S(  55, 634),
 S(  47, 597), S(  64, 579), S(  60, 623), S(  93, 624),
 S(  41, 638), S(  53, 673), S(  66, 668), S(  46, 671),
 S(  54, 597), S(  28, 690), S(  37, 665), S(  24, 709),
 S(  38, 570), S(  44, 595), S(  38, 628), S(  29, 632),
 S(  46, 500), S(  41, 529), S(  53, 533), S(  51, 560),
 S(  17, 544), S(  18, 539), S(  21, 551), S(  27, 558),
},{
 S( 130, 575), S( 236, 556), S(-140, 783), S(  93, 561),
 S( 110, 544), S(  84, 549), S(  75, 601), S(  41, 667),
 S( 111, 540), S(  84, 508), S( 112, 569), S(  87, 623),
 S(  44, 623), S(  67, 631), S( 105, 562), S(  62, 672),
 S(  62, 566), S(  57, 613), S(  72, 613), S(  33, 703),
 S(  55, 495), S(  68, 543), S(  69, 592), S(  40, 631),
 S(  86, 431), S(  85, 425), S(  73, 474), S(  60, 554),
 S(  73, 470), S(  10, 531), S(  18, 502), S(  35, 524),
}};

const Score KING_PSQT[2][32] = {{
 S( 530,-392), S( 190,-147), S(  39,-113), S( 276,-145),
 S(-242, 104), S(-290, 234), S(-126, 153), S(  -1,  83),
 S( -32,  74), S(-127, 197), S( -12, 152), S( -96, 172),
 S(   9,  51), S( -32, 138), S( -75, 125), S( -68, 118),
 S( -51,  14), S(   1,  76), S(  60,  43), S( -58,  63),
 S( -30, -49), S(   3,  14), S(  -2,   1), S( -37,  10),
 S(  23, -82), S( -37,  22), S( -41, -15), S( -52, -15),
 S(  38,-202), S(  45,-107), S(  26,-116), S( -75, -96),
},{
 S(-122,-282), S(-440,  59), S( -35,-100), S(  -1,-115),
 S(-416,  30), S(-189, 208), S(-239, 174), S( 325,  30),
 S(-194,  47), S(  59, 202), S( 302, 146), S(  92, 182),
 S(-150,  22), S(  20, 136), S(  50, 146), S(  21, 138),
 S(-136,  -4), S( -82,  82), S(  32,  67), S( -11,  86),
 S( -10, -38), S(  39,  35), S(  15,  26), S(  22,  33),
 S(  25, -33), S(  12,  49), S( -14,  20), S( -45,  17),
 S(  31,-132), S(  33, -35), S(   9, -60), S( -55, -63),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S(-100,  25), S(  11,  40), S(  55,  58), S( 117,  81),
 S(  28,  -4), S(  75,  42), S(  56,  55), S(  79,  78),
 S(  40,  -1), S(  65,  22), S(  38,  31), S(  40,  42),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -13,  11), S(  43,   6), S( 113,  26), S( 119,  16),
 S(   7,  -1), S(  47,  25), S(  82,   8), S( 100,  22),
 S(  -8,  37), S(  83,  14), S(  60,  25), S(  76,  41),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-241, -44), S(-162, 111), S(-126, 188), S(-102, 217),
 S( -85, 243), S( -71, 269), S( -56, 276), S( -41, 282),
 S( -24, 266),};

const Score BISHOP_MOBILITIES[14] = {
 S( -34, 132), S(  12, 192), S(  37, 234), S(  50, 277),
 S(  63, 294), S(  70, 316), S(  73, 331), S(  80, 339),
 S(  79, 348), S(  85, 351), S(  99, 346), S( 119, 340),
 S( 121, 351), S( 149, 318),};

const Score ROOK_MOBILITIES[15] = {
 S(-239, -96), S(-272, 364), S(-243, 430), S(-231, 432),
 S(-239, 471), S(-230, 484), S(-241, 500), S(-232, 501),
 S(-226, 512), S(-217, 522), S(-214, 532), S(-223, 547),
 S(-217, 553), S(-206, 565), S(-196, 558),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2227,-2391), S(-140,-373), S( -44, 589), S(  -4, 861),
 S(  20, 940), S(  33, 958), S(  32,1022), S(  36,1063),
 S(  41,1096), S(  47,1105), S(  53,1115), S(  61,1115),
 S(  61,1127), S(  69,1127), S(  69,1129), S(  83,1114),
 S(  81,1117), S(  78,1122), S(  88,1089), S( 123,1031),
 S( 154, 967), S( 175, 894), S( 233, 836), S( 300, 669),
 S( 266, 652), S( 298, 540), S(   5, 547), S( 733, -48),
};

const Score KING_MOBILITIES[9] = {
 S(   7, -25), S(   5,  40), S(   1,  46),
 S(  -4,  45), S(  -3,  28), S(  -4,   5),
 S(  -8,  -1), S(   0, -23), S(   5, -85),
};

const Score MINOR_BEHIND_PAWN = S(11, 27);

const Score KNIGHT_OUTPOST_REACHABLE = S(19, 40);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 13);

const Score BISHOP_TRAPPED = S(-216, -511);

const Score ROOK_TRAPPED = S(-76, -54);

const Score BAD_BISHOP_PAWNS = S(-2, -8);

const Score DRAGON_BISHOP = S(43, 36);

const Score ROOK_OPEN_FILE_OFFSET = S(29, 14);

const Score ROOK_OPEN_FILE = S(53, 32);

const Score ROOK_SEMI_OPEN = S(32, 14);

const Score ROOK_TO_OPEN = S(31, 35);

const Score QUEEN_OPPOSITE_ROOK = S(-28, 4);

const Score QUEEN_ROOK_BATTERY = S(7, 106);

const Score DEFENDED_PAWN = S(23, 18);

const Score DOUBLED_PAWN = S(33, -74);

const Score ISOLATED_PAWN[4] = {
 S(   1, -21), S(   0, -30), S( -11, -13), S(   3, -16),
};

const Score OPEN_ISOLATED_PAWN = S(-10, -20);

const Score BACKWARDS_PAWN = S(-17, -32);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(144, 32), S(-3, 59), S(0, 17), S(0, -1), S(5, 2), S(8, -5), S(0, 0),},
 { S(0, 0), S(140, 67), S(32, 81), S(11, 25), S(19, 5), S(12, -1), S(-1, 2), S(0, 0),},
 { S(0, 0), S(178, 140), S(62, 82), S(26, 25), S(11, 7), S(10, 6), S(3, -5), S(0, 0),},
 { S(0, 0), S(120, 170), S(55, 93), S(19, 35), S(11, 16), S(11, 9), S(11, 8), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 353, 324), S(  34, 168),
 S( -22, 143), S( -41,  97), S( -69,  49), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(7, -34);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 217, 424), S(  98, 389), S(  34, 240),
 S( -20, 162), S( -23,  88), S( -18,  76), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(0, -22);

const Score PASSED_PAWN_KING_PROXIMITY = S(-12, 49);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 203, 545), S(  24, 293), S(  11, 100), S(  22,  20),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(53, -239);

const Score PASSED_PAWN_SQ_RULE = S(0, 776);

const Score PASSED_PAWN_UNSUPPORTED = S(-49, -9);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(3, 172);

const Score KNIGHT_THREATS[6] = { S(-1, 41), S(-5, 85), S(69, 76), S(169, 23), S(138, -97), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 40), S(44, 75), S(0, 0), S(140, 42), S(121, 64), S(187, 2926),};

const Score ROOK_THREATS[6] = { S(-1, 45), S(61, 89), S(66, 108), S(16, 42), S(96, -73), S(396, 2122),};

const Score KING_THREAT = S(25, 70);

const Score PAWN_THREAT = S(166, 82);

const Score PAWN_PUSH_THREAT = S(35, 41);

const Score PAWN_PUSH_THREAT_PINNED = S(104, 256);

const Score HANGING_THREAT = S(22, 42);

const Score KNIGHT_CHECK_QUEEN = S(26, -1);

const Score BISHOP_CHECK_QUEEN = S(44, 46);

const Score ROOK_CHECK_QUEEN = S(39, 12);

const Score SPACE = 217;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(10, 46), S(0, 0),},
 { S(3, 37), S(-8, -44), S(0, 0),},
 { S(-1, 64), S(-50, -7), S(-12, 4), S(0, 0),},
 { S(81, 76), S(-110, 83), S(-59, 171), S(-474, 336), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-37, -23), S(74, 187), S(6, 90), S(-25, 0), S(23, -27), S(84, -73), S(72, -119), S(0, 0),},
 { S(-87, 9), S(-9, 163), S(-38, 110), S(-62, 52), S(-54, 22), S(51, -35), S(66, -63), S(0, 0),},
 { S(-59, 3), S(-17, 211), S(-92, 106), S(-43, 44), S(-42, 18), S(-21, 6), S(41, -16), S(0, 0),},
 { S(-77, 30), S(77, 130), S(-119, 67), S(-63, 54), S(-58, 39), S(-34, 16), S(-55, 36), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-32, -20), S(-41, 0), S(-42, -11), S(-51, 12), S(-112, 67), S(124, 169), S(563, 162), S(0, 0),},
 { S(-19, 3), S(12, -9), S(12, -6), S(-17, 23), S(-48, 47), S(-50, 163), S(204, 279), S(0, 0),},
 { S(53, -5), S(56, -17), S(51, -6), S(22, 9), S(-17, 29), S(-69, 141), S(209, 182), S(0, 0),},
 { S(2, -5), S(13, -22), S(37, -19), S(14, -24), S(-28, 4), S(-38, 96), S(-100, 268), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-20, -83), S(75, -106), S(45, -71), S(40, -62), S(17, -52), S(29, -176), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(82, -35);

const Score COMPLEXITY_PAWNS = 6;

const Score COMPLEXITY_PAWNS_BOTH_SIDES = 148;

const Score COMPLEXITY_OFFSET = -204;

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

      float netEval = NetworkEval(board->pieces[PAWN_WHITE], board->pieces[PAWN_BLACK], PAWN_NET);
      Score rounded = (Score)netEval;
      pawnS += S(rounded, rounded);

      TTPawnPut(board->pawnHash, pawnS, data.passedPawns, thread);
      s += pawnS;
    }
  } else {
    s += PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);

    float netEval = NetworkEval(board->pieces[PAWN_WHITE], board->pieces[PAWN_BLACK], PAWN_NET);
    Score rounded = (Score)netEval;
    s += S(rounded, rounded);
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
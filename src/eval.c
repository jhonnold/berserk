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

const Score BISHOP_PAIR = S(42, 184);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 190, 400), S( 220, 501), S( 147, 474), S( 221, 415),
 S( 125, 324), S(  94, 345), S( 117, 249), S( 139, 197),
 S(  98, 275), S( 109, 240), S(  78, 217), S(  72, 181),
 S(  81, 216), S(  64, 207), S(  70, 185), S(  79, 158),
 S(  72, 203), S(  58, 185), S(  71, 182), S(  69, 187),
 S(  69, 202), S(  97, 197), S( 111, 197), S(  97, 182),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 282, 294), S( 117, 345), S( 416, 303), S( 211, 434),
 S( 159, 211), S( 174, 180), S( 209, 151), S( 152, 171),
 S(  79, 201), S( 121, 186), S(  89, 169), S( 119, 158),
 S(  47, 180), S(  67, 177), S(  68, 164), S(  91, 144),
 S(  17, 181), S(  56, 160), S(  67, 175), S(  83, 181),
 S(  14, 193), S( 108, 182), S( 107, 197), S(  85, 200),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-317, 185), S(-347, 243), S(-297, 274), S(-217, 232),
 S(-180, 256), S(-144, 273), S( -94, 249), S( -82, 252),
 S( -54, 212), S( -76, 242), S( -84, 264), S(-107, 274),
 S( -85, 274), S( -74, 265), S( -50, 265), S( -70, 275),
 S( -85, 264), S( -73, 259), S( -46, 279), S( -61, 283),
 S(-127, 221), S(-106, 219), S( -87, 234), S( -93, 267),
 S(-150, 228), S(-116, 214), S(-107, 230), S( -95, 226),
 S(-195, 239), S(-118, 226), S(-117, 225), S( -99, 246),
},{
 S(-415,-135), S(-499, 182), S(-225, 157), S( -80, 217),
 S(-122, 126), S(  12, 181), S( -58, 239), S(-101, 248),
 S( -27, 158), S(-109, 210), S( -11, 160), S( -41, 230),
 S( -62, 237), S( -58, 232), S( -42, 247), S( -61, 271),
 S( -73, 262), S( -92, 250), S( -38, 272), S( -70, 303),
 S( -85, 211), S( -55, 206), S( -74, 221), S( -64, 259),
 S( -89, 219), S( -65, 209), S( -93, 217), S( -94, 221),
 S(-109, 239), S(-131, 230), S( -97, 215), S( -83, 242),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -59, 323), S(-105, 359), S( -87, 329), S(-129, 366),
 S( -24, 319), S( -48, 287), S(  27, 308), S(  -8, 341),
 S(  15, 329), S(  19, 325), S( -41, 291), S(  24, 313),
 S(   2, 326), S(  52, 315), S(  28, 326), S(  12, 358),
 S(  43, 299), S(  20, 325), S(  43, 321), S(  61, 316),
 S(  35, 277), S(  74, 307), S(  42, 274), S(  49, 305),
 S(  46, 270), S(  42, 213), S(  71, 256), S(  35, 291),
 S(  29, 224), S(  69, 264), S(  43, 302), S(  52, 296),
},{
 S(-132, 235), S(  67, 290), S( -97, 304), S(-153, 313),
 S( -10, 224), S( -79, 292), S( -33, 324), S(  24, 296),
 S(  57, 304), S(  -4, 313), S(  -3, 300), S(  16, 319),
 S(  -5, 304), S(  55, 307), S(  59, 318), S(  52, 317),
 S(  75, 255), S(  58, 298), S(  64, 315), S(  53, 318),
 S(  63, 257), S(  92, 283), S(  59, 273), S(  48, 326),
 S(  66, 215), S(  65, 216), S(  65, 280), S(  53, 292),
 S(  62, 177), S(  50, 317), S(  30, 308), S(  65, 293),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-297, 546), S(-288, 561), S(-280, 566), S(-344, 573),
 S(-268, 528), S(-284, 553), S(-251, 553), S(-214, 513),
 S(-305, 551), S(-240, 540), S(-259, 542), S(-254, 528),
 S(-285, 569), S(-264, 565), S(-234, 556), S(-228, 527),
 S(-287, 552), S(-283, 560), S(-265, 547), S(-255, 534),
 S(-297, 533), S(-276, 507), S(-265, 513), S(-267, 512),
 S(-286, 494), S(-277, 506), S(-250, 500), S(-244, 486),
 S(-289, 511), S(-274, 498), S(-269, 509), S(-255, 484),
},{
 S(-175, 517), S(-331, 596), S(-399, 596), S(-281, 551),
 S(-225, 487), S(-211, 514), S(-237, 520), S(-268, 523),
 S(-275, 501), S(-166, 493), S(-228, 485), S(-186, 479),
 S(-266, 519), S(-204, 516), S(-218, 500), S(-209, 507),
 S(-291, 501), S(-259, 510), S(-259, 503), S(-233, 513),
 S(-270, 450), S(-231, 446), S(-245, 464), S(-239, 485),
 S(-258, 444), S(-209, 425), S(-240, 446), S(-232, 465),
 S(-281, 468), S(-240, 460), S(-258, 465), S(-235, 462),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-154, 800), S(   7, 610), S(  79, 621), S(  60, 675),
 S(  -5, 662), S(  45, 585), S(  62, 657), S(  63, 649),
 S(  54, 612), S(  72, 589), S(  66, 639), S( 101, 642),
 S(  46, 655), S(  57, 693), S(  73, 683), S(  50, 695),
 S(  59, 613), S(  33, 709), S(  41, 684), S(  29, 729),
 S(  44, 578), S(  50, 606), S(  44, 646), S(  33, 650),
 S(  50, 510), S(  46, 540), S(  58, 547), S(  55, 577),
 S(  19, 556), S(  21, 554), S(  26, 563), S(  31, 574),
},{
 S( 143, 585), S( 252, 562), S(-126, 794), S( 106, 580),
 S( 119, 564), S(  86, 565), S(  86, 617), S(  49, 685),
 S( 119, 557), S(  96, 524), S( 125, 585), S(  94, 642),
 S(  49, 645), S(  71, 653), S( 111, 585), S(  70, 691),
 S(  69, 583), S(  63, 631), S(  76, 636), S(  39, 724),
 S(  60, 511), S(  76, 559), S(  76, 611), S(  45, 650),
 S(  89, 441), S(  91, 439), S(  80, 488), S(  66, 570),
 S(  71, 496), S(   8, 553), S(  22, 515), S(  41, 539),
}};

const Score KING_PSQT[2][32] = {{
 S( 510,-403), S( 219,-149), S(  34,-111), S( 254,-176),
 S(-227, 108), S(-270, 240), S(-124, 147), S( -24,  90),
 S(   0,  66), S(-130, 199), S( -18, 158), S(-107, 170),
 S(  27,  58), S( -25, 141), S( -68, 118), S( -66, 107),
 S( -49,  21), S(   9,  79), S(  53,  56), S( -69,  65),
 S( -16, -41), S(   3,  21), S(   2,   7), S( -28,  18),
 S(  37, -90), S( -29,  20), S( -32,  -9), S( -37, -15),
 S(  42,-208), S(  55,-109), S(  21,-128), S( -80,-110),
},{
 S(-114,-309), S(-489,  56), S( -42,-141), S( -42,-140),
 S(-426,  27), S(-214, 216), S(-318, 164), S( 324,  24),
 S(-198,  45), S(  50, 203), S( 284, 130), S(  78, 155),
 S(-156,  24), S(  11, 139), S(  24, 131), S(  -6, 123),
 S(-162,   9), S(-106,  82), S(  34,  67), S( -12,  75),
 S(  -4, -31), S(  33,  40), S(   7,  27), S(  19,  38),
 S(  35, -30), S(  10,  52), S( -10,  25), S( -45,  16),
 S(  30,-131), S(  38, -32), S(  -1, -64), S( -67, -79),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S(-106,  29), S(  10,  43), S(  54,  57), S( 121,  87),
 S(  30,  -7), S(  77,  46), S(  58,  58), S(  82,  83),
 S(  42,   0), S(  69,  25), S(  45,  34), S(  40,  44),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -10,  13), S(  48,   2), S( 116,  26), S( 124,  17),
 S(   8,  -5), S(  47,  25), S(  86,   8), S( 103,  20),
 S( -10,  44), S(  84,  19), S(  63,  27), S(  73,  41),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-251, -44), S(-167, 120), S(-130, 200), S(-106, 230),
 S( -88, 256), S( -73, 283), S( -57, 291), S( -40, 297),
 S( -24, 281),};

const Score BISHOP_MOBILITIES[14] = {
 S( -42, 138), S(   7, 203), S(  35, 249), S(  48, 292),
 S(  61, 310), S(  68, 332), S(  71, 348), S(  79, 356),
 S(  79, 366), S(  86, 370), S( 100, 363), S( 120, 358),
 S( 120, 368), S( 145, 339),};

const Score ROOK_MOBILITIES[15] = {
 S(-250, -80), S(-281, 377), S(-252, 449), S(-240, 449),
 S(-246, 492), S(-237, 506), S(-247, 523), S(-239, 524),
 S(-232, 536), S(-223, 547), S(-220, 557), S(-230, 573),
 S(-225, 580), S(-214, 592), S(-198, 581),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2258,-2395), S(-134,-384), S( -39, 589), S(   1, 874),
 S(  24, 955), S(  38, 972), S(  38,1037), S(  42,1080),
 S(  48,1112), S(  55,1121), S(  61,1133), S(  68,1133),
 S(  68,1145), S(  77,1145), S(  76,1150), S(  92,1133),
 S(  90,1138), S(  87,1143), S(  98,1110), S( 133,1054),
 S( 165, 987), S( 186, 919), S( 251, 855), S( 299, 705),
 S( 262, 695), S( 271, 596), S(  21, 586), S( 792, -63),
};

const Score KING_MOBILITIES[9] = {
 S(   8, -29), S(   5,  39), S(   1,  47),
 S(  -4,  46), S(  -4,  29), S(  -6,   6),
 S(  -9,   0), S(  -2, -22), S(   5, -85),
};

const Score MINOR_BEHIND_PAWN = S(11, 28);

const Score KNIGHT_OUTPOST_REACHABLE = S(20, 42);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 13);

const Score BISHOP_TRAPPED = S(-222, -535);

const Score ROOK_TRAPPED = S(-77, -54);

const Score BAD_BISHOP_PAWNS = S(-2, -9);

const Score DRAGON_BISHOP = S(45, 37);

const Score ROOK_OPEN_FILE_OFFSET = S(30, 14);

const Score ROOK_OPEN_FILE = S(56, 34);

const Score ROOK_SEMI_OPEN = S(33, 14);

const Score ROOK_TO_OPEN = S(32, 38);

const Score QUEEN_OPPOSITE_ROOK = S(-29, 6);

const Score QUEEN_ROOK_BATTERY = S(7, 108);

const Score DEFENDED_PAWN = S(24, 18);

const Score DOUBLED_PAWN = S(36, -73);

const Score ISOLATED_PAWN[4] = {
 S(   2, -28), S(  -4, -31), S( -14, -16), S(   0, -16),
};

const Score OPEN_ISOLATED_PAWN = S(-6, -21);

const Score BACKWARDS_PAWN = S(-16, -34);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(127, 31), S(-9, 60), S(1, 21), S(2, 0), S(6, 0), S(9, -7), S(0, 0),},
 { S(0, 0), S(152, 54), S(32, 86), S(9, 27), S(19, 4), S(14, -4), S(-1, 2), S(0, 0),},
 { S(0, 0), S(145, 151), S(56, 85), S(27, 29), S(12, 8), S(10, 5), S(2, -7), S(0, 0),},
 { S(0, 0), S(124, 160), S(53, 96), S(19, 38), S(12, 17), S(11, 9), S(10, 6), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 386, 320), S(  27, 166),
 S( -27, 143), S( -44, 101), S( -62,  52), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(9, -34);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 254, 458), S( 117, 409), S(  29, 249),
 S( -20, 168), S( -17,  92), S( -11,  80), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -23);

const Score PASSED_PAWN_KING_PROXIMITY = S(-13, 51);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 184, 566), S(  20, 298), S(  15, 102), S(  24,  23),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(59, -250);

const Score PASSED_PAWN_SQ_RULE = S(0, 803);

const Score PASSED_PAWN_UNSUPPORTED = S(-47, -10);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(-34, 176);

const Score KNIGHT_THREATS[6] = { S(-1, 43), S(-7, 97), S(72, 79), S(175, 25), S(145, -103), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 41), S(46, 79), S(0, 0), S(146, 44), S(128, 61), S(205, 2970),};

const Score ROOK_THREATS[6] = { S(0, 47), S(65, 92), S(69, 113), S(16, 44), S(102, -76), S(406, 2164),};

const Score KING_THREAT = S(27, 69);

const Score PAWN_THREAT = S(173, 85);

const Score PAWN_PUSH_THREAT = S(36, 41);

const Score PAWN_PUSH_THREAT_PINNED = S(107, 265);

const Score HANGING_THREAT = S(23, 44);

const Score KNIGHT_CHECK_QUEEN = S(27, -2);

const Score BISHOP_CHECK_QUEEN = S(46, 45);

const Score ROOK_CHECK_QUEEN = S(42, 10);

const Score SPACE = 232;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(14, 53), S(0, 0),},
 { S(7, 43), S(-7, -44), S(0, 0),},
 { S(3, 73), S(-53, -4), S(-17, 7), S(0, 0),},
 { S(91, 96), S(-119, 99), S(-66, 184), S(-504, 359), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-41, -19), S(66, 182), S(-15, 97), S(-23, 3), S(25, -29), S(87, -76), S(76, -121), S(0, 0),},
 { S(-91, 12), S(-29, 161), S(-63, 112), S(-66, 55), S(-53, 24), S(54, -38), S(71, -65), S(0, 0),},
 { S(-59, 6), S(-14, 216), S(-94, 107), S(-44, 43), S(-44, 19), S(-23, 7), S(42, -19), S(0, 0),},
 { S(-82, 30), S(63, 139), S(-113, 71), S(-71, 52), S(-66, 38), S(-38, 16), S(-53, 39), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-32, -20), S(-40, -1), S(-40, -11), S(-55, 9), S(-111, 64), S(119, 157), S(583, 168), S(0, 0),},
 { S(-15, 4), S(18, -8), S(17, -5), S(-21, 20), S(-52, 43), S(-56, 163), S(201, 286), S(0, 0),},
 { S(56, -5), S(59, -16), S(46, -8), S(20, 5), S(-23, 26), S(-75, 158), S(176, 194), S(0, 0),},
 { S(-3, -6), S(7, -25), S(29, -22), S(9, -26), S(-29, 6), S(-40, 96), S(-121, 274), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-19, -73), S(86, -110), S(42, -76), S(37, -68), S(19, -57), S(19, -182), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(81, -44);

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
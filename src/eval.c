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
const Score MATERIAL_VALUES[7] = { S(100, 100), S(325, 325), S(325, 325), S(550, 550), S(1000, 1000), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(20, 89);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 110, 211), S( 106, 237), S(  72, 232), S( 111, 196),
 S(  72, 150), S(  52, 165), S(  66, 109), S(  72,  83),
 S(  71, 130), S(  70, 113), S(  52,  96), S(  50,  83),
 S(  66, 103), S(  56, 104), S(  56,  88), S(  64,  84),
 S(  57,  94), S(  58,  97), S(  56,  89), S(  57,  94),
 S(  63,  98), S(  71,  99), S(  70,  95), S(  72,  91),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 152, 162), S(  65, 164), S( 167, 158), S(  95, 203),
 S(  89,  93), S(  96,  83), S( 111,  56), S(  79,  70),
 S(  62,  92), S(  77,  83), S(  58,  75), S(  74,  72),
 S(  50,  84), S(  62,  86), S(  58,  78), S(  75,  75),
 S(  31,  81), S(  63,  82), S(  60,  83), S(  67,  89),
 S(  36,  92), S(  79,  90), S(  69,  92), S(  70, 104),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-189,  66), S(-199,  91), S(-170, 101), S(-128,  78),
 S(-119,  98), S(-100, 101), S( -74,  85), S( -70,  87),
 S( -59,  76), S( -67,  86), S( -71,  97), S( -81,  97),
 S( -71, 103), S( -66,  97), S( -54,  95), S( -65, 100),
 S( -72,  97), S( -66,  95), S( -52, 103), S( -60, 104),
 S( -92,  76), S( -82,  76), S( -73,  83), S( -76, 100),
 S(-103,  77), S( -87,  75), S( -82,  80), S( -77,  80),
 S(-125,  84), S( -88,  77), S( -89,  79), S( -80,  90),
},{
 S(-234, -98), S(-265,  55), S(-140,  48), S( -64,  70),
 S( -92,  29), S( -24,  54), S( -61,  83), S( -77,  88),
 S( -45,  44), S( -88,  68), S( -35,  43), S( -51,  78),
 S( -62,  80), S( -58,  77), S( -52,  84), S( -60,  97),
 S( -66,  93), S( -75,  85), S( -49,  97), S( -65, 111),
 S( -72,  71), S( -57,  68), S( -67,  76), S( -62,  95),
 S( -73,  72), S( -62,  72), S( -75,  75), S( -77,  78),
 S( -83,  81), S( -94,  79), S( -77,  74), S( -71,  88),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -26, 130), S( -48, 149), S( -34, 129), S( -57, 152),
 S(  -8, 132), S( -20, 116), S(  18, 122), S(   3, 135),
 S(  10, 137), S(  12, 136), S( -16, 115), S(  14, 124),
 S(   4, 133), S(  29, 126), S(  17, 131), S(   9, 149),
 S(  25, 117), S(  14, 129), S(  24, 129), S(  32, 127),
 S(  20, 109), S(  38, 123), S(  23, 107), S(  26, 123),
 S(  25, 106), S(  23,  79), S(  37,  99), S(  20, 117),
 S(  17,  83), S(  36, 102), S(  24, 122), S(  28, 118),
},{
 S( -62,  89), S(  43, 111), S( -41, 119), S( -70, 126),
 S(  -1,  85), S( -35, 116), S( -12, 132), S(  13, 118),
 S(  30, 122), S(   1, 124), S(   4, 116), S(  14, 126),
 S(   1, 117), S(  30, 120), S(  33, 125), S(  29, 126),
 S(  40,  96), S(  33, 117), S(  34, 127), S(  28, 128),
 S(  34, 100), S(  49, 112), S(  32, 107), S(  26, 134),
 S(  36,  81), S(  36,  82), S(  36, 113), S(  29, 118),
 S(  35,  62), S(  25, 135), S(  19, 126), S(  35, 117),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-146, 230), S(-142, 238), S(-137, 239), S(-167, 242),
 S(-131, 224), S(-138, 235), S(-122, 235), S(-104, 215),
 S(-149, 235), S(-116, 230), S(-126, 229), S(-125, 223),
 S(-142, 247), S(-128, 244), S(-115, 240), S(-112, 223),
 S(-143, 241), S(-139, 242), S(-131, 237), S(-126, 229),
 S(-147, 231), S(-136, 218), S(-132, 222), S(-131, 218),
 S(-141, 212), S(-136, 218), S(-125, 216), S(-121, 207),
 S(-143, 220), S(-135, 214), S(-134, 220), S(-126, 206),
},{
 S( -83, 215), S(-150, 249), S(-196, 254), S(-143, 238),
 S(-112, 205), S(-101, 219), S(-122, 223), S(-132, 222),
 S(-135, 210), S( -86, 207), S(-110, 198), S( -90, 198),
 S(-130, 218), S( -96, 214), S(-105, 204), S(-102, 212),
 S(-145, 214), S(-127, 216), S(-128, 211), S(-114, 216),
 S(-133, 188), S(-115, 189), S(-121, 192), S(-118, 205),
 S(-128, 190), S(-104, 179), S(-119, 188), S(-115, 196),
 S(-139, 200), S(-119, 196), S(-126, 196), S(-117, 196),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-106, 445), S( -19, 341), S(  22, 338), S(   7, 369),
 S( -27, 376), S(  -2, 335), S(   7, 369), S(   3, 371),
 S(   1, 352), S(  12, 337), S(   8, 363), S(  25, 359),
 S(  -2, 372), S(   5, 387), S(  10, 384), S(   1, 388),
 S(   5, 350), S(  -8, 395), S(  -5, 389), S( -10, 407),
 S(  -2, 333), S(   1, 348), S(  -3, 365), S(  -7, 366),
 S(   1, 301), S(   0, 311), S(   5, 316), S(   4, 331),
 S( -13, 324), S( -13, 318), S( -11, 324), S(  -8, 329),
},{
 S(  49, 323), S(  99, 318), S( -62, 411), S(  17, 339),
 S(  36, 314), S(  22, 316), S(  19, 352), S(   0, 381),
 S(  33, 323), S(  20, 301), S(  33, 339), S(  25, 357),
 S(   1, 357), S(  12, 363), S(  31, 326), S(   9, 385),
 S(  11, 327), S(   8, 352), S(  14, 356), S(  -5, 400),
 S(   7, 293), S(  13, 321), S(  13, 346), S(  -1, 365),
 S(  23, 263), S(  22, 262), S(  15, 289), S(   9, 327),
 S(  18, 277), S( -17, 314), S(  -9, 293), S(  -3, 310),
}};

const Score KING_PSQT[2][32] = {{
 S( 225,-148), S( 131, -66), S( -23, -19), S( 146, -64),
 S(-125,  67), S( -79,  85), S( -46,  68), S(  10,  29),
 S( -17,  52), S( -34,  85), S(   3,  73), S( -37,  75),
 S(  -4,  45), S(   2,  65), S( -33,  68), S( -43,  59),
 S( -30,  24), S(  10,  40), S(  26,  33), S( -36,  35),
 S( -28,  -3), S(  -3,  15), S(  -5,  11), S( -19,   5),
 S(   7, -26), S( -21,   9), S( -25,  -7), S( -32, -15),
 S(  16, -86), S(  20, -41), S(   7, -41), S( -46, -39),
},{
 S( -47,-137), S(-271,  52), S( -28, -32), S( -44, -42),
 S(-208,  15), S(-125, 102), S(-135,  91), S( 150,   8),
 S(-107,  29), S(  36,  93), S( 149,  75), S(  53,  73),
 S( -77,  13), S(  24,  61), S(  15,  75), S(   1,  62),
 S( -73,   5), S( -44,  40), S(  11,  38), S( -14,  38),
 S(  -4, -13), S(  24,  13), S(   7,  14), S(  11,   8),
 S(  14, -16), S(   9,  11), S(  -7,   1), S( -24, -10),
 S(  17, -66), S(  17, -17), S(   2, -25), S( -31, -36),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -50,  12), S(   6,  19), S(  28,  25), S(  58,  40),
 S(  13,  -2), S(  37,  21), S(  28,  27), S(  39,  36),
 S(  20,  -3), S(  33,  10), S(  19,  16), S(  20,  22),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -6,   6), S(  23,   1), S(  57,  15), S(  61,   5),
 S(   3,  -2), S(  23,  12), S(  41,   3), S(  51,   9),
 S(  -5,  22), S(  41,   9), S(  30,  13), S(  38,  21),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-156, -16), S(-117,  61), S( -99, 100), S( -87, 114),
 S( -79, 126), S( -72, 139), S( -64, 142), S( -56, 145),
 S( -48, 136),};

const Score BISHOP_MOBILITIES[14] = {
 S( -17,  67), S(   6,  97), S(  18, 119), S(  25, 140),
 S(  31, 148), S(  35, 159), S(  36, 166), S(  40, 170),
 S(  39, 173), S(  42, 175), S(  50, 172), S(  59, 169),
 S(  63, 171), S(  72, 158),};

const Score ROOK_MOBILITIES[15] = {
 S(-112, -68), S(-125, 154), S(-111, 187), S(-105, 188),
 S(-109, 207), S(-105, 213), S(-110, 220), S(-106, 221),
 S(-102, 227), S( -98, 232), S( -96, 236), S(-100, 242),
 S( -97, 245), S( -91, 250), S( -85, 245),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1883,-1384), S(-125,-259), S( -72, 199), S( -52, 333),
 S( -41, 375), S( -34, 386), S( -34, 416), S( -33, 437),
 S( -30, 454), S( -27, 458), S( -24, 463), S( -21, 464),
 S( -20, 469), S( -16, 470), S( -17, 471), S( -10, 463),
 S( -11, 465), S( -12, 467), S(  -4, 444), S(  16, 410),
 S(  32, 378), S(  38, 347), S(  76, 307), S( 130, 201),
 S(  99, 209), S( 144, 130), S(  12, 122), S( 160, -20),
};

const Score MINOR_BEHIND_PAWN = S(5, 13);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-110, -252);

const Score ROOK_TRAPPED = S(-37, -27);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(22, 18);

const Score ROOK_OPEN_FILE_OFFSET = S(12, 8);

const Score ROOK_OPEN_FILE = S(27, 16);

const Score ROOK_SEMI_OPEN = S(16, 7);

const Score ROOK_TO_OPEN = S(16, 20);

const Score QUEEN_OPPOSITE_ROOK = S(-14, 4);

const Score QUEEN_ROOK_BATTERY = S(2, 56);

const Score DEFENDED_PAWN = S(12, 8);

const Score DOUBLED_PAWN = S(16, -36);

const Score ISOLATED_PAWN[4] = {
 S(   0, -10), S(   0, -15), S(  -6,  -6), S(   1,  -8),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -10);

const Score BACKWARDS_PAWN = S(-9, -16);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(63, 19), S(0, 27), S(0, 8), S(0, -1), S(3, 1), S(4, -3), S(0, 0),},
 { S(0, 0), S(79, 26), S(18, 37), S(6, 11), S(9, 2), S(6, 0), S(-1, 2), S(0, 0),},
 { S(0, 0), S(93, 67), S(32, 40), S(13, 12), S(6, 3), S(5, 3), S(1, -3), S(0, 0),},
 { S(0, 0), S(62, 82), S(28, 47), S(10, 17), S(6, 8), S(5, 5), S(5, 4), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 191, 169), S(  16,  86),
 S( -11,  71), S( -20,  48), S( -35,  24), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(3, -17);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 117, 203), S(  48, 196), S(  15, 123),
 S( -12,  83), S( -13,  45), S( -11,  40), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -12);

const Score PASSED_PAWN_KING_PROXIMITY = S(-6, 25);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S(  94, 272), S(  12, 145), S(   5,  51), S(  10,  11),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(26, -119);

const Score PASSED_PAWN_SQ_RULE = S(0, 379);

const Score PASSED_PAWN_UNSUPPORTED = S(-26, -5);

const Score KNIGHT_THREATS[6] = { S(-1, 20), S(-2, 32), S(35, 38), S(84, 12), S(70, -49), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 20), S(22, 38), S(-66, 81), S(71, 21), S(59, 43), S(260, 812),};

const Score ROOK_THREATS[6] = { S(-1, 24), S(31, 44), S(33, 56), S(8, 22), S(48, -35), S(343, 823),};

const Score KING_THREAT = S(12, 37);

const Score PAWN_THREAT = S(83, 39);

const Score PAWN_PUSH_THREAT = S(18, 19);

const Score PAWN_PUSH_THREAT_PINNED = S(53, 128);

const Score HANGING_THREAT = S(11, 22);

const Score KNIGHT_CHECK_QUEEN = S(12, 2);

const Score BISHOP_CHECK_QUEEN = S(22, 23);

const Score ROOK_CHECK_QUEEN = S(19, 9);

const Score SPACE = 104;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(8, 24), S(0, 0),},
 { S(5, 20), S(-9, -31), S(0, 0),},
 { S(1, 34), S(-34, -23), S(-8, -3), S(0, 0),},
 { S(49, 45), S(-73, 18), S(-17, 81), S(-204, 169), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-21, -5), S(28, 102), S(-3, 52), S(-17, 7), S(7, -6), S(38, -26), S(33, -49), S(0, 0),},
 { S(-44, -1), S(-11, 83), S(-21, 47), S(-33, 20), S(-28, 6), S(24, -19), S(32, -33), S(0, 0),},
 { S(-22, -14), S(2, 88), S(-41, 37), S(-14, 6), S(-15, -5), S(-5, -9), S(28, -19), S(0, 0),},
 { S(-37, 18), S(39, 72), S(-59, 36), S(-31, 29), S(-29, 22), S(-17, 14), S(-27, 24), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-19, -13), S(-24, -2), S(-24, -7), S(-29, 4), S(-59, 31), S(60, 80), S(270, 76), S(0, 0),},
 { S(-13, -1), S(3, -6), S(3, -5), S(-11, 9), S(-27, 21), S(-32, 86), S(96, 137), S(0, 0),},
 { S(24, -6), S(25, -10), S(24, -6), S(8, 3), S(-13, 17), S(-43, 75), S(61, 109), S(0, 0),},
 { S(-2, -5), S(4, -14), S(15, -12), S(3, -14), S(-19, 4), S(-25, 51), S(-44, 131), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-11, -47), S(37, -55), S(20, -38), S(17, -32), S(5, -25), S(9, -88), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(41, -19);

const Score COMPLEXITY_PAWNS = 7;

const Score COMPLEXITY_KING_DISTANCE = 3;

const Score COMPLEXITY_PAWN_DIFF = 5;

const Score COMPLEXITY_OFFSET = -69;

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

const Score TEMPO = 37;
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
      int numOpenFiles = 8 - bits(Fill(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK], N) & 0xFULL);
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
    s += S(-danger * danger / 1024, -danger / 32);

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

INLINE Score Complexity(Board* board, EvalData* data, Score eg) {
  int pawns = bits(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK]);
  int kingDistance = abs(file(data->kingSq[WHITE]) - file(data->kingSq[BLACK])) -
                     abs(rank(data->kingSq[WHITE]) - rank(data->kingSq[BLACK]));
  int pawnImbalanace = bits(0xFFULL & (FileFill(board->pieces[PAWN_WHITE]) ^ FileFill(board->pieces[PAWN_BLACK])));

  Score complexity = pawns * COMPLEXITY_PAWNS                  //
                     + kingDistance * COMPLEXITY_KING_DISTANCE //
                     + pawnImbalanace * COMPLEXITY_PAWN_DIFF   //
                     + COMPLEXITY_OFFSET;

  if (T) {
    C.complexPawns = pawns;
    C.complexKingDistance = kingDistance;
    C.complexPawnImbalance = pawnImbalanace;
    C.complexOffset = 1;
  }

  int sign = (eg > 0) - (eg < 0);
  return S(0, sign * max(complexity, -abs(eg)));
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

  if (T || abs(scoreMG(s) + scoreEG(s)) / 2 < 1024) {
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

  s += Complexity(board, &data, scoreEG(s));
  s += thread->data.contempt;

  // taper
  int phase = GetPhase(board);
  Score res = (phase * scoreMG(s) + (128 - phase) * scoreEG(s)) / 128;

  if (T)
    C.ss = res >= 0 ? WHITE : BLACK;

  // scale the score
  res = (res * Scale(board, res >= 0 ? WHITE : BLACK)) / MAX_SCALE;
  return TEMPO + (board->side == WHITE ? res : -res);
}

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

const Score BISHOP_PAIR = S(20, 87);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 118, 197), S( 109, 222), S(  79, 217), S( 113, 192),
 S(  79, 135), S(  59, 153), S(  79,  99), S(  84,  79),
 S(  77, 116), S(  76, 103), S(  64,  86), S(  59,  79),
 S(  73,  91), S(  68,  94), S(  64,  80), S(  72,  81),
 S(  65,  83), S(  64,  86), S(  64,  81), S(  66,  90),
 S(  74,  87), S(  78,  93), S(  76,  85), S(  83,  87),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 157, 151), S(  53, 157), S( 160, 148), S(  98, 195),
 S(  93,  82), S( 100,  74), S( 117,  49), S(  90,  67),
 S(  67,  79), S(  83,  73), S(  66,  64), S(  83,  67),
 S(  56,  72), S(  72,  77), S(  63,  69), S(  83,  73),
 S(  37,  71), S(  69,  72), S(  67,  75), S(  76,  85),
 S(  47,  81), S(  85,  83), S(  76,  83), S(  84,  99),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-189,  53), S(-195,  75), S(-165,  84), S(-123,  64),
 S(-118,  82), S( -98,  86), S( -73,  69), S( -68,  70),
 S( -60,  63), S( -66,  71), S( -69,  80), S( -81,  82),
 S( -71,  89), S( -66,  81), S( -53,  79), S( -64,  83),
 S( -72,  82), S( -65,  77), S( -51,  86), S( -59,  86),
 S( -92,  61), S( -82,  60), S( -73,  68), S( -76,  83),
 S(-103,  62), S( -86,  58), S( -83,  65), S( -77,  64),
 S(-126,  70), S( -90,  60), S( -89,  62), S( -81,  74),
},{
 S(-242, -97), S(-290,  51), S(-139,  34), S( -70,  60),
 S( -91,  15), S( -21,  38), S( -60,  68), S( -77,  71),
 S( -47,  33), S( -88,  50), S( -34,  29), S( -50,  62),
 S( -61,  65), S( -58,  61), S( -51,  68), S( -59,  79),
 S( -66,  78), S( -75,  69), S( -48,  80), S( -65,  94),
 S( -72,  56), S( -57,  52), S( -67,  60), S( -62,  79),
 S( -73,  57), S( -62,  55), S( -75,  57), S( -77,  61),
 S( -83,  66), S( -95,  63), S( -76,  56), S( -72,  71),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -21, 113), S( -40, 134), S( -28, 115), S( -43, 130),
 S(  -1, 118), S( -15, 104), S(  24, 109), S(  10, 121),
 S(  17, 121), S(  19, 121), S( -10, 102), S(  22, 110),
 S(  10, 119), S(  36, 113), S(  25, 116), S(  17, 133),
 S(  31, 103), S(  21, 115), S(  30, 116), S(  38, 113),
 S(  25,  96), S(  43, 109), S(  27,  94), S(  31, 110),
 S(  29,  93), S(  28,  66), S(  42,  84), S(  24, 104),
 S(  20,  73), S(  39,  88), S(  27, 105), S(  32, 104),
},{
 S( -61,  76), S(  44,  99), S( -35, 106), S( -61, 110),
 S(   5,  73), S( -29, 103), S(  -8, 120), S(  21, 102),
 S(  37, 107), S(   8, 109), S(   9, 104), S(  23, 111),
 S(   8, 103), S(  35, 108), S(  39, 111), S(  35, 113),
 S(  45,  81), S(  37, 104), S(  40, 113), S(  34, 114),
 S(  39,  86), S(  54,  97), S(  36,  94), S(  31, 120),
 S(  40,  68), S(  40,  67), S(  41,  99), S(  34, 104),
 S(  39,  50), S(  29, 123), S(  23, 110), S(  38, 102),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -97, 237), S( -88, 241), S( -89, 245), S(-117, 247),
 S( -87, 231), S( -93, 241), S( -76, 240), S( -61, 223),
 S(-102, 239), S( -66, 233), S( -78, 234), S( -76, 228),
 S( -95, 252), S( -80, 248), S( -67, 245), S( -64, 229),
 S( -98, 247), S( -90, 247), S( -86, 244), S( -78, 234),
 S(-101, 236), S( -91, 224), S( -85, 227), S( -84, 224),
 S( -97, 221), S( -92, 226), S( -80, 223), S( -76, 214),
 S( -99, 230), S( -92, 222), S( -90, 228), S( -82, 213),
},{
 S( -35, 222), S( -86, 246), S(-143, 257), S( -88, 238),
 S( -65, 211), S( -54, 224), S( -76, 228), S( -88, 229),
 S( -91, 216), S( -36, 211), S( -63, 204), S( -44, 205),
 S( -84, 223), S( -49, 219), S( -58, 208), S( -56, 219),
 S(-102, 221), S( -82, 222), S( -83, 217), S( -69, 223),
 S( -90, 195), S( -71, 195), S( -77, 199), S( -72, 211),
 S( -84, 197), S( -61, 185), S( -74, 194), S( -71, 204),
 S( -97, 210), S( -76, 203), S( -82, 204), S( -74, 203),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-112, 403), S( -30, 307), S(   5, 308), S(   0, 329),
 S( -44, 345), S( -27, 311), S( -14, 338), S( -19, 339),
 S(  -7, 313), S(  -2, 304), S( -10, 333), S(   9, 327),
 S(  -9, 336), S(  -2, 350), S(   2, 349), S(  -8, 352),
 S(   5, 306), S( -13, 358), S(  -7, 347), S( -14, 366),
 S(  -3, 291), S(  -1, 308), S(  -5, 326), S(  -9, 326),
 S(   1, 259), S(  -3, 273), S(   3, 274), S(   3, 288),
 S( -14, 281), S( -14, 277), S( -11, 280), S(  -9, 289),
},{
 S(  35, 290), S(  96, 265), S(-100, 393), S(  12, 294),
 S(  28, 281), S(  14, 269), S(  -7, 324), S( -21, 347),
 S(  33, 274), S(   6, 270), S(  27, 291), S(   1, 330),
 S(   0, 313), S(  10, 317), S(  26, 285), S(   2, 345),
 S(   9, 284), S(   6, 309), S(  13, 311), S(  -9, 362),
 S(   5, 252), S(  13, 275), S(  12, 301), S(  -3, 323),
 S(  25, 213), S(  22, 215), S(  16, 242), S(   7, 284),
 S(  14, 248), S( -15, 264), S( -10, 249), S(  -4, 266),
}};

const Score KING_PSQT[2][32] = {{
 S( 231,-134), S( 135, -58), S( -18, -15), S( 106, -42),
 S( -97,  58), S( -32,  70), S(   1,  56), S(  37,  23),
 S(  13,  44), S(  -2,  75), S(  29,  64), S(  -9,  67),
 S(  18,  40), S(  13,  61), S(  -9,  61), S( -19,  53),
 S( -26,  25), S(  19,  38), S(  33,  31), S( -28,  34),
 S( -25,  -2), S(   2,  14), S(  -1,  11), S( -12,   5),
 S(   5, -23), S( -21,  11), S( -23,  -4), S( -28, -13),
 S(  12, -81), S(  18, -38), S(   7, -37), S( -48, -35),
},{
 S( -27,-135), S(-296,  56), S(   9, -35), S( -41, -35),
 S(-208,  16), S( -88,  87), S( -73,  69), S( 147,  14),
 S(-116,  30), S(  43,  86), S( 146,  70), S(  84,  60),
 S( -93,  17), S(  16,  58), S(   4,  72), S(   6,  57),
 S( -79,   4), S( -48,  38), S(   6,  36), S(  -8,  35),
 S(  -4, -15), S(  24,  11), S(   8,  12), S(  15,   7),
 S(  13, -15), S(   9,  10), S(  -5,   0), S( -19, -10),
 S(  16, -63), S(  17, -16), S(   4, -25), S( -32, -34),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -47,   9), S(   6,  18), S(  28,  23), S(  60,  35),
 S(  13,  -5), S(  38,  18), S(  29,  26), S(  40,  35),
 S(  21,  -3), S(  34,   9), S(  19,  15), S(  20,  23),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -7,   6), S(  22,   3), S(  58,  12), S(  61,   6),
 S(   1,  -2), S(  24,  11), S(  40,   3), S(  51,   9),
 S(  -7,  23), S(  42,   7), S(  31,  11), S(  38,  20),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-158, -32), S(-116,  43), S( -99,  82), S( -87,  96),
 S( -78, 109), S( -71, 121), S( -63, 124), S( -55, 126),
 S( -47, 117),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -9,  51), S(  14,  79), S(  26, 101), S(  33, 122),
 S(  39, 131), S(  42, 142), S(  44, 149), S(  47, 152),
 S(  47, 155), S(  50, 155), S(  58, 152), S(  70, 147),
 S(  70, 154), S(  80, 139),};

const Score ROOK_MOBILITIES[15] = {
 S( -78, -68), S( -95, 147), S( -80, 184), S( -73, 189),
 S( -75, 212), S( -71, 220), S( -76, 230), S( -71, 232),
 S( -68, 238), S( -62, 242), S( -60, 246), S( -62, 251),
 S( -58, 253), S( -50, 257), S( -38, 249),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1871,-1381), S(-127,-314), S( -79, 143), S( -55, 270),
 S( -43, 308), S( -37, 319), S( -38, 351), S( -37, 374),
 S( -35, 392), S( -31, 396), S( -29, 401), S( -26, 401),
 S( -26, 408), S( -22, 407), S( -24, 409), S( -19, 401),
 S( -21, 402), S( -23, 403), S( -17, 383), S(  -1, 354),
 S(  15, 320), S(  29, 280), S(  64, 247), S( 109, 156),
 S(  69, 169), S( 198,  36), S(  22,  55), S(  53,   1),
};

const Score MINOR_BEHIND_PAWN = S(5, 14);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-117, -241);

const Score ROOK_TRAPPED = S(-42, -33);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(23, 17);

const Score ROOK_OPEN_FILE = S(29, 15);

const Score ROOK_SEMI_OPEN = S(16, 6);

const Score QUEEN_OPPOSITE_ROOK = S(-15, 3);

const Score QUEEN_ROOK_BATTERY = S(6, 53);

const Score DEFENDED_PAWN = S(12, 10);

const Score DOUBLED_PAWN = S(16, -36);

const Score ISOLATED_PAWN[4] = {
 S(   0,  -5), S(   0, -13), S(  -6,  -5), S(   1, -13),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -11);

const Score BACKWARDS_PAWN = S(-9, -18);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  84,  45), S(  27,  38), S(   9,  13),
 S(   6,   3), S(   5,   2), S(   2,   0), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 160, 165), S(  15,  75),
 S( -14,  61), S( -24,  39), S( -37,  15), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(5, -11);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 118, 186), S(  41, 184), S(  12, 114),
 S( -14,  75), S( -13,  42), S( -11,  37), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-6, 24);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S(  88, 262), S(  16, 138), S(   8,  47), S(  11,  10),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(27, -114);

const Score PASSED_PAWN_SQ_RULE = S(0, 357);

const Score KNIGHT_THREATS[6] = { S(0, 20), S(-4, 43), S(35, 37), S(85, 9), S(69, -47), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 19), S(22, 37), S(-67, 83), S(69, 20), S(73, 86), S(0, 0),};

const Score ROOK_THREATS[6] = { S(1, 22), S(31, 43), S(31, 55), S(4, 22), S(82, 0), S(0, 0),};

const Score KING_THREAT = S(13, 34);

const Score PAWN_THREAT = S(84, 40);

const Score PAWN_PUSH_THREAT = S(19, 19);

const Score HANGING_THREAT = S(12, 22);

const Score KNIGHT_CHECK_QUEEN = S(12, 1);

const Score BISHOP_CHECK_QUEEN = S(22, 18);

const Score SPACE = 108;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(10, 25), S(0, 0),},
 { S(6, 20), S(-10, -31), S(0, 0),},
 { S(-1, 35), S(-33, -24), S(-4, -2), S(0, 0),},
 { S(56, 47), S(-69, 18), S(-11, 84), S(-206, 158), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-21, -4), S(21, 94), S(-2, 50), S(-13, 7), S(8, -5), S(37, -26), S(34, -49), S(0, 0),},
 { S(-44, 1), S(-9, 74), S(-22, 44), S(-31, 20), S(-27, 7), S(24, -19), S(32, -33), S(0, 0),},
 { S(-23, -12), S(-2, 88), S(-37, 34), S(-14, 7), S(-15, -4), S(-4, -9), S(29, -21), S(0, 0),},
 { S(-39, 17), S(15, 75), S(-61, 35), S(-31, 29), S(-29, 22), S(-17, 13), S(-26, 22), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-21, -12), S(-23, -3), S(-24, -8), S(-28, 3), S(-57, 29), S(58, 80), S(276, 72), S(0, 0),},
 { S(-13, -1), S(3, -7), S(3, -5), S(-12, 8), S(-27, 20), S(-31, 83), S(93, 129), S(0, 0),},
 { S(24, -4), S(26, -8), S(23, -5), S(7, 4), S(-15, 17), S(-47, 75), S(60, 95), S(0, 0),},
 { S(-3, -5), S(3, -12), S(15, -11), S(4, -12), S(-18, 4), S(-17, 47), S(-36, 119), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-2, -45), S(36, -52), S(18, -37), S(15, -32), S(4, -25), S(6, -84), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(43, -20);

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
Score MaterialValue(Board* board, int side) {
  Score s = S(0, 0);

  uint8_t eks = SQ_SIDE[lsb(board->pieces[KING[side ^ 1]])];

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

// Mobility + various piece specific mobility bonus
// Note: This method also builds up attack data critical for
// threat and king safety calculations
Score PieceEval(Board* board, EvalData* data, int side) {
  Score s = S(0, 0);

  int xside = side ^ 1;
  BitBoard enemyKingArea = data->kingArea[xside];
  BitBoard mob = data->mobilitySquares[side];
  BitBoard outposts = data->outposts[side];
  BitBoard myPawns = board->pieces[PAWN[side]];
  BitBoard opponentPawns = board->pieces[PAWN[xside]];
  BitBoard allPawns = myPawns | opponentPawns;

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

  for (int p = KNIGHT[side]; p <= KING[side]; p += 2) {
    BitBoard pieces = board->pieces[p];
    int pieceType = PIECE_TYPE[p];

    while (pieces) {
      BitBoard bb = pieces & -pieces;
      int sq = lsb(pieces);

      // Calculate a mobility bonus - bishops/rooks can see through queen/rook
      // because batteries shouldn't "limit" mobility
      // TODO: Consider queen behind rook acceptable?
      BitBoard movement = EMPTY;
      switch (pieceType) {
      case KNIGHT_TYPE:
        movement = GetKnightAttacks(sq);
        s += KNIGHT_MOBILITIES[bits(movement & mob)];

        if (T)
          C.knightMobilities[bits(movement & mob)] += cs[side];
        break;
      case BISHOP_TYPE:
        movement = GetBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]);
        s += BISHOP_MOBILITIES[bits(movement & mob)];

        if (T)
          C.bishopMobilities[bits(movement & mob)] += cs[side];
        break;
      case ROOK_TYPE:
        movement =
            GetRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[ROOK[side]] ^ board->pieces[QUEEN[side]]);
        s += ROOK_MOBILITIES[bits(movement & mob)];

        if (T)
          C.rookMobilities[bits(movement & mob)] += cs[side];
        break;
      case QUEEN_TYPE:
        movement = GetQueenAttacks(sq, board->occupancies[BOTH]);
        s += QUEEN_MOBILITIES[bits(movement & mob)];

        if (T)
          C.queenMobilities[bits(movement & mob)] += cs[side];
        break;
      case KING_TYPE:
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
  }

  return s;
}

// Threats bonus (piece attacks piece)
// TODO: Make this vary more based on piece type
Score Threats(Board* board, EvalData* data, int side) {
  Score s = S(0, 0);

  int xside = side ^ 1;

  BitBoard covered = data->attacks[xside][PAWN_TYPE] | (data->twoAttacks[xside] & ~data->twoAttacks[side]);
  BitBoard nonPawnEnemies = board->occupancies[xside] & ~board->pieces[PAWN[xside]];
  BitBoard weak = ~covered & data->allAttacks[side];

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

  s += bits(pawnPushAttacks) * PAWN_PUSH_THREAT;

  if (T) {
    C.pawnThreat += cs[side] * bits(pawnThreats);
    C.hangingThreat += cs[side] * bits(hangingPieces);
    C.pawnPushThreat += cs[side] * bits(pawnPushAttacks);
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

    if (T) {
      C.knightCheckQueen += cs[side] * bits(knightQueenHits);
      C.bishopCheckQueen += cs[side] * bits(bishopQueenHits);
    }
  }

  return s;
}

// King safety is a quadratic based - there is a summation of smaller values
// into a single score, then that value is squared and diveded by 1000
// This fit for KS is strong as it can ignore a single piece attacking, but will
// spike on a secondary piece joining.
// It is heavily influenced by Toga, Rebel, SF, and Ethereal
Score KingSafety(Board* board, EvalData* data, int side) {
  Score s = S(0, 0);
  Score shelter = S(0, 0);

  int xside = side ^ 1;

  BitBoard ourPawns = board->pieces[PAWN[side]] & ~data->attacks[xside][PAWN_TYPE] &
                      ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];
  BitBoard opponentPawns = board->pieces[PAWN[xside]] & ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];

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
Score Space(Board* board, EvalData* data, int side) {
  static const BitBoard CENTER_FILES = (C_FILE | D_FILE | E_FILE | F_FILE) & ~(RANK_1 | RANK_8);

  Score s = S(0, 0);
  int xside = side ^ 1;

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

Score Imbalance(Board* board, int side) {
  Score s = S(0, 0);
  int xside = side ^ 1;

  for (int i = KNIGHT_TYPE; i < KING_TYPE; i++) {
    for (int j = PAWN_TYPE; j < i; j++) {
      s += IMBALANCE[i][j] * bits(board->pieces[2 * i + side]) * bits(board->pieces[2 * j + xside]);

      if (T)
        C.imbalance[i][j] += bits(board->pieces[2 * i + side]) * bits(board->pieces[2 * j + xside]) * cs[side];
    }
  }

  return s;
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
    s += PieceEval(board, &data, WHITE) - PieceEval(board, &data, BLACK);
    s += PasserEval(board, &data, WHITE) - PasserEval(board, &data, BLACK);
    s += Threats(board, &data, WHITE) - Threats(board, &data, BLACK);
    s += KingSafety(board, &data, WHITE) - KingSafety(board, &data, BLACK);
    s += Space(board, &data, WHITE) - Space(board, &data, BLACK);
  }

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

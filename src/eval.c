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
 S( 120, 197), S( 110, 223), S(  79, 219), S( 113, 193),
 S(  82, 135), S(  61, 153), S(  80, 100), S(  85,  80),
 S(  79, 116), S(  79, 103), S(  65,  86), S(  61,  79),
 S(  74,  91), S(  71,  94), S(  67,  80), S(  74,  81),
 S(  67,  83), S(  68,  86), S(  67,  81), S(  69,  89),
 S(  75,  87), S(  81,  92), S(  80,  85), S(  87,  86),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 158, 152), S(  57, 157), S( 162, 149), S(  97, 196),
 S(  95,  82), S( 103,  74), S( 118,  49), S(  91,  67),
 S(  69,  79), S(  85,  73), S(  69,  64), S(  85,  68),
 S(  59,  71), S(  76,  76), S(  67,  69), S(  85,  73),
 S(  40,  71), S(  72,  72), S(  71,  74), S(  79,  85),
 S(  48,  81), S(  88,  83), S(  79,  82), S(  88,  98),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-188,  54), S(-197,  76), S(-168,  85), S(-128,  65),
 S(-118,  83), S( -98,  87), S( -73,  70), S( -69,  70),
 S( -59,  64), S( -66,  72), S( -69,  81), S( -81,  83),
 S( -70,  89), S( -65,  81), S( -53,  79), S( -64,  83),
 S( -70,  82), S( -64,  77), S( -50,  87), S( -59,  86),
 S( -91,  61), S( -81,  60), S( -72,  68), S( -74,  83),
 S(-102,  63), S( -85,  58), S( -82,  65), S( -75,  63),
 S(-124,  71), S( -87,  63), S( -87,  62), S( -78,  74),
},{
 S(-239, -96), S(-279,  49), S(-139,  34), S( -68,  60),
 S( -91,  16), S( -23,  39), S( -60,  69), S( -75,  71),
 S( -46,  33), S( -87,  50), S( -34,  29), S( -50,  63),
 S( -60,  65), S( -57,  62), S( -50,  68), S( -59,  80),
 S( -65,  78), S( -74,  69), S( -47,  81), S( -64,  94),
 S( -71,  56), S( -56,  52), S( -65,  60), S( -60,  79),
 S( -72,  57), S( -61,  56), S( -74,  58), S( -75,  61),
 S( -81,  66), S( -93,  65), S( -75,  57), S( -69,  71),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -20, 113), S( -42, 134), S( -31, 116), S( -44, 131),
 S(  -2, 118), S( -16, 104), S(  23, 109), S(   9, 121),
 S(  16, 121), S(  18, 121), S( -12, 102), S(  19, 111),
 S(  10, 119), S(  35, 113), S(  23, 117), S(  15, 133),
 S(  31, 103), S(  19, 116), S(  30, 115), S(  38, 113),
 S(  24,  96), S(  43, 108), S(  27,  93), S(  32, 109),
 S(  29,  92), S(  28,  65), S(  43,  83), S(  25, 103),
 S(  21,  73), S(  42,  87), S(  29, 107), S(  34, 104),
},{
 S( -60,  76), S(  46,  98), S( -38, 107), S( -64, 111),
 S(   4,  74), S( -32, 104), S(  -8, 120), S(  21, 102),
 S(  35, 107), S(   6, 109), S(   9, 104), S(  20, 112),
 S(   7, 103), S(  36, 107), S(  39, 111), S(  34, 113),
 S(  46,  81), S(  38, 104), S(  40, 113), S(  34, 114),
 S(  39,  86), S(  54,  97), S(  37,  93), S(  31, 120),
 S(  41,  68), S(  40,  67), S(  41,  98), S(  35, 103),
 S(  40,  50), S(  30, 123), S(  24, 112), S(  41, 101),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-107, 229), S( -97, 233), S( -95, 236), S(-124, 239),
 S( -93, 222), S( -98, 232), S( -81, 231), S( -66, 214),
 S(-109, 231), S( -74, 225), S( -85, 226), S( -85, 220),
 S(-103, 245), S( -88, 240), S( -76, 237), S( -72, 221),
 S(-104, 239), S( -98, 239), S( -94, 236), S( -87, 226),
 S(-108, 229), S( -98, 216), S( -93, 219), S( -93, 216),
 S(-104, 212), S( -99, 217), S( -86, 213), S( -83, 204),
 S(-105, 220), S( -98, 212), S( -95, 218), S( -88, 204),
},{
 S( -40, 212), S( -95, 238), S(-154, 249), S( -96, 229),
 S( -74, 204), S( -60, 215), S( -81, 219), S( -94, 220),
 S( -96, 208), S( -44, 203), S( -70, 195), S( -51, 197),
 S( -90, 216), S( -55, 211), S( -65, 200), S( -63, 211),
 S(-107, 213), S( -87, 214), S( -89, 208), S( -76, 214),
 S( -96, 188), S( -78, 187), S( -83, 191), S( -80, 203),
 S( -90, 188), S( -67, 176), S( -81, 184), S( -77, 194),
 S(-102, 201), S( -81, 194), S( -88, 194), S( -79, 194),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-106, 395), S( -26, 300), S(  14, 299), S(   7, 322),
 S( -28, 330), S(  -4, 291), S(   7, 321), S(   5, 321),
 S(   2, 304), S(  11, 292), S(   6, 320), S(  22, 319),
 S(  -4, 331), S(   4, 343), S(   9, 342), S(  -1, 347),
 S(   4, 305), S(  -9, 353), S(  -5, 342), S( -11, 363),
 S(  -4, 289), S(  -1, 305), S(  -4, 322), S(  -9, 323),
 S(   0, 257), S(  -3, 270), S(   3, 271), S(   3, 285),
 S( -15, 279), S( -15, 274), S( -12, 279), S( -10, 285),
},{
 S(  44, 282), S( 107, 257), S( -86, 383), S(  25, 284),
 S(  33, 275), S(  22, 262), S(  17, 306), S(   1, 331),
 S(  34, 270), S(  17, 260), S(  36, 285), S(  23, 315),
 S(   1, 310), S(  13, 314), S(  30, 282), S(   9, 339),
 S(   9, 282), S(   8, 307), S(  14, 310), S(  -6, 358),
 S(   5, 250), S(  12, 275), S(  12, 299), S(  -3, 321),
 S(  23, 213), S(  20, 216), S(  14, 241), S(   7, 282),
 S(  13, 245), S( -17, 263), S( -12, 251), S(  -5, 265),
}};

const Score KING_PSQT[2][32] = {{
 S( 232,-135), S( 130, -57), S( -19, -15), S( 110, -43),
 S( -96,  58), S( -32,  70), S(  -6,  57), S(  31,  24),
 S(  11,  44), S(  -2,  75), S(  27,  64), S( -11,  67),
 S(  15,  40), S(  13,  61), S( -10,  61), S( -22,  53),
 S( -27,  25), S(  18,  38), S(  32,  31), S( -30,  34),
 S( -25,  -2), S(   2,  14), S(  -2,  11), S( -14,   5),
 S(   6, -23), S( -21,  11), S( -24,  -4), S( -30, -13),
 S(  13, -81), S(  18, -38), S(   6, -37), S( -46, -35),
},{
 S( -26,-135), S(-296,  56), S(   9, -35), S( -48, -34),
 S(-211,  16), S( -90,  88), S( -80,  70), S( 144,  14),
 S(-111,  29), S(  41,  86), S( 146,  70), S(  79,  61),
 S( -95,  18), S(  15,  58), S(   3,  72), S(   4,  57),
 S( -80,   4), S( -49,  38), S(   5,  36), S( -10,  35),
 S(  -4, -14), S(  23,  11), S(   6,  12), S(  11,   7),
 S(  14, -15), S(   9,  11), S(  -7,   0), S( -22, -10),
 S(  16, -63), S(  17, -16), S(   3, -24), S( -31, -34),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -48,   9), S(   6,  19), S(  27,  23), S(  60,  35),
 S(  13,  -4), S(  38,  18), S(  29,  26), S(  41,  35),
 S(  21,  -4), S(  34,   9), S(  19,  15), S(  20,  23),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -6,   6), S(  23,   2), S(  57,  12), S(  61,   6),
 S(   1,  -2), S(  24,  11), S(  41,   3), S(  52,   8),
 S(  -7,  24), S(  42,   7), S(  30,  12), S(  38,  20),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-157, -32), S(-115,  42), S( -97,  82), S( -85,  95),
 S( -77, 109), S( -70, 121), S( -62, 124), S( -54, 126),
 S( -46, 117),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -9,  50), S(  14,  79), S(  27, 100), S(  33, 122),
 S(  39, 131), S(  43, 141), S(  44, 148), S(  48, 151),
 S(  48, 154), S(  51, 155), S(  58, 152), S(  70, 147),
 S(  71, 153), S(  82, 138),};

const Score ROOK_MOBILITIES[15] = {
 S( -85, -66), S( -96, 151), S( -81, 184), S( -76, 186),
 S( -79, 205), S( -75, 212), S( -80, 219), S( -76, 220),
 S( -73, 226), S( -68, 230), S( -66, 235), S( -70, 240),
 S( -67, 243), S( -61, 248), S( -49, 240),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1877,-1383), S(-126,-326), S( -77, 133), S( -55, 265),
 S( -43, 303), S( -37, 315), S( -37, 347), S( -36, 369),
 S( -33, 386), S( -30, 390), S( -27, 396), S( -23, 396),
 S( -23, 402), S( -19, 401), S( -19, 402), S( -12, 393),
 S( -13, 393), S( -14, 393), S(  -6, 372), S(  12, 343),
 S(  29, 308), S(  43, 269), S(  80, 234), S( 123, 145),
 S(  91, 153), S( 220,  21), S(  42,  38), S(  66, -10),
};

const Score MINOR_BEHIND_PAWN = S(6, 14);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-116, -244);

const Score ROOK_TRAPPED = S(-37, -27);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(23, 17);

const Score ROOK_OPEN_FILE = S(27, 16);

const Score ROOK_SEMI_OPEN = S(16, 6);

const Score ROOK_TO_OPEN = S(17, 18);

const Score QUEEN_OPPOSITE_ROOK = S(-14, 3);

const Score QUEEN_ROOK_BATTERY = S(3, 51);

const Score DEFENDED_PAWN = S(11, 10);

const Score DOUBLED_PAWN = S(16, -36);

const Score ISOLATED_PAWN[4] = {
 S(   1,  -5), S(  -1, -13), S(  -7,  -5), S(   1, -13),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -11);

const Score BACKWARDS_PAWN = S(-9, -17);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  87,  44), S(  27,  38), S(   9,  13),
 S(   6,   3), S(   5,   2), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 150, 166), S(  14,  75),
 S( -14,  61), S( -23,  39), S( -38,  16), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(5, -11);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 120, 186), S(  41, 184), S(  12, 114),
 S( -13,  75), S( -12,  41), S( -10,  37), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-6, 24);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S(  90, 260), S(  16, 137), S(   8,  47), S(  11,  10),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(25, -112);

const Score PASSED_PAWN_SQ_RULE = S(0, 356);

const Score KNIGHT_THREATS[6] = { S(0, 20), S(-4, 45), S(35, 37), S(85, 10), S(69, -48), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 19), S(22, 37), S(-66, 81), S(71, 20), S(60, 42), S(258, 301),};

const Score ROOK_THREATS[6] = { S(-1, 23), S(31, 43), S(33, 54), S(8, 21), S(44, -22), S(311, 335),};

const Score KING_THREAT = S(12, 35);

const Score PAWN_THREAT = S(83, 40);

const Score PAWN_PUSH_THREAT = S(19, 19);

const Score HANGING_THREAT = S(12, 22);

const Score KNIGHT_CHECK_QUEEN = S(13, 1);

const Score BISHOP_CHECK_QUEEN = S(22, 21);

const Score ROOK_CHECK_QUEEN = S(20, 7);

const Score SPACE = 110;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(10, 25), S(0, 0),},
 { S(6, 20), S(-9, -31), S(0, 0),},
 { S(1, 35), S(-34, -24), S(-5, -2), S(0, 0),},
 { S(54, 49), S(-67, 17), S(-12, 84), S(-202, 165), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-21, -4), S(23, 93), S(-3, 50), S(-15, 7), S(7, -5), S(37, -26), S(34, -49), S(0, 0),},
 { S(-44, 0), S(-6, 73), S(-21, 44), S(-31, 19), S(-27, 7), S(24, -19), S(32, -33), S(0, 0),},
 { S(-23, -12), S(3, 85), S(-37, 34), S(-14, 7), S(-15, -4), S(-4, -9), S(28, -20), S(0, 0),},
 { S(-37, 17), S(24, 72), S(-58, 35), S(-30, 29), S(-28, 23), S(-16, 14), S(-26, 22), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-19, -13), S(-24, -3), S(-24, -8), S(-28, 3), S(-57, 29), S(56, 81), S(273, 72), S(0, 0),},
 { S(-13, -1), S(3, -7), S(3, -5), S(-11, 8), S(-27, 20), S(-31, 83), S(96, 129), S(0, 0),},
 { S(24, -4), S(25, -8), S(23, -5), S(7, 4), S(-15, 17), S(-49, 76), S(60, 95), S(0, 0),},
 { S(-2, -5), S(2, -12), S(14, -10), S(2, -12), S(-19, 4), S(-18, 47), S(-37, 120), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-7, -43), S(36, -52), S(19, -37), S(16, -31), S(4, -25), S(6, -84), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(41, -19);

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

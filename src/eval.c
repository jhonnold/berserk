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

const Score BISHOP_PAIR = S(21, 87);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 118, 197), S( 109, 222), S(  80, 217), S( 114, 192),
 S(  80, 134), S(  59, 153), S(  79,  99), S(  84,  78),
 S(  78, 115), S(  77, 102), S(  64,  85), S(  59,  78),
 S(  73,  91), S(  68,  94), S(  65,  80), S(  72,  81),
 S(  66,  82), S(  65,  86), S(  63,  81), S(  66,  89),
 S(  74,  86), S(  78,  92), S(  76,  85), S(  82,  88),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 157, 152), S(  53, 157), S( 162, 148), S(  98, 194),
 S(  93,  82), S( 100,  73), S( 116,  49), S(  91,  67),
 S(  67,  78), S(  83,  73), S(  66,  64), S(  83,  67),
 S(  56,  71), S(  72,  76), S(  63,  69), S(  83,  73),
 S(  38,  70), S(  69,  72), S(  66,  75), S(  76,  85),
 S(  47,  81), S(  85,  83), S(  76,  83), S(  84,  99),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-188,  54), S(-193,  76), S(-163,  85), S(-120,  64),
 S(-116,  83), S( -97,  88), S( -73,  72), S( -68,  73),
 S( -59,  65), S( -65,  74), S( -68,  85), S( -80,  86),
 S( -70,  91), S( -65,  84), S( -53,  83), S( -63,  87),
 S( -71,  85), S( -65,  80), S( -51,  90), S( -59,  91),
 S( -92,  64), S( -82,  63), S( -74,  72), S( -75,  87),
 S(-103,  64), S( -86,  61), S( -83,  68), S( -78,  68),
 S(-125,  71), S( -90,  63), S( -89,  64), S( -81,  76),
},{
 S(-241, -95), S(-286,  50), S(-137,  35), S( -67,  60),
 S( -89,  16), S( -19,  39), S( -58,  70), S( -78,  75),
 S( -46,  35), S( -87,  52), S( -36,  34), S( -48,  66),
 S( -61,  67), S( -58,  66), S( -51,  72), S( -59,  83),
 S( -66,  81), S( -75,  72), S( -48,  84), S( -64,  97),
 S( -72,  58), S( -58,  55), S( -66,  61), S( -62,  83),
 S( -73,  59), S( -62,  58), S( -75,  61), S( -77,  63),
 S( -82,  65), S( -95,  67), S( -76,  58), S( -72,  73),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -23, 113), S( -41, 134), S( -29, 115), S( -44, 130),
 S(   1, 117), S( -16, 103), S(  24, 109), S(   9, 121),
 S(  18, 120), S(  19, 120), S( -12, 102), S(  23, 109),
 S(  11, 118), S(  35, 113), S(  25, 115), S(  16, 133),
 S(  31, 103), S(  22, 115), S(  29, 116), S(  39, 111),
 S(  26,  95), S(  43, 109), S(  28,  92), S(  31, 109),
 S(  29,  93), S(  28,  65), S(  42,  84), S(  26, 103),
 S(  22,  72), S(  40,  87), S(  29, 104), S(  33, 103),
},{
 S( -61,  76), S(  40,  99), S( -36, 106), S( -64, 110),
 S(   5,  73), S( -28, 103), S(  -9, 120), S(  22, 101),
 S(  39, 106), S(   8, 108), S(   9, 104), S(  22, 111),
 S(   7, 103), S(  35, 108), S(  38, 111), S(  35, 112),
 S(  47,  81), S(  37, 104), S(  41, 113), S(  34, 114),
 S(  39,  85), S(  55,  96), S(  36,  93), S(  32, 120),
 S(  42,  67), S(  40,  67), S(  42,  98), S(  35, 103),
 S(  40,  49), S(  30, 122), S(  23, 109), S(  41, 101),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -98, 236), S( -89, 241), S( -90, 245), S(-117, 247),
 S( -88, 230), S( -93, 240), S( -76, 239), S( -61, 222),
 S(-101, 238), S( -67, 232), S( -78, 234), S( -76, 227),
 S( -95, 251), S( -80, 247), S( -67, 245), S( -64, 228),
 S( -98, 246), S( -91, 246), S( -86, 244), S( -79, 233),
 S(-102, 236), S( -91, 223), S( -86, 227), S( -85, 224),
 S( -98, 220), S( -93, 225), S( -81, 222), S( -77, 213),
 S(-100, 229), S( -93, 221), S( -91, 227), S( -83, 213),
},{
 S( -36, 221), S( -87, 246), S(-145, 257), S( -90, 237),
 S( -65, 210), S( -53, 223), S( -76, 227), S( -89, 229),
 S( -91, 215), S( -35, 210), S( -63, 203), S( -44, 204),
 S( -84, 222), S( -50, 218), S( -58, 207), S( -56, 218),
 S(-102, 220), S( -82, 221), S( -83, 216), S( -69, 222),
 S( -90, 194), S( -72, 194), S( -77, 199), S( -73, 210),
 S( -85, 196), S( -62, 184), S( -75, 193), S( -72, 203),
 S( -98, 209), S( -77, 203), S( -83, 203), S( -75, 203),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-113, 404), S( -31, 307), S(   4, 308), S(  -6, 332),
 S( -44, 344), S( -34, 317), S( -19, 341), S( -26, 343),
 S( -13, 317), S(  -6, 305), S( -17, 339), S(   4, 330),
 S( -10, 336), S(  -6, 353), S(  -1, 349), S( -16, 359),
 S(   3, 306), S( -13, 357), S( -10, 349), S( -19, 373),
 S(  -3, 290), S(  -2, 307), S(  -5, 325), S( -10, 325),
 S(   1, 259), S(  -1, 271), S(   4, 272), S(   4, 285),
 S( -11, 279), S( -13, 275), S(  -9, 277), S(  -7, 285),
},{
 S(  35, 290), S(  94, 267), S(-102, 394), S(   7, 297),
 S(  26, 283), S(   7, 274), S( -13, 328), S( -31, 354),
 S(  28, 277), S(   3, 272), S(  20, 295), S(  -3, 332),
 S(  -1, 311), S(   7, 318), S(  24, 286), S(  -8, 353),
 S(   8, 284), S(   6, 308), S(  11, 311), S( -12, 365),
 S(   5, 251), S(  13, 273), S(  12, 300), S(  -3, 322),
 S(  26, 213), S(  22, 213), S(  17, 240), S(   9, 282),
 S(  16, 246), S( -14, 263), S(  -9, 247), S(  -2, 264),
}};

const Score KING_PSQT[2][32] = {{
 S( 226,-134), S( 134, -58), S( -19, -15), S( 104, -42),
 S( -96,  58), S( -32,  70), S(   1,  56), S(  36,  23),
 S(  12,  44), S(  -3,  75), S(  27,  64), S(  -9,  67),
 S(  19,  39), S(  12,  61), S(  -9,  61), S( -19,  53),
 S( -26,  24), S(  19,  38), S(  33,  31), S( -28,  34),
 S( -26,  -2), S(   1,  14), S(  -2,  11), S( -12,   5),
 S(   5, -23), S( -22,  11), S( -24,  -4), S( -29, -13),
 S(  12, -81), S(  18, -38), S(   7, -37), S( -48, -35),
},{
 S( -20,-136), S(-295,  55), S(   9, -35), S( -43, -35),
 S(-209,  16), S( -89,  87), S( -74,  69), S( 146,  14),
 S(-116,  29), S(  42,  86), S( 146,  70), S(  85,  60),
 S( -93,  17), S(  15,  58), S(   4,  72), S(   6,  57),
 S( -79,   4), S( -49,  38), S(   6,  36), S(  -8,  35),
 S(  -5, -15), S(  24,  11), S(   8,  12), S(  15,   7),
 S(  13, -15), S(   8,  11), S(  -5,   0), S( -19, -10),
 S(  16, -63), S(  17, -16), S(   5, -25), S( -32, -34),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -48,   9), S(   6,  19), S(  28,  23), S(  60,  35),
 S(  13,  -4), S(  39,  18), S(  29,  26), S(  41,  35),
 S(  21,  -3), S(  34,   9), S(  20,  15), S(  20,  23),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -7,   6), S(  22,   3), S(  58,  11), S(  61,   6),
 S(   1,  -2), S(  25,  11), S(  41,   3), S(  51,   9),
 S(  -7,  23), S(  43,   6), S(  31,  11), S(  37,  20),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-159, -25), S(-117,  48), S( -99,  86), S( -87, 100),
 S( -78, 113), S( -71, 125), S( -63, 127), S( -55, 128),
 S( -47, 119),};

const Score BISHOP_MOBILITIES[14] = {
 S( -11,  51), S(  13,  79), S(  25, 100), S(  32, 122),
 S(  39, 131), S(  43, 141), S(  45, 148), S(  49, 151),
 S(  50, 153), S(  53, 154), S(  61, 151), S(  74, 146),
 S(  74, 152), S(  84, 137),};

const Score ROOK_MOBILITIES[15] = {
 S( -82, -66), S( -96, 150), S( -81, 184), S( -74, 189),
 S( -76, 211), S( -72, 220), S( -77, 230), S( -72, 231),
 S( -68, 237), S( -63, 241), S( -60, 245), S( -62, 250),
 S( -58, 252), S( -50, 256), S( -37, 248),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1870,-1381), S(-125,-322), S( -76, 137), S( -54, 266),
 S( -42, 305), S( -37, 315), S( -38, 348), S( -37, 372),
 S( -35, 390), S( -32, 395), S( -30, 401), S( -27, 401),
 S( -28, 408), S( -25, 408), S( -27, 411), S( -21, 402),
 S( -24, 403), S( -26, 404), S( -20, 384), S(  -4, 355),
 S(  13, 320), S(  27, 280), S(  63, 246), S( 108, 155),
 S(  71, 166), S( 198,  34), S(  27,  49), S(  56,  -4),
};

const Score MINOR_BEHIND_PAWN = S(5, 14);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 6);

const Score BISHOP_TRAPPED = S(-120, -240);

const Score ROOK_TRAPPED = S(-41, -34);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(23, 17);

const Score ROOK_OPEN_FILE = S(29, 15);

const Score ROOK_SEMI_OPEN = S(16, 6);

const Score QUEEN_OPPOSITE_ROOK = S(-15, 3);

const Score QUEEN_ROOK_BATTERY = S(6, 52);

const Score DEFENDED_PAWN = S(11, 10);

const Score DOUBLED_PAWN = S(16, -36);

const Score ISOLATED_PAWN[4] = {
 S(   0,  -6), S(   0, -13), S(  -7,  -5), S(   1, -13),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -11);

const Score BACKWARDS_PAWN = S(-9, -17);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  83,  45), S(  27,  38), S(   9,  12),
 S(   6,   3), S(   5,   2), S(   2,   0), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 155, 168), S(  15,  75),
 S( -14,  61), S( -24,  39), S( -37,  15), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(5, -11);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 119, 186), S(  42, 183), S(  12, 113),
 S( -14,  75), S( -13,  42), S( -11,  37), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-6, 24);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S(  88, 261), S(  16, 138), S(   8,  47), S(  11,  10),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(26, -113);

const Score PASSED_PAWN_SQ_RULE = S(0, 357);

const Score KNIGHT_THREATS[6] = { S(1, 19), S(-5, 39), S(35, 37), S(86, 9), S(69, -50), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 19), S(23, 38), S(-64, 78), S(69, 21), S(80, 89), S(0, 0),};

const Score ROOK_THREATS[6] = { S(1, 22), S(32, 43), S(31, 55), S(3, 22), S(81, 0), S(0, 0),};

const Score KING_THREAT = S(13, 34);

const Score PAWN_THREAT = S(83, 40);

const Score PAWN_PUSH_THREAT = S(18, 19);

const Score HANGING_THREAT = S(11, 22);

const Score KNIGHT_CHECK_QUEEN = S(12, -4);

const Score SPACE = 107;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(10, 25), S(0, 0),},
 { S(6, 20), S(-9, -29), S(0, 0),},
 { S(-1, 35), S(-32, -21), S(-5, -2), S(0, 0),},
 { S(56, 47), S(-71, 18), S(-12, 79), S(-206, 156), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-21, -4), S(16, 95), S(-2, 50), S(-13, 7), S(8, -5), S(37, -26), S(34, -49), S(0, 0),},
 { S(-44, 1), S(-9, 73), S(-22, 44), S(-31, 20), S(-27, 7), S(24, -19), S(32, -33), S(0, 0),},
 { S(-23, -12), S(-3, 87), S(-37, 34), S(-13, 7), S(-15, -4), S(-4, -9), S(29, -21), S(0, 0),},
 { S(-39, 17), S(12, 75), S(-60, 35), S(-31, 29), S(-29, 22), S(-17, 13), S(-26, 22), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-21, -12), S(-23, -3), S(-24, -8), S(-28, 3), S(-56, 29), S(57, 80), S(275, 72), S(0, 0),},
 { S(-13, -1), S(3, -7), S(3, -5), S(-12, 8), S(-27, 20), S(-31, 82), S(93, 129), S(0, 0),},
 { S(24, -5), S(26, -8), S(23, -5), S(7, 4), S(-15, 17), S(-48, 75), S(62, 95), S(0, 0),},
 { S(-3, -5), S(3, -12), S(15, -11), S(4, -12), S(-18, 4), S(-17, 46), S(-36, 119), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(1, -44), S(36, -52), S(18, -37), S(16, -32), S(4, -25), S(5, -84), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(42, -20);

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

  BitBoard queenHits = GetKnightAttacks(lsb(board->pieces[QUEEN[xside]])) & data->attacks[side][KNIGHT_TYPE] &
                       ~board->pieces[PAWN[side]] & ~covered;
  s += bits(queenHits) * KNIGHT_CHECK_QUEEN;

  if (T) {
    C.pawnThreat += cs[side] * bits(pawnThreats);
    C.hangingThreat += cs[side] * bits(hangingPieces);
    C.pawnPushThreat += cs[side] * bits(pawnPushAttacks);
    C.knightCheckQueen += cs[side] * bits(queenHits);
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

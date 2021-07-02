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
 S(  70, 250), S(  65, 273), S(  40, 254), S(  72, 219),
 S(  21, 135), S(   7, 152), S(  36,  93), S(  43,  60),
 S(  16, 116), S(  19, 100), S(  22,  80), S(  20,  71),
 S(  12,  90), S(  12,  92), S(  19,  77), S(  27,  77),
 S(   7,  81), S(   8,  84), S(  12,  79), S(  15,  88),
 S(  17,  84), S(  22,  89), S(  18,  85), S(  24,  88),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 141, 196), S(  32, 199), S(  73, 210), S(  61, 213),
 S(  29,  87), S(  41,  80), S(  55,  59), S(  48,  50),
 S(   6,  78), S(  25,  72), S(  10,  69), S(  43,  61),
 S(  -3,  70), S(  15,  75), S(   3,  74), S(  38,  69),
 S( -19,  68), S(  12,  70), S(   4,  78), S(  24,  83),
 S( -10,  78), S(  28,  80), S(  12,  85), S(  26, 100),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-153,  46), S(-156,  69), S(-125,  77), S( -83,  59),
 S( -78,  72), S( -59,  77), S( -34,  64), S( -28,  64),
 S( -23,  55), S( -25,  67), S( -26,  76), S( -35,  78),
 S( -32,  83), S( -28,  80), S( -10,  76), S( -24,  81),
 S( -33,  80), S( -26,  75), S( -13,  86), S( -19,  84),
 S( -54,  61), S( -45,  62), S( -33,  69), S( -38,  83),
 S( -66,  66), S( -49,  61), S( -46,  69), S( -39,  66),
 S( -93,  74), S( -50,  59), S( -54,  63), S( -45,  74),
},{
 S(-180,-114), S(-244,  44), S( -99,  24), S( -31,  54),
 S( -48,   9), S(  19,  32), S( -13,  59), S( -35,  66),
 S(  -8,  28), S( -47,  44), S(   5,  25), S(  -2,  56),
 S( -26,  61), S( -18,  59), S( -13,  67), S( -16,  75),
 S( -28,  76), S( -38,  68), S(  -9,  79), S( -26,  92),
 S( -34,  56), S( -21,  54), S( -28,  63), S( -24,  79),
 S( -35,  60), S( -25,  57), S( -39,  61), S( -38,  62),
 S( -51,  73), S( -54,  59), S( -39,  58), S( -36,  72),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -12, 110), S( -28, 130), S( -14, 110), S( -34, 126),
 S(   6, 113), S(  -7, 101), S(  37, 103), S(  22, 115),
 S(  25, 116), S(  34, 114), S(  -2, 101), S(  36, 108),
 S(  23, 117), S(  46, 111), S(  37, 113), S(  26, 132),
 S(  41, 103), S(  33, 113), S(  41, 115), S(  50, 111),
 S(  38,  95), S(  54, 110), S(  40,  96), S(  44, 110),
 S(  41,  96), S(  41,  68), S(  53,  87), S(  38, 106),
 S(  33,  73), S(  52,  89), S(  40, 105), S(  41, 106),
},{
 S( -38,  69), S(  57,  94), S( -25, 103), S( -51, 103),
 S(  19,  67), S(  -9,  95), S(   7, 114), S(  35,  95),
 S(  45, 104), S(  17, 106), S(  22, 100), S(  34, 108),
 S(  16, 102), S(  48, 106), S(  48, 110), S(  46, 112),
 S(  59,  80), S(  48, 102), S(  53, 113), S(  43, 114),
 S(  50,  86), S(  67,  98), S(  48,  96), S(  45, 120),
 S(  53,  72), S(  52,  70), S(  54, 100), S(  47, 106),
 S(  48,  53), S(  44, 121), S(  36, 108), S(  51, 103),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -89, 233), S( -80, 236), S( -80, 241), S(-107, 243),
 S( -75, 220), S( -81, 232), S( -60, 230), S( -44, 215),
 S( -89, 224), S( -53, 219), S( -64, 221), S( -59, 214),
 S( -83, 237), S( -66, 233), S( -53, 231), S( -49, 215),
 S( -89, 236), S( -80, 237), S( -74, 234), S( -66, 224),
 S( -92, 228), S( -81, 217), S( -75, 220), S( -72, 217),
 S( -90, 219), S( -82, 221), S( -69, 218), S( -66, 210),
 S( -81, 224), S( -81, 220), S( -78, 225), S( -69, 211),
},{
 S( -18, 215), S( -71, 239), S(-136, 251), S( -76, 232),
 S( -48, 199), S( -36, 214), S( -60, 218), S( -74, 221),
 S( -80, 203), S( -24, 197), S( -50, 188), S( -29, 192),
 S( -74, 209), S( -36, 203), S( -44, 192), S( -42, 205),
 S( -92, 209), S( -71, 212), S( -71, 207), S( -58, 213),
 S( -81, 189), S( -61, 189), S( -65, 191), S( -61, 205),
 S( -80, 198), S( -52, 182), S( -64, 190), S( -60, 200),
 S( -70, 198), S( -69, 204), S( -70, 202), S( -62, 203),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -99, 386), S( -21, 295), S(  13, 296), S(   2, 323),
 S( -34, 326), S( -27, 308), S(  -8, 329), S( -19, 335),
 S(  -5, 302), S(   5, 288), S(  -9, 328), S(  12, 322),
 S(   0, 323), S(   4, 341), S(   8, 340), S( -11, 356),
 S(   8, 304), S(  -3, 349), S(  -1, 342), S(  -7, 363),
 S(   6, 284), S(  12, 300), S(   8, 316), S(   3, 319),
 S(  12, 255), S(  13, 264), S(  20, 266), S(  19, 278),
 S(   1, 274), S(   2, 266), S(   5, 274), S(  11, 277),
},{
 S(  58, 262), S( 124, 242), S( -93, 377), S(  15, 288),
 S(  41, 265), S(  20, 266), S(  -1, 310), S( -25, 345),
 S(  38, 263), S(  13, 255), S(  29, 279), S(   8, 321),
 S(  10, 298), S(  21, 300), S(  31, 275), S(  -4, 345),
 S(  26, 266), S(  17, 298), S(  17, 307), S(  -4, 359),
 S(  20, 237), S(  27, 264), S(  20, 296), S(   7, 318),
 S(  39, 210), S(  38, 207), S(  32, 231), S(  23, 277),
 S(  33, 241), S(   3, 254), S(   5, 238), S(  13, 258),
}};

const Score KING_PSQT[2][32] = {{
 S( 189,-110), S( 146, -49), S(  13, -18), S(  83, -30),
 S( -49,  41), S(  49,  45), S(  28,  43), S(  38,  16),
 S(  62,  21), S(  81,  44), S(  68,  44), S(  16,  49),
 S(  68,  21), S(  48,  44), S(  12,  49), S( -31,  47),
 S(  -7,  15), S(  22,  34), S(  24,  28), S( -64,  37),
 S( -22,  -1), S(   2,  17), S( -13,  14), S( -35,  11),
 S(   9, -20), S( -16,  14), S( -35,   1), S( -52,  -6),
 S(  16, -78), S(  24, -36), S(  -2, -33), S( -27, -48),
},{
 S(  15,-128), S(-229,  58), S( -13, -18), S( -92, -14),
 S(-117,   2), S(  -5,  69), S( -62,  61), S( 126,  15),
 S( -28,   8), S( 121,  60), S( 179,  51), S(  87,  45),
 S( -58,   8), S(  42,  47), S(   8,  62), S( -32,  57),
 S( -72,   2), S( -49,  37), S(  -6,  35), S( -47,  40),
 S(  -9, -10), S(  19,  15), S( -10,  17), S( -19,  15),
 S(   9, -10), S(   4,  16), S( -28,   8), S( -51,  -1),
 S(  11, -59), S(  15, -12), S( -12, -21), S(  -9, -51),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -46,   9), S(   7,  20), S(  30,  23), S(  59,  39),
 S(  15,  -2), S(  40,  19), S(  30,  26), S(  42,  37),
 S(  21,  -3), S(  33,  10), S(  22,  13), S(  20,  23),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -1,   6), S(  23,   5), S(  59,  13), S(  62,   8),
 S(   4,  -4), S(  26,  12), S(  41,   6), S(  52,  11),
 S(  -7,  22), S(  45,   8), S(  31,  11), S(  38,  21),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-111, -30), S( -70,  43), S( -52,  82), S( -40,  96),
 S( -31, 108), S( -23, 119), S( -14, 121), S(  -6, 123),
 S(   2, 116),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -1,  55), S(  22,  78), S(  35,  99), S(  42, 119),
 S(  49, 127), S(  54, 137), S(  56, 144), S(  60, 147),
 S(  61, 149), S(  64, 151), S(  72, 148), S(  85, 143),
 S(  87, 149), S(  91, 142),};

const Score ROOK_MOBILITIES[15] = {
 S( -88, -37), S( -84, 149), S( -68, 184), S( -60, 190),
 S( -62, 212), S( -60, 221), S( -65, 231), S( -61, 231),
 S( -55, 233), S( -50, 235), S( -46, 237), S( -46, 240),
 S( -43, 242), S( -32, 243), S( -15, 235),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1685,-1324), S(-105,-389), S( -56, 124), S( -33, 253),
 S( -23, 300), S( -17, 312), S( -17, 341), S( -16, 364),
 S( -13, 380), S( -10, 382), S(  -8, 388), S(  -5, 387),
 S(  -6, 393), S(  -2, 392), S(  -4, 393), S(   2, 384),
 S(   0, 382), S(  -1, 380), S(   7, 357), S(  25, 326),
 S(  45, 288), S(  59, 250), S( 100, 209), S( 142, 122),
 S( 114, 122), S( 238,  -7), S(  76,  11), S(  84, -36),
};

const Score MINOR_BEHIND_PAWN = S(4, 17);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 20);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 8);

const Score BISHOP_TRAPPED = S(-113, -236);

const Score ROOK_TRAPPED = S(-55, -26);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(23, 16);

const Score ROOK_OPEN_FILE = S(27, 20);

const Score ROOK_SEMI_OPEN = S(13, 15);

const Score DEFENDED_PAWN = S(11, 11);

const Score DOUBLED_PAWN = S(12, -34);

const Score ISOLATED_PAWN[4] = {
 S(   1,  -6), S(  -1, -13), S(  -7,  -7), S(  -2, -13),
};

const Score OPEN_ISOLATED_PAWN = S(-4, -9);

const Score BACKWARDS_PAWN = S(-9, -18);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  78,  48), S(  28,  39), S(  11,  11),
 S(   6,   2), S(   5,   1), S(   2,   0), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 122, 155), S(   8,  80),
 S( -12,  60), S( -22,  36), S( -35,  12), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -10);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  59, 256), S(  12, 284), S(  -4, 142),
 S( -22,  74), S(  -9,  44), S(  -9,  40), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(2, -12);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 26);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(21, 35);

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(-4, -64);

const Score KNIGHT_THREATS[6] = { S(0, 19), S(-2, 47), S(29, 33), S(76, 0), S(50, -55), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 19), S(21, 35), S(-60, 87), S(57, 12), S(62, 86), S(0, 0),};

const Score ROOK_THREATS[6] = { S(-1, 23), S(30, 39), S(29, 52), S(3, 22), S(75, -4), S(0, 0),};

const Score KING_THREAT = S(14, 32);

const Score PAWN_THREAT = S(72, 30);

const Score PAWN_PUSH_THREAT = S(18, 19);

const Score HANGING_THREAT = S(5, 8);

const Score SPACE[15] = {
 S( 552, -20), S( 144, -14), S(  64, -10), S(  24,  -6), S(   4,  -2),
 S(  -5,   0), S(  -9,   3), S( -10,   6), S(  -9,   9), S(  -7,  12),
 S(  -4,  14), S(  -2,  19), S(   1,  18), S(   3,  14), S(   5,-322),
};

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(6, 22), S(0, 0),},
 { S(2, 18), S(-4, -25), S(0, 0),},
 { S(-7, 29), S(-24, -7), S(-6, 5), S(0, 0),},
 { S(35, 36), S(-31, 42), S(4, 88), S(-197, 172), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-24, -5), S(19, 71), S(-11, 51), S(-15, 9), S(5, -6), S(33, -26), S(30, -49), S(0, 0),},
 { S(-42, 0), S(11, 62), S(-21, 46), S(-31, 21), S(-26, 6), S(24, -18), S(31, -31), S(0, 0),},
 { S(-23, -11), S(14, 68), S(-38, 38), S(-12, 9), S(-14, -4), S(-5, -8), S(29, -19), S(0, 0),},
 { S(-29, 15), S(17, 73), S(-58, 43), S(-20, 30), S(-18, 21), S(-8, 12), S(-17, 18), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-21, -13), S(-24, -4), S(-25, -10), S(-29, 2), S(-59, 39), S(36, 163), S(270, 180), S(0, 0),},
 { S(-12, -3), S(2, -7), S(2, -7), S(-11, 5), S(-28, 25), S(-50, 166), S(87, 235), S(0, 0),},
 { S(40, -15), S(37, -17), S(26, -10), S(8, 0), S(-15, 23), S(-67, 142), S(-19, 204), S(0, 0),},
 { S(-2, -7), S(5, -17), S(16, -12), S(8, -15), S(-18, 11), S(-35, 114), S(-103, 216), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-4, -24), S(42, -54), S(20, -42), S(17, -35), S(4, -25), S(3, -75), S(0, 0), S(0, 0),
};

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

const Score TEMPO = 20;
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
  BitBoard whitePawns = board->pieces[PAWN_WHITE];
  BitBoard blackPawns = board->pieces[PAWN_BLACK];
  BitBoard whitePawnAttacks = ShiftNE(whitePawns) | ShiftNW(whitePawns);
  BitBoard blackPawnAttacks = ShiftSE(blackPawns) | ShiftSW(blackPawns);

  data->allAttacks[WHITE] = data->attacks[WHITE][PAWN_TYPE] = whitePawnAttacks;
  data->allAttacks[BLACK] = data->attacks[BLACK][PAWN_TYPE] = blackPawnAttacks;
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

  BitBoard nonPawnEnemies = board->occupancies[xside] & ~board->pieces[PAWN[xside]];
  BitBoard weak =
      ~(data->attacks[xside][PAWN_TYPE] | (data->twoAttacks[xside] & ~data->twoAttacks[side])) & data->allAttacks[side];

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
  Score s = S(0, 0);
  int xside = side ^ 1;

  static const BitBoard CENTER_FILES = (C_FILE | D_FILE | E_FILE | F_FILE);

  BitBoard space = side == WHITE ? ShiftS(board->pieces[PAWN[side]]) : ShiftN(board->pieces[PAWN[side]]);
  space |= side == WHITE ? (ShiftS(space) | ShiftSS(space)) : (ShiftN(space) | ShiftNN(space));
  space &= ~(board->pieces[PAWN[side]] | data->attacks[xside][PAWN_TYPE]);
  space &= CENTER_FILES;

  s += SPACE[bits(board->occupancies[side]) - 2] * bits(space);

  if (T)
    C.space[bits(board->occupancies[side]) - 2] += cs[side] * bits(space);

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

  EvalData data = {0};
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

  if (T || abs(scoreMG(s) + scoreEG(s)) / 2 < 640) {
    s += PieceEval(board, &data, WHITE) - PieceEval(board, &data, BLACK);
    s += PasserEval(board, &data, WHITE) - PasserEval(board, &data, BLACK);
    s += Threats(board, &data, WHITE) - Threats(board, &data, BLACK);
    s += KingSafety(board, &data, WHITE) - KingSafety(board, &data, BLACK);
    s += Space(board, &data, WHITE) - Space(board, &data, BLACK);
  }

  // taper
  int phase = GetPhase(board);
  Score res = (phase * scoreMG(s) + (128 - phase) * scoreEG(s)) / 128;

  if (T)
    C.ss = res >= 0 ? WHITE : BLACK;

  // scale the score
  res = (res * Scale(board, res >= 0 ? WHITE : BLACK)) / MAX_SCALE;
  return TEMPO + (board->side == WHITE ? res : -res);
}

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
 S(  84, 243), S(  76, 264), S(  54, 244), S(  86, 208),
 S(  47, 134), S(  32, 149), S(  62,  90), S(  70,  58),
 S(  43, 112), S(  45,  98), S(  49,  77), S(  47,  66),
 S(  39,  86), S(  38,  90), S(  45,  74), S(  54,  73),
 S(  34,  78), S(  34,  82), S(  39,  76), S(  41,  84),
 S(  43,  81), S(  48,  88), S(  44,  83), S(  51,  86),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 149, 186), S(  40, 189), S( 100, 191), S(  73, 204),
 S(  57,  80), S(  67,  73), S(  82,  53), S(  74,  48),
 S(  33,  72), S(  51,  68), S(  37,  63), S(  70,  57),
 S(  24,  65), S(  41,  72), S(  30,  70), S(  64,  65),
 S(   7,  65), S(  38,  68), S(  31,  76), S(  51,  80),
 S(  17,  75), S(  54,  79), S(  38,  83), S(  52,  97),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-175,  45), S(-177,  67), S(-147,  77), S(-107,  59),
 S(-101,  71), S( -81,  76), S( -57,  64), S( -50,  64),
 S( -46,  53), S( -47,  63), S( -49,  75), S( -57,  75),
 S( -54,  77), S( -49,  75), S( -32,  73), S( -46,  76),
 S( -55,  76), S( -48,  72), S( -35,  84), S( -41,  80),
 S( -75,  56), S( -67,  59), S( -55,  66), S( -60,  81),
 S( -87,  60), S( -71,  55), S( -68,  64), S( -61,  63),
 S(-115,  69), S( -72,  55), S( -76,  59), S( -66,  69),
},{
 S(-203,-114), S(-274,  45), S(-123,  25), S( -55,  55),
 S( -69,   6), S(  -2,  29), S( -35,  57), S( -58,  66),
 S( -30,  24), S( -69,  41), S( -17,  23), S( -26,  54),
 S( -48,  56), S( -40,  55), S( -35,  63), S( -38,  71),
 S( -50,  72), S( -60,  65), S( -31,  76), S( -48,  89),
 S( -56,  52), S( -43,  51), S( -50,  60), S( -46,  77),
 S( -57,  56), S( -47,  52), S( -60,  56), S( -60,  59),
 S( -73,  68), S( -76,  55), S( -61,  54), S( -58,  67),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -17, 108), S( -33, 128), S( -19, 110), S( -39, 125),
 S(   2, 110), S( -11, 100), S(  32, 103), S(  17, 115),
 S(  21, 114), S(  30, 113), S(  -6,  99), S(  31, 106),
 S(  19, 112), S(  42, 108), S(  33, 110), S(  22, 129),
 S(  37, 101), S(  29, 111), S(  37, 113), S(  46, 109),
 S(  34,  92), S(  50, 106), S(  36,  92), S(  40, 107),
 S(  36,  93), S(  37,  64), S(  49,  83), S(  34, 103),
 S(  28,  70), S(  48,  85), S(  36, 102), S(  37, 102),
},{
 S( -42,  67), S(  52,  92), S( -29, 101), S( -57, 104),
 S(  15,  63), S( -14,  94), S(   3, 113), S(  31,  94),
 S(  41, 100), S(  12, 104), S(  18,  98), S(  29, 106),
 S(  12,  97), S(  44, 104), S(  44, 107), S(  42, 109),
 S(  55,  76), S(  43, 101), S(  49, 111), S(  39, 112),
 S(  46,  83), S(  63,  94), S(  44,  93), S(  41, 117),
 S(  48,  69), S(  48,  67), S(  50,  96), S(  43, 103),
 S(  43,  50), S(  40, 116), S(  32, 105), S(  47,  99),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-112, 232), S(-103, 235), S(-102, 238), S(-128, 239),
 S( -97, 219), S(-102, 230), S( -81, 228), S( -66, 213),
 S(-110, 223), S( -73, 218), S( -84, 219), S( -80, 211),
 S(-103, 234), S( -87, 231), S( -73, 228), S( -69, 212),
 S(-109, 230), S( -99, 231), S( -93, 228), S( -85, 217),
 S(-111, 222), S(-101, 212), S( -94, 214), S( -91, 210),
 S(-110, 214), S(-102, 217), S( -89, 213), S( -85, 205),
 S(-101, 220), S(-101, 216), S( -99, 221), S( -89, 206),
},{
 S( -39, 213), S( -91, 236), S(-156, 247), S( -98, 228),
 S( -70, 198), S( -58, 212), S( -80, 215), S( -94, 219),
 S(-101, 201), S( -44, 196), S( -70, 188), S( -49, 189),
 S( -94, 206), S( -57, 203), S( -64, 191), S( -62, 203),
 S(-112, 204), S( -90, 206), S( -91, 201), S( -77, 207),
 S(-101, 182), S( -81, 184), S( -85, 187), S( -81, 199),
 S( -99, 192), S( -72, 177), S( -84, 186), S( -80, 195),
 S( -90, 193), S( -89, 200), S( -90, 199), S( -82, 198),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-107, 383), S( -31, 291), S(   3, 293), S(  -8, 320),
 S( -43, 321), S( -35, 302), S( -17, 325), S( -27, 329),
 S( -13, 295), S(  -4, 284), S( -17, 320), S(   5, 311),
 S(  -8, 313), S(  -4, 331), S(   0, 331), S( -19, 344),
 S(   1, 295), S( -11, 339), S(  -9, 331), S( -14, 351),
 S(  -2, 274), S(   4, 290), S(   0, 305), S(  -4, 307),
 S(   4, 246), S(   5, 254), S(  12, 255), S(  12, 267),
 S(  -7, 265), S(  -6, 257), S(  -3, 264), S(   3, 267),
},{
 S(  49, 260), S( 113, 239), S(-102, 371), S(   4, 284),
 S(  33, 260), S(  11, 261), S( -10, 305), S( -34, 339),
 S(  30, 257), S(   5, 250), S(  20, 273), S(   0, 313),
 S(   2, 290), S(  13, 293), S(  23, 267), S( -13, 337),
 S(  18, 257), S(  10, 289), S(   9, 296), S( -12, 347),
 S(  12, 227), S(  19, 254), S(  13, 286), S(   0, 306),
 S(  31, 201), S(  30, 197), S(  24, 220), S(  15, 266),
 S(  25, 231), S(  -5, 245), S(  -3, 229), S(   5, 248),
}};

const Score KING_PSQT[2][32] = {{
 S( 212,-118), S( 154, -51), S(  11, -19), S(  93, -36),
 S( -46,  42), S(  41,  51), S(  28,  45), S(  42,  16),
 S(  61,  26), S(  75,  52), S(  73,  48), S(  16,  52),
 S(  70,  21), S(  45,  49), S(  12,  51), S( -32,  48),
 S(  -4,  15), S(  23,  35), S(  24,  29), S( -62,  37),
 S( -20,  -4), S(   4,  14), S( -13,  12), S( -36,   9),
 S(   9, -22), S( -16,  12), S( -35,   0), S( -52,  -7),
 S(  16, -79), S(  24, -37), S(  -2, -34), S( -27, -48),
},{
 S(  28,-128), S(-237,  60), S(   1, -22), S( -84, -19),
 S(-121,   5), S(  -9,  73), S( -46,  59), S( 132,  13),
 S( -26,   9), S( 123,  64), S( 189,  51), S(  94,  46),
 S( -56,   7), S(  40,  49), S(   8,  63), S( -23,  56),
 S( -70,   1), S( -49,  38), S(  -6,  36), S( -45,  41),
 S(  -8, -11), S(  19,  14), S( -11,  17), S( -19,  14),
 S(   9, -11), S(   4,  15), S( -28,   7), S( -51,  -2),
 S(  11, -60), S(  15, -12), S( -12, -21), S(  -9, -49),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -46,  11), S(   6,  22), S(  29,  25), S(  59,  40),
 S(  14,   1), S(  39,  20), S(  29,  29), S(  41,  40),
 S(  22,  -5), S(  34,   7), S(  22,  11), S(  20,  22),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -2,   9), S(  23,   6), S(  58,  15), S(  62,  10),
 S(   2,   1), S(  26,  13), S(  40,   8), S(  51,  13),
 S(  -5,  14), S(  46,   5), S(  31,   8), S(  38,  18),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-138, -31), S( -97,  42), S( -79,  80), S( -67,  93),
 S( -57, 105), S( -49, 115), S( -41, 117), S( -32, 118),
 S( -24, 109),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -8,  53), S(  16,  76), S(  29,  97), S(  36, 117),
 S(  43, 125), S(  47, 134), S(  49, 140), S(  54, 143),
 S(  54, 145), S(  58, 146), S(  66, 142), S(  80, 136),
 S(  81, 143), S(  87, 132),};

const Score ROOK_MOBILITIES[15] = {
 S( -97, -53), S( -99, 144), S( -84, 178), S( -76, 183),
 S( -77, 206), S( -75, 215), S( -81, 226), S( -77, 226),
 S( -71, 229), S( -66, 232), S( -62, 234), S( -63, 238),
 S( -59, 240), S( -50, 242), S( -33, 234),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1717,-1334), S(-115,-406), S( -67, 110), S( -45, 237),
 S( -34, 284), S( -28, 296), S( -28, 325), S( -27, 348),
 S( -24, 364), S( -21, 366), S( -19, 372), S( -16, 372),
 S( -17, 377), S( -13, 376), S( -15, 378), S(  -9, 369),
 S( -11, 368), S( -12, 366), S(  -5, 344), S(  13, 314),
 S(  32, 277), S(  47, 238), S(  88, 198), S( 129, 112),
 S( 100, 114), S( 221, -13), S(  62,   1), S(  58, -36),
};

const Score MINOR_BEHIND_PAWN = S(5, 15);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-113, -239);

const Score ROOK_TRAPPED = S(-55, -24);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(23, 17);

const Score ROOK_OPEN_FILE = S(27, 19);

const Score ROOK_SEMI_OPEN = S(14, 10);

const Score DEFENDED_PAWN = S(11, 11);

const Score DOUBLED_PAWN = S(11, -33);

const Score ISOLATED_PAWN[4] = {
 S(   1,  -5), S(  -1, -14), S(  -7,  -7), S(  -1, -12),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -9);

const Score BACKWARDS_PAWN = S(-9, -17);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  78,  49), S(  28,  40), S(  11,  12),
 S(   6,   2), S(   5,   2), S(   2,   0), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 121, 146), S(   9,  78),
 S( -13,  60), S( -23,  37), S( -36,  13), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -11);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  86, 234), S(  26, 245), S(   7, 106),
 S(  -9,  37), S( -10,  42), S( -10,  39), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(2, -10);

const Score PASSED_PAWN_KING_PROXIMITY = S(-7, 23);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(5, 61);

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(18, -91);

const Score KNIGHT_THREATS[6] = { S(0, 19), S(-2, 44), S(29, 32), S(77, -1), S(50, -57), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(1, 19), S(21, 33), S(-60, 86), S(57, 11), S(62, 85), S(0, 0),};

const Score ROOK_THREATS[6] = { S(-1, 22), S(30, 40), S(29, 52), S(3, 24), S(75, -4), S(0, 0),};

const Score KING_THREAT = S(16, 28);

const Score PAWN_THREAT = S(72, 30);

const Score PAWN_PUSH_THREAT = S(19, 17);

const Score HANGING_THREAT = S(5, 8);

const Score SPACE[15] = {
 S( 514, -21), S( 143, -14), S(  63, -11), S(  24,  -7), S(   4,  -3),
 S(  -5,   0), S(  -9,   3), S( -10,   6), S(  -9,   9), S(  -7,  12),
 S(  -4,  14), S(  -2,  20), S(   1,  18), S(   3,  13), S(   5,-331),
};

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(9, 24), S(0, 0),},
 { S(4, 20), S(-8, -28), S(0, 0),},
 { S(-2, 33), S(-27, -14), S(-2, 4), S(0, 0),},
 { S(43, 44), S(-47, 31), S(0, 88), S(-204, 166), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-24, -6), S(16, 76), S(-10, 50), S(-16, 8), S(5, -5), S(33, -26), S(30, -49), S(0, 0),},
 { S(-42, -1), S(9, 65), S(-20, 45), S(-30, 21), S(-26, 6), S(24, -18), S(31, -31), S(0, 0),},
 { S(-23, -11), S(13, 73), S(-37, 38), S(-12, 9), S(-14, -4), S(-5, -8), S(29, -19), S(0, 0),},
 { S(-29, 15), S(15, 76), S(-56, 41), S(-20, 30), S(-18, 21), S(-8, 12), S(-17, 19), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-22, -11), S(-25, -3), S(-25, -8), S(-28, 1), S(-56, 30), S(42, 135), S(272, 162), S(0, 0),},
 { S(-12, -1), S(2, -6), S(2, -5), S(-11, 6), S(-26, 20), S(-45, 140), S(90, 218), S(0, 0),},
 { S(40, -12), S(36, -13), S(26, -7), S(8, 1), S(-14, 17), S(-62, 124), S(2, 184), S(0, 0),},
 { S(-2, -4), S(5, -15), S(16, -11), S(8, -17), S(-15, 1), S(-29, 92), S(-92, 203), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-4, -25), S(41, -51), S(20, -39), S(17, -34), S(5, -26), S(4, -80), S(0, 0), S(0, 0),
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

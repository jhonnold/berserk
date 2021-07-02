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
 S(  99, 205), S(  93, 227), S(  72, 219), S( 107, 190),
 S(  58, 130), S(  45, 145), S(  75,  90), S(  83,  66),
 S(  55, 112), S(  57,  97), S(  61,  76), S(  60,  66),
 S(  51,  86), S(  51,  88), S(  58,  72), S(  67,  71),
 S(  46,  77), S(  47,  81), S(  52,  75), S(  54,  83),
 S(  55,  81), S(  61,  87), S(  57,  83), S(  63,  86),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 138, 160), S(  47, 161), S(  88, 164), S(  91, 188),
 S(  69,  77), S(  81,  69), S(  96,  51), S(  87,  55),
 S(  46,  72), S(  64,  67), S(  50,  62), S(  82,  56),
 S(  36,  65), S(  54,  71), S(  43,  68), S(  77,  63),
 S(  20,  64), S(  51,  66), S(  45,  73), S(  64,  79),
 S(  29,  75), S(  67,  78), S(  51,  83), S(  64,  97),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-184,  45), S(-184,  66), S(-156,  75), S(-115,  57),
 S(-109,  69), S( -89,  74), S( -66,  63), S( -60,  63),
 S( -54,  52), S( -56,  62), S( -58,  73), S( -66,  74),
 S( -63,  78), S( -58,  75), S( -41,  71), S( -54,  75),
 S( -63,  74), S( -56,  69), S( -44,  82), S( -49,  80),
 S( -84,  55), S( -76,  56), S( -64,  63), S( -68,  78),
 S( -95,  55), S( -79,  52), S( -76,  60), S( -69,  60),
 S(-121,  60), S( -80,  53), S( -84,  55), S( -75,  66),
},{
 S(-213,-114), S(-280,  43), S(-135,  26), S( -65,  54),
 S( -79,   4), S( -12,  28), S( -44,  57), S( -67,  65),
 S( -39,  24), S( -78,  41), S( -27,  23), S( -35,  53),
 S( -57,  56), S( -49,  54), S( -44,  62), S( -46,  70),
 S( -58,  70), S( -69,  63), S( -40,  75), S( -56,  88),
 S( -64,  50), S( -51,  49), S( -58,  57), S( -54,  74),
 S( -65,  52), S( -55,  49), S( -69,  53), S( -68,  56),
 S( -80,  58), S( -84,  53), S( -69,  50), S( -67,  65),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -21, 105), S( -37, 126), S( -24, 108), S( -44, 124),
 S(  -2, 108), S( -15,  97), S(  28, 101), S(  12, 114),
 S(  17, 112), S(  26, 111), S( -10,  96), S(  28, 103),
 S(  15, 110), S(  38, 106), S(  28, 109), S(  18, 127),
 S(  33,  97), S(  25, 109), S(  33, 111), S(  42, 107),
 S(  30,  90), S(  46, 104), S(  32,  88), S(  36, 104),
 S(  33,  87), S(  34,  61), S(  45,  79), S(  30,  98),
 S(  25,  64), S(  44,  80), S(  32,  99), S(  34,  99),
},{
 S( -46,  64), S(  47,  91), S( -34,  99), S( -62, 103),
 S(  11,  61), S( -19,  93), S(  -2, 112), S(  25,  94),
 S(  37,  98), S(   8, 102), S(  14,  95), S(  25, 104),
 S(   8,  96), S(  40, 101), S(  40, 105), S(  38, 106),
 S(  51,  73), S(  39,  99), S(  45, 108), S(  35, 110),
 S(  42,  81), S(  59,  91), S(  40,  90), S(  37, 114),
 S(  45,  62), S(  44,  64), S(  46,  93), S(  39,  99),
 S(  40,  44), S(  37, 110), S(  28, 103), S(  43,  95),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-114, 225), S(-106, 230), S(-106, 233), S(-134, 235),
 S(-103, 217), S(-108, 227), S( -87, 225), S( -72, 210),
 S(-117, 224), S( -80, 218), S( -91, 219), S( -87, 212),
 S(-110, 237), S( -94, 233), S( -80, 231), S( -77, 214),
 S(-116, 234), S(-107, 233), S(-101, 230), S( -92, 219),
 S(-118, 224), S(-107, 212), S(-101, 214), S( -98, 211),
 S(-116, 209), S(-108, 213), S( -95, 210), S( -91, 202),
 S(-107, 216), S(-107, 212), S(-104, 217), S( -94, 202),
},{
 S( -46, 209), S(-100, 233), S(-161, 244), S(-105, 225),
 S( -76, 196), S( -64, 209), S( -87, 212), S(-100, 216),
 S(-109, 202), S( -51, 197), S( -78, 190), S( -57, 190),
 S(-102, 210), S( -65, 207), S( -72, 194), S( -70, 205),
 S(-120, 208), S( -98, 209), S( -98, 204), S( -84, 209),
 S(-108, 184), S( -88, 184), S( -91, 187), S( -87, 199),
 S(-104, 187), S( -77, 174), S( -90, 183), S( -86, 192),
 S( -95, 189), S( -95, 195), S( -96, 195), S( -88, 194),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-113, 384), S( -38, 291), S(  -2, 292), S( -14, 319),
 S( -48, 321), S( -40, 300), S( -21, 321), S( -32, 327),
 S( -18, 296), S(  -8, 284), S( -22, 319), S(   0, 311),
 S( -13, 315), S(  -9, 333), S(  -5, 333), S( -24, 345),
 S(  -4, 296), S( -16, 341), S( -14, 333), S( -19, 354),
 S(  -7, 274), S(  -1, 290), S(  -5, 305), S(  -9, 307),
 S(   0, 243), S(   1, 252), S(   8, 253), S(   7, 266),
 S( -12, 263), S( -10, 254), S(  -8, 261), S(  -1, 265),
},{
 S(  43, 263), S( 109, 239), S(-110, 374), S(  -2, 284),
 S(  28, 261), S(   6, 260), S( -15, 303), S( -39, 338),
 S(  24, 259), S(  -1, 253), S(  15, 274), S(  -5, 313),
 S(  -3, 292), S(   8, 295), S(  17, 269), S( -18, 339),
 S(  13, 259), S(   5, 290), S(   4, 298), S( -16, 348),
 S(   7, 229), S(  14, 254), S(   8, 286), S(  -5, 306),
 S(  27, 199), S(  26, 193), S(  19, 219), S(  10, 265),
 S(  22, 226), S(  -9, 243), S(  -7, 228), S(   0, 246),
}};

const Score KING_PSQT[2][32] = {{
 S( 211,-132), S( 145, -60), S(   9, -29), S(  84, -42),
 S( -44,  38), S(  41,  47), S(  28,  40), S(  48,  10),
 S(  61,  26), S(  68,  54), S(  67,  48), S(  12,  52),
 S(  68,  23), S(  41,  52), S(  10,  53), S( -34,  50),
 S(  -6,  17), S(  21,  37), S(  23,  30), S( -63,  39),
 S( -21,  -2), S(   3,  16), S( -16,  13), S( -37,  10),
 S(   8, -21), S( -16,  11), S( -34,  -2), S( -51,  -9),
 S(  16, -79), S(  24, -37), S(  -2, -34), S( -27, -48),
},{
 S(  19,-140), S(-236,  45), S(   9, -32), S( -92, -25),
 S(-115,  -2), S(   3,  66), S( -39,  55), S( 127,  10),
 S( -27,  10), S( 123,  64), S( 184,  50), S(  91,  45),
 S( -61,  10), S(  37,  52), S(   7,  65), S( -28,  58),
 S( -75,   5), S( -52,  40), S( -10,  38), S( -48,  43),
 S( -10, -10), S(  19,  15), S( -12,  17), S( -19,  14),
 S(   9, -11), S(   4,  14), S( -27,   6), S( -51,  -3),
 S(  11, -59), S(  15, -12), S( -12, -21), S(  -9, -49),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -45,  11), S(   7,  20), S(  30,  23), S(  60,  38),
 S(  15,  -2), S(  39,  20), S(  29,  27), S(  42,  38),
 S(  21,  -1), S(  33,  11), S(  21,  15), S(  20,  24),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -2,   9), S(  23,   5), S(  59,  14), S(  62,  10),
 S(   2,   1), S(  25,  12), S(  41,   6), S(  52,  12),
 S(  -8,  25), S(  44,   8), S(  30,  13), S(  37,  22),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-147, -33), S(-106,  39), S( -88,  78), S( -75,  90),
 S( -66, 102), S( -58, 113), S( -50, 115), S( -41, 116),
 S( -33, 108),};

const Score BISHOP_MOBILITIES[14] = {
 S( -12,  49), S(  11,  73), S(  24,  94), S(  32, 114),
 S(  39, 121), S(  43, 131), S(  45, 137), S(  49, 140),
 S(  50, 142), S(  53, 143), S(  62, 140), S(  75, 134),
 S(  77, 141), S(  85, 128),};

const Score ROOK_MOBILITIES[15] = {
 S( -93, -71), S(-104, 138), S( -88, 172), S( -80, 178),
 S( -82, 201), S( -80, 210), S( -86, 221), S( -81, 222),
 S( -76, 227), S( -71, 230), S( -68, 234), S( -69, 238),
 S( -65, 240), S( -57, 244), S( -41, 237),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1733,-1339), S(-122,-419), S( -76, 104), S( -54, 235),
 S( -43, 280), S( -37, 293), S( -37, 323), S( -36, 346),
 S( -33, 362), S( -30, 364), S( -28, 370), S( -25, 370),
 S( -26, 376), S( -23, 376), S( -25, 378), S( -19, 370),
 S( -21, 369), S( -22, 368), S( -15, 347), S(   2, 317),
 S(  20, 283), S(  35, 243), S(  75, 204), S( 116, 118),
 S(  80, 128), S( 201,  -1), S(  54,   5), S(  47, -31),
};

const Score MINOR_BEHIND_PAWN = S(5, 14);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-112, -241);

const Score ROOK_TRAPPED = S(-56, -22);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(23, 17);

const Score ROOK_OPEN_FILE = S(27, 16);

const Score ROOK_SEMI_OPEN = S(15, 7);

const Score DEFENDED_PAWN = S(11, 11);

const Score DOUBLED_PAWN = S(11, -33);

const Score ISOLATED_PAWN[4] = {
 S(   1,  -6), S(  -1, -14), S(  -7,  -7), S(  -1, -12),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -10);

const Score BACKWARDS_PAWN = S(-9, -17);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  86,  43), S(  29,  37), S(  11,  12),
 S(   6,   3), S(   5,   2), S(   2,   0), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 129, 160), S(   9,  77),
 S( -12,  60), S( -22,  36), S( -36,  13), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -11);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  97, 196), S(  32, 190), S(   4, 117),
 S( -17,  79), S( -11,  44), S( -10,  40), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 24);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 123, 195), S(  10, 141), S(   7,  50), S(  13,  10),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(26, -108);

const Score KNIGHT_THREATS[6] = { S(0, 19), S(-2, 48), S(29, 32), S(76, 0), S(50, -56), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(1, 19), S(21, 33), S(-59, 83), S(57, 10), S(62, 85), S(0, 0),};

const Score ROOK_THREATS[6] = { S(0, 21), S(30, 40), S(29, 52), S(3, 22), S(76, -5), S(0, 0),};

const Score KING_THREAT = S(15, 27);

const Score PAWN_THREAT = S(72, 30);

const Score PAWN_PUSH_THREAT = S(18, 18);

const Score HANGING_THREAT = S(5, 9);

const Score SPACE[15] = {
 S( 505, -23), S( 138, -14), S(  61, -10), S(  23,  -6), S(   4,  -2),
 S(  -5,   0), S(  -9,   3), S( -10,   6), S(  -9,   9), S(  -7,  13),
 S(  -4,  15), S(  -2,  20), S(   1,  18), S(   3,  13), S(   5,-336),
};

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(10, 25), S(0, 0),},
 { S(5, 21), S(-9, -28), S(0, 0),},
 { S(0, 35), S(-28, -17), S(-2, 2), S(0, 0),},
 { S(47, 47), S(-52, 24), S(-2, 84), S(-200, 157), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-24, -5), S(5, 93), S(-8, 46), S(-15, 8), S(5, -5), S(33, -26), S(30, -49), S(0, 0),},
 { S(-42, -1), S(-8, 74), S(-18, 41), S(-30, 20), S(-26, 6), S(24, -18), S(31, -31), S(0, 0),},
 { S(-23, -11), S(0, 84), S(-37, 33), S(-12, 9), S(-14, -3), S(-5, -8), S(29, -19), S(0, 0),},
 { S(-29, 15), S(0, 82), S(-55, 37), S(-20, 29), S(-18, 21), S(-8, 12), S(-17, 18), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-22, -11), S(-24, -2), S(-25, -8), S(-29, 4), S(-56, 32), S(55, 86), S(264, 95), S(0, 0),},
 { S(-12, -1), S(2, -5), S(3, -5), S(-11, 8), S(-26, 21), S(-33, 92), S(100, 145), S(0, 0),},
 { S(39, -11), S(35, -12), S(26, -6), S(8, 4), S(-14, 20), S(-53, 86), S(-11, 125), S(0, 0),},
 { S(-2, -4), S(5, -13), S(16, -10), S(8, -13), S(-15, 3), S(-19, 49), S(-66, 140), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(3, -44), S(40, -50), S(20, -38), S(17, -33), S(4, -25), S(4, -82), S(0, 0), S(0, 0),
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

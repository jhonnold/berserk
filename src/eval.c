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
 S(  72, 251), S(  66, 274), S(  42, 254), S(  74, 219),
 S(  25, 135), S(  11, 152), S(  40,  93), S(  47,  59),
 S(  20, 116), S(  23, 100), S(  26,  80), S(  24,  71),
 S(  16,  90), S(  16,  92), S(  23,  76), S(  31,  76),
 S(  11,  81), S(  12,  84), S(  16,  79), S(  19,  88),
 S(  21,  84), S(  27,  89), S(  22,  85), S(  28,  88),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 142, 196), S(  34, 199), S(  74, 209), S(  63, 213),
 S(  33,  86), S(  45,  79), S(  59,  59), S(  52,  50),
 S(  10,  78), S(  29,  72), S(  14,  69), S(  47,  60),
 S(   1,  70), S(  19,  74), S(   7,  74), S(  42,  68),
 S( -15,  68), S(  16,  69), S(   8,  78), S(  28,  83),
 S(  -6,  78), S(  32,  80), S(  16,  85), S(  30, 100),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-155,  46), S(-158,  68), S(-128,  77), S( -86,  58),
 S( -81,  72), S( -61,  77), S( -37,  64), S( -30,  64),
 S( -26,  55), S( -28,  67), S( -29,  76), S( -37,  78),
 S( -35,  83), S( -30,  79), S( -13,  76), S( -26,  81),
 S( -36,  80), S( -28,  75), S( -16,  86), S( -22,  83),
 S( -56,  61), S( -48,  62), S( -36,  69), S( -41,  83),
 S( -68,  66), S( -52,  61), S( -49,  68), S( -42,  66),
 S( -95,  74), S( -53,  58), S( -57,  63), S( -47,  74),
},{
 S(-183,-114), S(-247,  44), S(-102,  24), S( -34,  54),
 S( -51,   9), S(  16,  32), S( -16,  59), S( -38,  66),
 S( -10,  28), S( -50,  44), S(   2,  25), S(  -5,  55),
 S( -29,  61), S( -21,  59), S( -16,  67), S( -18,  75),
 S( -31,  76), S( -41,  68), S( -12,  79), S( -29,  92),
 S( -37,  56), S( -24,  54), S( -31,  62), S( -26,  79),
 S( -37,  60), S( -28,  56), S( -41,  60), S( -41,  62),
 S( -54,  73), S( -57,  59), S( -42,  58), S( -39,  71),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -12, 110), S( -28, 130), S( -15, 110), S( -34, 126),
 S(   5, 113), S(  -7, 101), S(  36, 103), S(  22, 115),
 S(  25, 116), S(  34, 114), S(  -2, 101), S(  36, 108),
 S(  23, 117), S(  46, 110), S(  36, 113), S(  26, 132),
 S(  41, 102), S(  33, 113), S(  41, 115), S(  50, 111),
 S(  38,  95), S(  54, 110), S(  39,  95), S(  44, 110),
 S(  40,  96), S(  41,  68), S(  52,  87), S(  37, 106),
 S(  32,  73), S(  52,  89), S(  40, 104), S(  41, 106),
},{
 S( -38,  69), S(  57,  94), S( -26, 102), S( -52, 103),
 S(  18,  67), S( -10,  95), S(   7, 114), S(  35,  95),
 S(  45, 104), S(  16, 106), S(  22, 100), S(  34, 108),
 S(  15, 102), S(  48, 106), S(  48, 110), S(  45, 111),
 S(  58,  79), S(  47, 102), S(  53, 113), S(  43, 113),
 S(  50,  86), S(  66,  98), S(  47,  95), S(  45, 120),
 S(  52,  72), S(  52,  70), S(  53,  99), S(  47, 105),
 S(  47,  53), S(  44, 121), S(  35, 108), S(  51, 103),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -92, 233), S( -83, 236), S( -83, 240), S(-110, 242),
 S( -79, 219), S( -85, 231), S( -64, 230), S( -48, 214),
 S( -92, 224), S( -56, 219), S( -67, 220), S( -63, 213),
 S( -87, 236), S( -70, 232), S( -56, 231), S( -52, 214),
 S( -93, 235), S( -84, 237), S( -78, 233), S( -70, 223),
 S( -95, 228), S( -84, 216), S( -78, 220), S( -76, 216),
 S( -94, 219), S( -86, 221), S( -72, 217), S( -69, 209),
 S( -85, 224), S( -85, 220), S( -82, 225), S( -72, 211),
},{
 S( -21, 214), S( -74, 238), S(-138, 250), S( -79, 231),
 S( -51, 199), S( -40, 213), S( -64, 218), S( -77, 221),
 S( -84, 202), S( -28, 197), S( -53, 187), S( -32, 191),
 S( -78, 208), S( -39, 203), S( -47, 192), S( -45, 205),
 S( -96, 209), S( -74, 211), S( -75, 207), S( -61, 213),
 S( -85, 188), S( -65, 189), S( -68, 191), S( -65, 204),
 S( -83, 198), S( -55, 181), S( -67, 190), S( -64, 200),
 S( -73, 197), S( -73, 204), S( -74, 202), S( -66, 202),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-100, 385), S( -23, 294), S(  13, 294), S(   1, 321),
 S( -36, 325), S( -28, 306), S(  -9, 327), S( -20, 333),
 S(  -7, 300), S(   4, 287), S( -10, 326), S(  11, 320),
 S(  -1, 322), S(   3, 339), S(   6, 338), S( -12, 354),
 S(   7, 302), S(  -4, 347), S(  -2, 340), S(  -8, 361),
 S(   4, 282), S(  10, 298), S(   6, 314), S(   2, 317),
 S(  11, 254), S(  12, 263), S(  19, 264), S(  18, 276),
 S(   0, 272), S(   1, 264), S(   3, 272), S(  10, 275),
},{
 S(  57, 261), S( 122, 241), S( -95, 376), S(  13, 286),
 S(  40, 264), S(  19, 264), S(  -3, 308), S( -27, 343),
 S(  37, 261), S(  12, 253), S(  28, 277), S(   7, 319),
 S(   9, 296), S(  20, 298), S(  30, 273), S(  -6, 343),
 S(  24, 264), S(  16, 296), S(  15, 306), S(  -6, 357),
 S(  19, 235), S(  26, 262), S(  19, 295), S(   6, 316),
 S(  38, 208), S(  36, 205), S(  30, 229), S(  21, 275),
 S(  32, 239), S(   1, 253), S(   4, 237), S(  11, 256),
}};

const Score KING_PSQT[2][32] = {{
 S( 193,-110), S( 145, -49), S(  14, -19), S(  82, -31),
 S( -48,  40), S(  48,  45), S(  28,  43), S(  38,  15),
 S(  61,  21), S(  81,  44), S(  70,  44), S(  16,  49),
 S(  67,  21), S(  47,  45), S(  12,  49), S( -30,  47),
 S(  -6,  15), S(  22,  34), S(  24,  28), S( -64,  37),
 S( -22,  -1), S(   2,  17), S( -13,  14), S( -35,  10),
 S(   9, -20), S( -16,  14), S( -35,   1), S( -52,  -6),
 S(  15, -78), S(  24, -36), S(  -2, -34), S( -27, -48),
},{
 S(  16,-128), S(-230,  57), S( -17, -17), S( -91, -15),
 S(-113,   2), S(  -6,  69), S( -62,  61), S( 126,  15),
 S( -29,   8), S( 121,  59), S( 179,  50), S(  86,  45),
 S( -58,   7), S(  42,  47), S(   7,  62), S( -31,  57),
 S( -72,   2), S( -49,  37), S(  -6,  35), S( -47,  40),
 S(  -9, -10), S(  19,  14), S( -10,  17), S( -19,  14),
 S(   9, -10), S(   4,  16), S( -28,   8), S( -51,  -1),
 S(  11, -59), S(  15, -12), S( -12, -21), S(  -9, -51),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -46,   9), S(   7,  20), S(  30,  23), S(  59,  39),
 S(  15,  -2), S(  40,  19), S(  30,  26), S(  42,  37),
 S(  21,  -4), S(  33,   9), S(  22,  13), S(  20,  23),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -2,   7), S(  23,   5), S(  59,  13), S(  62,   8),
 S(   3,  -4), S(  26,  12), S(  41,   6), S(  52,  11),
 S(  -7,  22), S(  45,   8), S(  31,  11), S(  38,  21),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-114, -31), S( -73,  43), S( -55,  82), S( -43,  95),
 S( -33, 108), S( -26, 119), S( -17, 121), S(  -9, 123),
 S(  -1, 115),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -2,  54), S(  22,  77), S(  35,  98), S(  42, 119),
 S(  49, 127), S(  53, 137), S(  55, 143), S(  59, 147),
 S(  60, 149), S(  63, 151), S(  71, 148), S(  84, 143),
 S(  87, 148), S(  90, 141),};

const Score ROOK_MOBILITIES[15] = {
 S( -90, -39), S( -87, 148), S( -71, 184), S( -63, 189),
 S( -65, 212), S( -63, 220), S( -68, 231), S( -64, 230),
 S( -58, 233), S( -53, 235), S( -49, 237), S( -49, 239),
 S( -46, 241), S( -35, 243), S( -19, 235),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1692,-1326), S(-108,-392), S( -58, 121), S( -35, 249),
 S( -25, 297), S( -19, 309), S( -19, 338), S( -18, 361),
 S( -15, 376), S( -12, 378), S( -10, 384), S(  -7, 384),
 S(  -8, 389), S(  -4, 388), S(  -6, 390), S(   0, 380),
 S(  -2, 379), S(  -3, 377), S(   5, 354), S(  23, 323),
 S(  42, 286), S(  57, 247), S(  98, 206), S( 139, 120),
 S( 112, 120), S( 233,  -9), S(  72,   8), S(  78, -35),
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
 S(   0,   0), S(   0,   0), S( 119, 155), S(   8,  80),
 S( -12,  60), S( -22,  36), S( -35,  12), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -10);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  61, 256), S(  12, 285), S(  -5, 143),
 S( -23,  74), S( -10,  44), S(  -9,  40), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(2, -12);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 26);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(21, 34);

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(18, -75);

const Score KNIGHT_THREATS[6] = { S(0, 19), S(-2, 43), S(29, 33), S(76, 0), S(50, -55), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 19), S(21, 35), S(-60, 88), S(57, 12), S(62, 85), S(0, 0),};

const Score ROOK_THREATS[6] = { S(-1, 23), S(30, 39), S(29, 52), S(3, 22), S(75, -4), S(0, 0),};

const Score KING_THREAT = S(13, 32);

const Score PAWN_THREAT = S(72, 30);

const Score PAWN_PUSH_THREAT = S(18, 19);

const Score HANGING_THREAT = S(5, 8);

const Score SPACE[15] = {
 S( 548, -20), S( 144, -14), S(  64, -10), S(  24,  -6), S(   4,  -2),
 S(  -5,   1), S(  -9,   3), S( -10,   6), S(  -9,   9), S(  -7,  12),
 S(  -4,  14), S(  -2,  20), S(   1,  18), S(   3,  14), S(   5,-324),
};

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(6, 23), S(0, 0),},
 { S(2, 18), S(-5, -26), S(0, 0),},
 { S(-6, 30), S(-24, -7), S(-6, 6), S(0, 0),},
 { S(36, 38), S(-33, 41), S(4, 88), S(-198, 171), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-24, -5), S(19, 70), S(-11, 50), S(-15, 9), S(5, -6), S(33, -26), S(30, -49), S(0, 0),},
 { S(-42, 0), S(11, 61), S(-21, 45), S(-31, 21), S(-26, 6), S(24, -18), S(31, -31), S(0, 0),},
 { S(-23, -11), S(14, 68), S(-38, 38), S(-12, 9), S(-14, -4), S(-5, -8), S(29, -19), S(0, 0),},
 { S(-29, 15), S(16, 72), S(-58, 43), S(-20, 30), S(-18, 20), S(-8, 12), S(-17, 18), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-21, -13), S(-24, -4), S(-25, -10), S(-29, 2), S(-59, 39), S(36, 163), S(270, 179), S(0, 0),},
 { S(-12, -3), S(2, -7), S(2, -7), S(-11, 5), S(-28, 25), S(-50, 165), S(87, 234), S(0, 0),},
 { S(40, -15), S(37, -16), S(26, -10), S(8, 0), S(-15, 23), S(-67, 142), S(-20, 203), S(0, 0),},
 { S(-2, -7), S(4, -17), S(16, -12), S(8, -15), S(-18, 11), S(-35, 113), S(-102, 215), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-4, -23), S(41, -53), S(20, -42), S(17, -35), S(4, -25), S(3, -76), S(0, 0), S(0, 0),
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

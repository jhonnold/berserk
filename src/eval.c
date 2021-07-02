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
 S(  62, 243), S(  57, 269), S(  32, 251), S(  63, 217),
 S(  -4, 135), S( -18, 151), S(  12,  93), S(  18,  61),
 S(  -9, 116), S(  -6, 100), S(  -2,  80), S(  -4,  72),
 S( -13,  91), S( -13,  92), S(  -5,  76), S(   3,  76),
 S( -18,  82), S( -17,  84), S( -12,  79), S( -10,  88),
 S(  -8,  85), S(  -2,  89), S(  -6,  85), S(   0,  88),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 133, 192), S(  23, 197), S(  66, 210), S(  52, 213),
 S(   4,  87), S(  16,  81), S(  30,  61), S(  23,  52),
 S( -20,  79), S(   0,  72), S( -15,  70), S(  19,  61),
 S( -29,  71), S( -10,  75), S( -22,  74), S(  14,  69),
 S( -45,  69), S( -14,  70), S( -21,  79), S(   0,  83),
 S( -35,  79), S(   4,  80), S( -13,  85), S(   1, 100),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-129,  48), S(-132,  70), S(-101,  79), S( -61,  61),
 S( -54,  74), S( -35,  79), S( -10,  66), S(  -5,  66),
 S(   1,  57), S(   0,  69), S(  -1,  78), S( -10,  80),
 S(  -8,  85), S(  -3,  82), S(  15,  78), S(   0,  83),
 S(  -9,  82), S(  -1,  77), S(  11,  89), S(   6,  86),
 S( -29,  63), S( -21,  64), S(  -9,  71), S( -14,  85),
 S( -41,  68), S( -25,  63), S( -22,  70), S( -15,  68),
 S( -68,  75), S( -25,  61), S( -30,  65), S( -20,  76),
},{
 S(-157,-114), S(-227,  46), S( -76,  26), S(  -9,  56),
 S( -27,  10), S(  45,  33), S(  10,  61), S( -13,  68),
 S(  15,  30), S( -25,  46), S(  28,  26), S(  22,  58),
 S(  -5,  64), S(   5,  60), S(   9,  69), S(   8,  77),
 S(  -5,  78), S( -17,  70), S(  14,  81), S(  -2,  94),
 S( -10,  58), S(   3,  56), S(  -3,  64), S(   1,  81),
 S( -10,  62), S(  -1,  58), S( -14,  62), S( -14,  64),
 S( -27,  74), S( -30,  61), S( -15,  60), S( -12,  74),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -10, 111), S( -25, 131), S( -13, 112), S( -32, 127),
 S(   7, 114), S(  -5, 103), S(  38, 105), S(  24, 116),
 S(  27, 118), S(  36, 116), S(   1, 102), S(  38, 110),
 S(  25, 119), S(  48, 113), S(  39, 115), S(  27, 134),
 S(  43, 104), S(  35, 115), S(  43, 116), S(  52, 113),
 S(  40,  97), S(  55, 111), S(  41,  97), S(  45, 112),
 S(  42,  98), S(  43,  70), S(  54,  89), S(  39, 108),
 S(  34,  74), S(  53,  90), S(  42, 107), S(  43, 108),
},{
 S( -37,  70), S(  59,  95), S( -23, 104), S( -50, 104),
 S(  18,  68), S( -12,  97), S(   6, 116), S(  35,  97),
 S(  46, 105), S(  16, 108), S(  22, 102), S(  34, 110),
 S(  15, 104), S(  49, 108), S(  49, 112), S(  46, 113),
 S(  60,  81), S(  49, 104), S(  55, 115), S(  44, 115),
 S(  52,  88), S(  68,  99), S(  50,  97), S(  47, 122),
 S(  54,  74), S(  53,  72), S(  55, 101), S(  49, 107),
 S(  49,  55), S(  46, 123), S(  37, 110), S(  53, 105),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -64, 239), S( -55, 242), S( -56, 247), S( -86, 251),
 S( -51, 224), S( -58, 235), S( -37, 234), S( -22, 219),
 S( -63, 229), S( -26, 224), S( -38, 225), S( -34, 219),
 S( -57, 241), S( -40, 237), S( -26, 235), S( -22, 219),
 S( -61, 240), S( -54, 241), S( -48, 238), S( -40, 229),
 S( -65, 233), S( -54, 221), S( -48, 224), S( -45, 221),
 S( -63, 224), S( -56, 226), S( -41, 222), S( -38, 214),
 S( -54, 229), S( -54, 224), S( -51, 230), S( -41, 216),
},{
 S(   3, 224), S( -51, 247), S(-125, 261), S( -56, 240),
 S( -28, 206), S( -19, 220), S( -47, 224), S( -52, 225),
 S( -59, 208), S(  -2, 202), S( -27, 191), S(  -4, 196),
 S( -50, 213), S( -12, 206), S( -18, 194), S( -17, 209),
 S( -68, 214), S( -46, 215), S( -44, 209), S( -32, 217),
 S( -56, 193), S( -37, 193), S( -38, 195), S( -35, 209),
 S( -53, 202), S( -26, 185), S( -37, 194), S( -33, 204),
 S( -43, 202), S( -42, 207), S( -43, 206), S( -35, 207),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -91, 407), S( -13, 314), S(  20, 317), S(   8, 343),
 S( -26, 348), S( -20, 330), S(  -1, 352), S( -12, 359),
 S(   3, 323), S(  12, 311), S(  -2, 351), S(  18, 344),
 S(   8, 346), S(  11, 365), S(  14, 365), S(  -6, 381),
 S(  16, 327), S(   4, 372), S(   5, 365), S(  -1, 387),
 S(  13, 306), S(  19, 321), S(  15, 338), S(  10, 341),
 S(  20, 276), S(  21, 286), S(  27, 287), S(  27, 300),
 S(   9, 294), S(  10, 287), S(  12, 296), S(  19, 300),
},{
 S(  63, 281), S( 129, 258), S( -97, 398), S(  19, 305),
 S(  45, 283), S(  22, 285), S(  -3, 327), S( -21, 365),
 S(  39, 279), S(  12, 270), S(  28, 295), S(  11, 342),
 S(  14, 315), S(  25, 317), S(  36, 291), S(   0, 368),
 S(  32, 283), S(  24, 318), S(  23, 327), S(   2, 382),
 S(  27, 255), S(  34, 284), S(  28, 316), S(  14, 340),
 S(  47, 229), S(  45, 227), S(  39, 252), S(  30, 299),
 S(  42, 259), S(  11, 273), S(  14, 257), S(  20, 281),
}};

const Score KING_PSQT[2][32] = {{
 S( 192,-109), S( 141, -47), S(  14, -17), S(  89, -31),
 S( -54,  41), S(  44,  45), S(  26,  43), S(  40,  15),
 S(  60,  21), S(  77,  44), S(  69,  44), S(  16,  49),
 S(  68,  21), S(  46,  44), S(  14,  49), S( -29,  47),
 S(  -6,  15), S(  23,  33), S(  26,  28), S( -61,  37),
 S( -22,  -2), S(   1,  17), S( -13,  14), S( -33,  10),
 S(   8, -20), S( -17,  14), S( -35,   1), S( -52,  -6),
 S(  15, -79), S(  24, -36), S(  -2, -33), S( -27, -47),
},{
 S(  16,-127), S(-238,  60), S( -12, -17), S( -83, -15),
 S(-119,   3), S( -10,  70), S( -58,  61), S( 131,  15),
 S( -26,   8), S( 123,  60), S( 186,  51), S(  92,  46),
 S( -58,   8), S(  42,  47), S(  11,  63), S( -27,  57),
 S( -71,   2), S( -48,  37), S(  -3,  35), S( -44,  41),
 S(  -8, -10), S(  20,  14), S(  -9,  17), S( -18,  15),
 S(   9, -10), S(   4,  16), S( -28,   8), S( -51,  -1),
 S(  11, -59), S(  15, -12), S( -11, -21), S(  -9, -50),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -46,  10), S(   6,  20), S(  29,  23), S(  59,  39),
 S(  15,  -3), S(  39,  20), S(  29,  26), S(  41,  37),
 S(  21,  -3), S(  33,   9), S(  22,  13), S(  19,  24),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -2,   7), S(  24,   5), S(  59,  12), S(  61,   8),
 S(   4,  -4), S(  26,  11), S(  41,   5), S(  52,  11),
 S(  -7,  22), S(  45,   7), S(  31,  11), S(  37,  21),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -84, -27), S( -43,  45), S( -25,  84), S( -12,  98),
 S(  -3, 110), S(   5, 121), S(  14, 123), S(  22, 125),
 S(  30, 117),};

const Score BISHOP_MOBILITIES[14] = {
 S(   1,  56), S(  25,  80), S(  38, 101), S(  45, 121),
 S(  52, 129), S(  56, 139), S(  58, 146), S(  62, 149),
 S(  63, 151), S(  66, 153), S(  74, 150), S(  87, 145),
 S(  88, 150), S(  91, 144),};

const Score ROOK_MOBILITIES[15] = {
 S( -66, -34), S( -64, 153), S( -48, 188), S( -40, 193),
 S( -42, 216), S( -40, 224), S( -45, 235), S( -41, 235),
 S( -35, 238), S( -30, 240), S( -26, 243), S( -26, 246),
 S( -23, 248), S( -13, 249), S(   6, 239),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1649,-1313), S( -97,-364), S( -46, 161), S( -24, 292),
 S( -14, 339), S(  -8, 350), S(  -8, 379), S(  -7, 402),
 S(  -4, 417), S(  -1, 419), S(   1, 424), S(   4, 424),
 S(   3, 429), S(   7, 427), S(   5, 429), S(  11, 418),
 S(   9, 416), S(   8, 413), S(  17, 389), S(  35, 356),
 S(  56, 316), S(  73, 274), S( 115, 232), S( 160, 141),
 S( 141, 134), S( 269,   0), S( 106,  21), S( 114, -29),
};

const Score MINOR_BEHIND_PAWN = S(4, 17);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 21);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 8);

const Score BISHOP_TRAPPED = S(-116, -235);

const Score ROOK_TRAPPED = S(-55, -26);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(23, 16);

const Score ROOK_OPEN_FILE = S(27, 17);

const Score ROOK_SEMI_OPEN = S(7, 12);

const Score DEFENDED_PAWN = S(12, 11);

const Score DOUBLED_PAWN = S(12, -34);

const Score ISOLATED_PAWN[4] = {
 S(   1,  -6), S(  -1, -13), S(  -7,  -7), S(  -2, -12),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -10);

const Score BACKWARDS_PAWN = S(-10, -18);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  83,  47), S(  29,  39), S(  12,  11),
 S(   6,   2), S(   5,   1), S(   2,   0), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 118, 156), S(   8,  80),
 S( -13,  60), S( -23,  36), S( -36,  12), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -10);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  49, 252), S(  15, 280), S(  -3, 140),
 S( -21,  73), S(  -8,  44), S(  -9,  41), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(2, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 26);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(21, 34);

const Score KNIGHT_THREATS[6] = { S(0, 19), S(-2, 46), S(29, 32), S(77, 0), S(50, -55), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(1, 19), S(21, 34), S(-60, 86), S(57, 12), S(62, 86), S(0, 0),};

const Score ROOK_THREATS[6] = { S(3, 27), S(32, 40), S(30, 53), S(3, 22), S(77, -3), S(0, 0),};

const Score KING_THREAT = S(15, 32);

const Score PAWN_THREAT = S(72, 30);

const Score PAWN_PUSH_THREAT = S(18, 19);

const Score HANGING_THREAT = S(5, 7);

const Score SPACE[15] = {
 S( 588, -20), S( 146, -14), S(  64, -10), S(  24,  -6), S(   5,  -2),
 S(  -4,   0), S(  -8,   3), S( -10,   6), S(  -9,   8), S(  -6,  12),
 S(  -3,  14), S(  -2,  19), S(   1,  18), S(   3,  13), S(   4,-312),
};

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(4, 20), S(0, 0),},
 { S(-1, 16), S(-1, -21), S(0, 0),},
 { S(-11, 25), S(-21, -2), S(-10, 3), S(0, 0),},
 { S(29, 26), S(-12, 45), S(6, 76), S(-186, 169), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-24, -5), S(21, 79), S(-11, 53), S(-16, 10), S(5, -5), S(33, -26), S(30, -49), S(0, 0),},
 { S(-42, 0), S(12, 67), S(-21, 47), S(-31, 22), S(-26, 6), S(24, -18), S(31, -31), S(0, 0),},
 { S(-22, -11), S(15, 73), S(-38, 39), S(-13, 9), S(-14, -4), S(-5, -8), S(29, -20), S(0, 0),},
 { S(-29, 15), S(20, 75), S(-57, 43), S(-20, 31), S(-18, 21), S(-8, 12), S(-16, 18), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-20, -13), S(-25, -4), S(-25, -9), S(-29, 2), S(-59, 39), S(36, 162), S(273, 174), S(0, 0),},
 { S(-12, -3), S(2, -8), S(2, -7), S(-11, 5), S(-28, 25), S(-51, 164), S(89, 231), S(0, 0),},
 { S(41, -16), S(37, -17), S(27, -10), S(8, 0), S(-16, 23), S(-68, 142), S(-15, 201), S(0, 0),},
 { S(-2, -7), S(4, -17), S(16, -12), S(7, -15), S(-18, 10), S(-35, 112), S(-103, 212), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-5, -27), S(42, -56), S(20, -43), S(17, -35), S(4, -25), S(3, -74), S(0, 0), S(0, 0),
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
            s += ROOK_SEMI_OPEN;

            if (T)
              C.rookSemiOpen += cs[side];
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

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
 S(  68, 243), S(  62, 269), S(  37, 251), S(  69, 217),
 S(   8, 135), S(  -6, 151), S(  24,  93), S(  30,  61),
 S(   3, 116), S(   6, 100), S(   9,  80), S(   7,  72),
 S(  -1,  91), S(  -1,  92), S(   6,  77), S(  15,  77),
 S(  -6,  82), S(  -5,  85), S(   0,  79), S(   2,  88),
 S(   4,  85), S(  10,  89), S(   5,  86), S(  11,  89),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 139, 192), S(  27, 198), S(  69, 209), S(  56, 213),
 S(  16,  87), S(  28,  81), S(  42,  61), S(  35,  52),
 S(  -8,  79), S(  12,  72), S(  -3,  70), S(  30,  61),
 S( -16,  71), S(   2,  75), S( -10,  74), S(  25,  69),
 S( -33,  69), S(  -1,  70), S(  -9,  79), S(  11,  83),
 S( -23,  79), S(  15,  80), S(  -1,  85), S(  13, 100),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-139,  46), S(-142,  69), S(-111,  78), S( -70,  60),
 S( -63,  73), S( -45,  78), S( -20,  65), S( -15,  65),
 S(  -8,  56), S( -10,  68), S( -12,  77), S( -21,  79),
 S( -18,  84), S( -14,  81), S(   4,  77), S( -11,  81),
 S( -19,  81), S( -11,  76), S(   1,  87), S(  -5,  84),
 S( -39,  62), S( -31,  63), S( -19,  70), S( -24,  84),
 S( -51,  66), S( -35,  62), S( -32,  69), S( -25,  67),
 S( -78,  74), S( -36,  59), S( -40,  64), S( -31,  75),
},{
 S(-168,-114), S(-237,  45), S( -86,  24), S( -19,  55),
 S( -36,   9), S(  34,  32), S(   0,  60), S( -23,  67),
 S(   5,  29), S( -35,  44), S(  18,  25), S(  11,  56),
 S( -15,  62), S(  -6,  59), S(  -1,  67), S(  -3,  76),
 S( -15,  77), S( -27,  69), S(   4,  80), S( -13,  93),
 S( -20,  57), S(  -7,  54), S( -14,  63), S( -10,  80),
 S( -21,  61), S( -11,  57), S( -25,  61), S( -24,  63),
 S( -37,  73), S( -40,  60), S( -25,  59), S( -22,  72),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -10, 110), S( -26, 130), S( -13, 111), S( -32, 126),
 S(   8, 113), S(  -4, 101), S(  38, 104), S(  24, 115),
 S(  27, 117), S(  36, 115), S(   1, 101), S(  38, 109),
 S(  25, 117), S(  48, 111), S(  39, 114), S(  27, 133),
 S(  43, 103), S(  35, 114), S(  42, 115), S(  51, 112),
 S(  40,  96), S(  56, 111), S(  41,  96), S(  46, 111),
 S(  42,  97), S(  43,  69), S(  54,  88), S(  39, 107),
 S(  34,  73), S(  53,  89), S(  42, 105), S(  43, 107),
},{
 S( -36,  69), S(  58,  94), S( -23, 103), S( -50, 104),
 S(  19,  67), S( -11,  96), S(   6, 115), S(  36,  96),
 S(  46, 104), S(  16, 107), S(  22, 101), S(  35, 109),
 S(  16, 103), S(  49, 107), S(  49, 111), S(  46, 112),
 S(  60,  80), S(  49, 103), S(  55, 114), S(  44, 114),
 S(  52,  87), S(  69,  98), S(  49,  96), S(  47, 121),
 S(  54,  73), S(  54,  71), S(  56, 100), S(  49, 106),
 S(  49,  53), S(  46, 122), S(  38, 109), S(  53, 103),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -75, 237), S( -66, 240), S( -67, 244), S( -96, 248),
 S( -60, 223), S( -66, 234), S( -45, 233), S( -29, 218),
 S( -74, 228), S( -37, 222), S( -48, 223), S( -45, 217),
 S( -68, 239), S( -50, 235), S( -37, 233), S( -33, 217),
 S( -73, 238), S( -64, 238), S( -58, 236), S( -50, 226),
 S( -76, 230), S( -65, 219), S( -59, 222), S( -57, 219),
 S( -74, 221), S( -67, 223), S( -53, 220), S( -50, 212),
 S( -66, 226), S( -65, 222), S( -62, 227), S( -53, 213),
},{
 S(  -8, 222), S( -62, 244), S(-135, 258), S( -66, 236),
 S( -38, 204), S( -26, 217), S( -54, 222), S( -59, 224),
 S( -71, 207), S( -15, 200), S( -40, 190), S( -15, 194),
 S( -61, 211), S( -22, 204), S( -29, 192), S( -26, 207),
 S( -78, 211), S( -57, 212), S( -56, 207), S( -42, 215),
 S( -67, 190), S( -48, 190), S( -50, 192), S( -46, 206),
 S( -64, 199), S( -37, 182), S( -49, 191), S( -45, 202),
 S( -54, 199), S( -54, 204), S( -55, 203), S( -46, 204),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -94, 397), S( -17, 305), S(  18, 306), S(   5, 332),
 S( -29, 337), S( -22, 319), S(  -3, 340), S( -14, 347),
 S(   0, 312), S(  10, 300), S(  -4, 339), S(  16, 332),
 S(   6, 335), S(   9, 353), S(  13, 353), S(  -7, 369),
 S(  14, 315), S(   2, 360), S(   4, 353), S(  -2, 374),
 S(  11, 294), S(  17, 310), S(  13, 326), S(   9, 329),
 S(  18, 264), S(  19, 274), S(  25, 276), S(  25, 288),
 S(   7, 282), S(   8, 276), S(  10, 285), S(  17, 288),
},{
 S(  59, 272), S( 124, 251), S(-100, 388), S(  15, 295),
 S(  43, 273), S(  20, 275), S(  -5, 316), S( -23, 353),
 S(  38, 268), S(  10, 259), S(  27, 283), S(  10, 329),
 S(  12, 303), S(  23, 305), S(  34, 279), S(  -1, 355),
 S(  30, 271), S(  22, 306), S(  21, 315), S(   0, 370),
 S(  25, 244), S(  32, 272), S(  26, 305), S(  13, 328),
 S(  45, 217), S(  43, 216), S(  37, 240), S(  28, 287),
 S(  39, 248), S(   9, 262), S(  11, 246), S(  18, 269),
}};

const Score KING_PSQT[2][32] = {{
 S( 200,-111), S( 144, -48), S(  14, -18), S(  90, -31),
 S( -52,  41), S(  44,  45), S(  26,  43), S(  38,  16),
 S(  60,  21), S(  77,  44), S(  68,  44), S(  17,  49),
 S(  69,  21), S(  45,  44), S(  15,  49), S( -28,  47),
 S(  -6,  15), S(  23,  34), S(  27,  28), S( -61,  37),
 S( -21,  -2), S(   2,  17), S( -12,  14), S( -33,  11),
 S(   9, -20), S( -16,  14), S( -35,   1), S( -52,  -6),
 S(  15, -78), S(  24, -36), S(  -2, -33), S( -27, -47),
},{
 S(  16,-127), S(-240,  60), S( -18, -16), S( -85, -15),
 S(-117,   2), S( -13,  70), S( -61,  61), S( 129,  15),
 S( -28,   8), S( 122,  60), S( 183,  51), S(  92,  45),
 S( -58,   8), S(  42,  47), S(  11,  62), S( -27,  57),
 S( -71,   2), S( -48,  37), S(  -3,  35), S( -43,  41),
 S(  -7, -10), S(  20,  14), S(  -8,  17), S( -16,  15),
 S(   9, -10), S(   4,  16), S( -28,   8), S( -51,  -1),
 S(  11, -59), S(  15, -12), S( -12, -21), S( -10, -50),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -47,  10), S(   6,  20), S(  30,  23), S(  59,  39),
 S(  15,  -2), S(  40,  20), S(  30,  26), S(  42,  37),
 S(  21,  -3), S(  33,  10), S(  22,  13), S(  20,  24),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -2,   7), S(  23,   5), S(  59,  12), S(  61,   8),
 S(   3,  -4), S(  26,  12), S(  41,   6), S(  52,  11),
 S(  -7,  22), S(  45,   8), S(  31,  11), S(  38,  21),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -95, -28), S( -54,  44), S( -36,  83), S( -23,  97),
 S( -14, 109), S(  -6, 120), S(   2, 122), S(  11, 124),
 S(  19, 116),};

const Score BISHOP_MOBILITIES[14] = {
 S(   1,  55), S(  25,  79), S(  38, 100), S(  45, 120),
 S(  52, 128), S(  56, 138), S(  58, 144), S(  62, 148),
 S(  63, 150), S(  66, 152), S(  74, 148), S(  87, 144),
 S(  88, 149), S(  92, 142),};

const Score ROOK_MOBILITIES[15] = {
 S( -76, -36), S( -72, 151), S( -56, 187), S( -48, 192),
 S( -50, 215), S( -48, 223), S( -53, 233), S( -49, 233),
 S( -43, 235), S( -38, 237), S( -34, 240), S( -35, 243),
 S( -32, 245), S( -21, 245), S(  -3, 235),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1666,-1318), S(-100,-379), S( -49, 143), S( -27, 273),
 S( -16, 320), S( -11, 332), S( -11, 361), S( -10, 383),
 S(  -7, 398), S(  -4, 401), S(  -2, 406), S(   1, 406),
 S(   1, 411), S(   4, 409), S(   2, 411), S(   8, 401),
 S(   6, 399), S(   5, 396), S(  14, 372), S(  31, 340),
 S(  52, 301), S(  68, 260), S( 110, 217), S( 154, 128),
 S( 130, 125), S( 255,  -7), S(  97,   9), S( 103, -39),
};

const Score MINOR_BEHIND_PAWN = S(4, 17);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 20);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 8);

const Score BISHOP_TRAPPED = S(-114, -236);

const Score ROOK_TRAPPED = S(-55, -26);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(23, 16);

const Score ROOK_OPEN_FILE = S(27, 18);

const Score ROOK_SEMI_OPEN = S(12, 20);

const Score DEFENDED_PAWN = S(11, 10);

const Score DOUBLED_PAWN = S(12, -34);

const Score ISOLATED_PAWN[4] = {
 S(   1,  -6), S(  -2, -13), S(  -7,  -7), S(  -2, -12),
};

const Score OPEN_ISOLATED_PAWN = S(-4, -10);

const Score BACKWARDS_PAWN = S(-9, -17);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  79,  47), S(  29,  39), S(  11,  11),
 S(   6,   1), S(   5,   1), S(   2,   0), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 121, 158), S(   9,  80),
 S( -12,  60), S( -22,  37), S( -35,  12), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -10);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  55, 252), S(  14, 280), S(  -4, 140),
 S( -22,  73), S(  -9,  44), S(  -9,  41), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(2, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 26);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(21, 34);

const Score KNIGHT_THREATS[6] = { S(0, 19), S(-2, 43), S(29, 33), S(77, 0), S(50, -55), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(1, 19), S(21, 35), S(-60, 87), S(57, 11), S(62, 86), S(0, 0),};

const Score ROOK_THREATS[6] = { S(-1, 24), S(30, 40), S(29, 52), S(3, 23), S(76, -5), S(0, 0),};

const Score KING_THREAT = S(16, 32);

const Score PAWN_THREAT = S(72, 30);

const Score PAWN_PUSH_THREAT = S(18, 19);

const Score HANGING_THREAT = S(5, 8);

const Score SPACE[15] = {
 S( 568, -20), S( 145, -14), S(  64, -10), S(  24,  -6), S(   4,  -2),
 S(  -5,   0), S(  -9,   3), S( -10,   6), S(  -9,   9), S(  -6,  12),
 S(  -3,  14), S(  -2,  19), S(   1,  18), S(   3,  14), S(   5,-320),
};

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(5, 21), S(0, 0),},
 { S(0, 17), S(-2, -23), S(0, 0),},
 { S(-9, 27), S(-23, -4), S(-9, 4), S(0, 0),},
 { S(32, 31), S(-20, 44), S(5, 81), S(-193, 172), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-24, -5), S(19, 77), S(-11, 53), S(-16, 10), S(5, -6), S(33, -26), S(30, -49), S(0, 0),},
 { S(-42, 0), S(11, 66), S(-21, 47), S(-31, 22), S(-26, 6), S(24, -18), S(31, -31), S(0, 0),},
 { S(-23, -11), S(15, 71), S(-38, 39), S(-12, 9), S(-14, -4), S(-5, -8), S(29, -20), S(0, 0),},
 { S(-29, 15), S(19, 74), S(-58, 43), S(-20, 31), S(-18, 21), S(-8, 12), S(-17, 18), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-20, -13), S(-25, -4), S(-25, -9), S(-29, 2), S(-60, 39), S(36, 162), S(274, 174), S(0, 0),},
 { S(-12, -3), S(2, -7), S(2, -7), S(-11, 5), S(-28, 25), S(-50, 164), S(89, 232), S(0, 0),},
 { S(41, -16), S(37, -17), S(26, -10), S(8, 0), S(-16, 23), S(-67, 141), S(-17, 201), S(0, 0),},
 { S(-1, -7), S(4, -17), S(16, -12), S(7, -15), S(-18, 10), S(-35, 113), S(-104, 213), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-4, -26), S(42, -55), S(20, -43), S(17, -35), S(4, -25), S(3, -75), S(0, 0), S(0, 0),
};

const Score KS_ATTACKER_WEIGHTS[5] = {
 0, 32, 31, 18, 24
};

const Score KS_ATTACK = 4;

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
        data->ksSqAttackCount[side] += bits(movement & enemyKingArea);
        data->ksAttackerCount[side]++;

        if (T) {
          C.ksAttackerCount[xside]++;
          C.ksAttackerWeights[xside][pieceType]++;
          C.ksAttack[xside] += bits(movement & enemyKingArea);
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
                 + (KS_ATTACK * data->ksSqAttackCount[xside])                   // general pieces aimed
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

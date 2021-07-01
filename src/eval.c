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

const Score BISHOP_PAIR = S(24, 82);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  42, 238), S(  39, 263), S(  14, 240), S(  47, 205),
 S( -35, 111), S( -47, 127), S( -16,  65), S( -10,  33),
 S( -40,  92), S( -36,  75), S( -31,  54), S( -33,  45),
 S( -44,  66), S( -43,  67), S( -35,  51), S( -25,  51),
 S( -49,  58), S( -47,  60), S( -42,  54), S( -38,  61),
 S( -41,  62), S( -33,  66), S( -37,  61), S( -29,  61),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 111, 190), S(   3, 193), S(  47, 201), S(  37, 201),
 S( -28,  65), S( -16,  60), S(   3,  35), S(  -6,  25),
 S( -51,  56), S( -31,  49), S( -43,  44), S( -10,  35),
 S( -60,  47), S( -40,  51), S( -51,  48), S( -15,  43),
 S( -77,  46), S( -43,  46), S( -51,  55), S( -29,  57),
 S( -68,  57), S( -27,  58), S( -44,  61), S( -28,  71),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-102,  68), S( -99,  96), S( -67, 110), S( -27,  90),
 S( -19, 102), S(   0, 111), S(  24, 101), S(  32, 101),
 S(  36,  86), S(  35, 101), S(  33, 118), S(  24, 120),
 S(  27, 115), S(  32, 115), S(  49, 117), S(  34, 122),
 S(  25, 118), S(  34, 110), S(  46, 129), S(  39, 127),
 S(   5, 100), S(  13, 105), S(  24, 117), S(  20, 128),
 S(  -8, 103), S(   9,  98), S(  12, 110), S(  19, 110),
 S( -33, 108), S(   8,  99), S(   4, 101), S(  13, 114),
},{
 S(-133, -96), S(-182,  68), S( -46,  52), S(  21,  85),
 S(   8,  38), S(  80,  63), S(  45,  93), S(  23, 102),
 S(  50,  57), S(  12,  76), S(  64,  64), S(  56,  97),
 S(  30,  94), S(  39,  96), S(  43, 108), S(  41, 117),
 S(  29, 114), S(  18, 105), S(  48, 121), S(  31, 135),
 S(  24,  94), S(  36,  97), S(  29, 112), S(  35, 123),
 S(  23,  99), S(  33,  92), S(  20, 101), S(  19, 106),
 S(   9, 103), S(   4,  99), S(  19,  96), S(  21, 111),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -19, 132), S( -29, 153), S( -19, 135), S( -37, 149),
 S(   5, 135), S(  -8, 126), S(  35, 128), S(  22, 140),
 S(  26, 139), S(  33, 139), S(  -3, 126), S(  34, 135),
 S(  21, 141), S(  44, 137), S(  35, 140), S(  22, 160),
 S(  39, 129), S(  30, 140), S(  37, 143), S(  47, 139),
 S(  35, 123), S(  50, 137), S(  36, 123), S(  40, 139),
 S(  37, 124), S(  38,  95), S(  49, 115), S(  34, 132),
 S(  29, 102), S(  48, 116), S(  37, 133), S(  39, 132),
},{
 S( -47,  92), S(  54, 116), S( -27, 126), S( -57, 126),
 S(  17,  90), S( -16, 121), S(   2, 139), S(  32, 122),
 S(  44, 126), S(  13, 131), S(  18, 126), S(  30, 136),
 S(  12, 128), S(  44, 133), S(  44, 137), S(  42, 139),
 S(  55, 107), S(  45, 130), S(  50, 142), S(  39, 142),
 S(  47, 114), S(  63, 125), S(  44, 124), S(  41, 148),
 S(  49,  99), S(  49,  96), S(  50, 126), S(  44, 132),
 S(  44,  82), S(  41, 147), S(  32, 136), S(  49, 128),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -36, 266), S( -28, 270), S( -30, 275), S( -58, 279),
 S( -19, 250), S( -26, 261), S(  -6, 259), S(   9, 244),
 S( -32, 256), S(   5, 251), S(  -6, 251), S(  -2, 244),
 S( -24, 268), S(  -8, 263), S(   6, 260), S(  10, 244),
 S( -29, 266), S( -21, 266), S( -15, 263), S(  -7, 253),
 S( -32, 258), S( -22, 247), S( -15, 250), S( -12, 246),
 S( -30, 250), S( -23, 251), S(  -8, 247), S(  -5, 238),
 S( -20, 252), S( -21, 248), S( -17, 252), S(  -7, 238),
},{
 S(  32, 253), S( -25, 276), S(-101, 290), S( -34, 268),
 S(   0, 235), S(   9, 248), S( -15, 250), S( -20, 250),
 S( -29, 237), S(  28, 230), S(   3, 218), S(  28, 222),
 S( -20, 241), S(  20, 233), S(  15, 220), S(  16, 234),
 S( -36, 240), S( -14, 241), S( -11, 234), S(   1, 242),
 S( -24, 219), S(  -5, 219), S(  -6, 220), S(  -2, 233),
 S( -21, 228), S(   7, 212), S(  -4, 219), S(   0, 229),
 S( -10, 225), S( -10, 232), S(  -9, 229), S(  -1, 229),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -83, 548), S(   2, 450), S(  31, 458), S(  20, 486),
 S( -18, 484), S( -14, 473), S(   2, 499), S( -10, 508),
 S(   9, 462), S(  16, 456), S(   1, 496), S(  21, 491),
 S(  12, 488), S(  14, 510), S(  16, 512), S(  -4, 531),
 S(  18, 476), S(   6, 520), S(   7, 516), S(   0, 540),
 S(  15, 456), S(  20, 474), S(  16, 491), S(  11, 495),
 S(  21, 428), S(  22, 437), S(  28, 441), S(  28, 453),
 S(  11, 443), S(  11, 438), S(  14, 447), S(  20, 453),
},{
 S(  69, 426), S( 143, 396), S( -78, 532), S(  36, 442),
 S(  49, 430), S(  27, 427), S(   2, 475), S( -18, 514),
 S(  42, 426), S(  14, 420), S(  29, 446), S(  13, 492),
 S(  16, 464), S(  27, 467), S(  38, 441), S(   2, 518),
 S(  33, 435), S(  25, 470), S(  23, 480), S(   3, 533),
 S(  28, 409), S(  34, 438), S(  28, 470), S(  15, 494),
 S(  49, 380), S(  46, 379), S(  40, 405), S(  31, 452),
 S(  47, 402), S(  12, 422), S(  16, 405), S(  21, 433),
}};

const Score KING_PSQT[2][32] = {{
 S( 211,-113), S( 160, -52), S(  15, -15), S(  97, -32),
 S( -61,  42), S(  42,  48), S(  29,  45), S(  50,  14),
 S(  55,  25), S(  77,  46), S(  68,  47), S(  16,  49),
 S(  64,  23), S(  46,  44), S(  11,  51), S( -33,  47),
 S(  -4,  15), S(  26,  32), S(  27,  28), S( -60,  34),
 S( -20,  -3), S(   5,  15), S( -11,  13), S( -32,   8),
 S(   8, -19), S( -17,  14), S( -35,   2), S( -52,  -7),
 S(  15, -77), S(  24, -35), S(  -4, -32), S( -28, -47),
},{
 S(  29,-124), S(-213,  60), S(  12, -19), S( -66, -16),
 S(-136,   7), S(  -1,  70), S( -59,  66), S( 135,  15),
 S( -35,  12), S( 118,  63), S( 186,  53), S(  89,  46),
 S( -62,  10), S(  43,  47), S(  11,  64), S( -28,  56),
 S( -72,   2), S( -47,  36), S(   1,  35), S( -41,  38),
 S(  -7, -11), S(  23,  13), S(  -6,  17), S( -14,  12),
 S(  10,  -9), S(   4,  16), S( -27,   9), S( -51,  -1),
 S(  12, -57), S(  16, -11), S( -12, -19), S(  -9, -50),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -47,  15), S(   5,  26), S(  29,  24), S(  59,  38),
 S(  14,   2), S(  39,  21), S(  29,  27), S(  42,  36),
 S(  22,  -6), S(  32,   8), S(  21,  11), S(  19,  21),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -5,  12), S(  23,   8), S(  58,  14), S(  61,   8),
 S(   3,  -3), S(  26,  10), S(  40,   6), S(  51,  10),
 S(  -6,  20), S(  46,   6), S(  31,   9), S(  37,  21),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -46,  25), S(  -5,  96), S(  13, 134), S(  26, 146),
 S(  35, 156), S(  42, 165), S(  51, 165), S(  59, 163),
 S(  68, 150),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -3,  76), S(  19, 106), S(  31, 128), S(  38, 149),
 S(  45, 158), S(  49, 167), S(  51, 174), S(  55, 177),
 S(  56, 178), S(  60, 179), S(  68, 176), S(  82, 170),
 S(  84, 174), S(  89, 166),};

const Score ROOK_MOBILITIES[15] = {
 S( -39,  -5), S( -35, 176), S( -20, 211), S( -12, 215),
 S( -13, 238), S( -11, 248), S( -17, 258), S( -12, 259),
 S(  -7, 262), S(  -2, 265), S(   2, 268), S(   2, 271),
 S(   6, 274), S(  17, 274), S(  34, 266),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1493,-1274), S( -90,-178), S( -38, 357), S( -15, 482),
 S(  -4, 528), S(   1, 539), S(   1, 568), S(   1, 593),
 S(   3, 611), S(   6, 614), S(   8, 620), S(  11, 620),
 S(  10, 624), S(  14, 621), S(  13, 621), S(  20, 609),
 S(  19, 605), S(  19, 600), S(  28, 573), S(  49, 537),
 S(  71, 495), S(  91, 448), S( 131, 406), S( 186, 308),
 S( 170, 298), S( 323, 146), S( 115, 189), S( 182, 107),
};

const Score MINOR_BEHIND_PAWN = S(5, 18);

const Score KNIGHT_OUTPOST_REACHABLE = S(11, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(7, 7);

const Score BISHOP_TRAPPED = S(-119, -227);

const Score ROOK_TRAPPED = S(-54, -25);

const Score BAD_BISHOP_PAWNS = S(-1, -5);

const Score DRAGON_BISHOP = S(23, 16);

const Score ROOK_OPEN_FILE = S(26, 17);

const Score ROOK_SEMI_OPEN = S(6, 14);

const Score DEFENDED_PAWN = S(12, 10);

const Score DOUBLED_PAWN = S(10, -33);

const Score ISOLATED_PAWN[4] = {
 S(   1,  -6), S(  -2, -13), S(  -7,  -7), S(  -2, -12),
};

const Score OPEN_ISOLATED_PAWN = S(-6, -9);

const Score BACKWARDS_PAWN = S(-10, -17);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  81,  53), S(  28,  40), S(  11,  12),
 S(   6,   2), S(   5,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 112, 151), S(   8,  78),
 S( -14,  61), S( -25,  40), S( -36,  13), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -11);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  28, 244), S(   9, 288), S(  -8, 148),
 S( -26,  79), S( -13,  49), S( -11,  44), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -13);

const Score PASSED_PAWN_KING_PROXIMITY = S(-9, 27);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(23, 32);

const Score KNIGHT_THREATS[6] = { S(0, 21), S(-2, 39), S(28, 36), S(76, 1), S(52, -66), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(1, 19), S(21, 30), S(-61, 92), S(56, 17), S(63, 81), S(0, 0),};

const Score ROOK_THREATS[6] = { S(3, 27), S(32, 42), S(31, 49), S(3, 22), S(76, 2), S(0, 0),};

const Score KING_THREAT = S(16, 32);

const Score PAWN_THREAT = S(72, 28);

const Score PAWN_PUSH_THREAT = S(18, 18);

const Score HANGING_THREAT = S(6, 7);

const Score SPACE[15] = {
 S( 877, -23), S( 161, -14), S(  66,  -7), S(  24,  -3), S(   6,   0),
 S(  -4,   2), S(  -8,   3), S( -10,   5), S( -10,   7), S(  -7,   9),
 S(  -4,   9), S(  -1,  14), S(   2,  12), S(   5,   6), S(   8,-308),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-25, -4), S(20, 80), S(-12, 57), S(-17, 12), S(4, -5), S(32, -26), S(29, -50), S(0, 0),},
 { S(-41, -3), S(13, 65), S(-20, 46), S(-30, 20), S(-26, 4), S(24, -19), S(30, -32), S(0, 0),},
 { S(-21, -10), S(14, 70), S(-38, 42), S(-13, 11), S(-14, -3), S(-4, -7), S(30, -19), S(0, 0),},
 { S(-28, 14), S(14, 71), S(-56, 41), S(-19, 29), S(-18, 20), S(-8, 12), S(-16, 16), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-19, -14), S(-25, -3), S(-25, -9), S(-29, 3), S(-58, 38), S(35, 163), S(265, 183), S(0, 0),},
 { S(-12, -4), S(2, -8), S(3, -7), S(-11, 5), S(-27, 24), S(-48, 165), S(86, 238), S(0, 0),},
 { S(40, -16), S(36, -17), S(25, -9), S(8, 0), S(-16, 23), S(-64, 141), S(-20, 205), S(0, 0),},
 { S(-1, -7), S(5, -18), S(17, -13), S(8, -15), S(-17, 10), S(-33, 111), S(-104, 211), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-2, -23), S(42, -57), S(20, -43), S(17, -35), S(5, -25), S(3, -72), S(0, 0), S(0, 0),
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
  Score s = 0;

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

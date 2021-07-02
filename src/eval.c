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

const Score BISHOP_PAIR = S(23, 85);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  59, 241), S(  55, 266), S(  28, 247), S(  60, 212),
 S(  -4, 128), S( -18, 144), S(  11,  86), S(  17,  53),
 S(  -9, 109), S(  -6,  93), S(  -2,  73), S(  -5,  64),
 S( -14,  84), S( -13,  85), S(  -6,  69), S(   3,  69),
 S( -19,  75), S( -17,  77), S( -12,  72), S( -10,  80),
 S(  -9,  78), S(  -2,  82), S(  -7,  78), S(   0,  81),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 131, 189), S(  21, 193), S(  70, 204), S(  49, 209),
 S(   4,  80), S(  16,  73), S(  30,  53), S(  23,  45),
 S( -20,  72), S(   0,  65), S( -16,  63), S(  19,  54),
 S( -29,  64), S( -10,  68), S( -23,  68), S(  14,  61),
 S( -45,  62), S( -13,  63), S( -22,  72), S(   0,  76),
 S( -35,  72), S(   4,  73), S( -13,  79), S(   1,  92),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-121,  42), S(-127,  70), S( -93,  77), S( -54,  59),
 S( -45,  72), S( -26,  77), S(  -2,  65), S(   4,  65),
 S(  10,  55), S(   8,  67), S(   7,  78), S(  -2,  80),
 S(   1,  84), S(   5,  80), S(  23,  78), S(   8,  82),
 S(   0,  80), S(   7,  76), S(  20,  88), S(  14,  86),
 S( -21,  61), S( -13,  64), S(  -1,  71), S(  -5,  85),
 S( -33,  66), S( -16,  61), S( -13,  70), S(  -7,  68),
 S( -60,  73), S( -17,  59), S( -22,  64), S( -12,  75),
},{
 S(-151,-119), S(-219,  44), S( -70,  24), S(  -5,  55),
 S( -18,   8), S(  53,  32), S(  19,  60), S(  -4,  67),
 S(  23,  28), S( -16,  44), S(  37,  26), S(  30,  58),
 S(   4,  62), S(  13,  59), S(  17,  69), S(  16,  77),
 S(   4,  76), S(  -9,  69), S(  23,  81), S(   6,  94),
 S(  -1,  56), S(  11,  55), S(   4,  65), S(   9,  81),
 S(  -2,  61), S(   7,  57), S(  -6,  62), S(  -6,  64),
 S( -19,  72), S( -21,  60), S(  -7,  59), S(  -4,  73),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -11, 107), S( -28, 128), S( -16, 109), S( -35, 124),
 S(   5, 110), S(  -7,  99), S(  36, 101), S(  21, 113),
 S(  25, 114), S(  34, 112), S(  -1,  98), S(  36, 106),
 S(  23, 115), S(  46, 108), S(  37, 110), S(  25, 130),
 S(  41, 101), S(  33, 111), S(  41, 113), S(  50, 109),
 S(  38,  93), S(  53, 108), S(  39,  94), S(  44, 108),
 S(  40,  95), S(  41,  66), S(  52,  85), S(  37, 104),
 S(  31,  72), S(  50,  87), S(  40, 103), S(  41, 104),
},{
 S( -39,  67), S(  55,  92), S( -23, 100), S( -52, 101),
 S(  17,  65), S( -15,  94), S(   4, 112), S(  33,  94),
 S(  44, 101), S(  14, 105), S(  21,  98), S(  32, 106),
 S(  13, 100), S(  47, 104), S(  47, 108), S(  45, 109),
 S(  58,  78), S(  48, 100), S(  53, 111), S(  42, 111),
 S(  50,  84), S(  66,  96), S(  47,  94), S(  45, 118),
 S(  52,  71), S(  51,  68), S(  53,  98), S(  47, 103),
 S(  47,  52), S(  44, 119), S(  35, 107), S(  51, 101),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -28, 244), S( -22, 248), S( -25, 253), S( -54, 257),
 S( -14, 229), S( -21, 239), S(   0, 238), S(  15, 223),
 S( -26, 234), S(  11, 228), S(   0, 229), S(   3, 223),
 S( -19, 246), S(  -3, 241), S(  11, 239), S(  15, 223),
 S( -24, 244), S( -17, 244), S( -10, 242), S(  -2, 232),
 S( -27, 237), S( -17, 225), S( -10, 228), S(  -8, 225),
 S( -25, 228), S( -18, 229), S(  -4, 226), S(   0, 218),
 S( -16, 232), S( -16, 227), S( -13, 233), S(  -3, 219),
},{
 S(  39, 229), S( -17, 252), S( -97, 268), S( -27, 246),
 S(   9, 210), S(  18, 224), S(  -9, 228), S( -14, 228),
 S( -22, 213), S(  35, 206), S(  10, 195), S(  33, 200),
 S( -13, 217), S(  26, 210), S(  20, 198), S(  21, 213),
 S( -31, 218), S(  -9, 218), S(  -7, 213), S(   6, 221),
 S( -18, 197), S(   1, 196), S(  -1, 199), S(   3, 213),
 S( -15, 206), S(  12, 189), S(   1, 197), S(   4, 208),
 S(  -5, 205), S(  -4, 210), S(  -5, 209), S(   3, 210),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-115, 483), S( -36, 391), S(  -6, 397), S( -18, 424),
 S( -55, 431), S( -49, 415), S( -31, 439), S( -42, 446),
 S( -26, 406), S( -18, 397), S( -32, 437), S( -12, 431),
 S( -21, 430), S( -18, 451), S( -16, 452), S( -36, 469),
 S( -14, 412), S( -26, 458), S( -24, 452), S( -31, 475),
 S( -16, 390), S( -11, 408), S( -15, 425), S( -19, 427),
 S(  -9, 360), S(  -9, 371), S(  -2, 373), S(  -3, 386),
 S( -21, 380), S( -20, 372), S( -17, 382), S( -11, 386),
},{
 S(  37, 359), S( 106, 336), S(-118, 475), S(  -1, 380),
 S(  15, 370), S(  -6, 368), S( -31, 413), S( -51, 452),
 S(   9, 367), S( -19, 360), S(  -2, 384), S( -20, 432),
 S( -16, 404), S(  -5, 406), S(   6, 379), S( -30, 456),
 S(   2, 371), S(  -6, 406), S(  -7, 414), S( -28, 469),
 S(  -3, 341), S(   4, 371), S(  -2, 403), S( -15, 427),
 S(  18, 311), S(  17, 310), S(   9, 337), S(   1, 385),
 S(  13, 341), S( -18, 356), S( -16, 341), S(  -9, 366),
}};

const Score KING_PSQT[2][32] = {{
 S( 190,-107), S( 138, -46), S(  12, -16), S(  76, -28),
 S( -57,  42), S(  46,  46), S(  29,  43), S(  42,  15),
 S(  62,  22), S(  80,  44), S(  71,  45), S(  17,  49),
 S(  66,  22), S(  49,  44), S(  14,  50), S( -29,  46),
 S(  -6,  16), S(  25,  34), S(  26,  28), S( -63,  36),
 S( -21,  -2), S(   2,  17), S( -13,  15), S( -34,   9),
 S(   9, -19), S( -16,  14), S( -36,   2), S( -53,  -7),
 S(  16, -79), S(  24, -36), S(  -3, -33), S( -28, -48),
},{
 S(  14,-124), S(-229,  60), S(  -7, -17), S( -79, -15),
 S(-125,   5), S(  -5,  70), S( -64,  65), S( 134,  15),
 S( -26,  10), S( 122,  61), S( 191,  51), S(  92,  45),
 S( -57,   9), S(  45,  47), S(  13,  63), S( -27,  56),
 S( -71,   3), S( -48,  38), S(  -2,  36), S( -44,  40),
 S(  -7, -10), S(  21,  15), S(  -8,  18), S( -18,  14),
 S(  10, -10), S(   5,  16), S( -28,   9), S( -52,  -2),
 S(  12, -59), S(  16, -11), S( -12, -20), S(  -9, -51),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -46,  10), S(   6,  21), S(  29,  23), S(  59,  38),
 S(  15,  -3), S(  39,  20), S(  29,  26), S(  42,  36),
 S(  21,  -3), S(  33,   9), S(  22,  12), S(  20,  23),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -2,   7), S(  24,   5), S(  59,  12), S(  61,   8),
 S(   4,  -4), S(  25,  11), S(  41,   6), S(  52,  11),
 S(  -7,  21), S(  45,   8), S(  31,  10), S(  37,  21),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -72, -24), S( -30,  49), S( -12,  88), S(   0, 102),
 S(   9, 113), S(  17, 124), S(  26, 126), S(  34, 127),
 S(  42, 118),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -2,  54), S(  21,  78), S(  34,  99), S(  41, 120),
 S(  48, 128), S(  52, 138), S(  54, 145), S(  58, 149),
 S(  59, 151), S(  62, 153), S(  69, 150), S(  82, 145),
 S(  83, 151), S(  87, 144),};

const Score ROOK_MOBILITIES[15] = {
 S( -33, -34), S( -32, 154), S( -16, 190), S(  -8, 195),
 S( -10, 218), S(  -8, 227), S( -14, 238), S(  -9, 237),
 S(  -3, 241), S(   2, 243), S(   5, 246), S(   5, 249),
 S(   9, 252), S(  19, 252), S(  36, 243),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1526,-1281), S(-111,-254), S( -63, 288), S( -40, 419),
 S( -30, 465), S( -24, 476), S( -24, 506), S( -23, 529),
 S( -21, 545), S( -18, 548), S( -16, 553), S( -13, 553),
 S( -14, 558), S( -10, 556), S( -12, 557), S(  -5, 546),
 S(  -7, 544), S(  -8, 540), S(   1, 515), S(  20, 481),
 S(  43, 439), S(  61, 394), S( 102, 353), S( 154, 256),
 S( 140, 245), S( 287,  98), S(  97, 135), S( 144,  66),
};

const Score MINOR_BEHIND_PAWN = S(5, 17);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 20);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 7);

const Score BISHOP_TRAPPED = S(-117, -234);

const Score ROOK_TRAPPED = S(-55, -27);

const Score BAD_BISHOP_PAWNS = S(-1, -5);

const Score DRAGON_BISHOP = S(23, 16);

const Score ROOK_OPEN_FILE = S(27, 17);

const Score ROOK_SEMI_OPEN = S(7, 12);

const Score DEFENDED_PAWN = S(12, 11);

const Score DOUBLED_PAWN = S(11, -34);

const Score ISOLATED_PAWN[4] = {
 S(   1,  -6), S(  -2, -13), S(  -7,  -7), S(  -2, -12),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -10);

const Score BACKWARDS_PAWN = S(-10, -18);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  87,  49), S(  29,  39), S(  12,  11),
 S(   6,   2), S(   5,   1), S(   2,   0), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 117, 154), S(   8,  79),
 S( -13,  60), S( -23,  37), S( -36,  12), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -10);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  54, 248), S(  16, 281), S(  -3, 141),
 S( -22,  74), S(  -9,  45), S(  -9,  42), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(2, -12);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 26);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(22, 34);

const Score KNIGHT_THREATS[6] = { S(0, 19), S(-2, 50), S(29, 33), S(77, 1), S(50, -55), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(1, 19), S(21, 35), S(-60, 91), S(57, 12), S(62, 89), S(0, 0),};

const Score ROOK_THREATS[6] = { S(3, 27), S(32, 41), S(30, 53), S(3, 22), S(79, -10), S(0, 0),};

const Score KING_THREAT = S(16, 32);

const Score PAWN_THREAT = S(72, 31);

const Score PAWN_PUSH_THREAT = S(18, 19);

const Score HANGING_THREAT = S(5, 7);

const Score SPACE[15] = {
 S( 786, -23), S( 152, -14), S(  66, -10), S(  26,  -6), S(   6,  -2),
 S(  -3,   0), S(  -7,   3), S(  -9,   5), S(  -9,   8), S(  -7,  12),
 S(  -4,  14), S(  -2,  19), S(   1,  19), S(   3,  13), S(   5,-295),
};

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(5, 16), S(0, 0),},
 { S(1, 13), S(1, -17), S(0, 0),},
 { S(-5, 17), S(-11, -15), S(-5, -15), S(0, 0),},
 { S(17, 25), S(7, 5), S(16, 24), S(-40, 25), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-24, -5), S(25, 78), S(-10, 53), S(-16, 10), S(4, -6), S(32, -26), S(29, -49), S(0, 0),},
 { S(-42, -3), S(16, 64), S(-19, 44), S(-30, 20), S(-26, 4), S(24, -19), S(31, -33), S(0, 0),},
 { S(-22, -10), S(20, 71), S(-35, 40), S(-12, 10), S(-14, -3), S(-4, -7), S(30, -18), S(0, 0),},
 { S(-29, 15), S(21, 72), S(-56, 42), S(-20, 30), S(-18, 20), S(-8, 11), S(-16, 17), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-21, -13), S(-25, -3), S(-25, -9), S(-29, 2), S(-59, 39), S(37, 162), S(275, 175), S(0, 0),},
 { S(-12, -3), S(2, -7), S(2, -7), S(-12, 5), S(-28, 25), S(-49, 164), S(92, 230), S(0, 0),},
 { S(41, -17), S(38, -18), S(27, -10), S(7, 0), S(-15, 22), S(-66, 140), S(-7, 200), S(0, 0),},
 { S(-2, -7), S(4, -17), S(16, -12), S(7, -16), S(-18, 10), S(-35, 112), S(-103, 211), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-6, -26), S(40, -55), S(20, -43), S(17, -35), S(4, -25), S(3, -75), S(0, 0), S(0, 0),
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

  for (int i = KNIGHT_TYPE; i < KING_TYPE; i++) {
    for (int j = PAWN_TYPE; j < i; j++) {
      s += IMBALANCE[i][j] * bits(board->pieces[2 * i + side]) * bits(board->pieces[2 * j + xside]);

      if (T)
        C.imbalance[i][j] += bits(board->pieces[2 * i + side]) * bits(board->pieces[2 * j + xside]) * cs[side];
    }
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

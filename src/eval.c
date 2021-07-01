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

const Score BISHOP_PAIR = S(20, 66);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  30, 191), S(  25, 213), S(  14, 184), S(  45, 157),
 S( -41,  74), S( -52,  84), S( -23,  35), S( -17,   9),
 S( -45,  55), S( -41,  42), S( -36,  25), S( -36,  17),
 S( -49,  35), S( -48,  36), S( -39,  21), S( -30,  21),
 S( -54,  28), S( -51,  30), S( -46,  25), S( -42,  29),
 S( -47,  32), S( -39,  35), S( -43,  30), S( -39,  32),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  78, 155), S(   2, 151), S(  15, 163), S(  28, 155),
 S( -39,  35), S( -23,  27), S(  -6,  10), S( -13,  -1),
 S( -55,  26), S( -35,  19), S( -46,  17), S( -20,   9),
 S( -67,  21), S( -45,  22), S( -51,  19), S( -22,  15),
 S( -75,  18), S( -44,  17), S( -53,  25), S( -35,  26),
 S( -69,  27), S( -31,  26), S( -48,  30), S( -34,  36),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-114,  33), S( -78,  41), S( -64,  57), S( -38,  42),
 S( -29,  53), S( -20,  62), S(   9,  50), S(  10,  53),
 S(  17,  38), S(  15,  53), S(  12,  68), S(   5,  67),
 S(  11,  62), S(  15,  64), S(  27,  66), S(  14,  68),
 S(  11,  64), S(  16,  61), S(  26,  74), S(  20,  71),
 S(  -6,  49), S(  -1,  56), S(   8,  65), S(   5,  73),
 S( -17,  54), S(  -3,  48), S(  -2,  58), S(   3,  60),
 S( -41,  61), S(  -5,  49), S(  -8,  51), S(  -1,  62),
},{
 S(-139,-100), S(-137,  12), S( -94,  19), S(   5,  35),
 S( -18,  -2), S(  55,  22), S(  33,  37), S(   2,  54),
 S(  21,  11), S(  -6,  26), S(  35,  17), S(  35,  45),
 S(   7,  43), S(  18,  45), S(  19,  54), S(  19,  64),
 S(  10,  59), S(  -4,  54), S(  27,  64), S(  12,  77),
 S(   4,  49), S(  19,  46), S(  13,  59), S(  17,  69),
 S(   7,  46), S(  18,  43), S(   5,  51), S(   4,  56),
 S(  -4,  51), S( -10,  50), S(   4,  47), S(   6,  58),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -23,  77), S( -20,  91), S( -31,  82), S( -48,  93),
 S(  -8,  86), S( -18,  76), S(  16,  78), S(   6,  85),
 S(  15,  84), S(  15,  86), S(  -8,  72), S(  16,  82),
 S(   8,  88), S(  27,  83), S(  18,  85), S(   5, 101),
 S(  22,  75), S(  17,  83), S(  20,  87), S(  30,  84),
 S(  19,  71), S(  30,  83), S(  21,  70), S(  23,  83),
 S(  19,  72), S(  22,  47), S(  30,  65), S(  19,  79),
 S(  14,  52), S(  28,  67), S(  20,  79), S(  24,  78),
},{
 S( -43,  43), S(  32,  66), S( -30,  74), S( -57,  74),
 S( -13,  49), S( -29,  68), S( -15,  87), S(  15,  71),
 S(  27,  71), S(  -8,  78), S(   2,  77), S(  11,  81),
 S(  -2,  74), S(  27,  76), S(  24,  83), S(  22,  85),
 S(  35,  56), S(  27,  75), S(  31,  86), S(  21,  87),
 S(  28,  65), S(  43,  71), S(  29,  69), S(  25,  91),
 S(  30,  53), S(  30,  49), S(  33,  73), S(  27,  79),
 S(  24,  44), S(  23,  91), S(  15,  83), S(  32,  75),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -65, 158), S( -55, 160), S( -61, 164), S( -84, 167),
 S( -45, 143), S( -53, 151), S( -35, 149), S( -23, 136),
 S( -55, 147), S( -25, 143), S( -34, 143), S( -34, 138),
 S( -50, 158), S( -36, 153), S( -24, 151), S( -20, 138),
 S( -56, 157), S( -46, 157), S( -44, 156), S( -33, 143),
 S( -58, 151), S( -48, 142), S( -42, 143), S( -39, 139),
 S( -56, 146), S( -48, 143), S( -38, 142), S( -34, 134),
 S( -48, 147), S( -48, 144), S( -45, 148), S( -37, 135),
},{
 S( -16, 147), S( -61, 163), S(-129, 176), S( -62, 159),
 S( -47, 134), S( -35, 143), S( -50, 140), S( -43, 139),
 S( -64, 133), S( -16, 127), S( -33, 114), S(  -9, 118),
 S( -54, 138), S( -19, 127), S( -21, 118), S( -16, 129),
 S( -66, 137), S( -46, 134), S( -41, 127), S( -27, 135),
 S( -56, 120), S( -41, 118), S( -36, 117), S( -31, 128),
 S( -56, 131), S( -28, 113), S( -34, 116), S( -30, 125),
 S( -39, 124), S( -43, 131), S( -40, 126), S( -32, 127),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -71, 332), S( -16, 265), S(  27, 251), S(   6, 278),
 S( -20, 282), S( -21, 272), S(  -6, 293), S( -10, 295),
 S(   2, 266), S(  10, 258), S(  -2, 288), S(  12, 283),
 S(   4, 293), S(   8, 306), S(  11, 301), S(  -9, 315),
 S(  12, 272), S(   1, 311), S(   3, 305), S(  -5, 322),
 S(   7, 262), S(  14, 267), S(  10, 282), S(   7, 285),
 S(  12, 237), S(  16, 238), S(  21, 241), S(  20, 252),
 S(   7, 247), S(   7, 241), S(   9, 248), S(  15, 249),
},{
 S(  44, 244), S(  91, 225), S( -67, 308), S(  19, 240),
 S(  33, 226), S(  13, 228), S(  -5, 268), S( -20, 293),
 S(  29, 216), S(   3, 214), S(  17, 233), S(   7, 276),
 S(   4, 258), S(  15, 258), S(  27, 233), S(  -4, 302),
 S(  22, 231), S(  16, 261), S(  17, 267), S(   0, 315),
 S(  17, 214), S(  24, 233), S(  20, 262), S(  10, 284),
 S(  38, 192), S(  35, 187), S(  32, 208), S(  23, 252),
 S(  35, 202), S(   9, 223), S(  12, 209), S(  17, 233),
}};

const Score KING_PSQT[2][32] = {{
 S( 148, -88), S( 118, -38), S(  -2,  -9), S(  84, -29),
 S( -30,  30), S(  31,  37), S(  30,  34), S(  57,   8),
 S(  55,  16), S(  82,  32), S(  81,  33), S(  30,  32),
 S(  64,  16), S(  63,  31), S(  22,  37), S(  -9,  33),
 S(  -9,  12), S(  32,  21), S(  39,  20), S( -34,  24),
 S( -16,  -3), S(   6,  11), S(  -2,  10), S( -20,   6),
 S(   3, -17), S( -16,  10), S( -31,   3), S( -49,  -3),
 S(   8, -65), S(  16, -28), S(  -7, -25), S( -30, -35),
},{
 S(  67,-106), S(-156,  36), S(  10, -14), S( -21, -22),
 S(-102,   2), S(   1,  48), S(   2,  43), S( 134,   7),
 S(  -4,   3), S( 132,  43), S( 223,  32), S( 101,  29),
 S( -27,   2), S(  60,  32), S(  41,  44), S(  -3,  40),
 S( -52,  -2), S( -24,  24), S(   9,  25), S( -21,  28),
 S(  -3, -11), S(  23,   9), S(   5,  11), S(  -6,   9),
 S(   6,  -8), S(   1,  13), S( -25,   9), S( -46,   1),
 S(   7, -47), S(  10, -10), S( -13, -13), S( -12, -39),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -36,  17), S(   8,  20), S(  26,  19), S(  50,  29),
 S(  14,   1), S(  34,  13), S(  24,  22), S(  35,  28),
 S(  16,  -4), S(  28,   6), S(  17,  11), S(  16,  18),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -7,  10), S(  24,   3), S(  45,  13), S(  53,   4),
 S(   1,  -2), S(  21,   9), S(  34,   4), S(  44,   8),
 S(  -5,  17), S(  37,   5), S(  27,   5), S(  30,  16),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -49, -30), S( -22,  37), S(  -7,  68), S(   3,  78),
 S(  11,  85), S(  18,  92), S(  25,  91), S(  32,  89),
 S(  39,  78),};

const Score BISHOP_MOBILITIES[14] = {
 S( -17,  15), S(  -3,  43), S(   7,  60), S(  12,  78),
 S(  18,  85), S(  21,  93), S(  23,  98), S(  26, 100),
 S(  27, 101), S(  29, 102), S(  36,  98), S(  49,  93),
 S(  55,  95), S(  51,  92),};

const Score ROOK_MOBILITIES[15] = {
 S( -62, -38), S( -51,  85), S( -39, 113), S( -33, 117),
 S( -34, 136), S( -33, 144), S( -37, 151), S( -33, 152),
 S( -28, 154), S( -24, 155), S( -21, 158), S( -21, 160),
 S( -18, 161), S(  -9, 162), S(   8, 153),};

const Score QUEEN_MOBILITIES[28] = {
 S(-964,-959), S( -92,-224), S(   5,  14), S(  -7, 197),
 S(  -1, 250), S(   2, 265), S(   1, 288), S(   2, 308),
 S(   3, 324), S(   5, 329), S(   7, 333), S(   8, 334),
 S(   8, 336), S(  11, 336), S(  10, 337), S(  15, 327),
 S(  14, 324), S(  14, 322), S(  18, 301), S(  38, 272),
 S(  53, 239), S(  65, 204), S( 104, 162), S( 135,  95),
 S( 157,  57), S( 265, -51), S( 150, -66), S( 155,-108),
};

const Score MINOR_BEHIND_PAWN = S(4, 15);

const Score KNIGHT_OUTPOST_REACHABLE = S(9, 16);

const Score BISHOP_OUTPOST_REACHABLE = S(5, 6);

const Score BISHOP_TRAPPED = S(-110, -176);

const Score ROOK_TRAPPED = S(-50, -19);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(19, 14);

const Score ROOK_OPEN_FILE = S(22, 14);

const Score ROOK_SEMI_OPEN = S(5, 13);

const Score DEFENDED_PAWN = S(10, 8);

const Score DOUBLED_PAWN = S(7, -27);

const Score ISOLATED_PAWN[4] = {
 S(   0,  -4), S(  -3, -10), S(  -5,  -6), S(  -3,  -9),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -7);

const Score BACKWARDS_PAWN = S(-8, -13);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  77,  42), S(  26,  32), S(   9,  10),
 S(   5,   2), S(   4,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S(  98, 112), S(  15,  45),
 S(  -4,  32), S( -14,  15), S( -27,   1), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  19, 191), S(  10, 231), S(  -3, 116),
 S( -19,  62), S(  -8,  38), S(  -7,  34), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -10);

const Score PASSED_PAWN_KING_PROXIMITY = S(-7, 22);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(20, 25);

const Score KNIGHT_THREATS[6] = { S(0, 18), S(-1, 8), S(26, 30), S(70, 2), S(45, -50), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(1, 15), S(19, 27), S(-46, 73), S(51, 15), S(55, 70), S(0, 0),};

const Score ROOK_THREATS[6] = { S(2, 22), S(30, 33), S(29, 39), S(5, 16), S(66, 5), S(0, 0),};

const Score KING_THREAT = S(16, 27);

const Score PAWN_THREAT = S(63, 22);

const Score PAWN_PUSH_THREAT = S(16, 14);

const Score HANGING_THREAT = S(6, 8);

const Score SPACE[15] = {
 S( 686, -15), S( 126, -11), S(  51,  -6), S(  19,  -3), S(   5,   0),
 S(  -3,   1), S(  -7,   3), S(  -9,   4), S(  -8,   5), S(  -6,   7),
 S(  -3,   7), S(  -2,  11), S(   1,  10), S(   3,   5), S(   7,-243),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-20, -5), S(28, 47), S(-5, 41), S(-14, 9), S(4, -7), S(26, -24), S(24, -43), S(0, 0),},
 { S(-33, -4), S(19, 40), S(-11, 38), S(-24, 15), S(-20, 1), S(21, -19), S(25, -30), S(0, 0),},
 { S(-17, -7), S(12, 49), S(-33, 37), S(-10, 10), S(-11, -1), S(-2, -5), S(27, -17), S(0, 0),},
 { S(-23, 9), S(30, 46), S(-48, 34), S(-14, 22), S(-14, 14), S(-7, 7), S(-9, 6), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-13, -13), S(-19, -4), S(-19, -9), S(-27, 1), S(-48, 29), S(26, 135), S(210, 165), S(0, 0),},
 { S(-13, -5), S(3, -9), S(4, -8), S(-11, 3), S(-24, 18), S(-48, 139), S(72, 197), S(0, 0),},
 { S(32, -16), S(29, -17), S(19, -9), S(5, -2), S(-15, 16), S(-58, 116), S(-57, 183), S(0, 0),},
 { S(0, -8), S(4, -18), S(15, -13), S(6, -14), S(-15, 6), S(-24, 89), S(-98, 181), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-10, -8), S(31, -51), S(17, -38), S(14, -31), S(4, -22), S(2, -61), S(0, 0), S(0, 0),
};

const Score KS_ATTACKER_WEIGHTS[5] = {
 0, 32, 31, 18, 24
};

const Score KS_ATTACK = 4;

const Score KS_WEAK_SQS = 78;

const Score KS_PINNED = 74;

const Score KS_KNIGHT_CHECK = 270;

const Score KS_BISHOP_CHECK = 309;

const Score KS_ROOK_CHECK = 263;

const Score KS_QUEEN_CHECK = 199;

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

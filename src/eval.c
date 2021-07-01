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
 S(  31, 194), S(  25, 216), S(  15, 186), S(  47, 159),
 S( -40,  76), S( -52,  85), S( -23,  35), S( -17,   9),
 S( -45,  57), S( -41,  42), S( -36,  25), S( -36,  17),
 S( -49,  36), S( -48,  37), S( -39,  22), S( -30,  21),
 S( -54,  29), S( -51,  31), S( -46,  26), S( -42,  30),
 S( -47,  33), S( -40,  35), S( -43,  31), S( -40,  33),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  79, 158), S(   2, 153), S(  15, 166), S(  29, 157),
 S( -38,  36), S( -22,  27), S(  -5,  10), S( -13,  -1),
 S( -54,  27), S( -35,  20), S( -45,  17), S( -19,  10),
 S( -66,  22), S( -45,  23), S( -50,  20), S( -23,  15),
 S( -75,  19), S( -44,  18), S( -53,  26), S( -35,  26),
 S( -69,  28), S( -31,  27), S( -48,  31), S( -34,  37),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-112,  34), S( -75,  43), S( -62,  59), S( -35,  44),
 S( -27,  55), S( -19,  64), S(  11,  53), S(  11,  55),
 S(  18,  40), S(  16,  56), S(  13,  70), S(   6,  70),
 S(  12,  65), S(  16,  66), S(  29,  69), S(  15,  70),
 S(  12,  67), S(  17,  63), S(  28,  77), S(  21,  74),
 S(  -5,  52), S(   0,  58), S(   9,  68), S(   6,  75),
 S( -16,  56), S(  -2,  50), S(  -1,  61), S(   4,  62),
 S( -40,  64), S(  -3,  52), S(  -7,  53), S(   0,  64),
},{
 S(-139,-100), S(-138,  14), S( -92,  20), S(   7,  36),
 S( -17,   0), S(  56,  24), S(  35,  38), S(   3,  56),
 S(  22,  12), S(  -4,  28), S(  37,  18), S(  37,  47),
 S(   8,  45), S(  19,  47), S(  20,  56), S(  21,  66),
 S(  12,  61), S(  -3,  55), S(  28,  67), S(  13,  79),
 S(   6,  51), S(  20,  49), S(  14,  62), S(  18,  72),
 S(   9,  48), S(  19,  45), S(   6,  53), S(   5,  59),
 S(  -3,  53), S(  -9,  53), S(   5,  50), S(   8,  61),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -21,  78), S( -18,  93), S( -29,  84), S( -44,  94),
 S(  -7,  88), S( -17,  78), S(  17,  80), S(   7,  87),
 S(  16,  86), S(  15,  88), S(  -7,  74), S(  17,  84),
 S(   9,  90), S(  28,  85), S(  19,  87), S(   6, 103),
 S(  23,  77), S(  18,  85), S(  21,  88), S(  30,  86),
 S(  20,  73), S(  31,  85), S(  22,  72), S(  23,  85),
 S(  20,  74), S(  23,  49), S(  30,  67), S(  19,  81),
 S(  15,  54), S(  29,  69), S(  20,  81), S(  25,  80),
},{
 S( -42,  45), S(  34,  68), S( -29,  76), S( -55,  76),
 S( -13,  51), S( -27,  70), S( -13,  89), S(  16,  73),
 S(  28,  73), S(  -7,  81), S(   3,  79), S(  12,  83),
 S(  -1,  75), S(  28,  78), S(  25,  85), S(  23,  87),
 S(  36,  58), S(  28,  77), S(  32,  88), S(  22,  89),
 S(  29,  67), S(  44,  73), S(  30,  71), S(  25,  93),
 S(  31,  55), S(  31,  51), S(  34,  75), S(  27,  81),
 S(  25,  46), S(  24,  94), S(  16,  85), S(  33,  77),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -64, 160), S( -54, 163), S( -57, 166), S( -82, 169),
 S( -45, 146), S( -52, 154), S( -34, 151), S( -21, 139),
 S( -54, 150), S( -24, 146), S( -33, 145), S( -33, 140),
 S( -49, 161), S( -35, 156), S( -23, 154), S( -19, 140),
 S( -55, 160), S( -46, 160), S( -43, 159), S( -32, 146),
 S( -57, 154), S( -47, 144), S( -41, 146), S( -39, 142),
 S( -56, 149), S( -47, 146), S( -37, 145), S( -33, 137),
 S( -48, 150), S( -47, 147), S( -45, 151), S( -36, 138),
},{
 S( -15, 149), S( -61, 166), S(-129, 179), S( -60, 161),
 S( -47, 137), S( -34, 146), S( -47, 143), S( -41, 142),
 S( -63, 135), S( -15, 130), S( -31, 116), S(  -8, 120),
 S( -54, 140), S( -18, 129), S( -20, 121), S( -15, 132),
 S( -66, 140), S( -45, 137), S( -41, 130), S( -27, 137),
 S( -55, 122), S( -40, 121), S( -35, 120), S( -31, 131),
 S( -56, 134), S( -28, 116), S( -34, 119), S( -30, 128),
 S( -39, 127), S( -42, 134), S( -39, 129), S( -32, 130),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -75, 342), S( -20, 273), S(  27, 256), S(   3, 285),
 S( -26, 291), S( -28, 281), S( -11, 302), S( -14, 301),
 S(  -4, 275), S(   4, 266), S(  -8, 296), S(   7, 292),
 S(  -2, 302), S(   2, 316), S(   5, 311), S( -15, 326),
 S(   6, 281), S(  -5, 323), S(  -3, 317), S( -12, 335),
 S(   1, 273), S(   8, 278), S(   4, 293), S(   1, 296),
 S(   6, 247), S(  10, 248), S(  15, 251), S(  14, 262),
 S(   1, 257), S(   1, 251), S(   3, 257), S(   9, 259),
},{
 S(  40, 248), S(  89, 225), S( -74, 318), S(  15, 248),
 S(  24, 230), S(   6, 233), S( -11, 279), S( -25, 301),
 S(  23, 221), S(  -4, 221), S(  11, 240), S(   1, 284),
 S(  -2, 265), S(   9, 265), S(  21, 241), S( -10, 311),
 S(  17, 238), S(  10, 270), S(  11, 276), S(  -6, 326),
 S(  12, 221), S(  18, 242), S(  14, 272), S(   4, 295),
 S(  32, 200), S(  30, 196), S(  26, 218), S(  17, 262),
 S(  30, 209), S(   3, 231), S(   6, 218), S(  11, 243),
}};

const Score KING_PSQT[2][32] = {{
 S( 139, -87), S( 128, -39), S(   3, -11), S(  94, -32),
 S( -30,  30), S(  34,  37), S(  40,  32), S(  67,   6),
 S(  62,  15), S(  88,  32), S(  92,  30), S(  41,  31),
 S(  72,  15), S(  72,  31), S(  30,  36), S(  -3,  32),
 S(  -6,  12), S(  38,  20), S(  46,  18), S( -29,  23),
 S( -15,  -3), S(   8,  11), S(   0,   9), S( -18,   5),
 S(   3, -17), S( -15,  10), S( -31,   2), S( -50,  -4),
 S(   8, -65), S(  16, -28), S(  -7, -25), S( -31, -35),
},{
 S(  67,-105), S(-161,  38), S(  -1, -12), S( -18, -22),
 S(-103,   3), S(   0,  49), S(   6,  43), S( 146,   7),
 S(   2,   4), S( 139,  44), S( 242,  30), S( 114,  28),
 S( -26,   3), S(  66,  33), S(  49,  44), S(   8,  39),
 S( -48,  -1), S( -19,  25), S(  15,  25), S( -14,  28),
 S(   0, -10), S(  26,  10), S(   9,  11), S(  -2,   9),
 S(   9,  -7), S(   4,  14), S( -22,   9), S( -45,   1),
 S(   8, -46), S(  13,  -8), S( -11, -13), S( -11, -38),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -36,  17), S(   8,  20), S(  26,  19), S(  50,  29),
 S(  13,   1), S(  34,  13), S(  24,  22), S(  36,  28),
 S(  16,  -4), S(  28,   6), S(  17,  11), S(  16,  18),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -7,  10), S(  24,   3), S(  45,  13), S(  53,   4),
 S(   1,  -2), S(  21,   9), S(  34,   4), S(  44,   8),
 S(  -4,  17), S(  37,   5), S(  27,   6), S(  30,  16),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -48, -29), S( -21,  38), S(  -7,  69), S(   4,  80),
 S(  12,  86), S(  18,  94), S(  25,  93), S(  32,  91),
 S(  40,  79),};

const Score BISHOP_MOBILITIES[14] = {
 S( -16,  17), S(  -2,  45), S(   8,  62), S(  13,  81),
 S(  19,  87), S(  22,  95), S(  24, 100), S(  27, 102),
 S(  28, 103), S(  30, 104), S(  37, 100), S(  51,  96),
 S(  56,  97), S(  53,  94),};

const Score ROOK_MOBILITIES[15] = {
 S( -61, -34), S( -50,  90), S( -38, 118), S( -31, 122),
 S( -33, 141), S( -31, 149), S( -35, 156), S( -31, 157),
 S( -27, 159), S( -22, 160), S( -20, 162), S( -20, 164),
 S( -16, 166), S(  -7, 166), S(  10, 157),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1152,-1111), S(-101,-214), S(  -3,  31), S( -16, 215),
 S( -10, 269), S(  -7, 284), S(  -8, 306), S(  -7, 326),
 S(  -6, 342), S(  -4, 346), S(  -3, 350), S(  -1, 351),
 S(  -1, 353), S(   1, 353), S(   1, 353), S(   6, 342),
 S(   5, 337), S(   5, 336), S(  10, 312), S(  30, 282),
 S(  46, 246), S(  60, 208), S( 101, 163), S( 131,  93),
 S( 164,  46), S( 257, -55), S( 147, -75), S( 140,-115),
};

const Score MINOR_BEHIND_PAWN = S(4, 15);

const Score KNIGHT_OUTPOST_REACHABLE = S(9, 16);

const Score BISHOP_OUTPOST_REACHABLE = S(5, 6);

const Score BISHOP_TRAPPED = S(-110, -178);

const Score ROOK_TRAPPED = S(-50, -19);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(19, 14);

const Score ROOK_OPEN_FILE = S(22, 14);

const Score ROOK_SEMI_OPEN = S(5, 14);

const Score DEFENDED_PAWN = S(10, 9);

const Score DOUBLED_PAWN = S(7, -27);

const Score ISOLATED_PAWN[4] = {
 S(  -1,  -4), S(  -2, -10), S(  -5,  -6), S(  -3,  -9),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -7);

const Score BACKWARDS_PAWN = S(-8, -14);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  78,  43), S(  26,  33), S(   9,  10),
 S(   5,   2), S(   4,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 101, 111), S(  15,  45),
 S(  -4,  32), S( -14,  15), S( -27,   1), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  20, 193), S(  11, 233), S(  -3, 117),
 S( -19,  62), S(  -7,  38), S(  -7,  34), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -10);

const Score PASSED_PAWN_KING_PROXIMITY = S(-7, 22);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(20, 26);

const Score KNIGHT_THREATS[6] = { S(0, 18), S(-2, 10), S(26, 30), S(70, 2), S(45, -50), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 15), S(19, 27), S(-46, 72), S(51, 15), S(54, 72), S(0, 0),};

const Score ROOK_THREATS[6] = { S(2, 22), S(30, 34), S(29, 40), S(5, 16), S(65, 7), S(0, 0),};

const Score KING_THREAT = S(15, 27);

const Score PAWN_THREAT = S(63, 23);

const Score PAWN_PUSH_THREAT = S(16, 14);

const Score HANGING_THREAT = S(6, 8);

const Score SPACE[15] = {
 S( 707, -15), S( 128, -11), S(  52,  -6), S(  19,  -3), S(   5,   0),
 S(  -3,   1), S(  -7,   3), S(  -8,   4), S(  -8,   5), S(  -6,   7),
 S(  -3,   7), S(  -2,  11), S(   1,  10), S(   4,   5), S(   7,-245),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-20, -5), S(28, 47), S(-4, 40), S(-14, 8), S(3, -8), S(26, -24), S(23, -44), S(0, 0),},
 { S(-33, -4), S(20, 41), S(-11, 39), S(-24, 16), S(-21, 1), S(20, -19), S(25, -30), S(0, 0),},
 { S(-17, -7), S(12, 50), S(-33, 37), S(-9, 10), S(-11, -1), S(-2, -5), S(27, -17), S(0, 0),},
 { S(-23, 10), S(36, 46), S(-48, 34), S(-14, 22), S(-14, 14), S(-7, 7), S(-9, 6), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-13, -13), S(-19, -4), S(-18, -9), S(-26, 1), S(-47, 29), S(27, 136), S(213, 167), S(0, 0),},
 { S(-13, -4), S(3, -9), S(5, -7), S(-11, 4), S(-24, 18), S(-49, 141), S(73, 199), S(0, 0),},
 { S(32, -16), S(29, -17), S(19, -9), S(6, -2), S(-14, 17), S(-58, 118), S(-59, 185), S(0, 0),},
 { S(1, -8), S(4, -18), S(16, -13), S(6, -14), S(-15, 7), S(-24, 90), S(-97, 183), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-9, -9), S(31, -51), S(17, -38), S(15, -31), S(5, -22), S(3, -61), S(0, 0), S(0, 0),
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

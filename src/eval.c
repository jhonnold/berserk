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

const Score BISHOP_PAIR = S(19, 67);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  32, 197), S(  25, 214), S(  18, 185), S(  55, 154),
 S( -32,  87), S( -44,  93), S(  -9,  46), S(   2,  19),
 S( -41,  58), S( -38,  40), S( -33,  24), S( -31,  14),
 S( -46,  38), S( -46,  34), S( -36,  20), S( -27,  17),
 S( -50,  31), S( -50,  27), S( -44,  24), S( -38,  27),
 S( -43,  35), S( -38,  33), S( -39,  31), S( -35,  33),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  92, 161), S(   5, 152), S(  91, 151), S(  37, 153),
 S( -26,  47), S( -11,  37), S(  10,  22), S(   8,  11),
 S( -49,  29), S( -32,  17), S( -39,  15), S( -15,   7),
 S( -62,  23), S( -42,  20), S( -44,  17), S( -19,  12),
 S( -71,  21), S( -42,  15), S( -49,  24), S( -32,  24),
 S( -64,  30), S( -28,  24), S( -46,  30), S( -31,  37),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-111,  34), S( -72,  44), S( -54,  58), S( -29,  43),
 S( -22,  53), S( -13,  62), S(  18,  51), S(  19,  54),
 S(  24,  37), S(  24,  53), S(  21,  67), S(  14,  68),
 S(  20,  62), S(  24,  64), S(  36,  66), S(  24,  69),
 S(  19,  64), S(  24,  61), S(  35,  74), S(  29,  72),
 S(   3,  49), S(   8,  55), S(  17,  65), S(  14,  73),
 S(  -9,  54), S(   5,  48), S(   7,  58), S(  11,  59),
 S( -34,  62), S(   3,  50), S(   0,  51), S(   8,  62),
},{
 S(-141, -93), S(-122,  14), S( -89,  21), S(  10,  37),
 S( -10,   0), S(  65,  22), S(  44,  37), S(  12,  53),
 S(  31,  12), S(   4,  28), S(  47,  19), S(  46,  46),
 S(  16,  44), S(  27,  46), S(  29,  56), S(  28,  65),
 S(  19,  59), S(   6,  54), S(  36,  65), S(  21,  77),
 S(  13,  49), S(  27,  46), S(  21,  59), S(  26,  69),
 S(  16,  46), S(  26,  43), S(  13,  50), S(  12,  56),
 S(   4,  51), S(  -2,  50), S(  12,  47), S(  15,  58),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -17,  77), S( -12,  90), S( -24,  82), S( -34,  92),
 S(  -2,  86), S( -11,  75), S(  23,  77), S(  16,  85),
 S(  22,  84), S(  22,  86), S(  -1,  71), S(  24,  81),
 S(  16,  87), S(  35,  82), S(  25,  84), S(  13, 100),
 S(  30,  75), S(  24,  82), S(  29,  86), S(  37,  83),
 S(  28,  71), S(  39,  82), S(  29,  69), S(  31,  83),
 S(  28,  71), S(  30,  47), S(  38,  65), S(  26,  79),
 S(  23,  51), S(  36,  67), S(  27,  80), S(  33,  78),
},{
 S( -41,  44), S(  40,  67), S( -27,  76), S( -49,  74),
 S(  -6,  50), S( -20,  66), S(  -5,  86), S(  22,  71),
 S(  35,  71), S(   1,  78), S(  10,  76), S(  19,  81),
 S(   6,  74), S(  35,  76), S(  32,  83), S(  30,  84),
 S(  43,  56), S(  36,  75), S(  39,  86), S(  29,  86),
 S(  36,  65), S(  51,  71), S(  37,  68), S(  32,  91),
 S(  39,  52), S(  39,  48), S(  40,  73), S(  35,  79),
 S(  33,  43), S(  31,  91), S(  24,  83), S(  40,  75),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -47, 154), S( -35, 156), S( -35, 159), S( -61, 163),
 S( -28, 140), S( -36, 148), S( -16, 146), S(  -3, 132),
 S( -39, 145), S(  -8, 141), S( -18, 140), S( -17, 135),
 S( -34, 156), S( -19, 150), S(  -8, 149), S(  -3, 135),
 S( -40, 154), S( -30, 154), S( -27, 154), S( -17, 140),
 S( -42, 148), S( -32, 139), S( -26, 141), S( -23, 136),
 S( -41, 143), S( -32, 141), S( -22, 140), S( -18, 132),
 S( -33, 144), S( -32, 141), S( -30, 145), S( -21, 132),
},{
 S(   6, 144), S( -39, 160), S(-102, 173), S( -42, 155),
 S( -25, 132), S( -16, 140), S( -28, 137), S( -24, 136),
 S( -46, 131), S(   6, 124), S( -13, 111), S(   9, 115),
 S( -38, 136), S(  -1, 125), S(  -4, 116), S(   1, 127),
 S( -51, 136), S( -29, 133), S( -25, 125), S( -11, 132),
 S( -40, 118), S( -24, 116), S( -20, 115), S( -15, 126),
 S( -41, 128), S( -13, 111), S( -18, 114), S( -14, 123),
 S( -24, 121), S( -27, 128), S( -24, 124), S( -16, 124),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -39, 319), S(  19, 244), S(  71, 221), S(  41, 257),
 S(  10, 263), S(   8, 252), S(  26, 271), S(  26, 266),
 S(  33, 247), S(  41, 238), S(  29, 268), S(  44, 263),
 S(  35, 273), S(  39, 289), S(  42, 286), S(  22, 301),
 S(  44, 254), S(  32, 300), S(  34, 295), S(  26, 312),
 S(  39, 249), S(  46, 254), S(  41, 270), S(  38, 273),
 S(  44, 222), S(  48, 223), S(  53, 226), S(  52, 236),
 S(  40, 231), S(  38, 225), S(  41, 231), S(  47, 234),
},{
 S(  79, 205), S( 130, 175), S( -30, 284), S(  53, 221),
 S(  56, 183), S(  38, 193), S(  24, 256), S(  12, 271),
 S(  62, 188), S(  35, 182), S(  45, 210), S(  39, 253),
 S(  37, 233), S(  48, 232), S(  59, 210), S(  26, 282),
 S(  55, 204), S(  49, 240), S(  50, 249), S(  31, 301),
 S(  50, 190), S(  57, 215), S(  52, 246), S(  42, 269),
 S(  71, 169), S(  67, 171), S(  64, 192), S(  55, 236),
 S(  70, 174), S(  42, 203), S(  45, 189), S(  49, 216),
}};

const Score KING_PSQT[2][32] = {{
 S( 119, -84), S( 129, -39), S(  29, -14), S(  92, -29),
 S( -42,  31), S(  44,  33), S(  53,  31), S(  73,   7),
 S(  65,  13), S(  93,  30), S(  96,  31), S(  42,  31),
 S(  84,  11), S(  79,  29), S(  30,  36), S(  -9,  34),
 S(  -6,  11), S(  38,  20), S(  44,  20), S( -29,  25),
 S( -17,  -3), S(   8,  11), S(   2,  10), S( -19,   6),
 S(   2, -17), S( -17,  11), S( -32,   3), S( -48,  -3),
 S(   7, -64), S(  15, -28), S(  -7, -24), S( -29, -34),
},{
 S(  48,-100), S(-192,  40), S( -36,  -6), S(  -3, -23),
 S(-109,   4), S(  -1,  49), S(  12,  43), S( 150,   7),
 S(  14,   2), S( 139,  43), S( 250,  30), S( 119,  28),
 S( -33,   4), S(  67,  32), S(  45,  45), S(   0,  41),
 S( -49,  -1), S( -19,  25), S(  11,  27), S( -16,  30),
 S(  -1,  -9), S(  24,  10), S(   8,  12), S(  -3,  11),
 S(   5,  -7), S(   0,  15), S( -25,  10), S( -46,   3),
 S(   4, -46), S(   9,  -8), S( -13, -12), S( -12, -37),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -36,  17), S(   9,  20), S(  27,  18), S(  51,  27),
 S(  14,   1), S(  34,  13), S(  25,  21), S(  36,  28),
 S(  16,  -4), S(  29,   6), S(  17,  11), S(  16,  18),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -6,  10), S(  24,   4), S(  47,  12), S(  54,   3),
 S(   1,  -2), S(  21,   9), S(  36,   4), S(  45,   8),
 S(  -5,  18), S(  37,   5), S(  29,   5), S(  31,  16),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -39, -34), S( -12,  33), S(   3,  64), S(  13,  75),
 S(  21,  82), S(  28,  89), S(  35,  88), S(  42,  87),
 S(  49,  76),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -6,  13), S(   8,  40), S(  19,  57), S(  24,  75),
 S(  30,  82), S(  34,  90), S(  35,  95), S(  39,  97),
 S(  40,  99), S(  42, 100), S(  49,  96), S(  62,  92),
 S(  69,  93), S(  69,  91),};

const Score ROOK_MOBILITIES[15] = {
 S( -53, -38), S( -38,  80), S( -26, 109), S( -20, 112),
 S( -21, 132), S( -19, 140), S( -24, 147), S( -20, 148),
 S( -15, 150), S( -11, 152), S(  -8, 154), S(  -7, 156),
 S(  -4, 158), S(   5, 159), S(  23, 150),};

const Score QUEEN_MOBILITIES[28] = {
 S(-395,-429), S( -39,-224), S(  45,  -5), S(  34, 177),
 S(  38, 235), S(  41, 251), S(  40, 272), S(  41, 292),
 S(  42, 308), S(  43, 312), S(  45, 316), S(  46, 316),
 S(  46, 317), S(  49, 316), S(  48, 317), S(  53, 305),
 S(  52, 298), S(  52, 295), S(  58, 269), S(  78, 237),
 S(  96, 195), S( 112, 151), S( 157, 100), S( 189,  22),
 S( 237, -43), S( 300,-128), S( 167,-137), S(  54,-138),
};

const Score MINOR_BEHIND_PAWN = S(4, 15);

const Score KNIGHT_OUTPOST_REACHABLE = S(9, 16);

const Score BISHOP_OUTPOST_REACHABLE = S(5, 6);

const Score BISHOP_TRAPPED = S(-114, -173);

const Score ROOK_TRAPPED = S(-52, -17);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(19, 14);

const Score ROOK_OPEN_FILE = S(22, 14);

const Score ROOK_SEMI_OPEN = S(5, 14);

const Score DEFENDED_PAWN = S(10, 8);

const Score DOUBLED_PAWN = S(7, -24);

const Score OPPOSED_ISOLATED_PAWN = S(-3, -7);

const Score OPEN_ISOLATED_PAWN = S(-8, -14);

const Score BACKWARDS_PAWN = S(-8, -13);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  78,  49), S(  24,  32), S(  10,  10),
 S(   5,   2), S(   5,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -16, 219), S(  12,  52),
 S(  -7,  31), S( -17,  13), S( -28,   2), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  36, 187), S(   6, 219), S(   3, 117),
 S( -17,  63), S(  -5,  38), S(  -5,  34), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -10);

const Score PASSED_PAWN_KING_PROXIMITY = S(-4, 21);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(22, 26);

const Score KNIGHT_THREATS[6] = { S(0, 18), S(-2, 7), S(26, 30), S(71, 1), S(45, -50), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 15), S(19, 27), S(-45, 70), S(52, 15), S(54, 77), S(0, 0),};

const Score ROOK_THREATS[6] = { S(2, 23), S(30, 34), S(29, 39), S(6, 15), S(65, 11), S(0, 0),};

const Score KING_THREAT = S(15, 27);

const Score PAWN_THREAT = S(63, 23);

const Score PAWN_PUSH_THREAT = S(16, 14);

const Score HANGING_THREAT = S(6, 9);

const Score SPACE[15] = {
 S( 630, -14), S( 128, -11), S(  53,  -5), S(  20,  -2), S(   5,   1),
 S(  -3,   2), S(  -7,   4), S(  -8,   5), S(  -7,   6), S(  -6,   7),
 S(  -3,   8), S(  -2,  11), S(   1,  11), S(   3,   5), S(   7,-287),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-21, -4), S(27, 28), S(-8, 45), S(-15, 8), S(3, -7), S(26, -23), S(23, -43), S(0, 0),},
 { S(-33, -4), S(21, 21), S(-18, 37), S(-23, 14), S(-20, 1), S(22, -19), S(26, -30), S(0, 0),},
 { S(-17, -7), S(-2, 35), S(-41, 36), S(-10, 9), S(-12, -1), S(-2, -4), S(27, -16), S(0, 0),},
 { S(-24, 9), S(35, 28), S(-54, 32), S(-15, 20), S(-15, 14), S(-8, 7), S(-11, 6), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-15, -12), S(-19, -3), S(-18, -8), S(-27, 2), S(-49, 28), S(3, 132), S(226, 166), S(0, 0),},
 { S(-15, -3), S(3, -7), S(4, -6), S(-11, 4), S(-24, 18), S(-87, 135), S(76, 200), S(0, 0),},
 { S(27, -14), S(24, -15), S(16, -7), S(5, -2), S(-16, 17), S(-88, 118), S(6, 174), S(0, 0),},
 { S(1, -7), S(4, -16), S(15, -12), S(6, -13), S(-17, 7), S(-37, 92), S(-96, 182), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-4, 12), S(34, -60), S(17, -36), S(14, -30), S(4, -21), S(9, -52), S(0, 0), S(0, 0),
};

const Score KS_ATTACKER_WEIGHTS[5] = {
 0, 32, 31, 18, 24
};

const Score KS_ATTACK = 4;

const Score KS_WEAK_SQS = 78;

const Score KS_PINNED = 74;

const Score KS_SAFE_CHECK = 248;

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

  // all possible checks are used
  int safeChecks = bits(possibleKnightChecks & vulnerable) + bits(possibleBishopChecks & vulnerable) +
                   bits(possibleRookChecks & vulnerable) + bits(possibleQueenChecks & vulnerable);
  int unsafeChecks = bits(possibleKnightChecks & ~vulnerable) + bits(possibleBishopChecks & ~vulnerable) +
                     bits(possibleRookChecks & ~vulnerable);

  Score danger = data->ksAttackWeight[xside] * data->ksAttackerCount[xside] // Similar concept to toga's weight attack
                 + (KS_SAFE_CHECK * safeChecks)                             //
                 + (KS_UNSAFE_CHECK * unsafeChecks)                         //
                 + (KS_WEAK_SQS * bits(weak & kingArea))                    // weak sqs makes you vulnerable
                 + (KS_ATTACK * data->ksSqAttackCount[xside])               // general pieces aimed
                 + (KS_PINNED * bits(board->pinned & board->occupancies[side]))           //
                 + (KS_ENEMY_QUEEN * !board->pieces[QUEEN[xside]])                        //
                 + (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT_TYPE] & kingArea)); // knight f8 = no m8

  // only include this if in danger
  if (danger > 0)
    s += S(-danger * danger / 1024, -danger / 32);

  // TODO: Utilize Texel tuning for these values
  if (T) {
    C.ks += s * cs[side];
    C.danger[side] = danger;

    C.ksSafeCheck[side] = safeChecks;
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

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

const Score BISHOP_PAIR = S(20, 67);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  32, 195), S(  25, 217), S(  16, 187), S(  49, 159),
 S( -39,  76), S( -51,  85), S( -22,  36), S( -15,   9),
 S( -43,  57), S( -39,  43), S( -35,  25), S( -35,  18),
 S( -47,  37), S( -47,  37), S( -37,  22), S( -29,  22),
 S( -52,  29), S( -50,  31), S( -45,  26), S( -41,  30),
 S( -45,  33), S( -38,  36), S( -41,  31), S( -38,  33),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  83, 158), S(   2, 154), S(  35, 164), S(  31, 157),
 S( -37,  36), S( -22,  28), S(  -3,  12), S( -11,   0),
 S( -53,  27), S( -34,  20), S( -43,  18), S( -18,  10),
 S( -65,  22), S( -43,  23), S( -49,  20), S( -21,  16),
 S( -74,  19), S( -43,  18), S( -52,  26), S( -34,  27),
 S( -67,  28), S( -30,  27), S( -47,  31), S( -33,  38),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-110,  35), S( -69,  43), S( -54,  60), S( -30,  45),
 S( -25,  55), S( -15,  65), S(  15,  53), S(  16,  56),
 S(  21,  40), S(  20,  56), S(  19,  70), S(  11,  71),
 S(  17,  65), S(  20,  67), S(  33,  69), S(  20,  72),
 S(  16,  67), S(  21,  63), S(  31,  77), S(  25,  75),
 S(  -1,  52), S(   4,  58), S(  13,  68), S(  10,  76),
 S( -13,  56), S(   2,  51), S(   3,  61), S(   8,  62),
 S( -37,  64), S(   0,  52), S(  -3,  54), S(   4,  65),
},{
 S(-138, -98), S(-126,  15), S( -88,  22), S(   8,  39),
 S( -14,   2), S(  62,  25), S(  41,  40), S(   8,  56),
 S(  27,  14), S(   0,  30), S(  44,  21), S(  43,  49),
 S(  13,  47), S(  23,  49), S(  25,  59), S(  25,  68),
 S(  16,  62), S(   2,  57), S(  32,  68), S(  17,  80),
 S(   9,  51), S(  24,  49), S(  17,  62), S(  22,  72),
 S(  12,  48), S(  22,  45), S(  10,  53), S(   9,  59),
 S(   1,  54), S(  -5,  52), S(   9,  50), S(  12,  61),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -18,  79), S( -14,  93), S( -24,  84), S( -36,  94),
 S(  -5,  89), S( -14,  78), S(  21,  81), S(  13,  88),
 S(  20,  87), S(  19,  89), S(  -3,  75), S(  20,  84),
 S(  13,  91), S(  32,  86), S(  22,  88), S(   9, 104),
 S(  27,  78), S(  21,  86), S(  25,  89), S(  34,  87),
 S(  24,  74), S(  35,  86), S(  25,  73), S(  27,  86),
 S(  24,  75), S(  27,  50), S(  35,  68), S(  23,  82),
 S(  19,  55), S(  33,  70), S(  24,  83), S(  29,  82),
},{
 S( -42,  47), S(  38,  69), S( -29,  78), S( -50,  77),
 S(  -9,  53), S( -23,  70), S(  -8,  90), S(  19,  73),
 S(  31,  75), S(  -2,  82), S(   7,  79), S(  16,  84),
 S(   3,  77), S(  31,  80), S(  29,  86), S(  26,  88),
 S(  40,  59), S(  33,  78), S(  36,  89), S(  26,  90),
 S(  33,  68), S(  47,  74), S(  33,  72), S(  29,  94),
 S(  35,  56), S(  35,  52), S(  37,  77), S(  31,  82),
 S(  29,  47), S(  27,  95), S(  20,  86), S(  36,  79),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -54, 162), S( -43, 164), S( -43, 167), S( -70, 171),
 S( -36, 147), S( -43, 156), S( -24, 153), S( -11, 140),
 S( -46, 152), S( -15, 148), S( -25, 148), S( -24, 143),
 S( -42, 163), S( -27, 157), S( -15, 156), S( -11, 142),
 S( -48, 162), S( -38, 162), S( -35, 161), S( -24, 148),
 S( -50, 155), S( -39, 146), S( -34, 148), S( -31, 144),
 S( -48, 150), S( -39, 148), S( -29, 147), S( -25, 139),
 S( -40, 151), S( -40, 149), S( -37, 152), S( -29, 139),
},{
 S(  -3, 152), S( -48, 168), S(-110, 180), S( -50, 163),
 S( -34, 139), S( -23, 148), S( -35, 144), S( -32, 143),
 S( -53, 138), S(  -2, 132), S( -20, 119), S(   1, 123),
 S( -45, 143), S(  -9, 132), S( -12, 123), S(  -7, 134),
 S( -58, 143), S( -37, 140), S( -33, 132), S( -18, 139),
 S( -48, 125), S( -32, 123), S( -28, 122), S( -23, 133),
 S( -49, 136), S( -21, 118), S( -26, 121), S( -22, 130),
 S( -32, 128), S( -35, 135), S( -32, 131), S( -24, 132),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -60, 338), S(  -1, 263), S(  50, 240), S(  20, 278),
 S( -12, 284), S( -14, 274), S(   4, 293), S(   3, 288),
 S(  10, 269), S(  19, 259), S(   7, 289), S(  22, 284),
 S(  13, 295), S(  16, 311), S(  19, 309), S(  -1, 324),
 S(  21, 276), S(   9, 322), S(  11, 318), S(   2, 336),
 S(  16, 270), S(  23, 275), S(  18, 292), S(  15, 295),
 S(  21, 244), S(  25, 245), S(  30, 248), S(  29, 258),
 S(  17, 253), S(  15, 247), S(  18, 253), S(  24, 256),
},{
 S(  57, 225), S( 107, 195), S( -54, 307), S(  31, 242),
 S(  32, 205), S(  15, 212), S(   1, 278), S( -11, 292),
 S(  38, 209), S(  11, 203), S(  22, 231), S(  16, 275),
 S(  13, 256), S(  25, 254), S(  36, 231), S(   3, 305),
 S(  32, 226), S(  26, 262), S(  26, 271), S(   8, 324),
 S(  27, 211), S(  34, 236), S(  29, 268), S(  19, 292),
 S(  48, 191), S(  45, 191), S(  41, 214), S(  32, 259),
 S(  47, 196), S(  20, 222), S(  22, 212), S(  26, 238),
}};

const Score KING_PSQT[2][32] = {{
 S( 125, -86), S( 146, -43), S(  24, -13), S( 106, -32),
 S( -35,  31), S(  39,  37), S(  56,  31), S(  83,   5),
 S(  60,  15), S(  92,  31), S(  95,  31), S(  50,  30),
 S(  76,  13), S(  76,  29), S(  30,  36), S(  -6,  33),
 S(  -6,  11), S(  40,  19), S(  45,  19), S( -27,  23),
 S( -16,  -3), S(   9,  11), S(   1,   9), S( -18,   5),
 S(   3, -18), S( -15,  10), S( -32,   3), S( -48,  -3),
 S(   8, -66), S(  16, -28), S(  -7, -25), S( -30, -35),
},{
 S(  70,-105), S(-186,  40), S( -33,  -7), S(  -8, -22),
 S(-116,   4), S(   0,  49), S(  10,  44), S( 159,   6),
 S(  12,   3), S( 141,  43), S( 252,  30), S( 119,  28),
 S( -32,   3), S(  67,  32), S(  46,  44), S(   2,  40),
 S( -49,  -2), S( -18,  24), S(  12,  25), S( -16,  29),
 S(  -1, -10), S(  25,   9), S(   7,  12), S(  -3,  10),
 S(   6,  -8), S(   2,  14), S( -25,  10), S( -48,   2),
 S(   5, -47), S(  10,  -8), S( -13, -12), S( -13, -38),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -36,  17), S(   8,  21), S(  26,  19), S(  50,  28),
 S(  13,   1), S(  34,  13), S(  25,  22), S(  36,  28),
 S(  16,  -3), S(  29,   5), S(  17,  11), S(  16,  18),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -7,   9), S(  24,   4), S(  45,  12), S(  54,   3),
 S(   1,  -2), S(  22,   9), S(  35,   4), S(  45,   8),
 S(  -5,  18), S(  38,   5), S(  28,   6), S(  31,  16),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -43, -28), S( -16,  39), S(  -1,  71), S(   8,  81),
 S(  17,  88), S(  23,  96), S(  30,  95), S(  37,  93),
 S(  45,  82),};

const Score BISHOP_MOBILITIES[14] = {
 S( -11,  18), S(   3,  45), S(  14,  63), S(  19,  81),
 S(  25,  88), S(  29,  96), S(  30, 101), S(  34, 104),
 S(  34, 105), S(  37, 107), S(  44, 102), S(  58,  98),
 S(  64, 100), S(  64,  97),};

const Score ROOK_MOBILITIES[15] = {
 S( -58, -34), S( -44,  89), S( -32, 118), S( -26, 121),
 S( -27, 141), S( -26, 149), S( -30, 156), S( -26, 157),
 S( -21, 159), S( -17, 161), S( -14, 163), S( -14, 165),
 S( -11, 167), S(  -1, 168), S(  17, 159),};

const Score QUEEN_MOBILITIES[28] = {
 S(-782,-785), S( -63,-221), S(  16,  28), S(   5, 208),
 S(   9, 264), S(  12, 280), S(  11, 301), S(  12, 321),
 S(  13, 336), S(  15, 340), S(  16, 344), S(  18, 344),
 S(  18, 345), S(  20, 344), S(  19, 344), S(  24, 332),
 S(  24, 325), S(  24, 322), S(  30, 296), S(  50, 262),
 S(  68, 221), S(  86, 176), S( 128, 126), S( 161,  46),
 S( 205, -15), S( 281,-110), S( 164,-129), S(  97,-162),
};

const Score MINOR_BEHIND_PAWN = S(4, 15);

const Score KNIGHT_OUTPOST_REACHABLE = S(9, 16);

const Score BISHOP_OUTPOST_REACHABLE = S(5, 6);

const Score BISHOP_TRAPPED = S(-112, -177);

const Score ROOK_TRAPPED = S(-51, -18);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(19, 14);

const Score ROOK_OPEN_FILE = S(22, 14);

const Score ROOK_SEMI_OPEN = S(5, 14);

const Score DEFENDED_PAWN = S(10, 9);

const Score DOUBLED_PAWN = S(7, -27);

const Score ISOLATED_PAWN[4] = {
 S(  -1,  -4), S(  -2, -10), S(  -5,  -6), S(  -3,  -9),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -8);

const Score BACKWARDS_PAWN = S(-8, -14);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  81,  44), S(  27,  33), S(  10,  10),
 S(   5,   2), S(   4,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 107, 109), S(  15,  44),
 S(  -4,  32), S( -14,  15), S( -27,   1), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  24, 192), S(  12, 233), S(  -2, 117),
 S( -18,  62), S(  -8,  38), S(  -7,  34), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -10);

const Score PASSED_PAWN_KING_PROXIMITY = S(-6, 22);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(21, 26);

const Score KNIGHT_THREATS[6] = { S(0, 18), S(-2, 9), S(26, 31), S(71, 2), S(45, -49), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 15), S(19, 27), S(-46, 71), S(51, 15), S(54, 78), S(0, 0),};

const Score ROOK_THREATS[6] = { S(3, 23), S(30, 34), S(29, 40), S(6, 16), S(64, 12), S(0, 0),};

const Score KING_THREAT = S(14, 27);

const Score PAWN_THREAT = S(63, 23);

const Score PAWN_PUSH_THREAT = S(16, 14);

const Score HANGING_THREAT = S(6, 8);

const Score SPACE[15] = {
 S( 717, -15), S( 130, -11), S(  53,  -6), S(  20,  -3), S(   5,   0),
 S(  -3,   1), S(  -6,   3), S(  -8,   4), S(  -7,   5), S(  -6,   6),
 S(  -3,   7), S(  -1,  10), S(   1,  10), S(   3,   4), S(   7,-270),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-21, -5), S(31, 46), S(-5, 41), S(-15, 9), S(2, -7), S(25, -24), S(22, -43), S(0, 0),},
 { S(-32, -5), S(25, 40), S(-10, 37), S(-23, 14), S(-20, 0), S(21, -20), S(26, -31), S(0, 0),},
 { S(-17, -8), S(14, 48), S(-33, 36), S(-9, 10), S(-12, -2), S(-2, -6), S(27, -17), S(0, 0),},
 { S(-23, 8), S(46, 43), S(-47, 33), S(-14, 21), S(-14, 13), S(-7, 6), S(-10, 6), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-15, -13), S(-20, -4), S(-19, -9), S(-28, 2), S(-49, 29), S(26, 136), S(217, 168), S(0, 0),},
 { S(-14, -4), S(2, -8), S(4, -7), S(-11, 4), S(-25, 19), S(-53, 142), S(72, 201), S(0, 0),},
 { S(30, -15), S(27, -16), S(18, -8), S(4, -2), S(-15, 18), S(-61, 120), S(-42, 184), S(0, 0),},
 { S(0, -7), S(4, -18), S(15, -13), S(5, -14), S(-16, 7), S(-26, 91), S(-97, 185), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-10, -8), S(29, -50), S(16, -37), S(14, -30), S(4, -22), S(1, -61), S(0, 0), S(0, 0),
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

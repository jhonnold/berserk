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

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "endgame.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "pawns.h"
#include "string.h"
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
const Score MATERIAL_VALUES[7] = { S(78, 171), S(375, 436), S(404, 456), S(599, 916), S(1240, 1730), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(31, 94);

const Score PAWN_PSQT[2][32] = {
{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  63, 225), S(  58, 247), S(  34, 221), S(  79, 190),
 S(   9,  65), S(  -9,  77), S(  36,  21), S(  47, -14),
 S(  -8,  34), S(  -7,   9), S(   0,  -9), S(   2, -23),
 S( -15,   7), S( -18,   2), S(  -5, -13), S(   9, -17),
 S( -19,  -2), S( -20,  -7), S( -12,  -8), S(  -2,  -4),
 S( -10,   4), S(  -2,   1), S(  -2,   1), S(   5,   5),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 136, 178), S(  24, 175), S(   2, 196), S(  50, 186),
 S(  13,  17), S(  33,   4), S(  64, -14), S(  55, -25),
 S( -21,  -4), S(   0, -18), S(  -8, -20), S(  25, -33),
 S( -38, -11), S( -14, -15), S( -15, -16), S(  17, -24),
 S( -48, -14), S( -11, -21), S( -18,  -8), S(   7,  -7),
 S( -38,  -2), S(  10,  -9), S( -10,   1), S(  11,  12),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {
{
 S(-132,  33), S(-119,  61), S( -75,  74), S( -40,  65),
 S( -20,  63), S(   4,  76), S(  51,  60), S(  54,  69),
 S(  43,  46), S(  58,  58), S(  56,  78), S(  49,  86),
 S(  37,  77), S(  46,  79), S(  59,  85), S(  52,  98),
 S(  43,  74), S(  46,  73), S(  65,  90), S(  59,  97),
 S(  13,  64), S(  23,  67), S(  38,  77), S(  41,  98),
 S(   4,  71), S(  19,  66), S(  25,  78), S(  34,  78),
 S( -31,  83), S(  21,  65), S(  15,  71), S(  28,  89),
},{
 S(-172,-105), S(-153,  38), S(-140,  52), S( -25,  65),
 S(  -1,  12), S(  56,  43), S(  56,  56), S(  52,  67),
 S(  56,  28), S(  45,  40), S( 113,  38), S(  95,  63),
 S(  44,  67), S(  52,  74), S(  53,  92), S(  51,  94),
 S(  50,  78), S(  25,  80), S(  68,  89), S(  46, 109),
 S(  31,  68), S(  52,  62), S(  46,  74), S(  56,  95),
 S(  44,  72), S(  48,  63), S(  37,  71), S(  36,  77),
 S(  18,  80), S(  17,  71), S(  37,  73), S(  39,  81),
}};

const Score BISHOP_PSQT[2][32] = {
{
 S( -31,  69), S( -13,  93), S( -53,  88), S( -75, 101),
 S( -14,  87), S( -20,  68), S(  24,  78), S(  19,  85),
 S(  27,  80), S(  29,  85), S(   1,  63), S(  43,  79),
 S(  17,  88), S(  53,  80), S(  39,  87), S(  40, 108),
 S(  43,  71), S(  34,  86), S(  45,  95), S(  55,  90),
 S(  33,  74), S(  54,  91), S(  31,  75), S(  37,  96),
 S(  41,  82), S(  34,  47), S(  47,  71), S(  28,  88),
 S(  20,  49), S(  48,  74), S(  31,  87), S(  33,  85),
},{
 S( -61,  37), S(  32,  66), S( -74,  83), S( -78,  79),
 S( -15,  51), S( -48,  68), S(   1,  91), S(  33,  67),
 S(  56,  71), S(  29,  78), S(  38,  69), S(  43,  84),
 S(   6,  81), S(  51,  79), S(  46,  89), S(  49,  89),
 S(  65,  50), S(  49,  78), S(  53,  90), S(  47,  91),
 S(  43,  69), S(  60,  75), S(  39,  71), S(  40, 103),
 S(  49,  53), S(  47,  51), S(  50,  78), S(  40,  86),
 S(  40,  33), S(  36,  95), S(  27,  92), S(  42,  78),
}};

const Score ROOK_PSQT[2][32] = {
{
 S( -11, 104), S( -12, 112), S( -10, 118), S( -32, 122),
 S(   4,  88), S(  -6, 102), S(  13, 102), S(  34,  85),
 S( -12,  92), S(  26,  89), S(  15,  92), S(  17,  87),
 S(  -2, 101), S(  18,  94), S(  34,  94), S(  37,  82),
 S( -10,  98), S(   4,  98), S(  10,  98), S(  21,  87),
 S( -12,  90), S(   1,  80), S(   8,  85), S(  10,  83),
 S( -11,  84), S(   1,  82), S(  16,  81), S(  20,  76),
 S(   3,  86), S(   2,  83), S(   6,  88), S(  17,  73),
},{
 S(  79,  94), S(  34, 111), S( -48, 128), S( -13, 114),
 S(  42,  79), S(  55,  92), S(  12,  95), S(  11,  89),
 S(  -6,  84), S(  64,  74), S(  37,  64), S(  48,  66),
 S(   7,  85), S(  53,  71), S(  36,  67), S(  45,  71),
 S( -11,  81), S(  20,  76), S(   8,  74), S(  28,  76),
 S(  -3,  61), S(  24,  60), S(  15,  62), S(  24,  70),
 S(  -8,  70), S(  36,  48), S(  18,  58), S(  24,  65),
 S(  16,  57), S(  12,  70), S(  13,  66), S(  24,  64),
}};

const Score QUEEN_PSQT[2][32] = {
{
 S( -62, 170), S(   0, 106), S(  56, 109), S(  50, 140),
 S( -30, 138), S( -38, 142), S( -20, 176), S( -33, 197),
 S(  -1, 108), S(   9, 101), S(   4, 133), S(   4, 155),
 S(   1, 149), S(  13, 166), S(  14, 168), S(  -5, 207),
 S(  21, 116), S(   4, 179), S(   8, 184), S( -10, 214),
 S(   5, 119), S(  19, 130), S(   8, 155), S(   3, 161),
 S(  15,  81), S(  17,  89), S(  25,  94), S(  23, 112),
 S(   5,  92), S(   2,  88), S(   5, 100), S(  16,  95),
},{
 S( 106,  86), S( 203,  31), S(   9, 141), S(  61, 114),
 S(  63, 100), S(  27, 142), S( -11, 189), S( -45, 197),
 S(  28,  91), S(  19, 104), S(  24, 117), S(  18, 161),
 S(   5, 136), S(  18, 155), S(  15, 116), S(  -5, 199),
 S(  36, 102), S(  39, 134), S(  22, 136), S(   0, 204),
 S(  22,  85), S(  37, 101), S(  23, 131), S(   9, 160),
 S(  47,  26), S(  45,  36), S(  39,  53), S(  27, 108),
 S(  35,  40), S(   7,  66), S(   5,  52), S(  17,  77),
}};

const Score KING_PSQT[2][32] = {
{
 S( 175,-105), S( 155, -45), S(  34,  -6), S(  92, -20),
 S( -20,  42), S(  34,  54), S(  27,  58), S(  29,  28),
 S(  45,  28), S(  91,  49), S(  62,  57), S( -34,  67),
 S(  32,  30), S(  51,  47), S( -12,  62), S( -73,  64),
 S( -20,  18), S(  18,  33), S(  11,  38), S( -88,  49),
 S(  -8,  -2), S(  12,  16), S( -14,  21), S( -48,  23),
 S(  13, -19), S( -17,  16), S( -38,   9), S( -61,   7),
 S(  26, -79), S(  29, -37), S(   1, -28), S( -29, -39),
},{
 S(  81,-140), S(-168,  41), S( 126, -24), S( -22, -20),
 S(-117,   3), S(  36,  58), S( -27,  63), S( 141,  22),
 S(  -3,   9), S( 165,  54), S( 219,  53), S(  58,  55),
 S( -86,   9), S(  32,  45), S( -20,  66), S( -87,  66),
 S( -68,  -4), S( -46,  30), S( -25,  37), S( -78,  48),
 S(   0, -14), S(  18,  11), S(  -8,  17), S( -30,  20),
 S(   8, -11), S(   2,  13), S( -35,  11), S( -66,   6),
 S(  13, -61), S(  15, -17), S( -13, -21), S( -13, -47),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -47,  41), S(   6,  44), S(  27,  39), S(  55,  41),
 S(  21,  18), S(  43,  28), S(  38,  38), S(  47,  43),
 S(  19,   9), S(  37,  23), S(  18,  30), S(  25,  34),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -11,  38), S(  28,  19), S(  54,  31), S(  58,   9),
 S(   1,  18), S(  21,  24), S(  38,  13), S(  49,  16),
 S( -16,  41), S(  44,  16), S(  32,  16), S(  39,  30),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -39, -65), S(  -1,  23), S(  20,  66), S(  34,  81),
 S(  45,  93), S(  54, 105), S(  64, 106), S(  74, 106),
 S(  83,  95),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -6,  -2), S(  15,  33), S(  30,  59), S(  38,  85),
 S(  47,  95), S(  53, 106), S(  55, 113), S(  59, 118),
 S(  60, 121), S(  62, 124), S(  71, 119), S(  87, 117),
 S(  88, 123), S(  88, 118),};

const Score ROOK_MOBILITIES[15] = {
 S( -36,-172), S( -16,   0), S(   0,  35), S(  10,  40),
 S(   9,  66), S(  11,  78), S(   6,  89), S(  10,  91),
 S(  18,  94), S(  24,  99), S(  27, 103), S(  26, 108),
 S(  29, 114), S(  41, 117), S(  59, 110),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1601,-1645), S( -46,-586), S(  11,-208), S(  -1,  34),
 S(   5, 119), S(  10, 139), S(   8, 178), S(   9, 208),
 S(  11, 229), S(  14, 235), S(  16, 245), S(  19, 248),
 S(  19, 251), S(  23, 249), S(  23, 250), S(  31, 237),
 S(  31, 230), S(  33, 224), S(  38, 202), S(  68, 165),
 S(  88, 124), S( 103,  81), S( 140,  41), S( 186, -46),
 S( 199, -67), S( 351,-224), S( 283,-230), S( 343,-348),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(13, 22);

const Score BISHOP_OUTPOST_REACHABLE = S(9, 9);

const Score BISHOP_TRAPPED = S(-144, -217);

const Score ROOK_TRAPPED = S(-64, -24);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(30, 20);

const Score ROOK_OPEN_FILE = S(31, 18);

const Score ROOK_SEMI_OPEN = S(7, 16);

const Score DEFENDED_PAWN = S(14, 10);

const Score DOUBLED_PAWN = S(11, -32);

const Score OPPOSED_ISOLATED_PAWN = S(-5, -10);

const Score OPEN_ISOLATED_PAWN = S(-10, -19);

const Score BACKWARDS_PAWN = S(-12, -18);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  74,  56), S(  31,  39), S(  14,  12),
 S(   9,   2), S(   7,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -39, 289), S(  12,  74),
 S( -10,  45), S( -25,  19), S( -35,   4), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  62, 228), S( -11, 283), S(  -9, 149),
 S( -27,  78), S(  -8,  47), S(  -7,  42), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -13);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 28);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(32, 33);

const Score KNIGHT_THREATS[6] = { S(1, 21), S(-1, 3), S(44, 41), S(100, 6), S(54, -59), S(269, 15),};

const Score BISHOP_THREATS[6] = { S(0, 20), S(36, 40), S(10, 31), S(66, 14), S(70, 62), S(136, 107),};

const Score ROOK_THREATS[6] = { S(3, 32), S(42, 44), S(45, 54), S(13, 13), S(89, 7), S(277, -12),};

const Score KING_THREAT = S(-22, 35);

const Score PAWN_THREAT = S(78, 27);

const Score PAWN_PUSH_THREAT = S(24, 1);

const Score HANGING_THREAT = S(8, 9);

const Score SPACE[15] = {
 S( 540, -15), S( 149, -13), S(  62,  -8), S(  19,  -4), S(   1,   0),
 S(  -9,   4), S( -14,   6), S( -14,   9), S( -13,  11), S( -10,  15),
 S(  -6,  17), S(  -3,  23), S(   2,  25), S(   7,  16), S(  14,-238),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-28, 2), S(51, 64), S(-25, 66), S(-22, 19), S(2, 1), S(34, -21), S(30, -46), S(0, 0),},
 { S(-54, -1), S(21, 59), S(-33, 49), S(-41, 25), S(-34, 8), S(23, -17), S(30, -29), S(0, 0),},
 { S(-29, -7), S(44, 61), S(-61, 46), S(-20, 16), S(-19, 3), S(-8, -1), S(33, -14), S(0, 0),},
 { S(-32, 10), S(32, 64), S(-78, 44), S(-25, 29), S(-25, 21), S(-14, 13), S(-16, 13), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-21, -16), S(-29, -4), S(-27, -10), S(-38, 4), S(-67, 39), S(11, 165), S(315, 203), S(0, 0),},
 { S(-20, -2), S(3, -7), S(4, -5), S(-15, 8), S(-33, 29), S(-107, 173), S(113, 256), S(0, 0),},
 { S(33, -16), S(30, -17), S(20, -9), S(5, 1), S(-20, 25), S(-116, 148), S(-111, 239), S(0, 0),},
 { S(-4, -6), S(2, -13), S(17, -11), S(4, -13), S(-24, 12), S(-52, 121), S(-109, 240), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-44, -3), S(49, -71), S(21, -45), S(17, -37), S(1, -28), S(-2, -69), S(0, 0), S(0, 0),
};

const Score KS_ATTACKER_WEIGHTS[5] = {
 0, 29, 19, 16, 5
};

const Score KS_ATTACK = 25;

const Score KS_WEAK_SQS = 65;

const Score KS_PINNED = 32;

const Score KS_SAFE_CHECK = 204;

const Score KS_UNSAFE_CHECK = 52;

const Score KS_ENEMY_QUEEN = -307;

const Score KS_KNIGHT_DEFENSE = -21;

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
  if (ssPawns < 4)
    return MAX_SCALE - 4 * (4 - ssPawns) * (4 - ssPawns);

  return MAX_SCALE;
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

// Material + PSQT value
// TODO: Perhaps a/b lazy eval at this point? Would require eval saving to occur
// separate from search
Score MaterialValue(Board* board, int side) {
  Score s = 0;

  uint8_t enemyKingSide = SQ_SIDE[lsb(board->pieces[KING[side ^ 1]])];

  for (int pc = PAWN[side]; pc <= KING[side]; pc += 2) {
    BitBoard pieces = board->pieces[pc];

    if (T)
      C.pieces[PIECE_TYPE[pc]] += cs[side] * bits(pieces);

    while (pieces) {
      int sq = lsb(pieces);

      s += PSQT[pc][enemyKingSide == SQ_SIDE[sq]][sq];

      popLsb(pieces);

      if (T)
        C.psqt[PIECE_TYPE[pc]][enemyKingSide == SQ_SIDE[sq]][psqtIdx(side == WHITE ? sq : MIRROR[sq])] += cs[side];
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

  // bishop pair bonus first
  if (bits(board->pieces[BISHOP[side]]) > 1) {
    s += BISHOP_PAIR;

    if (T)
      C.bishopPair += cs[side];
  }

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
  Score s = 0;

  int xside = side ^ 1;
  BitBoard weak = board->occupancies[xside] &
                  ~(data->attacks[xside][PAWN_TYPE] | (data->twoAttacks[xside] & ~data->twoAttacks[side])) &
                  data->allAttacks[side];

  for (int piece = KNIGHT_TYPE; piece <= ROOK_TYPE; piece++) {
    BitBoard threats = weak & data->attacks[side][piece];
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

  BitBoard kingThreats = weak & data->attacks[side][KING_TYPE];
  if (kingThreats) {
    s += KING_THREAT;

    if (T)
      C.kingThreat += cs[side];
  }

  BitBoard pawnThreats = board->occupancies[xside] & ~board->pieces[PAWN[xside]] & data->attacks[side][PAWN_TYPE];
  s += bits(pawnThreats) * PAWN_THREAT;

  BitBoard hangingPieces = board->occupancies[xside] & ~data->allAttacks[xside] & data->allAttacks[side];
  s += bits(hangingPieces) * HANGING_THREAT;

  BitBoard pawnPushes = ~board->occupancies[BOTH] &
                        (side == WHITE ? ShiftN(board->pieces[PAWN_WHITE]) : ShiftS(board->pieces[PAWN_BLACK]));
  pawnPushes |= ~board->occupancies[BOTH] & (side == WHITE ? ShiftN(pawnPushes & RANK_3) : ShiftS(pawnPushes & RANK_6));
  BitBoard pawnPushAttacks =
      (side == WHITE ? ShiftNE(pawnPushes) | ShiftNW(pawnPushes) : ShiftSE(pawnPushes) | ShiftSW(pawnPushes));
  pawnPushAttacks &= (board->occupancies[xside] & ~board->pieces[PAWN[xside]]);

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

  s += PieceEval(board, &data, WHITE) - PieceEval(board, &data, BLACK);

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

  s += PasserEval(board, &data, WHITE) - PasserEval(board, &data, BLACK);
  s += Threats(board, &data, WHITE) - Threats(board, &data, BLACK);
  s += KingSafety(board, &data, WHITE) - KingSafety(board, &data, BLACK);
  s += Space(board, &data, WHITE) - Space(board, &data, BLACK);

  // taper
  int phase = GetPhase(board);
  Score res = (phase * scoreMG(s) + (128 - phase) * scoreEG(s)) / 128;

  if (T)
    C.ss = res >= 0 ? WHITE : BLACK;

  // scale the score
  res = (res * Scale(board, res >= 0 ? WHITE : BLACK) + MAX_SCALE / 2) / MAX_SCALE;
  return TEMPO + (board->side == WHITE ? res : -res);
}

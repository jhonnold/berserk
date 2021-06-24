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
const Score MATERIAL_VALUES[7] = { S(80, 180), S(386, 458), S(421, 480), S(634, 991), S(1326, 1871), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(34, 102);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  70, 243), S(  58, 267), S(  41, 238), S(  91, 200),
 S(  11,  77), S(  -8,  89), S(  40,  29), S(  49,  -9),
 S(  -6,  43), S(  -6,  17), S(   2,  -4), S(   3, -18),
 S( -15,  14), S( -18,   8), S(  -4,  -8), S(  11, -12),
 S( -18,   4), S( -20,   0), S( -10,  -2), S(  -1,   3),
 S(  -8,  11), S(  -1,   8), S(  -1,   7), S(   8,  11),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 150, 193), S(  23, 188), S( -22, 215), S(  61, 193),
 S(  15,  23), S(  39,   8), S(  64, -10), S(  59, -21),
 S( -23,   2), S(   2, -14), S( -10, -14), S(  30, -29),
 S( -40,  -6), S( -14, -11), S( -20, -10), S(  21, -20),
 S( -51,  -9), S( -10, -17), S( -20,  -1), S(  10,  -2),
 S( -41,   5), S(  13,  -3), S(  -9,   8), S(  13,  20),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-143,  56), S(-127,  78), S( -62,  89), S( -34,  81),
 S( -16,  79), S(   6,  93), S(  58,  77), S(  68,  84),
 S(  49,  61), S(  67,  73), S(  64,  96), S(  55, 105),
 S(  44,  92), S(  54,  97), S(  68, 103), S(  64, 117),
 S(  50,  90), S(  56,  89), S(  74, 108), S(  69, 115),
 S(  18,  80), S(  28,  83), S(  45,  94), S(  48, 117),
 S(   9,  87), S(  24,  83), S(  31,  94), S(  41,  95),
 S( -30, 100), S(  27,  81), S(  20,  89), S(  34, 109),
},{
 S(-177,-108), S(-205,  60), S(-156,  69), S( -21,  82),
 S(   6,  21), S(  71,  57), S(  69,  70), S(  64,  83),
 S(  68,  40), S(  57,  54), S( 140,  49), S( 110,  79),
 S(  57,  82), S(  63,  92), S(  66, 111), S(  62, 113),
 S(  60,  96), S(  37,  98), S(  79, 107), S(  56, 129),
 S(  39,  84), S(  61,  77), S(  54,  91), S(  65, 114),
 S(  52,  88), S(  57,  78), S(  45,  88), S(  43,  93),
 S(  23,  94), S(  23,  87), S(  45,  91), S(  47, 100),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -33,  85), S( -19, 111), S( -56, 105), S( -75, 118),
 S(  -9, 104), S( -19,  83), S(  27,  93), S(  21, 102),
 S(  30,  98), S(  31, 101), S(  -1,  78), S(  50,  95),
 S(  22, 104), S(  60,  96), S(  43, 104), S(  46, 126),
 S(  48,  86), S(  37, 103), S(  52, 113), S(  62, 107),
 S(  38,  90), S(  62, 108), S(  37,  91), S(  44, 113),
 S(  48, 100), S(  40,  59), S(  55,  87), S(  34, 105),
 S(  25,  62), S(  56,  88), S(  37, 104), S(  39, 102),
},{
 S( -61,  47), S(  31,  82), S( -54,  93), S( -98,  98),
 S(  -8,  59), S( -51,  81), S(   5, 108), S(  39,  80),
 S(  65,  86), S(  36,  94), S(  48,  83), S(  48, 101),
 S(  12,  95), S(  58,  95), S(  55, 106), S(  57, 105),
 S(  75,  61), S(  55,  94), S(  60, 107), S(  54, 108),
 S(  50,  83), S(  68,  90), S(  46,  86), S(  47, 121),
 S(  55,  68), S(  55,  64), S(  57,  94), S(  46, 103),
 S(  46,  46), S(  41, 112), S(  32, 110), S(  49,  95),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -15, 118), S( -13, 126), S( -13, 134), S( -31, 137),
 S(   2, 101), S(  -9, 117), S(  15, 115), S(  38,  98),
 S( -14, 105), S(  26, 102), S(  15, 105), S(  16, 101),
 S(  -3, 115), S(  17, 107), S(  34, 109), S(  39,  94),
 S( -11, 111), S(   2, 112), S(   9, 112), S(  21, 100),
 S( -12, 102), S(   0,  91), S(   6,  98), S(  11,  94),
 S( -12,  96), S(   0,  94), S(  17,  93), S(  21,  87),
 S(   2,  98), S(   1,  94), S(   5, 100), S(  17,  84),
},{
 S(  90, 106), S(  37, 126), S( -47, 143), S( -11, 128),
 S(  52,  90), S(  71, 103), S(  16, 108), S(  11, 103),
 S(  -1,  96), S(  81,  82), S(  47,  72), S(  54,  75),
 S(  10,  95), S(  61,  81), S(  43,  76), S(  47,  82),
 S( -10,  91), S(  25,  87), S(   9,  85), S(  31,  87),
 S(  -1,  69), S(  30,  68), S(  19,  72), S(  26,  81),
 S(  -8,  79), S(  41,  56), S(  21,  66), S(  26,  75),
 S(  18,  65), S(  15,  80), S(  15,  76), S(  26,  73),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -81, 201), S(  -2, 126), S(  70, 124), S(  50, 166),
 S( -39, 157), S( -42, 158), S( -24, 198), S( -35, 219),
 S(  -3, 123), S(   9, 114), S(   2, 151), S(   4, 176),
 S(   2, 164), S(  12, 182), S(  16, 188), S(  -6, 230),
 S(  22, 129), S(   2, 202), S(   8, 203), S( -12, 238),
 S(   5, 132), S(  19, 146), S(   8, 174), S(   3, 181),
 S(  15,  94), S(  17, 104), S(  26, 110), S(  25, 128),
 S(   4, 105), S(   1,  99), S(   5, 113), S(  17, 108),
},{
 S( 127, 101), S( 233,  43), S(  20, 153), S(  68, 134),
 S(  80, 120), S(  39, 169), S(   5, 205), S( -50, 223),
 S(  35, 109), S(  29, 124), S(  35, 138), S(  23, 184),
 S(   8, 157), S(  22, 183), S(  18, 135), S(  -5, 222),
 S(  39, 120), S(  43, 153), S(  25, 154), S(   1, 225),
 S(  26,  96), S(  41, 117), S(  25, 148), S(  10, 178),
 S(  51,  34), S(  48,  48), S(  41,  64), S(  28, 124),
 S(  38,  46), S(   6,  77), S(   4,  63), S(  17,  88),
}};

const Score KING_PSQT[2][32] = {{
 S( 190,-115), S( 153, -43), S(  18,  -2), S(  86, -16),
 S( -19,  48), S(  31,  61), S(  20,  65), S(  28,  34),
 S(  42,  33), S(  91,  55), S(  53,  65), S( -50,  78),
 S(  42,  32), S(  58,  51), S( -18,  69), S( -83,  72),
 S( -27,  21), S(  15,  38), S(   6,  43), S(-104,  57),
 S( -11,  -2), S(   8,  19), S( -21,  25), S( -59,  28),
 S(  16, -21), S( -17,  17), S( -39,  10), S( -66,  10),
 S(  30, -86), S(  33, -41), S(   4, -30), S( -31, -41),
},{
 S( 123,-159), S(-184,  47), S( 137, -25), S( -57, -12),
 S(-139,   6), S(  33,  67), S( -52,  74), S( 150,  28),
 S( -19,  13), S( 173,  62), S( 222,  62), S(  63,  63),
 S( -91,  10), S(  26,  51), S( -26,  75), S(-100,  76),
 S( -80,  -2), S( -56,  35), S( -35,  43), S( -92,  57),
 S(  -3, -13), S(  18,  14), S( -14,  21), S( -39,  26),
 S(   9, -11), S(   2,  16), S( -39,  13), S( -72,  10),
 S(  15, -66), S(  17, -18), S( -14, -22), S( -14, -49),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -51,  47), S(   6,  50), S(  29,  42), S(  61,  45),
 S(  22,  20), S(  48,  30), S(  41,  41), S(  52,  47),
 S(  21,  11), S(  39,  26), S(  21,  32), S(  26,  37),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -11,  41), S(  35,  19), S(  61,  34), S(  64,  10),
 S(   2,  21), S(  22,  26), S(  43,  14), S(  55,  17),
 S( -16,  47), S(  49,  17), S(  35,  17), S(  43,  32),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -35, -65), S(   6,  32), S(  29,  81), S(  43,  96),
 S(  56, 109), S(  66, 123), S(  76, 124), S(  87, 124),
 S(  97, 113),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -1,   7), S(  21,  45), S(  38,  74), S(  47, 101),
 S(  56, 113), S(  62, 125), S(  65, 132), S(  69, 137),
 S(  70, 142), S(  73, 145), S(  83, 139), S( 102, 136),
 S( 105, 143), S( 101, 138),};

const Score ROOK_MOBILITIES[15] = {
 S( -38,-183), S( -17,   5), S(   0,  42), S(  11,  49),
 S(  10,  76), S(  12,  89), S(   6, 102), S(  11, 104),
 S(  20, 107), S(  26, 113), S(  30, 117), S(  30, 123),
 S(  32, 129), S(  46, 132), S(  64, 126),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2074,-2003), S( -82,-505), S(  13,-212), S(   1,  42),
 S(   8, 139), S(  12, 159), S(  10, 202), S(  11, 238),
 S(  13, 261), S(  16, 268), S(  19, 279), S(  22, 282),
 S(  22, 286), S(  27, 284), S(  27, 287), S(  35, 273),
 S(  35, 267), S(  38, 260), S(  42, 239), S(  73, 201),
 S(  96, 156), S( 111, 114), S( 150,  74), S( 190, -12),
 S( 206, -30), S( 362,-198), S( 285,-207), S( 357,-337),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(14, 24);

const Score BISHOP_OUTPOST_REACHABLE = S(10, 10);

const Score BISHOP_TRAPPED = S(-160, -230);

const Score ROOK_TRAPPED = S(-69, -27);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(32, 22);

const Score ROOK_OPEN_FILE = S(34, 19);

const Score ROOK_SEMI_OPEN = S(8, 17);

const Score DEFENDED_PAWN = S(15, 11);

const Score DOUBLED_PAWN = S(12, -35);

const Score OPPOSED_ISOLATED_PAWN = S(-5, -11);

const Score OPEN_ISOLATED_PAWN = S(-11, -21);

const Score BACKWARDS_PAWN = S(-13, -20);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  81,  62), S(  33,  44), S(  15,  13),
 S(   9,   2), S(   7,   1), S(   2,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -31, 311), S(  12,  81),
 S( -11,  49), S( -27,  21), S( -36,   4), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  63, 246), S( -13, 308), S( -12, 162),
 S( -30,  85), S(  -9,  51), S(  -7,  45), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(4, -14);

const Score PASSED_PAWN_KING_PROXIMITY = S(-9, 30);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(34, 36);

const Score KNIGHT_THREATS[6] = { S(1, 22), S(2, 2), S(47, 43), S(105, 2), S(56, -71), S(281, 20),};

const Score BISHOP_THREATS[6] = { S(0, 22), S(38, 44), S(13, 41), S(70, 11), S(74, 53), S(144, 116),};

const Score ROOK_THREATS[6] = { S(3, 34), S(46, 46), S(50, 56), S(13, 22), S(94, -3), S(296, -14),};

const Score KING_THREAT = S(-24, 37);

const Score PAWN_THREAT = S(83, 27);

const Score PAWN_PUSH_THREAT = S(26, 2);

const Score HANGING_THREAT = S(8, 9);

const Score SPACE[15] = {
 S( 621, -21), S( 163, -15), S(  66,  -9), S(  20,  -4), S(   0,   1),
 S( -11,   4), S( -15,   7), S( -16,  10), S( -15,  13), S( -11,  17),
 S(  -7,  19), S(  -3,  26), S(   2,  28), S(   8,  18), S(  16,-242),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-31, 3), S(46, 78), S(-30, 73), S(-23, 22), S(2, 2), S(37, -22), S(33, -50), S(0, 0),},
 { S(-59, 0), S(16, 70), S(-39, 56), S(-45, 28), S(-38, 10), S(24, -17), S(32, -29), S(0, 0),},
 { S(-32, -7), S(43, 73), S(-61, 50), S(-21, 18), S(-21, 4), S(-9, 0), S(35, -13), S(0, 0),},
 { S(-36, 12), S(19, 78), S(-84, 47), S(-28, 32), S(-27, 24), S(-15, 14), S(-17, 15), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-23, -16), S(-33, -3), S(-30, -10), S(-41, 5), S(-72, 43), S(11, 179), S(340, 214), S(0, 0),},
 { S(-23, 0), S(2, -6), S(3, -4), S(-18, 10), S(-36, 32), S(-112, 188), S(118, 269), S(0, 0),},
 { S(41, -18), S(38, -20), S(24, -10), S(5, 1), S(-20, 27), S(-119, 160), S(-143, 257), S(0, 0),},
 { S(-6, -6), S(2, -13), S(18, -11), S(4, -13), S(-25, 15), S(-57, 134), S(-120, 250), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-40, -12), S(55, -76), S(22, -48), S(18, -40), S(1, -30), S(-4, -75), S(0, 0), S(0, 0),
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
  res = (res * Scale(board, res >= 0 ? WHITE : BLACK)) / MAX_SCALE;
  return TEMPO + (board->side == WHITE ? res : -res);
}

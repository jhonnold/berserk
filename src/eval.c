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
const Score MATERIAL_VALUES[7] = { S(79, 176), S(380, 448), S(415, 471), S(618, 956), S(1291, 1801), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(33, 99);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  63, 234), S(  56, 257), S(  38, 228), S(  86, 191),
 S(  10,  72), S(  -9,  83), S(  38,  24), S(  47, -12),
 S(  -7,  39), S(  -7,  14), S(   1,  -6), S(   3, -20),
 S( -15,  11), S( -19,   5), S(  -5, -11), S(  10, -15),
 S( -19,   1), S( -21,  -3), S( -11,  -5), S(  -2,   0),
 S(  -9,   8), S(  -2,   5), S(  -2,   4), S(   7,   8),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 146, 184), S(  22, 180), S(   4, 201), S(  59, 184),
 S(  14,  20), S(  37,   6), S(  66, -14), S(  56, -23),
 S( -23,  -1), S(   0, -16), S(  -8, -18), S(  28, -31),
 S( -40,  -8), S( -15, -12), S( -16, -14), S(  19, -22),
 S( -51, -11), S( -11, -18), S( -18,  -5), S(   8,  -4),
 S( -41,   2), S(  11,  -6), S( -10,   5), S(  12,  16),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-139,  49), S(-124,  71), S( -61,  81), S( -36,  73),
 S( -17,  72), S(   4,  85), S(  55,  69), S(  63,  76),
 S(  46,  55), S(  63,  66), S(  60,  88), S(  52,  96),
 S(  41,  86), S(  50,  89), S(  64,  94), S(  59, 108),
 S(  47,  83), S(  52,  82), S(  70,  99), S(  65, 106),
 S(  16,  73), S(  26,  76), S(  42,  86), S(  44, 109),
 S(   7,  80), S(  22,  75), S(  28,  86), S(  38,  87),
 S( -31,  92), S(  24,  74), S(  18,  81), S(  31, 100),
},{
 S(-174,-108), S(-187,  50), S(-148,  61), S( -25,  75),
 S(   3,  16), S(  66,  50), S(  63,  64), S(  60,  75),
 S(  63,  35), S(  51,  47), S( 129,  44), S( 102,  72),
 S(  51,  75), S(  58,  84), S(  60, 102), S(  57, 104),
 S(  55,  88), S(  32,  90), S(  74,  99), S(  51, 120),
 S(  36,  77), S(  57,  70), S(  50,  83), S(  61, 105),
 S(  48,  80), S(  53,  72), S(  42,  80), S(  40,  86),
 S(  21,  87), S(  20,  79), S(  42,  83), S(  44,  91),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -33,  77), S( -22, 102), S( -56,  96), S( -71, 108),
 S( -11,  96), S( -20,  75), S(  25,  85), S(  19,  94),
 S(  27,  90), S(  29,  93), S(  -2,  71), S(  46,  87),
 S(  19,  95), S(  57,  87), S(  41,  95), S(  43, 117),
 S(  44,  78), S(  35,  94), S(  48, 104), S(  59,  98),
 S(  35,  81), S(  58,  99), S(  34,  82), S(  41, 104),
 S(  44,  91), S(  37,  52), S(  51,  78), S(  31,  96),
 S(  22,  55), S(  52,  81), S(  34,  95), S(  36,  93),
},{
 S( -58,  41), S(  26,  75), S( -62,  86), S( -91,  89),
 S( -11,  53), S( -50,  74), S(   1, 100), S(  36,  73),
 S(  61,  78), S(  32,  86), S(  44,  75), S(  45,  92),
 S(   8,  87), S(  54,  86), S(  51,  98), S(  53,  96),
 S(  70,  54), S(  52,  85), S(  56,  98), S(  49, 100),
 S(  47,  75), S(  64,  82), S(  42,  78), S(  43, 112),
 S(  52,  60), S(  51,  57), S(  54,  86), S(  43,  94),
 S(  43,  38), S(  39, 103), S(  29, 101), S(  46,  86),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -14, 112), S( -13, 120), S( -12, 127), S( -30, 130),
 S(   3,  95), S(  -9, 111), S(  14, 109), S(  37,  92),
 S( -12,  99), S(  26,  96), S(  15, 100), S(  17,  95),
 S(  -2, 109), S(  17, 102), S(  34, 103), S(  37,  90),
 S( -11, 105), S(   3, 106), S(  10, 107), S(  21,  94),
 S( -12,  97), S(   1,  87), S(   7,  92), S(  11,  89),
 S( -11,  90), S(   0,  89), S(  17,  88), S(  21,  82),
 S(   3,  93), S(   2,  89), S(   6,  94), S(  17,  79),
},{
 S(  87, 101), S(  37, 119), S( -48, 137), S( -12, 122),
 S(  46,  86), S(  67,  97), S(  14, 102), S(  10,  97),
 S(  -4,  91), S(  75,  78), S(  43,  68), S(  52,  71),
 S(   8,  91), S(  58,  77), S(  40,  72), S(  46,  77),
 S( -11,  87), S(  23,  82), S(   9,  80), S(  30,  82),
 S(  -2,  65), S(  28,  64), S(  17,  67), S(  25,  76),
 S(  -8,  75), S(  39,  52), S(  20,  62), S(  25,  70),
 S(  17,  62), S(  14,  75), S(  14,  71), S(  25,  69),
}};

const Score QUEEN_PSQT[2][32] = {{
 S( -77, 192), S(  -2, 121), S(  68, 117), S(  49, 156),
 S( -35, 148), S( -40, 152), S( -22, 190), S( -33, 209),
 S(  -2, 118), S(  10, 109), S(   3, 143), S(   4, 168),
 S(   2, 158), S(  13, 176), S(  16, 181), S(  -5, 220),
 S(  22, 124), S(   3, 194), S(   9, 196), S( -10, 229),
 S(   5, 127), S(  19, 140), S(   9, 167), S(   4, 173),
 S(  15,  89), S(  17,  98), S(  26, 104), S(  25, 122),
 S(   5, 100), S(   2,  95), S(   6, 109), S(  17, 104),
},{
 S( 120,  96), S( 224,  39), S(  17, 148), S(  65, 128),
 S(  74, 112), S(  35, 159), S(   1, 197), S( -47, 212),
 S(  32, 102), S(  26, 115), S(  34, 127), S(  21, 175),
 S(   8, 149), S(  21, 172), S(  18, 127), S(  -4, 212),
 S(  39, 111), S(  42, 146), S(  25, 146), S(   1, 216),
 S(  25,  91), S(  40, 110), S(  24, 141), S(  10, 171),
 S(  50,  30), S(  47,  43), S(  41,  60), S(  28, 118),
 S(  38,  42), S(   7,  74), S(   5,  60), S(  17,  84),
}};

const Score KING_PSQT[2][32] = {{
 S( 183,-111), S( 149, -43), S(  20,  -4), S(  87, -18),
 S( -19,  45), S(  30,  58), S(  20,  61), S(  29,  30),
 S(  44,  30), S(  86,  52), S(  55,  61), S( -43,  72),
 S(  37,  31), S(  58,  49), S( -13,  64), S( -76,  67),
 S( -27,  20), S(  17,  35), S(   9,  40), S( -96,  52),
 S( -10,  -2), S(   9,  17), S( -18,  22), S( -53,  24),
 S(  15, -20), S( -17,  16), S( -38,   9), S( -64,   7),
 S(  29, -83), S(  31, -39), S(   3, -30), S( -30, -41),
},{
 S( 113,-153), S(-181,  45), S( 135, -25), S( -37, -17),
 S(-134,   5), S(  25,  65), S( -46,  69), S( 146,  25),
 S( -11,  11), S( 168,  59), S( 222,  58), S(  65,  58),
 S( -91,  10), S(  32,  48), S( -22,  71), S( -93,  71),
 S( -76,  -3), S( -53,  33), S( -30,  41), S( -84,  52),
 S(  -2, -13), S(  18,  13), S( -12,  19), S( -34,  22),
 S(   9, -11), S(   2,  15), S( -37,  12), S( -69,   7),
 S(  15, -64), S(  16, -18), S( -13, -22), S( -13, -49),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -49,  44), S(   5,  48), S(  29,  40), S(  58,  43),
 S(  22,  19), S(  46,  29), S(  39,  40), S(  50,  45),
 S(  20,  11), S(  37,  25), S(  20,  31), S(  25,  36),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -10,  39), S(  33,  19), S(  59,  32), S(  62,   9),
 S(   2,  20), S(  21,  26), S(  40,  14), S(  53,  17),
 S( -16,  45), S(  47,  16), S(  34,  16), S(  41,  31),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -36, -65), S(   4,  28), S(  26,  75), S(  39,  89),
 S(  52, 102), S(  61, 115), S(  71, 116), S(  82, 116),
 S(  91, 105),};

const Score BISHOP_MOBILITIES[14] = {
 S(  -5,   2), S(  17,  39), S(  34,  66), S(  42,  93),
 S(  51, 103), S(  57, 115), S(  60, 122), S(  63, 127),
 S(  64, 131), S(  67, 134), S(  76, 129), S(  94, 126),
 S(  99, 132), S(  94, 127),};

const Score ROOK_MOBILITIES[15] = {
 S( -35,-178), S( -16,   3), S(   1,  38), S(  11,  44),
 S(  10,  71), S(  12,  84), S(   6,  96), S(  11,  97),
 S(  19, 101), S(  25, 106), S(  29, 110), S(  28, 116),
 S(  31, 122), S(  44, 125), S(  62, 118),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1765,-1786), S( -37,-612), S(  13,-213), S(   1,  40),
 S(   7, 130), S(  12, 150), S(  10, 192), S(  11, 225),
 S(  13, 248), S(  16, 255), S(  18, 265), S(  21, 267),
 S(  21, 271), S(  25, 270), S(  26, 272), S(  34, 258),
 S(  33, 252), S(  36, 245), S(  41, 223), S(  71, 186),
 S(  93, 143), S( 107, 100), S( 144,  62), S( 189, -26),
 S( 197, -42), S( 374,-217), S( 339,-250), S( 352,-340),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(14, 23);

const Score BISHOP_OUTPOST_REACHABLE = S(9, 10);

const Score BISHOP_TRAPPED = S(-155, -222);

const Score ROOK_TRAPPED = S(-67, -26);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(31, 21);

const Score ROOK_OPEN_FILE = S(32, 18);

const Score ROOK_SEMI_OPEN = S(8, 17);

const Score DEFENDED_PAWN = S(15, 11);

const Score DOUBLED_PAWN = S(11, -33);

const Score OPPOSED_ISOLATED_PAWN = S(-5, -11);

const Score OPEN_ISOLATED_PAWN = S(-11, -20);

const Score BACKWARDS_PAWN = S(-13, -19);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  79,  59), S(  32,  42), S(  15,  13),
 S(   9,   2), S(   7,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -33, 301), S(  12,  77),
 S( -11,  47), S( -26,  20), S( -35,   4), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  64, 235), S( -11, 296), S( -10, 156),
 S( -28,  81), S(  -8,  49), S(  -7,  43), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -13);

const Score PASSED_PAWN_KING_PROXIMITY = S(-9, 29);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(33, 35);

const Score KNIGHT_THREATS[6] = { S(1, 22), S(1, 2), S(45, 42), S(103, 2), S(56, -69), S(273, 19),};

const Score BISHOP_THREATS[6] = { S(0, 21), S(37, 42), S(12, 36), S(68, 10), S(72, 54), S(140, 113),};

const Score ROOK_THREATS[6] = { S(3, 33), S(44, 45), S(48, 55), S(13, 17), S(91, -3), S(288, -14),};

const Score KING_THREAT = S(-22, 35);

const Score PAWN_THREAT = S(80, 27);

const Score PAWN_PUSH_THREAT = S(25, 2);

const Score HANGING_THREAT = S(8, 9);

const Score SPACE[15] = {
 S( 603, -18), S( 157, -14), S(  64,  -8), S(  20,  -4), S(   1,   0),
 S( -10,   4), S( -15,   7), S( -15,   9), S( -14,  12), S( -11,  16),
 S(  -6,  18), S(  -3,  25), S(   2,  27), S(   7,  17), S(  15,-247),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-31, 3), S(51, 68), S(-28, 71), S(-23, 21), S(1, 2), S(35, -21), S(31, -48), S(0, 0),},
 { S(-57, 0), S(26, 58), S(-37, 53), S(-43, 27), S(-36, 10), S(24, -16), S(31, -29), S(0, 0),},
 { S(-30, -7), S(41, 63), S(-61, 48), S(-21, 17), S(-20, 3), S(-9, -1), S(34, -14), S(0, 0),},
 { S(-34, 11), S(26, 68), S(-81, 45), S(-26, 30), S(-25, 23), S(-14, 13), S(-16, 14), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-20, -17), S(-30, -4), S(-28, -11), S(-39, 3), S(-68, 40), S(13, 171), S(335, 203), S(0, 0),},
 { S(-21, -2), S(3, -8), S(4, -6), S(-16, 8), S(-34, 29), S(-111, 180), S(120, 256), S(0, 0),},
 { S(35, -16), S(32, -18), S(21, -9), S(4, 1), S(-21, 26), S(-115, 154), S(-118, 243), S(0, 0),},
 { S(-6, -6), S(1, -13), S(17, -11), S(4, -13), S(-24, 14), S(-55, 129), S(-118, 239), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-45, -4), S(52, -74), S(21, -46), S(17, -39), S(1, -29), S(-2, -73), S(0, 0), S(0, 0),
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

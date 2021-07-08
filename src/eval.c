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

const Score BISHOP_PAIR = S(22, 87);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 115, 194), S( 105, 218), S(  78, 213), S( 110, 188),
 S(  77, 129), S(  58, 147), S(  80,  93), S(  86,  73),
 S(  75, 110), S(  73,  98), S(  66,  79), S(  63,  71),
 S(  70,  85), S(  66,  89), S(  61,  76), S(  69,  77),
 S(  65,  77), S(  62,  82), S(  60,  77), S(  62,  86),
 S(  74,  82), S(  76,  88), S(  70,  83), S(  76,  87),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 152, 150), S(  49, 154), S( 146, 146), S(  94, 191),
 S(  88,  76), S(  96,  68), S( 111,  44), S(  91,  60),
 S(  63,  73), S(  79,  67), S(  64,  59), S(  86,  60),
 S(  54,  66), S(  70,  71), S(  56,  67), S(  80,  69),
 S(  38,  65), S(  66,  67), S(  61,  72), S(  72,  82),
 S(  47,  76), S(  83,  79), S(  71,  80), S(  78,  97),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-195,  49), S(-200,  70), S(-169,  78), S(-125,  59),
 S(-120,  72), S(-101,  78), S( -74,  63), S( -71,  65),
 S( -64,  54), S( -66,  63), S( -68,  74), S( -77,  75),
 S( -73,  80), S( -68,  76), S( -51,  72), S( -65,  77),
 S( -73,  76), S( -65,  70), S( -53,  82), S( -60,  81),
 S( -94,  56), S( -85,  57), S( -74,  64), S( -78,  78),
 S(-105,  56), S( -89,  53), S( -87,  61), S( -79,  60),
 S(-130,  60), S( -90,  55), S( -93,  56), S( -83,  68),
},{
 S(-248, -99), S(-301,  49), S(-147,  30), S( -74,  56),
 S( -96,  10), S( -25,  32), S( -60,  61), S( -79,  67),
 S( -51,  27), S( -91,  43), S( -37,  24), S( -46,  55),
 S( -67,  59), S( -59,  55), S( -54,  63), S( -57,  72),
 S( -69,  72), S( -79,  64), S( -50,  75), S( -67,  89),
 S( -74,  51), S( -61,  49), S( -68,  57), S( -64,  74),
 S( -75,  52), S( -65,  51), S( -79,  54), S( -79,  57),
 S( -90,  58), S( -95,  58), S( -79,  51), S( -76,  65),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -26, 107), S( -45, 129), S( -30, 110), S( -49, 127),
 S(  -7, 110), S( -19,  99), S(  19, 104), S(   7, 116),
 S(  13, 114), S(  17, 115), S( -14,  98), S(  21, 106),
 S(   9, 112), S(  33, 108), S(  23, 112), S(  13, 129),
 S(  29,  99), S(  20, 111), S(  29, 112), S(  37, 109),
 S(  25,  92), S(  42, 105), S(  27,  91), S(  31, 106),
 S(  29,  88), S(  29,  63), S(  40,  81), S(  26, 100),
 S(  21,  67), S(  39,  82), S(  30, 100), S(  31,  99),
},{
 S( -67,  71), S(  37,  94), S( -43, 103), S( -67, 106),
 S(   3,  65), S( -28,  98), S( -10, 115), S(  19,  97),
 S(  32, 101), S(   3, 103), S(   4, 100), S(  19, 107),
 S(   3,  97), S(  35, 103), S(  34, 108), S(  32, 109),
 S(  46,  76), S(  34, 101), S(  41, 109), S(  30, 112),
 S(  38,  82), S(  54,  93), S(  35,  91), S(  32, 117),
 S(  40,  64), S(  40,  65), S(  41,  95), S(  35, 101),
 S(  35,  45), S(  31, 113), S(  24, 106), S(  39,  97),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-108, 225), S(-100, 230), S(-100, 233), S(-126, 235),
 S(-100, 218), S(-106, 229), S( -85, 227), S( -69, 212),
 S(-114, 225), S( -78, 219), S( -88, 221), S( -86, 215),
 S(-107, 238), S( -91, 234), S( -77, 232), S( -75, 215),
 S(-111, 234), S(-104, 235), S( -98, 231), S( -90, 221),
 S(-114, 225), S(-104, 213), S( -97, 215), S( -95, 212),
 S(-111, 210), S(-105, 215), S( -92, 211), S( -88, 202),
 S(-109, 221), S(-103, 212), S(-100, 216), S( -90, 201),
},{
 S( -43, 209), S( -97, 234), S(-155, 245), S( -97, 226),
 S( -78, 198), S( -65, 212), S( -89, 216), S( -99, 219),
 S(-104, 202), S( -48, 198), S( -77, 192), S( -56, 192),
 S( -97, 209), S( -62, 207), S( -70, 196), S( -67, 206),
 S(-114, 208), S( -95, 211), S( -96, 206), S( -82, 211),
 S(-102, 184), S( -84, 185), S( -88, 189), S( -84, 200),
 S( -97, 186), S( -74, 175), S( -87, 184), S( -83, 193),
 S(-106, 201), S( -87, 194), S( -92, 195), S( -85, 194),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-119, 396), S( -41, 301), S(  -8, 306), S( -18, 331),
 S( -50, 329), S( -43, 308), S( -28, 334), S( -36, 338),
 S( -22, 304), S( -15, 294), S( -26, 329), S(  -6, 323),
 S( -19, 325), S( -12, 339), S( -10, 340), S( -29, 354),
 S(  -8, 303), S( -20, 349), S( -17, 341), S( -23, 361),
 S( -12, 285), S(  -4, 297), S(  -9, 314), S( -12, 313),
 S(  -4, 254), S(  -3, 260), S(   4, 260), S(   4, 272),
 S( -14, 270), S( -14, 262), S( -10, 267), S(  -3, 269),
},{
 S(  34, 279), S(  90, 258), S(-117, 391), S(  -7, 297),
 S(  21, 269), S(  -3, 265), S( -21, 317), S( -43, 349),
 S(  20, 268), S(  -5, 262), S(   6, 288), S( -11, 326),
 S(  -7, 300), S(   2, 304), S(  13, 279), S( -23, 350),
 S(   7, 269), S(   1, 298), S(   0, 306), S( -21, 357),
 S(   2, 239), S(   9, 264), S(   4, 293), S(  -9, 314),
 S(  22, 206), S(  22, 201), S(  14, 229), S(   7, 270),
 S(  17, 233), S( -13, 250), S( -11, 236), S(  -6, 256),
}};

const Score KING_PSQT[2][32] = {{
 S( 236,-141), S( 134, -61), S( -28, -16), S(  97, -41),
 S( -90,  53), S( -26,  66), S(  -3,  53), S(  35,  22),
 S(  20,  39), S(   1,  73), S(  28,  62), S( -11,  66),
 S(  31,  35), S(  12,  60), S(  -8,  60), S( -23,  53),
 S( -23,  22), S(  19,  37), S(  31,  30), S( -37,  36),
 S( -25,  -3), S(   1,  14), S(  -4,  11), S( -16,   6),
 S(   4, -23), S( -19,  11), S( -24,  -5), S( -30, -12),
 S(  11, -80), S(  19, -37), S(   9, -38), S( -48, -36),
},{
 S( -20,-137), S(-287,  55), S(   5, -34), S( -50, -34),
 S(-209,  16), S( -84,  85), S( -93,  70), S( 135,  14),
 S(-110,  29), S(  41,  85), S( 130,  70), S(  75,  59),
 S( -89,  16), S(  10,  58), S(  -2,  71), S(  -5,  58),
 S( -78,   4), S( -48,  38), S(   3,  36), S( -13,  37),
 S(  -6, -14), S(  23,  11), S(   6,  12), S(   9,   9),
 S(  13, -14), S(   9,  11), S(  -7,   0), S( -21, -10),
 S(  14, -62), S(  19, -15), S(   4, -25), S( -33, -34),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -45,  10), S(   7,  20), S(  28,  24), S(  60,  37),
 S(  14,  -3), S(  40,  18), S(  30,  27), S(  42,  35),
 S(  21,  -3), S(  34,  10), S(  22,  14), S(  21,  22),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -3,   8), S(  23,   6), S(  59,  13), S(  61,   9),
 S(   2,   0), S(  25,  12), S(  41,   5), S(  52,   9),
 S(  -7,  24), S(  44,   7), S(  30,  12), S(  39,  20),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-160, -33), S(-119,  40), S(-100,  78), S( -88,  92),
 S( -79, 104), S( -71, 116), S( -63, 118), S( -55, 120),
 S( -46, 111),};

const Score BISHOP_MOBILITIES[14] = {
 S( -17,  48), S(   7,  74), S(  20,  94), S(  27, 115),
 S(  34, 123), S(  38, 134), S(  40, 140), S(  44, 143),
 S(  45, 145), S(  48, 146), S(  56, 142), S(  70, 137),
 S(  72, 143), S(  80, 129),};

const Score ROOK_MOBILITIES[15] = {
 S( -88, -73), S(-103, 142), S( -88, 176), S( -81, 181),
 S( -83, 203), S( -79, 211), S( -84, 221), S( -79, 222),
 S( -75, 228), S( -70, 232), S( -68, 236), S( -70, 241),
 S( -66, 243), S( -57, 247), S( -45, 240),};

const Score QUEEN_MOBILITIES[28] = {
 S(-1837,-1371), S(-122,-385), S( -82, 117), S( -58, 244),
 S( -48, 288), S( -42, 300), S( -42, 331), S( -41, 355),
 S( -39, 372), S( -36, 376), S( -34, 383), S( -31, 383),
 S( -32, 389), S( -29, 389), S( -31, 392), S( -26, 383),
 S( -29, 384), S( -31, 384), S( -25, 364), S(  -8, 335),
 S(   9, 301), S(  22, 262), S(  59, 228), S( 103, 140),
 S(  65, 149), S( 188,  23), S(  40,  24), S(  36,  -6),
};

const Score MINOR_BEHIND_PAWN = S(5, 14);

const Score KNIGHT_OUTPOST_REACHABLE = S(10, 19);

const Score BISHOP_OUTPOST_REACHABLE = S(6, 6);

const Score BISHOP_TRAPPED = S(-116, -232);

const Score ROOK_TRAPPED = S(-43, -33);

const Score BAD_BISHOP_PAWNS = S(-1, -4);

const Score DRAGON_BISHOP = S(23, 16);

const Score ROOK_OPEN_FILE = S(28, 16);

const Score ROOK_SEMI_OPEN = S(16, 5);

const Score DEFENDED_PAWN = S(11, 11);

const Score DOUBLED_PAWN = S(18, -36);

const Score ISOLATED_PAWN[4] = {
 S(  -2,  -5), S(  -1, -13), S(  -7,  -5), S(   0, -13),
};

const Score OPEN_ISOLATED_PAWN = S(-5, -11);

const Score BACKWARDS_PAWN = S(-9, -18);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  84,  44), S(  28,  38), S(  10,  13),
 S(   6,   3), S(   5,   2), S(   2,   0), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 142, 156), S(  10,  74),
 S( -14,  60), S( -23,  39), S( -36,  14), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(4, -11);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 118, 183), S(  43, 182), S(  11, 114),
 S( -12,  75), S( -10,  41), S(  -9,  37), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(1, -11);

const Score PASSED_PAWN_KING_PROXIMITY = S(-6, 23);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S(  84, 241), S(  15, 136), S(   8,  46), S(  12,  10),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(26, -111);

const Score PASSED_PAWN_SQ_RULE = S(0, 360);

const Score KNIGHT_THREATS[6] = { S(1, 18), S(-2, 47), S(29, 32), S(76, -1), S(50, -56), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(2, 18), S(21, 33), S(-60, 85), S(58, 10), S(63, 84), S(0, 0),};

const Score ROOK_THREATS[6] = { S(0, 21), S(30, 40), S(30, 51), S(3, 22), S(76, -7), S(0, 0),};

const Score KING_THREAT = S(11, 30);

const Score PAWN_THREAT = S(72, 31);

const Score PAWN_PUSH_THREAT = S(18, 19);

const Score HANGING_THREAT = S(5, 9);

const Score SPACE = 84;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(11, 25), S(0, 0),},
 { S(6, 20), S(-10, -30), S(0, 0),},
 { S(0, 35), S(-32, -22), S(-3, -1), S(0, 0),},
 { S(57, 46), S(-65, 12), S(-10, 77), S(-208, 151), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-22, -4), S(13, 93), S(-3, 49), S(-12, 7), S(8, -5), S(37, -26), S(34, -49), S(0, 0),},
 { S(-43, 1), S(-13, 75), S(-20, 44), S(-31, 20), S(-27, 7), S(25, -18), S(32, -32), S(0, 0),},
 { S(-23, -12), S(-2, 85), S(-34, 34), S(-11, 7), S(-15, -4), S(-5, -8), S(28, -20), S(0, 0),},
 { S(-37, 16), S(12, 75), S(-58, 35), S(-29, 29), S(-28, 22), S(-16, 13), S(-25, 22), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-21, -12), S(-24, -2), S(-24, -7), S(-29, 4), S(-56, 31), S(62, 79), S(275, 75), S(0, 0),},
 { S(-13, 0), S(2, -6), S(2, -4), S(-12, 9), S(-27, 21), S(-29, 84), S(103, 129), S(0, 0),},
 { S(28, -5), S(31, -9), S(25, -4), S(6, 5), S(-14, 19), S(-46, 75), S(52, 97), S(0, 0),},
 { S(-3, -4), S(3, -10), S(15, -9), S(3, -10), S(-19, 5), S(-16, 47), S(-33, 119), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(4, -45), S(38, -51), S(18, -36), S(15, -31), S(3, -24), S(6, -83), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(43, -18);

const Score KS_ATTACKER_WEIGHTS[5] = {
 0, 33, 32, 19, 25
};

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
  data->passedPawns = 0ULL;

  BitBoard whitePawns = board->pieces[PAWN_WHITE];
  BitBoard blackPawns = board->pieces[PAWN_BLACK];
  BitBoard whitePawnAttacks = ShiftNE(whitePawns) | ShiftNW(whitePawns);
  BitBoard blackPawnAttacks = ShiftSE(blackPawns) | ShiftSW(blackPawns);

  data->allAttacks[WHITE] = data->attacks[WHITE][PAWN_TYPE] = whitePawnAttacks;
  data->allAttacks[BLACK] = data->attacks[BLACK][PAWN_TYPE] = blackPawnAttacks;
  data->attacks[WHITE][KNIGHT_TYPE] = data->attacks[BLACK][KNIGHT_TYPE] = 0ULL;
  data->attacks[WHITE][BISHOP_TYPE] = data->attacks[BLACK][BISHOP_TYPE] = 0ULL;
  data->attacks[WHITE][ROOK_TYPE] = data->attacks[BLACK][ROOK_TYPE] = 0ULL;
  data->attacks[WHITE][QUEEN_TYPE] = data->attacks[BLACK][QUEEN_TYPE] = 0ULL;
  data->attacks[WHITE][KING_TYPE] = data->attacks[BLACK][KING_TYPE] = 0ULL;
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
  data->ksAttackWeight[WHITE] = data->ksAttackWeight[BLACK] = 0;
  data->ksAttackerCount[WHITE] = data->ksAttackerCount[BLACK] = 0;

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
        data->ksAttackerCount[side]++;

        if (T) {
          C.ksAttackerCount[xside]++;
          C.ksAttackerWeights[xside][pieceType]++;
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

  uint8_t rights = side == WHITE ? (board->castling & 0xC) : (board->castling & 0x3);

  shelter += CAN_CASTLE * bits((uint64_t)rights);
  if (T)
    C.castlingRights += cs[side] * bits((uint64_t)rights);

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
  static const BitBoard CENTER_FILES = (C_FILE | D_FILE | E_FILE | F_FILE);

  Score s = S(0, 0);
  int xside = side ^ 1;

  BitBoard space = side == WHITE ? ShiftS(board->pieces[PAWN[side]]) : ShiftN(board->pieces[PAWN[side]]);
  space |= side == WHITE ? (ShiftS(space) | ShiftSS(space)) : (ShiftN(space) | ShiftNN(space));
  space &= ~(board->pieces[PAWN[side]] | data->attacks[xside][PAWN_TYPE] |
             (data->twoAttacks[xside] & ~data->twoAttacks[side]));
  space &= CENTER_FILES;

  int pieces = bits(board->occupancies[side]);
  int openFiles = 8 - bits(Fill(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK], N) & 0xFULL);
  int scalar = bits(space) * max(0, pieces - openFiles) * max(0, pieces - openFiles);

  s += S((SPACE * scalar) / 1024, 0);

  if (T)
    C.space += cs[side] * scalar;

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

  EvalData data;
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

  if (T || abs(scoreMG(s) + scoreEG(s)) / 2 < 1024) {
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

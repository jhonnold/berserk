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
const Score MATERIAL_VALUES[7] = { S(100, 100), S(325, 325), S(325, 325), S(550, 550), S(1000, 1000), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(30, 91);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  50, 243), S(  46, 264), S(  24, 237), S(  75, 202),
 S( -15, 123), S( -32, 134), S(  12,  80), S(  21,  46),
 S( -31,  94), S( -31,  71), S( -23,  53), S( -22,  41),
 S( -38,  69), S( -42,  64), S( -28,  49), S( -15,  46),
 S( -42,  60), S( -43,  57), S( -35,  55), S( -26,  59),
 S( -33,  66), S( -27,  64), S( -26,  63), S( -19,  67),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 131, 192), S(  15, 191), S( -16, 216), S(  46, 198),
 S( -11,  76), S(  11,  63), S(  35,  46), S(  30,  35),
 S( -44,  57), S( -24,  43), S( -35,  43), S(   2,  30),
 S( -60,  52), S( -37,  47), S( -44,  48), S(  -6,  38),
 S( -71,  49), S( -34,  42), S( -45,  56), S( -17,  55),
 S( -62,  61), S( -14,  53), S( -34,  63), S( -14,  75),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-120,  79), S(-111, 105), S( -46, 113), S( -27, 105),
 S(  -3, 102), S(  18, 115), S(  66, 100), S(  70, 108),
 S(  56,  86), S(  71,  98), S(  70, 118), S(  61, 126),
 S(  51, 116), S(  60, 119), S(  75, 123), S(  66, 137),
 S(  58, 114), S(  62, 112), S(  79, 128), S(  74, 134),
 S(  29, 104), S(  38, 106), S(  52, 117), S(  55, 136),
 S(  20, 112), S(  34, 107), S(  40, 117), S(  49, 118),
 S( -16, 125), S(  36, 106), S(  31, 109), S(  41, 125),
},{
 S(-158, -59), S(-183,  89), S(-153, 101), S(  -7, 105),
 S(  10,  54), S(  79,  81), S(  69,  98), S(  68, 107),
 S(  65,  70), S(  59,  81), S( 133,  77), S( 112, 100),
 S(  56, 106), S(  65, 114), S(  64, 132), S(  64, 134),
 S(  63, 119), S(  38, 121), S(  82, 127), S(  60, 146),
 S(  48, 107), S(  67, 102), S(  60, 113), S(  70, 133),
 S(  58, 112), S(  63, 103), S(  52, 111), S(  50, 116),
 S(  34, 119), S(  32, 111), S(  52, 111), S(  54, 119),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -10, 116), S(   8, 139), S( -24, 133), S( -58, 148),
 S(  22, 130), S(   9, 115), S(  49, 124), S(  45, 131),
 S(  53, 127), S(  54, 130), S(  23, 112), S(  69, 124),
 S(  45, 134), S(  78, 127), S(  64, 132), S(  62, 153),
 S(  67, 119), S(  58, 132), S(  70, 140), S(  79, 135),
 S(  59, 121), S(  79, 136), S(  58, 122), S(  63, 141),
 S(  67, 129), S(  61,  93), S(  73, 117), S(  55, 133),
 S(  48,  95), S(  73, 120), S(  57, 134), S(  60, 129),
},{
 S( -41,  85), S(  47, 115), S( -41, 127), S( -79, 129),
 S(  13,  94), S( -26, 116), S(  29, 136), S(  58, 114),
 S(  81, 117), S(  51, 125), S(  61, 120), S(  67, 130),
 S(  32, 126), S(  74, 126), S(  71, 135), S(  73, 134),
 S(  88,  97), S(  74, 124), S(  78, 135), S(  70, 137),
 S(  69, 115), S(  86, 120), S(  66, 118), S(  65, 148),
 S(  74, 102), S(  73,  99), S(  75, 125), S(  67, 131),
 S(  67,  81), S(  62, 140), S(  53, 138), S(  69, 124),
}};

const Score ROOK_PSQT[2][32] = {{
 S(   1, 255), S(   3, 262), S(   9, 267), S( -17, 272),
 S(  19, 238), S(   9, 253), S(  29, 252), S(  50, 235),
 S(   5, 242), S(  42, 239), S(  29, 243), S(  30, 239),
 S(  15, 252), S(  33, 245), S(  48, 246), S(  52, 233),
 S(   8, 248), S(  19, 249), S(  26, 248), S(  35, 238),
 S(   5, 241), S(  18, 231), S(  23, 237), S(  26, 234),
 S(   6, 236), S(  17, 234), S(  31, 233), S(  35, 227),
 S(  18, 238), S(  18, 234), S(  21, 239), S(  32, 224),
},{
 S(  90, 245), S(  44, 262), S( -24, 275), S(   7, 263),
 S(  53, 231), S(  76, 241), S(  21, 248), S(  25, 241),
 S(   5, 238), S(  79, 223), S(  47, 216), S(  64, 216),
 S(  22, 236), S(  68, 222), S(  51, 217), S(  57, 222),
 S(   4, 232), S(  35, 228), S(  22, 225), S(  43, 228),
 S(  11, 213), S(  38, 212), S(  30, 214), S(  38, 222),
 S(   7, 222), S(  50, 200), S(  33, 209), S(  39, 216),
 S(  31, 209), S(  27, 222), S(  28, 217), S(  39, 215),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(  15, 464), S(  87, 400), S( 149, 396), S( 138, 429),
 S(  60, 428), S(  56, 429), S(  75, 460), S(  65, 478),
 S(  89, 402), S( 101, 392), S(  95, 421), S(  96, 443),
 S(  93, 440), S( 104, 454), S( 105, 461), S(  86, 494),
 S( 112, 407), S(  93, 473), S(  99, 472), S(  81, 502),
 S(  95, 411), S( 109, 420), S(  99, 445), S(  94, 451),
 S( 107, 371), S( 107, 382), S( 115, 385), S( 114, 402),
 S(  96, 386), S(  94, 378), S(  97, 392), S( 107, 388),
},{
 S( 201, 369), S( 297, 315), S(  89, 435), S( 148, 406),
 S( 156, 387), S( 126, 426), S(  86, 473), S(  43, 488),
 S( 116, 378), S( 108, 391), S( 119, 400), S( 112, 446),
 S(  95, 422), S( 107, 444), S( 107, 401), S(  86, 484),
 S( 127, 387), S( 129, 423), S( 114, 421), S(  92, 489),
 S( 114, 371), S( 128, 390), S( 113, 419), S( 100, 449),
 S( 139, 312), S( 136, 326), S( 129, 345), S( 117, 399),
 S( 129, 329), S( 100, 354), S(  97, 342), S( 107, 370),
}};

const Score KING_PSQT[2][32] = {{
 S( 193,-110), S( 160, -45), S(  23,  -3), S(  82, -15),
 S( -17,  39), S(  33,  52), S(  19,  57), S(  25,  33),
 S(  54,  24), S(  82,  47), S(  60,  54), S( -28,  67),
 S(  39,  27), S(  56,  44), S( -11,  60), S( -67,  64),
 S( -21,  18), S(  22,  31), S(  14,  37), S( -83,  50),
 S(  -9,  -1), S(   8,  16), S( -14,  22), S( -44,  25),
 S(  13, -18), S( -17,  16), S( -36,  10), S( -60,  10),
 S(  24, -75), S(  27, -35), S(   0, -24), S( -31, -32),
},{
 S( 102,-144), S(-181,  42), S( 111, -22), S( -15, -19),
 S(-126,   5), S(   8,  62), S( -50,  64), S( 138,  22),
 S( -13,   9), S( 155,  53), S( 218,  51), S(  57,  55),
 S( -79,   8), S(  39,  41), S( -13,  63), S( -82,  66),
 S( -63,  -5), S( -43,  28), S( -22,  36), S( -73,  49),
 S(   1, -13), S(  20,  10), S(  -6,  17), S( -26,  22),
 S(  10, -11), S(   3,  13), S( -32,  12), S( -63,   9),
 S(  14, -58), S(  15, -16), S( -12, -18), S( -13, -41),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -45,  41), S(   6,  43), S(  24,  37), S(  53,  41),
 S(  20,  18), S(  42,  27), S(  37,  36), S(  45,  39),
 S(  19,   9), S(  35,  21), S(  18,  28), S(  23,  33),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -10,  37), S(  30,  17), S(  58,  27), S(  56,   9),
 S(   1,  17), S(  21,  22), S(  37,  12), S(  50,  14),
 S( -15,  39), S(  45,  14), S(  31,  16), S(  39,  28),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -15, -16), S(  21,  66), S(  42, 108), S(  54, 123),
 S(  66, 134), S(  75, 146), S(  84, 147), S(  93, 147),
 S( 103, 137),};

const Score BISHOP_MOBILITIES[14] = {
 S(  31,  56), S(  51,  89), S(  66, 114), S(  74, 139),
 S(  82, 148), S(  87, 159), S(  90, 166), S(  93, 171),
 S(  94, 174), S(  98, 177), S( 106, 171), S( 122, 169),
 S( 122, 177), S( 121, 171),};

const Score ROOK_MOBILITIES[15] = {
 S( -20, -23), S(   0, 159), S(  16, 192), S(  25, 197),
 S(  24, 222), S(  25, 235), S(  20, 246), S(  25, 247),
 S(  31, 251), S(  37, 255), S(  41, 260), S(  39, 265),
 S(  42, 270), S(  53, 273), S(  72, 266),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2077,-2004), S(  88,-276), S( 132, 119), S( 121, 362),
 S( 127, 436), S( 131, 457), S( 129, 495), S( 129, 525),
 S( 131, 545), S( 134, 552), S( 136, 561), S( 139, 562),
 S( 139, 566), S( 143, 565), S( 142, 567), S( 150, 552),
 S( 149, 547), S( 153, 539), S( 159, 517), S( 185, 482),
 S( 207, 439), S( 221, 398), S( 264, 353), S( 305, 272),
 S( 298, 261), S( 485,  89), S( 412,  67), S( 486, -59),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(13, 21);

const Score BISHOP_OUTPOST_REACHABLE = S(8, 9);

const Score BISHOP_TRAPPED = S(-148, -198);

const Score ROOK_TRAPPED = S(-62, -21);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(28, 19);

const Score ROOK_OPEN_FILE = S(30, 17);

const Score ROOK_SEMI_OPEN = S(7, 15);

const Score DEFENDED_PAWN = S(14, 10);

const Score DOUBLED_PAWN = S(10, -31);

const Score OPPOSED_ISOLATED_PAWN = S(-4, -10);

const Score OPEN_ISOLATED_PAWN = S(-10, -19);

const Score BACKWARDS_PAWN = S(-12, -18);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  76,  57), S(  29,  39), S(  13,  12),
 S(   8,   2), S(   7,   1), S(   2,  -1), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -50, 322), S(  12,  71),
 S( -10,  44), S( -24,  19), S( -33,   3), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  49, 248), S( -10, 274), S(  -9, 144),
 S( -26,  76), S(  -8,  46), S(  -6,  40), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -12);

const Score PASSED_PAWN_KING_PROXIMITY = S(-8, 27);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(31, 32);

const Score KNIGHT_THREATS[6] = { S(0, 21), S(1, 1), S(45, 40), S(106, 2), S(54, -68), S(261, 16),};

const Score BISHOP_THREATS[6] = { S(0, 20), S(34, 37), S(11, 34), S(72, 11), S(70, 54), S(133, 104),};

const Score ROOK_THREATS[6] = { S(2, 31), S(39, 43), S(43, 53), S(9, 18), S(90, -5), S(281, -17),};

const Score KING_THREAT = S(-24, 35);

const Score PAWN_THREAT = S(76, 22);

const Score PAWN_PUSH_THREAT = S(24, 1);

const Score HANGING_THREAT = S(7, 11);

const Score SPACE[15] = {
 S( 581, -16), S( 147, -13), S(  61,  -8), S(  20,  -3), S(   2,   1),
 S(  -9,   3), S( -13,   6), S( -14,   8), S( -13,  11), S( -10,  14),
 S(  -6,  16), S(  -3,  22), S(   2,  23), S(   7,  15), S(  13,-250),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-26, 4), S(22, 78), S(-24, 65), S(-22, 21), S(2, 3), S(34, -18), S(30, -43), S(0, 0),},
 { S(-51, 2), S(5, 70), S(-35, 51), S(-39, 27), S(-32, 10), S(23, -14), S(29, -25), S(0, 0),},
 { S(-29, -5), S(31, 66), S(-56, 46), S(-19, 17), S(-19, 4), S(-8, 1), S(31, -12), S(0, 0),},
 { S(-30, 11), S(22, 66), S(-74, 42), S(-24, 28), S(-23, 21), S(-13, 13), S(-14, 13), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-21, -15), S(-30, -2), S(-28, -8), S(-38, 4), S(-66, 37), S(10, 159), S(319, 184), S(0, 0),},
 { S(-22, 0), S(0, -6), S(2, -4), S(-17, 8), S(-35, 29), S(-109, 168), S(113, 237), S(0, 0),},
 { S(37, -17), S(34, -19), S(21, -8), S(4, 1), S(-19, 23), S(-110, 141), S(-120, 229), S(0, 0),},
 { S(-6, -5), S(0, -11), S(15, -11), S(2, -12), S(-24, 13), S(-52, 116), S(-99, 216), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-23, -11), S(46, -67), S(19, -42), S(15, -35), S(-1, -27), S(-1, -68), S(0, 0), S(0, 0),
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

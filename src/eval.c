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

const Score BISHOP_PAIR = S(31, 103);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  44, 292), S(  39, 317), S(  20, 281), S(  70, 239),
 S( -16, 164), S( -36, 176), S(  12, 112), S(  24,  74),
 S( -32, 129), S( -31, 102), S( -23,  81), S( -22,  68),
 S( -40,  99), S( -43,  93), S( -30,  76), S( -16,  74),
 S( -44,  89), S( -46,  85), S( -37,  83), S( -29,  89),
 S( -34,  96), S( -28,  94), S( -29,  93), S( -20,  97),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 121, 242), S(   6, 231), S( -53, 261), S(  37, 236),
 S( -13, 110), S(  10,  96), S(  41,  74), S(  31,  62),
 S( -48,  88), S( -23,  72), S( -35,  70), S(   2,  57),
 S( -65,  79), S( -38,  74), S( -46,  76), S(  -6,  66),
 S( -75,  76), S( -36,  69), S( -47,  85), S( -19,  85),
 S( -66,  90), S( -15,  83), S( -38,  94), S( -16, 105),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-120, 127), S(-103, 151), S( -52, 168), S( -31, 160),
 S(   3, 155), S(  23, 171), S(  69, 157), S(  79, 165),
 S(  67, 136), S(  78, 154), S(  76, 176), S(  67, 186),
 S(  57, 171), S(  67, 174), S(  81, 181), S(  73, 198),
 S(  64, 168), S(  67, 167), S(  85, 186), S(  79, 194),
 S(  34, 155), S(  43, 160), S(  58, 173), S(  59, 194),
 S(  22, 160), S(  37, 155), S(  44, 170), S(  54, 174),
 S( -14, 173), S(  39, 155), S(  34, 163), S(  47, 181),
},{
 S(-149, -45), S(-179, 129), S(-134, 141), S(  -7, 158),
 S(  21,  95), S(  83, 134), S(  86, 149), S(  79, 161),
 S(  75, 119), S(  65, 135), S( 146, 129), S( 119, 160),
 S(  64, 161), S(  71, 169), S(  74, 192), S(  71, 192),
 S(  71, 175), S(  48, 178), S(  89, 186), S(  66, 208),
 S(  53, 159), S(  74, 156), S(  66, 171), S(  75, 191),
 S(  61, 159), S(  67, 151), S(  57, 164), S(  56, 171),
 S(  36, 164), S(  36, 159), S(  55, 164), S(  59, 176),
}};

const Score BISHOP_PSQT[2][32] = {{
 S(  -2, 173), S(   7, 200), S( -31, 193), S( -49, 207),
 S(  18, 193), S(  10, 171), S(  56, 181), S(  49, 192),
 S(  59, 186), S(  59, 190), S(  30, 167), S(  77, 183),
 S(  50, 192), S(  87, 182), S(  71, 192), S(  70, 215),
 S(  75, 175), S(  64, 189), S(  77, 201), S(  88, 193),
 S(  65, 175), S(  86, 194), S(  62, 177), S(  68, 199),
 S(  70, 179), S(  66, 143), S(  79, 171), S(  60, 191),
 S(  51, 148), S(  79, 175), S(  62, 192), S(  65, 190),
},{
 S( -24, 133), S(  55, 171), S( -37, 183), S( -66, 184),
 S(  19, 148), S( -20, 170), S(  31, 198), S(  68, 169),
 S(  89, 177), S(  60, 183), S(  73, 173), S(  74, 190),
 S(  37, 183), S(  82, 180), S(  81, 194), S(  81, 193),
 S( 100, 150), S(  82, 181), S(  86, 194), S(  78, 196),
 S(  77, 169), S(  94, 177), S(  71, 173), S(  71, 206),
 S(  79, 145), S(  78, 145), S(  81, 179), S(  72, 188),
 S(  71, 130), S(  64, 202), S(  56, 194), S(  75, 185),
}};

const Score ROOK_PSQT[2][32] = {{
 S(  -1, 368), S(   4, 375), S(   6, 383), S( -18, 387),
 S(  18, 350), S(   7, 367), S(  29, 366), S(  53, 347),
 S(   5, 354), S(  43, 352), S(  29, 356), S(  32, 350),
 S(  13, 364), S(  32, 357), S(  49, 359), S(  53, 344),
 S(   7, 359), S(  19, 359), S(  24, 361), S(  36, 348),
 S(   5, 350), S(  17, 340), S(  23, 346), S(  25, 343),
 S(   7, 343), S(  17, 343), S(  33, 342), S(  36, 336),
 S(  20, 344), S(  19, 341), S(  23, 346), S(  35, 330),
},{
 S( 108, 355), S(  57, 375), S( -20, 391), S(   8, 377),
 S(  65, 341), S(  83, 357), S(  30, 360), S(  25, 353),
 S(  14, 346), S(  86, 337), S(  56, 325), S(  66, 326),
 S(  23, 346), S(  68, 334), S(  51, 329), S(  58, 332),
 S(   4, 340), S(  36, 337), S(  22, 335), S(  44, 336),
 S(  15, 317), S(  43, 318), S(  32, 321), S(  40, 330),
 S(  10, 327), S(  56, 304), S(  37, 315), S(  42, 323),
 S(  36, 311), S(  31, 327), S(  31, 323), S(  42, 320),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(  14, 648), S(  88, 580), S( 156, 580), S( 139, 621),
 S(  55, 605), S(  52, 609), S(  67, 653), S(  56, 673),
 S(  89, 575), S( 100, 567), S(  94, 603), S(  95, 629),
 S(  94, 614), S( 104, 632), S( 105, 643), S(  84, 683),
 S( 114, 581), S(  94, 651), S( 100, 652), S(  81, 685),
 S(  97, 583), S( 110, 598), S(  99, 626), S(  95, 631),
 S( 108, 543), S( 109, 556), S( 117, 563), S( 116, 581),
 S(  95, 559), S(  94, 551), S(  97, 564), S( 108, 562),
},{
 S( 214, 560), S( 313, 512), S( 117, 604), S( 154, 592),
 S( 162, 582), S( 118, 645), S(  96, 664), S(  40, 680),
 S( 117, 575), S( 111, 589), S( 120, 602), S( 108, 644),
 S(  95, 614), S( 108, 638), S( 106, 592), S(  83, 678),
 S( 126, 576), S( 130, 609), S( 113, 609), S(  91, 675),
 S( 115, 551), S( 129, 573), S( 113, 603), S( 101, 629),
 S( 142, 485), S( 138, 500), S( 131, 518), S( 119, 577),
 S( 131, 497), S(  99, 525), S(  97, 518), S( 109, 542),
}};

const Score KING_PSQT[2][32] = {{
 S( 209,-121), S( 159, -47), S(  36,  -5), S(  66, -12),
 S( -33,  51), S(  26,  62), S(  26,  65), S(  17,  39),
 S(  32,  34), S(  85,  56), S(  46,  67), S( -57,  82),
 S(  28,  34), S(  50,  53), S( -29,  72), S( -95,  77),
 S( -35,  21), S(   7,  38), S(  -8,  46), S(-112,  61),
 S( -10,  -4), S(   5,  18), S( -24,  25), S( -60,  30),
 S(  17, -24), S( -16,  14), S( -38,   8), S( -66,  11),
 S(  31, -91), S(  32, -43), S(   4, -34), S( -30, -42),
},{
 S( 129,-163), S(-181,  48), S( 140, -26), S( -53, -11),
 S(-154,   8), S(  31,  70), S( -56,  78), S( 148,  33),
 S( -18,  14), S( 176,  65), S( 221,  67), S(  63,  69),
 S(-102,  12), S(  32,  53), S( -29,  78), S(-109,  82),
 S( -83,  -2), S( -63,  37), S( -38,  46), S(-100,  62),
 S(  -2, -14), S(  16,  15), S( -15,  23), S( -40,  29),
 S(  10, -13), S(   2,  15), S( -37,  13), S( -71,  12),
 S(  16, -68), S(  17, -20), S( -13, -23), S( -15, -48),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -58,  33), S(   3,  36), S(  26,  34), S(  60,  47),
 S(  19,   9), S(  45,  25), S(  38,  36), S(  48,  46),
 S(  20,   2), S(  40,  16), S(  20,  27), S(  24,  32),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -15,  23), S(  29,   9), S(  54,  26), S(  62,   8),
 S(  -2,   3), S(  20,  20), S(  39,   8), S(  52,  14),
 S( -18,  31), S(  48,  10), S(  33,  11), S(  40,  30),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(  -9,  27), S(  28, 122), S(  49, 170), S(  62, 186),
 S(  73, 199), S(  82, 212), S(  92, 213), S( 102, 214),
 S( 112, 202),};

const Score BISHOP_MOBILITIES[14] = {
 S(  38, 101), S(  60, 145), S(  75, 173), S(  83, 201),
 S(  91, 213), S(  97, 225), S( 100, 233), S( 103, 239),
 S( 105, 243), S( 107, 247), S( 117, 242), S( 135, 238),
 S( 142, 245), S( 138, 239),};

const Score ROOK_MOBILITIES[15] = {
 S( -14,  54), S(   3, 244), S(  20, 287), S(  30, 294),
 S(  28, 322), S(  30, 336), S(  24, 348), S(  29, 350),
 S(  36, 354), S(  41, 361), S(  45, 366), S(  44, 372),
 S(  47, 379), S(  61, 382), S(  80, 376),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2083,-2005), S(  42,   7), S( 136, 335), S( 123, 589),
 S( 130, 673), S( 135, 692), S( 133, 734), S( 134, 766),
 S( 135, 789), S( 138, 796), S( 140, 807), S( 143, 810),
 S( 143, 815), S( 146, 814), S( 147, 816), S( 154, 802),
 S( 154, 798), S( 156, 791), S( 160, 773), S( 189, 734),
 S( 208, 693), S( 225, 651), S( 267, 607), S( 318, 516),
 S( 312, 513), S( 500, 328), S( 434, 306), S( 458, 219),
};

const Score MINOR_BEHIND_PAWN = S(5, 21);

const Score KNIGHT_OUTPOST_REACHABLE = S(13, 24);

const Score BISHOP_OUTPOST_REACHABLE = S(8, 10);

const Score BISHOP_TRAPPED = S(-149, -241);

const Score ROOK_TRAPPED = S(-66, -26);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(30, 21);

const Score ROOK_OPEN_FILE = S(31, 19);

const Score ROOK_SEMI_OPEN = S(7, 16);

const Score DEFENDED_PAWN = S(14, 12);

const Score DOUBLED_PAWN = S(11, -35);

const Score OPPOSED_ISOLATED_PAWN = S(-4, -11);

const Score OPEN_ISOLATED_PAWN = S(-10, -21);

const Score BACKWARDS_PAWN = S(-12, -20);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  77,  66), S(  30,  46), S(  14,  14),
 S(   9,   3), S(   7,   1), S(   2,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -41, 329), S(  13,  78),
 S( -10,  48), S( -25,  21), S( -37,   4), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  34, 310), S( -22, 325), S( -15, 172),
 S( -31,  91), S( -11,  57), S(  -9,  50), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -15);

const Score PASSED_PAWN_KING_PROXIMITY = S(-10, 32);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(30, 37);

const Score KNIGHT_THREATS[6] = { S(1, 22), S(0, 0), S(44, 44), S(103, 4), S(57, -66), S(303, 17),};

const Score BISHOP_THREATS[6] = { S(0, 22), S(35, 47), S(11, 43), S(69, 13), S(71, 71), S(142, 127),};

const Score ROOK_THREATS[6] = { S(3, 34), S(38, 49), S(40, 58), S(10, 18), S(91, 2), S(302, -11),};

const Score KING_THREAT = S(-25, 38);

const Score PAWN_THREAT = S(79, 30);

const Score PAWN_PUSH_THREAT = S(24, 2);

const Score HANGING_THREAT = S(9, 12);

const Score SPACE[15] = {
 S( 974, -24), S( 174, -16), S(  70,  -7), S(  22,  -2), S(   3,   1),
 S(  -8,   4), S( -13,   6), S( -14,   8), S( -13,  10), S( -10,  13),
 S(  -6,  15), S(  -2,  21), S(   2,  21), S(   7,  10), S(  14,-256),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-28, 1), S(46, 79), S(-28, 76), S(-22, 22), S(3, 0), S(37, -23), S(33, -50), S(0, 0),},
 { S(-56, 0), S(9, 69), S(-38, 59), S(-42, 30), S(-35, 11), S(23, -15), S(31, -26), S(0, 0),},
 { S(-31, -7), S(28, 72), S(-64, 52), S(-22, 18), S(-21, 4), S(-10, 1), S(33, -12), S(0, 0),},
 { S(-34, 12), S(10, 78), S(-83, 47), S(-25, 31), S(-24, 23), S(-14, 14), S(-16, 15), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-22, -16), S(-32, -2), S(-30, -10), S(-41, 5), S(-70, 43), S(5, 189), S(317, 241), S(0, 0),},
 { S(-23, 1), S(0, -6), S(2, -4), S(-18, 11), S(-35, 33), S(-120, 195), S(107, 293), S(0, 0),},
 { S(40, -19), S(36, -20), S(24, -9), S(5, 1), S(-21, 26), S(-117, 164), S(-171, 283), S(0, 0),},
 { S(-5, -7), S(1, -15), S(17, -12), S(4, -15), S(-24, 13), S(-59, 137), S(-130, 270), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-36, -5), S(53, -78), S(21, -48), S(17, -39), S(1, -30), S(0, -77), S(0, 0), S(0, 0),
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

  for (int pc = KNIGHT_WHITE; pc <= KNIGHT_BLACK; pc++)
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

  // lazy eval is based on distance from 0, not a/b. we can be more inaccurate the higher the score
  // this idea exists in SF classical in multiple stages. In berserk we just execute it after the
  // two cached values have been determined (material/pawns)
  Score mat = NonPawnMaterialValue(board);
  if (T || (596 + mat / 24 > abs(scoreMG(s) + scoreEG(s)) / 2)) {
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

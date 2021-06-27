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
 S(  44, 292), S(  38, 317), S(  19, 280), S(  71, 237),
 S( -16, 163), S( -35, 175), S(  14, 111), S(  24,  72),
 S( -33, 128), S( -32, 101), S( -23,  80), S( -22,  66),
 S( -40,  98), S( -43,  92), S( -31,  76), S( -16,  72),
 S( -44,  88), S( -47,  84), S( -36,  82), S( -29,  88),
 S( -34,  96), S( -28,  93), S( -29,  93), S( -19,  96),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 118, 242), S(   5, 231), S( -49, 259), S(  38, 235),
 S( -14, 109), S(  10,  95), S(  41,  73), S(  31,  61),
 S( -48,  87), S( -23,  70), S( -35,  69), S(   2,  56),
 S( -65,  79), S( -38,  73), S( -46,  75), S(  -6,  64),
 S( -75,  75), S( -37,  67), S( -47,  84), S( -20,  84),
 S( -65,  89), S( -16,  82), S( -39,  94), S( -15, 104),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-120, 128), S(-104, 151), S( -51, 167), S( -30, 159),
 S(   3, 154), S(  24, 169), S(  70, 155), S(  79, 163),
 S(  67, 136), S(  78, 152), S(  77, 173), S(  68, 183),
 S(  58, 171), S(  68, 174), S(  82, 179), S(  76, 196),
 S(  65, 168), S(  69, 167), S(  87, 185), S(  81, 194),
 S(  33, 157), S(  43, 160), S(  58, 172), S(  58, 196),
 S(  20, 165), S(  36, 159), S(  43, 172), S(  54, 174),
 S( -16, 177), S(  37, 159), S(  33, 164), S(  47, 181),
},{
 S(-149, -44), S(-178, 129), S(-133, 140), S(  -6, 157),
 S(  21,  94), S(  82, 133), S(  87, 147), S(  79, 159),
 S(  76, 118), S(  66, 134), S( 149, 127), S( 119, 157),
 S(  66, 162), S(  73, 172), S(  75, 191), S(  74, 192),
 S(  71, 176), S(  49, 178), S(  91, 186), S(  68, 209),
 S(  53, 160), S(  74, 155), S(  66, 169), S(  75, 192),
 S(  58, 165), S(  65, 156), S(  56, 165), S(  55, 171),
 S(  34, 169), S(  34, 163), S(  52, 168), S(  59, 175),
}};

const Score BISHOP_PSQT[2][32] = {{
 S(  -2, 171), S(   8, 198), S( -30, 191), S( -49, 204),
 S(  19, 190), S(  11, 169), S(  57, 179), S(  49, 189),
 S(  60, 183), S(  60, 188), S(  31, 164), S(  77, 181),
 S(  52, 190), S(  88, 182), S(  73, 190), S(  73, 213),
 S(  76, 173), S(  65, 189), S(  79, 199), S(  90, 193),
 S(  64, 176), S(  87, 195), S(  63, 178), S(  67, 201),
 S(  68, 185), S(  65, 146), S(  77, 173), S(  59, 191),
 S(  51, 149), S(  77, 177), S(  62, 192), S(  65, 188),
},{
 S( -24, 130), S(  56, 169), S( -37, 181), S( -66, 182),
 S(  20, 145), S( -20, 168), S(  32, 195), S(  69, 166),
 S(  91, 175), S(  61, 181), S(  77, 171), S(  75, 188),
 S(  40, 183), S(  86, 181), S(  83, 193), S(  84, 192),
 S( 101, 148), S(  82, 181), S(  87, 193), S(  80, 196),
 S(  76, 170), S(  94, 177), S(  72, 174), S(  71, 208),
 S(  76, 152), S(  76, 151), S(  80, 182), S(  71, 189),
 S(  69, 132), S(  63, 203), S(  54, 197), S(  75, 182),
}};

const Score ROOK_PSQT[2][32] = {{
 S(  -2, 366), S(   3, 374), S(   5, 381), S( -18, 385),
 S(  19, 348), S(   7, 365), S(  29, 364), S(  53, 345),
 S(   5, 352), S(  43, 351), S(  28, 355), S(  32, 348),
 S(  14, 363), S(  32, 356), S(  49, 358), S(  53, 343),
 S(   7, 358), S(  19, 358), S(  24, 360), S(  36, 347),
 S(   5, 349), S(  17, 339), S(  23, 345), S(  25, 342),
 S(   7, 342), S(  17, 341), S(  33, 340), S(  37, 334),
 S(  20, 342), S(  19, 339), S(  23, 345), S(  35, 328),
},{
 S( 108, 353), S(  57, 374), S( -21, 389), S(   7, 375),
 S(  65, 340), S(  83, 355), S(  30, 358), S(  26, 351),
 S(  14, 345), S(  86, 335), S(  55, 323), S(  66, 324),
 S(  24, 345), S(  68, 333), S(  51, 328), S(  58, 331),
 S(   4, 339), S(  36, 336), S(  22, 334), S(  43, 335),
 S(  15, 315), S(  43, 317), S(  32, 320), S(  40, 329),
 S(  10, 325), S(  56, 303), S(  37, 313), S(  42, 322),
 S(  36, 310), S(  31, 325), S(  31, 322), S(  42, 318),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(  14, 647), S(  88, 579), S( 156, 579), S( 139, 620),
 S(  55, 605), S(  51, 608), S(  66, 652), S(  56, 672),
 S(  89, 575), S( 100, 567), S(  94, 602), S(  94, 629),
 S(  94, 614), S( 104, 632), S( 104, 643), S(  83, 683),
 S( 114, 580), S(  93, 651), S( 100, 652), S(  81, 685),
 S(  97, 583), S( 110, 597), S(  99, 625), S(  95, 629),
 S( 107, 543), S( 108, 556), S( 116, 563), S( 115, 580),
 S(  94, 559), S(  94, 551), S(  97, 563), S( 108, 562),
},{
 S( 213, 559), S( 313, 512), S( 118, 603), S( 154, 591),
 S( 162, 582), S( 117, 645), S(  95, 663), S(  40, 679),
 S( 117, 574), S( 111, 588), S( 119, 602), S( 108, 643),
 S(  94, 615), S( 108, 638), S( 105, 592), S(  82, 678),
 S( 126, 576), S( 129, 609), S( 113, 608), S(  91, 675),
 S( 114, 551), S( 129, 573), S( 113, 602), S( 101, 628),
 S( 142, 485), S( 137, 500), S( 131, 518), S( 119, 576),
 S( 131, 496), S(  98, 526), S(  97, 517), S( 108, 541),
}};

const Score KING_PSQT[2][32] = {{
 S( 209,-121), S( 159, -47), S(  36,  -5), S(  67, -12),
 S( -32,  51), S(  27,  62), S(  26,  65), S(  17,  39),
 S(  32,  34), S(  85,  57), S(  47,  67), S( -56,  82),
 S(  29,  34), S(  50,  53), S( -29,  72), S( -94,  77),
 S( -35,  22), S(   7,  38), S(  -7,  46), S(-113,  61),
 S( -10,  -4), S(   5,  18), S( -23,  24), S( -61,  30),
 S(  17, -23), S( -16,  14), S( -38,   7), S( -66,  10),
 S(  31, -91), S(  32, -43), S(   4, -34), S( -30, -42),
},{
 S( 130,-162), S(-182,  48), S( 141, -26), S( -52, -11),
 S(-154,   8), S(  30,  71), S( -57,  78), S( 148,  33),
 S( -18,  14), S( 176,  66), S( 220,  67), S(  61,  69),
 S(-101,  13), S(  31,  53), S( -29,  79), S(-109,  83),
 S( -83,  -1), S( -62,  37), S( -38,  46), S(-100,  62),
 S(  -2, -14), S(  16,  15), S( -15,  23), S( -40,  30),
 S(  10, -12), S(   2,  15), S( -37,  13), S( -71,  12),
 S(  16, -68), S(  17, -19), S( -13, -23), S( -15, -48),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -53,  48), S(   7,  49), S(  27,  45), S(  60,  48),
 S(  21,  21), S(  45,  32), S(  38,  43), S(  48,  49),
 S(  20,  10), S(  38,  26), S(  20,  34), S(  25,  37),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -11,  42), S(  35,  20), S(  57,  36), S(  63,  10),
 S(   1,  20), S(  21,  27), S(  41,  15), S(  52,  19),
 S( -17,  46), S(  48,  18), S(  33,  19), S(  40,  35),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(  -9,  24), S(  28, 119), S(  49, 168), S(  62, 185),
 S(  74, 198), S(  82, 212), S(  92, 213), S( 102, 215),
 S( 112, 204),};

const Score BISHOP_MOBILITIES[14] = {
 S(  37, 106), S(  59, 147), S(  75, 175), S(  83, 203),
 S(  91, 214), S(  97, 226), S( 100, 234), S( 103, 240),
 S( 105, 244), S( 107, 249), S( 116, 243), S( 134, 240),
 S( 141, 246), S( 138, 241),};

const Score ROOK_MOBILITIES[15] = {
 S( -14,  53), S(   2, 247), S(  21, 286), S(  30, 292),
 S(  29, 320), S(  30, 334), S(  24, 346), S(  29, 349),
 S(  35, 353), S(  41, 359), S(  45, 365), S(  44, 371),
 S(  47, 377), S(  61, 381), S(  80, 375),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2082,-2005), S(  40,   2), S( 134, 334), S( 121, 587),
 S( 129, 670), S( 134, 688), S( 132, 729), S( 134, 760),
 S( 135, 784), S( 137, 791), S( 139, 802), S( 142, 805),
 S( 142, 811), S( 145, 810), S( 146, 812), S( 153, 798),
 S( 153, 794), S( 155, 787), S( 159, 769), S( 188, 731),
 S( 208, 689), S( 224, 647), S( 266, 604), S( 318, 513),
 S( 313, 508), S( 501, 325), S( 433, 303), S( 458, 216),
};

const Score MINOR_BEHIND_PAWN = S(10, 5);

const Score KNIGHT_OUTPOST_REACHABLE = S(12, 24);

const Score BISHOP_OUTPOST_REACHABLE = S(9, 11);

const Score BISHOP_TRAPPED = S(-148, -241);

const Score ROOK_TRAPPED = S(-66, -26);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(30, 21);

const Score ROOK_OPEN_FILE = S(31, 19);

const Score ROOK_SEMI_OPEN = S(7, 16);

const Score DEFENDED_PAWN = S(14, 12);

const Score DOUBLED_PAWN = S(11, -34);

const Score OPPOSED_ISOLATED_PAWN = S(-4, -11);

const Score OPEN_ISOLATED_PAWN = S(-10, -21);

const Score BACKWARDS_PAWN = S(-12, -20);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  77,  66), S(  31,  46), S(  14,  14),
 S(   9,   3), S(   7,   2), S(   2,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -42, 329), S(  13,  79),
 S( -10,  48), S( -25,  21), S( -36,   4), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  34, 308), S( -24, 325), S( -17, 172),
 S( -32,  91), S( -11,  57), S(  -9,  50), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(4, -15);

const Score PASSED_PAWN_KING_PROXIMITY = S(-10, 32);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(32, 38);

const Score KNIGHT_THREATS[6] = { S(1, 22), S(1, -2), S(43, 44), S(103, 3), S(57, -68), S(304, 16),};

const Score BISHOP_THREATS[6] = { S(0, 22), S(36, 46), S(12, 41), S(69, 13), S(71, 69), S(141, 126),};

const Score ROOK_THREATS[6] = { S(3, 34), S(38, 50), S(40, 60), S(10, 18), S(91, 2), S(302, -11),};

const Score KING_THREAT = S(-26, 38);

const Score PAWN_THREAT = S(79, 30);

const Score PAWN_PUSH_THREAT = S(24, 3);

const Score HANGING_THREAT = S(9, 12);

const Score SPACE[15] = {
 S( 969, -24), S( 174, -16), S(  70,  -7), S(  22,  -2), S(   3,   2),
 S(  -8,   4), S( -13,   6), S( -14,   8), S( -13,  10), S( -10,  13),
 S(  -6,  15), S(  -3,  21), S(   2,  22), S(   7,  11), S(  14,-253),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-28, 1), S(48, 77), S(-27, 76), S(-22, 21), S(3, 0), S(37, -23), S(33, -50), S(0, 0),},
 { S(-56, 0), S(11, 68), S(-38, 58), S(-42, 30), S(-35, 11), S(22, -14), S(31, -27), S(0, 0),},
 { S(-31, -7), S(29, 71), S(-65, 53), S(-22, 18), S(-22, 4), S(-10, 1), S(33, -12), S(0, 0),},
 { S(-34, 12), S(11, 77), S(-83, 47), S(-25, 30), S(-24, 23), S(-14, 14), S(-15, 14), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-22, -16), S(-32, -3), S(-29, -10), S(-41, 5), S(-70, 43), S(5, 188), S(315, 241), S(0, 0),},
 { S(-23, 1), S(0, -6), S(2, -4), S(-18, 10), S(-35, 33), S(-119, 195), S(106, 292), S(0, 0),},
 { S(40, -19), S(35, -20), S(23, -9), S(5, 1), S(-19, 26), S(-118, 163), S(-167, 281), S(0, 0),},
 { S(-5, -7), S(2, -14), S(17, -12), S(4, -14), S(-24, 13), S(-57, 136), S(-130, 269), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-37, -4), S(53, -78), S(21, -47), S(17, -39), S(1, -31), S(0, -77), S(0, 0), S(0, 0),
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

  // bishop pair bonus first
  if (bits(board->pieces[BISHOP[side]]) > 1) {
    s += BISHOP_PAIR;

    if (T)
      C.bishopPair += cs[side];
  }

  BitBoard minorsBehindPawns = (board->pieces[KNIGHT[side]] | board->pieces[BISHOP[side]]) &
                               (side == WHITE ? ShiftS(board->pieces[PAWN_WHITE]) : ShiftN(board->pieces[PAWN_BLACK]));
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

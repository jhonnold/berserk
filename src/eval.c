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

const Score BISHOP_PAIR = S(28, 96);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  43, 268), S(  38, 290), S(  21, 258), S(  64, 223),
 S( -20, 144), S( -38, 155), S(   9,  95), S(  18,  60),
 S( -35, 111), S( -34,  86), S( -26,  67), S( -25,  54),
 S( -41,  84), S( -45,  78), S( -31,  62), S( -19,  60),
 S( -45,  74), S( -47,  70), S( -38,  69), S( -30,  73),
 S( -36,  81), S( -30,  79), S( -30,  78), S( -23,  81),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 125, 219), S(  13, 211), S( -31, 236), S(  35, 216),
 S( -16,  94), S(   5,  81), S(  33,  61), S(  26,  49),
 S( -49,  73), S( -27,  58), S( -37,  56), S(  -2,  44),
 S( -65,  66), S( -41,  61), S( -46,  61), S( -10,  52),
 S( -75,  62), S( -38,  55), S( -48,  70), S( -21,  70),
 S( -66,  75), S( -18,  68), S( -38,  79), S( -18,  88),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-117, 103), S(-105, 129), S( -51, 141), S( -30, 133),
 S(  -4, 130), S(  17, 144), S(  60, 130), S(  65, 138),
 S(  56, 114), S(  69, 127), S(  65, 147), S(  57, 156),
 S(  49, 145), S(  57, 148), S(  70, 154), S(  61, 170),
 S(  55, 143), S(  59, 142), S(  75, 159), S(  69, 167),
 S(  27, 133), S(  35, 136), S(  49, 146), S(  51, 168),
 S(  18, 139), S(  32, 135), S(  37, 147), S(  46, 148),
 S( -17, 150), S(  33, 133), S(  29, 140), S(  40, 158),
},{
 S(-148, -58), S(-189, 109), S(-143, 121), S( -17, 135),
 S(  10,  75), S(  75, 109), S(  65, 127), S(  64, 136),
 S(  64,  96), S(  52, 110), S( 129, 104), S( 104, 132),
 S(  53, 137), S(  61, 145), S(  59, 165), S(  60, 166),
 S(  59, 149), S(  34, 153), S(  77, 159), S(  55, 181),
 S(  44, 137), S(  63, 131), S(  57, 143), S(  67, 165),
 S(  55, 141), S(  60, 132), S(  49, 140), S(  47, 146),
 S(  32, 145), S(  29, 139), S(  49, 142), S(  51, 150),
}};

const Score BISHOP_PSQT[2][32] = {{
 S(  -7, 148), S(   8, 172), S( -30, 166), S( -42, 178),
 S(  17, 163), S(   7, 145), S(  49, 154), S(  42, 165),
 S(  50, 159), S(  51, 163), S(  24, 141), S(  67, 157),
 S(  42, 166), S(  76, 159), S(  61, 166), S(  60, 188),
 S(  65, 151), S(  55, 165), S(  66, 175), S(  77, 169),
 S(  56, 153), S(  75, 171), S(  55, 154), S(  60, 175),
 S(  63, 163), S(  58, 125), S(  70, 150), S(  52, 167),
 S(  45, 128), S(  69, 154), S(  54, 167), S(  57, 164),
},{
 S( -31, 112), S(  48, 147), S( -41, 158), S( -72, 160),
 S(  10, 124), S( -28, 146), S(  22, 171), S(  56, 145),
 S(  78, 152), S(  49, 157), S(  63, 150), S(  64, 164),
 S(  29, 159), S(  72, 158), S(  68, 169), S(  71, 168),
 S(  88, 128), S(  72, 157), S(  75, 169), S(  68, 171),
 S(  66, 147), S(  82, 154), S(  63, 151), S(  63, 182),
 S(  72, 132), S(  70, 131), S(  72, 158), S(  64, 165),
 S(  65, 111), S(  58, 178), S(  50, 172), S(  66, 158),
}};

const Score ROOK_PSQT[2][32] = {{
 S( -10, 319), S(  -4, 325), S(  -5, 333), S( -26, 336),
 S(   7, 303), S(  -4, 319), S(  16, 318), S(  39, 299),
 S(  -7, 307), S(  30, 304), S(  18, 308), S(  19, 303),
 S(   3, 316), S(  20, 310), S(  36, 311), S(  40, 297),
 S(  -3, 312), S(   8, 312), S(  14, 313), S(  24, 302),
 S(  -6, 303), S(   7, 293), S(  12, 299), S(  14, 296),
 S(  -4, 296), S(   6, 296), S(  21, 295), S(  24, 290),
 S(   8, 298), S(   7, 295), S(  11, 300), S(  21, 285),
},{
 S(  85, 308), S(  39, 326), S( -39, 342), S( -11, 329),
 S(  43, 296), S(  60, 310), S(   9, 313), S(  13, 305),
 S(  -3, 300), S(  67, 291), S(  38, 279), S(  54, 279),
 S(   9, 300), S(  55, 287), S(  39, 283), S(  46, 286),
 S(  -7, 294), S(  24, 291), S(  12, 288), S(  32, 290),
 S(   2, 273), S(  27, 274), S(  20, 275), S(  28, 284),
 S(  -3, 282), S(  40, 261), S(  23, 270), S(  29, 278),
 S(  21, 268), S(  17, 282), S(  18, 278), S(  28, 276),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(  13, 575), S(  84, 509), S( 148, 506), S( 135, 540),
 S(  57, 532), S(  53, 533), S(  69, 571), S(  60, 589),
 S(  87, 504), S(  98, 498), S(  92, 528), S(  93, 549),
 S(  91, 543), S( 102, 559), S( 103, 565), S(  84, 601),
 S( 111, 510), S(  91, 576), S(  98, 576), S(  79, 607),
 S(  95, 512), S( 107, 524), S(  97, 549), S(  93, 554),
 S( 104, 476), S( 106, 485), S( 113, 490), S( 112, 507),
 S(  94, 488), S(  92, 482), S(  95, 495), S( 105, 491),
},{
 S( 197, 489), S( 295, 437), S(  95, 541), S( 145, 517),
 S( 150, 507), S( 115, 551), S(  81, 587), S(  40, 597),
 S( 111, 493), S( 103, 508), S( 111, 518), S( 106, 559),
 S(  92, 533), S( 103, 557), S( 102, 513), S(  82, 594),
 S( 122, 499), S( 126, 532), S( 110, 530), S(  89, 596),
 S( 110, 479), S( 124, 499), S( 111, 526), S(  98, 554),
 S( 137, 418), S( 133, 432), S( 127, 448), S( 115, 503),
 S( 126, 433), S(  97, 459), S(  95, 446), S( 106, 472),
}};

const Score KING_PSQT[2][32] = {{
 S( 184,-111), S( 145, -44), S(  30,  -6), S(  81, -15),
 S( -28,  44), S(  25,  57), S(  18,  60), S(  16,  35),
 S(  36,  30), S(  77,  51), S(  51,  60), S( -43,  74),
 S(  32,  29), S(  53,  48), S( -18,  65), S( -83,  70),
 S( -30,  19), S(  13,  33), S(   3,  40), S( -95,  54),
 S(  -8,  -4), S(   7,  15), S( -18,  21), S( -51,  26),
 S(  14, -22), S( -16,  13), S( -36,   7), S( -61,   9),
 S(  27, -84), S(  29, -40), S(   2, -31), S( -29, -39),
},{
 S( 102,-150), S(-193,  46), S( 128, -27), S( -33, -14),
 S(-148,   5), S(  12,  65), S( -44,  68), S( 144,  27),
 S( -14,  10), S( 171,  57), S( 225,  57), S(  65,  61),
 S( -96,  10), S(  32,  46), S( -20,  70), S( -93,  73),
 S( -77,  -4), S( -55,  32), S( -30,  40), S( -84,  54),
 S(  -1, -15), S(  17,  11), S( -11,  19), S( -33,  25),
 S(   9, -13), S(   1,  12), S( -34,  11), S( -67,  10),
 S(  14, -64), S(  15, -19), S( -12, -22), S( -14, -45),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -48,  44), S(   5,  46), S(  24,  42), S(  53,  44),
 S(  19,  19), S(  41,  30), S(  35,  39), S(  44,  44),
 S(  19,  10), S(  35,  23), S(  18,  31), S(  23,  35),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -12,  40), S(  29,  20), S(  52,  32), S(  56,   9),
 S(   1,  18), S(  19,  24), S(  37,  13), S(  48,  17),
 S( -16,  42), S(  45,  15), S(  31,  17), S(  38,  31),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -16,   7), S(  19,  98), S(  38, 144), S(  51, 158),
 S(  62, 170), S(  70, 183), S(  80, 184), S(  89, 185),
 S(  98, 175),};

const Score BISHOP_MOBILITIES[14] = {
 S(  32,  83), S(  51, 119), S(  65, 147), S(  72, 172),
 S(  80, 182), S(  85, 194), S(  88, 202), S(  91, 207),
 S(  92, 211), S(  95, 214), S( 103, 209), S( 120, 206),
 S( 123, 213), S( 121, 207),};

const Score ROOK_MOBILITIES[15] = {
 S( -22,  30), S(  -5, 209), S(  10, 243), S(  19, 248),
 S(  18, 274), S(  20, 286), S(  14, 297), S(  19, 299),
 S(  26, 303), S(  31, 308), S(  35, 313), S(  33, 318),
 S(  36, 325), S(  48, 327), S(  66, 322),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2078,-2004), S(  44, -50), S( 132, 219), S( 120, 458),
 S( 126, 536), S( 130, 555), S( 128, 593), S( 128, 624),
 S( 130, 645), S( 132, 652), S( 134, 662), S( 137, 663),
 S( 137, 668), S( 141, 667), S( 141, 669), S( 148, 656),
 S( 147, 651), S( 148, 646), S( 153, 626), S( 180, 591),
 S( 201, 548), S( 213, 510), S( 256, 466), S( 300, 381),
 S( 298, 369), S( 458, 211), S( 432, 166), S( 457,  68),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(12, 23);

const Score BISHOP_OUTPOST_REACHABLE = S(8, 10);

const Score BISHOP_TRAPPED = S(-141, -219);

const Score ROOK_TRAPPED = S(-62, -25);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(28, 20);

const Score ROOK_OPEN_FILE = S(29, 18);

const Score ROOK_SEMI_OPEN = S(7, 15);

const Score DEFENDED_PAWN = S(13, 11);

const Score DOUBLED_PAWN = S(10, -32);

const Score OPPOSED_ISOLATED_PAWN = S(-4, -10);

const Score OPEN_ISOLATED_PAWN = S(-10, -20);

const Score BACKWARDS_PAWN = S(-11, -19);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  78,  59), S(  29,  42), S(  13,  13),
 S(   8,   3), S(   7,   1), S(   2,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -47, 324), S(  11,  74),
 S(  -9,  45), S( -24,  20), S( -34,   5), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  37, 281), S( -18, 300), S( -13, 158),
 S( -29,  84), S( -11,  53), S(  -9,  46), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -14);

const Score PASSED_PAWN_KING_PROXIMITY = S(-9, 30);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(30, 35);

const Score KNIGHT_THREATS[6] = { S(0, 21), S(0, -1), S(42, 42), S(97, 6), S(53, -66), S(260, 22),};

const Score BISHOP_THREATS[6] = { S(0, 20), S(33, 42), S(11, 35), S(66, 12), S(66, 63), S(130, 115),};

const Score ROOK_THREATS[6] = { S(2, 32), S(39, 48), S(42, 56), S(10, 17), S(87, 3), S(278, -9),};

const Score KING_THREAT = S(-23, 36);

const Score PAWN_THREAT = S(74, 28);

const Score PAWN_PUSH_THREAT = S(23, 2);

const Score HANGING_THREAT = S(8, 12);

const Score SPACE[15] = {
 S( 857, -21), S( 162, -14), S(  65,  -7), S(  22,  -2), S(   3,   1),
 S(  -8,   4), S( -12,   5), S( -13,   7), S( -12,   9), S(  -9,  12),
 S(  -5,  13), S(  -2,  19), S(   2,  20), S(   6,   9), S(  13,-264),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-26, 1), S(53, 71), S(-27, 70), S(-21, 20), S(2, 0), S(34, -22), S(30, -47), S(0, 0),},
 { S(-51, 0), S(19, 65), S(-36, 54), S(-39, 28), S(-32, 11), S(23, -14), S(29, -25), S(0, 0),},
 { S(-29, -6), S(41, 65), S(-60, 49), S(-19, 17), S(-19, 4), S(-9, 1), S(30, -11), S(0, 0),},
 { S(-31, 11), S(30, 66), S(-76, 44), S(-23, 28), S(-22, 21), S(-13, 13), S(-15, 14), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-20, -15), S(-30, -2), S(-28, -8), S(-38, 5), S(-65, 39), S(6, 175), S(314, 211), S(0, 0),},
 { S(-22, 1), S(0, -5), S(2, -4), S(-17, 10), S(-33, 30), S(-113, 181), S(115, 257), S(0, 0),},
 { S(37, -17), S(34, -18), S(22, -8), S(5, 1), S(-18, 24), S(-110, 151), S(-137, 246), S(0, 0),},
 { S(-5, -6), S(2, -14), S(16, -11), S(4, -13), S(-23, 12), S(-55, 126), S(-123, 238), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-43, -4), S(51, -72), S(20, -44), S(16, -37), S(1, -28), S(2, -72), S(0, 0), S(0, 0),
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
  // this idea exists in SF classical in multiple stages
  Score mat = NonPawnMaterialValue(board);
  if (600 + mat / 25 > abs(scoreMG(s) + scoreEG(s) / 2)) {
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

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

const Score BISHOP_PAIR = S(29, 100);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  44, 280), S(  37, 305), S(  18, 271), S(  68, 229),
 S( -18, 155), S( -38, 167), S(  10, 105), S(  20,  68),
 S( -34, 121), S( -34,  95), S( -26,  75), S( -24,  62),
 S( -41,  92), S( -44,  86), S( -30,  70), S( -18,  68),
 S( -44,  82), S( -46,  78), S( -37,  77), S( -29,  82),
 S( -35,  90), S( -29,  87), S( -29,  86), S( -21,  90),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 116, 232), S(   7, 221), S( -42, 249), S(  34, 227),
 S( -15, 103), S(   7,  89), S(  37,  69), S(  27,  58),
 S( -49,  81), S( -25,  65), S( -37,  65), S(   0,  52),
 S( -65,  73), S( -40,  68), S( -46,  69), S(  -8,  60),
 S( -75,  70), S( -37,  62), S( -47,  78), S( -19,  78),
 S( -66,  83), S( -16,  76), S( -37,  87), S( -16,  98),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-118, 118), S(-105, 142), S( -56, 156), S( -32, 148),
 S(  -1, 144), S(  19, 159), S(  64, 144), S(  70, 153),
 S(  60, 127), S(  72, 141), S(  69, 162), S(  61, 171),
 S(  52, 160), S(  62, 163), S(  75, 168), S(  67, 185),
 S(  59, 158), S(  63, 156), S(  80, 174), S(  75, 182),
 S(  30, 147), S(  39, 150), S(  54, 161), S(  56, 184),
 S(  21, 155), S(  35, 149), S(  41, 161), S(  50, 162),
 S( -15, 167), S(  37, 148), S(  32, 153), S(  42, 170),
},{
 S(-147, -49), S(-183, 120), S(-132, 130), S(  -5, 145),
 S(  15,  87), S(  77, 123), S(  77, 138), S(  72, 149),
 S(  69, 109), S(  58, 123), S( 136, 117), S( 111, 147),
 S(  58, 152), S(  66, 161), S(  66, 180), S(  66, 181),
 S(  65, 165), S(  41, 167), S(  83, 174), S(  61, 197),
 S(  48, 151), S(  68, 145), S(  61, 158), S(  72, 180),
 S(  60, 155), S(  65, 146), S(  53, 155), S(  52, 160),
 S(  35, 159), S(  33, 154), S(  53, 156), S(  55, 163),
}};

const Score BISHOP_PSQT[2][32] = {{
 S(  -3, 159), S(   7, 187), S( -32, 180), S( -46, 192),
 S(  16, 178), S(   7, 158), S(  52, 168), S(  45, 179),
 S(  54, 172), S(  54, 177), S(  27, 154), S(  72, 170),
 S(  46, 179), S(  82, 171), S(  66, 179), S(  66, 202),
 S(  70, 163), S(  60, 179), S(  72, 189), S(  83, 182),
 S(  61, 165), S(  81, 185), S(  59, 167), S(  65, 189),
 S(  69, 175), S(  63, 137), S(  75, 163), S(  56, 180),
 S(  48, 139), S(  76, 166), S(  59, 181), S(  60, 177),
},{
 S( -27, 122), S(  51, 159), S( -39, 170), S( -68, 172),
 S(  14, 136), S( -25, 158), S(  26, 185), S(  62, 157),
 S(  83, 164), S(  55, 170), S(  70, 161), S(  69, 177),
 S(  34, 172), S(  78, 171), S(  75, 182), S(  77, 181),
 S(  94, 139), S(  76, 170), S(  81, 183), S(  73, 185),
 S(  71, 160), S(  88, 167), S(  68, 164), S(  68, 196),
 S(  77, 143), S(  76, 143), S(  77, 172), S(  68, 179),
 S(  69, 123), S(  63, 192), S(  55, 186), S(  70, 171),
}};

const Score ROOK_PSQT[2][32] = {{
 S(  -8, 345), S(  -4, 353), S(  -2, 360), S( -25, 364),
 S(  11, 328), S(   0, 345), S(  20, 344), S(  44, 325),
 S(  -3, 332), S(  35, 330), S(  20, 335), S(  24, 329),
 S(   7, 342), S(  25, 335), S(  41, 337), S(  45, 322),
 S(   1, 337), S(  13, 338), S(  18, 339), S(  29, 327),
 S(  -2, 329), S(  11, 319), S(  17, 324), S(  18, 322),
 S(   0, 322), S(  10, 321), S(  26, 320), S(  30, 314),
 S(  13, 323), S(  12, 319), S(  16, 324), S(  27, 309),
},{
 S(  94, 334), S(  47, 353), S( -28, 368), S(  -4, 355),
 S(  54, 320), S(  71, 335), S(  19, 338), S(  17, 331),
 S(   4, 325), S(  75, 316), S(  44, 304), S(  57, 305),
 S(  15, 325), S(  59, 313), S(  43, 308), S(  51, 311),
 S(  -3, 319), S(  28, 316), S(  15, 314), S(  36, 315),
 S(   7, 297), S(  34, 297), S(  24, 300), S(  32, 309),
 S(   2, 306), S(  47, 284), S(  29, 294), S(  34, 302),
 S(  27, 291), S(  23, 306), S(  23, 302), S(  34, 299),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(  13, 617), S(  82, 552), S( 145, 553), S( 133, 588),
 S(  52, 576), S(  49, 578), S(  63, 620), S(  55, 637),
 S(  85, 546), S(  96, 539), S(  89, 572), S(  90, 598),
 S(  89, 586), S( 100, 603), S( 101, 611), S(  81, 649),
 S( 109, 553), S(  89, 620), S(  96, 620), S(  77, 653),
 S(  93, 554), S( 106, 569), S(  95, 596), S(  91, 600),
 S( 103, 516), S( 104, 527), S( 112, 534), S( 111, 551),
 S(  92, 529), S(  90, 523), S(  93, 536), S( 103, 534),
},{
 S( 204, 531), S( 305, 481), S( 105, 578), S( 146, 561),
 S( 153, 552), S( 112, 608), S(  87, 632), S(  39, 644),
 S( 111, 542), S( 104, 557), S( 112, 568), S( 103, 609),
 S(  90, 582), S( 103, 605), S( 101, 560), S(  80, 644),
 S( 121, 545), S( 125, 578), S( 109, 577), S(  87, 642),
 S( 110, 522), S( 124, 542), S( 109, 571), S(  96, 599),
 S( 137, 459), S( 132, 472), S( 126, 491), S( 114, 547),
 S( 126, 469), S(  95, 498), S(  93, 490), S( 104, 514),
}};

const Score KING_PSQT[2][32] = {{
 S( 196,-115), S( 151, -45), S(  36,  -6), S(  75, -14),
 S( -31,  48), S(  23,  61), S(  27,  62), S(  20,  36),
 S(  35,  32), S(  82,  54), S(  50,  63), S( -50,  78),
 S(  30,  32), S(  52,  51), S( -23,  68), S( -89,  73),
 S( -32,  20), S(   9,  36), S(  -2,  42), S(-105,  57),
 S( -11,  -4), S(   6,  16), S( -21,  22), S( -56,  28),
 S(  14, -23), S( -17,  14), S( -37,   7), S( -64,   9),
 S(  29, -88), S(  30, -42), S(   3, -33), S( -29, -41),
},{
 S( 111,-155), S(-179,  46), S( 143, -28), S( -48, -12),
 S(-149,   8), S(  31,  67), S( -52,  74), S( 146,  31),
 S( -16,  13), S( 174,  62), S( 222,  62), S(  66,  65),
 S( -97,  11), S(  31,  50), S( -23,  74), S(-104,  79),
 S( -79,  -3), S( -59,  35), S( -34,  43), S( -93,  59),
 S(  -2, -14), S(  17,  13), S( -13,  20), S( -37,  27),
 S(   9, -12), S(   2,  14), S( -35,  12), S( -69,  11),
 S(  15, -66), S(  16, -19), S( -12, -23), S( -14, -47),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -52,  47), S(   6,  47), S(  27,  43), S(  58,  47),
 S(  21,  19), S(  42,  31), S(  36,  42), S(  46,  47),
 S(  20,   9), S(  38,  24), S(  19,  33), S(  24,  36),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -11,  40), S(  32,  20), S(  55,  35), S(  61,  10),
 S(   1,  19), S(  20,  26), S(  39,  14), S(  50,  18),
 S( -16,  44), S(  47,  17), S(  33,  18), S(  40,  34),
};

const Score KNIGHT_MOBILITIES[9] = {
 S( -13,  15), S(  24, 109), S(  44, 156), S(  57, 172),
 S(  68, 185), S(  77, 199), S(  87, 200), S(  96, 202),
 S( 106, 191),};

const Score BISHOP_MOBILITIES[14] = {
 S(  36,  96), S(  56, 135), S(  71, 162), S(  79, 189),
 S(  87, 200), S(  92, 212), S(  95, 220), S(  98, 225),
 S(  99, 229), S( 102, 233), S( 111, 228), S( 129, 225),
 S( 134, 230), S( 132, 226),};

const Score ROOK_MOBILITIES[15] = {
 S( -19,  42), S(  -2, 232), S(  14, 267), S(  24, 272),
 S(  22, 300), S(  24, 313), S(  18, 325), S(  23, 327),
 S(  30, 331), S(  36, 337), S(  40, 342), S(  38, 348),
 S(  41, 354), S(  55, 357), S(  73, 352),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2080,-2004), S(  80,-162), S( 130, 274), S( 117, 526),
 S( 123, 607), S( 127, 627), S( 125, 668), S( 126, 698),
 S( 128, 721), S( 130, 728), S( 132, 739), S( 135, 741),
 S( 135, 746), S( 138, 746), S( 139, 747), S( 146, 734),
 S( 146, 729), S( 147, 724), S( 153, 704), S( 180, 667),
 S( 200, 625), S( 216, 584), S( 258, 541), S( 306, 454),
 S( 300, 447), S( 485, 268), S( 427, 244), S( 445, 158),
};

const Score KNIGHT_OUTPOST_REACHABLE = S(13, 24);

const Score BISHOP_OUTPOST_REACHABLE = S(8, 10);

const Score BISHOP_TRAPPED = S(-143, -234);

const Score ROOK_TRAPPED = S(-64, -26);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(29, 21);

const Score ROOK_OPEN_FILE = S(30, 19);

const Score ROOK_SEMI_OPEN = S(7, 16);

const Score DEFENDED_PAWN = S(14, 11);

const Score DOUBLED_PAWN = S(11, -34);

const Score OPPOSED_ISOLATED_PAWN = S(-4, -11);

const Score OPEN_ISOLATED_PAWN = S(-10, -21);

const Score BACKWARDS_PAWN = S(-11, -19);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  74,  64), S(  30,  44), S(  14,  14),
 S(   8,   3), S(   7,   1), S(   2,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -45, 318), S(  12,  76),
 S(  -9,  46), S( -25,  20), S( -35,   4), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  34, 297), S( -21, 313), S( -15, 165),
 S( -30,  87), S( -11,  55), S(  -9,  48), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -15);

const Score PASSED_PAWN_KING_PROXIMITY = S(-10, 31);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(31, 37);

const Score KNIGHT_THREATS[6] = { S(0, 21), S(0, -1), S(43, 43), S(100, 4), S(55, -65), S(292, 16),};

const Score BISHOP_THREATS[6] = { S(0, 21), S(34, 44), S(11, 38), S(67, 12), S(69, 67), S(136, 121),};

const Score ROOK_THREATS[6] = { S(3, 33), S(38, 48), S(41, 58), S(9, 18), S(89, 3), S(294, -11),};

const Score KING_THREAT = S(-24, 37);

const Score PAWN_THREAT = S(76, 29);

const Score PAWN_PUSH_THREAT = S(24, 2);

const Score HANGING_THREAT = S(9, 12);

const Score SPACE[15] = {
 S( 939, -23), S( 168, -15), S(  68,  -7), S(  22,  -2), S(   3,   1),
 S(  -8,   3), S( -12,   5), S( -13,   7), S( -12,   9), S(  -9,  12),
 S(  -5,  14), S(  -2,  20), S(   2,  21), S(   7,  10), S(  14,-257),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-26, 0), S(45, 76), S(-26, 72), S(-20, 20), S(4, -1), S(36, -23), S(32, -49), S(0, 0),},
 { S(-54, 0), S(15, 67), S(-37, 57), S(-41, 29), S(-33, 11), S(23, -14), S(30, -26), S(0, 0),},
 { S(-30, -7), S(31, 68), S(-62, 51), S(-20, 18), S(-20, 4), S(-9, 1), S(31, -11), S(0, 0),},
 { S(-32, 11), S(22, 72), S(-79, 46), S(-24, 29), S(-23, 22), S(-13, 14), S(-15, 15), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-21, -15), S(-31, -2), S(-29, -9), S(-40, 5), S(-68, 41), S(6, 182), S(308, 231), S(0, 0),},
 { S(-22, 1), S(0, -6), S(2, -4), S(-18, 10), S(-34, 32), S(-116, 189), S(109, 279), S(0, 0),},
 { S(38, -18), S(35, -20), S(23, -9), S(4, 1), S(-19, 26), S(-113, 157), S(-155, 269), S(0, 0),},
 { S(-5, -6), S(2, -14), S(16, -12), S(4, -14), S(-23, 13), S(-57, 132), S(-120, 257), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-38, -4), S(52, -75), S(21, -46), S(16, -38), S(1, -29), S(1, -74), S(0, 0), S(0, 0),
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

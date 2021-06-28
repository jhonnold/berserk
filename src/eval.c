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

const Score BISHOP_PAIR = S(32, 105);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  48, 302), S(  41, 328), S(  23, 290), S(  76, 246),
 S( -15, 171), S( -34, 183), S(  16, 117), S(  27,  78),
 S( -31, 136), S( -28, 108), S( -20,  86), S( -20,  72),
 S( -38, 105), S( -40,  98), S( -27,  81), S( -11,  78),
 S( -43,  95), S( -45,  90), S( -36,  89), S( -27,  94),
 S( -33, 102), S( -27,  99), S( -28,  99), S( -20, 103),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 121, 253), S(   3, 241), S( -58, 271), S(  36, 246),
 S( -11, 116), S(  11, 102), S(  44,  78), S(  35,  65),
 S( -46,  93), S( -20,  77), S( -33,  75), S(   6,  61),
 S( -63,  84), S( -36,  79), S( -45,  80), S(  -1,  69),
 S( -75,  81), S( -34,  73), S( -44,  90), S( -17,  90),
 S( -65,  95), S( -14,  88), S( -38, 100), S( -15, 111),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-121, 130), S( -99, 156), S( -51, 174), S( -27, 165),
 S(   5, 159), S(  25, 177), S(  74, 164), S(  85, 172),
 S(  72, 142), S(  82, 162), S(  80, 186), S(  72, 194),
 S(  62, 178), S(  72, 187), S(  86, 192), S(  76, 210),
 S(  65, 181), S(  71, 181), S(  88, 202), S(  85, 206),
 S(  37, 161), S(  46, 169), S(  62, 182), S(  61, 204),
 S(  25, 166), S(  41, 160), S(  48, 177), S(  59, 181),
 S( -11, 178), S(  43, 160), S(  39, 169), S(  52, 188),
},{
 S(-149, -46), S(-179, 134), S(-130, 146), S(   2, 162),
 S(  27,  98), S(  86, 139), S(  93, 156), S(  86, 168),
 S(  83, 123), S(  70, 142), S( 153, 137), S( 126, 168),
 S(  71, 169), S(  77, 185), S(  82, 206), S(  75, 206),
 S(  74, 190), S(  53, 195), S(  94, 204), S(  71, 224),
 S(  58, 165), S(  79, 165), S(  70, 181), S(  79, 200),
 S(  66, 164), S(  71, 157), S(  61, 172), S(  60, 178),
 S(  41, 170), S(  40, 164), S(  59, 170), S(  65, 182),
}};

const Score BISHOP_PSQT[2][32] = {{
 S(   0, 182), S(  11, 210), S( -26, 203), S( -52, 219),
 S(  22, 202), S(  15, 180), S(  62, 191), S(  53, 202),
 S(  64, 195), S(  66, 200), S(  36, 177), S(  82, 193),
 S(  54, 202), S(  93, 195), S(  76, 203), S(  76, 228),
 S(  78, 186), S(  65, 201), S(  77, 215), S(  92, 207),
 S(  69, 184), S(  90, 205), S(  66, 187), S(  73, 209),
 S(  74, 188), S(  71, 149), S(  84, 179), S(  64, 200),
 S(  56, 156), S(  84, 184), S(  67, 200), S(  70, 199),
},{
 S( -22, 141), S(  63, 180), S( -38, 193), S( -61, 194),
 S(  25, 154), S( -16, 179), S(  35, 209), S(  75, 178),
 S(  96, 186), S(  68, 193), S(  79, 184), S(  81, 201),
 S(  43, 195), S(  89, 195), S(  89, 207), S(  87, 207),
 S( 103, 162), S(  82, 193), S(  86, 210), S(  83, 210),
 S(  82, 178), S(  98, 187), S(  76, 183), S(  76, 216),
 S(  83, 152), S(  84, 152), S(  86, 188), S(  76, 197),
 S(  76, 137), S(  70, 211), S(  61, 203), S(  80, 193),
}};

const Score ROOK_PSQT[2][32] = {{
 S(   5, 388), S(  11, 396), S(  13, 403), S( -12, 407),
 S(  26, 369), S(  13, 387), S(  36, 386), S(  61, 366),
 S(  12, 373), S(  50, 371), S(  35, 376), S(  39, 369),
 S(  19, 384), S(  40, 378), S(  55, 380), S(  59, 364),
 S(  12, 379), S(  25, 383), S(  28, 384), S(  43, 370),
 S(  11, 370), S(  22, 360), S(  29, 366), S(  32, 363),
 S(  13, 363), S(  23, 362), S(  40, 361), S(  43, 355),
 S(  27, 362), S(  26, 359), S(  30, 365), S(  42, 348),
},{
 S( 119, 374), S(  66, 395), S( -10, 410), S(  17, 396),
 S(  73, 361), S(  93, 376), S(  40, 377), S(  33, 372),
 S(  23, 365), S(  96, 355), S(  66, 343), S(  74, 344),
 S(  33, 366), S(  79, 356), S(  60, 351), S(  67, 354),
 S(   8, 363), S(  44, 363), S(  29, 360), S(  52, 359),
 S(  22, 335), S(  50, 338), S(  39, 341), S(  46, 350),
 S(  17, 346), S(  64, 323), S(  45, 333), S(  49, 342),
 S(  44, 328), S(  39, 345), S(  39, 341), S(  50, 338),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(  23, 674), S(  96, 607), S( 164, 609), S( 150, 649),
 S(  63, 629), S(  59, 637), S(  75, 681), S(  64, 702),
 S(  98, 599), S( 108, 592), S( 103, 629), S( 103, 657),
 S( 100, 639), S( 108, 660), S( 111, 672), S(  91, 712),
 S( 114, 614), S(  96, 680), S( 102, 684), S(  87, 716),
 S( 103, 609), S( 116, 626), S( 106, 653), S( 103, 657),
 S( 115, 569), S( 117, 579), S( 125, 586), S( 124, 605),
 S( 104, 582), S( 102, 576), S( 106, 590), S( 117, 586),
},{
 S( 226, 588), S( 328, 539), S( 133, 628), S( 169, 617),
 S( 176, 608), S( 128, 674), S( 109, 691), S(  50, 708),
 S( 128, 602), S( 123, 615), S( 129, 633), S( 118, 673),
 S( 103, 645), S( 114, 671), S( 115, 623), S(  90, 709),
 S( 126, 609), S( 132, 638), S( 115, 643), S(  98, 707),
 S( 122, 578), S( 136, 599), S( 121, 629), S( 109, 656),
 S( 151, 508), S( 147, 522), S( 141, 541), S( 127, 602),
 S( 140, 519), S( 108, 551), S( 105, 542), S( 118, 565),
}};

const Score KING_PSQT[2][32] = {{
 S( 213,-124), S( 161, -48), S(  30,  -5), S(  61, -10),
 S( -35,  52), S(  26,  64), S(  20,  68), S(  18,  41),
 S(  30,  35), S(  86,  58), S(  43,  69), S( -62,  86),
 S(  24,  35), S(  45,  55), S( -37,  74), S(-102,  80),
 S( -39,  22), S(   1,  41), S( -15,  48), S(-121,  65),
 S( -10,  -5), S(   1,  18), S( -28,  25), S( -64,  32),
 S(  18, -25), S( -17,  15), S( -40,   7), S( -67,  11),
 S(  34, -94), S(  34, -45), S(   4, -36), S( -29, -44),
},{
 S( 137,-169), S(-183,  49), S( 144, -27), S( -49, -11),
 S(-158,   8), S(  33,  72), S( -59,  80), S( 149,  35),
 S( -16,  13), S( 179,  67), S( 221,  68), S(  63,  72),
 S(-106,  12), S(  30,  55), S( -33,  81), S(-115,  86),
 S( -89,  -1), S( -71,  40), S( -46,  49), S(-106,  66),
 S(  -5, -15), S(  14,  15), S( -20,  23), S( -45,  32),
 S(  10, -13), S(   1,  15), S( -40,  12), S( -73,  13),
 S(  17, -71), S(  17, -21), S( -14, -25), S( -14, -50),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -59,  32), S(   4,  36), S(  27,  34), S(  61,  48),
 S(  18,   7), S(  46,  19), S(  38,  33), S(  51,  42),
 S(  24,  -6), S(  43,   9), S(  23,  20), S(  26,  26),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -15,  22), S(  28,   8), S(  55,  25), S(  65,   7),
 S(  -1,   1), S(  21,  17), S(  41,   5), S(  55,  12),
 S( -15,  28), S(  55,   9), S(  41,   6), S(  44,  26),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(  -8,  39), S(  32, 136), S(  53, 185), S(  67, 201),
 S(  78, 213), S(  88, 225), S(  98, 226), S( 109, 226),
 S( 119, 213),};

const Score BISHOP_MOBILITIES[14] = {
 S(  43, 113), S(  65, 157), S(  81, 186), S(  89, 214),
 S(  97, 226), S( 103, 239), S( 105, 247), S( 108, 253),
 S( 109, 257), S( 111, 261), S( 121, 255), S( 140, 251),
 S( 145, 257), S( 147, 250),};

const Score ROOK_MOBILITIES[15] = {
 S( -10,  61), S(   8, 260), S(  26, 305), S(  36, 312),
 S(  34, 341), S(  36, 356), S(  30, 368), S(  35, 370),
 S(  42, 374), S(  48, 381), S(  52, 386), S(  51, 392),
 S(  54, 399), S(  69, 402), S(  87, 397),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2088,-2006), S(  93, -93), S( 152, 375), S( 137, 635),
 S( 144, 725), S( 148, 745), S( 146, 787), S( 147, 820),
 S( 148, 843), S( 151, 850), S( 153, 860), S( 156, 864),
 S( 155, 869), S( 159, 869), S( 159, 870), S( 166, 857),
 S( 166, 852), S( 168, 846), S( 171, 827), S( 201, 788),
 S( 221, 746), S( 239, 704), S( 281, 661), S( 330, 569),
 S( 326, 564), S( 522, 374), S( 451, 355), S( 475, 266),
};

const Score MINOR_BEHIND_PAWN = S(6, 22);

const Score KNIGHT_OUTPOST_REACHABLE = S(14, 25);

const Score BISHOP_OUTPOST_REACHABLE = S(8, 9);

const Score BISHOP_TRAPPED = S(-154, -248);

const Score ROOK_TRAPPED = S(-67, -28);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(31, 22);

const Score ROOK_OPEN_FILE = S(32, 20);

const Score ROOK_SEMI_OPEN = S(7, 17);

const Score DEFENDED_PAWN = S(15, 12);

const Score DOUBLED_PAWN = S(11, -36);

const Score OPPOSED_ISOLATED_PAWN = S(-5, -11);

const Score OPEN_ISOLATED_PAWN = S(-11, -22);

const Score BACKWARDS_PAWN = S(-12, -20);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  76,  68), S(  31,  47), S(  14,  14),
 S(   8,   3), S(   6,   1), S(   2,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -36, 335), S(  13,  76),
 S( -11,  47), S( -26,  21), S( -38,   3), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  34, 321), S( -18, 335), S( -14, 177),
 S( -31,  94), S( -10,  58), S(  -8,  51), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -16);

const Score PASSED_PAWN_KING_PROXIMITY = S(-11, 33);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(30, 36);

const Score KNIGHT_THREATS[6] = { S(0, 26), S(0, 3), S(44, 45), S(106, 3), S(58, -70), S(311, 16),};

const Score BISHOP_THREATS[6] = { S(0, 23), S(35, 49), S(11, 45), S(71, 13), S(72, 70), S(145, 131),};

const Score ROOK_THREATS[6] = { S(3, 35), S(40, 50), S(42, 60), S(10, 19), S(94, 1), S(313, -12),};

const Score KING_THREAT = S(-26, 39);

const Score PAWN_THREAT = S(80, 33);

const Score PAWN_PUSH_THREAT = S(24, 22);

const Score HANGING_THREAT = S(9, 12);

const Score SPACE[15] = {
 S(1008, -25), S( 178, -16), S(  71,  -7), S(  22,  -2), S(   2,   2),
 S(  -8,   4), S( -13,   6), S( -14,   8), S( -13,  10), S( -10,  14),
 S(  -6,  16), S(  -2,  21), S(   2,  22), S(   8,   9), S(  15,-258),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-30, 1), S(40, 82), S(-31, 78), S(-22, 22), S(4, 0), S(38, -24), S(34, -52), S(0, 0),},
 { S(-57, 2), S(7, 72), S(-38, 62), S(-41, 32), S(-34, 14), S(25, -14), S(34, -26), S(0, 0),},
 { S(-31, -7), S(26, 75), S(-67, 54), S(-20, 18), S(-22, 5), S(-10, 0), S(35, -12), S(0, 0),},
 { S(-35, 12), S(-2, 82), S(-85, 50), S(-25, 32), S(-24, 24), S(-14, 15), S(-15, 16), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-21, -16), S(-32, -3), S(-30, -10), S(-41, 5), S(-72, 44), S(2, 196), S(322, 251), S(0, 0),},
 { S(-23, 0), S(1, -6), S(4, -5), S(-18, 11), S(-35, 33), S(-123, 202), S(106, 303), S(0, 0),},
 { S(41, -19), S(37, -20), S(26, -9), S(4, 2), S(-23, 28), S(-128, 170), S(-179, 294), S(0, 0),},
 { S(-5, -7), S(2, -15), S(18, -13), S(5, -15), S(-27, 14), S(-65, 140), S(-135, 280), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-29, -6), S(57, -80), S(22, -49), S(18, -41), S(2, -31), S(1, -79), S(0, 0), S(0, 0),
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
  pawnPushes &= (data->allAttacks[side] | ~data->allAttacks[xside]);
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

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

const Score BISHOP_PAIR = S(31, 106);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  49, 313), S(  43, 340), S(  24, 299), S(  79, 254),
 S( -14, 179), S( -32, 191), S(  19, 122), S(  32,  81),
 S( -30, 143), S( -26, 114), S( -18,  91), S( -18,  76),
 S( -37, 110), S( -38, 104), S( -25,  85), S(  -9,  82),
 S( -42, 101), S( -43,  96), S( -33,  93), S( -25,  99),
 S( -32, 108), S( -25, 105), S( -27, 104), S( -18, 109),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 123, 262), S(   5, 250), S( -64, 280), S(  39, 253),
 S( -10, 122), S(  15, 107), S(  47,  83), S(  40,  68),
 S( -46,  99), S( -18,  82), S( -32,  80), S(   9,  65),
 S( -63,  89), S( -34,  84), S( -43,  84), S(   1,  73),
 S( -75,  86), S( -32,  78), S( -43,  95), S( -15,  95),
 S( -64, 100), S( -12,  94), S( -36, 106), S( -13, 117),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-120, 136), S(-103, 165), S( -53, 184), S( -25, 174),
 S(  -1, 171), S(  15, 190), S(  57, 178), S(  56, 190),
 S(  66, 152), S(  70, 175), S(  69, 199), S(  56, 209),
 S(  67, 186), S(  76, 196), S(  91, 200), S(  80, 220),
 S(  69, 190), S(  77, 189), S(  93, 211), S(  92, 216),
 S(  45, 168), S(  52, 177), S(  68, 192), S(  66, 213),
 S(  29, 175), S(  46, 168), S(  51, 186), S(  64, 189),
 S(  -9, 187), S(  48, 168), S(  43, 178), S(  56, 197),
},{
 S(-148, -46), S(-150, 136), S(-137, 154), S(   3, 172),
 S(  -4, 112), S(  46, 159), S(  74, 171), S(  51, 188),
 S(  66, 136), S(  51, 155), S( 128, 153), S(  97, 187),
 S(  75, 178), S(  82, 194), S(  89, 215), S(  82, 215),
 S(  77, 201), S(  59, 204), S(  99, 214), S(  77, 234),
 S(  65, 173), S(  86, 173), S(  78, 190), S(  85, 210),
 S(  72, 173), S(  76, 166), S(  66, 180), S(  65, 186),
 S(  46, 178), S(  45, 171), S(  65, 178), S(  69, 191),
}};

const Score BISHOP_PSQT[2][32] = {{
 S(   7, 191), S(  10, 221), S( -32, 214), S( -57, 230),
 S(  25, 212), S(  11, 192), S(  60, 202), S(  48, 214),
 S(  63, 206), S(  66, 210), S(  29, 189), S(  74, 205),
 S(  53, 213), S(  85, 207), S(  70, 215), S(  69, 239),
 S(  76, 197), S(  65, 211), S(  81, 224), S(  87, 218),
 S(  72, 193), S(  95, 215), S(  72, 196), S(  77, 219),
 S(  79, 196), S(  77, 157), S(  88, 188), S(  71, 207),
 S(  62, 164), S(  89, 193), S(  74, 209), S(  75, 209),
},{
 S( -18, 150), S(  65, 190), S( -53, 206), S( -70, 206),
 S(  16, 168), S( -15, 188), S(  26, 222), S(  56, 193),
 S(  95, 196), S(  64, 204), S(  77, 194), S(  72, 213),
 S(  44, 203), S(  79, 209), S(  85, 218), S(  79, 218),
 S(  97, 176), S(  85, 203), S(  91, 219), S(  80, 221),
 S(  84, 188), S( 105, 196), S(  83, 191), S(  82, 226),
 S(  88, 160), S(  91, 159), S(  91, 198), S(  83, 205),
 S(  82, 146), S(  75, 221), S(  66, 213), S(  86, 202),
}};

const Score ROOK_PSQT[2][32] = {{
 S(  11, 407), S(  18, 414), S(  20, 422), S(  -7, 427),
 S(  31, 388), S(  19, 405), S(  42, 405), S(  67, 385),
 S(  16, 392), S(  57, 389), S(  42, 394), S(  47, 387),
 S(  25, 402), S(  48, 396), S(  65, 398), S(  68, 382),
 S(  19, 398), S(  31, 402), S(  36, 402), S(  51, 388),
 S(  17, 388), S(  30, 378), S(  38, 384), S(  40, 382),
 S(  18, 381), S(  29, 380), S(  47, 379), S(  51, 372),
 S(  34, 380), S(  33, 377), S(  37, 383), S(  50, 365),
},{
 S( 128, 392), S(  75, 414), S(  -5, 430), S(  24, 415),
 S(  82, 379), S( 101, 394), S(  46, 397), S(  38, 391),
 S(  31, 383), S( 108, 373), S(  74, 360), S(  81, 362),
 S(  40, 384), S(  88, 374), S(  69, 369), S(  76, 371),
 S(  15, 381), S(  54, 381), S(  38, 377), S(  60, 377),
 S(  30, 352), S(  60, 355), S(  49, 357), S(  56, 367),
 S(  24, 363), S(  72, 339), S(  53, 350), S(  57, 359),
 S(  52, 345), S(  46, 362), S(  47, 358), S(  57, 355),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(  29, 703), S( 100, 639), S( 173, 639), S( 156, 681),
 S(  69, 658), S(  66, 664), S(  82, 712), S(  69, 734),
 S( 105, 625), S( 113, 622), S( 107, 662), S( 110, 690),
 S( 108, 666), S( 114, 690), S( 118, 702), S(  97, 744),
 S( 120, 643), S( 103, 710), S( 110, 715), S(  94, 747),
 S( 111, 637), S( 123, 655), S( 115, 681), S( 111, 686),
 S( 121, 599), S( 123, 608), S( 132, 614), S( 132, 633),
 S( 109, 610), S( 107, 605), S( 113, 617), S( 125, 614),
},{
 S( 235, 620), S( 339, 568), S( 136, 662), S( 176, 648),
 S( 185, 641), S( 138, 706), S( 120, 722), S(  57, 740),
 S( 138, 632), S( 133, 646), S( 139, 665), S( 127, 704),
 S( 110, 678), S( 122, 703), S( 124, 655), S(  97, 742),
 S( 134, 640), S( 140, 669), S( 124, 672), S( 105, 739),
 S( 131, 606), S( 145, 627), S( 132, 656), S( 118, 685),
 S( 157, 538), S( 156, 548), S( 148, 569), S( 135, 629),
 S( 149, 544), S( 114, 580), S( 112, 570), S( 125, 594),
}};

const Score KING_PSQT[2][32] = {{
 S( 220,-129), S( 160, -50), S(  30,  -6), S(  56, -10),
 S( -38,  53), S(  28,  65), S(  10,  72), S(  17,  41),
 S(  28,  36), S(  89,  59), S(  40,  72), S( -65,  87),
 S(  22,  36), S(  46,  56), S( -42,  76), S(-106,  82),
 S( -45,  23), S(  -1,  41), S( -19,  50), S(-128,  66),
 S( -11,  -6), S(  -1,  18), S( -30,  25), S( -69,  32),
 S(  17, -26), S( -19,  14), S( -42,   6), S( -70,  10),
 S(  32, -97), S(  34, -48), S(   5, -38), S( -30, -47),
},{
 S( 143,-172), S(-187,  52), S( 143, -25), S( -51, -10),
 S(-178,  12), S(  34,  75), S( -63,  84), S( 151,  37),
 S( -19,  15), S( 181,  71), S( 221,  72), S(  62,  75),
 S(-110,  13), S(  29,  58), S( -36,  85), S(-122,  90),
 S( -91,   0), S( -73,  42), S( -48,  52), S(-110,  69),
 S(  -6, -14), S(  14,  16), S( -20,  25), S( -47,  33),
 S(   9, -13), S(   1,  16), S( -41,  14), S( -75,  13),
 S(  16, -72), S(  18, -21), S( -13, -25), S( -13, -52),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -46,  29), S(  13,  37), S(  37,  34), S(  75,  49),
 S(  19,   7), S(  50,  21), S(  39,  37), S(  55,  45),
 S(  26,  -6), S(  45,  11), S(  25,  21), S(  27,  27),
};

const Score BISHOP_POST_PSQT[12] = {
 S( -13,  23), S(  33,   9), S(  62,  26), S(  75,   9),
 S(   2,   2), S(  29,  17), S(  49,   6), S(  63,  13),
 S( -12,  29), S(  58,  11), S(  41,   9), S(  48,  26),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(  -5,  45), S(  35, 147), S(  57, 197), S(  71, 213),
 S(  84, 226), S(  94, 238), S( 105, 239), S( 116, 239),
 S( 126, 226),};

const Score BISHOP_MOBILITIES[14] = {
 S(  48, 122), S(  70, 167), S(  86, 197), S(  94, 226),
 S( 103, 238), S( 109, 251), S( 111, 261), S( 116, 266),
 S( 117, 271), S( 120, 275), S( 131, 268), S( 150, 265),
 S( 157, 271), S( 160, 264),};

const Score ROOK_MOBILITIES[15] = {
 S( -14,  80), S(  12, 276), S(  31, 323), S(  41, 331),
 S(  40, 361), S(  41, 376), S(  35, 389), S(  41, 390),
 S(  48, 395), S(  54, 401), S(  58, 407), S(  58, 413),
 S(  62, 420), S(  76, 424), S(  94, 418),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2095,-2007), S(  81,  48), S( 165, 415), S( 150, 678),
 S( 157, 771), S( 162, 792), S( 160, 832), S( 162, 867),
 S( 163, 892), S( 166, 898), S( 168, 909), S( 170, 913),
 S( 170, 918), S( 174, 918), S( 174, 920), S( 181, 906),
 S( 181, 902), S( 182, 897), S( 187, 876), S( 215, 839),
 S( 235, 797), S( 253, 753), S( 297, 710), S( 348, 616),
 S( 346, 610), S( 540, 421), S( 427, 421), S( 496, 311),
};

const Score MINOR_BEHIND_PAWN = S(6, 22);

const Score KNIGHT_OUTPOST_REACHABLE = S(14, 25);

const Score BISHOP_OUTPOST_REACHABLE = S(9, 10);

const Score BISHOP_TRAPPED = S(-148, -257);

const Score ROOK_TRAPPED = S(-70, -28);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(29, 23);

const Score ROOK_OPEN_FILE = S(33, 21);

const Score ROOK_SEMI_OPEN = S(8, 18);

const Score DEFENDED_PAWN = S(15, 13);

const Score DOUBLED_PAWN = S(11, -37);

const Score OPPOSED_ISOLATED_PAWN = S(-4, -12);

const Score OPEN_ISOLATED_PAWN = S(-11, -22);

const Score BACKWARDS_PAWN = S(-12, -21);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  77,  71), S(  31,  48), S(  14,  15),
 S(   8,   3), S(   6,   1), S(   2,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -34, 349), S(  15,  77),
 S( -11,  48), S( -27,  22), S( -37,   3), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  35, 331), S( -19, 345), S( -13, 181),
 S( -31,  97), S( -10,  60), S(  -8,  52), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -16);

const Score PASSED_PAWN_KING_PROXIMITY = S(-12, 34);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(31, 37);

const Score KNIGHT_THREATS[6] = { S(0, 27), S(-6, 53), S(38, 46), S(100, 13), S(60, -53), S(303, 23),};

const Score BISHOP_THREATS[6] = { S(3, 24), S(28, 43), S(-47, 69), S(68, 24), S(72, 98), S(121, 151),};

const Score ROOK_THREATS[6] = { S(3, 36), S(42, 51), S(43, 61), S(9, 20), S(97, 1), S(318, -11),};

const Score KING_THREAT = S(-31, 41);

const Score PAWN_THREAT = S(81, 34);

const Score PAWN_PUSH_THREAT = S(24, 24);

const Score HANGING_THREAT = S(9, 12);

const Score SPACE[15] = {
 S(1042, -26), S( 183, -16), S(  72,  -7), S(  23,  -2), S(   2,   2),
 S(  -9,   5), S( -14,   7), S( -15,   9), S( -13,  11), S( -10,  14),
 S(  -6,  16), S(  -2,  23), S(   2,  23), S(   8,  12), S(  15,-248),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-30, 2), S(43, 86), S(-32, 81), S(-22, 23), S(4, 1), S(39, -24), S(35, -52), S(0, 0),},
 { S(-58, 2), S(10, 75), S(-40, 63), S(-43, 33), S(-35, 14), S(25, -14), S(35, -26), S(0, 0),},
 { S(-32, -7), S(26, 80), S(-68, 57), S(-21, 19), S(-22, 5), S(-10, 1), S(36, -12), S(0, 0),},
 { S(-36, 14), S(-2, 87), S(-86, 52), S(-26, 34), S(-25, 26), S(-14, 16), S(-16, 17), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-22, -17), S(-32, -3), S(-30, -11), S(-42, 5), S(-74, 45), S(3, 201), S(333, 257), S(0, 0),},
 { S(-24, 0), S(1, -7), S(3, -5), S(-19, 11), S(-37, 34), S(-124, 207), S(112, 313), S(0, 0),},
 { S(42, -20), S(38, -21), S(26, -10), S(4, 1), S(-25, 29), S(-134, 176), S(-189, 303), S(0, 0),},
 { S(-5, -8), S(1, -15), S(18, -13), S(5, -15), S(-27, 14), S(-66, 144), S(-129, 287), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-32, -8), S(58, -82), S(22, -50), S(18, -42), S(2, -32), S(1, -81), S(0, 0), S(0, 0),
};

const Score KS_ATTACKER_WEIGHTS[5] = {
 0, 32, 31, 18, 24
};

const Score KS_ATTACK = 4;

const Score KS_WEAK_SQS = 78;

const Score KS_PINNED = 74;

const Score KS_SAFE_CHECK = 248;

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

  if (T || abs(scoreMG(s) + scoreEG(s)) / 2 < 640) {
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

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

const Score BISHOP_PAIR = S(32, 109);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S(  55, 333), S(  47, 361), S(  27, 319), S(  85, 270),
 S( -10, 194), S( -28, 206), S(  26, 132), S(  40,  89),
 S( -26, 155), S( -23, 125), S( -14, 100), S( -13,  85),
 S( -34, 121), S( -35, 114), S( -20,  94), S(  -4,  91),
 S( -39, 110), S( -39, 105), S( -29, 102), S( -20, 108),
 S( -27, 118), S( -20, 115), S( -21, 113), S( -12, 117),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 125, 280), S(  13, 262), S( -52, 298), S(  48, 270),
 S(  -3, 132), S(  21, 116), S(  51,  92), S(  49,  75),
 S( -42, 109), S( -12,  90), S( -27,  88), S(  15,  73),
 S( -60,  99), S( -30,  93), S( -39,  93), S(   7,  82),
 S( -72,  95), S( -27,  86), S( -39, 104), S( -10, 103),
 S( -60, 110), S(  -6, 102), S( -31, 114), S(  -8, 126),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-113, 151), S( -87, 178), S( -40, 200), S(  -6, 185),
 S(   9, 188), S(  24, 209), S(  73, 195), S(  79, 201),
 S(  76, 170), S(  81, 193), S(  85, 217), S(  73, 224),
 S(  75, 206), S(  87, 213), S( 104, 218), S( 102, 226),
 S(  76, 211), S(  86, 207), S( 104, 230), S(  98, 232),
 S(  51, 188), S(  59, 198), S(  76, 213), S(  71, 231),
 S(  34, 194), S(  54, 186), S(  59, 204), S(  69, 206),
 S(  -5, 206), S(  55, 187), S(  49, 193), S(  61, 212),
},{
 S(-132, -50), S(-138, 145), S(-124, 164), S(  31, 182),
 S(  13, 119), S(  59, 173), S(  99, 180), S(  78, 198),
 S(  92, 141), S(  74, 161), S( 152, 153), S( 125, 194),
 S(  98, 182), S( 104, 198), S( 120, 212), S( 105, 221),
 S(  89, 211), S(  85, 206), S( 114, 224), S(  93, 243),
 S(  70, 191), S(  94, 189), S(  85, 208), S(  91, 227),
 S(  76, 188), S(  88, 181), S(  72, 196), S(  70, 203),
 S(  47, 194), S(  50, 189), S(  69, 192), S(  74, 207),
}};

const Score BISHOP_PSQT[2][32] = {{
 S(  12, 209), S(  20, 238), S( -30, 231), S( -46, 245),
 S(  32, 229), S(  16, 211), S(  70, 218), S(  56, 231),
 S(  68, 226), S(  72, 229), S(  31, 208), S(  78, 223),
 S(  58, 233), S(  91, 226), S(  76, 232), S(  71, 257),
 S(  82, 214), S(  73, 227), S(  88, 238), S(  97, 233),
 S(  80, 208), S( 103, 230), S(  81, 211), S(  88, 232),
 S(  87, 209), S(  85, 172), S(  99, 201), S(  79, 223),
 S(  70, 177), S( 101, 204), S(  83, 224), S(  85, 224),
},{
 S( -15, 162), S(  83, 202), S( -53, 222), S( -62, 220),
 S(  21, 179), S(   3, 200), S(  38, 235), S(  67, 208),
 S( 101, 209), S(  73, 216), S(  77, 210), S(  83, 226),
 S(  56, 213), S(  93, 222), S( 100, 231), S(  90, 233),
 S( 108, 187), S(  93, 217), S( 100, 235), S(  90, 235),
 S(  95, 201), S( 115, 211), S(  91, 209), S(  89, 243),
 S(  97, 175), S( 100, 174), S(  99, 215), S(  90, 223),
 S(  87, 162), S(  84, 238), S(  75, 230), S(  96, 218),
}};

const Score ROOK_PSQT[2][32] = {{
 S(  33, 435), S(  41, 441), S(  46, 448), S(  16, 453),
 S(  51, 416), S(  39, 433), S(  66, 432), S(  92, 411),
 S(  35, 422), S(  81, 417), S(  68, 420), S(  73, 413),
 S(  43, 436), S(  64, 429), S(  82, 430), S(  88, 411),
 S(  35, 432), S(  46, 436), S(  53, 435), S(  68, 419),
 S(  33, 422), S(  47, 410), S(  56, 416), S(  58, 412),
 S(  34, 414), S(  46, 413), S(  63, 412), S(  69, 403),
 S(  50, 415), S(  48, 411), S(  52, 417), S(  66, 398),
},{
 S( 160, 417), S(  99, 440), S(  34, 452), S(  52, 441),
 S( 108, 401), S( 144, 415), S(  99, 413), S(  64, 416),
 S(  66, 402), S( 147, 392), S( 114, 376), S( 117, 383),
 S(  62, 407), S( 116, 396), S( 100, 389), S(  97, 399),
 S(  36, 406), S(  74, 405), S(  65, 400), S(  78, 407),
 S(  52, 376), S(  83, 378), S(  74, 380), S(  74, 396),
 S(  41, 393), S(  94, 365), S(  77, 375), S(  75, 388),
 S(  69, 377), S(  65, 392), S(  66, 388), S(  75, 386),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(  37, 758), S( 120, 681), S( 198, 681), S( 186, 731),
 S(  78, 709), S(  77, 706), S( 100, 754), S(  85, 779),
 S( 119, 673), S( 130, 664), S( 122, 707), S( 134, 738),
 S( 121, 714), S( 127, 739), S( 132, 750), S( 110, 790),
 S( 133, 692), S( 116, 761), S( 121, 767), S( 108, 800),
 S( 125, 683), S( 137, 702), S( 129, 730), S( 125, 735),
 S( 135, 643), S( 139, 655), S( 148, 661), S( 147, 678),
 S( 123, 661), S( 122, 650), S( 127, 662), S( 139, 663),
},{
 S( 266, 665), S( 379, 609), S( 190, 714), S( 210, 703),
 S( 213, 669), S( 170, 761), S( 177, 780), S(  82, 796),
 S( 182, 671), S( 177, 693), S( 179, 714), S( 157, 761),
 S( 137, 726), S( 150, 748), S( 155, 705), S( 114, 793),
 S( 156, 679), S( 149, 714), S( 143, 724), S( 116, 792),
 S( 149, 639), S( 160, 666), S( 148, 705), S( 130, 732),
 S( 174, 580), S( 170, 590), S( 164, 616), S( 150, 676),
 S( 167, 583), S( 129, 621), S( 127, 615), S( 140, 639),
}};

const Score KING_PSQT[2][32] = {{
 S( 257,-145), S( 208, -63), S(  43, -11), S( 112, -22),
 S( -17,  47), S(  68,  61), S(  37,  70), S(  54,  38),
 S(  72,  29), S( 125,  56), S(  89,  68), S(  -9,  82),
 S(  62,  31), S(  89,  53), S(   1,  75), S( -60,  80),
 S( -33,  22), S(  23,  40), S(  12,  49), S(-107,  67),
 S( -11,  -7), S(   3,  20), S( -27,  28), S( -63,  34),
 S(  14, -28), S( -21,  16), S( -47,   9), S( -76,  13),
 S(  27,-102), S(  33, -48), S(   2, -38), S( -37, -45),
},{
 S( 129,-184), S(-229,  53), S(  96, -21), S( -34, -18),
 S(-156,   3), S(  47,  72), S( -44,  81), S( 201,  30),
 S(  -3,   8), S( 213,  68), S( 290,  66), S( 120,  70),
 S( -95,   8), S(  71,  53), S(   4,  83), S( -73,  88),
 S( -95,  -3), S( -62,  41), S( -31,  52), S( -85,  69),
 S( -10, -17), S(  19,  15), S( -17,  26), S( -39,  34),
 S(   9, -16), S(   2,  16), S( -44,  15), S( -79,  15),
 S(  14, -78), S(  18, -22), S( -17, -25), S( -15, -53),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -45,  28), S(  14,  38), S(  38,  34), S(  79,  50),
 S(  20,   7), S(  52,  22), S(  39,  40), S(  56,  48),
 S(  27,  -6), S(  42,  14), S(  26,  22), S(  26,  29),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -9,  21), S(  34,  10), S(  70,  25), S(  82,   8),
 S(   4,   0), S(  32,  17), S(  53,   7), S(  69,  14),
 S(  -9,  30), S(  60,  11), S(  44,  10), S(  50,  28),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(   2,  57), S(  44, 162), S(  67, 214), S(  82, 231),
 S(  95, 244), S( 105, 256), S( 116, 256), S( 127, 255),
 S( 138, 240),};

const Score BISHOP_MOBILITIES[14] = {
 S(  59, 143), S(  81, 190), S(  99, 219), S( 108, 249),
 S( 117, 261), S( 123, 274), S( 127, 283), S( 133, 288),
 S( 135, 291), S( 139, 295), S( 150, 288), S( 172, 282),
 S( 184, 288), S( 190, 279),};

const Score ROOK_MOBILITIES[15] = {
 S(  14,  88), S(  23, 315), S(  43, 361), S(  53, 369),
 S(  52, 399), S(  54, 414), S(  48, 427), S(  54, 429),
 S(  62, 433), S(  69, 438), S(  74, 442), S(  75, 447),
 S(  81, 452), S(  98, 455), S( 118, 447),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2104,-2007), S( 159,  -8), S( 205, 509), S( 189, 774),
 S( 196, 882), S( 202, 899), S( 200, 939), S( 201, 975),
 S( 203,1001), S( 206,1007), S( 208,1017), S( 211,1021),
 S( 212,1024), S( 215,1024), S( 216,1024), S( 225,1006),
 S( 225,1001), S( 227, 994), S( 235, 965), S( 268, 921),
 S( 292, 869), S( 308, 819), S( 366, 764), S( 423, 659),
 S( 418, 638), S( 649, 432), S( 496, 428), S( 584, 309),
};

const Score MINOR_BEHIND_PAWN = S(6, 23);

const Score KNIGHT_OUTPOST_REACHABLE = S(14, 26);

const Score BISHOP_OUTPOST_REACHABLE = S(10, 10);

const Score BISHOP_TRAPPED = S(-152, -276);

const Score ROOK_TRAPPED = S(-75, -30);

const Score BAD_BISHOP_PAWNS = S(-1, -7);

const Score DRAGON_BISHOP = S(31, 23);

const Score ROOK_OPEN_FILE = S(35, 22);

const Score ROOK_SEMI_OPEN = S(8, 19);

const Score DEFENDED_PAWN = S(15, 14);

const Score DOUBLED_PAWN = S(12, -39);

const Score OPPOSED_ISOLATED_PAWN = S(-4, -12);

const Score OPEN_ISOLATED_PAWN = S(-11, -24);

const Score BACKWARDS_PAWN = S(-12, -22);

const Score CONNECTED_PAWN[8] = {
 S(   0,   0), S(  91,  76), S(  33,  51), S(  15,  16),
 S(   8,   3), S(   7,   1), S(   3,  -2), S(   0,   0),
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( -38, 372), S(  15,  83),
 S( -10,  50), S( -27,  23), S( -40,   2), S(   0,   0),
};

const Score PASSED_PAWN[8] = {
 S(   0,   0), S(  43, 351), S( -17, 364), S( -11, 191),
 S( -31, 102), S( -10,  62), S(  -7,  55), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(3, -17);

const Score PASSED_PAWN_KING_PROXIMITY = S(-11, 36);

const Score PASSED_PAWN_ADVANCE_DEFENDED = S(32, 40);

const Score KNIGHT_THREATS[6] = { S(1, 27), S(1, 40), S(37, 47), S(100, 14), S(62, -57), S(313, 29),};

const Score BISHOP_THREATS[6] = { S(4, 23), S(28, 42), S(-64, 109), S(69, 25), S(74, 116), S(126, 145),};

const Score ROOK_THREATS[6] = { S(5, 36), S(45, 52), S(44, 62), S(9, 22), S(95, 10), S(333, -22),};

const Score KING_THREAT = S(-38, 44);

const Score PAWN_THREAT = S(86, 34);

const Score PAWN_PUSH_THREAT = S(25, 24);

const Score HANGING_THREAT = S(9, 12);

const Score SPACE[15] = {
 S(1114, -26), S( 195, -17), S(  79,  -8), S(  26,  -2), S(   4,   2),
 S(  -8,   4), S( -13,   7), S( -15,   9), S( -13,  11), S( -11,  15),
 S(  -6,  17), S(  -3,  24), S(   2,  24), S(   8,  14), S(  15,-252),
};

const Score PAWN_SHELTER[4][8] = {
 { S(-36, 4), S(41, 89), S(-26, 84), S(-20, 23), S(6, 0), S(42, -26), S(37, -57), S(0, 0),},
 { S(-61, 4), S(3, 81), S(-35, 67), S(-43, 35), S(-36, 15), S(28, -15), S(37, -29), S(0, 0),},
 { S(-34, -5), S(37, 81), S(-67, 61), S(-22, 22), S(-23, 6), S(-10, 2), S(37, -14), S(0, 0),},
 { S(-38, 15), S(17, 86), S(-87, 54), S(-26, 35), S(-24, 25), S(-12, 15), S(-16, 15), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-27, -18), S(-33, -4), S(-31, -12), S(-42, 4), S(-75, 47), S(8, 212), S(345, 274), S(0, 0),},
 { S(-26, -1), S(1, -8), S(3, -6), S(-20, 11), S(-37, 34), S(-128, 217), S(125, 326), S(0, 0),},
 { S(42, -21), S(40, -23), S(26, -11), S(4, 1), S(-25, 30), S(-141, 186), S(-179, 318), S(0, 0),},
 { S(-7, -8), S(1, -18), S(19, -15), S(5, -17), S(-28, 14), S(-66, 152), S(-132, 303), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-24, -8), S(54, -87), S(22, -54), S(18, -45), S(3, -34), S(0, -85), S(0, 0), S(0, 0),
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

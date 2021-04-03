#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "string.h"
#include "types.h"
#include "util.h"

#define S(mg, eg) (makeScore((mg), (eg)))
#define rel(sq, side) ((side) ? MIRROR[(sq)] : (sq))
#define distance(a, b) max(abs(rank(a) - rank(b)), abs(file(a) - file(b)))

Score PHASE_MULTIPLIERS[5] = {0, 1, 1, 2, 4};

int STATIC_MATERIAL_VALUE[7] = {100, 325, 325, 550, 1050, 30000, 0};

Score MATERIAL_VALUES[7][2] = {
    {68, 102}, {240, 323}, {276, 346}, {384, 570}, {758, 1078}, {30000, 30000}, {0, 0},
};

// clang-format off
Score PAWN_PSQT[32][2] = {
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
  {  23,  27}, {  33,  25}, {  59,  12}, {  81, -17},
  { -18,  30}, {  -9,  27}, {  23,   3}, {  28, -27},
  { -27,   9}, { -19,   0}, {  -9,  -9}, {   1, -15},
  { -27,  -6}, { -25,  -5}, { -12, -10}, {  -5,  -8},
  { -22,  -9}, { -11, -10}, { -10,  -8}, {  -4,  -5},
  { -27,  -5}, {  -8,  -4}, {  -9,   1}, {  -7,   7},
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
};

Score KNIGHT_PSQT[32][2] = {
  { -95, -49}, {-112, -13}, {-146,  -1}, {-149,  16},
  { -19, -18}, { -12,   2}, {  28,  -2}, {  -2,  12},
  {   6, -15}, {   5,   1}, {   4,  13}, {  15,   6},
  {  32,  -1}, {  14,   1}, {  35,   2}, {  34,   6},
  {  28,   5}, {  41,  -5}, {  33,   7}, {  36,  12},
  {   8,  -4}, {  19,  -1}, {  19,   7}, {  27,  14},
  {  19,  15}, {  17,  -1}, {  14,  -1}, {  24,   3},
  {  10,   1}, {  13,   0}, {  21,  -4}, {  23,   4},
};

Score KNIGHT_POST_PSQT[32][2] = {
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
  {   0,   0}, {   8,  15}, {  46,  14}, {  32,  29}, 
  {   0,   0}, {  35,  21}, {  37,  22}, {  51,  22}, 
  {   0,   0}, {  38,   6}, {  26,  15}, {  24,  27}, 
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0},
};

Score BISHOP_PSQT[32][2] = {
  { -58,   2}, { -54,   7}, {-103,  12}, {-131,  17},
  {  -2,  -9}, { -17,  -3}, {   0,  -3}, { -26,   8},
  {  10,  -2}, {  12,   1}, { -15,  13}, {  12,  -4},
  { -11,   7}, {  14,   4}, {   7,   5}, {  26,   5},
  {  20,  -9}, {   0,   1}, {  14,   5}, {  22,   3},
  {  11,  -7}, {  29,   1}, {  19,   3}, {  20,  11},
  {  24,  -8}, {  28,  -7}, {  25, -10}, {  15,   3},
  {  26, -12}, {  26,  -7}, {  19,   1}, {  15,   0},
};

Score ROOK_PSQT[32][2] = {
  {  11,   8}, { -12,  17}, { -24,  28}, { -30,  27},
  {   1, -21}, { -22,  -4}, {  -7,  -4}, {   0, -10},
  {   3,  -4}, {  29,  -5}, {  24,   0}, {  29,  -9},
  {  -2,   3}, {   2,   2}, {  14,   0}, {  11,   0},
  { -12,   2}, {  -6,  -2}, { -15,   9}, {  -4,   6},
  { -10,  -6}, {  -5,  -8}, {  -2,  -1}, {  -2,  -1},
  {  -5,  -3}, {  -6,  -4}, {   1,  -3}, {   8,  -2},
  {   3,  -1}, {  -1,  -4}, {   4,   1}, {   7,  -4},
};

Score QUEEN_PSQT[32][2] = {
  {  24, -36}, {  23, -36}, {  39, -18}, {  17,  -4},
  {  -1, -28}, { -37,  14}, { -44,  41}, { -38,  37},
  { -11, -19}, { -20,   9}, {  -8,  26}, {  -4,  21},
  {  -8,   3}, {   2,  16}, {  -9,  21}, { -11,  45},
  {   4,   8}, {  -1,  19}, {  -3,  21}, {  -4,  39},
  {   4, -15}, {  10,   0}, {   1,  19}, {  -1,  23},
  {  13, -40}, {  12, -32}, {  12, -15}, {  11,   1},
  {  20, -60}, {  -2, -19}, {  -3, -19}, {   5, -18},
};

Score KING_PSQT[32][2] = {
  { -69,-200}, {   3,-113}, { -41, -82}, {  30, -78}, 
  { -53, -99}, { -39, -53}, { -20, -42}, { -43, -35}, 
  {-149, -86}, { -71, -57}, { -63, -42}, { -76, -32}, 
  {-188, -79}, {-100, -60}, {-150, -35}, {-157, -25}, 
  {-190, -80}, {-144, -63}, {-133, -49}, {-132, -40}, 
  {-190, -80}, {-144, -63}, {-133, -49}, {-132, -40}, 
  { -79,-104}, { -75, -86}, { -88, -75}, {-110, -66}, 
  { -76,-134}, { -62,-110}, { -69, -98}, { -84,-101}, 
};

// clang-format on

Score KNIGHT_MOBILITIES[9][2] = {{-59, -73}, {-28, -29}, {-12, -3}, {-4, 9}, {5, 17},
                                 {12, 22},   {19, 22},   {28, 20},  {34, 13}};

Score BISHOP_MOBILITIES[14][2] = {{-59, -42}, {-38, -34}, {-24, -26}, {-17, -11}, {-7, -2}, {-1, 5},  {2, 10},
                                  {3, 14},    {6, 17},    {5, 20},    {9, 17},    {29, 13}, {33, 11}, {46, 12}};

Score ROOK_MOBILITIES[15][2] = {{-40, -174}, {-17, -21}, {-7, -8}, {-2, -13}, {-1, 2},  {-3, 10}, {-6, 20}, {-5, 17},
                                {1, 17},     {4, 21},    {8, 22},  {9, 22},   {10, 26}, {15, 24}, {22, 22}};

Score QUEEN_MOBILITIES[28][2] = {{54, 12},  {-20, -63}, {-20, -133}, {-20, -99}, {-20, -47}, {-20, -37},  {-20, -8},
                                 {-20, 12}, {-20, 32},  {-17, 32},   {-14, 35},  {-13, 38},  {-14, 44},   {-12, 45},
                                 {-12, 48}, {-11, 45},  {-17, 52},   {-15, 51},  {-20, 51},  {-15, 37},   {-12, 33},
                                 {-17, 39}, {-10, 17},  {-20, 12},   {-20, -21}, {97, -83},  {117, -110}, {106, -48}};

Score DOUBLED_PAWN[2] = {-16, -19};
Score OPPOSED_ISOLATED_PAWN[2] = {-7, -7};
Score OPEN_ISOLATED_PAWN[2] = {-26, -4};
Score BACKWARDS_PAWN[2] = {-12, -12};
Score DEFENDED_PAWN[2] = {3, 5};
Score CONNECTED_PAWN[8][2] = {{0, 0}, {51, 14}, {24, 11}, {8, 8}, {4, 3}, {0, 2}, {0, 0}, {0, 0}};
Score PASSED_PAWN_ADVANCE_DEFENDED[2] = {5, 33};
Score PASSED_PAWN[8][2] = {{0, 0}, {38, 151}, {29, 111}, {31, 60}, {12, 34}, {4, 23}, {1, 17}, {0, 0}};
Score PASSED_PAWN_EDGE_DISTANCE[2] = {-8, -6};

Score DEFENDED_MINOR[2] = {0, 0};

Score ROOK_OPEN_FILE[2] = {16, 16};
Score ROOK_SEMI_OPEN[2] = {3, 11};
Score ROOK_SEVENTH_RANK[2] = {0, 19};
Score ROOK_OPPOSITE_KING[2] = {40, -28};
Score ROOK_ADJACENT_KING[2] = {0, -19};

Score ROOK_TRAPPED[2] = {-38, -24};

Score BISHOP_TRAPPED[2] = {-122, -90};
Score BISHOP_PAIR[2] = {17, 56};

Score KNIGHT_THREATS[6][2] = {{-7, 15}, {9, 26}, {32, 23}, {57, 1}, {20, -12}, {0, 0}};
Score BISHOP_THREATS[6][2] = {{-1, 12}, {23, 27}, {17, 20}, {26, 14}, {6, 100}, {0, 0}};
Score ROOK_THREATS[6][2] = {{-6, 19}, {15, 33}, {22, 39}, {-20, 29}, {35, 39}, {0, 0}};
Score KING_THREATS[6][2] = {{69, 43}, {-17, 36}, {70, 10}, {25, 0}, {0, 0}, {0, 0}};
Score HANGING_THREAT[2] = {0, 0};

Score PAWN_SHELTER[2][8][2] = {
    {{-26, 11}, {4, 83}, {-5, 22}, {-13, -18}, {-15, -20}, {-16, 15}, {-6, 12}, {0, 0}},
    {{-69, -31}, {-39, 48}, {-59, 1}, {-47, -62}, {-46, -51}, {-35, -34}, {-30, -35}, {0, 0}},
};
Score PAWN_STORM[8][2] = {{0, 0}, {0, 0}, {2, -6}, {-2, 8}, {-13, 8}, {-21, -27}, {36, 68}, {0, 0}};

Score KS_ATTACKER_WEIGHTS[5] = {0, 64, 53, 67, 25};
Score KS_ATTACK = 39;
Score KS_WEAK_SQS = 74;
Score KS_SAFE_CHECK = 90;
Score KS_UNSAFE_CHECK = 12;
Score KS_ENEMY_QUEEN = -247;
Score KS_ALLIES = -17;

Score* PSQT[12][64];
Score* KNIGHT_POSTS[64];

void initPSQT() {
  for (int sq = 0; sq < 32; sq++) {
    int r = sq / 4;
    int f = sq % 4;

    PSQT[PAWN[WHITE]][r * 8 + f] = PAWN_PSQT[sq];
    PSQT[PAWN[WHITE]][r * 8 + (7 - f)] = PAWN_PSQT[sq];
    PSQT[PAWN[BLACK]][MIRROR[r * 8 + f]] = PAWN_PSQT[sq];
    PSQT[PAWN[BLACK]][MIRROR[r * 8 + (7 - f)]] = PAWN_PSQT[sq];

    PSQT[KNIGHT[WHITE]][r * 8 + f] = KNIGHT_PSQT[sq];
    PSQT[KNIGHT[WHITE]][r * 8 + (7 - f)] = KNIGHT_PSQT[sq];
    PSQT[KNIGHT[BLACK]][MIRROR[r * 8 + f]] = KNIGHT_PSQT[sq];
    PSQT[KNIGHT[BLACK]][MIRROR[r * 8 + (7 - f)]] = KNIGHT_PSQT[sq];

    PSQT[BISHOP[WHITE]][r * 8 + f] = BISHOP_PSQT[sq];
    PSQT[BISHOP[WHITE]][r * 8 + (7 - f)] = BISHOP_PSQT[sq];
    PSQT[BISHOP[BLACK]][MIRROR[r * 8 + f]] = BISHOP_PSQT[sq];
    PSQT[BISHOP[BLACK]][MIRROR[r * 8 + (7 - f)]] = BISHOP_PSQT[sq];

    PSQT[ROOK[WHITE]][r * 8 + f] = ROOK_PSQT[sq];
    PSQT[ROOK[WHITE]][r * 8 + (7 - f)] = ROOK_PSQT[sq];
    PSQT[ROOK[BLACK]][MIRROR[r * 8 + f]] = ROOK_PSQT[sq];
    PSQT[ROOK[BLACK]][MIRROR[r * 8 + (7 - f)]] = ROOK_PSQT[sq];

    PSQT[QUEEN[WHITE]][r * 8 + f] = QUEEN_PSQT[sq];
    PSQT[QUEEN[WHITE]][r * 8 + (7 - f)] = QUEEN_PSQT[sq];
    PSQT[QUEEN[BLACK]][MIRROR[r * 8 + f]] = QUEEN_PSQT[sq];
    PSQT[QUEEN[BLACK]][MIRROR[r * 8 + (7 - f)]] = QUEEN_PSQT[sq];

    PSQT[KING[WHITE]][r * 8 + f] = KING_PSQT[sq];
    PSQT[KING[WHITE]][r * 8 + (7 - f)] = KING_PSQT[sq];
    PSQT[KING[BLACK]][MIRROR[r * 8 + f]] = KING_PSQT[sq];
    PSQT[KING[BLACK]][MIRROR[r * 8 + (7 - f)]] = KING_PSQT[sq];

    KNIGHT_POSTS[r * 8 + f] = KNIGHT_POST_PSQT[sq];
    KNIGHT_POSTS[r * 8 + (7 - f)] = KNIGHT_POST_PSQT[sq];
  }
}

inline Score maxPhase() {
  return 4 * PHASE_MULTIPLIERS[KNIGHT_TYPE] + 4 * PHASE_MULTIPLIERS[BISHOP_TYPE] + 4 * PHASE_MULTIPLIERS[ROOK_TYPE] +
         2 * PHASE_MULTIPLIERS[QUEEN_TYPE];
}

inline Score getPhase(Board* board) {
  Score maxP = maxPhase();

  Score phase = 0;
  for (int i = KNIGHT_WHITE; i <= QUEEN_BLACK; i++)
    phase += PHASE_MULTIPLIERS[PIECE_TYPE[i]] * bits(board->pieces[i]);

  phase = min(maxP, phase);
  return phase;
}

inline Score taper(Score mg, Score eg, Score phase) {
  Score maxP = maxPhase();

  return (phase * mg + (maxP - phase) * eg) / maxP;
}

inline int isMaterialDraw(Board* board) {
  switch (board->piecesCounts) {
  case 0x0:      // Kk
  case 0x100:    // KNk
  case 0x200:    // KNNk
  case 0x1000:   // Kkn
  case 0x2000:   // Kknn
  case 0x1100:   // KNkn
  case 0x10000:  // KBk
  case 0x100000: // Kkb
  case 0x11000:  // KBkn
  case 0x100100: // KNkb
  case 0x110000: // KBkb
    return 1;
  default:
    return 0;
  }
}

void EvaluateSide(Board* board, int side, EvalData* data) {
  data->material[MG] = 0;
  data->material[EG] = 0;
  data->pawns[MG] = 0;
  data->pawns[EG] = 0;
  data->knights[MG] = 0;
  data->knights[EG] = 0;
  data->bishops[MG] = 0;
  data->bishops[EG] = 0;
  data->rooks[MG] = 0;
  data->rooks[EG] = 0;
  data->queens[MG] = 0;
  data->queens[EG] = 0;
  data->kings[MG] = 0;
  data->kings[EG] = 0;
  data->mobility[MG] = 0;
  data->mobility[EG] = 0;

  data->attacks2 = 0;
  data->allAttacks = 0;
  data->attackCount = 0;
  data->attackWeight = 0;
  data->attackers = 0;
  memset(data->attacks, 0, sizeof(data->attacks));

  int xside = 1 - side;

  // Random utility stuff
  BitBoard myPawns = board->pieces[PAWN[side]];
  BitBoard opponentPawns = board->pieces[PAWN[xside]];

  BitBoard pawnAttacks = shift(myPawns, PAWN_DIRECTIONS[side] + E) | shift(myPawns, PAWN_DIRECTIONS[side] + W);
  BitBoard oppoPawnAttacks =
      shift(opponentPawns, PAWN_DIRECTIONS[xside] + E) | shift(opponentPawns, PAWN_DIRECTIONS[xside] + W);

  BitBoard myBlockedAndHomePawns =
      (shift(board->occupancies[BOTH], PAWN_DIRECTIONS[xside]) | HOME_RANKS[side] | THIRD_RANKS[side]) &
      board->pieces[PAWN[side]];

  int kingSq = lsb(board->pieces[KING[side]]);
  int enemyKingSq = lsb(board->pieces[KING[xside]]);
  BitBoard enemyKingArea = getKingAttacks(enemyKingSq);

  // Initial attack stuff from pawns (pawn eval done elsewhere)
  data->attacks[PAWN_TYPE] = pawnAttacks;
  data->allAttacks = pawnAttacks;
  data->attacks2 = shift(myPawns, PAWN_DIRECTIONS[side] + E) & shift(myPawns, PAWN_DIRECTIONS[side] + W);

  BitBoard outposts = ~fill(oppoPawnAttacks, PAWN_DIRECTIONS[xside]) &
                      (pawnAttacks | shift(myPawns | opponentPawns, PAWN_DIRECTIONS[xside]));
  // KNIGHTS
  for (BitBoard knights = board->pieces[KNIGHT[side]]; knights; popLsb(knights)) {
    int sq = lsb(knights);
    BitBoard sqBb = knights & -knights;

    data->material[MG] += MATERIAL_VALUES[KNIGHT_TYPE][MG];
    data->material[EG] += MATERIAL_VALUES[KNIGHT_TYPE][EG];
    data->knights[MG] += PSQT[KNIGHT[side]][sq][MG];
    data->knights[EG] += PSQT[KNIGHT[side]][sq][EG];

    BitBoard movement = getKnightAttacks(sq);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[KNIGHT_TYPE] |= movement;
    data->allAttacks |= movement;
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[KNIGHT_TYPE];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[MG] += KNIGHT_MOBILITIES[bits(mobilitySquares)][MG];
    data->mobility[EG] += KNIGHT_MOBILITIES[bits(mobilitySquares)][EG];

    if (outposts & sqBb) {
      data->knights[MG] += KNIGHT_POSTS[rel(sq, side)][MG];
      data->knights[EG] += KNIGHT_POSTS[rel(sq, side)][EG];
    }
  }

  data->knights[MG] += DEFENDED_MINOR[MG] * bits(pawnAttacks & board->pieces[KNIGHT[side]]);
  data->knights[EG] += DEFENDED_MINOR[EG] * bits(pawnAttacks & board->pieces[KNIGHT[side]]);

  // BISHOPS
  if (bits(board->pieces[BISHOP[side]]) > 1) {
    data->bishops[MG] += BISHOP_PAIR[MG];
    data->bishops[EG] += BISHOP_PAIR[EG];
  }

  for (BitBoard bishops = board->pieces[BISHOP[side]]; bishops; popLsb(bishops)) {
    int sq = lsb(bishops);

    data->material[MG] += MATERIAL_VALUES[BISHOP_TYPE][MG];
    data->material[EG] += MATERIAL_VALUES[BISHOP_TYPE][EG];
    data->bishops[MG] += PSQT[BISHOP[side]][sq][MG];
    data->bishops[EG] += PSQT[BISHOP[side]][sq][EG];

    BitBoard movement = getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[BISHOP_TYPE] |= movement;
    data->allAttacks |= movement;
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[BISHOP_TYPE];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[MG] += BISHOP_MOBILITIES[bits(mobilitySquares)][MG];
    data->mobility[EG] += BISHOP_MOBILITIES[bits(mobilitySquares)][EG];

    if (side == WHITE) {
      if ((sq == A7 || sq == B8) && getBit(opponentPawns, B6) && getBit(opponentPawns, C7)) {
        data->bishops[MG] += BISHOP_TRAPPED[MG];
        data->bishops[EG] += BISHOP_TRAPPED[EG];
      } else if ((sq == H7 || sq == G8) && getBit(opponentPawns, F7) && getBit(opponentPawns, G6)) {
        data->bishops[MG] += BISHOP_TRAPPED[MG];
        data->bishops[EG] += BISHOP_TRAPPED[EG];
      }
    } else {
      if ((sq == A2 || sq == B1) && getBit(opponentPawns, B3) && getBit(opponentPawns, C2)) {
        data->bishops[MG] += BISHOP_TRAPPED[MG];
        data->bishops[EG] += BISHOP_TRAPPED[EG];
      } else if ((sq == H2 || sq == G1) && getBit(opponentPawns, G3) && getBit(opponentPawns, F2)) {
        data->bishops[MG] += BISHOP_TRAPPED[MG];
        data->bishops[EG] += BISHOP_TRAPPED[EG];
      }
    }
  }

  data->bishops[MG] += DEFENDED_MINOR[MG] * bits(pawnAttacks & board->pieces[BISHOP[side]]);
  data->bishops[EG] += DEFENDED_MINOR[EG] * bits(pawnAttacks & board->pieces[BISHOP[side]]);

  // ROOKS
  for (BitBoard rooks = board->pieces[ROOK[side]]; rooks; popLsb(rooks)) {
    int sq = lsb(rooks);
    BitBoard sqBb = rooks & -rooks;
    int file = file(sq);

    data->material[MG] += MATERIAL_VALUES[ROOK_TYPE][MG];
    data->material[EG] += MATERIAL_VALUES[ROOK_TYPE][EG];
    data->rooks[MG] += PSQT[ROOK[side]][sq][MG];
    data->rooks[EG] += PSQT[ROOK[side]][sq][EG];

    BitBoard movement =
        getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]] ^ board->pieces[ROOK[side]]);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[ROOK_TYPE] |= movement;
    data->allAttacks |= movement;
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[ROOK_TYPE];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[MG] += ROOK_MOBILITIES[bits(mobilitySquares)][MG];
    data->mobility[EG] += ROOK_MOBILITIES[bits(mobilitySquares)][EG];

    if (sqBb & PROMOTION_RANKS[side]) {
      data->rooks[MG] += ROOK_SEVENTH_RANK[MG];
      data->rooks[EG] += ROOK_SEVENTH_RANK[EG];
    }

    if (!(FILE_MASKS[file] & myPawns)) {
      if (!(FILE_MASKS[file] & opponentPawns)) {
        data->rooks[MG] += ROOK_OPEN_FILE[MG];
        data->rooks[EG] += ROOK_OPEN_FILE[EG];
      } else {
        data->rooks[MG] += ROOK_SEMI_OPEN[MG];
        data->rooks[EG] += ROOK_SEMI_OPEN[EG];
      }

      if (FILE_MASKS[file] & board->pieces[KING[xside]]) {
        data->rooks[MG] += ROOK_OPPOSITE_KING[MG];
        data->rooks[EG] += ROOK_OPPOSITE_KING[EG];
      } else if (ADJACENT_FILE_MASKS[file] & board->pieces[KING[xside]]) {
        data->rooks[MG] += ROOK_ADJACENT_KING[MG];
        data->rooks[EG] += ROOK_ADJACENT_KING[EG];
      }
    }

    if (side == WHITE) {
      if ((sq == A1 || sq == A2 || sq == B1) && (kingSq == C1 || kingSq == B1)) {
        data->rooks[MG] += ROOK_TRAPPED[MG];
        data->rooks[EG] += ROOK_TRAPPED[EG];
      } else if ((sq == H1 || sq == H2 || sq == G1) && (kingSq == F1 || kingSq == G1)) {
        data->rooks[MG] += ROOK_TRAPPED[MG];
        data->rooks[EG] += ROOK_TRAPPED[EG];
      }
    } else {
      if ((sq == A8 || sq == A7 || sq == B8) && (kingSq == B8 || kingSq == C8)) {
        data->rooks[MG] += ROOK_TRAPPED[MG];
        data->rooks[EG] += ROOK_TRAPPED[EG];
      } else if ((sq == H8 || sq == H7 || sq == G8) && (kingSq == F8 || kingSq == G8)) {
        data->rooks[MG] += ROOK_TRAPPED[MG];
        data->rooks[EG] += ROOK_TRAPPED[EG];
      }
    }
  }

  // QUEENS
  for (BitBoard queens = board->pieces[QUEEN[side]]; queens; popLsb(queens)) {
    int sq = lsb(queens);

    data->material[MG] += MATERIAL_VALUES[QUEEN_TYPE][MG];
    data->material[EG] += MATERIAL_VALUES[QUEEN_TYPE][EG];
    data->queens[MG] += PSQT[QUEEN[side]][sq][MG];
    data->queens[EG] += PSQT[QUEEN[side]][sq][EG];

    BitBoard movement = getQueenAttacks(sq, board->occupancies[BOTH]);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[QUEEN_TYPE] |= movement;
    data->allAttacks |= movement;
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[QUEEN_TYPE];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[MG] += QUEEN_MOBILITIES[bits(mobilitySquares)][MG];
    data->mobility[EG] += QUEEN_MOBILITIES[bits(mobilitySquares)][EG];
  }

  // KINGS
  for (BitBoard kings = board->pieces[KING[side]]; kings; popLsb(kings)) {
    int sq = lsb(kings);

    data->kings[MG] += PSQT[KING[side]][sq][MG];
    data->kings[EG] += PSQT[KING[side]][sq][EG];

    BitBoard movement = getKingAttacks(sq) & ~getKingAttacks(lsb(board->pieces[KING[xside]]));
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[KING_TYPE] |= movement;
    data->allAttacks |= movement;
  }

  // TODO: Probably separate this function
  // PAWNS
  for (BitBoard pawns = board->pieces[PAWN[side]]; pawns; popLsb(pawns)) {
    BitBoard sqBitboard = pawns & -pawns;
    int sq = lsb(pawns);

    data->material[MG] += MATERIAL_VALUES[PAWN_TYPE][MG];
    data->material[EG] += MATERIAL_VALUES[PAWN_TYPE][EG];
    data->pawns[MG] += PSQT[PAWN[side]][sq][MG];
    data->pawns[EG] += PSQT[PAWN[side]][sq][EG];

    int file = file(sq);
    int rank = rank(sq);
    int adjustedRank = side ? 7 - rank : rank;
    int adjustedFile = file > 3 ? 7 - file : file;

    BitBoard opposed = board->pieces[PAWN[xside]] & FILE_MASKS[file] & FORWARD_RANK_MASKS[side][rank];
    BitBoard doubled = board->pieces[PAWN[side]] & shift(sqBitboard, PAWN_DIRECTIONS[xside]);
    BitBoard neighbors = board->pieces[PAWN[side]] & ADJACENT_FILE_MASKS[file];
    BitBoard connected = neighbors & RANK_MASKS[rank];
    BitBoard defenders = board->pieces[PAWN[side]] & getPawnAttacks(sq, xside);
    BitBoard forwardLever = board->pieces[PAWN[xside]] & getPawnAttacks(sq + PAWN_DIRECTIONS[side], side);

    int backwards = !(neighbors & FORWARD_RANK_MASKS[xside][rank(sq + PAWN_DIRECTIONS[side])]) && forwardLever;
    int passed = !(board->pieces[PAWN[xside]] & FORWARD_RANK_MASKS[side][rank] &
                   (ADJACENT_FILE_MASKS[file] | FILE_MASKS[file])) &&
                 !(board->pieces[PAWN[side]] & FORWARD_RANK_MASKS[side][rank] & FILE_MASKS[file]);

    if (doubled) {
      data->pawns[MG] += DOUBLED_PAWN[MG];
      data->pawns[EG] += DOUBLED_PAWN[EG];
    }

    if (!neighbors) {
      data->pawns[MG] += opposed ? OPPOSED_ISOLATED_PAWN[MG] : OPEN_ISOLATED_PAWN[MG];
      data->pawns[EG] += opposed ? OPPOSED_ISOLATED_PAWN[EG] : OPEN_ISOLATED_PAWN[EG];
    } else if (backwards) {
      data->pawns[MG] += BACKWARDS_PAWN[MG];
      data->pawns[EG] += BACKWARDS_PAWN[EG];
    } else if (defenders | connected) {
      int s = 2;
      if (connected)
        s++;
      if (opposed)
        s--;

      data->pawns[MG] += CONNECTED_PAWN[adjustedRank][MG] * s + DEFENDED_PAWN[MG] * bits(defenders);
      data->pawns[EG] += CONNECTED_PAWN[adjustedRank][EG] * s + DEFENDED_PAWN[EG] * bits(defenders);
    }

    if (passed) {
      data->pawns[MG] += PASSED_PAWN[adjustedRank][MG];
      data->pawns[EG] += PASSED_PAWN[adjustedRank][EG];

      data->pawns[MG] += PASSED_PAWN_EDGE_DISTANCE[MG] * adjustedFile;
      data->pawns[EG] += PASSED_PAWN_EDGE_DISTANCE[EG] * adjustedFile;

      // TODO: enemy piece proximity and king proximity
      // king proximity seems to exist in the shelter/storm values

      BitBoard advance = bit(sq + PAWN_DIRECTIONS[side]);
      if (adjustedRank <= 4 && !(board->occupancies[BOTH] & advance)) {
        BitBoard pusher = getRookAttacks(sq, board->occupancies[BOTH]) & FILE_MASKS[file] &
                          FORWARD_RANK_MASKS[xside][rank] & (board->pieces[ROOK[side]] | board->pieces[QUEEN[side]]);

        if ((pusher | data->allAttacks) & advance) {
          data->pawns[MG] += PASSED_PAWN_ADVANCE_DEFENDED[MG];
          data->pawns[EG] += PASSED_PAWN_ADVANCE_DEFENDED[EG];
        }
      }
    }
  }
}

void EvaluateThreats(Board* board, int side, EvalData* data, EvalData* enemyData) {
  data->threats[MG] = 0;
  data->threats[EG] = 0;

  int xside = 1 - side;

  BitBoard* myAttacks = data->attacks;
  BitBoard* enemyAttacks = enemyData->attacks;

  BitBoard weak = board->occupancies[xside] & ~enemyAttacks[PAWN_TYPE] & data->allAttacks;

  for (BitBoard knightThreats = weak & myAttacks[KNIGHT_TYPE]; knightThreats; popLsb(knightThreats)) {
    int piece = board->squares[lsb(knightThreats)];

    assert(piece != NO_PIECE);

    data->threats[MG] += KNIGHT_THREATS[PIECE_TYPE[piece]][MG];
    data->threats[EG] += KNIGHT_THREATS[PIECE_TYPE[piece]][EG];
  }

  for (BitBoard bishopThreats = weak & myAttacks[BISHOP_TYPE]; bishopThreats; popLsb(bishopThreats)) {
    int piece = board->squares[lsb(bishopThreats)];

    assert(piece != NO_PIECE);

    data->threats[MG] += BISHOP_THREATS[PIECE_TYPE[piece]][MG];
    data->threats[EG] += BISHOP_THREATS[PIECE_TYPE[piece]][EG];
  }

  for (BitBoard rookThreats = weak & myAttacks[ROOK_TYPE]; rookThreats; popLsb(rookThreats)) {
    int piece = board->squares[lsb(rookThreats)];

    assert(piece != NO_PIECE);

    data->threats[MG] += ROOK_THREATS[PIECE_TYPE[piece]][MG];
    data->threats[EG] += ROOK_THREATS[PIECE_TYPE[piece]][EG];
  }

  for (BitBoard kingThreats = weak & myAttacks[KING_TYPE]; kingThreats; popLsb(kingThreats)) {
    int piece = board->squares[lsb(kingThreats)];

    assert(piece != NO_PIECE);

    data->threats[MG] += KING_THREATS[PIECE_TYPE[piece]][MG];
    data->threats[EG] += KING_THREATS[PIECE_TYPE[piece]][EG];
  }

  BitBoard hanging = data->allAttacks & ~enemyData->allAttacks & board->occupancies[xside];
  data->threats[MG] += HANGING_THREAT[MG] * bits(hanging);
  data->threats[EG] += HANGING_THREAT[EG] * bits(hanging);
}

void EvaluateKingSafety(Board* board, int side, EvalData* data, EvalData* enemyData) {
  data->kingSafety[MG] = 0;
  data->kingSafety[EG] = 0;

  int xside = 1 - side;

  int kingSq = lsb(board->pieces[KING[side]]);

  if (!!(board->pieces[QUEEN[xside]]) + enemyData->attackers > 1) {
    BitBoard kingArea = getKingAttacks(kingSq);
    BitBoard weak = enemyData->allAttacks & ~data->attacks2 & (~data->allAttacks | data->attacks[4] | data->attacks[5]);
    BitBoard vulnerable = (~data->allAttacks | (weak & enemyData->attacks2)) & ~board->occupancies[xside];

    BitBoard possibleKnightChecks =
        getKnightAttacks(kingSq) & enemyData->attacks[KNIGHT_TYPE] & ~board->occupancies[xside];
    int safeChecks = bits(possibleKnightChecks & vulnerable);
    int unsafeChecks = bits(possibleKnightChecks & ~vulnerable);

    BitBoard possibleBishopChecks = getBishopAttacks(kingSq, board->occupancies[BOTH]) &
                                    enemyData->attacks[BISHOP_TYPE] & ~board->occupancies[xside];
    safeChecks += bits(possibleBishopChecks & vulnerable);
    unsafeChecks += bits(possibleBishopChecks & ~vulnerable);

    BitBoard possibleRookChecks =
        getRookAttacks(kingSq, board->occupancies[BOTH]) & enemyData->attacks[ROOK_TYPE] & ~board->occupancies[xside];
    safeChecks += bits(possibleRookChecks & vulnerable);
    unsafeChecks += bits(possibleRookChecks & ~vulnerable);

    BitBoard possibleQueenChecks =
        getQueenAttacks(kingSq, board->occupancies[BOTH]) & enemyData->attacks[QUEEN_TYPE] & ~board->occupancies[xside];
    safeChecks += bits(possibleQueenChecks & vulnerable);
    unsafeChecks += bits(possibleQueenChecks & ~vulnerable);

    BitBoard allies = kingArea & (board->occupancies[side] & ~board->pieces[QUEEN[side]]);

    int score = (enemyData->attackWeight + KS_SAFE_CHECK * safeChecks + KS_UNSAFE_CHECK * unsafeChecks +
                 KS_WEAK_SQS * bits(vulnerable & kingArea) + KS_ATTACK * (enemyData->attackCount * 8 / bits(kingArea)) +
                 KS_ENEMY_QUEEN * !(board->pieces[QUEEN[xside]]) + KS_ALLIES * (bits(allies) - 2));

    if (score > 0) {
      data->kingSafety[MG] += -score * score / 1000;
      data->kingSafety[EG] += -score / 30;
    }
  }

  if (!board->pieces[QUEEN[xside]] ||
      !(board->pieces[ROOK[xside]] | board->pieces[BISHOP[xside]] | board->pieces[KNIGHT[xside]]))
    return;

  // PAWN STORM / SHIELD
  for (int c = max(0, file(kingSq) - 1); c <= min(7, file(kingSq) + 1); c++) {
    BitBoard pawnFile = board->pieces[PAWN[side]] & FILE_MASKS[c];
    int pawnRank = pawnFile ? (side ? 7 - rank(lsb(pawnFile)) : rank(msb(pawnFile))) : 0;

    data->kingSafety[MG] += PAWN_SHELTER[c == file(kingSq)][pawnRank][MG];
    data->kingSafety[EG] += PAWN_SHELTER[c == file(kingSq)][pawnRank][EG];

    BitBoard enemyFile = board->pieces[PAWN[xside]] & FILE_MASKS[c];
    if (enemyFile) {
      int pushRank = (side ? 7 - rank(lsb(enemyFile)) : rank(msb(enemyFile)));

      data->kingSafety[MG] += PAWN_STORM[pushRank][MG];
      data->kingSafety[EG] += PAWN_STORM[pushRank][EG];
    }
  }
}

Score Evaluate(Board* board) {
  EvalData sideData[1];
  EvalData xsideData[1];

  EvaluateSide(board, board->side, sideData);
  EvaluateSide(board, board->xside, xsideData);

  EvaluateThreats(board, board->side, sideData, xsideData);
  EvaluateThreats(board, board->xside, xsideData, sideData);

  EvaluateKingSafety(board, board->side, sideData, xsideData);
  EvaluateKingSafety(board, board->xside, xsideData, sideData);

  return toScore(sideData, board) - toScore(xsideData, board);
}

inline Score toScore(EvalData* data, Board* board) {
  Score mg = data->pawns[MG] + data->knights[MG] + data->bishops[MG] + data->rooks[MG] + data->queens[MG] +
             data->kings[MG] + data->kingSafety[MG] + data->material[MG] + data->mobility[MG] + data->threats[MG];
  Score eg = data->pawns[EG] + data->knights[EG] + data->bishops[EG] + data->rooks[EG] + data->queens[EG] +
             data->kings[EG] + data->kingSafety[EG] + data->material[EG] + data->mobility[EG] + data->threats[EG];

  return taper(mg, eg, getPhase(board));
}

#ifndef TUNE
void PrintEvaluation(Board* board) {
  EvalData whiteEval[1];
  EvalData blackEval[1];

  EvaluateSide(board, WHITE, whiteEval);
  EvaluateSide(board, BLACK, blackEval);

  EvaluateThreats(board, WHITE, whiteEval, blackEval);
  EvaluateThreats(board, BLACK, blackEval, whiteEval);

  EvaluateKingSafety(board, WHITE, whiteEval, blackEval);
  EvaluateKingSafety(board, BLACK, blackEval, whiteEval);

  int phase = getPhase(board);

  printf("|            | WHITE mg,eg,taper | BLACK mg,eg,taper | DIFF  mg,eg,taper |\n");
  printf("|------------|-------------------|-------------------|-------------------|\n");
  printf("| material   | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->material[MG], whiteEval->material[EG],
         taper(whiteEval->material[MG], whiteEval->material[EG], phase), blackEval->material[MG],
         blackEval->material[EG], taper(blackEval->material[MG], blackEval->material[EG], phase),
         whiteEval->material[MG] - blackEval->material[MG], whiteEval->material[EG] - blackEval->material[EG],
         taper(whiteEval->material[MG] - blackEval->material[MG], whiteEval->material[EG] - blackEval->material[EG],
               phase));
  printf("| pawns      | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->pawns[MG], whiteEval->pawns[EG],
         taper(whiteEval->pawns[MG], whiteEval->pawns[EG], phase), blackEval->pawns[MG], blackEval->pawns[EG],
         taper(blackEval->pawns[MG], blackEval->pawns[EG], phase), whiteEval->pawns[MG] - blackEval->pawns[MG],
         whiteEval->pawns[EG] - blackEval->pawns[EG],
         taper(whiteEval->pawns[MG] - blackEval->pawns[MG], whiteEval->pawns[EG] - blackEval->pawns[EG], phase));
  printf(
      "| knights    | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->knights[MG], whiteEval->knights[EG],
      taper(whiteEval->knights[MG], whiteEval->knights[EG], phase), blackEval->knights[MG], blackEval->knights[EG],
      taper(blackEval->knights[MG], blackEval->knights[EG], phase), whiteEval->knights[MG] - blackEval->knights[MG],
      whiteEval->knights[EG] - blackEval->knights[EG],
      taper(whiteEval->knights[MG] - blackEval->knights[MG], whiteEval->knights[EG] - blackEval->knights[EG], phase));
  printf(
      "| bishops    | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->bishops[MG], whiteEval->bishops[EG],
      taper(whiteEval->bishops[MG], whiteEval->bishops[EG], phase), blackEval->bishops[MG], blackEval->bishops[EG],
      taper(blackEval->bishops[MG], blackEval->bishops[EG], phase), whiteEval->bishops[MG] - blackEval->bishops[MG],
      whiteEval->bishops[EG] - blackEval->bishops[EG],
      taper(whiteEval->bishops[MG] - blackEval->bishops[MG], whiteEval->bishops[EG] - blackEval->bishops[EG], phase));
  printf("| rooks      | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->rooks[MG], whiteEval->rooks[EG],
         taper(whiteEval->rooks[MG], whiteEval->rooks[EG], phase), blackEval->rooks[MG], blackEval->rooks[EG],
         taper(blackEval->rooks[MG], blackEval->rooks[EG], phase), whiteEval->rooks[MG] - blackEval->rooks[MG],
         whiteEval->rooks[EG] - blackEval->rooks[EG],
         taper(whiteEval->rooks[MG] - blackEval->rooks[MG], whiteEval->rooks[EG] - blackEval->rooks[EG], phase));
  printf("| queens     | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->queens[MG], whiteEval->queens[EG],
         taper(whiteEval->queens[MG], whiteEval->queens[EG], phase), blackEval->queens[MG], blackEval->queens[EG],
         taper(blackEval->queens[MG], blackEval->queens[EG], phase), whiteEval->queens[MG] - blackEval->queens[MG],
         whiteEval->queens[EG] - blackEval->queens[EG],
         taper(whiteEval->queens[MG] - blackEval->queens[MG], whiteEval->queens[EG] - blackEval->queens[EG], phase));
  printf("| kings      | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->kings[MG], whiteEval->kings[EG],
         taper(whiteEval->kings[MG], whiteEval->kings[EG], phase), blackEval->kings[MG], blackEval->kings[EG],
         taper(blackEval->kings[MG], blackEval->kings[EG], phase), whiteEval->kings[MG] - blackEval->kings[MG],
         whiteEval->kings[EG] - blackEval->kings[EG],
         taper(whiteEval->kings[MG] - blackEval->kings[MG], whiteEval->kings[EG] - blackEval->kings[EG], phase));
  printf("| mobility   | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->mobility[MG], whiteEval->mobility[EG],
         taper(whiteEval->mobility[MG], whiteEval->mobility[EG], phase), blackEval->mobility[MG],
         blackEval->mobility[EG], taper(blackEval->mobility[MG], blackEval->mobility[EG], phase),
         whiteEval->mobility[MG] - blackEval->mobility[MG], whiteEval->mobility[EG] - blackEval->mobility[EG],
         taper(whiteEval->mobility[MG] - blackEval->mobility[MG], whiteEval->mobility[EG] - blackEval->mobility[EG],
               phase));
  printf(
      "| threats    | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->threats[MG], whiteEval->threats[EG],
      taper(whiteEval->threats[MG], whiteEval->threats[EG], phase), blackEval->threats[MG], blackEval->threats[EG],
      taper(blackEval->threats[MG], blackEval->threats[EG], phase), whiteEval->threats[MG] - blackEval->threats[MG],
      whiteEval->threats[EG] - blackEval->threats[EG],
      taper(whiteEval->threats[MG] - blackEval->threats[MG], whiteEval->threats[EG] - blackEval->threats[EG], phase));
  printf("| kingSafety | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->kingSafety[MG],
         whiteEval->kingSafety[EG], taper(whiteEval->kingSafety[MG], whiteEval->kingSafety[EG], phase),
         blackEval->kingSafety[MG], blackEval->kingSafety[EG],
         taper(blackEval->kingSafety[MG], blackEval->kingSafety[EG], phase),
         whiteEval->kingSafety[MG] - blackEval->kingSafety[MG], whiteEval->kingSafety[EG] - blackEval->kingSafety[EG],
         taper(whiteEval->kingSafety[MG] - blackEval->kingSafety[MG],
               whiteEval->kingSafety[EG] - blackEval->kingSafety[EG], phase));
  printf("|------------|-------------------|-------------------|-------------------|\n");
  printf("\nResult (white): %5d\n\n", toScore(whiteEval, board) - toScore(blackEval, board));
}
#else
void PrintEvaluation(Board* board) {
  EvalData whiteEval[1];
  EvalData blackEval[1];

  EvaluateSide(board, WHITE, whiteEval);
  EvaluateSide(board, BLACK, blackEval);

  EvaluateThreats(board, WHITE, whiteEval, blackEval);
  EvaluateThreats(board, BLACK, blackEval, whiteEval);

  EvaluateKingSafety(board, WHITE, whiteEval, blackEval);
  EvaluateKingSafety(board, BLACK, blackEval, whiteEval);

  int phase = getPhase(board);

  printf("|            | WHITE mg,eg,taper | BLACK mg,eg,taper | DIFF  mg,eg,taper |\n");
  printf("|------------|-------------------|-------------------|-------------------|\n");
  printf("| material   | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval->material[MG],
         whiteEval->material[EG], taper(whiteEval->material[MG], whiteEval->material[EG], phase),
         blackEval->material[MG], blackEval->material[EG],
         taper(blackEval->material[MG], blackEval->material[EG], phase),
         whiteEval->material[MG] - blackEval->material[MG], whiteEval->material[EG] - blackEval->material[EG],
         taper(whiteEval->material[MG] - blackEval->material[MG], whiteEval->material[EG] - blackEval->material[EG],
               phase));
  printf("| pawns      | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval->pawns[MG],
         whiteEval->pawns[EG], taper(whiteEval->pawns[MG], whiteEval->pawns[EG], phase), blackEval->pawns[MG],
         blackEval->pawns[EG], taper(blackEval->pawns[MG], blackEval->pawns[EG], phase),
         whiteEval->pawns[MG] - blackEval->pawns[MG], whiteEval->pawns[EG] - blackEval->pawns[EG],
         taper(whiteEval->pawns[MG] - blackEval->pawns[MG], whiteEval->pawns[EG] - blackEval->pawns[EG], phase));
  printf(
      "| knights    | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval->knights[MG],
      whiteEval->knights[EG], taper(whiteEval->knights[MG], whiteEval->knights[EG], phase), blackEval->knights[MG],
      blackEval->knights[EG], taper(blackEval->knights[MG], blackEval->knights[EG], phase),
      whiteEval->knights[MG] - blackEval->knights[MG], whiteEval->knights[EG] - blackEval->knights[EG],
      taper(whiteEval->knights[MG] - blackEval->knights[MG], whiteEval->knights[EG] - blackEval->knights[EG], phase));
  printf(
      "| bishops    | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval->bishops[MG],
      whiteEval->bishops[EG], taper(whiteEval->bishops[MG], whiteEval->bishops[EG], phase), blackEval->bishops[MG],
      blackEval->bishops[EG], taper(blackEval->bishops[MG], blackEval->bishops[EG], phase),
      whiteEval->bishops[MG] - blackEval->bishops[MG], whiteEval->bishops[EG] - blackEval->bishops[EG],
      taper(whiteEval->bishops[MG] - blackEval->bishops[MG], whiteEval->bishops[EG] - blackEval->bishops[EG], phase));
  printf("| rooks      | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval->rooks[MG],
         whiteEval->rooks[EG], taper(whiteEval->rooks[MG], whiteEval->rooks[EG], phase), blackEval->rooks[MG],
         blackEval->rooks[EG], taper(blackEval->rooks[MG], blackEval->rooks[EG], phase),
         whiteEval->rooks[MG] - blackEval->rooks[MG], whiteEval->rooks[EG] - blackEval->rooks[EG],
         taper(whiteEval->rooks[MG] - blackEval->rooks[MG], whiteEval->rooks[EG] - blackEval->rooks[EG], phase));
  printf("| queens     | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval->queens[MG],
         whiteEval->queens[EG], taper(whiteEval->queens[MG], whiteEval->queens[EG], phase), blackEval->queens[MG],
         blackEval->queens[EG], taper(blackEval->queens[MG], blackEval->queens[EG], phase),
         whiteEval->queens[MG] - blackEval->queens[MG], whiteEval->queens[EG] - blackEval->queens[EG],
         taper(whiteEval->queens[MG] - blackEval->queens[MG], whiteEval->queens[EG] - blackEval->queens[EG], phase));
  printf("| kings      | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval->kings[MG],
         whiteEval->kings[EG], taper(whiteEval->kings[MG], whiteEval->kings[EG], phase), blackEval->kings[MG],
         blackEval->kings[EG], taper(blackEval->kings[MG], blackEval->kings[EG], phase),
         whiteEval->kings[MG] - blackEval->kings[MG], whiteEval->kings[EG] - blackEval->kings[EG],
         taper(whiteEval->kings[MG] - blackEval->kings[MG], whiteEval->kings[EG] - blackEval->kings[EG], phase));
  printf("| mobility   | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval->mobility[MG],
         whiteEval->mobility[EG], taper(whiteEval->mobility[MG], whiteEval->mobility[EG], phase),
         blackEval->mobility[MG], blackEval->mobility[EG],
         taper(blackEval->mobility[MG], blackEval->mobility[EG], phase),
         whiteEval->mobility[MG] - blackEval->mobility[MG], whiteEval->mobility[EG] - blackEval->mobility[EG],
         taper(whiteEval->mobility[MG] - blackEval->mobility[MG], whiteEval->mobility[EG] - blackEval->mobility[EG],
               phase));
  printf(
      "| threats    | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval->threats[MG],
      whiteEval->threats[EG], taper(whiteEval->threats[MG], whiteEval->threats[EG], phase), blackEval->threats[MG],
      blackEval->threats[EG], taper(blackEval->threats[MG], blackEval->threats[EG], phase),
      whiteEval->threats[MG] - blackEval->threats[MG], whiteEval->threats[EG] - blackEval->threats[EG],
      taper(whiteEval->threats[MG] - blackEval->threats[MG], whiteEval->threats[EG] - blackEval->threats[EG], phase));
  printf("| kingSafety | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval->kingSafety[MG],
         whiteEval->kingSafety[EG], taper(whiteEval->kingSafety[MG], whiteEval->kingSafety[EG], phase),
         blackEval->kingSafety[MG], blackEval->kingSafety[EG],
         taper(blackEval->kingSafety[MG], blackEval->kingSafety[EG], phase),
         whiteEval->kingSafety[MG] - blackEval->kingSafety[MG], whiteEval->kingSafety[EG] - blackEval->kingSafety[EG],
         taper(whiteEval->kingSafety[MG] - blackEval->kingSafety[MG],
               whiteEval->kingSafety[EG] - blackEval->kingSafety[EG], phase));
  printf("|------------|-------------------|-------------------|-------------------|\n");
  printf("\nResult (white): %5.0f\n\n", toScore(whiteEval, board) - toScore(blackEval, board));
}
#endif
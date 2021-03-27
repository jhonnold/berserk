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

int MAX_PHASE = 24;
int PHASE_MULTIPLIERS[12] = {0, 0, 1, 1, 1, 1, 2, 2, 4, 4, 0, 0};

Score MATERIAL_VALUES[7][2] = {
    {100, 170}, {570, 543}, {582, 533}, {738, 954}, {1464, 1755}, {30000, 30000}, {0, 0},
};

// clang-format off
Score PAWN_PSQT[32][2] = {
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
  { -14,  73}, {  23,  60}, {   3,  33}, { -12,  13}, 
  { -19,  77}, {  -3,  58}, {  29,  18}, {  19, -17}, 
  { -21,  39}, { -15,  20}, { -12,   0}, {   3, -15}, 
  { -28,  23}, { -26,  15}, { -17,  -3}, { -13,  -8}, 
  { -27,  12}, { -23,  16}, { -23,  12}, { -18,  11}, 
  { -31,  13}, {  -3,  16}, { -16,  19}, { -22,  12}, 
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
};

Score KNIGHT_PSQT[32][2] = {
  {-110, -57}, { -29, -37}, { -34, -15}, {  -2, -17}, 
  { -19, -22}, { -25,  10}, {  28,  -6}, {  16,  10}, 
  { -32, -34}, {  -9,  -6}, {  10,  15}, {  33,   4}, 
  {  24, -22}, {   2, -11}, {   9,   4}, {   3,  16}, 
  {  -6, -12}, {  20, -13}, {   8,  10}, {   3,  17}, 
  { -24, -33}, { -18, -23}, { -15, -17}, {   3,   6}, 
  { -22, -30}, { -12,   0}, { -23, -24}, { -14,  -3}, 
  { -57, -37}, { -26, -30}, { -47,   0}, { -16,  -7},
};

Score KNIGHT_POST_PSQT[32][2] = {
  {   2,   2}, {  -1,  -1}, {   1,   5}, {  -2,  -2}, 
  {  -5,  -5}, {   3,  13}, {  -2,  -9}, {   1,  -1}, 
  {  16,  21}, {  28,  15}, {  21,  12}, {  48,  18}, 
  {  -4, -12}, {  28,   9}, {  48,  29}, {  46,  40}, 
  {   8,   6}, {   9,  -7}, {  24,   8}, {  40,  18}, 
  {  -8, -11}, {  -5,  14}, {  -7,  18}, {  -4,   9}, 
  { -13, -19}, {   2,  16}, { -11,  24}, { -12,  24}, 
  {   1,   8}, {   6,  15}, {   5,  11}, {   5,  18},
};

Score BISHOP_PSQT[32][2] = {
  { -17, -13}, {  -1,  -3}, { -31,  -5}, { -27,  -2}, 
  { -42,  12}, { -18,   4}, { -15,   5}, { -10,  -8}, 
  {  -2,  -2}, {  18,  -1}, {  27,   4}, { -11,  -7}, 
  { -24, -18}, { -11,  -2}, {  -7,  -8}, {   0,   9}, 
  {   1, -28}, {   0, -19}, { -16,   3}, {   9,  -2}, 
  { -10,  -3}, {   5, -24}, {  -5,  -7}, {  -7,   7}, 
  {  -3, -25}, {   1, -24}, {  -5, -20}, { -15,  -2}, 
  { -13, -16}, {   4,   7}, { -24,   0}, { -19,  -8}, 
};

Score ROOK_PSQT[32][2] = {
  {  26,   9}, {  27,  20}, {  21,  29}, {  31,  26}, 
  {  -6, -13}, { -32,  -4}, {   8, -19}, {  10, -16}, 
  {  12,  16}, {  43,   3}, {  38,  14}, {  44,  11}, 
  {  -2,  21}, {   9,  18}, {  33,   7}, {  42,  11}, 
  { -24,   6}, {  -3,   3}, { -15,  12}, { -11,  13}, 
  { -44, -11}, { -22,  -8}, { -24,  -9}, { -10, -14}, 
  { -29, -20}, {  -4, -28}, { -13, -22}, {  -4, -23}, 
  { -16, -23}, {  -9, -25}, {  -8, -17}, {   0, -28}, 
};

Score QUEEN_PSQT[32][2] = {
  {  21,  -2}, {  23,   4}, {  23,  10}, {  26,  11}, 
  {   1, -21}, { -35,  -8}, {   0,   8}, { -14,  18}, 
  {   4, -15}, {   9,   4}, {  11,  12}, {  -1,  32}, 
  {  -2,  20}, { -23,  33}, { -16,  18}, { -17,  41}, 
  { -12, -10}, {  -2,  28}, { -25,  25}, { -28,  48}, 
  { -13, -12}, {  -8, -19}, { -23,  13}, { -21,   3}, 
  { -25, -31}, { -15, -51}, {  -2, -41}, { -12, -13}, 
  {  -7, -41}, { -32, -42}, { -23, -53}, { -10, -60}, 
};

Score KING_PSQT[32][2] = {
  { -45, -83}, { -21, -37}, { -35, -12}, { -64, -26}, 
  { -25, -38}, {   0,  13}, { -16,  34}, { -33,  34}, 
  { -13, -25}, {  16,  26}, {   4,  47}, { -17,  35}, 
  { -22, -24}, {  15,   8}, {   8,  37}, { -14,  41}, 
  { -42, -49}, {  -8, -20}, { -12,  10}, { -36,  32}, 
  { -39, -51}, {  24, -33}, {   2,  -9}, { -23,   7}, 
  {  -1, -63}, {  35,  48}, { -12, -23}, { -31, -12}, 
  { -11,-101}, {  56, -81}, {  16, -60}, {  25, -81}, 
};

// clang-format on

Score KNIGHT_MOBILITIES[9][2] = {
    {-99, -177}, {-96, -108}, {-89, -40}, {-83, -7}, {-74, 5}, {-70, 24}, {-63, 21}, {-56, 22}, {-43, 9},
};

Score BISHOP_MOBILITIES[14][2] = {
    {-91, -156}, {-94, -60}, {-82, -34}, {-80, 1},  {-73, 26}, {-67, 41}, {-61, 51},
    {-61, 58},   {-54, 61},  {-56, 69},  {-42, 58}, {-9, 52},  {2, 57},   {56, 37},
};

Score ROOK_MOBILITIES[15][2] = {
    {-47, -125}, {-76, -72}, {-73, -35}, {-78, -41}, {-77, -15}, {-80, 10}, {-82, 23}, {-86, 27},
    {-78, 35},   {-75, 42},  {-69, 50},  {-58, 50},  {-53, 51},  {-39, 54}, {-5, 40},
};

Score QUEEN_MOBILITIES[28][2] = {{-10, -75}, {-10, -52}, {-71, -50}, {-84, -65}, {-83, -77}, {-81, -38}, {-82, -5},
                                 {-78, 17},  {-75, 15},  {-70, 25},  {-66, 36},  {-66, 58},  {-60, 48},  {-56, 46},
                                 {-53, 64},  {-51, 65},  {-46, 64},  {-43, 70},  {-37, 58},  {-22, 38},  {-22, 47},
                                 {-15, 46},  {3, 30},    {6, 25},    {5, 26},    {-6, 44},   {-1, 44},   {0, 48}};

Score BISHOP_PAIR[2] = {20, 108};
Score DOUBLED_PAWN[2] = {-3, -50};
Score OPPOSED_ISOLATED_PAWN[2] = {-9, 4};
Score OPEN_ISOLATED_PAWN[2] = {-16, -16};
Score BACKWARDS_PAWN[2] = {-10, -11};
Score DEFENDED_PAWN[2] = {7, 1};
Score CONNECTED_PAWN[8][2] = {
    {0, 0}, {58, 32}, {47, 13}, {15, 17}, {8, 6}, {4, 3}, {-1, 1}, {0, 0},
};

Score PASSED_PAWN[8][2] = {
    {0, 0}, {185, 225}, {79, 146}, {32, 88}, {1, 51}, {-1, 16}, {14, 12}, {0, 0},
};

Score DEFENDED_MINOR[2] = {-2,
                           8}; // intriguing subtraction, probably coming from the knight post psqt over compensating?

Score ROOK_OPEN_FILE[2] = {24, 11};
Score ROOK_SEMI_OPEN[2] = {0, 13};
Score ROOK_SEVENTH_RANK[2] = {24, 24};
Score ROOK_OPPOSITE_KING[2] = {52, -43};
Score ROOK_ADJACENT_KING[2] = {7, -22}; // interesting EG value

Score ROOK_TRAPPED[2] = {-64, -25};
Score BISHOP_TRAPPED[2] = {-145, -144};

Score KNIGHT_THREATS[6][2] = {{-6, 27}, {-10, -10}, {28, 32}, {56, 3}, {18, 40}, {86, 30}};
Score BISHOP_THREATS[6][2] = {{2, 23}, {17, 44}, {-1, 5}, {29, 18}, {13, 66}, {81, 78}};
Score ROOK_THREATS[6][2] = {{6, 25}, {12, 38}, {23, 44}, {-5, 1}, {46, 41}, {87, 39}};
Score KING_THREATS[6][2] = {{83, 84}, {-36, 29}, {42, 11}, {-15, 4}, {-22, 28}, {15, 60}};
Score HANGING_THREAT[2] = {11, 14};

// This honestly looks like a useless table
Score PAWN_SHELTER[2][8][2] = {
    {{-82, 49}, {-16, 1}, {-53, 43}, {-53, 15}, {-55, 3}, {-50, 57}, {-46, 91}, {0, 0}},
    {{-122, -43}, {-99, -75}, {-78, -34}, {-89, -99}, {-86, -63}, {-79, -18}, {-78, 15}, {0, 0}},
};
Score PAWN_STORM[8][2] = {{0, 0}, {0, 0}, {9, -31}, {10, -25}, {6, -37}, {-23, -80}, {121, 74}, {0, 0}};

Score KS_ATTACKER_WEIGHTS[5] = {0, 81, 75, 72, 21};
Score KS_ATTACK = 16;
Score KS_WEAK_SQS = 50;
Score KS_SAFE_CHECK = 88;
Score KS_UNSAFE_CHECK = 32;
Score KS_ENEMY_QUEEN = -150;
Score KS_ALLIES = -35;

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

// TODO: Convert this to use board->pieceCounts?
inline int getPhase(Board* board) {
  int currentPhase = 0;
  for (int i = 2; i < 10; i++)
    currentPhase += PHASE_MULTIPLIERS[i] * bits(board->pieces[i]);
  currentPhase = MAX_PHASE - currentPhase;

  return ((currentPhase << 8) + (MAX_PHASE / 2)) / MAX_PHASE;
}

inline Score taper(Score mg, Score eg, int phase) { return (mg * (256 - phase) + (eg * phase)) / 256; }

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

  // PAWNS
  data->attacks[PAWN_TYPE] = pawnAttacks;
  data->allAttacks = pawnAttacks;
  data->attacks2 = shift(myPawns, PAWN_DIRECTIONS[side] + E) & shift(myPawns, PAWN_DIRECTIONS[side] + W);

  for (BitBoard pawns = board->pieces[PAWN[side]]; pawns; popLsb(pawns)) {
    BitBoard sqBitboard = pawns & -pawns;
    int sq = lsb(pawns);

    data->material[MG] += MATERIAL_VALUES[PAWN_TYPE][MG];
    data->material[EG] += MATERIAL_VALUES[PAWN_TYPE][EG];
    data->pawns[MG] += PSQT[PAWN[side]][sq][MG];
    data->pawns[EG] += PSQT[PAWN[side]][sq][EG];

    int file = file(sq);
    int rank = rank(sq);

    BitBoard opposed = board->pieces[PAWN[xside]] & FILE_MASKS[file] & FORWARD_RANK_MASKS[side][rank];
    BitBoard doubled = board->pieces[PAWN[side]] & shift(sqBitboard, PAWN_DIRECTIONS[xside]);
    BitBoard neighbors = board->pieces[PAWN[side]] & ADJACENT_FILE_MASKS[file];
    BitBoard connected = neighbors & RANK_MASKS[rank];
    BitBoard defenders = board->pieces[PAWN[side]] & getPawnAttacks(sq, xside);
    BitBoard forwardLever = board->pieces[PAWN[xside]] & getPawnAttacks(sq + PAWN_DIRECTIONS[side], side);

    int backwards = !(neighbors & FORWARD_RANK_MASKS[xside][rank(sq + PAWN_DIRECTIONS[side])]) && forwardLever;
    int passed =
        !(board->pieces[PAWN[xside]] & FORWARD_RANK_MASKS[side][rank] & (ADJACENT_FILE_MASKS[file] | FILE_MASKS[file]));

    if (doubled) {
      data->pawns[MG] += DOUBLED_PAWN[MG];
      data->pawns[EG] += DOUBLED_PAWN[EG];
    }

    if (!neighbors) {
      data->pawns[MG] += opposed ? OPPOSED_ISOLATED_PAWN[MG] : OPEN_ISOLATED_PAWN[MG];
      data->pawns[EG] += opposed ? OPPOSED_ISOLATED_PAWN[EG] : OPEN_ISOLATED_PAWN[EG];
    }

    if (backwards) {
      data->pawns[MG] += BACKWARDS_PAWN[MG];
      data->pawns[EG] += BACKWARDS_PAWN[EG];
    }

    int adjustedRank = side ? 7 - rank : rank;
    if (passed) {
      data->pawns[MG] += PASSED_PAWN[adjustedRank][MG];
      data->pawns[EG] += PASSED_PAWN[adjustedRank][EG];
    }

    if (defenders | connected) {
      int s = 2;
      if (connected)
        s++;
      if (opposed)
        s--;

      data->pawns[MG] += CONNECTED_PAWN[adjustedRank][MG] * s + DEFENDED_PAWN[MG] * bits(defenders);
      data->pawns[EG] += CONNECTED_PAWN[adjustedRank][EG] * s + DEFENDED_PAWN[EG] * bits(defenders);
    }
  }

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
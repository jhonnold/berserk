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
    {100, 184}, {548, 525}, {569, 517}, {725, 925}, {1487, 1756}, {30000, 30000}, {0, 0},
};

// clang-format off
Score PAWN_PSQT[32][2] = {
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
  {   4,  32}, {   9,  26}, {   1,  14}, {  -1,  -1}, 
  {   5,  29}, {  13,  25}, {  25,  -1}, {  -4, -29}, 
  {  -9,  16}, {   8,  -1}, {  19, -19}, {  29, -29}, 
  { -25,  -2}, { -18,  -9}, {   1, -22}, {  10, -23}, 
  { -21, -11}, {  -8, -13}, {  -2, -17}, {  -3, -11}, 
  { -25,  -9}, {  16, -12}, {   5,  -4}, {  -8,   0}, 
  {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
};

Score KNIGHT_PSQT[32][2] = {
  { -95, -61}, { -35, -52}, { -37, -26}, {  -8, -30}, 
  { -40, -41}, { -30,  -8}, {  31, -22}, {   8,   1}, 
  { -26, -39}, {   8, -23}, {  11,  11}, {  19,   9}, 
  {  13, -13}, {  16,  -4}, {  12,  18}, {  21,  24}, 
  {   1,  -8}, {  20,  -9}, {  17,  16}, {  17,  20}, 
  { -23, -13}, {  -2, -16}, {   0,  -5}, {   8,  18}, 
  { -14, -30}, { -22, -12}, {  -6, -10}, {   5,   1}, 
  { -38, -24}, { -11, -26}, { -31,  -8}, {  -4,  -4},
};

Score KNIGHT_POST_PSQT[32][2] = {
  {   2,   2}, {  -2,  -3}, {   0,   3}, {  -2,  -1}, 
  {  -5,  -4}, {  -2,   6}, {  -6, -18}, {  -3,   2}, 
  {  10,  19}, {  49,  23}, {  37,  24}, {  41,  20}, 
  {  22,   9}, {  45,   3}, {  37,  18}, {  56,  17}, 
  {  22,  10}, {  28,  11}, {  38,  16}, {  40,  32}, 
  {   6, -10}, {  -6,  18}, {   4,  19}, {   6,  17}, 
  {   8,  -8}, {   7,  17}, {  -7,  21}, { -11,  26}, 
  {   4,  14}, {  16,   7}, {   3,   3}, {   1,  14},
};

Score BISHOP_PSQT[32][2] = {
  { -24, -15}, {  -2, -18}, { -38, -23}, { -29, -14}, 
  { -47,  -6}, {  -25, -7}, { -27,  -3}, { -20, -14}, 
  { -24,  -1}, {   4, -11}, {   7,  -4}, { -13, -10}, 
  { -23,   0}, { -15,  -6}, { -11,   2}, {  13,  -2}, 
  {  -6, -10}, { -16,  -9}, { -15,   2}, {  14,  -1}, 
  { -19,  -3}, {   3, -10}, {   5,  -3}, {  -3,   7}, 
  {  -5, -14}, {  23, -19}, {   7, -11}, {  -8,   2}, 
  { -34,  -9}, { -14,   7}, { -15,   8}, { -15,   3}, 
};

Score ROOK_PSQT[32][2] = {
  {   4,  15}, {  16,  14}, {   7,  16}, {  26,  17}, 
  { -15, -14}, { -14, -12}, {  10, -17}, {   5, -19}, 
  {  -4,   0}, {  16,   5}, {  15,   0}, {  18,   0}, 
  { -19,   8}, {  -8,   0}, {  21,   8}, {  22,  -3}, 
  { -32,   4}, {  -7,   2}, { -10,   5}, {   4,  -3}, 
  { -28,  -5}, {  -3,  -3}, {   4, -13}, {   2,  -7}, 
  { -33,  -1}, {   2, -10}, {   5,  -6}, {  20,  -8}, 
  {   3, -14}, {  -7,  -3}, {  15,  -7}, {  20, -11}, 
};

Score QUEEN_PSQT[32][2] = {
  {   6, -16}, {  15,  -6}, {  14,  -2}, {  18,   4}, 
  { -13, -33}, { -37, -33}, { -19, -10}, { -23,   7}, 
  {   0, -22}, {  -8, -17}, {   6,  -7}, {  -6,  25}, 
  { -23,  16}, { -26,  26}, { -23,  14}, { -28,  25}, 
  { -12,  -3}, { -22,  23}, { -20,  26}, { -25,  37}, 
  { -17,  -2}, {   4, -13}, { -14,  19}, { -11,  14}, 
  { -18, -28}, { -13, -31}, {  17, -31}, {   6,  -7}, 
  { -15, -38}, { -19, -33}, { -15, -27}, {  12, -35}, 
};

Score KING_PSQT[32][2] = {
  { -43, -71}, { -22, -42}, { -38, -21}, { -67, -33}, 
  { -23, -28}, {  -4,  -4}, { -21,  13}, { -40,  12}, 
  {  -8, -19}, {  13,  10}, {  -3,  23}, { -31,   6}, 
  { -20, -33}, {  14,  -2}, {  -2,  17}, { -23,  19}, 
  { -29, -46}, {  -8, -19}, { -22,  12}, { -46,  24}, 
  { -12, -39}, {  12, -15}, { -15,   6}, { -32,  18}, 
  {  31, -52}, {  45, -31}, {  -7,  -3}, { -32,   5}, 
  {  13, -95}, {  62, -64}, {  23, -39}, {  22, -47}, 
};

// clang-format on

Score KNIGHT_MOBILITIES[9][2] = {
    {-79, -164}, {-66, -115}, {-57, -39}, {-53, -5}, {-45, 9}, {-42, 27}, {-34, 23}, {-26, 22}, {-13, 10},
};

Score BISHOP_MOBILITIES[14][2] = {
    {-62, -150}, {-70, -61}, {-53, -40}, {-54, -2}, {-48, 25}, {-40, 43}, {-32, 54},
    {-32, 61},   {-25, 65},  {-30, 75},  {-12, 65}, {13, 63},  {13, 67},  {63, 48},
};

Score ROOK_MOBILITIES[15][2] = {
    {-38, -113}, {-47, -73}, {-42, -33}, {-46, -38}, {-46, -12}, {-51, 19}, {-53, 29}, {-56, 34},
    {-48, 42},   {-45, 50},  {-40, 58},  {-29, 56},  {-24, 57},  {-12, 58}, {12, 50},
};

Score QUEEN_MOBILITIES[28][2] = {{-10, -75}, {-10, -52}, {-49, -47}, {-66, -57}, {-64, -70}, {-63, -37}, {-63, -3},
                                 {-58, 16},  {-55, 7},   {-49, 23},  {-44, 34},  {-44, 54},  {-37, 45},  {-34, 43},
                                 {-29, 62},  {-27, 63},  {-21, 65},  {-21, 70},  {-18, 58},  {-4, 42},   {-7, 52},
                                 {-2, 54},   {13, 38},   {12, 34},   {10, 34},   {-3, 50},   {0, 47},    {1, 50}};

Score BISHOP_PAIR[2] = {27, 111};
Score DOUBLED_PAWN[2] = {-5, -47};
Score OPPOSED_ISOLATED_PAWN[2] = {-6, 1};
Score OPEN_ISOLATED_PAWN[2] = {-14, -14};
Score BACKWARDS_PAWN[2] = {-11, -12};
Score DEFENDED_PAWN[2] = {7, 3};
Score CONNECTED_PAWN[8][2] = {
    {0, 0}, {56, 27}, {50, 11}, {13, 14}, {9, 6}, {5, 4}, {-1, 2}, {0, 0},
};

Score PASSED_PAWN[8][2] = {
    {0, 0}, {199, 247}, {102, 165}, {29, 97}, {0, 59}, {0, 24}, {14, 17}, {0, 0},
};

Score DEFENDED_MINOR[2] = {2, 7};

Score ROOK_OPEN_FILE[2] = {33, 17};
Score ROOK_SEMI_OPEN[2] = {2, 10};
Score ROOK_SEVENTH_RANK[2] = {32, 17};
Score ROOK_OPPOSITE_KING[2] = {61, -56};
Score ROOK_ADJACENT_KING[2] = {8, -29}; // interesting EG value

Score ROOK_TRAPPED[2] = {-79, -32};
Score BISHOP_TRAPPED[2] = {-147, -145};

Score KNIGHT_THREATS[6][2] = {{-3, 28}, {-15, -12}, {29, 35}, {67, -1}, {18, 41}, {86, 30}};
Score BISHOP_THREATS[6][2] = {{4, 25}, {22, 48}, {-3, 6}, {37, 17}, {18, 62}, {81, 78}};
Score ROOK_THREATS[6][2] = {{10, 36}, {16, 41}, {26, 53}, {5, 11}, {52, 40}, {87, 39}};
Score KING_THREATS[6][2] = {{93, 98}, {-39, 32}, {43, 9}, {-18, 0}, {-22, 28}, {15, 60}};
Score HANGING_THREAT[2] = {12, 15}; // what is that eg value??

Score PAWN_SHELTER[2][8][2] = {
    {{-66, 51}, {-19, -4}, {-45, 37}, {-32, 4}, {-34, 3}, {-29, 45}, {-28, 90}, {0, 0}},
    {{-98, -45}, {-95, -73}, {-60, -28}, {-60, -100}, {-58, -65}, {-50, -29}, {-50, 10}, {0, 0}},
};
Score PAWN_STORM[8][2] = {{0, 0}, {0, 0}, {12, -38}, {15, -38}, {13, -42}, {-27, -82}, {109, 72}, {0, 0}};

Score KS_ATTACKER_WEIGHTS[5] = {0, 77, 82, 72, 39};
Score KS_ATTACK = 21;
Score KS_WEAK_SQS = 48;
Score KS_SAFE_CHECK = 86;
Score KS_UNSAFE_CHECK = 29;
Score KS_ENEMY_QUEEN = -149;
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
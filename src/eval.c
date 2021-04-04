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

// clang-format off
TScore MATERIAL_VALUES[7] = {{60, 93}, {310, 310}, {323, 323}, {435, 548}, {910, 970}, {30000, 30000}, {0, 0}};

TScore PAWN_PSQT[32] = {{0,0}, {0,0}, {0,0}, {0,0}, {39,159}, {79,141}, {88,127}, {105,119}, {0,25}, {9,24}, {52,1}, {34,-9}, {-9,-2}, {10,-8}, {16,-11}, {31,-23}, {-21,-8}, {-13,-10}, {3,-15}, {15,-13}, {-13,-15}, {1,-15}, {4,-12}, {13,-6}, {-15,-14}, {15,-19}, {9,-7}, {16,-8}, {0,0}, {0,0}, {0,0}, {0,0}};
TScore KNIGHT_PSQT[32] = {{-88,-65}, {-50,-56}, {-61,-33}, {1,-47}, {-48,-35}, {-23,-23}, {29,-36}, {-4,-19}, {-12,-51}, {-1,-31}, {0,-13}, {10,-16}, {6,-34}, {6,-23}, {-3,-11}, {9,-8}, {5,-29}, {11,-27}, {14,-11}, {13,-12}, {-5,-28}, {4,-27}, {3,-18}, {9,-5}, {-3,-41}, {-3,-28}, {0,-21}, {6,-16}, {-26,-29}, {2,-38}, {-9,-29}, {1,-26}};
TScore BISHOP_PSQT[32] = {{-7,-31}, {0,-28}, {-66,-23}, {-60,-18}, {-49,-15}, {-26,-13}, {-16,-13}, {-23,-21}, {-27,-14}, {-6,-17}, {-3,-12}, {-3,-22}, {-19,-17}, {-10,-15}, {-13,-10}, {5,-10}, {0,-26}, {-10,-25}, {-11,-13}, {1,-11}, {-8,-21}, {-1,-22}, {2,-16}, {-4,-11}, {-6,-31}, {13,-30}, {1,-24}, {-9,-15}, {-22,-20}, {-10,-16}, {-4,-19}, {-10,-18}};
TScore ROOK_PSQT[32] = {{-23,-17}, {-17,-17}, {-52,-6}, {-3,-16}, {-1,-24}, {7,-24}, {33,-33}, {33,-43}, {0,-31}, {30,-34}, {33,-40}, {27,-37}, {-6,-21}, {-1,-29}, {21,-26}, {21,-33}, {-23,-16}, {2,-23}, {-3,-20}, {4,-24}, {-23,-19}, {-4,-24}, {1,-29}, {0,-25}, {-24,-16}, {0,-29}, {0,-22}, {11,-25}, {1,-25}, {-5,-15}, {4,-17}, {5,-19}};
TScore QUEEN_PSQT[32] = {{6,-71}, {-18,-46}, {31,-76}, {8,-57}, {-47,-49}, {-85,-12}, {-30,-28}, {-68,-8}, {-37,-39}, {-41,-29}, {-16,-38}, {-33,-1}, {-64,1}, {-60,8}, {-67,2}, {-57,1}, {-42,-26}, {-56,1}, {-53,4}, {-61,12}, {-50,-10}, {-34,-32}, {-57,3}, {-47,-11}, {-45,-37}, {-38,-50}, {-27,-50}, {-38,-25}, {-42,-38}, {-48,-39}, {-45,-44}, {-32,-55}};
TScore KING_PSQT[32] = {{-95,-82}, {27,-81}, {15,-59}, {-23,-63}, {-23,-48}, {44,-43}, {46,-31}, {88,-45}, {-9,-47}, {111,-43}, {120,-37}, {83,-45}, {-66,-41}, {50,-36}, {47,-25}, {36,-22}, {-97,-45}, {19,-42}, {4,-18}, {-20,-7}, {-47,-46}, {4,-35}, {0,-21}, {0,-13}, {-14,-67}, {-8,-42}, {-37,-22}, {-72,-9}, {-38,-95}, {-6,-69}, {-34,-44}, {-45,-47}};
TScore BISHOP_PAIR = {36,38};
TScore BISHOP_TRAPPED = {-97,-92};
TScore KNIGHT_POST_PSQT[32] = {{0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {10,21}, {42,10}, {23,10}, {19,23}, {24,1}, {43,6}, {32,15}, {40,15}, {22,0}, {22,13}, {25,12}, {31,23}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}};
TScore KNIGHT_MOBILITIES[9] = {{-39,-93}, {-35,-45}, {-24,-26}, {-20,-21}, {-10,-21}, {-5,-15}, {0,-19}, {6,-22}, {11,-31}};
TScore BISHOP_MOBILITIES[14] = {{-51,-50}, {-33,-55}, {-22,-37}, {-19,-25}, {-9,-19}, {-4,-13}, {0,-10}, {0,-11}, {0,-7}, {2,-10}, {10,-17}, {16,-16}, {-6,1}, {12,-12}};
TScore ROOK_MOBILITIES[15] = {{-45,-99}, {-46,-72}, {-40,-53}, {-34,-51}, {-37,-30}, {-37,-22}, {-34,-20}, {-32,-20}, {-27,-22}, {-21,-22}, {-16,-23}, {-13,-24}, {-11,-22}, {-13,-20}, {3,-31}};
TScore QUEEN_MOBILITIES[28] = {{0,0}, {-102,-50}, {-60,-200}, {-67,-170}, {-69,-63}, {-62,-55}, {-64,-64}, {-60,-35}, {-58,-20}, {-56,-15}, {-57,-5}, {-59,3}, {-57,-1}, {-60,9}, {-57,13}, {-56,6}, {-56,5}, {-61,9}, {-67,11}, {-30,-30}, {-41,-31}, {-38,-27}, {-6,-82}, {-9,-80}, {-1,-99}, {-3,-123}, {-51,-83}, {-34,-118}};
TScore DOUBLED_PAWN = {-4,-17};
TScore OPPOSED_ISOLATED_PAWN = {-13,-6};
TScore OPEN_ISOLATED_PAWN = {-14,-6};
TScore BACKWARDS_PAWN = {-10,-6};
TScore DEFENDED_PAWN = {0,0};
TScore CONNECTED_PAWN[8] = {{0,0}, {23,2}, {21,11}, {9,5}, {6,0}, {1,0}, {0,0}, {0,0}};
TScore PASSED_PAWN[8] = {{0,0}, {41,72}, {52,107}, {25,62}, {15,34}, {16,21}, {14,20}, {0,0}};
TScore PASSED_PAWN_ADVANCE_DEFENDED = {7,24};
TScore PASSED_PAWN_EDGE_DISTANCE = {-9,-6};
TScore ROOK_OPEN_FILE = {25,7};
TScore ROOK_SEMI_OPEN = {5,11};
TScore ROOK_SEVENTH_RANK = {3,3};
TScore ROOK_OPPOSITE_KING = {22,-2};
TScore ROOK_ADJACENT_KING = {3,-21};
TScore ROOK_TRAPPED = {-50,-11};
TScore KNIGHT_THREATS[6] = {{0,18}, {-4,7}, {35,26}, {59,7}, {39,-24}, {100,39}};
TScore BISHOP_THREATS[6] = {{0,12}, {21,30}, {2,10}, {47,14}, {43,51}, {100,75}};
TScore ROOK_THREATS[6] = {{-2,18}, {21,24}, {21,36}, {6,-8}, {76,13}, {100,40}};
TScore KING_THREATS[6] = {{24,62}, {15,39}, {-22,41}, {13,18}, {-100,-100}, {0,0}};
TScore TEMPO = {23,0};
//clang-format on

Score PAWN_SHELTER[4][8] = {
  {-2, 12, 12, 15, 22, 38, 34, 0},
  {-18, -18, -5, -12, -21, 14, 28, 0},
  {-5, -15, 3, 13, -2, 10, 32, 0},
  {-16, -50, -24, -16, -21, -14, -5, 0},
};

Score PAWN_STORM[4][8] = {
  {-36, -20, -20, -20, -40, 70, 119, 0 },
  {-18, -9, 4, -14, -20, -50, 10, 0 },
  {3, 4, 5, 1, -14, -65, -21, 0 },
  {6, 12, 6, -2, 0, -45, 5, 0 },
};

Score BLOCKED_PAWN_STORM[8] = {
  0, 0, 0, 1, 3, -31, 0, 0
};


Score KS_ATTACKER_WEIGHTS[5] = {0, 28, 18, 15, 4};
Score KS_SAFE_CHECKS[5][2] = {{0, 0}, {278, 447}, {221, 337}, {376, 650}, {263, 392}};
Score KS_ATTACK = 24;
Score KS_WEAK_SQS = 63;
Score KS_UNSAFE_CHECK = 51;
Score KS_ENEMY_QUEEN = -300;
Score KS_KNIGHT_PROTECTOR = -35;
Score KS_DISTANCE_DEFENSE = -3;

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
  data->tempo[MG] = 0;
  data->tempo[EG] = 0;

  data->attacks2 = 0;
  data->allAttacks = 0;
  data->attackCount = 0;
  data->attackWeight = 0;
  data->attackers = 0;
  memset(data->attacks, 0, sizeof(data->attacks));

  int xside = 1 - side;

  if (side == board->side) {
    data->tempo[MG] += TEMPO[MG];
    data->tempo[EG] += TEMPO[EG];
  }

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
                      (pawnAttacks | shift(myPawns | opponentPawns, PAWN_DIRECTIONS[xside])) & 
                      (RANK_4 | RANK_5 | THIRD_RANKS[xside]);
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
}


void EvaluateKingSafety(Board* board, int side, EvalData* data, EvalData* enemyData) {
  data->kingSafety[MG] = 0;
  data->kingSafety[EG] = 0;

  int xside = 1 - side;
  int kingSq = lsb(board->pieces[KING[side]]);
  int kingFile = file(kingSq);

  int leftMostFile, rightMostFile;
  switch (kingFile) {
  case 0:
    leftMostFile = 0;
    rightMostFile = 2;
    break;
  case 7:
    leftMostFile = 5;
    rightMostFile = 7;
    break;
  default:
    leftMostFile = kingFile - 1;
    rightMostFile = kingFile + 1;
    break;
  }

  int shelterScore = 0;
  
  BitBoard ourPawns = board->pieces[PAWN[side]] & ~enemyData->attacks[PAWN_TYPE] & ~FORWARD_RANK_MASKS[xside][rank(kingSq)];
  BitBoard enemyPawns = board->pieces[PAWN[xside]] & ~FORWARD_RANK_MASKS[xside][rank(kingSq)];
  for (int f = leftMostFile; f <= rightMostFile; f++) {
    int adjustedFile = f > 3 ? 7 - f : f;

    BitBoard ourPawnFile = ourPawns & FILE_MASKS[f];
    int pawnRank = ourPawnFile ? (side ? 7 - rank(lsb(ourPawnFile)) : rank(msb(ourPawnFile))) : 0;
    shelterScore += PAWN_SHELTER[adjustedFile][pawnRank];

    BitBoard enemyPawnFile = enemyPawns & FILE_MASKS[f];
    int theirRank = enemyPawnFile ? (side ? 7 - rank(lsb(enemyPawnFile)) : rank(msb(enemyPawnFile))) : 0;
    if (pawnRank && pawnRank == theirRank + 1)
      shelterScore += BLOCKED_PAWN_STORM[theirRank];
    else
      shelterScore += PAWN_STORM[adjustedFile][theirRank];
  }

  BitBoard kingArea = getKingAttacks(kingSq);
  BitBoard weak = enemyData->allAttacks & ~data->attacks2 & (~data->allAttacks | data->attacks[4] | data->attacks[5]);
  BitBoard vulnerable = (~data->allAttacks | (weak & enemyData->attacks2)) & ~board->occupancies[xside];

  int safeCheckScore = 0;
  BitBoard unsafeChecks = 0ULL;

  BitBoard possibleKnightChecks =
      getKnightAttacks(kingSq) & enemyData->attacks[KNIGHT_TYPE] & ~board->occupancies[xside];
  if (vulnerable & possibleKnightChecks)
    safeCheckScore += KS_SAFE_CHECKS[KNIGHT_TYPE][bits(vulnerable & possibleKnightChecks) > 1];
  else 
    unsafeChecks |= possibleKnightChecks;

  BitBoard possibleRookChecks =
      getRookAttacks(kingSq, board->occupancies[BOTH] ^ board->occupancies[QUEEN[side]]) & enemyData->attacks[ROOK_TYPE] & ~board->occupancies[xside];
  if (possibleRookChecks & vulnerable)
    safeCheckScore += KS_SAFE_CHECKS[ROOK_TYPE][bits(possibleRookChecks & vulnerable) > 1];
  else
    unsafeChecks |= possibleRookChecks;

  BitBoard possibleQueenChecks =
      getQueenAttacks(kingSq, board->occupancies[BOTH]) & enemyData->attacks[QUEEN_TYPE] & ~board->occupancies[xside];
  if (possibleQueenChecks & vulnerable & ~possibleRookChecks)
    safeCheckScore += KS_SAFE_CHECKS[QUEEN_TYPE][bits(possibleQueenChecks & vulnerable & ~possibleRookChecks) > 1];
  
  // ignore unsafe queen checks

  BitBoard possibleBishopChecks = getBishopAttacks(kingSq, board->occupancies[BOTH] ^ board->occupancies[QUEEN[side]]) &
                                  enemyData->attacks[BISHOP_TYPE] & ~board->occupancies[xside];
  if (possibleBishopChecks & vulnerable & ~possibleQueenChecks)
    safeCheckScore += KS_SAFE_CHECKS[BISHOP_TYPE][bits(possibleBishopChecks & vulnerable & ~possibleQueenChecks) > 1];
  else
    unsafeChecks |= possibleBishopChecks;

  int attackWeight = enemyData->attackers * enemyData->attackWeight;
  int unsafeCheckScore = KS_UNSAFE_CHECK * bits(unsafeChecks);
  int weakScore = KS_WEAK_SQS * bits(weak & kingArea);
  int sqAttackScore = KS_ATTACK * enemyData->attackCount;
  int noQueenScore = KS_ENEMY_QUEEN * !(board->pieces[QUEEN[xside]]);
  int knightProtectorScore = KS_KNIGHT_PROTECTOR * !!(data->attacks[KNIGHT_TYPE] & kingArea);
  int mobilityGapScore = (enemyData->mobility[MG] - data->mobility[MG]) / 2;

  int score = attackWeight + safeCheckScore + unsafeCheckScore + weakScore + sqAttackScore + noQueenScore + knightProtectorScore + mobilityGapScore - shelterScore / 2 + 18;

  if (score > 0) {
    data->kingSafety[MG] += -score * score / 1000;
    data->kingSafety[EG] += -score / 30;
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
             data->kings[MG] + data->kingSafety[MG] + data->material[MG] + data->mobility[MG] + data->threats[MG] +
             data->tempo[MG];
  Score eg = data->pawns[EG] + data->knights[EG] + data->bishops[EG] + data->rooks[EG] + data->queens[EG] +
             data->kings[EG] + data->kingSafety[EG] + data->material[EG] + data->mobility[EG] + data->threats[EG] +
             data->tempo[MG];

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
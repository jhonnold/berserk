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

TScore PAWN_PSQT[32] = {{0,0}, {0,0}, {0,0}, {0,0}, {56,138}, {72,130}, {71,109}, {80,104}, {-26,46}, {-14,47}, {20,22}, {13,11}, {-16,0}, {3,-8}, {9,-15}, {19,-21}, {-24,-8}, {-16,-10}, {0,-16}, {4,-13}, {-12,-16}, {4,-16}, {10,-15}, {9,-8}, {-19,-13}, {13,-17}, {6,-6}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}};
TScore KNIGHT_PSQT[32] = {{-100,-61}, {-49,-58}, {-53,-32}, {8,-45}, {-45,-40}, {-43,-18}, {16,-36}, {-9,-16}, {-22,-46}, {-2,-33}, {-1,-11}, {17,-15}, {15,-35}, {11,-24}, {11,-10}, {20,-7}, {-3,-29}, {18,-27}, {11,-9}, {12,-7}, {-14,-31}, {1,-28}, {1,-19}, {6,-5}, {-9,-46}, {-13,-29}, {-3,-25}, {3,-19}, {-34,-33}, {-9,-38}, {-15,-30}, {-2,-25}};
TScore BISHOP_PSQT[32] = {{-16,-36}, {-19,-33}, {-90,-21}, {-78,-20}, {-52,-18}, {-31,-20}, {-32,-17}, {-30,-24}, {-29,-17}, {-5,-23}, {-13,-15}, {-16,-20}, {-17,-22}, {-14,-20}, {-10,-13}, {8,-15}, {-6,-30}, {-8,-27}, {-13,-16}, {6,-16}, {-10,-25}, {0,-25}, {3,-20}, {-4,-13}, {-10,-32}, {13,-34}, {2,-26}, {-7,-20}, {-24,-27}, {-8,-19}, {-7,-19}, {-7,-21}};
TScore ROOK_PSQT[32] = {{-11,-20}, {-7,-19}, {-37,-9}, {3,-17}, {-3,-26}, {-1,-24}, {24,-34}, {30,-40}, {-12,-30}, {6,-29}, {6,-31}, {-3,-29}, {-25,-21}, {-21,-26}, {1,-24}, {-4,-27}, {-33,-23}, {-13,-26}, {-21,-22}, {-16,-23}, {-36,-26}, {-11,-28}, {-11,-33}, {-15,-27}, {-41,-20}, {-5,-35}, {-9,-29}, {-3,-28}, {-15,-33}, {-20,-26}, {-11,-27}, {-8,-30}};
TScore QUEEN_PSQT[32] = {{3,-54}, {-1,-33}, {12,-25}, {43,-44}, {-23,-39}, {-63,-9}, {-34,0}, {-63,29}, {-15,-37}, {-28,-19}, {-16,-15}, {-29,20}, {-42,-4}, {-55,14}, {-48,6}, {-55,27}, {-41,-20}, {-47,4}, {-50,4}, {-52,14}, {-42,-17}, {-29,-34}, {-45,-6}, {-46,-13}, {-46,-40}, {-35,-54}, {-24,-51}, {-34,-33}, {-38,-59}, {-45,-53}, {-43,-51}, {-30,-68}};
TScore KING_PSQT[32] = {{-45,-72}, {97,-74}, {40,-53}, {-9,-54}, {-11,-44}, {14,-35}, {0,-19}, {29,-33}, {8,-43}, {63,-34}, {42,-22}, {6,-31}, {-71,-36}, {-31,-24}, {-39,-11}, {-67,-9}, {-106,-37}, {-51,-30}, {-85,-6}, {-129,5}, {-51,-39}, {-27,-28}, {-62,-12}, {-82,-4}, {-17,-50}, {0,-40}, {-43,-21}, {-74,-11}, {-25,-81}, {15,-65}, {-12,-48}, {-15,-53}};
TScore BISHOP_PAIR = {33,42};
TScore BISHOP_TRAPPED = {-88,-83};
TScore KNIGHT_POST_PSQT[32] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{14,20}, {41,13}, {28,17}, {10,25}, {20,4}, {30,4}, {19,17}, {33,17}, {21,0}, {10,11}, {19,10}, {19,21}, {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
TScore KNIGHT_MOBILITIES[9] = {{-39,-115}, {-30,-57}, {-21,-36}, {-21,-23}, {-8,-24}, {-2,-18}, {2,-20}, {8,-21}, {14,-27}};
TScore BISHOP_MOBILITIES[14] = {{-56,-64}, {-42,-62}, {-27,-46}, {-22,-31}, {-15,-23}, {-6,-19}, {-1,-15}, {1,-13}, {4,-8}, {6,-10}, {21,-17}, {44,-18}, {27,-5}, {70,-23}};
TScore ROOK_MOBILITIES[15] = {{-58,-94}, {-56,-80}, {-49,-64}, {-45,-56}, {-47,-40}, {-47,-28}, {-44,-23}, {-43,-22}, {-38,-22}, {-30,-20}, {-22,-19}, {-11,-20}, {-1,-19}, {8,-19}, {24,-23}};
TScore QUEEN_MOBILITIES[28] = {{0,0}, {-106,-51}, {-72,-188}, {-73,-191}, {-72,-90}, {-68,-85}, {-66,-90}, {-61,-56}, {-56,-47}, {-53,-38}, {-51,-23}, {-50,-14}, {-45,-16}, {-46,0}, {-41,-1}, {-37,1}, {-38,5}, {-31,2}, {-32,6}, {-2,0}, {-6,0}, {0,6}, {28,-17}, {18,-6}, {28,-12}, {41,-29}, {-1,-6}, {33,-23}};
TScore DOUBLED_PAWN = {0,-17};
TScore OPPOSED_ISOLATED_PAWN = {-11,-5};
TScore OPEN_ISOLATED_PAWN = {-14,-6};
TScore BACKWARDS_PAWN = {-9,-8};
TScore DEFENDED_PAWN = {0,0};
TScore CONNECTED_PAWN[8] = {{0,0}, {16,14}, {13,11}, {9,4}, {5,0}, {0,0}, {0,0}, {0,0}};
TScore PASSED_PAWN[8] = {{0,0}, {34,62}, {47,75}, {19,56}, {8,29}, {14,17}, {14,13}, {0,0}};
TScore PASSED_PAWN_ADVANCE_DEFENDED = {7,20};
TScore PASSED_PAWN_EDGE_DISTANCE = {-8,-4};
TScore ROOK_OPEN_FILE = {22,3};
TScore ROOK_SEMI_OPEN = {1,7};
TScore ROOK_SEVENTH_RANK = {1,0};
TScore ROOK_OPPOSITE_KING = {38,-10};
TScore ROOK_ADJACENT_KING = {23,-17};
TScore ROOK_TRAPPED = {-41,-4};
TScore KNIGHT_THREATS[6] = {{-1,16}, {-2,1}, {34,28}, {56,10}, {43,-18}, {100,26}};
TScore BISHOP_THREATS[6] = {{1,12}, {21,28}, {4,7}, {46,15}, {46,43}, {98,78}};
TScore ROOK_THREATS[6] = {{-2,19}, {19,23}, {18,35}, {9,-6}, {77,4}, {100,41}};
TScore KING_THREATS[6] = {{3,59}, {-21,33}, {-23,38}, {-68,39}, {-100,-100}, {0,0}};
TScore PAWN_SHELTER[2][8] = {{{-33,-9}, {-13,17}, {-29,8}, {-15,-38}, {-14,-16}, {-10,0}, {-4,25}, {0,0}}, {{-34,-2}, {-60,-3}, {-44,22}, {-23,-13}, {-14,4}, {1,6}, {1,20}, {0,0}}};
TScore PAWN_STORM[8] = {{0,0}, {-4,0}, {4,-9}, {-6,4}, {-12,4}, {-42,-17}, {3,72}, {0,0}};
TScore TEMPO = {23,0};
//clang-format on

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
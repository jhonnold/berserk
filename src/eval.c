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
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "string.h"
#include "types.h"
#include "util.h"

#define S(mg, eg) (makeScore((mg), (eg)))
#define rel(sq, side) ((side) ? MIRROR[(sq)] : (sq))
#define distance(a, b) max(abs(rank(a) - rank(b)), abs(file(a) - file(b)))

// Phase generation calculation is standard from CPW
Score PHASE_MULTIPLIERS[5] = {0, 1, 1, 2, 4};

// static piece values for see and delta pruning just match EG values
int STATIC_MATERIAL_VALUE[7] = {93, 310, 323, 548, 970, 30000, 0};

// clang-format off
TScore MATERIAL_VALUES[7] = {{60, 93}, {310, 310}, {323, 323}, {435, 548}, {910, 970}, {30000, 30000}, {0, 0}};

TScore PAWN_PSQT[32] = {{0,0}, {0,0}, {0,0}, {0,0}, {56,132}, {40,143}, {67,127}, {92,119}, {6,13}, {-1,12}, {27,-10}, {28,-18}, {-11,0}, {1,-6}, {0,-10}, {17,-11}, {-14,-9}, {-10,-7}, {1,-9}, {8,-4}, {-12,-13}, {7,-13}, {0,-6}, {11,-2}, {-12,-10}, {2,-12}, {-6,-2}, {7,6}, {0,0}, {0,0}, {0,0}, {0,0}};
TScore KNIGHT_PSQT[32] = {{-50,-67}, {-15,-33}, {-17,-21}, {26,-24}, {9,-21}, {14,-3}, {50,-13}, {35,-5}, {24,-26}, {9,-5}, {26,3}, {45,-7}, {33,-10}, {20,-7}, {32,-2}, {39,0}, {32,-5}, {33,-10}, {36,0}, {38,3}, {14,-7}, {25,-9}, {20,-2}, {32,7}, {24,3}, {30,-8}, {22,-6}, {26,-2}, {4,-2}, {18,-12}, {23,-10}, {26,-3}};
TScore BISHOP_PSQT[32] = {{-34,-2}, {-27,-2}, {-99,10}, {-104,9}, {-15,-6}, {-18,-3}, {-6,-2}, {-20,0}, {-8,-6}, {-9,-8}, {-18,1}, {6,-11}, {-22,-3}, {1,-8}, {-3,-6}, {12,-6}, {9,-15}, {-9,-10}, {-3,-3}, {6,-7}, {-1,-7}, {13,-4}, {2,-2}, {1,3}, {9,-10}, {15,-11}, {8,-12}, {-4,0}, {7,-12}, {8,-9}, {1,-3}, {-2,-3}};
TScore ROOK_PSQT[32] = {{8,9}, {4,12}, {-16,23}, {-13,23}, {18,-8}, {12,1}, {28,-3}, {27,-10}, {11,0}, {47,-4}, {48,-3}, {45,-6}, {7,5}, {19,1}, {35,0}, {33,-1}, {4,4}, {16,0}, {12,7}, {19,5}, {5,0}, {19,-5}, {20,0}, {18,1}, {5,4}, {19,-1}, {24,0}, {25,0}, {20,0}, {16,1}, {19,1}, {23,0}};
TScore QUEEN_PSQT[32] = {{-24,-16}, {-25,3}, {19,-17}, {-4,6}, {-61,34}, {-85,70}, {-58,66}, {-65,59}, {-48,25}, {-51,50}, {-48,50}, {-45,62}, {-62,60}, {-56,70}, {-56,64}, {-64,83}, {-56,55}, {-55,67}, {-59,72}, {-66,86}, {-56,46}, {-51,51}, {-61,66}, {-59,60}, {-52,26}, {-47,15}, {-49,28}, {-50,41}, {-51,14}, {-62,27}, {-62,25}, {-55,21}};
TScore KING_PSQT[32] = {{-97,-141}, {42,-118}, {14,-95}, {-42,-91}, {10,-100}, {92,-80}, {83,-74}, {84,-85}, {23,-99}, {150,-90}, {135,-83}, {65,-78}, {-42,-92}, {83,-88}, {49,-72}, {0,-64}, {-80,-85}, {23,-84}, {22,-72}, {-20,-64}, {-31,-92}, {10,-84}, {19,-78}, {13,-73}, {-50,-100}, {-36,-81}, {-38,-73}, {-54,-68}, {-57,-127}, {-50,-104}, {-43,-90}, {-49,-91}};
TScore BISHOP_PAIR = {27,41};
TScore BISHOP_TRAPPED = {-116,-83};
TScore KNIGHT_OUTPOST_REACHABLE = {5,9};
TScore BISHOP_OUTPOST_REACHABLE = {3,3};
TScore BISHOP_POST_PSQT[32] = {{0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {13,9}, {15,10}, {18,16}, {16,9}, {10,8}, {16,14}, {16,12}, {16,12}, {3,9}, {15,14}, {15,12}, {16,15}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}};
TScore KNIGHT_POST_PSQT[32] = {{0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {3,13}, {45,0}, {30,5}, {27,24}, {22,0}, {43,11}, {37,17}, {43,21}, {20,0}, {33,8}, {22,12}, {25,21}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}};
TScore KNIGHT_MOBILITIES[9] = {{-87,-82}, {-70,-32}, {-57,-8}, {-48,0}, {-40,5}, {-33,11}, {-28,12}, {-22,10}, {-17,0}};
TScore BISHOP_MOBILITIES[14] = {{-58,-41}, {-42,-30}, {-32,-19}, {-25,-5}, {-16,1}, {-10,9}, {-7,13}, {-5,15}, {-3,19}, {-3,20}, {2,16}, {13,15}, {3,27}, {8,23}};
TScore ROOK_MOBILITIES[15] = {{-78,-111}, {-59,-29}, {-50,-16}, {-44,-21}, {-47,-3}, {-47,2}, {-49,7}, {-45,6}, {-40,5}, {-38,9}, {-36,10}, {-38,13}, {-38,16}, {-36,16}, {-27,7}};
TScore QUEEN_MOBILITIES[28] = {{-5,0}, {-126,-58}, {-99,-197}, {-102,-116}, {-102,2}, {-102,18}, {-103,36}, {-102,62}, {-102,79}, {-101,82}, {-98,86}, {-97,89}, {-99,96}, {-96,93}, {-97,97}, {-96,90}, {-98,94}, {-100,95}, {-99,89}, {-82,55}, {-82,52}, {-75,45}, {-35,-28}, {-22,-47}, {16,-100}, {16,-115}, {-35,-69}, {-6,-120}};
TScore DOUBLED_PAWN = {-4,-14};
TScore OPPOSED_ISOLATED_PAWN = {-10,-9};
TScore OPEN_ISOLATED_PAWN = {-15,-7};
TScore BACKWARDS_PAWN = {-11,-8};
TScore CONNECTED_PAWN[8] = {{0,0}, {54,4}, {25,12}, {11,7}, {5,2}, {0,2}, {0,0}, {0,0}};
TScore PASSED_PAWN[8] = {{0,0}, {43,92}, {34,153}, {18,87}, {4,47}, {3,31}, {4,27}, {0,0}};
TScore PASSED_PAWN_ADVANCE_DEFENDED = {15,18};
TScore PASSED_PAWN_EDGE_DISTANCE = {-5,-8};
TScore PASSED_PAWN_KING_PROXIMITY = {-2,18};
TScore ROOK_OPEN_FILE = {24,7};
TScore ROOK_SEMI_OPEN = {4,5};
TScore ROOK_SEVENTH_RANK = {3,8};
TScore ROOK_OPPOSITE_KING = {20,-8};
TScore ROOK_ADJACENT_KING = {4,-21};
TScore ROOK_TRAPPED = {-45,-14};
TScore KNIGHT_THREATS[6] = {{0,17}, {0,9}, {36,28}, {70,4}, {43,-40}, {106,32}};
TScore BISHOP_THREATS[6] = {{0,18}, {27,31}, {10,16}, {48,14}, {51,61}, {101,76}};
TScore ROOK_THREATS[6] = {{2,22}, {22,31}, {27,35}, {3,16}, {73,5}, {98,36}};
TScore KING_THREATS[6] = {{18,52}, {22,45}, {6,34}, {47,11}, {-106,-106}, {0,0}};
TScore TEMPO = {22,0};
// clang-format on

const int SHELTER_STORM_FILES[8][2] = {{0, 2}, {0, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6}, {5, 7}, {5, 7}};

Score PAWN_SHELTER[4][8] = {
    {-2, 12, 12, 15, 22, 38, 34, 0},
    {-18, -18, -5, -12, -21, 14, 28, 0},
    {-5, -15, 3, 13, -2, 10, 32, 0},
    {-16, -50, -24, -16, -21, -14, -5, 0},
};

Score PAWN_STORM[4][8] = {
    {-36, -20, -20, -20, -40, 70, 119, 0},
    {-18, -9, 4, -14, -20, -50, 10, 0},
    {3, 4, 5, 1, -14, -65, -21, 0},
    {6, 12, 6, -2, 0, -45, 5, 0},
};

TScore BLOCKED_PAWN_STORM[8] = {
    {0}, {0}, {2, -2}, {2, -2}, {2, -4}, {-31, -32}, {0}, {0},
};

TScore KS_KING_FILE[4] = {{9, -4}, {4, 0}, {0, 0}, {-4, 2}};

Score KS_ATTACKER_WEIGHTS[5] = {0, 64, 53, 67, 25};
Score KS_ATTACK = 39;
Score KS_WEAK_SQS = 74;
Score KS_SAFE_CHECK = 90;
Score KS_UNSAFE_CHECK = 12;
Score KS_ENEMY_QUEEN = 247;
Score KS_ALLIES = 17;

Score* PSQT[12][64];
Score* KNIGHT_POSTS[64];
Score* BISHOP_POSTS[64];

// reflect the PSQT for fast lookups
void InitPSQT() {
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

    BISHOP_POSTS[r * 8 + f] = BISHOP_POST_PSQT[sq];
    BISHOP_POSTS[r * 8 + (7 - f)] = BISHOP_POST_PSQT[sq];
  }
}

// TODO: since I don't tune this, probably should just make it 24
inline Score MaxPhase() {
  return 4 * PHASE_MULTIPLIERS[KNIGHT_TYPE] + 4 * PHASE_MULTIPLIERS[BISHOP_TYPE] + 4 * PHASE_MULTIPLIERS[ROOK_TYPE] +
         2 * PHASE_MULTIPLIERS[QUEEN_TYPE];
}

// game phase is remaing piece phase score / max phase score. So opening is 1, endgame is 0
inline Score GetPhase(Board* board) {
  Score maxP = MaxPhase();

  Score phase = 0;
  for (int i = KNIGHT_WHITE; i <= QUEEN_BLACK; i++)
    phase += PHASE_MULTIPLIERS[PIECE_TYPE[i]] * bits(board->pieces[i]);

  phase = min(maxP, phase);
  return phase;
}

// combine a mg and eg score to a linearly tapered value
inline Score Taper(Score mg, Score eg, Score phase) {
  Score maxP = MaxPhase();

  return (phase * mg + (maxP - phase) * eg) / maxP;
}

// check for all sorts of different piece keys that represent a draw
// TODO: Why doesn't my eval do this as well?
inline int IsMaterialDraw(Board* board) {
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
  // This is the most basic portion of Berserk's evalution
  // It calculates material values, piece bonuses, mobility, and special pawn bonuses
  // It is also REQUIRED TO COME FIRST - the data it builds are used by later methods
  // within the evaluation

  // Just make sure everything is 0
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

  // Tempo score if it's your turn to move
  if (side == board->side) {
    data->tempo[MG] += TEMPO[MG];
    data->tempo[EG] += TEMPO[EG];
  }

  // Random utility stuff
  BitBoard myPawns = board->pieces[PAWN[side]];
  BitBoard opponentPawns = board->pieces[PAWN[xside]];

  BitBoard pawnAttacks = Shift(myPawns, PAWN_DIRECTIONS[side] + E) | Shift(myPawns, PAWN_DIRECTIONS[side] + W);
  BitBoard oppoPawnAttacks =
      Shift(opponentPawns, PAWN_DIRECTIONS[xside] + E) | Shift(opponentPawns, PAWN_DIRECTIONS[xside] + W);

  // used in combination with mobility
  BitBoard myBlockedAndHomePawns =
      (Shift(board->occupancies[BOTH], PAWN_DIRECTIONS[xside]) | HOME_RANKS[side] | THIRD_RANKS[side]) &
      board->pieces[PAWN[side]];

  int kingSq = lsb(board->pieces[KING[side]]);

  // king area required for properly building attack data
  int enemyKingSq = lsb(board->pieces[KING[xside]]);
  BitBoard enemyKingArea = GetKingAttacks(enemyKingSq);

  // Initial attack stuff from pawns (pawn eval done elsewhere)
  data->attacks[PAWN_TYPE] = pawnAttacks;
  data->allAttacks = pawnAttacks;
  data->attacks2 = Shift(myPawns, PAWN_DIRECTIONS[side] + E) & Shift(myPawns, PAWN_DIRECTIONS[side] + W);

  // Outposts are determined as squares that are on the 4/5/6 rank (side relative), can't be attacked by
  // an enemy pawn, and is either defended by a pawn, in front of an enemy pawn, or behind ally pawn
  BitBoard outposts = ~Fill(oppoPawnAttacks, PAWN_DIRECTIONS[xside]) &
                      (pawnAttacks | Shift(myPawns | opponentPawns, PAWN_DIRECTIONS[xside])) &
                      (RANK_4 | RANK_5 | THIRD_RANKS[xside]);
  // KNIGHTS
  for (BitBoard knights = board->pieces[KNIGHT[side]]; knights; popLsb(knights)) {
    int sq = lsb(knights);
    BitBoard sqBb = knights & -knights;

    data->material[MG] += MATERIAL_VALUES[KNIGHT_TYPE][MG];
    data->material[EG] += MATERIAL_VALUES[KNIGHT_TYPE][EG];
    data->knights[MG] += PSQT[KNIGHT[side]][sq][MG];
    data->knights[EG] += PSQT[KNIGHT[side]][sq][EG];

    BitBoard movement = GetKnightAttacks(sq);

    // build standard attack data
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[KNIGHT_TYPE] |= movement;
    data->allAttacks |= movement;

    // build king safety attack data
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[KNIGHT_TYPE];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    // mobility is where ever the enemy pawns aren't attacking and where allied pawns aren't in the way
    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[MG] += KNIGHT_MOBILITIES[bits(mobilitySquares)][MG];
    data->mobility[EG] += KNIGHT_MOBILITIES[bits(mobilitySquares)][EG];

    if (outposts & sqBb) {
      // apply bonus if outpost on outpost, each square has a specific bonus
      data->knights[MG] += KNIGHT_POSTS[rel(sq, side)][MG];
      data->knights[EG] += KNIGHT_POSTS[rel(sq, side)][EG];
    } else if (movement & outposts) {
      // bonus if you can "see" an outpost
      data->knights[MG] += KNIGHT_OUTPOST_REACHABLE[MG];
      data->knights[EG] += KNIGHT_OUTPOST_REACHABLE[EG];
    }
  }

  // BISHOPS
  // bishop pair bonus
  if (bits(board->pieces[BISHOP[side]]) > 1) {
    data->bishops[MG] += BISHOP_PAIR[MG];
    data->bishops[EG] += BISHOP_PAIR[EG];
  }

  for (BitBoard bishops = board->pieces[BISHOP[side]]; bishops; popLsb(bishops)) {
    int sq = lsb(bishops);
    BitBoard sqBb = bishops & -bishops;

    data->material[MG] += MATERIAL_VALUES[BISHOP_TYPE][MG];
    data->material[EG] += MATERIAL_VALUES[BISHOP_TYPE][EG];
    data->bishops[MG] += PSQT[BISHOP[side]][sq][MG];
    data->bishops[EG] += PSQT[BISHOP[side]][sq][EG];

    BitBoard movement = GetBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]);

    // build standard attack data
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[BISHOP_TYPE] |= movement;
    data->allAttacks |= movement;

    // build king safety attack data
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[BISHOP_TYPE];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    // mobility is where ever the enemy pawns aren't attacking and where allied pawns aren't in the way
    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[MG] += BISHOP_MOBILITIES[bits(mobilitySquares)][MG];
    data->mobility[EG] += BISHOP_MOBILITIES[bits(mobilitySquares)][EG];

    if (outposts & sqBb) {
      // apply bonus if outpost on outpost, each square has a specific bonus
      data->bishops[MG] += BISHOP_POSTS[rel(sq, side)][MG];
      data->bishops[EG] += BISHOP_POSTS[rel(sq, side)][EG];
    } else if (movement & outposts) {
      // bonus if you can "see" an outpost
      data->bishops[MG] += BISHOP_OUTPOST_REACHABLE[MG];
      data->bishops[EG] += BISHOP_OUTPOST_REACHABLE[EG];
    }

    // TODO: Center square control? good/bad bishop? enemy pawn targets?

    // trapped bishop scenarios is hard coded
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
        GetRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]] ^ board->pieces[ROOK[side]]);

    // build standard attack data
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[ROOK_TYPE] |= movement;
    data->allAttacks |= movement;

    // build king safety attack data
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[ROOK_TYPE];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    // mobility is where ever the enemy pawns aren't attacking and where allied pawns aren't in the way
    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[MG] += ROOK_MOBILITIES[bits(mobilitySquares)][MG];
    data->mobility[EG] += ROOK_MOBILITIES[bits(mobilitySquares)][EG];

    // bonus for 7th rank
    if (sqBb & PROMOTION_RANKS[side]) {
      data->rooks[MG] += ROOK_SEVENTH_RANK[MG];
      data->rooks[EG] += ROOK_SEVENTH_RANK[EG];
    }

    // bonus for file openness
    if (!(FILE_MASKS[file] & myPawns)) {
      if (!(FILE_MASKS[file] & opponentPawns)) {
        data->rooks[MG] += ROOK_OPEN_FILE[MG];
        data->rooks[EG] += ROOK_OPEN_FILE[EG];
      } else {
        // TODO: Modify this if pawn on file is defended by a pawn?
        data->rooks[MG] += ROOK_SEMI_OPEN[MG];
        data->rooks[EG] += ROOK_SEMI_OPEN[EG];
      }

      // king threats bonus - these tuned really negative in the EG which is both surprising but not
      if (FILE_MASKS[file] & board->pieces[KING[xside]]) {
        data->rooks[MG] += ROOK_OPPOSITE_KING[MG];
        data->rooks[EG] += ROOK_OPPOSITE_KING[EG];
      } else if (ADJACENT_FILE_MASKS[file] & board->pieces[KING[xside]]) {
        data->rooks[MG] += ROOK_ADJACENT_KING[MG];
        data->rooks[EG] += ROOK_ADJACENT_KING[EG];
      }
    }

    // if the king has slid left/right and the rook is still stuck, it's essentially trapped
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

    BitBoard movement = GetQueenAttacks(sq, board->occupancies[BOTH]);

    // build standard attack data
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[QUEEN_TYPE] |= movement;
    data->allAttacks |= movement;

    // build king safety attack data
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[QUEEN_TYPE];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    // mobility is where ever the enemy pawns aren't attacking and where allied pawns aren't in the way
    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[MG] += QUEEN_MOBILITIES[bits(mobilitySquares)][MG];
    data->mobility[EG] += QUEEN_MOBILITIES[bits(mobilitySquares)][EG];

    // TODO: Batteries? Queen on 7th? enemy rooks?
  }

  // KINGS
  for (BitBoard kings = board->pieces[KING[side]]; kings; popLsb(kings)) {
    int sq = lsb(kings);

    data->kings[MG] += PSQT[KING[side]][sq][MG];
    data->kings[EG] += PSQT[KING[side]][sq][EG];

    // only include legal king attacks into attack data
    // TODO: reconsider this?
    BitBoard movement = GetKingAttacks(sq) & ~GetKingAttacks(lsb(board->pieces[KING[xside]]));
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[KING_TYPE] |= movement;
    data->allAttacks |= movement;
  }

  // TODO: Probably should separate this function
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
    int adjustedFile = file > 3 ? 7 - file : file; // 0-3

    // opposed if enemy pawn on the same file
    BitBoard opposed = board->pieces[PAWN[xside]] & FILE_MASKS[file] & FORWARD_RANK_MASKS[side][rank];

    // doubled only if the pawns are touching on the same file
    BitBoard doubled = board->pieces[PAWN[side]] & Shift(sqBitboard, PAWN_DIRECTIONS[xside]);

    // neighbors = pawns on adjacent files
    BitBoard neighbors = board->pieces[PAWN[side]] & ADJACENT_FILE_MASKS[file];

    // connected are neighbor pawns on the same rank
    BitBoard connected = neighbors & RANK_MASKS[rank];

    // defenders are supporting this pawn
    BitBoard defenders = board->pieces[PAWN[side]] & GetPawnAttacks(sq, xside);

    // enemy pawn is controlling square in front of this pawn
    BitBoard forwardLever = board->pieces[PAWN[xside]] & GetPawnAttacks(sq + PAWN_DIRECTIONS[side], side);

    // backwards is if there are no neighbors on same rank or behind and square in front is controlled
    int backwards = !(neighbors & FORWARD_RANK_MASKS[xside][rank(sq + PAWN_DIRECTIONS[side])]) && forwardLever;

    // passed if no enemy pawns within it's attack span AND there's not an allied pawn in front on the same file
    // (doubled but separated)
    int passed = !(board->pieces[PAWN[xside]] & FORWARD_RANK_MASKS[side][rank] &
                   (ADJACENT_FILE_MASKS[file] | FILE_MASKS[file])) &&
                 !(board->pieces[PAWN[side]] & FORWARD_RANK_MASKS[side][rank] & FILE_MASKS[file]);

    if (doubled) {
      data->pawns[MG] += DOUBLED_PAWN[MG];
      data->pawns[EG] += DOUBLED_PAWN[EG];
    }

    if (!neighbors) {
      // open file isolated pawns are way worse
      data->pawns[MG] += opposed ? OPPOSED_ISOLATED_PAWN[MG] : OPEN_ISOLATED_PAWN[MG];
      data->pawns[EG] += opposed ? OPPOSED_ISOLATED_PAWN[EG] : OPEN_ISOLATED_PAWN[EG];
    } else if (backwards) {
      data->pawns[MG] += BACKWARDS_PAWN[MG];
      data->pawns[EG] += BACKWARDS_PAWN[EG];
    } else if (defenders | connected) {
      int s = 2 + !!connected - !!opposed;

      // scale connection rank if it's opposed/defended instead of connected
      data->pawns[MG] += CONNECTED_PAWN[adjustedRank][MG] * s;
      data->pawns[EG] += CONNECTED_PAWN[adjustedRank][EG] * s;
    }

    if (passed) {
      // flat passer bonus for rank
      data->pawns[MG] += PASSED_PAWN[adjustedRank][MG];
      data->pawns[EG] += PASSED_PAWN[adjustedRank][EG];

      // edge pawns get better near the end
      data->pawns[MG] += PASSED_PAWN_EDGE_DISTANCE[MG] * adjustedFile;
      data->pawns[EG] += PASSED_PAWN_EDGE_DISTANCE[EG] * adjustedFile;

      int advSq = sq + PAWN_DIRECTIONS[side];
      BitBoard advance = bit(advSq);

      // we only apply these values if the passed pawn is actually on the move
      // this encourages pushing and removes awkward value tuning
      if (adjustedRank <= 4) {
        int myDistance = distance(advSq, kingSq);
        int enemyDistance = distance(advSq, enemyKingSq);

        // king proximity is a big deal in the EG, this also balances well with king safety (use enemy pawn as shield)
        data->pawns[MG] += PASSED_PAWN_KING_PROXIMITY[MG] * min(4, max(enemyDistance - myDistance, -4));
        data->pawns[EG] += PASSED_PAWN_KING_PROXIMITY[EG] * min(4, max(enemyDistance - myDistance, -4));

        // if the enemy side isn't blocking our pawn
        if (!(board->occupancies[xside] & advance)) {
          // rook from behind attacks wouldn't make it into our data object, so we check for that here
          BitBoard pusher = GetRookAttacks(sq, board->occupancies[BOTH]) & FILE_MASKS[file] &
                            FORWARD_RANK_MASKS[xside][rank] & (board->pieces[ROOK[side]] | board->pieces[QUEEN[side]]);

          // bonus if sq in front is defended
          if (pusher | (data->allAttacks & advance)) {
            data->pawns[MG] += PASSED_PAWN_ADVANCE_DEFENDED[MG];
            data->pawns[EG] += PASSED_PAWN_ADVANCE_DEFENDED[EG];
          }
        }
      }
    }
  }
}

void EvaluateThreats(Board* board, int side, EvalData* data, EvalData* enemyData) {
  // Threats are pretty simple in Berserk
  // If a piece is attacking another piece that is either undefended or "weak", then
  // usually we apply a bonus for this
  // we do not care for queen threats as it hits too much stuff

  data->threats[MG] = 0;
  data->threats[EG] = 0;

  int xside = 1 - side;

  BitBoard* myAttacks = data->attacks;
  BitBoard* enemyAttacks = enemyData->attacks;

  // "weak" is an enemy piece that is not defended by by a pawn or isn't defended twice and is hit twice
  BitBoard weak = board->occupancies[xside] & ~(enemyAttacks[PAWN_TYPE] | (enemyData->attacks2 & ~data->attacks2)) &
                  data->allAttacks;

  // apply bonus for knight threats by piece type
  for (BitBoard knightThreats = weak & myAttacks[KNIGHT_TYPE]; knightThreats; popLsb(knightThreats)) {
    int piece = board->squares[lsb(knightThreats)];

    assert(piece != NO_PIECE);

    data->threats[MG] += KNIGHT_THREATS[PIECE_TYPE[piece]][MG];
    data->threats[EG] += KNIGHT_THREATS[PIECE_TYPE[piece]][EG];
  }

  // apply bonus for bishop threats by piece type
  for (BitBoard bishopThreats = weak & myAttacks[BISHOP_TYPE]; bishopThreats; popLsb(bishopThreats)) {
    int piece = board->squares[lsb(bishopThreats)];

    assert(piece != NO_PIECE);

    data->threats[MG] += BISHOP_THREATS[PIECE_TYPE[piece]][MG];
    data->threats[EG] += BISHOP_THREATS[PIECE_TYPE[piece]][EG];
  }

  // apply bonus for rook threats by piece type
  for (BitBoard rookThreats = weak & myAttacks[ROOK_TYPE]; rookThreats; popLsb(rookThreats)) {
    int piece = board->squares[lsb(rookThreats)];

    assert(piece != NO_PIECE);

    data->threats[MG] += ROOK_THREATS[PIECE_TYPE[piece]][MG];
    data->threats[EG] += ROOK_THREATS[PIECE_TYPE[piece]][EG];
  }

  // apply bonus for king threats by piece type
  // TODO: King hitting QUEEN is insanely bad and why do we check that?
  for (BitBoard kingThreats = weak & myAttacks[KING_TYPE]; kingThreats; popLsb(kingThreats)) {
    int piece = board->squares[lsb(kingThreats)];

    assert(piece != NO_PIECE);

    data->threats[MG] += KING_THREATS[PIECE_TYPE[piece]][MG];
    data->threats[EG] += KING_THREATS[PIECE_TYPE[piece]][EG];
  }
}

void EvaluateKingSafety(Board* board, int side, EvalData* data, EvalData* enemyData) {
  // King safety scales quadratically in Berserk - the values around this are not tuned atm as a result
  // it takes into consideration pawn shelter/storm, file status, checks, unsafe checks, attack weight (piece types
  // threatening), attack count, weak squares, mobility difference, knight defender, and queen on board

  data->kingSafety[MG] = 0;
  data->kingSafety[EG] = 0;

  int xside = 1 - side;
  int kingSq = lsb(board->pieces[KING[side]]);
  int kingFile = file(kingSq);

  int shelterScoreMg = 2, shelterScoreEg = 2;
  BitBoard ourPawns =
      board->pieces[PAWN[side]] & ~enemyData->attacks[PAWN_TYPE] & ~FORWARD_RANK_MASKS[xside][rank(kingSq)];
  BitBoard enemyPawns = board->pieces[PAWN[xside]] & ~FORWARD_RANK_MASKS[xside][rank(kingSq)];
  for (int f = SHELTER_STORM_FILES[kingFile][0]; f <= SHELTER_STORM_FILES[kingFile][1]; f++) {
    int adjustedFile = f > 3 ? 7 - f : f;

    BitBoard ourPawnFile = ourPawns & FILE_MASKS[f];
    int pawnRank = ourPawnFile ? (side ? 7 - rank(lsb(ourPawnFile)) : rank(msb(ourPawnFile))) : 0;
    // apply score for this pawn shelter on file, nothing is added to EG score
    shelterScoreMg += PAWN_SHELTER[adjustedFile][pawnRank];

    BitBoard enemyPawnFile = enemyPawns & FILE_MASKS[f];
    int theirRank = enemyPawnFile ? (side ? 7 - rank(lsb(enemyPawnFile)) : rank(msb(enemyPawnFile))) : 0;
    if (pawnRank && pawnRank == theirRank + 1) {
      // apply score for pawn storm that is blocked, blockades in EG are actually bad for us, so that is accounted for
      // blocked eg is dangerous because it's keeping one of our pawns from advancing
      shelterScoreMg += BLOCKED_PAWN_STORM[theirRank][MG];
      shelterScoreEg += BLOCKED_PAWN_STORM[theirRank][EG];
    } else {
      // storm score, no change in the EG
      shelterScoreMg += PAWN_STORM[adjustedFile][theirRank];
    }

    if (f == file(kingSq)) {
      // for the file of the king look to see the status of pawns on said file
      // open/semi open (both ways)/closed
      int idx = 2 * !(board->pieces[PAWN[side]] & FILE_MASKS[f]) + !(board->pieces[PAWN[xside]] & FILE_MASKS[f]);

      shelterScoreMg += KS_KING_FILE[idx][MG];
      shelterScoreEg += KS_KING_FILE[idx][EG];
    }
  }

  BitBoard kingArea = GetKingAttacks(kingSq);
  BitBoard weak = enemyData->allAttacks & ~data->attacks2 & (~data->allAttacks | data->attacks[4] | data->attacks[5]);
  BitBoard vulnerable = (~data->allAttacks | (weak & enemyData->attacks2)) & ~board->occupancies[xside];

  BitBoard possibleKnightChecks =
      GetKnightAttacks(kingSq) & enemyData->attacks[KNIGHT_TYPE] & ~board->occupancies[xside];
  int safeChecks = bits(possibleKnightChecks & vulnerable);
  int unsafeChecks = bits(possibleKnightChecks & ~vulnerable);

  BitBoard possibleBishopChecks =
      GetBishopAttacks(kingSq, board->occupancies[BOTH]) & enemyData->attacks[BISHOP_TYPE] & ~board->occupancies[xside];
  safeChecks += bits(possibleBishopChecks & vulnerable);
  unsafeChecks += bits(possibleBishopChecks & ~vulnerable);

  BitBoard possibleRookChecks =
      GetRookAttacks(kingSq, board->occupancies[BOTH]) & enemyData->attacks[ROOK_TYPE] & ~board->occupancies[xside];
  safeChecks += bits(possibleRookChecks & vulnerable);
  unsafeChecks += bits(possibleRookChecks & ~vulnerable);

  BitBoard possibleQueenChecks =
      GetQueenAttacks(kingSq, board->occupancies[BOTH]) & enemyData->attacks[QUEEN_TYPE] & ~board->occupancies[xside];
  safeChecks += bits(possibleQueenChecks & vulnerable);

  BitBoard allies = kingArea & (board->occupancies[side] & ~board->pieces[QUEEN[side]]);

  int score = (enemyData->attackers * enemyData->attackWeight)  // weight of attacks
              + (KS_SAFE_CHECK * safeChecks)                    // safe checks
              + (KS_UNSAFE_CHECK * unsafeChecks)                // unsafe checks
              + (KS_WEAK_SQS * bits(weak & kingArea))           // weak sqs
              + (KS_ATTACK * enemyData->attackCount)            // aimed pieces
              - (KS_ENEMY_QUEEN * !board->pieces[QUEEN[xside]]) // enemy has queen?
              - (KS_ALLIES * (bits(allies) - 2))                // allies
              - shelterScoreMg / 2;                             // house

  // if my king is in danger apply score
  if (score > 0) {
    data->kingSafety[MG] += -score * score / 1000; // quadratic scale in MG
    data->kingSafety[EG] += -score / 30;           // small linear scale in EG
  }

  // add shelter score
  data->kingSafety[MG] += shelterScoreMg;
  data->kingSafety[EG] += shelterScoreEg;
}

// Main evalution method
Score Evaluate(Board* board) {
  EvalData sideData;
  EvalData xsideData;

  EvaluateSide(board, board->side, &sideData);
  EvaluateSide(board, board->xside, &xsideData);

  EvaluateThreats(board, board->side, &sideData, &xsideData);
  EvaluateThreats(board, board->xside, &xsideData, &sideData);

  EvaluateKingSafety(board, board->side, &sideData, &xsideData);
  EvaluateKingSafety(board, board->xside, &xsideData, &sideData);

  return ToScore(&sideData, board) - ToScore(&xsideData, board);
}

inline Score ToScore(EvalData* data, Board* board) {
  // Sum the parts and taper the score
  Score mg = data->pawns[MG] + data->knights[MG] + data->bishops[MG] + data->rooks[MG] + data->queens[MG] +
             data->kings[MG] + data->kingSafety[MG] + data->material[MG] + data->mobility[MG] + data->threats[MG] +
             data->tempo[MG];
  Score eg = data->pawns[EG] + data->knights[EG] + data->bishops[EG] + data->rooks[EG] + data->queens[EG] +
             data->kings[EG] + data->kingSafety[EG] + data->material[EG] + data->mobility[EG] + data->threats[EG] +
             data->tempo[MG];

  return Taper(mg, eg, GetPhase(board));
}

#ifndef TUNE
void PrintEvaluation(Board* board) {
  EvalData whiteEval;
  EvalData blackEval;

  EvaluateSide(board, WHITE, &whiteEval);
  EvaluateSide(board, BLACK, &blackEval);

  EvaluateThreats(board, WHITE, &whiteEval, &blackEval);
  EvaluateThreats(board, BLACK, &blackEval, &whiteEval);

  EvaluateKingSafety(board, WHITE, &whiteEval, &blackEval);
  EvaluateKingSafety(board, BLACK, &blackEval, &whiteEval);

  int phase = GetPhase(board);

  printf("|            | WHITE mg,eg,taper | BLACK mg,eg,taper | DIFF  mg,eg,taper |\n");
  printf("|------------|-------------------|-------------------|-------------------|\n");
  printf(
      "| material   | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval.material[MG], whiteEval.material[EG],
      Taper(whiteEval.material[MG], whiteEval.material[EG], phase), blackEval.material[MG], blackEval.material[EG],
      Taper(blackEval.material[MG], blackEval.material[EG], phase), whiteEval.material[MG] - blackEval.material[MG],
      whiteEval.material[EG] - blackEval.material[EG],
      Taper(whiteEval.material[MG] - blackEval.material[MG], whiteEval.material[EG] - blackEval.material[EG], phase));
  printf("| pawns      | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval.pawns[MG], whiteEval.pawns[EG],
         Taper(whiteEval.pawns[MG], whiteEval.pawns[EG], phase), blackEval.pawns[MG], blackEval.pawns[EG],
         Taper(blackEval.pawns[MG], blackEval.pawns[EG], phase), whiteEval.pawns[MG] - blackEval.pawns[MG],
         whiteEval.pawns[EG] - blackEval.pawns[EG],
         Taper(whiteEval.pawns[MG] - blackEval.pawns[MG], whiteEval.pawns[EG] - blackEval.pawns[EG], phase));
  printf("| knights    | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval.knights[MG], whiteEval.knights[EG],
         Taper(whiteEval.knights[MG], whiteEval.knights[EG], phase), blackEval.knights[MG], blackEval.knights[EG],
         Taper(blackEval.knights[MG], blackEval.knights[EG], phase), whiteEval.knights[MG] - blackEval.knights[MG],
         whiteEval.knights[EG] - blackEval.knights[EG],
         Taper(whiteEval.knights[MG] - blackEval.knights[MG], whiteEval.knights[EG] - blackEval.knights[EG], phase));
  printf("| bishops    | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval.bishops[MG], whiteEval.bishops[EG],
         Taper(whiteEval.bishops[MG], whiteEval.bishops[EG], phase), blackEval.bishops[MG], blackEval.bishops[EG],
         Taper(blackEval.bishops[MG], blackEval.bishops[EG], phase), whiteEval.bishops[MG] - blackEval.bishops[MG],
         whiteEval.bishops[EG] - blackEval.bishops[EG],
         Taper(whiteEval.bishops[MG] - blackEval.bishops[MG], whiteEval.bishops[EG] - blackEval.bishops[EG], phase));
  printf("| rooks      | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval.rooks[MG], whiteEval.rooks[EG],
         Taper(whiteEval.rooks[MG], whiteEval.rooks[EG], phase), blackEval.rooks[MG], blackEval.rooks[EG],
         Taper(blackEval.rooks[MG], blackEval.rooks[EG], phase), whiteEval.rooks[MG] - blackEval.rooks[MG],
         whiteEval.rooks[EG] - blackEval.rooks[EG],
         Taper(whiteEval.rooks[MG] - blackEval.rooks[MG], whiteEval.rooks[EG] - blackEval.rooks[EG], phase));
  printf("| queens     | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval.queens[MG], whiteEval.queens[EG],
         Taper(whiteEval.queens[MG], whiteEval.queens[EG], phase), blackEval.queens[MG], blackEval.queens[EG],
         Taper(blackEval.queens[MG], blackEval.queens[EG], phase), whiteEval.queens[MG] - blackEval.queens[MG],
         whiteEval.queens[EG] - blackEval.queens[EG],
         Taper(whiteEval.queens[MG] - blackEval.queens[MG], whiteEval.queens[EG] - blackEval.queens[EG], phase));
  printf("| kings      | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval.kings[MG], whiteEval.kings[EG],
         Taper(whiteEval.kings[MG], whiteEval.kings[EG], phase), blackEval.kings[MG], blackEval.kings[EG],
         Taper(blackEval.kings[MG], blackEval.kings[EG], phase), whiteEval.kings[MG] - blackEval.kings[MG],
         whiteEval.kings[EG] - blackEval.kings[EG],
         Taper(whiteEval.kings[MG] - blackEval.kings[MG], whiteEval.kings[EG] - blackEval.kings[EG], phase));
  printf(
      "| mobility   | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval.mobility[MG], whiteEval.mobility[EG],
      Taper(whiteEval.mobility[MG], whiteEval.mobility[EG], phase), blackEval.mobility[MG], blackEval.mobility[EG],
      Taper(blackEval.mobility[MG], blackEval.mobility[EG], phase), whiteEval.mobility[MG] - blackEval.mobility[MG],
      whiteEval.mobility[EG] - blackEval.mobility[EG],
      Taper(whiteEval.mobility[MG] - blackEval.mobility[MG], whiteEval.mobility[EG] - blackEval.mobility[EG], phase));
  printf("| threats    | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval.threats[MG], whiteEval.threats[EG],
         Taper(whiteEval.threats[MG], whiteEval.threats[EG], phase), blackEval.threats[MG], blackEval.threats[EG],
         Taper(blackEval.threats[MG], blackEval.threats[EG], phase), whiteEval.threats[MG] - blackEval.threats[MG],
         whiteEval.threats[EG] - blackEval.threats[EG],
         Taper(whiteEval.threats[MG] - blackEval.threats[MG], whiteEval.threats[EG] - blackEval.threats[EG], phase));
  printf("| kingSafety | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval.kingSafety[MG],
         whiteEval.kingSafety[EG], Taper(whiteEval.kingSafety[MG], whiteEval.kingSafety[EG], phase),
         blackEval.kingSafety[MG], blackEval.kingSafety[EG],
         Taper(blackEval.kingSafety[MG], blackEval.kingSafety[EG], phase),
         whiteEval.kingSafety[MG] - blackEval.kingSafety[MG], whiteEval.kingSafety[EG] - blackEval.kingSafety[EG],
         Taper(whiteEval.kingSafety[MG] - blackEval.kingSafety[MG], whiteEval.kingSafety[EG] - blackEval.kingSafety[EG],
               phase));
  printf("|------------|-------------------|-------------------|-------------------|\n");
  printf("\nResult (white): %5d\n\n", ToScore(&whiteEval, board) - ToScore(&blackEval, board));
}
#else
void PrintEvaluation(Board* board) {
  EvalData whiteEval;
  EvalData blackEval;

  EvaluateSide(board, WHITE, &whiteEval);
  EvaluateSide(board, BLACK, &blackEval);

  EvaluateThreats(board, WHITE, &whiteEval, &blackEval);
  EvaluateThreats(board, BLACK, &blackEval, &whiteEval);

  EvaluateKingSafety(board, WHITE, &whiteEval, &blackEval);
  EvaluateKingSafety(board, BLACK, &blackEval, &whiteEval);

  int phase = GetPhase(board);

  printf("|            | WHITE mg,eg,taper | BLACK mg,eg,taper | DIFF  mg,eg,taper |\n");
  printf("|------------|-------------------|-------------------|-------------------|\n");
  printf(
      "| material   | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval.material[MG],
      whiteEval.material[EG], Taper(whiteEval.material[MG], whiteEval.material[EG], phase), blackEval.material[MG],
      blackEval.material[EG], Taper(blackEval.material[MG], blackEval.material[EG], phase),
      whiteEval.material[MG] - blackEval.material[MG], whiteEval.material[EG] - blackEval.material[EG],
      Taper(whiteEval.material[MG] - blackEval.material[MG], whiteEval.material[EG] - blackEval.material[EG], phase));
  printf("| pawns      | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval.pawns[MG],
         whiteEval.pawns[EG], Taper(whiteEval.pawns[MG], whiteEval.pawns[EG], phase), blackEval.pawns[MG],
         blackEval.pawns[EG], Taper(blackEval.pawns[MG], blackEval.pawns[EG], phase),
         whiteEval.pawns[MG] - blackEval.pawns[MG], whiteEval.pawns[EG] - blackEval.pawns[EG],
         Taper(whiteEval.pawns[MG] - blackEval.pawns[MG], whiteEval.pawns[EG] - blackEval.pawns[EG], phase));
  printf("| knights    | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval.knights[MG],
         whiteEval.knights[EG], Taper(whiteEval.knights[MG], whiteEval.knights[EG], phase), blackEval.knights[MG],
         blackEval.knights[EG], Taper(blackEval.knights[MG], blackEval.knights[EG], phase),
         whiteEval.knights[MG] - blackEval.knights[MG], whiteEval.knights[EG] - blackEval.knights[EG],
         Taper(whiteEval.knights[MG] - blackEval.knights[MG], whiteEval.knights[EG] - blackEval.knights[EG], phase));
  printf("| bishops    | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval.bishops[MG],
         whiteEval.bishops[EG], Taper(whiteEval.bishops[MG], whiteEval.bishops[EG], phase), blackEval.bishops[MG],
         blackEval.bishops[EG], Taper(blackEval.bishops[MG], blackEval.bishops[EG], phase),
         whiteEval.bishops[MG] - blackEval.bishops[MG], whiteEval.bishops[EG] - blackEval.bishops[EG],
         Taper(whiteEval.bishops[MG] - blackEval.bishops[MG], whiteEval.bishops[EG] - blackEval.bishops[EG], phase));
  printf("| rooks      | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval.rooks[MG],
         whiteEval.rooks[EG], Taper(whiteEval.rooks[MG], whiteEval.rooks[EG], phase), blackEval.rooks[MG],
         blackEval.rooks[EG], Taper(blackEval.rooks[MG], blackEval.rooks[EG], phase),
         whiteEval.rooks[MG] - blackEval.rooks[MG], whiteEval.rooks[EG] - blackEval.rooks[EG],
         Taper(whiteEval.rooks[MG] - blackEval.rooks[MG], whiteEval.rooks[EG] - blackEval.rooks[EG], phase));
  printf("| queens     | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval.queens[MG],
         whiteEval.queens[EG], Taper(whiteEval.queens[MG], whiteEval.queens[EG], phase), blackEval.queens[MG],
         blackEval.queens[EG], Taper(blackEval.queens[MG], blackEval.queens[EG], phase),
         whiteEval.queens[MG] - blackEval.queens[MG], whiteEval.queens[EG] - blackEval.queens[EG],
         Taper(whiteEval.queens[MG] - blackEval.queens[MG], whiteEval.queens[EG] - blackEval.queens[EG], phase));
  printf("| kings      | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval.kings[MG],
         whiteEval.kings[EG], Taper(whiteEval.kings[MG], whiteEval.kings[EG], phase), blackEval.kings[MG],
         blackEval.kings[EG], Taper(blackEval.kings[MG], blackEval.kings[EG], phase),
         whiteEval.kings[MG] - blackEval.kings[MG], whiteEval.kings[EG] - blackEval.kings[EG],
         Taper(whiteEval.kings[MG] - blackEval.kings[MG], whiteEval.kings[EG] - blackEval.kings[EG], phase));
  printf(
      "| mobility   | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval.mobility[MG],
      whiteEval.mobility[EG], Taper(whiteEval.mobility[MG], whiteEval.mobility[EG], phase), blackEval.mobility[MG],
      blackEval.mobility[EG], Taper(blackEval.mobility[MG], blackEval.mobility[EG], phase),
      whiteEval.mobility[MG] - blackEval.mobility[MG], whiteEval.mobility[EG] - blackEval.mobility[EG],
      Taper(whiteEval.mobility[MG] - blackEval.mobility[MG], whiteEval.mobility[EG] - blackEval.mobility[EG], phase));
  printf("| threats    | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval.threats[MG],
         whiteEval.threats[EG], Taper(whiteEval.threats[MG], whiteEval.threats[EG], phase), blackEval.threats[MG],
         blackEval.threats[EG], Taper(blackEval.threats[MG], blackEval.threats[EG], phase),
         whiteEval.threats[MG] - blackEval.threats[MG], whiteEval.threats[EG] - blackEval.threats[EG],
         Taper(whiteEval.threats[MG] - blackEval.threats[MG], whiteEval.threats[EG] - blackEval.threats[EG], phase));
  printf("| kingSafety | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f | %5.0f %5.0f %5.0f |\n", whiteEval.kingSafety[MG],
         whiteEval.kingSafety[EG], Taper(whiteEval.kingSafety[MG], whiteEval.kingSafety[EG], phase),
         blackEval.kingSafety[MG], blackEval.kingSafety[EG],
         Taper(blackEval.kingSafety[MG], blackEval.kingSafety[EG], phase),
         whiteEval.kingSafety[MG] - blackEval.kingSafety[MG], whiteEval.kingSafety[EG] - blackEval.kingSafety[EG],
         Taper(whiteEval.kingSafety[MG] - blackEval.kingSafety[MG], whiteEval.kingSafety[EG] - blackEval.kingSafety[EG],
               phase));
  printf("|------------|-------------------|-------------------|-------------------|\n");
  printf("\nResult (white): %5.0f\n\n", ToScore(&whiteEval, board) - ToScore(&blackEval, board));
}
#endif
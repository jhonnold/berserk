#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "types.h"

#define S(mg, eg) (makeScore((mg), (eg)))

const int PAWN_VALUE = S(100, 150);
const int KNIGHT_VALUE = S(475, 475);
const int BISHOP_VALUE = S(475, 475);
const int ROOK_VALUE = S(700, 775);
const int QUEEN_VALUE = S(1600, 1450);
const int KING_VALUE = S(30000, 30000);

const int BISHOP_PAIR = S(50, 50);

// clang-format off
const int PAWN_PSQT[] = {
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   5,  40), S(   5,  40), S(  20,  40), S(  25,  35), S(  25,  35), S(  20,  40), S(   5,  40), S(   5,  40),
  S(   0,  20), S(   0,  20), S(  10,  20), S(  15,  20), S(  15,  20), S(  10,  20), S(   0,  20), S(   0,  20),
  S(  -5,  10), S(  -5,  10), S(   7,  10), S(  10,  10), S(  10,  10), S(   7,  10), S(  -5,  10), S(  -5,  10),
  S(  -5,   0), S(  -5,   0), S(   5,   0), S(   5,   0), S(   5,   0), S(   5,   0), S(  -5,   0), S(  -5,   0),
  S(  -4,  -5), S(  -5,  -5), S(   0,  -5), S(   1,  -5), S(   1,  -5), S(   0,  -5), S(  -5,  -5), S(  -4,  -5),
  S( -10, -10), S(  -5, -10), S(   0, -10), S(  -1, -10), S(  -1, -10), S(   0, -10), S(  -5, -10), S( -10, -10),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)
};

const int KNIGHT_PSQT[] = {
  S( -60, -30), S( -15, -25), S( -10, -20), S(  -5, -15), S(  -5, -15), S( -10, -20), S( -15, -25), S( -60, -30),
  S( -10, -25), S(   0, -20), S(   5, -10), S(  10,  -5), S(  10,  -5), S(   5, -10), S(   0, -20), S( -10, -25),
  S( -10, -20), S(  10, -10), S(  20,   0), S(  30,  10), S(  30,  10), S(  20,   0), S(  10, -10), S( -10, -20),
  S( -10, -15), S(  10,  -5), S(  20,  10), S(  30,  20), S(  30,  20), S(  20,  10), S(  10,  -5), S( -10, -15),
  S(  -5, -15), S(   5,  -5), S(  10,  10), S(  20,  20), S(  20,  20), S(  10,  10), S(   5,  -5), S(  -5, -15),
  S( -10, -20), S(   0, -10), S(   5,   0), S(  10,  10), S(  10,  10), S(   5,   0), S(   0, -10), S( -10, -20),
  S( -20, -25), S( -15, -20), S(  -5, -10), S(   0,  -5), S(   0,  -5), S(  -5, -10), S( -15, -20), S( -20, -25),
  S( -30, -30), S( -25, -25), S( -20, -20), S( -15, -15), S( -15, -15), S( -20, -20), S( -25, -25), S( -30, -30)
};

const int KNIGHT_POST_PSQT[] = {
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   3,   3), S(  10,   0), S(  10,  10), S(   3,   3), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   2,   2), S(   5,   5), S(  10,  10), S(  10,  10), S(   5,   5), S(   2,   2), S(   0,   0),
  S(   0,   0), S(   2,   2), S(   5,   5), S(  10,  10), S(  10,  10), S(   5,   5), S(   2,   2), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   1,   1), S(   1,   1), S(   1,   1), S(   1,   1), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)
};

const int BISHOP_PSQT[] = {
  S( -10, -12), S( -10, -12), S( -10, -12), S(  -6,  -8), S(  -6,  -8), S( -10, -12), S( -10, -12), S( -10, -12),
  S( -10, -12), S(   2,   2), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   2,   2), S( -10, -12),
  S( -10, -12), S(   0,   0), S(   4,   6), S(   4,   6), S(   4,   6), S(   4,   6), S(   0,   0), S( -10, -12),
  S(  -6,  -8), S(   0,   0), S(   4,   6), S(  10,  10), S(  10,  10), S(   4,   6), S(   0,   0), S(  -6,  -8),
  S(  -6,  -8), S(   0,   0), S(   4,   6), S(  10,  10), S(  10,  10), S(   4,   6), S(   0,   0), S(  -6,  -8),
  S( -10, -12), S(   1,   0), S(   4,   6), S(   4,   6), S(   4,   6), S(   4,   6), S(   1,   0), S( -10, -12),
  S( -14, -14), S(   2,   2), S(   1,   0), S(   2,   2), S(   2,   2), S(   1,   0), S(   2,   2), S( -14, -14),
  S( -14, -14), S( -14, -14), S( -10, -10), S(  -6,  -6), S(  -6,  -6), S( -10, -10), S( -14, -14), S( -14, -14)
};

const int ROOK_PSQT[] = {
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
};

const int QUEEN_PSQT[] = {
  S(   0, -25), S(   0, -20), S(   0, -15), S(   0, -10), S(   0, -10), S(   0, -15), S(   0, -20), S(   0, -25),
  S(   0, -20), S(   0, -10), S(   0,  -5), S(   0,   0), S(   0,   0), S(   0,  -5), S(   0, -10), S(   0, -20),
  S(   0, -15), S(   0,  -5), S(   0,   0), S(   0,   5), S(   0,   5), S(   0,   0), S(   0,  -5), S(   0, -15),
  S(   0, -10), S(   0,   0), S(   0,   5), S(   0,  10), S(   0,  10), S(   0,   5), S(   0,   0), S(   0, -10),
  S(   0, -10), S(   0,   0), S(   0,   5), S(   0,  10), S(   0,  10), S(   0,   5), S(   0,   0), S(   0, -10),
  S(   0, -15), S(   0,  -5), S(   0,   0), S(   0,   5), S(   0,   5), S(   0,   0), S(   0,  -5), S(   0, -15),
  S(   0, -20), S(   0, -10), S(   0,  -5), S(   0,   0), S(   0,   0), S(   5,  -5), S(   0, -10), S(   0, -20),
  S( -10, -25), S( -10, -20), S( -10, -15), S( -10, -10), S( -10, -10), S( -10, -15), S( -10, -20), S( -10, -25),
};

const int KING_PSQT[] = {
  S( -40, -70), S( -30, -50), S( -50, -35), S( -70, -25), S( -70, -25), S( -50, -35), S( -30, -50), S( -40, -70),
  S( -30, -50), S( -20, -25), S( -40, -10), S( -60,   0), S( -60,   0), S( -40, -10), S( -20, -25), S( -30, -50),
  S( -20, -35), S( -10, -10), S( -30,   0), S( -50,  15), S( -50,  15), S( -30,   0), S( -10, -10), S( -20, -35),
  S( -10, -25), S(   0,   0), S( -20,  15), S( -40,  30), S( -40,  30), S( -20,  15), S(   0,   0), S( -10, -25),
  S(   0, -25), S(  10,   0), S( -10,  15), S( -30,  30), S( -30,  30), S( -10,  15), S(  10,   0), S(   0, -25),
  S(  10, -35), S(  20, -10), S(   0,   0), S( -20,  15), S( -20,  15), S(   0,   0), S(  20, -10), S(  10, -35),
  S(  20, -50), S(  30, -25), S(  20, -10), S(   0,   0), S(   0,   0), S(  20, -10), S(  30, -25), S(  20, -50),
  S(  30, -70), S(  40, -50), S(  20, -35), S(  10, -25), S(  10, -25), S(  20, -35), S(  40, -50), S(  30, -70),
};

const int MATERIAL_VALUES[12] = {
  PAWN_VALUE, PAWN_VALUE, 
  KNIGHT_VALUE, KNIGHT_VALUE, 
  BISHOP_VALUE, BISHOP_VALUE,
  ROOK_VALUE, ROOK_VALUE, 
  QUEEN_VALUE,  QUEEN_VALUE, 
  KING_VALUE,   KING_VALUE
};

// clang-format on

const int KNIGHT_MOBILITIES[] = {
    S(-60, -75), S(-30, -60), S(-10, -45), S(0, -30), S(5, -15), S(10, 0), S(15, 0), S(30, 0), S(50, 0),
};

const int BISHOP_MOBILITIES[] = {
    S(-50, -75), S(-25, -50), S(0, -25), S(0, 0),   S(0, 15),  S(5, 30),  S(10, 40),
    S(15, 50),   S(20, 55),   S(25, 60), S(30, 65), S(35, 70), S(40, 75), S(45, 80),
};

const int ROOK_MOBILITIES[] = {
    S(0, -60), S(0, -45), S(0, -30), S(0, -15), S(0, 0),  S(1, 5),  S(2, 10),  S(3, 15),
    S(4, 20),  S(5, 25),  S(6, 30),  S(7, 40),  S(8, 50), S(9, 60), S(10, 70),
};

const int QUEEN_MOBILITIES[] = {
    S(-10, -75), S(-7, -50), S(-4, -25), S(-1, 0),  S(2, 2),   S(3, 5),   S(4, 10),  S(5, 15),  S(6, 20),  S(7, 12),
    S(8, 30),    S(9, 35),   S(10, 36),  S(10, 37), S(10, 38), S(10, 39), S(11, 42), S(11, 45), S(11, 48), S(12, 51),
    S(12, 54),   S(12, 57),  S(13, 60),  S(13, 63), S(13, 66), S(14, 69), S(14, 72), S(14, 75),
};

const int MAX_PHASE = 24;
const int PHASE_MULTIPLIERS[] = {0, 0, 1, 1, 1, 1, 2, 2, 4, 4, 0, 0};

const int DOUBLED_PAWN = S(10, 10);
const int ISOLATED_PAWN = S(10, 10);
const int WEAK_PAWN = S(10, 5);
const int BACKWARDS_PAWN = S(10, 2);
const int PASSED_PAWN[2][8] = {
    {
        S(0, 0),
        S(75, 150),
        S(50, 100),
        S(15, 75),
        S(12, 60),
        S(11, 45),
        S(10, 45),
        S(0, 0),
    },
    // reverse of above
    {
        S(0, 0),
        S(10, 45),
        S(11, 45),
        S(12, 60),
        S(15, 75),
        S(50, 100),
        S(75, 150),
        S(0, 0),
    },
};

// pawn shield will act more as a scalar
const int PAWN_SHIELD = S(100, 0);

const int KING_AIR = S(-20, 10);
const int DEFENDED_MINOR = S(5, 2);
const int KNIGHT_OUTPOST = 5;
const int GOOD_KNIGHT_OUTPOST = 10;
const int ROOK_OPEN_FILE = 20;
const int ROOK_SEMI_OPEN_FILE = 10;
const int ROOK_SEVENTH_RANK = 20;
const int ROOK_TRAPPED = S(75, 75);
const int BISHOP_TRAPPED = S(150, 150);
const int ROOK_OPPOSITE_KING = S(10, 0);

const int ATTACK_WEIGHTS[] = {0, 50, 75, 88, 94, 97, 99};

// clang-format off
int MATERIAL_AND_PSQT_VALUES[12][64];

void initPositionValues() {
  for (int sq = 0; sq < 64; sq++) {
    MATERIAL_AND_PSQT_VALUES[0][sq] =
        S(scoreMG(PAWN_VALUE) + scoreMG(PAWN_PSQT[sq]), scoreEG(PAWN_VALUE) + scoreEG(PAWN_PSQT[sq]));
    MATERIAL_AND_PSQT_VALUES[1][mirror[sq]] =
        S(scoreMG(PAWN_VALUE) + scoreMG(PAWN_PSQT[sq]), scoreEG(PAWN_VALUE) + scoreEG(PAWN_PSQT[sq]));

    MATERIAL_AND_PSQT_VALUES[2][sq] =
        S(scoreMG(KNIGHT_VALUE) + scoreMG(KNIGHT_PSQT[sq]), scoreEG(KNIGHT_VALUE) + scoreEG(KNIGHT_PSQT[sq]));
    MATERIAL_AND_PSQT_VALUES[3][mirror[sq]] =
        S(scoreMG(KNIGHT_VALUE) + scoreMG(KNIGHT_PSQT[sq]), scoreEG(KNIGHT_VALUE) + scoreEG(KNIGHT_PSQT[sq]));

    MATERIAL_AND_PSQT_VALUES[4][sq] =
        S(scoreMG(BISHOP_VALUE) + scoreMG(BISHOP_PSQT[sq]), scoreEG(BISHOP_VALUE) + scoreEG(BISHOP_PSQT[sq]));
    MATERIAL_AND_PSQT_VALUES[5][mirror[sq]] =
        S(scoreMG(BISHOP_VALUE) + scoreMG(BISHOP_PSQT[sq]), scoreEG(BISHOP_VALUE) + scoreEG(BISHOP_PSQT[sq]));

    MATERIAL_AND_PSQT_VALUES[6][sq] =
        S(scoreMG(ROOK_VALUE) + scoreMG(ROOK_PSQT[sq]), scoreEG(ROOK_VALUE) + scoreEG(ROOK_PSQT[sq]));
    MATERIAL_AND_PSQT_VALUES[7][mirror[sq]] =
        S(scoreMG(ROOK_VALUE) + scoreMG(ROOK_PSQT[sq]), scoreEG(ROOK_VALUE) + scoreEG(ROOK_PSQT[sq]));

    MATERIAL_AND_PSQT_VALUES[8][sq] =
        S(scoreMG(QUEEN_VALUE) + scoreMG(QUEEN_PSQT[sq]), scoreEG(QUEEN_VALUE) + scoreEG(QUEEN_PSQT[sq]));
    MATERIAL_AND_PSQT_VALUES[9][mirror[sq]] =
        S(scoreMG(QUEEN_VALUE) + scoreMG(QUEEN_PSQT[sq]), scoreEG(QUEEN_VALUE) + scoreEG(QUEEN_PSQT[sq]));

    MATERIAL_AND_PSQT_VALUES[10][sq] =
        S(scoreMG(KING_VALUE) + scoreMG(KING_PSQT[sq]), scoreEG(KING_VALUE) + scoreEG(KING_PSQT[sq]));
    MATERIAL_AND_PSQT_VALUES[11][mirror[sq]] =
        S(scoreMG(KING_VALUE) + scoreMG(KING_PSQT[sq]), scoreEG(KING_VALUE) + scoreEG(KING_PSQT[sq]));
  }
}
// clang-format on

inline int getPhase(Board* board) {
  int currentPhase = 0;
  for (int i = 2; i < 10; i++)
    currentPhase += PHASE_MULTIPLIERS[i] * bits(board->pieces[i]);
  currentPhase = MAX_PHASE - currentPhase;

  return ((currentPhase << 8) + (MAX_PHASE / 2)) / MAX_PHASE;
}

inline int taper(int score, int phase) { return (scoreMG(score) * (256 - phase) + (scoreEG(score) * phase)) / 256; }

int Evaluate(Board* board) {
  int phase = getPhase(board);

  int score = 0;
  for (int i = board->side; i < 12; i += 2) {
    BitBoard pieces = board->pieces[i];
    while (pieces) {
      int sq = lsb(pieces);

      score += taper(MATERIAL_AND_PSQT_VALUES[i][sq], phase);
      popLsb(pieces);
    }
  }

  for (int i = board->xside; i < 12; i += 2) {
    BitBoard pieces = board->pieces[i];
    while (pieces) {
      int sq = lsb(pieces);

      score -= taper(MATERIAL_AND_PSQT_VALUES[i][sq], phase);
      popLsb(pieces);
    }
  }

  if (bits(board->pieces[BISHOP[board->side]]) > 1)
    score += taper(BISHOP_PAIR, phase);

  if (bits(board->pieces[BISHOP[board->xside]]) > 1)
    score -= taper(BISHOP_PAIR, phase);

  BitBoard myPawns = board->pieces[PAWN[board->side]];
  BitBoard opponentPawns = board->pieces[PAWN[board->xside]];
  BitBoard allPawns = myPawns | opponentPawns;

  BitBoard myPawnAttacksE = shift(myPawns, PAWN_DIRECTIONS[board->side] - 1);
  BitBoard myPawnAttacksW = shift(myPawns, PAWN_DIRECTIONS[board->side] + 1);
  BitBoard opponentPawnAttacksE = shift(opponentPawns, PAWN_DIRECTIONS[board->xside] - 1);
  BitBoard opponentPawnAttacksW = shift(opponentPawns, PAWN_DIRECTIONS[board->xside] + 1);

  BitBoard myPawnAttacks = myPawnAttacksE | myPawnAttacksW;
  BitBoard myBlockedAndHomePawns = (shift(board->occupancies[BOTH], PAWN_DIRECTIONS[board->xside]) |
                                    HOME_RANKS[board->side] | THIRD_RANKS[board->side]) &
                                   board->pieces[PAWN[board->side]];
  BitBoard oppoPawnAttacks = opponentPawnAttacksE | opponentPawnAttacksW;
  BitBoard opponentBlockedAndHomePawns = (shift(board->occupancies[BOTH], PAWN_DIRECTIONS[board->side]) |
                                          HOME_RANKS[board->xside] | THIRD_RANKS[board->xside]) &
                                         board->pieces[PAWN[board->xside]];

  // PAWNS
  // source for weak pawns: https://www.stmintz.com/ccc/index.php?id=56431

  BitBoard weakPawns = myPawns & ~myPawnAttacks;
  weakPawns &= ~shift(myPawnAttacks & ~allPawns, -PAWN_DIRECTIONS[board->side]);

  BitBoard singleStep = shift(myPawns, PAWN_DIRECTIONS[board->side]) & ~allPawns;
  BitBoard doubleStep = shift(singleStep & THIRD_RANKS[board->side], PAWN_DIRECTIONS[board->side]) & ~allPawns;

  weakPawns &=
      ~(shift(singleStep, PAWN_DIRECTIONS[board->side] + 1) | shift(singleStep, PAWN_DIRECTIONS[board->side] - 1) |
        shift(doubleStep, PAWN_DIRECTIONS[board->side] + 1) | shift(doubleStep, PAWN_DIRECTIONS[board->side] - 1));

  score -= (taper(WEAK_PAWN, phase) * bits(weakPawns));

  BitBoard backwards = shift(weakPawns, PAWN_DIRECTIONS[board->side]) & ~allPawns & oppoPawnAttacks;
  score -= (taper(BACKWARDS_PAWN, phase) * bits(backwards));

  weakPawns = opponentPawns & ~oppoPawnAttacks;
  weakPawns &= ~shift(oppoPawnAttacks & ~allPawns, -PAWN_DIRECTIONS[board->xside]);

  singleStep = shift(opponentPawns, PAWN_DIRECTIONS[board->xside]) & ~allPawns;
  doubleStep = shift(singleStep & THIRD_RANKS[board->xside], PAWN_DIRECTIONS[board->xside]) & ~allPawns;

  weakPawns &=
      ~(shift(singleStep, PAWN_DIRECTIONS[board->xside] + 1) | shift(singleStep, PAWN_DIRECTIONS[board->xside] - 1) |
        shift(doubleStep, PAWN_DIRECTIONS[board->xside] + 1) | shift(doubleStep, PAWN_DIRECTIONS[board->xside] - 1));

  score += (taper(WEAK_PAWN, phase) * bits(weakPawns));

  backwards = shift(weakPawns, PAWN_DIRECTIONS[board->xside]) & ~allPawns & myPawnAttacks;
  score += (taper(BACKWARDS_PAWN, phase) * bits(backwards));

  // PASSED PAWNS
  BitBoard passers = myPawns & ~getPawnSpans(opponentPawns, board->xside);
  for (; passers; popLsb(passers)) {
    int sq = lsb(passers);
    int rank = sq >> 3;

    score += taper(PASSED_PAWN[board->side][rank], phase);
  }

  passers = opponentPawns & ~getPawnSpans(myPawns, board->side);
  for (; passers; popLsb(passers)) {
    int sq = lsb(passers);
    int rank = sq >> 3;

    score -= taper(PASSED_PAWN[board->xside][rank], phase);
  }

  for (BitBoard pawns = myPawns; pawns; popLsb(pawns)) {
    int sq = lsb(pawns);
    int file = file(sq);

    if (bits(FILE_MASKS[file] & pawns) > 1)
      score -= taper(DOUBLED_PAWN, phase);

    if (!((file > 0 ? FILE_MASKS[file - 1] : 0) & myPawns) && !((file < 7 ? FILE_MASKS[file + 1] : 0) & myPawns))
      score -= taper(ISOLATED_PAWN, phase);
  }

  for (BitBoard pawns = opponentPawns; pawns; popLsb(pawns)) {
    int sq = lsb(pawns);
    int file = file(sq);

    if (bits(FILE_MASKS[file] & pawns) > 1)
      score += taper(DOUBLED_PAWN, phase);

    if (!((file > 0 ? FILE_MASKS[file - 1] : 0) & opponentPawns) &&
        !((file < 7 ? FILE_MASKS[file + 1] : 0) & opponentPawns))
      score += taper(ISOLATED_PAWN, phase);
  }

  int kingSq = lsb(board->pieces[KING[board->side]]);
  int oppoKingSq = lsb(board->pieces[KING[board->xside]]);
  BitBoard myKingArea = getKingAttacks(kingSq);
  BitBoard oppoKingArea = getKingAttacks(oppoKingSq);
  int myAttackScore = 0;
  int oppoAttackScore = 0;
  int myTotalAttackers = 0;
  int oppoTotalAttackers = 0;

  // KNIGHTS
  BitBoard myKnights = board->pieces[KNIGHT[board->side]];
  BitBoard oppoKnights = board->pieces[KNIGHT[board->xside]];

  while (myKnights) {
    int sq = lsb(myKnights);

    BitBoard mobilitySquares = getKnightAttacks(sq) & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(KNIGHT_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getKnightAttacks(sq) & oppoKingArea;
    if (attacksNearKing) {
      myTotalAttackers++;
      myAttackScore += 20 * bits(attacksNearKing);
    }

    int postSq = board->side == WHITE ? sq : mirror[sq];
    if (KNIGHT_POST_PSQT[postSq] && getPawnAttacks(sq, board->xside) & myPawns) {
      score += taper(KNIGHT_POST_PSQT[postSq], phase);
      if (!(fill(getPawnAttacks(sq, board->side), PAWN_DIRECTIONS[board->side]) & opponentPawns)) {
        score += GOOD_KNIGHT_OUTPOST;
      }
    }

    popLsb(myKnights);
  }

  while (oppoKnights) {
    int sq = lsb(oppoKnights);

    BitBoard mobilitySquares = getKnightAttacks(sq) & ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(KNIGHT_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getKnightAttacks(sq) & myKingArea;
    if (attacksNearKing) {
      oppoTotalAttackers++;
      oppoAttackScore += 20 * bits(attacksNearKing);
    }

    int postSq = board->xside == WHITE ? sq : mirror[sq];
    if (KNIGHT_POST_PSQT[postSq] && getPawnAttacks(sq, board->side) & opponentPawns) {
      score -= taper(KNIGHT_POST_PSQT[postSq], phase);
      if (!(fill(getPawnAttacks(sq, board->xside), PAWN_DIRECTIONS[board->xside]) & myPawns)) {
        score -= GOOD_KNIGHT_OUTPOST;
      }
    }

    popLsb(oppoKnights);
  }

  // Bishops
  BitBoard myBishops = board->pieces[BISHOP[board->side]];
  BitBoard opponentBishops = board->pieces[BISHOP[board->xside]];

  while (myBishops) {
    int sq = lsb(myBishops);

    BitBoard mobilitySquares = getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->side]]) &
                               ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(BISHOP_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing =
        getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->side]]) & oppoKingArea;
    if (attacksNearKing) {
      myTotalAttackers++;
      myAttackScore += 20 * bits(attacksNearKing);
    }

    if (board->side == WHITE) {
      if ((sq == A7 || sq == B8) && getBit(opponentPawns, B6) && getBit(opponentPawns, C7))
        score -= taper(BISHOP_TRAPPED, phase);
      else if ((sq == H7 || sq == G8) && getBit(opponentPawns, F7) && getBit(opponentPawns, G6))
        score -= taper(BISHOP_TRAPPED, phase);
    } else {
      if ((sq == A2 || sq == B1) && getBit(opponentPawns, B3) && getBit(opponentPawns, C2))
        score -= taper(BISHOP_TRAPPED, phase);
      else if ((sq == H2 || sq == G1) && getBit(opponentPawns, G3) && getBit(opponentPawns, F2))
        score -= taper(BISHOP_TRAPPED, phase);
    }

    popLsb(myBishops);
  }

  while (opponentBishops) {
    int sq = lsb(opponentBishops);

    BitBoard mobilitySquares = getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->xside]]) &
                               ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(BISHOP_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing =
        getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->xside]]) & myKingArea;
    if (attacksNearKing) {
      oppoTotalAttackers++;
      oppoAttackScore += 20 * bits(attacksNearKing);
    }

    if (board->xside == WHITE) {
      if ((sq == A7 || sq == B8) && getBit(myPawns, B6) && getBit(myPawns, C7))
        score += taper(BISHOP_TRAPPED, phase);
      else if ((sq == H7 || sq == G8) && getBit(myPawns, F7) && getBit(myPawns, G6))
        score += taper(BISHOP_TRAPPED, phase);
    } else {
      if ((sq == A2 || sq == B1) && getBit(myPawns, B3) && getBit(myPawns, C2))
        score += taper(BISHOP_TRAPPED, phase);
      else if ((sq == H2 || sq == G1) && getBit(myPawns, G3) && getBit(myPawns, F2))
        score += taper(BISHOP_TRAPPED, phase);
    }

    popLsb(opponentBishops);
  }

  score += taper(DEFENDED_MINOR, phase) *
           bits(myPawnAttacks & (board->pieces[KNIGHT[board->side]] | board->pieces[BISHOP[board->side]]));

  score -= taper(DEFENDED_MINOR, phase) *
           bits(oppoPawnAttacks & (board->pieces[KNIGHT[board->xside]] | board->pieces[BISHOP[board->xside]]));

  // Rooks
  BitBoard myRooks = board->pieces[ROOK[board->side]];
  BitBoard opponentRooks = board->pieces[ROOK[board->xside]];

  while (myRooks) {
    int sq = lsb(myRooks);
    int file = file(sq);

    BitBoard mobilitySquares = getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->side]] ^
                                                      board->pieces[ROOK[board->side]]) &
                               ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(ROOK_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->side]] ^
                                                      board->pieces[ROOK[board->side]]) &
                               oppoKingArea;
    if (attacksNearKing) {
      myTotalAttackers++;
      myAttackScore += 40 * bits(attacksNearKing);
    }

    if (board->side == WHITE && (sq >> 3) == 1) {
      score += ROOK_SEVENTH_RANK;
    } else if (board->side == BLACK && (sq >> 3) == 6) {
      score += ROOK_SEVENTH_RANK;
    }

    if (!(FILE_MASKS[file] & allPawns)) {
      score += ROOK_OPEN_FILE;
      if (file == file(oppoKingSq))
        score += taper(ROOK_OPPOSITE_KING, phase);
    } else if (!(FILE_MASKS[file] & myPawns)) {
      score += ROOK_SEMI_OPEN_FILE;
      if (file == file(oppoKingSq))
        score += taper(ROOK_OPPOSITE_KING, phase);
    }

    if (board->side == WHITE) {
      if ((sq == A1 || sq == A2 || sq == B1) && (kingSq == C1 || kingSq == B1))
        score -= taper(ROOK_TRAPPED, phase);
      else if ((sq == H1 || sq == H2 || sq == G1) && (kingSq == F1 || kingSq == G1))
        score -= taper(ROOK_TRAPPED, phase);
    } else {
      if ((sq == A8 || sq == A7 || sq == B8) && (kingSq == B8 || kingSq == C8))
        score -= taper(ROOK_TRAPPED, phase);
      else if ((sq == H8 || sq == H7 || sq == G8) && (kingSq == F8 || kingSq == G8))
        score -= taper(ROOK_TRAPPED, phase);
    }

    popLsb(myRooks);
  }

  while (opponentRooks) {
    int sq = lsb(opponentRooks);
    int file = file(sq);

    BitBoard mobilitySquares = getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->xside]] ^
                                                      board->pieces[ROOK[board->xside]]) &
                               ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(ROOK_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->xside]] ^
                                                      board->pieces[ROOK[board->xside]]) &
                               myKingArea;
    if (attacksNearKing) {
      oppoTotalAttackers++;
      oppoAttackScore += 40 * bits(attacksNearKing);
    }

    if (board->xside == WHITE && (sq >> 3) == 1) {
      score -= ROOK_SEVENTH_RANK;
    } else if (board->xside == BLACK && (sq >> 3) == 6) {
      score -= ROOK_SEVENTH_RANK;
    }

    if (!(FILE_MASKS[file] & allPawns)) {
      score -= ROOK_OPEN_FILE;
      if (file == file(kingSq))
        score -= taper(ROOK_OPPOSITE_KING, phase);
    } else if (!(FILE_MASKS[file] & opponentPawns)) {
      score -= ROOK_SEMI_OPEN_FILE;
      if (file == file(kingSq))
        score -= taper(ROOK_OPPOSITE_KING, phase);
    }

    if (board->xside == WHITE) {
      if ((sq == A1 || sq == A2 || sq == B1) && (oppoKingSq == C1 || oppoKingSq == B1))
        score += taper(ROOK_TRAPPED, phase);
      else if ((sq == H1 || sq == H2 || sq == G1) && (oppoKingSq == F1 || oppoKingSq == G1))
        score += taper(ROOK_TRAPPED, phase);
    } else {
      if ((sq == A8 || sq == A7 || sq == B8) && (oppoKingSq == B8 || oppoKingSq == C8))
        score += taper(ROOK_TRAPPED, phase);
      else if ((sq == H8 || sq == H7 || sq == G8) && (oppoKingSq == F8 || oppoKingSq == G8))
        score += taper(ROOK_TRAPPED, phase);
    }

    popLsb(opponentRooks);
  }

  // Queens
  BitBoard myQueens = board->pieces[QUEEN[board->side]];
  BitBoard opponentQueens = board->pieces[QUEEN[board->xside]];

  while (myQueens) {
    int sq = lsb(myQueens);

    BitBoard mobilitySquares =
        getQueenAttacks(sq, board->occupancies[BOTH]) & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(QUEEN_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getQueenAttacks(sq, board->occupancies[BOTH]) & oppoKingArea;
    if (attacksNearKing) {
      myTotalAttackers++;
      myAttackScore += 80 * bits(attacksNearKing);
    }

    popLsb(myQueens);
  }

  while (opponentQueens) {
    int sq = lsb(opponentQueens);

    BitBoard mobilitySquares =
        getQueenAttacks(sq, board->occupancies[BOTH]) & ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(QUEEN_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getQueenAttacks(sq, board->occupancies[BOTH]) & myKingArea;
    if (attacksNearKing) {
      oppoTotalAttackers++;
      oppoAttackScore += 80 * bits(attacksNearKing);
    }

    popLsb(opponentQueens);
  }

  // KING SAFETY

  score += myAttackScore * ATTACK_WEIGHTS[myTotalAttackers] / 100;
  score -= oppoAttackScore * ATTACK_WEIGHTS[oppoTotalAttackers] / 100;

  int kingFile = file(kingSq);
  if (board->side == WHITE) {
    for (int c = kingFile - 1; c <= kingFile + 1; c++) {
      if (c < 0 || c > 7)
        continue;

      BitBoard file = myPawns & FILE_MASKS[c];

      if (file) {
        int pawnRank = msb(file) / 8;
        score += -taper(PAWN_SHIELD, phase) * (36 - pawnRank * pawnRank) / 100;
      } else {
        score += -taper(PAWN_SHIELD, phase) * 36 / 100;
      }
    }
  } else {
    for (int c = kingFile - 1; c <= kingFile + 1; c++) {
      if (c < 0 || c > 7)
        continue;

      BitBoard file = myPawns & FILE_MASKS[c];
      if (file) {
        int pawnRank = 7 - lsb(file) / 8;
        score += taper(PAWN_SHIELD, phase) * -(36 - pawnRank * pawnRank) / 100;
      } else {
        score += -taper(PAWN_SHIELD, phase) * 36 / 100;
      }
    }
  }

  kingFile = file(oppoKingSq);
  if (board->xside == WHITE) {
    for (int c = kingFile - 1; c <= kingFile + 1; c++) {
      if (c < 0 || c > 7)
        continue;

      BitBoard file = opponentPawns & FILE_MASKS[c];

      if (file) {
        int pawnRank = msb(file) / 8;
        score -= -taper(PAWN_SHIELD, phase) * (36 - pawnRank * pawnRank) / 100;
      } else {
        score -= -taper(PAWN_SHIELD, phase) * 36 / 100;
      }
    }
  } else {
    for (int c = kingFile - 1; c <= kingFile + 1; c++) {
      if (c < 0 || c > 7)
        continue;

      BitBoard file = opponentPawns & FILE_MASKS[c];
      if (file) {
        int pawnRank = 7 - lsb(file) / 8;
        score -= taper(PAWN_SHIELD, phase) * -(36 - pawnRank * pawnRank) / 100;
      } else {
        score -= -taper(PAWN_SHIELD, phase) * 36 / 100;
      }
    }
  }

  return score;
}

// TODO: Mix this with normal eval
int TraceEvaluate(Board* board) {
  int phase = getPhase(board);

  int score = 0;

  printf("My Pieces\n---------\n\n");
  for (int i = board->side; i < 12; i += 2) {
    BitBoard pieces = board->pieces[i];
    while (pieces) {
      int sq = lsb(pieces);
      int pieceValue = taper(MATERIAL_AND_PSQT_VALUES[i][sq], phase);

      printf("%c (%s): %d\n", PIECE_TO_CHAR[i], SQ_TO_COORD[sq], pieceValue);
      score += pieceValue;
      popLsb(pieces);
    }
  }

  printf("\nEnemy Pieces\n------------\n\n");
  for (int i = board->xside; i < 12; i += 2) {
    BitBoard pieces = board->pieces[i];
    while (pieces) {
      int sq = lsb(pieces);
      int pieceValue = taper(MATERIAL_AND_PSQT_VALUES[i][sq], phase);

      printf("%c (%s): %d\n", PIECE_TO_CHAR[i], SQ_TO_COORD[sq], pieceValue);
      score -= pieceValue;
      popLsb(pieces);
    }
  }

  return score;
}
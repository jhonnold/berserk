#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "types.h"

#define S(mg, eg) (makeScore((mg), (eg)))

const int pawnValue = S(100, 150);
const int knightValue = S(475, 450);
const int bishopValue = S(475, 475);
const int rookValue = S(700, 775);
const int queenValue = S(1600, 1450);
const int kingValue = S(30000, 30000);
const int bishopPair = S(50, 50);

// clang-format off
// values concepts are inspired by cpw
const int pawnPositionValues[] = {
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   5,  40), S(   5,  40), S(  20,  40), S(  25,  35), S(  25,  35), S(  20,  40), S(   5,  40), S(   5,  40),
  S(   0,  20), S(   0,  20), S(  10,  20), S(  15,  20), S(  15,  20), S(  10,  20), S(   0,  20), S(   0,  20),
  S(  -5,  10), S(  -5,  10), S(   7,  10), S(  10,  10), S(  10,  10), S(   7,  10), S(  -5,  10), S(  -5,  10),
  S(  -5,   0), S(  -5,   0), S(   5,   0), S(   5,   0), S(   5,   0), S(   5,   0), S(  -5,   0), S(  -5,   0),
  S(  -4,  -5), S(  -5,  -5), S(   0,  -5), S(   1,  -5), S(   1,  -5), S(   0,  -5), S(  -5,  -5), S(  -4,  -5),
  S( -10, -10), S(  -5, -10), S(   0, -10), S(  -1, -10), S(  -1, -10), S(   0, -10), S(  -5, -10), S( -10, -10),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)
};

const int knightPositionValues[] = {
  S( -60, -30), S( -15, -25), S( -10, -20), S(  -5, -15), S(  -5, -15), S( -10, -20), S( -15, -25), S( -60, -30),
  S( -10, -25), S(   0, -20), S(   5, -10), S(  10,  -5), S(  10,  -5), S(   5, -10), S(   0, -20), S( -10, -25),
  S( -10, -20), S(  10, -10), S(  20,   0), S(  30,  10), S(  30,  10), S(  20,   0), S(  10, -10), S( -10, -20),
  S( -10, -15), S(  10,  -5), S(  20,  10), S(  30,  20), S(  30,  20), S(  20,  10), S(  10,  -5), S( -10, -15),
  S(  -5, -15), S(   5,  -5), S(  10,  10), S(  20,  20), S(  20,  20), S(  10,  10), S(   5,  -5), S(  -5, -15),
  S( -10, -20), S(   0, -10), S(   5,   0), S(  10,  10), S(  10,  10), S(   5,   0), S(   0, -10), S( -10, -20),
  S( -20, -25), S( -15, -20), S(  -5, -10), S(   0,  -5), S(   0,  -5), S(  -5, -10), S( -15, -20), S( -20, -25),
  S( -30, -30), S( -25, -25), S( -20, -20), S( -15, -15), S( -15, -15), S( -20, -20), S( -25, -25), S( -30, -30)
};

const int knightPostValues[] = {
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   3,   3), S(  10,   0), S(  10,  10), S(   3,   3), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   2,   2), S(   5,   5), S(  10,  10), S(  10,  10), S(   5,   5), S(   2,   2), S(   0,   0),
  S(   0,   0), S(   2,   2), S(   5,   5), S(  10,  10), S(  10,  10), S(   5,   5), S(   2,   2), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   1,   1), S(   1,   1), S(   1,   1), S(   1,   1), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)
};

const int bishopPositionValues[] = {
  S( -10, -12), S( -10, -12), S( -10, -12), S(  -6,  -8), S(  -6,  -8), S( -10, -12), S( -10, -12), S( -10, -12),
  S( -10, -12), S(   2,   2), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   2,   2), S( -10, -12),
  S( -10, -12), S(   0,   0), S(   4,   6), S(   4,   6), S(   4,   6), S(   4,   6), S(   0,   0), S( -10, -12),
  S(  -6,  -8), S(   0,   0), S(   4,   6), S(  10,  10), S(  10,  10), S(   4,   6), S(   0,   0), S(  -6,  -8),
  S(  -6,  -8), S(   0,   0), S(   4,   6), S(  10,  10), S(  10,  10), S(   4,   6), S(   0,   0), S(  -6,  -8),
  S( -10, -12), S(   1,   0), S(   4,   6), S(   4,   6), S(   4,   6), S(   4,   6), S(   1,   0), S( -10, -12),
  S( -14, -14), S(   2,   2), S(   1,   0), S(   2,   2), S(   2,   2), S(   1,   0), S(   2,   2), S( -14, -14),
  S( -14, -14), S( -14, -14), S( -10, -10), S(  -6,  -6), S(  -6,  -6), S( -10, -10), S( -14, -14), S( -14, -14)
};

const int rookPositionValues[] = {
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
  S(  -5,   0), S(   0,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(   0,   0), S(  -5,   0),
};

const int queenPositionValues[] = {
  S(   0, -25), S(   0, -20), S(   0, -15), S(   0, -10), S(   0, -10), S(   0, -15), S(   0, -20), S(   0, -25),
  S(   0, -20), S(   0, -10), S(   0,  -5), S(   0,   0), S(   0,   0), S(   0,  -5), S(   0, -10), S(   0, -20),
  S(   0, -15), S(   0,  -5), S(   0,   0), S(   0,   5), S(   0,   5), S(   0,   0), S(   0,  -5), S(   0, -15),
  S(   0, -10), S(   0,   0), S(   0,   5), S(   0,  10), S(   0,  10), S(   0,   5), S(   0,   0), S(   0, -10),
  S(   0, -10), S(   0,   0), S(   0,   5), S(   0,  10), S(   0,  10), S(   0,   5), S(   0,   0), S(   0, -10),
  S(   0, -15), S(   0,  -5), S(   0,   0), S(   0,   5), S(   0,   5), S(   0,   0), S(   0,  -5), S(   0, -15),
  S(   0, -20), S(   0, -10), S(   0,  -5), S(   0,   0), S(   0,   0), S(   5,  -5), S(   0, -10), S(   0, -20),
  S( -10, -25), S( -10, -20), S( -10, -15), S( -10, -10), S( -10, -10), S( -10, -15), S( -10, -20), S( -10, -25),
};

const int kingPositionValues[] = {
  S( -40, -70), S( -30, -50), S( -50, -35), S( -70, -25), S( -70, -25), S( -50, -35), S( -30, -50), S( -40, -70),
  S( -30, -50), S( -20, -25), S( -40, -10), S( -60,   0), S( -60,   0), S( -40, -10), S( -20, -25), S( -30, -50),
  S( -20, -35), S( -10, -10), S( -30,   0), S( -50,  15), S( -50,  15), S( -30,   0), S( -10, -10), S( -20, -35),
  S( -10, -25), S(   0,   0), S( -20,  15), S( -40,  30), S( -40,  30), S( -20,  15), S(   0,   0), S( -10, -25),
  S(   0, -25), S(  10,   0), S( -10,  15), S( -30,  30), S( -30,  30), S( -10,  15), S(  10,   0), S(   0, -25),
  S(  10, -35), S(  20, -10), S(   0,   0), S( -20,  15), S( -20,  15), S(   0,   0), S(  20, -10), S(  10, -35),
  S(  20, -50), S(  30, -25), S(  20, -10), S(   0,   0), S(   0,   0), S(  20, -10), S(  30, -25), S(  20, -50),
  S(  30, -70), S(  40, -50), S(  20, -35), S(  10, -25), S(  10, -25), S(  20, -35), S(  40, -50), S(  30, -70),
};

const int materialValues[12] = {
  pawnValue, pawnValue, 
  knightValue, knightValue, 
  bishopValue, bishopValue,
  rookValue, rookValue, 
  queenValue,  queenValue, 
  kingValue,   kingValue
};

// clang-format on

const int knightMobilities[] = {
    S(-60, -75), S(-30, -60), S(-10, -45), S(0, -30), S(5, -15), S(10, 0), S(15, 0), S(30, 0), S(50, 0),
};

const int bishopMobilities[] = {
    S(-50, -75), S(-25, -50), S(0, -25), S(0, 0),   S(0, 15),  S(5, 30),  S(10, 40),
    S(15, 50),   S(20, 55),   S(25, 60), S(30, 65), S(35, 70), S(40, 75), S(45, 80),
};

const int rookMobilities[] = {
    S(0, -60), S(0, -45), S(0, -30), S(0, -15), S(0, 0),  S(1, 5),  S(2, 10),  S(3, 15),
    S(4, 20),  S(5, 25),  S(6, 30),  S(7, 40),  S(8, 50), S(9, 60), S(10, 70),
};

const int queenMobilities[] = {
    S(-10, -75), S(-7, -50), S(-4, -25), S(-1, 0),  S(2, 2),   S(3, 5),   S(4, 10),  S(5, 15),  S(6, 20),  S(7, 12),
    S(8, 30),    S(9, 35),   S(10, 36),  S(10, 37), S(10, 38), S(10, 39), S(11, 42), S(11, 45), S(11, 48), S(12, 51),
    S(12, 54),   S(12, 57),  S(13, 60),  S(13, 63), S(13, 66), S(14, 69), S(14, 72), S(14, 75),
};

const int MAX_PHASE = 24;
const int phaseMultipliers[] = {0, 0, 1, 1, 1, 1, 2, 2, 4, 4, 0, 0};

const int doubledPawnPenalty = S(10, 10);
const int isolatedPawnPenalty = S(10, 10);
const int weakPawnPenalty = S(10, 5);
const int backwardsPawnPenalty = S(10, 2);
const int passedPawn[2][8] = {
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
const int pawnShield = S(100, 0);

const int kingAir = S(-20, 10);
const int minorDefended = S(5, 2);
const int knightOutpost = 5;
const int knightOutpostGood = 10;
const int openFile = 20;
const int semiOpenFile = 10;
const int rookOnSeventh = 20;
const int trappedRook = S(75, 75);
const int fischerBishopPenalty = S(150, 150);
const int rookOppositeKing = S(10, 0);

const int attackWeight[] = {0, 50, 75, 88, 94, 97, 99};

// clang-format off
int baseMaterialValues[12][64];

void initPositionValues() {
  for (int sq = 0; sq < 64; sq++) {
    baseMaterialValues[0][sq] =
        S(scoreMG(pawnValue) + scoreMG(pawnPositionValues[sq]), scoreEG(pawnValue) + scoreEG(pawnPositionValues[sq]));
    baseMaterialValues[1][mirror[sq]] =
        S(scoreMG(pawnValue) + scoreMG(pawnPositionValues[sq]), scoreEG(pawnValue) + scoreEG(pawnPositionValues[sq]));

    baseMaterialValues[2][sq] =
        S(scoreMG(knightValue) + scoreMG(knightPositionValues[sq]), scoreEG(knightValue) + scoreEG(knightPositionValues[sq]));
    baseMaterialValues[3][mirror[sq]] =
        S(scoreMG(knightValue) + scoreMG(knightPositionValues[sq]), scoreEG(knightValue) + scoreEG(knightPositionValues[sq]));

    baseMaterialValues[4][sq] =
        S(scoreMG(bishopValue) + scoreMG(bishopPositionValues[sq]), scoreEG(bishopValue) + scoreEG(bishopPositionValues[sq]));
    baseMaterialValues[5][mirror[sq]] =
        S(scoreMG(bishopValue) + scoreMG(bishopPositionValues[sq]), scoreEG(bishopValue) + scoreEG(bishopPositionValues[sq]));

    baseMaterialValues[6][sq] =
        S(scoreMG(rookValue) + scoreMG(rookPositionValues[sq]), scoreEG(rookValue) + scoreEG(rookPositionValues[sq]));
    baseMaterialValues[7][mirror[sq]] =
        S(scoreMG(rookValue) + scoreMG(rookPositionValues[sq]), scoreEG(rookValue) + scoreEG(rookPositionValues[sq]));

    baseMaterialValues[8][sq] =
        S(scoreMG(queenValue) + scoreMG(queenPositionValues[sq]), scoreEG(queenValue) + scoreEG(queenPositionValues[sq]));
    baseMaterialValues[9][mirror[sq]] =
        S(scoreMG(queenValue) + scoreMG(queenPositionValues[sq]), scoreEG(queenValue) + scoreEG(queenPositionValues[sq]));

    baseMaterialValues[10][sq] =
        S(scoreMG(kingValue) + scoreMG(kingPositionValues[sq]), scoreEG(kingValue) + scoreEG(kingPositionValues[sq]));
    baseMaterialValues[11][mirror[sq]] =
        S(scoreMG(kingValue) + scoreMG(kingPositionValues[sq]), scoreEG(kingValue) + scoreEG(kingPositionValues[sq]));
  }
}
// clang-format on

inline int getPhase(Board* board) {
  int currentPhase = 0;
  for (int i = 2; i < 10; i++)
    currentPhase += phaseMultipliers[i] * bits(board->pieces[i]);
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

      score += taper(baseMaterialValues[i][sq], phase);
      popLsb(pieces);
    }
  }

  for (int i = board->xside; i < 12; i += 2) {
    BitBoard pieces = board->pieces[i];
    while (pieces) {
      int sq = lsb(pieces);

      score -= taper(baseMaterialValues[i][sq], phase);
      popLsb(pieces);
    }
  }

  if (bits(board->pieces[BISHOP[board->side]]) > 1)
    score += taper(bishopPair, phase);

  if (bits(board->pieces[BISHOP[board->xside]]) > 1)
    score -= taper(bishopPair, phase);

  BitBoard myPawns = board->pieces[PAWN[board->side]];
  BitBoard opponentPawns = board->pieces[PAWN[board->xside]];
  BitBoard allPawns = myPawns | opponentPawns;

  BitBoard myPawnAttacksE = shift(myPawns, pawnDirections[board->side] - 1);
  BitBoard myPawnAttacksW = shift(myPawns, pawnDirections[board->side] + 1);
  BitBoard opponentPawnAttacksE = shift(opponentPawns, pawnDirections[board->xside] - 1);
  BitBoard opponentPawnAttacksW = shift(opponentPawns, pawnDirections[board->xside] + 1);

  BitBoard myPawnAttacks = myPawnAttacksE | myPawnAttacksW;
  BitBoard myBlockedAndHomePawns = (shift(board->occupancies[BOTH], pawnDirections[board->xside]) |
                                    homeRanks[board->side] | thirdRanks[board->side]) &
                                   board->pieces[PAWN[board->side]];
  BitBoard oppoPawnAttacks = opponentPawnAttacksE | opponentPawnAttacksW;
  BitBoard opponentBlockedAndHomePawns = (shift(board->occupancies[BOTH], pawnDirections[board->side]) |
                                          homeRanks[board->xside] | thirdRanks[board->xside]) &
                                         board->pieces[PAWN[board->xside]];

  // PAWNS
  // source for weak pawns: https://www.stmintz.com/ccc/index.php?id=56431

  BitBoard weakPawns = myPawns & ~myPawnAttacks;
  weakPawns &= ~shift(myPawnAttacks & ~allPawns, -pawnDirections[board->side]);

  BitBoard singleStep = shift(myPawns, pawnDirections[board->side]) & ~allPawns;
  BitBoard doubleStep = shift(singleStep & thirdRanks[board->side], pawnDirections[board->side]) & ~allPawns;

  weakPawns &=
      ~(shift(singleStep, pawnDirections[board->side] + 1) | shift(singleStep, pawnDirections[board->side] - 1) |
        shift(doubleStep, pawnDirections[board->side] + 1) | shift(doubleStep, pawnDirections[board->side] - 1));

  score -= (taper(weakPawnPenalty, phase) * bits(weakPawns));

  BitBoard backwards = shift(weakPawns, pawnDirections[board->side]) & ~allPawns & oppoPawnAttacks;
  score -= (taper(backwardsPawnPenalty, phase) * bits(backwards));

  weakPawns = opponentPawns & ~oppoPawnAttacks;
  weakPawns &= ~shift(oppoPawnAttacks & ~allPawns, -pawnDirections[board->xside]);

  singleStep = shift(opponentPawns, pawnDirections[board->xside]) & ~allPawns;
  doubleStep = shift(singleStep & thirdRanks[board->xside], pawnDirections[board->xside]) & ~allPawns;

  weakPawns &=
      ~(shift(singleStep, pawnDirections[board->xside] + 1) | shift(singleStep, pawnDirections[board->xside] - 1) |
        shift(doubleStep, pawnDirections[board->xside] + 1) | shift(doubleStep, pawnDirections[board->xside] - 1));

  score += (taper(weakPawnPenalty, phase) * bits(weakPawns));

  backwards = shift(weakPawns, pawnDirections[board->xside]) & ~allPawns & myPawnAttacks;
  score += (taper(backwardsPawnPenalty, phase) * bits(backwards));

  // PASSED PAWNS
  BitBoard passers = myPawns & ~getPawnSpans(opponentPawns, board->xside);
  for (; passers; popLsb(passers)) {
    int sq = lsb(passers);
    int rank = sq >> 3;

    score += taper(passedPawn[board->side][rank], phase);
  }

  passers = opponentPawns & ~getPawnSpans(myPawns, board->side);
  for (; passers; popLsb(passers)) {
    int sq = lsb(passers);
    int rank = sq >> 3;

    score -= taper(passedPawn[board->xside][rank], phase);
  }

  for (BitBoard pawns = myPawns; pawns; popLsb(pawns)) {
    int sq = lsb(pawns);
    int col = sq & 7;

    if (bits(columnMasks[col] & pawns) > 1)
      score -= taper(doubledPawnPenalty, phase);

    if (!((col > 0 ? columnMasks[col - 1] : 0) & myPawns) && !((col < 7 ? columnMasks[col + 1] : 0) & myPawns))
      score -= taper(isolatedPawnPenalty, phase);
  }

  for (BitBoard pawns = opponentPawns; pawns; popLsb(pawns)) {
    int sq = lsb(pawns);
    int col = sq & 7;

    if (bits(columnMasks[col] & pawns) > 1)
      score += taper(doubledPawnPenalty, phase);

    if (!((col > 0 ? columnMasks[col - 1] : 0) & opponentPawns) &&
        !((col < 7 ? columnMasks[col + 1] : 0) & opponentPawns))
      score += taper(isolatedPawnPenalty, phase);
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
    score += taper(knightMobilities[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getKnightAttacks(sq) & oppoKingArea;
    if (attacksNearKing) {
      myTotalAttackers++;
      myAttackScore += 20 * bits(attacksNearKing);
    }

    int postSq = board->side == WHITE ? sq : mirror[sq];
    if (knightPostValues[postSq] && getPawnAttacks(sq, board->xside) & myPawns) {
      score += taper(knightPostValues[postSq], phase);
      if (!(fill(getPawnAttacks(sq, board->side), pawnDirections[board->side]) & opponentPawns)) {
        score += knightOutpostGood;
      }
    }

    popLsb(myKnights);
  }

  while (oppoKnights) {
    int sq = lsb(oppoKnights);

    BitBoard mobilitySquares = getKnightAttacks(sq) & ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(knightMobilities[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getKnightAttacks(sq) & myKingArea;
    if (attacksNearKing) {
      oppoTotalAttackers++;
      oppoAttackScore += 20 * bits(attacksNearKing);
    }

    int postSq = board->xside == WHITE ? sq : mirror[sq];
    if (knightPostValues[postSq] && getPawnAttacks(sq, board->side) & opponentPawns) {
      score -= taper(knightPostValues[postSq], phase);
      if (!(fill(getPawnAttacks(sq, board->xside), pawnDirections[board->xside]) & myPawns)) {
        score -= knightOutpostGood;
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
    score += taper(bishopMobilities[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing =
        getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->side]]) & oppoKingArea;
    if (attacksNearKing) {
      myTotalAttackers++;
      myAttackScore += 20 * bits(attacksNearKing);
    }

    if (board->side == WHITE) {
      if ((sq == 8 || sq == 1) && getBit(opponentPawns, 17) && getBit(opponentPawns, 10))
        score -= taper(fischerBishopPenalty, phase);
      else if ((sq == 15 || sq == 6) && getBit(opponentPawns, 13) && getBit(opponentPawns, 22))
        score -= taper(fischerBishopPenalty, phase);
    } else {
      if ((sq == 57 || sq == 48) && getBit(opponentPawns, 41) && getBit(opponentPawns, 50))
        score -= taper(fischerBishopPenalty, phase);
      else if ((sq == 62 || sq == 55) && getBit(opponentPawns, 53) && getBit(opponentPawns, 46))
        score -= taper(fischerBishopPenalty, phase);
    }

    popLsb(myBishops);
  }

  while (opponentBishops) {
    int sq = lsb(opponentBishops);

    BitBoard mobilitySquares = getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->xside]]) &
                               ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(bishopMobilities[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing =
        getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->xside]]) & myKingArea;
    if (attacksNearKing) {
      oppoTotalAttackers++;
      oppoAttackScore += 20 * bits(attacksNearKing);
    }

    if (board->xside == WHITE) {
      if ((sq == 8 || sq == 1) && getBit(myPawns, 17) && getBit(myPawns, 10))
        score += taper(fischerBishopPenalty, phase);
      else if ((sq == 15 || sq == 6) && getBit(myPawns, 13) && getBit(myPawns, 22))
        score += taper(fischerBishopPenalty, phase);
    } else {
      if ((sq == 57 || sq == 48) && getBit(myPawns, 41) && getBit(myPawns, 50))
        score += taper(fischerBishopPenalty, phase);
      else if ((sq == 62 || sq == 55) && getBit(myPawns, 53) && getBit(myPawns, 46))
        score += taper(fischerBishopPenalty, phase);
    }

    popLsb(opponentBishops);
  }

  score += taper(minorDefended, phase) *
           bits(myPawnAttacks & (board->pieces[KNIGHT[board->side]] | board->pieces[BISHOP[board->side]]));

  score -= taper(minorDefended, phase) *
           bits(oppoPawnAttacks & (board->pieces[KNIGHT[board->xside]] | board->pieces[BISHOP[board->xside]]));

  // Rooks
  BitBoard myRooks = board->pieces[ROOK[board->side]];
  BitBoard opponentRooks = board->pieces[ROOK[board->xside]];

  while (myRooks) {
    int sq = lsb(myRooks);
    int col = sq & 7;

    BitBoard mobilitySquares = getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->side]] ^
                                                      board->pieces[ROOK[board->side]]) &
                               ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(rookMobilities[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->side]] ^
                                                      board->pieces[ROOK[board->side]]) &
                               oppoKingArea;
    if (attacksNearKing) {
      myTotalAttackers++;
      myAttackScore += 40 * bits(attacksNearKing);
    }

    if (board->side == WHITE && (sq >> 3) == 1) {
      score += rookOnSeventh;
    } else if (board->side == BLACK && (sq >> 3) == 6) {
      score += rookOnSeventh;
    }

    if (!(columnMasks[col] & allPawns)) {
      score += openFile;
      if (col == (oppoKingSq & 7))
        score += taper(rookOppositeKing, phase);
    } else if (!(columnMasks[col] & myPawns)) {
      score += semiOpenFile;
      if (col == (oppoKingSq & 7))
        score += taper(rookOppositeKing, phase);
    }

    // for the love of god magic constants
    if (board->side == WHITE) {
      if ((sq == 56 || sq == 48 || sq == 57) && (kingSq == 57 || kingSq == 58))
        score -= taper(trappedRook, phase);
      else if ((sq == 63 || sq == 55 || sq == 62) && (kingSq == 61 || kingSq == 62))
        score -= taper(trappedRook, phase);
    } else {
      if ((sq == 0 || sq == 8 || sq == 1) && (kingSq == 1 || kingSq == 2))
        score -= taper(trappedRook, phase);
      else if ((sq == 7 || sq == 15 || sq == 8) && (kingSq == 5 || kingSq == 6))
        score -= taper(trappedRook, phase);
    }

    popLsb(myRooks);
  }

  while (opponentRooks) {
    int sq = lsb(opponentRooks);
    int col = sq & 7;

    BitBoard mobilitySquares = getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->xside]] ^
                                                      board->pieces[ROOK[board->xside]]) &
                               ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(rookMobilities[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->xside]] ^
                                                      board->pieces[ROOK[board->xside]]) &
                               myKingArea;
    if (attacksNearKing) {
      oppoTotalAttackers++;
      oppoAttackScore += 40 * bits(attacksNearKing);
    }

    if (board->xside == WHITE && (sq >> 3) == 1) {
      score -= rookOnSeventh;
    } else if (board->xside == BLACK && (sq >> 3) == 6) {
      score -= rookOnSeventh;
    }

    if (!(columnMasks[col] & allPawns)) {
      score -= openFile;
      if (col == (kingSq & 7))
        score -= taper(rookOppositeKing, phase);
    } else if (!(columnMasks[col] & opponentPawns)) {
      score -= semiOpenFile;
      if (col == (kingSq & 7))
        score -= taper(rookOppositeKing, phase);
    }

    // for the love of god magic constants
    if (board->xside == WHITE) {
      if ((sq == 56 || sq == 48 || sq == 57) && (oppoKingSq == 57 || oppoKingSq == 58))
        score += taper(trappedRook, phase);
      else if ((sq == 63 || sq == 55 || sq == 62) && (oppoKingSq == 61 || oppoKingSq == 62))
        score += taper(trappedRook, phase);
    } else {
      if ((sq == 0 || sq == 8 || sq == 1) && (oppoKingSq == 1 || oppoKingSq == 2))
        score += taper(trappedRook, phase);
      else if ((sq == 7 || sq == 15 || sq == 8) && (oppoKingSq == 5 || oppoKingSq == 6))
        score += taper(trappedRook, phase);
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
    score += taper(queenMobilities[bits(mobilitySquares)], phase);

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
    score -= taper(queenMobilities[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getQueenAttacks(sq, board->occupancies[BOTH]) & myKingArea;
    if (attacksNearKing) {
      oppoTotalAttackers++;
      oppoAttackScore += 80 * bits(attacksNearKing);
    }

    popLsb(opponentQueens);
  }

  // KING SAFETY

  score += myAttackScore * attackWeight[myTotalAttackers] / 100;
  score -= oppoAttackScore * attackWeight[oppoTotalAttackers] / 100;

  int kingCol = kingSq & 7;
  if (board->side == WHITE) {
    for (int c = kingCol - 1; c <= kingCol + 1; c++) {
      if (c < 0 || c > 7)
        continue;

      BitBoard column = myPawns & columnMasks[c];

      if (column) {
        int pawnRank = msb(column) / 8;
        score += -taper(pawnShield, phase) * (36 - pawnRank * pawnRank) / 100;
      } else {
        score += -taper(pawnShield, phase) * 36 / 100;
      }
    }
  } else {
    for (int c = kingCol - 1; c <= kingCol + 1; c++) {
      if (c < 0 || c > 7)
        continue;

      BitBoard column = myPawns & columnMasks[c];
      if (column) {
        int pawnRank = 7 - lsb(column) / 8;
        score += taper(pawnShield, phase) * -(36 - pawnRank * pawnRank) / 100;
      } else {
        score += -taper(pawnShield, phase) * 36 / 100;
      }
    }
  }

  kingCol = oppoKingSq & 7;
  if (board->xside == WHITE) {
    for (int c = kingCol - 1; c <= kingCol + 1; c++) {
      if (c < 0 || c > 7)
        continue;

      BitBoard column = opponentPawns & columnMasks[c];

      if (column) {
        int pawnRank = msb(column) / 8;
        score -= -taper(pawnShield, phase) * (36 - pawnRank * pawnRank) / 100;
      } else {
        score -= -taper(pawnShield, phase) * 36 / 100;
      }
    }
  } else {
    for (int c = kingCol - 1; c <= kingCol + 1; c++) {
      if (c < 0 || c > 7)
        continue;

      BitBoard column = opponentPawns & columnMasks[c];
      if (column) {
        int pawnRank = 7 - lsb(column) / 8;
        score -= taper(pawnShield, phase) * -(36 - pawnRank * pawnRank) / 100;
      } else {
        score -= -taper(pawnShield, phase) * 36 / 100;
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
      int pieceValue = taper(baseMaterialValues[i][sq], phase);

      printf("%c (%s): %d\n", pieceChars[i], idxToCord[sq], pieceValue);
      score += pieceValue;
      popLsb(pieces);
    }
  }

  printf("\nEnemy Pieces\n------------\n\n");
  for (int i = board->xside; i < 12; i += 2) {
    BitBoard pieces = board->pieces[i];
    while (pieces) {
      int sq = lsb(pieces);
      int pieceValue = taper(baseMaterialValues[i][sq], phase);

      printf("%c (%s): %d\n", pieceChars[i], idxToCord[sq], pieceValue);
      score -= pieceValue;
      popLsb(pieces);
    }
  }

  return score;
}
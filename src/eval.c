#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "types.h"

#define S(mg, eg) (makeScore((mg), (eg)))

const int pawnValue = S(100, 150);
const int knightValue = S(475, 400);
const int bishopValue = S(500, 450);
const int rookValue = S(700, 775);
const int queenValue = S(1600, 1450);
const int kingValue = S(30000, 30000);

// clang-format off
// values concepts are inspired by cpw
const int pawnPositionValues[] = {
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   5,  40), S(   5,  40), S(  20,  40), S(  25,  35), S(  25,  35), S(  20,  40), S(   5,  40), S(   5,  40),
  S(   0,  20), S(   0,  20), S(  10,  20), S(  15,  20), S(  15,  20), S(  10,  20), S(   0,  20), S(   0,  20),
  S(  -5,  10), S(  -5,  10), S(   7,  10), S(  12,  10), S(  12,  10), S(   7,  10), S(  -5,  10), S(  -5,  10),
  S(  -5,   0), S(  -5,   0), S(   5,   0), S(  10,   0), S(  10,   0), S(   5,   0), S(  -5,   0), S(  -5,   0),
  S(  -4,  -5), S(  -5,  -5), S(   0,  -5), S(   5, -10), S(   5, -10), S(   0,  -5), S(  -5,  -5), S(  -4,  -5),
  S( -10, -10), S(  -5, -10), S(   0, -10), S( -10, -20), S( -10, -20), S(   0, -10), S(  -5, -10), S( -10, -10),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)
};

const int knightPositionValues[] = {
  S( -15, -15), S(  -5,  -5), S(  -5,  -5), S(  -5,  -5), S(  -5,  -5), S(  -5,  -5), S(  -5,  -5), S( -15, -15),
  S( -10, -10), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S( -10, -10),
  S( -10, -10), S(   0,   0), S(   5,   5), S(  10,  10), S(  10,  10), S(   5,   5), S(   0,   0), S( -10, -10),
  S( -10, -10), S(   0,   0), S(  10,  10), S(  20,  20), S(  20,  20), S(  10,  10), S(   0,   0), S( -10, -10),
  S( -10, -10), S(   0,   0), S(  10,  10), S(  20,  20), S(  20,  20), S(  10,  10), S(   0,   0), S( -10, -10),
  S( -10, -10), S(   0,   0), S(   5,   5), S(  10,  10), S(  10,  10), S(   5,   5), S(   0,   0), S( -10, -10),
  S( -10, -10), S(   0,   0), S(   0,   0), S(   5,   5), S(   5,   5), S(   0,   0), S(   0,   0), S( -10, -10),
  S( -25, -25), S( -15, -15), S(  -5,  -5), S(  -5,  -5), S(  -5,  -5), S(  -5,  -5), S( -15, -15), S( -25, -25)
};

const int bishopPositionValues[] = {
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(  10,  10), S(  10,  10), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(  10,  10), S(  10,  10), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   5,   5), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   5,   5), S(   0,   0),
  S(   0,   0), S(   0,   0), S( -10, -10), S(   0,   0), S(   0,   0), S( -10, -10), S(   0,   0), S(   0,   0)
};

const int rookPositionValues[] = {
  S(   0,   0), S(   5,   5), S(   5,   5), S(   5,   5), S(   5,   5), S(   5,   5), S(   5,   5), S(   0,   0),
  S(   5,   5), S(  10,  10), S(  10,  10), S(  10,  10), S(  10,  10), S(  10,  10), S(  10,  10), S(   5,   5),
  S(  -5,  -5), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(  -5,  -5),
  S(  -5,  -5), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(  -5,  -5),
  S(  -5,  -5), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(  -5,  -5),
  S(  -5,  -5), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(  -5,  -5),
  S(  -5,  -5), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(  -5,  -5),
  S(   0,   0), S(   0,   0), S(   5,   5), S(   5,   5), S(   5,   5), S(   5,   5), S(   0,   0), S(   0,   0),
};

const int queenPositionValues[] = {
  S(   0,  -5), S(   0,  -5), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,  -5), S(   0,  -5),
  S(   0,  -5), S(   0,   0), S(   0,   0), S(   0,   5), S(   0,   5), S(   0,   0), S(   0,   0), S(   0,  -5),
  S(   0,   0), S(   0,   0), S(   0,   5), S(   0,  10), S(   0,  10), S(   0,   5), S(   0,   0), S(   0,   0),
  S(   0,   0), S(   0,   5), S(   0,  10), S(   0,  15), S(   0,  15), S(   0,  10), S(   0,   5), S(   0,   0),
  S(   0,   0), S(   0,   5), S(   0,  10), S(   0,  15), S(   0,  15), S(   0,  10), S(   0,   5), S(   0,   0),
  S(   0,   0), S(   5,   5), S(   0,   5), S(   0,  10), S(   0,  10), S(   0,   5), S(   5,   5), S(   0,   0),
  S(   0,  -5), S(   0,   0), S(   5,   5), S(  10,   5), S(  10,   5), S(   5,   5), S(   0,   0), S(   0,  -5),
  S( -20,  -5), S( -10,  -5), S( -10,   0), S( -10,   0), S( -10,   0), S( -10,   0), S( -10,  -5), S( -10,  -5),
};

const int kingPositionValues[] = {
  S( -40, -70), S( -30, -50), S( -50, -35), S( -70, -25), S( -70, -25), S( -50, -35), S( -30, -50), S( -40, -70),
  S( -30, -50), S( -20, -25), S( -40, -10), S( -60,   0), S( -60,   0), S( -40, -10), S( -20, -25), S( -30, -50),
  S( -20, -35), S( -10, -10), S( -30,   0), S( -50,  15), S( -50,  15), S( -30,   0), S( -10, -10), S( -20, -35),
  S( -10, -25), S(   0,   0), S( -20,  15), S( -40,  30), S( -40,  30), S( -20,  15), S(   0,   0), S( -10, -25),
  S(   0, -25), S(  10,   0), S( -10,  15), S( -30,  30), S( -30,  30), S( -10,  15), S(  10,   0), S(   0, -25),
  S(  10, -35), S(  20, -10), S(   0,   0), S( -20,  15), S( -20,  15), S(   0,   0), S(  20, -10), S(  10, -35),
  S(  30, -50), S(  40, -25), S(  20, -10), S(   0,   0), S(   0,   0), S(  20, -10), S(  40, -25), S(  30, -50),
  S(  40, -70), S(  50, -50), S(  30, -35), S(  10, -25), S(  10, -25), S(  30, -35), S(  50, -50), S(  40, -70),
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
    S(-50, -75), S(-30, -60), S(-10, -45), S(0, -30), S(0, -15), S(0, 0), S(10, 0), S(30, 0), S(50, 0),
};

const int bishopMobilities[] = {
    S(-50, -75), S(-25, -50), S(0, -25), S(10, 0),  S(20, 15), S(30, 30), S(40, 40),
    S(50, 50),   S(55, 55),   S(60, 60), S(65, 65), S(70, 70), S(75, 75), S(80, 80),
};

const int rookMobilities[] = {
    S(0, -60), S(0, -45), S(0, -30), S(0, -15), S(0, 0),  S(1, 5),  S(2, 10),  S(3, 15),
    S(4, 20),  S(5, 25),  S(6, 30),  S(7, 40),  S(8, 50), S(9, 60), S(10, 70),
};

const int queenMobilities[] = {
    S(-10, -75), S(-7, -50), S(-4, -25), S(-1, 0),  S(2, 2),   S(5, 5),   S(10, 10), S(15, 15), S(20, 20), S(25, 12),
    S(30, 30),   S(35, 35),  S(36, 36),  S(37, 37), S(38, 38), S(39, 39), S(40, 42), S(41, 45), S(42, 48), S(43, 51),
    S(44, 54),   S(45, 57),  S(46, 60),  S(47, 63), S(48, 66), S(49, 69), S(50, 72), S(51, 75),
};

const int MAX_PHASE = 24;
const int phaseMultipliers[] = {0, 0, 1, 1, 1, 1, 2, 2, 4, 4, 0, 0};

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

  BitBoard myPawns = board->pieces[PAWN[board->side]];
  BitBoard opponentPawns = board->pieces[PAWN[board->xside]];

  BitBoard myPawnAttacks =
      shift(myPawns, pawnDirections[board->side] + 1) | shift(myPawns, pawnDirections[board->side] - 1);
  BitBoard myBlockedAndHomePawns = (shift(board->occupancies[BOTH], pawnDirections[board->xside]) |
                                    homeRanks[board->side] | thirdRanks[board->side]) &
                                   board->pieces[PAWN[board->side]];
  BitBoard oppoPawnAttacks =
      shift(opponentPawns, pawnDirections[board->xside] + 1) | shift(opponentPawns, pawnDirections[board->xside] - 1);
  BitBoard opponentBlockedAndHomePawns = (shift(board->occupancies[BOTH], pawnDirections[board->side]) |
                                          homeRanks[board->xside] | thirdRanks[board->xside]) &
                                         board->pieces[PAWN[board->xside]];

  // KNIGHTS
  BitBoard myKnights = board->pieces[KNIGHT[board->side]];
  BitBoard oppoKnights = board->pieces[KNIGHT[board->xside]];

  while (myKnights) {
    int sq = lsb(myKnights);

    BitBoard mobilitySquares = getKnightAttacks(sq) & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(knightMobilities[bits(mobilitySquares)], phase);

    popLsb(myKnights);
  }

  while (oppoKnights) {
    int sq = lsb(oppoKnights);

    BitBoard mobilitySquares = getKnightAttacks(sq) & ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(knightMobilities[bits(mobilitySquares)], phase);

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

    popLsb(myBishops);
  }

  while (opponentBishops) {
    int sq = lsb(opponentBishops);

    BitBoard mobilitySquares = getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->xside]]) &
                               ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(bishopMobilities[bits(mobilitySquares)], phase);

    popLsb(opponentBishops);
  }

  // Rooks
  BitBoard myRooks = board->pieces[ROOK[board->side]];
  BitBoard opponentRooks = board->pieces[ROOK[board->xside]];

  while (myRooks) {
    int sq = lsb(myRooks);

    BitBoard mobilitySquares = getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->side]]) &
                               ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(rookMobilities[bits(mobilitySquares)], phase);

    popLsb(myRooks);
  }

  while (opponentRooks) {
    int sq = lsb(opponentRooks);

    BitBoard mobilitySquares = getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[board->xside]]) &
                               ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(rookMobilities[bits(mobilitySquares)], phase);

    popLsb(opponentRooks);
  }

  // Queens
  BitBoard myQueens = board->pieces[QUEEN[board->side]];
  BitBoard opponentQueens = board->pieces[QUEEN[board->xside]];

  while (myQueens) {
    int sq = lsb(myQueens);

    BitBoard mobilitySquares = getQueenAttacks(sq, board->occupancies[BOTH] ^ (board->pieces[BISHOP[board->side]] |
                                                                               board->pieces[ROOK[board->side]])) &
                               ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(queenMobilities[bits(mobilitySquares)], phase);

    popLsb(myQueens);
  }

  while (opponentQueens) {
    int sq = lsb(opponentQueens);

    BitBoard mobilitySquares = getQueenAttacks(sq, board->occupancies[BOTH] ^ (board->pieces[BISHOP[board->xside]] |
                                                                               board->pieces[ROOK[board->xside]])) &
                               ~myPawnAttacks & ~opponentBlockedAndHomePawns;
    score -= taper(queenMobilities[bits(mobilitySquares)], phase);

    popLsb(opponentQueens);
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
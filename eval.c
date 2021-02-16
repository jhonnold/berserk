#include <stdio.h>

#include "bits.h"
#include "board.h"
#include "eval.h"
#include "types.h"

#define S(mg, eg) (makeScore((mg), (eg)))

const int pawnValue = S(100, 146);
const int knightValue = S(554, 400);
const int bishopValue = S(557, 427);
const int rookValue = S(698, 765);
const int queenValue = S(1578, 1451);
const int kingValue = S(30000, 30000);

// clang-format off
const int pawnPositionValues[] = {
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
  S(  86, 165), S(  63, 164), S(  69, 121), S(  78, 100), S(  78, 100), S(  69, 121), S(  63, 164), S(  86, 165),
  S( -21,  82), S(  11,  73), S(  63,  28), S(  41,   5), S(  41,   5), S(  63,  28), S(  11,  73), S( -21,  82),
  S( -21,   2), S(  24, -12), S(  22, -26), S(  49, -48), S(  49, -48), S(  22, -26), S(  24, -12), S( -21,   2),
  S( -26, -19), S(   6, -27), S(  16, -37), S(  42, -49), S(  42, -49), S(  16, -37), S(   6, -17), S( -26, -19),
  S( -17, -32), S(  21, -40), S(  18, -40), S(  22, -33), S(  22, -33), S(  18, -40), S(  21, -40), S( -17, -32),
  S( -38, -22), S(  13, -40), S(  13, -27), S(   3, -22), S(   3, -22), S(  13, -27), S(  13, -40), S( -38, -22),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)
};

const int knightPositionValues[] = {
  S(-198, -70), S( -90, -55), S(-121, -26), S( -42, -39), S( -42, -39), S(-121, -26), S( -90, -55), S(-198, -70),
  S( -65, -30), S( -40, -11), S(  89, -48), S( -13, -10), S( -13, -10), S(  89, -48), S( -40, -11), S( -65, -30),
  S( -33, -38), S(  80, -30), S(  57,  13), S(  66,   1), S(  66,   1), S(  57,  13), S(  80, -30), S( -33, -38),
  S(   8, -18), S(   7,   7), S(  32,  28), S(  28,  30), S(  28,  30), S(  32,  28), S(   7,   7), S(   8, -18),
  S(   1, -10), S(  19,   2), S(  22,  28), S(  26,  32), S(  26,  32), S(  22,  28), S(  19,   2), S(   1, -10),
  S( -18, -13), S(   3,  -3), S(  16,   9), S(  22,  28), S(  22,  28), S(  16,   9), S(   3,  -3), S( -18, -13),
  S( -20, -23), S( -42,  -6), S(   2,  -2), S(   6,  11), S(   6,  11), S(   2,  -2), S( -42,  -6), S( -20, -23),
  S( -69, -23), S( -20, -33), S( -43,  -3), S( -19,   3), S( -19,   3), S( -43,  -3), S( -20, -33), S( -69, -23)
};

const int bishopPositionValues[] = {
  S( -31, -11), S( -20, -10), S(-121,  -2), S( -80,  -3), S( -80,  -3), S(-121,  -2), S( -20, -10), S( -31, -11),
  S( -58,   8), S(  12,  -2), S(  10,  -3), S( -10,  -8), S( -10,  -8), S(  10,  -3), S(  12,  -2), S( -58,   8),
  S( -30,  20), S(  26,   4), S(  46,   1), S(  21,  -1), S(  21,  -1), S(  46,   1), S(  26,   4), S( -30,  20),
  S( -12,   9), S( -18,  19), S(  13,  15), S(  35,  12), S(  35,  12), S(  13,  15), S( -18,  19), S( -12,   9),
  S(  -9,   1), S(   1,   3), S(   6,  12), S(  25,  14), S(  25,  14), S(   6,  12), S(   1,   3), S(  -9,   1),
  S( -12,   1), S(  16,  -3), S(  18,   7), S(  13,  19), S(  13,  19), S(  18,   7), S(  16,  -3), S( -12,   1),
  S(   9, -21), S(  27, -23), S(  20,  -6), S(  10,  15), S(  10,  15), S(  20,  -6), S(  27, -23), S(   9, -21),
  S( -52, -15), S( -21,   3), S(  -9,  -5), S( -18,  11), S( -18,  11), S(  -9,  -5), S( -21,   3), S( -52, -15)
};

const int rookPositionValues[] = {
  S(   1,  32), S(  26,  19), S(  -3,  26), S(  54,  15), S(  54,  15), S(  -3,  26), S(  26,  19), S(   1,  32),
  S(   6,  12), S(   3,  15), S(  70,  -6), S(  56,  -9), S(  56,  -9), S(  70,  -6), S(   3,  15), S(   6,  12),
  S(  -7,   0), S(  52,  -9), S(  33,  -5), S(  47,  -9), S(  47,  -9), S(  33,  -5), S(  52,  -9), S(  -7,   0),
  S( -18,  10), S(  -2,   2), S(  27,  10), S(  51,  -9), S(  51,  -9), S(  27,  10), S(  -2,   2), S( -18,  10),
  S( -35,  17), S(   1,   5), S(  -3,   9), S(  25,  -4), S(  25,  -4), S(  -3,   9), S(   1,   5), S( -35,  17),
  S( -46,  13), S(  -8,   7), S(  -1,  -7), S(  11,  -9), S(  11,  -9), S(  -1,  -7), S(  -8,   7), S( -46,  13),
  S( -68,  24), S(  -5,   1), S(  -7,   1), S(  10,  -1), S(  10,  -1), S(  -7,   1), S(  -5,   1), S( -68,  24),
  S( -12,   4), S( -16,  17), S(   6,   6), S(  40,  -3), S(  40,  -3), S(   6,   6), S( -16,  17), S( -12,   4)
};

const int queenPositionValues[] = {
  S(   3, -10), S(  23,   4), S(  36,  12), S(  52,  13), S(  52,  13), S(  36,  12), S(  23,   4), S(   3, -10),
  S( -14, -16), S( -50,  -5), S(  24,  12), S(   8,  16), S(   8,  16), S(  24,  12), S( -50,  -5), S( -14, -16),
  S(  19, -13), S(  20,  -6), S(  25,  11), S(  34,  16), S(  34,  16), S(  25,  11), S(  20,  -6), S(  19, -13),
  S(  -9,  -3), S( -27,  18), S( -10,  18), S( -10,  24), S( -10,  24), S( -10,  18), S( -27,  18), S(  -9,  -3),
  S( -13,  -6), S( -13,  24), S( -14,  21), S( -14,  24), S( -14,  24), S( -14,  21), S( -13,  24), S( -13,  -6),
  S( -20,  -3), S(   7, -21), S( -10,  15), S( -11,  14), S( -11,  14), S( -10,  15), S(   7, -21), S( -20,  -3),
  S( -52, -19), S( -25, -23), S(  16, -22), S(   0,  -4), S(   0,  -4), S(  16, -22), S( -25, -23), S( -52, -19),
  S( -36, -24), S( -34, -25), S( -26, -26), S(   6, -46), S(   6, -46), S( -26, -26), S( -34, -25), S( -36, -24)
};

const int kingPositionValues[] = {
  S( -51, -41), S(   6,  -6), S(   6,  -6), S( -19, -14), S( -19, -14), S(   6,  -6), S(   6,  -6), S( -51, -41),
  S(   9,   4), S(  57,  33), S(  78,  39), S(  72,  27), S(  72,  27), S(  78,  39), S(  57,  33), S(   9,   4),
  S(  38,   7), S( 123,  42), S(  69,  57), S(  45,  29), S(  45,  29), S(  69,  57), S( 123,  42), S(  38,   7),
  S( -12,  -7), S(  34,  42), S(  29,  53), S(  -5,  55), S(  -5,  55), S(  29,  53), S(  34,  42), S( -12,  -7),
  S(-102,  -2), S(   3,  20), S( -47,  57), S( -65,  67), S( -65,  67), S( -47,  57), S(   3,  20), S(-102,  -2),
  S( -51,  -6), S(  15,  19), S( -31,  45), S( -50,  58), S( -50,  58), S( -31,  45), S(  15,  19), S( -51,  -6),
  S(  36, -32), S(  38,   1), S( -40,  34), S( -78,  48), S( -78,  48), S( -40,  34), S(  38,   1), S(  36, -32),
  S(  28, -95), S(  68, -61), S( -41, -17), S(  34, -34), S(  34, -34), S( -41, -17), S(  68, -61), S(  28, -95)
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

const int MAX_PHASE = 24;
const int phaseMultipliers[] = {0, 0, 1, 1, 1, 1, 2, 2, 4, 4, 0, 0};

int baseMaterialValues[12][64];

// clang-format off
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
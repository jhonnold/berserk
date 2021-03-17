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

const int PAWN_VALUE = S(100, 150);
const int KNIGHT_VALUE = S(475, 475);
const int BISHOP_VALUE = S(475, 475);
const int ROOK_VALUE = S(700, 775);
const int QUEEN_VALUE = S(1600, 1450);
const int KING_VALUE = S(30000, 30000);

const int BISHOP_PAIR = S(50, 50);

// clang-format off
const int PAWN_PSQT[] = {
  S( -15,   0), S(  -5,   0), S(   0,   0), S(   5,   0), S(   5,   0), S(   0,   0), S(  -5,   0), S( -15,   0),
  S( -15,   0), S(  -5,   0), S(   0,   0), S(   5,   0), S(   5,   0), S(   0,   0), S(  -5,   0), S( -15,   0),
  S( -15,   0), S(  -5,   0), S(   0,   0), S(   5,   0), S(   5,   0), S(   0,   0), S(  -5,   0), S( -15,   0),
  S( -15,   0), S(  -5,   0), S(   0,   0), S(  15,   0), S(  15,   0), S(   0,   0), S(  -5,   0), S( -15,   0),
  S( -15,   0), S(  -5,   0), S(   0,   0), S(  25,   0), S(  25,   0), S(   0,   0), S(  -5,   0), S( -15,   0),
  S( -15,   0), S(  -5,   0), S(   0,   0), S(  15,   0), S(  15,   0), S(   0,   0), S(  -5,   0), S( -15,   0),
  S( -15,   0), S(  -5,   0), S(   0,   0), S(   5,   0), S(   5,   0), S(   0,   0), S(  -5,   0), S( -15,   0),
  S( -15,   0), S(  -5,   0), S(   0,   0), S(   5,   0), S(   5,   0), S(   0,   0), S(  -5,   0), S( -15,   0),
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
  S(   0,   0), S(  30,  20), S(  40,  28), S(  48,  32), S(  48,  32), S(  40,  28), S(  30,  20), S(   0,   0),
  S(   0,   0), S(  30,  20), S(  40,  28), S(  48,  32), S(  48,  32), S(  40,  28), S(  30,  20), S(   0,   0),
  S(   0,   0), S(  30,  20), S(  36,  24), S(  40,  28), S(  40,  28), S(  36,  24), S(  30,  20), S(   0,   0),
  S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
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

const int DOUBLED_PAWN = S(-10, -10);
const int OPPOSED_ISOLATED_PAWN = S(-2, -5);
const int OPEN_ISOLATED_PAWN = S(-6, -16);
const int BACKWARDS_PAWN = S(-20, -7);
const int DEFENDED_PAWN = S(8, 2);
const int CONNECTED_PAWN[2][8] = {{
                                      S(0, 0),
                                      S(75, 100),
                                      S(40, 40),
                                      S(20, 15),
                                      S(10, 5),
                                      S(6, 1),
                                      S(4, 0),
                                      S(0, 0),
                                  },
                                  {
                                      S(0, 0),
                                      S(4, 0),
                                      S(6, 1),
                                      S(10, 5),
                                      S(20, 15),
                                      S(40, 40),
                                      S(75, 100),
                                      S(0, 0),
                                  }};

const int PASSED_PAWN[2][8] = {
    {
        S(0, 0),
        S(223, 210),
        S(138, 143),
        S(52, 56),
        S(14, 32),
        S(12, 25),
        S(7, 23),
        S(0, 0),
    },
    {
        S(0, 0),
        S(7, 23),
        S(12, 25),
        S(14, 32),
        S(52, 56),
        S(138, 143),
        S(223, 210),
        S(0, 0),
    },
};

const int DEFENDED_MINOR = S(5, 2);
const int ROOK_OPEN_FILE = 20;
const int ROOK_SEMI_OPEN_FILE = 10;
const int ROOK_SEVENTH_RANK = 20;
const int ROOK_OPPOSITE_KING = S(10, 0);
const int ROOK_TRAPPED = S(-75, -75);
const int BISHOP_TRAPPED = S(-150, -150);

const int KNIGHT_THREATS[] = {S(0, 0), S(5, 30), S(30, 30), S(45, 45), S(60, 60), S(50, 50)};
const int BISHOP_THREATS[] = {S(0, 0), S(5, 30), S(30, 30), S(45, 45), S(60, 60), S(50, 50)};
const int ROOK_THREATS[] = {S(0, 0), S(0, 30), S(30, 30), S(45, 45), S(20, 30), S(50, 50)};
const int KING_THREATS[] = {S(0, 60), S(15, 60), S(15, 60), S(15, 60), S(15, 60), S(15, 60)};
const int HANGING_THREAT = S(45, 22);

const int PAWN_SHELTER = -36;
const int PAWN_STORM[8] = {0, 0, 0, -10, -30, -60, 0, 0};

// These values are *currently* borrowed from chess22k
const int KING_SAFETY_ALLIES[] = {2, 2, 1, 1, 0, 0, 0, 0, 3};
const int KING_SAFETY_ATTACKS[] = {0, 2, 2, 2, 2, 2, 3, 4, 4};
const int KING_SAFETY_WEAK_SQS[] = {0, 1, 2, 2, 2, 2, 2, 1, -5};
const int KING_SAFETY_QUEEN_CHECK[] = {0, 0, 0, 0, 2, 3, 4, 4, 4, 4, 3, 3, 3, 2, 0, 0, 0};
const int KING_SAFETY_QUEEN_TROPISM[] = {0, 0, 2, 2, 2, 2, 1, 1};
const int KING_SAFETY_DOUBLE_ATTACKS[] = {0, 1, 1, 3, 3, 9, 9, 9, 9};
const int KING_SAFETY_ATTACK_PATTERN[] = {0, 1, 2, 2, 2, 2, 2, 3, 1, 0, 1, 2, 1, 1, 1, 2,
                                          2, 2, 2, 3, 2, 2, 3, 4, 1, 2, 3, 3, 2, 3, 3, 5};
const int KING_SAFETY_SCORES[] = {0,   0,   0,   40,  60,  70,  80,  90,  100, 120, 150,
                                  200, 260, 300, 390, 450, 520, 640, 740, 760, 1260};

// clang-format off
int PSQT[12][64];

void initPSQT() {
  for (int sq = 0; sq < 64; sq++) {
    PSQT[PAWN[WHITE]][sq] = PAWN_PSQT[sq];
    PSQT[PAWN[BLACK]][MIRROR[sq]] = PAWN_PSQT[sq];
    PSQT[KNIGHT[WHITE]][sq] = KNIGHT_PSQT[sq];
    PSQT[KNIGHT[BLACK]][MIRROR[sq]] = KNIGHT_PSQT[sq];
    PSQT[BISHOP[WHITE]][sq] = BISHOP_PSQT[sq];
    PSQT[BISHOP[BLACK]][MIRROR[sq]] = BISHOP_PSQT[sq];
    PSQT[ROOK[WHITE]][sq] = ROOK_PSQT[sq];
    PSQT[ROOK[BLACK]][MIRROR[sq]] = ROOK_PSQT[sq];
    PSQT[QUEEN[WHITE]][sq] = QUEEN_PSQT[sq];
    PSQT[QUEEN[BLACK]][MIRROR[sq]] = QUEEN_PSQT[sq];
    PSQT[KING[WHITE]][sq] = KING_PSQT[sq];
    PSQT[KING[BLACK]][MIRROR[sq]] = KING_PSQT[sq];
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
  data->material = 0;
  data->pawns = 0;
  data->knights = 0;
  data->bishops = 0;
  data->rooks = 0;
  data->queens = 0;
  data->kings = 0;
  data->mobility = 0;
  data->kingSafety = 0;
  data->attacks2 = 0;
  data->allAttacks = 0;
  memset(data->attacks, 0, sizeof(data->attacks));

  int xside = 1 - side;
  int phase = getPhase(board);

  // Random utility stuff
  BitBoard myPawns = board->pieces[PAWN[side]];
  BitBoard opponentPawns = board->pieces[PAWN[xside]];
  BitBoard allPawns = myPawns | opponentPawns;

  BitBoard pawnAttacks = shift(myPawns, PAWN_DIRECTIONS[side] + E) | shift(myPawns, PAWN_DIRECTIONS[side] + W);
  BitBoard oppoPawnAttacks =
      shift(opponentPawns, PAWN_DIRECTIONS[xside] + E) | shift(opponentPawns, PAWN_DIRECTIONS[xside] + W);

  BitBoard myBlockedAndHomePawns =
      (shift(board->occupancies[BOTH], PAWN_DIRECTIONS[xside]) | HOME_RANKS[side] | THIRD_RANKS[side]) &
      board->pieces[PAWN[side]];

  int kingSq = lsb(board->pieces[KING[side]]);
  int oppoKingSq = lsb(board->pieces[KING[xside]]);

  // PAWNS
  data->attacks[PAWN[side] >> 1] = pawnAttacks;
  data->allAttacks = pawnAttacks;
  data->attacks2 = shift(myPawns, PAWN_DIRECTIONS[side] + E) & shift(myPawns, PAWN_DIRECTIONS[side] + W);

  for (BitBoard pawns = board->pieces[PAWN[side]]; pawns; popLsb(pawns)) {
    BitBoard sqBitboard = pawns & -pawns;
    int sq = lsb(pawns);

    data->material += taper(MATERIAL_VALUES[PAWN[side]], phase);
    data->pawns += taper(PSQT[PAWN[side]][sq], phase);

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

    if (doubled)
      data->pawns += taper(DOUBLED_PAWN, phase);

    if (!neighbors)
      data->pawns += taper(opposed ? OPPOSED_ISOLATED_PAWN : OPEN_ISOLATED_PAWN, phase);

    if (backwards)
      data->pawns += taper(BACKWARDS_PAWN, phase);

    if (passed)
      data->pawns += taper(PASSED_PAWN[side][rank], phase);

    if (defenders | connected) {
      int s = 2;
      if (connected)
        s++;
      if (opposed)
        s--;

      data->pawns += taper(CONNECTED_PAWN[side][rank], phase) * s + taper(DEFENDED_PAWN, phase) * bits(defenders);
    }
  }

  BitBoard outposts = ~fill(oppoPawnAttacks, PAWN_DIRECTIONS[xside]) &
                      (pawnAttacks | shift(myPawns | opponentPawns, PAWN_DIRECTIONS[xside]));
  // KNIGHTS
  for (BitBoard knights = board->pieces[KNIGHT[side]]; knights; popLsb(knights)) {
    int sq = lsb(knights);
    BitBoard sqBb = knights & -knights;

    data->material += taper(MATERIAL_VALUES[KNIGHT[side]], phase);
    data->knights += taper(PSQT[KNIGHT[side]][sq], phase);

    BitBoard movement = getKnightAttacks(sq);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[KNIGHT[side] >> 1] |= movement;
    data->allAttacks |= movement;

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility += taper(KNIGHT_MOBILITIES[bits(mobilitySquares)], phase);

    if (KNIGHT_POST_PSQT[rel(sq, side)] && (outposts & sqBb))
      data->knights += taper(KNIGHT_POST_PSQT[rel(sq, side)], phase);

    if (getPawnAttacks(sq, xside) & myPawns)
      data->knights += taper(DEFENDED_MINOR, phase);
  }

  // BISHOPS
  if (bits(board->pieces[BISHOP[side]]) > 1)
    data->bishops += taper(BISHOP_PAIR, phase);

  for (BitBoard bishops = board->pieces[BISHOP[side]]; bishops; popLsb(bishops)) {
    int sq = lsb(bishops);

    data->material += taper(MATERIAL_VALUES[BISHOP[side]], phase);
    data->bishops += taper(PSQT[BISHOP[side]][sq], phase);

    BitBoard movement = getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[BISHOP[side] >> 1] |= movement;
    data->allAttacks |= movement;

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility += taper(BISHOP_MOBILITIES[bits(mobilitySquares)], phase);

    if (side == WHITE) {
      if ((sq == A7 || sq == B8) && getBit(opponentPawns, B6) && getBit(opponentPawns, C7))
        data->bishops += taper(BISHOP_TRAPPED, phase);
      else if ((sq == H7 || sq == G8) && getBit(opponentPawns, F7) && getBit(opponentPawns, G6))
        data->bishops += taper(BISHOP_TRAPPED, phase);
    } else {
      if ((sq == A2 || sq == B1) && getBit(opponentPawns, B3) && getBit(opponentPawns, C2))
        data->bishops += taper(BISHOP_TRAPPED, phase);
      else if ((sq == H2 || sq == G1) && getBit(opponentPawns, G3) && getBit(opponentPawns, F2))
        data->bishops += taper(BISHOP_TRAPPED, phase);
    }

    if (getPawnAttacks(sq, xside) & myPawns)
      data->bishops += taper(DEFENDED_MINOR, phase);
  }

  // ROOKS
  for (BitBoard rooks = board->pieces[ROOK[side]]; rooks; popLsb(rooks)) {
    int sq = lsb(rooks);
    int file = file(sq);

    data->material += taper(MATERIAL_VALUES[ROOK[side]], phase);
    data->rooks += taper(PSQT[ROOK[side]][sq], phase);

    BitBoard movement =
        getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]] ^ board->pieces[ROOK[side]]);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[ROOK[side] >> 1] |= movement;
    data->allAttacks |= movement;

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility += taper(ROOK_MOBILITIES[bits(mobilitySquares)], phase);

    if (rank(rel(sq, side)) == 1)
      data->rooks += ROOK_SEVENTH_RANK;

    if (!(FILE_MASKS[file] & allPawns)) {
      data->rooks += ROOK_OPEN_FILE;
      if (file == file(oppoKingSq))
        data->rooks += taper(ROOK_OPPOSITE_KING, phase);
    } else if (!(FILE_MASKS[file] & myPawns)) {
      data->rooks += ROOK_SEMI_OPEN_FILE;
      if (file == file(oppoKingSq))
        data->rooks += taper(ROOK_OPPOSITE_KING, phase);
    }

    if (side == WHITE) {
      if ((sq == A1 || sq == A2 || sq == B1) && (kingSq == C1 || kingSq == B1))
        data->rooks += taper(ROOK_TRAPPED, phase);
      else if ((sq == H1 || sq == H2 || sq == G1) && (kingSq == F1 || kingSq == G1))
        data->rooks += taper(ROOK_TRAPPED, phase);
    } else {
      if ((sq == A8 || sq == A7 || sq == B8) && (kingSq == B8 || kingSq == C8))
        data->rooks += taper(ROOK_TRAPPED, phase);
      else if ((sq == H8 || sq == H7 || sq == G8) && (kingSq == F8 || kingSq == G8))
        data->rooks += taper(ROOK_TRAPPED, phase);
    }
  }

  // QUEENS
  for (BitBoard queens = board->pieces[QUEEN[side]]; queens; popLsb(queens)) {
    int sq = lsb(queens);

    data->material += taper(MATERIAL_VALUES[QUEEN[side]], phase);
    data->queens += taper(PSQT[QUEEN[side]][sq], phase);

    BitBoard movement = getQueenAttacks(sq, board->occupancies[BOTH]);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[QUEEN[side] >> 1] |= movement;
    data->allAttacks |= movement;

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility += taper(QUEEN_MOBILITIES[bits(mobilitySquares)], phase);
  }

  // KINGS
  for (BitBoard kings = board->pieces[KING[side]]; kings; popLsb(kings)) {
    int sq = lsb(kings);

    data->material += taper(MATERIAL_VALUES[KING[side]], phase);
    data->kings += taper(PSQT[KING[side]][sq], phase);

    BitBoard movement = getKingAttacks(sq) & ~getKingAttacks(lsb(board->pieces[KING[xside]]));
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[KING[side] >> 1] |= movement;
    data->allAttacks |= movement;
  }
}

void EvaluateThreats(Board* board, int side, EvalData* data, EvalData* enemyData) {
  data->threats = 0;

  int xside = 1 - side;
  int phase = getPhase(board);

  BitBoard* myAttacks = data->attacks;
  BitBoard* enemyAttacks = enemyData->attacks;

  BitBoard weak = board->occupancies[xside] & ~enemyAttacks[0] & data->allAttacks;

  for (BitBoard knightThreats = weak & myAttacks[1]; knightThreats; popLsb(knightThreats)) {
    int piece = pieceAt(lsb(knightThreats), xside, board);
    data->threats += taper(KNIGHT_THREATS[piece >> 1], phase);
  }

  for (BitBoard bishopThreats = weak & myAttacks[2]; bishopThreats; popLsb(bishopThreats)) {
    int piece = pieceAt(lsb(bishopThreats), xside, board);
    data->threats += taper(BISHOP_THREATS[piece >> 1], phase);
  }

  for (BitBoard rookThreats = weak & myAttacks[3]; rookThreats; popLsb(rookThreats)) {
    int piece = pieceAt(lsb(rookThreats), xside, board);
    data->threats += taper(ROOK_THREATS[piece >> 1], phase);
  }

  for (BitBoard kingThreats = weak & myAttacks[5]; kingThreats; popLsb(kingThreats)) {
    int piece = pieceAt(lsb(kingThreats), xside, board);
    data->threats += taper(KING_THREATS[piece >> 1], phase);
  }

  BitBoard hanging = data->allAttacks & ~enemyData->allAttacks & board->occupancies[xside];
  data->threats += taper(HANGING_THREAT, phase) * bits(hanging);
}

void EvaluateKingSafety(Board* board, int side, EvalData* data, EvalData* enemyData) {
  data->kingSafety = 0;
  int xside = 1 - side;

  if (!board->pieces[QUEEN[xside]])
    return;

  int ksCounter = 1 - (board->side ^ xside);

  int kingSq = lsb(board->pieces[KING[side]]);
  BitBoard kingArea = board->pieces[KING[side]] | getKingAttacks(kingSq);

  if (kingSq >= A1)
    kingArea |= shift(kingArea, N);
  else if (kingSq <= H8)
    kingArea |= shift(kingArea, S);

  if (file(kingSq) == 0)
    kingArea |= shift(kingArea, E);
  else if (file(kingSq) == 7)
    kingArea |= shift(kingArea, W);

  kingArea &= ~board->pieces[KING[side]];

  int allies = bits(kingArea & board->occupancies[side]);
  int enemyAttacks = bits(kingArea & enemyData->allAttacks);
  int enemyDoubleAttacks = bits(kingArea & enemyData->attacks2 & ~data->attacks[0]);
  int weakSqs = bits(kingArea & enemyData->allAttacks &
                     ~(data->attacks[0] | data->attacks[1] | data->attacks[2] | data->attacks[3]));

  ksCounter += KING_SAFETY_ALLIES[allies];
  ksCounter += KING_SAFETY_ATTACKS[enemyAttacks];
  ksCounter += KING_SAFETY_DOUBLE_ATTACKS[enemyDoubleAttacks];
  ksCounter += KING_SAFETY_WEAK_SQS[weakSqs];
  if (!(kingArea & data->attacks[1]))
    ksCounter++;

  BitBoard unsafeKing = getKingAttacks(kingSq) & enemyData->attacks2 & ~data->attacks2;

  BitBoard possibleKnightChecks = getKnightAttacks(kingSq) & enemyData->attacks[1] & ~board->occupancies[xside];
  if (possibleKnightChecks) {
    if (possibleKnightChecks & ~data->allAttacks)
      ksCounter += 3;
    else
      ksCounter++;
  }

  BitBoard possibleBishopChecks = getBishopAttacks(kingSq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]) &
                                  enemyData->attacks[2] & ~board->occupancies[xside];
  if (possibleBishopChecks) {
    if (possibleBishopChecks & ~data->allAttacks)
      ksCounter += 3;
    else
      ksCounter++;
  }

  BitBoard possibleRookChecks = getRookAttacks(kingSq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]) &
                                enemyData->attacks[3] & ~board->occupancies[xside];
  if (possibleRookChecks) {
    if (possibleRookChecks & (~data->allAttacks | unsafeKing))
      ksCounter += 3;
    else
      ksCounter++;
  }

  BitBoard possibleQueenChecks = getQueenAttacks(kingSq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]) &
                                 enemyData->attacks[4] & ~board->occupancies[xside];
  if (possibleQueenChecks) {
    // dont care for unsafe queen checks
    if (possibleQueenChecks & ~data->allAttacks)
      ksCounter += KING_SAFETY_QUEEN_CHECK[bits(board->occupancies[side])];

    // safe queen check on KING RING is additionally bad
    if (possibleQueenChecks & unsafeKing)
      ksCounter += 2;
  }

  // danger for close queen
  if (!(data->allAttacks & board->pieces[QUEEN[xside]]))
    ksCounter += KING_SAFETY_QUEEN_TROPISM[distance(kingSq, lsb(board->pieces[QUEEN[xside]]))];

  int attackersFlag = 0;
  if (enemyData->attacks[0] & kingArea)
    attackersFlag += 1;
  if (enemyData->attacks[1] & kingArea)
    attackersFlag += 2;
  if (enemyData->attacks[2] & kingArea)
    attackersFlag += 4;
  if (enemyData->attacks[3] & kingArea)
    attackersFlag += 8;
  if (enemyData->attacks[4] & kingArea)
    attackersFlag += 16;

  ksCounter += KING_SAFETY_ATTACK_PATTERN[attackersFlag];

  data->kingSafety -= KING_SAFETY_SCORES[min(ksCounter, 20)];

  // PAWN STORM / SHIELD
  int kingFile = file(kingSq);

  // shelter and storm
  for (int c = kingFile - 1; c <= kingFile + 1; c++) {
    if (c < 0 || c > 7)
      continue;

    BitBoard file = board->pieces[PAWN[side]] & FILE_MASKS[c];

    int s = PAWN_SHELTER;

    if (file) {
      int pawnRank = side == WHITE ? rank(msb(file)) : 7 - rank(lsb(file));
      s += pawnRank * pawnRank;
    }

    if (c == kingFile)
      s *= 2;

    data->kingSafety += s;

    file = board->pieces[PAWN[xside]] & FILE_MASKS[c];
    if (file) {
      int pawnRank = side == WHITE ? rank(msb(file)) : 7 - rank(lsb(file));
      data->kingSafety += PAWN_STORM[pawnRank];
    } else {
      data->kingSafety += PAWN_STORM[6];
    }
  }
}

int Evaluate(Board* board) {
  EvalData sideData[1];
  EvalData xsideData[1];

  EvaluateSide(board, board->side, sideData);
  EvaluateSide(board, board->xside, xsideData);

  EvaluateThreats(board, board->side, sideData, xsideData);
  EvaluateThreats(board, board->xside, xsideData, sideData);

  EvaluateKingSafety(board, board->side, sideData, xsideData);
  EvaluateKingSafety(board, board->xside, xsideData, sideData);

  return toScore(sideData) - toScore(xsideData);
}

inline int toScore(EvalData* data) {
  return data->pawns + data->knights + data->bishops + data->rooks + data->queens + data->kings + data->kingSafety +
         data->material + data->mobility + data->threats;
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

  printf("|            | WHITE | BLACK | DIFF  |\n");
  printf("|------------|-------|-------|-------|\n");
  printf("| material   | %5d | %5d | %5d |\n", whiteEval->material, blackEval->material,
         whiteEval->material - blackEval->material);
  printf("| pawns      | %5d | %5d | %5d |\n", whiteEval->pawns, blackEval->pawns, whiteEval->pawns - blackEval->pawns);
  printf("| knights    | %5d | %5d | %5d |\n", whiteEval->knights, blackEval->knights,
         whiteEval->knights - blackEval->knights);
  printf("| bishops    | %5d | %5d | %5d |\n", whiteEval->bishops, blackEval->bishops,
         whiteEval->bishops - blackEval->bishops);
  printf("| rooks      | %5d | %5d | %5d |\n", whiteEval->rooks, blackEval->rooks, whiteEval->rooks - blackEval->rooks);
  printf("| queens     | %5d | %5d | %5d |\n", whiteEval->queens, blackEval->queens,
         whiteEval->queens - blackEval->queens);
  printf("| kings      | %5d | %5d | %5d |\n", whiteEval->kings, blackEval->kings, whiteEval->kings - blackEval->kings);
  printf("| mobility   | %5d | %5d | %5d |\n", whiteEval->mobility, blackEval->mobility,
         whiteEval->mobility - blackEval->mobility);
  printf("| threats    | %5d | %5d | %5d |\n", whiteEval->threats, blackEval->threats,
         whiteEval->threats - blackEval->threats);
  printf("| kingSafety | %5d | %5d | %5d |\n", whiteEval->kingSafety, blackEval->kingSafety,
         whiteEval->kingSafety - blackEval->kingSafety);
  printf("|------------|-------|-------|-------|\n");
  printf("\nResult (white): %5d\n\n", toScore(whiteEval) - toScore(blackEval));
}
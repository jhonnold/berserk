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

const int ROOK_OPEN_FILE = S(20, 20);
const int ROOK_SEMI_OPEN = S(10, 10);
const int ROOK_SEVENTH_RANK = S(20, 40);
const int ROOK_OPPOSITE_KING = S(20, 0);
const int ROOK_ADJACENT_KING = S(10, 0);

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

const int KS_ATTACKER_WEIGHTS[] = {0, 60, 30, 40, 35}; // Inspired by ethereal's values
const int KS_ATTACK = 47;
const int KS_WEAK_SQS = 47;
const int KS_SAFE_CHECK = 105;
const int KS_UNSAFE_CHECK = 12;
const int KS_ENEMY_QUEEN = -280;
const int KS_ALLIES = -24;

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

inline int taper(int mg, int eg, int phase) { return (mg * (256 - phase) + (eg * phase)) / 256; }

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
  data->material[0] = 0;
  data->material[1] = 0;
  data->pawns[0] = 0;
  data->pawns[1] = 0;
  data->knights[0] = 0;
  data->knights[1] = 0;
  data->bishops[0] = 0;
  data->bishops[1] = 0;
  data->rooks[0] = 0;
  data->rooks[1] = 0;
  data->queens[0] = 0;
  data->queens[1] = 0;
  data->kings[0] = 0;
  data->kings[1] = 0;
  data->mobility[0] = 0;
  data->mobility[1] = 0;

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
  data->attacks[PAWN[side] >> 1] = pawnAttacks;
  data->allAttacks = pawnAttacks;
  data->attacks2 = shift(myPawns, PAWN_DIRECTIONS[side] + E) & shift(myPawns, PAWN_DIRECTIONS[side] + W);

  for (BitBoard pawns = board->pieces[PAWN[side]]; pawns; popLsb(pawns)) {
    BitBoard sqBitboard = pawns & -pawns;
    int sq = lsb(pawns);

    data->material[0] += scoreMG(MATERIAL_VALUES[PAWN[side]]);
    data->material[1] += scoreEG(MATERIAL_VALUES[PAWN[side]]);
    data->pawns[0] += scoreMG(PSQT[PAWN[side]][sq]);
    data->pawns[1] += scoreEG(PSQT[PAWN[side]][sq]);

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
      data->pawns[0] += scoreMG(DOUBLED_PAWN);
      data->pawns[1] += scoreEG(DOUBLED_PAWN);
    }

    if (!neighbors) {
      data->pawns[0] += scoreMG(opposed ? OPPOSED_ISOLATED_PAWN : OPEN_ISOLATED_PAWN);
      data->pawns[1] += scoreEG(opposed ? OPPOSED_ISOLATED_PAWN : OPEN_ISOLATED_PAWN);
    }

    if (backwards) {
      data->pawns[0] += scoreMG(BACKWARDS_PAWN);
      data->pawns[1] += scoreEG(BACKWARDS_PAWN);
    }

    if (passed) {
      data->pawns[0] += scoreMG(PASSED_PAWN[side][rank]);
      data->pawns[1] += scoreEG(PASSED_PAWN[side][rank]);
    }

    if (defenders | connected) {
      int s = 2;
      if (connected)
        s++;
      if (opposed)
        s--;

      data->pawns[0] += scoreMG(CONNECTED_PAWN[side][rank]) * s + scoreMG(DEFENDED_PAWN) * bits(defenders);
      data->pawns[1] += scoreEG(CONNECTED_PAWN[side][rank]) * s + scoreEG(DEFENDED_PAWN) * bits(defenders);
    }
  }

  BitBoard outposts = ~fill(oppoPawnAttacks, PAWN_DIRECTIONS[xside]) &
                      (pawnAttacks | shift(myPawns | opponentPawns, PAWN_DIRECTIONS[xside]));
  // KNIGHTS
  for (BitBoard knights = board->pieces[KNIGHT[side]]; knights; popLsb(knights)) {
    int sq = lsb(knights);
    BitBoard sqBb = knights & -knights;

    data->material[0] += scoreMG(MATERIAL_VALUES[KNIGHT[side]]);
    data->material[1] += scoreEG(MATERIAL_VALUES[KNIGHT[side]]);
    data->knights[0] += scoreMG(PSQT[KNIGHT[side]][sq]);
    data->knights[1] += scoreEG(PSQT[KNIGHT[side]][sq]);

    BitBoard movement = getKnightAttacks(sq);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[KNIGHT[side] >> 1] |= movement;
    data->allAttacks |= movement;
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[1];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[0] += scoreMG(KNIGHT_MOBILITIES[bits(mobilitySquares)]);
    data->mobility[1] += scoreEG(KNIGHT_MOBILITIES[bits(mobilitySquares)]);

    if (KNIGHT_POST_PSQT[rel(sq, side)] && (outposts & sqBb)) {
      data->knights[0] += scoreMG(KNIGHT_POST_PSQT[rel(sq, side)]);
      data->knights[1] += scoreEG(KNIGHT_POST_PSQT[rel(sq, side)]);
    }
  }

  data->knights[0] += scoreMG(DEFENDED_MINOR) * bits(pawnAttacks & board->pieces[KNIGHT[side]]);
  data->knights[1] += scoreEG(DEFENDED_MINOR) * bits(pawnAttacks & board->pieces[KNIGHT[side]]);

  // BISHOPS
  if (bits(board->pieces[BISHOP[side]]) > 1) {
    data->bishops[0] += scoreMG(BISHOP_PAIR);
    data->bishops[1] += scoreEG(BISHOP_PAIR);
  }

  for (BitBoard bishops = board->pieces[BISHOP[side]]; bishops; popLsb(bishops)) {
    int sq = lsb(bishops);

    data->material[0] += scoreMG(MATERIAL_VALUES[BISHOP[side]]);
    data->material[1] += scoreEG(MATERIAL_VALUES[BISHOP[side]]);
    data->bishops[0] += scoreMG(PSQT[BISHOP[side]][sq]);
    data->bishops[1] += scoreEG(PSQT[BISHOP[side]][sq]);

    BitBoard movement = getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[BISHOP[side] >> 1] |= movement;
    data->allAttacks |= movement;
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[2];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[0] += scoreMG(BISHOP_MOBILITIES[bits(mobilitySquares)]);
    data->mobility[1] += scoreEG(BISHOP_MOBILITIES[bits(mobilitySquares)]);

    if (side == WHITE) {
      if ((sq == A7 || sq == B8) && getBit(opponentPawns, B6) && getBit(opponentPawns, C7)) {
        data->bishops[0] += scoreMG(BISHOP_TRAPPED);
        data->bishops[1] += scoreEG(BISHOP_TRAPPED);
      } else if ((sq == H7 || sq == G8) && getBit(opponentPawns, F7) && getBit(opponentPawns, G6)) {
        data->bishops[0] += scoreMG(BISHOP_TRAPPED);
        data->bishops[1] += scoreEG(BISHOP_TRAPPED);
      }
    } else {
      if ((sq == A2 || sq == B1) && getBit(opponentPawns, B3) && getBit(opponentPawns, C2)) {
        data->bishops[0] += scoreMG(BISHOP_TRAPPED);
        data->bishops[1] += scoreEG(BISHOP_TRAPPED);
      } else if ((sq == H2 || sq == G1) && getBit(opponentPawns, G3) && getBit(opponentPawns, F2)) {
        data->bishops[0] += scoreMG(BISHOP_TRAPPED);
        data->bishops[1] += scoreEG(BISHOP_TRAPPED);
      }
    }
  }

  data->bishops[0] += scoreMG(DEFENDED_MINOR) * bits(pawnAttacks & board->pieces[BISHOP[side]]);
  data->bishops[1] += scoreEG(DEFENDED_MINOR) * bits(pawnAttacks & board->pieces[BISHOP[side]]);

  // ROOKS
  for (BitBoard rooks = board->pieces[ROOK[side]]; rooks; popLsb(rooks)) {
    int sq = lsb(rooks);
    BitBoard sqBb = rooks & -rooks;
    int file = file(sq);

    data->material[0] += scoreMG(MATERIAL_VALUES[ROOK[side]]);
    data->material[1] += scoreEG(MATERIAL_VALUES[ROOK[side]]);
    data->rooks[0] += scoreMG(PSQT[ROOK[side]][sq]);
    data->rooks[1] += scoreEG(PSQT[ROOK[side]][sq]);

    BitBoard movement =
        getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]] ^ board->pieces[ROOK[side]]);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[ROOK[side] >> 1] |= movement;
    data->allAttacks |= movement;
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[3];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[0] += scoreMG(ROOK_MOBILITIES[bits(mobilitySquares)]);
    data->mobility[1] += scoreEG(ROOK_MOBILITIES[bits(mobilitySquares)]);

    if (sqBb & PROMOTION_RANKS[side]) {
      data->rooks[0] += scoreMG(ROOK_SEVENTH_RANK);
      data->rooks[1] += scoreEG(ROOK_SEVENTH_RANK);
    }

    if (!(FILE_MASKS[file] & myPawns)) {
      if (!(FILE_MASKS[file] & opponentPawns)) {
        data->rooks[0] += scoreMG(ROOK_OPEN_FILE);
        data->rooks[1] += scoreEG(ROOK_OPEN_FILE);
      } else {
        data->rooks[0] += scoreMG(ROOK_SEMI_OPEN);
        data->rooks[1] += scoreEG(ROOK_SEMI_OPEN);
      }

      if (FILE_MASKS[file] & board->pieces[KING[xside]]) {
        data->rooks[0] += scoreMG(ROOK_OPPOSITE_KING);
        data->rooks[1] += scoreEG(ROOK_OPPOSITE_KING);
      } else if (ADJACENT_FILE_MASKS[file] & board->pieces[KING[xside]]) {
        data->rooks[0] += scoreMG(ROOK_ADJACENT_KING);
        data->rooks[1] += scoreEG(ROOK_ADJACENT_KING);
      }
    }

    if (side == WHITE) {
      if ((sq == A1 || sq == A2 || sq == B1) && (kingSq == C1 || kingSq == B1)) {
        data->rooks[0] += scoreMG(ROOK_TRAPPED);
        data->rooks[1] += scoreEG(ROOK_TRAPPED);
      } else if ((sq == H1 || sq == H2 || sq == G1) && (kingSq == F1 || kingSq == G1)) {
        data->rooks[0] += scoreMG(ROOK_TRAPPED);
        data->rooks[1] += scoreEG(ROOK_TRAPPED);
      }
    } else {
      if ((sq == A8 || sq == A7 || sq == B8) && (kingSq == B8 || kingSq == C8)) {
        data->rooks[0] += scoreMG(ROOK_TRAPPED);
        data->rooks[1] += scoreEG(ROOK_TRAPPED);
      } else if ((sq == H8 || sq == H7 || sq == G8) && (kingSq == F8 || kingSq == G8)) {
        data->rooks[0] += scoreMG(ROOK_TRAPPED);
        data->rooks[1] += scoreEG(ROOK_TRAPPED);
      }
    }
  }

  // QUEENS
  for (BitBoard queens = board->pieces[QUEEN[side]]; queens; popLsb(queens)) {
    int sq = lsb(queens);

    data->material[0] += scoreMG(MATERIAL_VALUES[QUEEN[side]]);
    data->material[1] += scoreEG(MATERIAL_VALUES[QUEEN[side]]);
    data->queens[0] += scoreMG(PSQT[QUEEN[side]][sq]);
    data->queens[1] += scoreEG(PSQT[QUEEN[side]][sq]);

    BitBoard movement = getQueenAttacks(sq, board->occupancies[BOTH]);
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[QUEEN[side] >> 1] |= movement;
    data->allAttacks |= movement;
    if (movement & enemyKingArea) {
      data->attackWeight += KS_ATTACKER_WEIGHTS[4];
      data->attackCount += bits(movement & enemyKingArea);
      data->attackers++;
    }

    BitBoard mobilitySquares = movement & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility[0] += scoreMG(QUEEN_MOBILITIES[bits(mobilitySquares)]);
    data->mobility[1] += scoreEG(QUEEN_MOBILITIES[bits(mobilitySquares)]);
  }

  // KINGS
  for (BitBoard kings = board->pieces[KING[side]]; kings; popLsb(kings)) {
    int sq = lsb(kings);

    data->material[0] += scoreMG(MATERIAL_VALUES[KING[side]]);
    data->material[1] += scoreEG(MATERIAL_VALUES[KING[side]]);
    data->kings[0] += scoreMG(PSQT[KING[side]][sq]);
    data->kings[1] += scoreEG(PSQT[KING[side]][sq]);

    BitBoard movement = getKingAttacks(sq) & ~getKingAttacks(lsb(board->pieces[KING[xside]]));
    data->attacks2 |= (movement & data->allAttacks);
    data->attacks[KING[side] >> 1] |= movement;
    data->allAttacks |= movement;
  }
}

void EvaluateThreats(Board* board, int side, EvalData* data, EvalData* enemyData) {
  data->threats[0] = 0;
  data->threats[1] = 0;

  int xside = 1 - side;

  BitBoard* myAttacks = data->attacks;
  BitBoard* enemyAttacks = enemyData->attacks;

  BitBoard weak = board->occupancies[xside] & ~enemyAttacks[0] & data->allAttacks;

  for (BitBoard knightThreats = weak & myAttacks[1]; knightThreats; popLsb(knightThreats)) {
    int piece = pieceAt(lsb(knightThreats), xside, board);
    data->threats[0] += scoreMG(KNIGHT_THREATS[piece >> 1]);
    data->threats[1] += scoreEG(KNIGHT_THREATS[piece >> 1]);
  }

  for (BitBoard bishopThreats = weak & myAttacks[2]; bishopThreats; popLsb(bishopThreats)) {
    int piece = pieceAt(lsb(bishopThreats), xside, board);
    data->threats[0] += scoreMG(BISHOP_THREATS[piece >> 1]);
    data->threats[1] += scoreEG(BISHOP_THREATS[piece >> 1]);
  }

  for (BitBoard rookThreats = weak & myAttacks[3]; rookThreats; popLsb(rookThreats)) {
    int piece = pieceAt(lsb(rookThreats), xside, board);
    data->threats[0] += scoreMG(ROOK_THREATS[piece >> 1]);
    data->threats[1] += scoreEG(ROOK_THREATS[piece >> 1]);
  }

  for (BitBoard kingThreats = weak & myAttacks[5]; kingThreats; popLsb(kingThreats)) {
    int piece = pieceAt(lsb(kingThreats), xside, board);
    data->threats[0] += scoreMG(KING_THREATS[piece >> 1]);
    data->threats[1] += scoreEG(KING_THREATS[piece >> 1]);
  }

  BitBoard hanging = data->allAttacks & ~enemyData->allAttacks & board->occupancies[xside];
  data->threats[0] += scoreMG(HANGING_THREAT) * bits(hanging);
  data->threats[1] += scoreEG(HANGING_THREAT) * bits(hanging);
}

void EvaluateKingSafety(Board* board, int side, EvalData* data, EvalData* enemyData) {
  data->kingSafety[0] = 0;
  data->kingSafety[1] = 0;

  int xside = 1 - side;

  int kingSq = lsb(board->pieces[KING[side]]);

  if (!!(board->pieces[QUEEN[xside]]) + enemyData->attackers > 1) {
    BitBoard kingArea = getKingAttacks(kingSq);
    BitBoard weak = enemyData->allAttacks & ~data->attacks2 & (~data->allAttacks | data->attacks[4] | data->attacks[5]);
    BitBoard vulnerable = (~data->allAttacks | (weak & enemyData->attacks2)) & ~board->occupancies[xside];

    BitBoard possibleKnightChecks = getKnightAttacks(kingSq) & enemyData->attacks[1] & ~board->occupancies[xside];
    int safeChecks = bits(possibleKnightChecks & vulnerable);
    int unsafeChecks = bits(possibleKnightChecks & ~vulnerable);

    BitBoard possibleBishopChecks =
        getBishopAttacks(kingSq, board->occupancies[BOTH]) & enemyData->attacks[2] & ~board->occupancies[xside];
    safeChecks += bits(possibleBishopChecks & vulnerable);
    unsafeChecks += bits(possibleBishopChecks & ~vulnerable);

    BitBoard possibleRookChecks =
        getRookAttacks(kingSq, board->occupancies[BOTH]) & enemyData->attacks[3] & ~board->occupancies[xside];
    safeChecks += bits(possibleRookChecks & vulnerable);
    unsafeChecks += bits(possibleRookChecks & ~vulnerable);

    BitBoard possibleQueenChecks =
        getQueenAttacks(kingSq, board->occupancies[BOTH]) & enemyData->attacks[4] & ~board->occupancies[xside];
    safeChecks += bits(possibleQueenChecks & vulnerable);
    unsafeChecks += bits(possibleQueenChecks & ~vulnerable);

    BitBoard allies = kingArea & (board->occupancies[side] & ~board->pieces[QUEEN[side]]);

    int score = (enemyData->attackWeight + KS_SAFE_CHECK * safeChecks + KS_UNSAFE_CHECK * unsafeChecks +
                 KS_WEAK_SQS * bits(vulnerable & kingArea) + KS_ATTACK * (enemyData->attackCount * 8 / bits(kingArea)) +
                 KS_ENEMY_QUEEN * !(board->pieces[QUEEN[xside]]) + KS_ALLIES * (bits(allies) - 2));

    if (score > 0) {
      data->kingSafety[0] += -score * score / 1000;
      data->kingSafety[1] += -score / 30;
    }
  }

  if (!board->pieces[QUEEN[xside]])
    return;
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

    data->kingSafety[0] += s;
    data->kingSafety[1] += s;

    file = board->pieces[PAWN[xside]] & FILE_MASKS[c];
    if (file) {
      int pawnRank = side == WHITE ? rank(msb(file)) : 7 - rank(lsb(file));
      data->kingSafety[0] += PAWN_STORM[pawnRank];
      data->kingSafety[1] += PAWN_STORM[pawnRank];
    } else {
      data->kingSafety[0] += PAWN_STORM[6];
      data->kingSafety[1] += PAWN_STORM[6];
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

  return toScore(sideData, board) - toScore(xsideData, board);
}

inline int toScore(EvalData* data, Board* board) {
  int mg = data->pawns[0] + data->knights[0] + data->bishops[0] + data->rooks[0] + data->queens[0] + data->kings[0] +
           data->kingSafety[0] + data->material[0] + data->mobility[0] + data->threats[0];
  int eg = data->pawns[1] + data->knights[1] + data->bishops[1] + data->rooks[1] + data->queens[1] + data->kings[1] +
           data->kingSafety[1] + data->material[1] + data->mobility[1] + data->threats[1];

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
  printf(
      "| material   | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->material[0], whiteEval->material[1],
      taper(whiteEval->material[0], whiteEval->material[1], phase), blackEval->material[0], blackEval->material[1],
      taper(blackEval->material[0], blackEval->material[1], phase), whiteEval->material[0] - blackEval->material[0],
      whiteEval->material[1] - blackEval->material[1],
      taper(whiteEval->material[0] - blackEval->material[0], whiteEval->material[1] - blackEval->material[1], phase));
  printf("| pawns      | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->pawns[0], whiteEval->pawns[1],
         taper(whiteEval->pawns[0], whiteEval->pawns[1], phase), blackEval->pawns[0], blackEval->pawns[1],
         taper(blackEval->pawns[0], blackEval->pawns[1], phase), whiteEval->pawns[0] - blackEval->pawns[0],
         whiteEval->pawns[1] - blackEval->pawns[1],
         taper(whiteEval->pawns[0] - blackEval->pawns[0], whiteEval->pawns[1] - blackEval->pawns[1], phase));
  printf("| knights    | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->knights[0], whiteEval->knights[1],
         taper(whiteEval->knights[0], whiteEval->knights[1], phase), blackEval->knights[0], blackEval->knights[1],
         taper(blackEval->knights[0], blackEval->knights[1], phase), whiteEval->knights[0] - blackEval->knights[0],
         whiteEval->knights[1] - blackEval->knights[1],
         taper(whiteEval->knights[0] - blackEval->knights[0], whiteEval->knights[1] - blackEval->knights[1], phase));
  printf("| bishops    | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->bishops[0], whiteEval->bishops[1],
         taper(whiteEval->bishops[0], whiteEval->bishops[1], phase), blackEval->bishops[0], blackEval->bishops[1],
         taper(blackEval->bishops[0], blackEval->bishops[1], phase), whiteEval->bishops[0] - blackEval->bishops[0],
         whiteEval->bishops[1] - blackEval->bishops[1],
         taper(whiteEval->bishops[0] - blackEval->bishops[0], whiteEval->bishops[1] - blackEval->bishops[1], phase));
  printf("| rooks      | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->rooks[0], whiteEval->rooks[1],
         taper(whiteEval->rooks[0], whiteEval->rooks[1], phase), blackEval->rooks[0], blackEval->rooks[1],
         taper(blackEval->rooks[0], blackEval->rooks[1], phase), whiteEval->rooks[0] - blackEval->rooks[0],
         whiteEval->rooks[1] - blackEval->rooks[1],
         taper(whiteEval->rooks[0] - blackEval->rooks[0], whiteEval->rooks[1] - blackEval->rooks[1], phase));
  printf("| queens     | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->queens[0], whiteEval->queens[1],
         taper(whiteEval->queens[0], whiteEval->queens[1], phase), blackEval->queens[0], blackEval->queens[1],
         taper(blackEval->queens[0], blackEval->queens[1], phase), whiteEval->queens[0] - blackEval->queens[0],
         whiteEval->queens[1] - blackEval->queens[1],
         taper(whiteEval->queens[0] - blackEval->queens[0], whiteEval->queens[1] - blackEval->queens[1], phase));
  printf("| kings      | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->kings[0], whiteEval->kings[1],
         taper(whiteEval->kings[0], whiteEval->kings[1], phase), blackEval->kings[0], blackEval->kings[1],
         taper(blackEval->kings[0], blackEval->kings[1], phase), whiteEval->kings[0] - blackEval->kings[0],
         whiteEval->kings[1] - blackEval->kings[1],
         taper(whiteEval->kings[0] - blackEval->kings[0], whiteEval->kings[1] - blackEval->kings[1], phase));
  printf(
      "| mobility   | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->mobility[0], whiteEval->mobility[1],
      taper(whiteEval->mobility[0], whiteEval->mobility[1], phase), blackEval->mobility[0], blackEval->mobility[1],
      taper(blackEval->mobility[0], blackEval->mobility[1], phase), whiteEval->mobility[0] - blackEval->mobility[0],
      whiteEval->mobility[1] - blackEval->mobility[1],
      taper(whiteEval->mobility[0] - blackEval->mobility[0], whiteEval->mobility[1] - blackEval->mobility[1], phase));
  printf("| threats    | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->threats[0], whiteEval->threats[1],
         taper(whiteEval->threats[0], whiteEval->threats[1], phase), blackEval->threats[0], blackEval->threats[1],
         taper(blackEval->threats[0], blackEval->threats[1], phase), whiteEval->threats[0] - blackEval->threats[0],
         whiteEval->threats[1] - blackEval->threats[1],
         taper(whiteEval->threats[0] - blackEval->threats[0], whiteEval->threats[1] - blackEval->threats[1], phase));
  printf("| kingSafety | %5d %5d %5d | %5d %5d %5d | %5d %5d %5d |\n", whiteEval->kingSafety[0],
         whiteEval->kingSafety[1], taper(whiteEval->kingSafety[0], whiteEval->kingSafety[1], phase),
         blackEval->kingSafety[0], blackEval->kingSafety[1],
         taper(blackEval->kingSafety[0], blackEval->kingSafety[1], phase),
         whiteEval->kingSafety[0] - blackEval->kingSafety[0], whiteEval->kingSafety[1] - blackEval->kingSafety[1],
         taper(whiteEval->kingSafety[0] - blackEval->kingSafety[0], whiteEval->kingSafety[1] - blackEval->kingSafety[1],
               phase));
  printf("|------------|-------------------|-------------------|-------------------|\n");
  printf("\nResult (white): %5d\n\n", toScore(whiteEval, board) - toScore(blackEval, board));
}
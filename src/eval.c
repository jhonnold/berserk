#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "types.h"

#define S(mg, eg) (makeScore((mg), (eg)))
#define rel(sq, side) ((side) ? MIRROR[(sq)] : (sq))

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
  S(   0,   0), S(   0,   0), S(  24,  16), S(  48,  32), S(  48,  32), S(  24,  16), S(   0,   0), S(   0,   0),
  S(   0,   0), S(  15,  10), S(  30,  20), S(  48,  32), S(  48,  32), S(  30,  20), S(  15,  10), S(   0,   0),
  S(   0,   0), S(  15,  10), S(  30,  20), S(  36,  24), S(  36,  24), S(  30,  20), S(  15,  10), S(   0,   0),
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

const int PAWN_SHELTER = -36;
const int PAWN_STORM[8] = {0, 0, 0, -10, -30, -60, 0, 0};

const int DEFENDED_MINOR = S(5, 2);
const int ROOK_OPEN_FILE = 20;
const int ROOK_SEMI_OPEN_FILE = 10;
const int ROOK_SEVENTH_RANK = 20;
const int ROOK_OPPOSITE_KING = S(10, 0);
const int ROOK_TRAPPED = S(-75, -75);
const int BISHOP_TRAPPED = S(-150, -150);

const int ATTACK_WEIGHTS[] = {0, 50, 75, 88, 94, 97, 99};

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
  data->attacking = 0;
  data->kingSafety = 0;

  int xside = 1 - side;
  int phase = getPhase(board);

  // Random utility stuff
  BitBoard myPawns = board->pieces[PAWN[side]];
  BitBoard opponentPawns = board->pieces[PAWN[xside]];
  BitBoard allPawns = myPawns | opponentPawns;

  BitBoard opponentPawnAttacksE = shift(opponentPawns, PAWN_DIRECTIONS[xside] + E);
  BitBoard opponentPawnAttacksW = shift(opponentPawns, PAWN_DIRECTIONS[xside] + W);
  BitBoard oppoPawnAttacks = opponentPawnAttacksE | opponentPawnAttacksW;

  BitBoard myBlockedAndHomePawns =
      (shift(board->occupancies[BOTH], PAWN_DIRECTIONS[xside]) | HOME_RANKS[side] | THIRD_RANKS[side]) &
      board->pieces[PAWN[side]];

  int kingSq = lsb(board->pieces[KING[side]]);
  int oppoKingSq = lsb(board->pieces[KING[xside]]);
  BitBoard oppoKingArea = getKingAttacks(oppoKingSq);
  int attackScore = 0;
  int attackers = 0;

  // PAWNS
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

  // KNIGHTS
  for (BitBoard knights = board->pieces[KNIGHT[side]]; knights; popLsb(knights)) {
    int sq = lsb(knights);

    data->material += taper(MATERIAL_VALUES[KNIGHT[side]], phase);
    data->knights += taper(PSQT[KNIGHT[side]][sq], phase);

    BitBoard mobilitySquares = getKnightAttacks(sq) & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility += taper(KNIGHT_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getKnightAttacks(sq) & oppoKingArea;
    if (attacksNearKing) {
      attackers++;
      attackScore += 20 * bits(attacksNearKing);
    }

    if (KNIGHT_POST_PSQT[rel(sq, side)] && (getPawnAttacks(sq, xside) & myPawns) &&
        !(fill(getPawnAttacks(sq, side), PAWN_DIRECTIONS[side]) & board->pieces[PAWN[xside]]))
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

    BitBoard mobilitySquares = getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]) &
                               ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility += taper(BISHOP_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing =
        getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]) & oppoKingArea;
    if (attacksNearKing) {
      attackers++;
      attackScore += 20 * bits(attacksNearKing);
    }

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

    BitBoard mobilitySquares =
        getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]] ^ board->pieces[ROOK[side]]) &
        ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility += taper(ROOK_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing =
        getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]] ^ board->pieces[ROOK[side]]) &
        oppoKingArea;
    if (attacksNearKing) {
      attackers++;
      attackScore += 40 * bits(attacksNearKing);
    }

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

    BitBoard mobilitySquares =
        getQueenAttacks(sq, board->occupancies[BOTH]) & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    data->mobility += taper(QUEEN_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getQueenAttacks(sq, board->occupancies[BOTH]) & oppoKingArea;
    if (attacksNearKing) {
      attackers++;
      attackScore += 80 * bits(attacksNearKing);
    }
  }

  // KINGS
  for (BitBoard kings = board->pieces[KING[side]]; kings; popLsb(kings)) {
    int sq = lsb(kings);

    data->material += taper(MATERIAL_VALUES[KING[side]], phase);
    data->kings += taper(PSQT[KING[side]][sq], phase);
  }

  // KING SAFETY AND ATTACKING
  if (board->pieces[QUEEN[xside]] &&
      (board->pieces[KNIGHT[xside]] | board->pieces[BISHOP[xside]] | board->pieces[ROOK[xside]])) {
    data->attacking += attackScore * ATTACK_WEIGHTS[attackers] / 100;
    int kingFile = file(kingSq);

    // shelter and storm
    for (int c = kingFile - 1; c <= kingFile + 1; c++) {
      if (c < 0 || c > 7)
        continue;

      BitBoard file = myPawns & FILE_MASKS[c];
      if (file) {
        int pawnRank = side == WHITE ? rank(msb(file)) : 7 - rank(lsb(file));
        data->kingSafety += PAWN_SHELTER + pawnRank * pawnRank;
      } else {
        data->kingSafety += PAWN_SHELTER;
      }

      file = opponentPawns & FILE_MASKS[c];
      if (file) {
        int pawnRank = side == WHITE ? rank(msb(file)) : 7 - rank(lsb(file));
        data->kingSafety += PAWN_STORM[pawnRank];
      } else {
        data->kingSafety += PAWN_STORM[6];
      }
    }
  }
}

int Evaluate(Board* board) {
  EvalData sideData[1];
  EvalData xsideData[1];

  EvaluateSide(board, board->side, sideData);
  EvaluateSide(board, board->xside, xsideData);

  return toScore(sideData) - toScore(xsideData);
}

inline int toScore(EvalData* data) {
  return data->pawns + data->knights + data->bishops + data->rooks + data->queens + data->kings + data->kingSafety +
         data->attacking + data->material + data->mobility;
}

void PrintEvaluation(Board* board) {
  EvalData whiteEval[1];
  EvalData blackEval[1];

  EvaluateSide(board, WHITE, whiteEval);
  EvaluateSide(board, BLACK, blackEval);

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
  printf("| attacking  | %5d | %5d | %5d |\n", whiteEval->attacking, blackEval->attacking,
         whiteEval->attacking - blackEval->attacking);
  printf("| kingSafety | %5d | %5d | %5d |\n", whiteEval->kingSafety, blackEval->kingSafety,
         whiteEval->kingSafety - blackEval->kingSafety);
  printf("|------------|-------|-------|-------|\n");
  printf("\nResult (white): %5d\n\n", toScore(whiteEval) - toScore(blackEval));
}
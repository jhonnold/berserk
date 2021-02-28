#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "types.h"

#define S(mg, eg) (makeScore((mg), (eg)))
#define rel(sq, side) ((side) ? mirror[(sq)] : (sq))

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

const int DOUBLED_PAWN = S(-10, -10);
const int ISOLATED_PAWN = S(-10, -10);
const int BACKWARDS_PAWN = S(-20, -7);
const int DEFENDED_PAWN = S(10, 10);
const int CONNECTED_PAWN[2][8] = {{
                                      S(0, 0),
                                      S(30, 30),
                                      S(20, 5),
                                      S(10, 2),
                                      S(5, 0),
                                      S(3, 0),
                                      S(2, 0),
                                      S(0, 0),
                                  },
                                  {
                                      S(0, 0),
                                      S(2, 0),
                                      S(3, 0),
                                      S(5, 0),
                                      S(10, 2),
                                      S(20, 5),
                                      S(30, 30),
                                      S(0, 0),
                                  }};

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

const int DEFENDED_MINOR = S(5, 2);
const int KNIGHT_OUTPOST = 5;
const int GOOD_KNIGHT_OUTPOST = 10;
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
    PSQT[PAWN[BLACK]][mirror[sq]] = PAWN_PSQT[sq];
    PSQT[KNIGHT[WHITE]][sq] = KNIGHT_PSQT[sq];
    PSQT[KNIGHT[BLACK]][mirror[sq]] = KNIGHT_PSQT[sq];
    PSQT[BISHOP[WHITE]][sq] = BISHOP_PSQT[sq];
    PSQT[BISHOP[BLACK]][mirror[sq]] = BISHOP_PSQT[sq];
    PSQT[ROOK[WHITE]][sq] = ROOK_PSQT[sq];
    PSQT[ROOK[BLACK]][mirror[sq]] = ROOK_PSQT[sq];
    PSQT[QUEEN[WHITE]][sq] = QUEEN_PSQT[sq];
    PSQT[QUEEN[BLACK]][mirror[sq]] = QUEEN_PSQT[sq];
    PSQT[KING[WHITE]][sq] = KING_PSQT[sq];
    PSQT[KING[BLACK]][mirror[sq]] = KING_PSQT[sq];
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

int EvaluateSide(Board* board, int side) {
  int score = 0;
  int xside = 1 - side;
  int phase = getPhase(board);

  // Material + PSQT
  for (int piece = PAWN[side]; piece <= KING[side]; piece += 2) {
    BitBoard pieces = board->pieces[piece];
    while (pieces) {
      int sq = lsb(pieces);

      score += taper(MATERIAL_VALUES[piece], phase);
      score += taper(PSQT[piece][sq], phase);

      popLsb(pieces);
    }
  }

  if (bits(board->pieces[BISHOP[side]]) > 1)
    score += taper(BISHOP_PAIR, phase);

  for (BitBoard pawns = board->pieces[PAWN[side]]; pawns; popLsb(pawns)) {
    BitBoard sqBitboard = pawns & -pawns;
    int sq = lsb(pawns);

    int file = file(sq);
    int rank = rank(sq);

    BitBoard opposed = board->pieces[PAWN[xside]] & FILE_MASKS[file] & FORWARD_RANK_MASKS[side][rank];
    BitBoard doubled = board->pieces[PAWN[side]] & shift(sqBitboard, PAWN_DIRECTIONS[xside]);
    BitBoard neighbors = board->pieces[PAWN[side]] & ADJACENT_FILE_MASKS[file];
    BitBoard connected = neighbors & RANK_MASKS[rank];
    BitBoard defenders = board->pieces[PAWN[side]] & getPawnAttacks(sq, xside);
    // BitBoard levers = board->pieces[PAWN[xside]] & getPawnAttacks(sq, side);
    BitBoard forwardLever = board->pieces[PAWN[xside]] & getPawnAttacks(sq + PAWN_DIRECTIONS[side], side);

    int backwards = !(neighbors & FORWARD_RANK_MASKS[xside][sq + PAWN_DIRECTIONS[side]]) && forwardLever;
    int passed =
        !(board->pieces[PAWN[xside]] & FORWARD_RANK_MASKS[side][rank] & (ADJACENT_FILE_MASKS[file] | FILE_MASKS[file]));

    if (doubled)
      score += taper(DOUBLED_PAWN, phase);

    if (!neighbors)
      score += taper(ISOLATED_PAWN, phase);

    if (backwards)
      score += taper(BACKWARDS_PAWN, phase);

    if (passed)
      score += taper(PASSED_PAWN[side][rank], phase);

    if (defenders | connected) {
      int s = 2;
      if (connected)
        s++;
      if (opposed)
        s--;

      score += taper(CONNECTED_PAWN[side][rank], phase) * s + taper(DEFENDED_PAWN, phase) * bits(defenders);
    }
  }

  // PAWNS
  BitBoard myPawns = board->pieces[PAWN[side]];
  BitBoard opponentPawns = board->pieces[PAWN[xside]];
  BitBoard allPawns = myPawns | opponentPawns;

  BitBoard myPawnAttacksE = shift(myPawns, PAWN_DIRECTIONS[side] + E);
  BitBoard myPawnAttacksW = shift(myPawns, PAWN_DIRECTIONS[side] + W);
  BitBoard myPawnAttacks = myPawnAttacksE | myPawnAttacksW;

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

  // KNIGHTS
  BitBoard myKnights = board->pieces[KNIGHT[side]];

  while (myKnights) {
    int sq = lsb(myKnights);

    BitBoard mobilitySquares = getKnightAttacks(sq) & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(KNIGHT_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getKnightAttacks(sq) & oppoKingArea;
    if (attacksNearKing) {
      attackers++;
      attackScore += 20 * bits(attacksNearKing);
    }

    if (KNIGHT_POST_PSQT[rel(sq, side)] && (getPawnAttacks(sq, side) & myPawns)) {
      score += taper(KNIGHT_POST_PSQT[rel(sq, side)], phase);
      if (!(fill(getPawnAttacks(sq, side), PAWN_DIRECTIONS[side]) & opponentPawns))
        score += GOOD_KNIGHT_OUTPOST;
    }

    popLsb(myKnights);
  }

  // BISHOPS
  BitBoard myBishops = board->pieces[BISHOP[side]];

  while (myBishops) {
    int sq = lsb(myBishops);

    BitBoard mobilitySquares = getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]) &
                               ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(BISHOP_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing =
        getBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]) & oppoKingArea;
    if (attacksNearKing) {
      attackers++;
      attackScore += 20 * bits(attacksNearKing);
    }

    if (side == WHITE) {
      if ((sq == A7 || sq == B8) && getBit(opponentPawns, B6) && getBit(opponentPawns, C7))
        score += taper(BISHOP_TRAPPED, phase);
      else if ((sq == H7 || sq == G8) && getBit(opponentPawns, F7) && getBit(opponentPawns, G6))
        score += taper(BISHOP_TRAPPED, phase);
    } else {
      if ((sq == A2 || sq == B1) && getBit(opponentPawns, B3) && getBit(opponentPawns, C2))
        score += taper(BISHOP_TRAPPED, phase);
      else if ((sq == H2 || sq == G1) && getBit(opponentPawns, G3) && getBit(opponentPawns, F2))
        score += taper(BISHOP_TRAPPED, phase);
    }

    popLsb(myBishops);
  }

  score +=
      taper(DEFENDED_MINOR, phase) * bits(myPawnAttacks & (board->pieces[KNIGHT[side]] | board->pieces[BISHOP[side]]));

  // ROOKS
  BitBoard myRooks = board->pieces[ROOK[side]];

  while (myRooks) {
    int sq = lsb(myRooks);
    int file = file(sq);

    BitBoard mobilitySquares =
        getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]] ^ board->pieces[ROOK[side]]) &
        ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(ROOK_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing =
        getRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]] ^ board->pieces[ROOK[side]]) &
        oppoKingArea;
    if (attacksNearKing) {
      attackers++;
      attackScore += 40 * bits(attacksNearKing);
    }

    if (rank(rel(sq, side)) == 1)
      score += ROOK_SEVENTH_RANK;

    if (!(FILE_MASKS[file] & allPawns)) {
      score += ROOK_OPEN_FILE;
      if (file == file(oppoKingSq))
        score += taper(ROOK_OPPOSITE_KING, phase);
    } else if (!(FILE_MASKS[file] & myPawns)) {
      score += ROOK_SEMI_OPEN_FILE;
      if (file == file(oppoKingSq))
        score += taper(ROOK_OPPOSITE_KING, phase);
    }

    if (side == WHITE) {
      if ((sq == A1 || sq == A2 || sq == B1) && (kingSq == C1 || kingSq == B1))
        score += taper(ROOK_TRAPPED, phase);
      else if ((sq == H1 || sq == H2 || sq == G1) && (kingSq == F1 || kingSq == G1))
        score += taper(ROOK_TRAPPED, phase);
    } else {
      if ((sq == A8 || sq == A7 || sq == B8) && (kingSq == B8 || kingSq == C8))
        score += taper(ROOK_TRAPPED, phase);
      else if ((sq == H8 || sq == H7 || sq == G8) && (kingSq == F8 || kingSq == G8))
        score += taper(ROOK_TRAPPED, phase);
    }

    popLsb(myRooks);
  }

  // QUEENS
  BitBoard myQueens = board->pieces[QUEEN[side]];

  while (myQueens) {
    int sq = lsb(myQueens);

    BitBoard mobilitySquares =
        getQueenAttacks(sq, board->occupancies[BOTH]) & ~oppoPawnAttacks & ~myBlockedAndHomePawns;
    score += taper(QUEEN_MOBILITIES[bits(mobilitySquares)], phase);

    BitBoard attacksNearKing = getQueenAttacks(sq, board->occupancies[BOTH]) & oppoKingArea;
    if (attacksNearKing) {
      attackers++;
      attackScore += 80 * bits(attacksNearKing);
    }

    popLsb(myQueens);
  }

  // KING
  score += attackScore * ATTACK_WEIGHTS[attackers] / 100;
  int kingFile = file(kingSq);
  for (int c = kingFile - 1; c <= kingFile + 1; c++) {
    if (c < 0 || c > 7)
      continue;

    BitBoard file = myPawns & FILE_MASKS[c];
    if (file) {
      int pawnRank = side == WHITE ? rank(msb(file)) : 7 - rank(lsb(file));
      score += -taper(PAWN_SHIELD, phase) * (36 - pawnRank * pawnRank) / 100;
    } else {
      score += -taper(PAWN_SHIELD, phase) * 36 / 100;
    }
  }

  return score;
}

int Evaluate(Board* board) {
  int score = 0;

  score += EvaluateSide(board, board->side);
  score -= EvaluateSide(board, board->xside);

  return score;
}
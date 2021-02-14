#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "movegen.h"
#include "types.h"

const int pawnDirections[] = {-8, 8};
const bb_t middleFour = 281474976645120UL;
const bb_t promotionRanks[] = {65280UL, 71776119061217280L};
const bb_t homeRanks[] = {71776119061217280L, 65280UL};
const bb_t thirdRanks[] = {280375465082880L, 16711680L};

void addMove(moves_t *moveList, move_t m) { moveList->moves[moveList->count++] = m; }

static inline bb_t shiftUp(bb_t bb) {
  return side == 0 ? bb >> 8 : bb << 8;
}

void generatePawnPromotions(moves_t *moveList) {
  bb_t pawns = pieces[side];
  int dir = pawnDirections[side];
  bb_t promotingPawns = pawns & promotionRanks[side];

  while (promotingPawns) {
    int start = lsb(promotingPawns);
    int end = start + dir;

    if (!getBit(occupancies[2], end)) {
      addMove(moveList, buildMove(start, end, side, 8 + side, 0, 0, 0, 0));
      addMove(moveList, buildMove(start, end, side, 6 + side, 0, 0, 0, 0));
      addMove(moveList, buildMove(start, end, side, 4 + side, 0, 0, 0, 0));
      addMove(moveList, buildMove(start, end, side, 2 + side, 0, 0, 0, 0));
    }

    long attacks = getPawnAttacks(start, side) & occupancies[xside];
    while (attacks) {
      end = lsb(attacks);

      addMove(moveList, buildMove(start, end, side, 8 + side, 1, 0, 0, 0));
      addMove(moveList, buildMove(start, end, side, 6 + side, 1, 0, 0, 0));
      addMove(moveList, buildMove(start, end, side, 4 + side, 1, 0, 0, 0));
      addMove(moveList, buildMove(start, end, side, 2 + side, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(promotingPawns);
  }
}

void generatePawnCaptures(moves_t *moveList) {
  bb_t pawns = pieces[side];
  bb_t nonPromotingPawns = pawns & ~promotionRanks[side];

  while (nonPromotingPawns) {
    int start = lsb(nonPromotingPawns);

    long attacks = getPawnAttacks(start, side) & occupancies[xside];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, side, 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    if (epSquare) {
      attacks = getPawnAttacks(start, side) & (1UL << epSquare);
      if (attacks)
        addMove(moveList, buildMove(start, epSquare, side, 0, 1, 0, 1, 0));
    }

    popLsb(nonPromotingPawns);
  }
}

void generatePawnQuiets(moves_t *moveList) {
  bb_t empty = ~occupancies[2];
  bb_t pawns = pieces[side];
  bb_t nonPromotingPawns = pawns & ~promotionRanks[side];

  bb_t singlePush = shiftUp(nonPromotingPawns) & empty;
  bb_t doublePush = shiftUp(singlePush & thirdRanks[side]) & empty;

  while (singlePush) {
    int end = lsb(singlePush);
    addMove(moveList, buildMove(end - pawnDirections[side], end, side, 0, 0, 0, 0, 0));
    popLsb(singlePush);
  }

  while (doublePush) {
    int end = lsb(doublePush);
    addMove(moveList, buildMove(end - pawnDirections[side] - pawnDirections[side], end, side, 0, 0, 1, 0, 0));
    popLsb(doublePush);
  }
}

void generatePawnMoves(moves_t *moveList) {
  generatePawnPromotions(moveList);
  generatePawnCaptures(moveList);
  generatePawnQuiets(moveList);
}

void generateKnightCaptures(moves_t *moveList) {
  int piece = 2 + side;
  bb_t knights = pieces[piece];

  while (knights) {
    int start = lsb(knights);

    bb_t attacks = getKnightAttacks(start) & occupancies[xside];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(knights);
  }
}

void generateKnightQuiets(moves_t *moveList) {
  int piece = 2 + side;
  bb_t knights = pieces[piece];

  while (knights) {
    int start = lsb(knights);

    bb_t attacks = getKnightAttacks(start) & ~occupancies[2];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(knights);
  }
}

void generateKnightMoves(moves_t *moveList) {
  generateKnightCaptures(moveList);
  generateKnightQuiets(moveList);
}

void generateBishopCaptures(moves_t *moveList) {
  int piece = 4 + side;
  bb_t bishops = pieces[piece];

  while (bishops) {
    int start = lsb(bishops);

    bb_t attacks = getBishopAttacks(start, occupancies[2]) & occupancies[xside];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(bishops);
  }
}

void generateBishopQuiets(moves_t *moveList) {
  int piece = 4 + side;
  bb_t bishops = pieces[piece];

  while (bishops) {
    int start = lsb(bishops);

    bb_t attacks = getBishopAttacks(start, occupancies[2]) & ~occupancies[2];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(bishops);
  }
}

void generateBishopMoves(moves_t *moveList) {
  generateBishopCaptures(moveList);
  generateBishopQuiets(moveList);
}

void generateRookCaptures(moves_t *moveList) {
  int piece = 6 + side;
  bb_t rooks = pieces[piece];

  while (rooks) {
    int start = lsb(rooks);

    bb_t attacks = getRookAttacks(start, occupancies[2]) & occupancies[xside];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(rooks);
  }
}

void generateRookQuiets(moves_t *moveList) {
  int piece = 6 + side;
  bb_t rooks = pieces[piece];

  while (rooks) {
    int start = lsb(rooks);

    bb_t attacks = getRookAttacks(start, occupancies[2]) & ~occupancies[2];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(rooks);
  }
}

void generateRookMoves(moves_t *moveList) {
  generateRookCaptures(moveList);
  generateRookQuiets(moveList);
}

void generateQueenCaptures(moves_t *moveList) {
  int piece = 8 + side;
  bb_t queens = pieces[piece];

  while (queens) {
    int start = lsb(queens);

    bb_t attacks = getQueenAttacks(start, occupancies[2]) & occupancies[xside];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(queens);
  }
}

void generateQueenQuiets(moves_t *moveList) {
  int piece = 8 + side;
  bb_t queens = pieces[piece];

  while (queens) {
    int start = lsb(queens);

    bb_t attacks = getQueenAttacks(start, occupancies[2]) & ~occupancies[2];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(queens);
  }
}

void generateQueenMoves(moves_t *moveList) {
  generateQueenCaptures(moveList);
  generateQueenQuiets(moveList);
}

void generateKingCaptures(moves_t *moveList) {
  int piece = 10 + side;
  bb_t kings = pieces[piece];

  while (kings) {
    int start = lsb(kings);

    bb_t attacks = getKingAttacks(start) & occupancies[xside];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(kings);
  }
}

void generateKingCastles(moves_t *moveList) {
  int piece = 10 + side;

  if (inCheck())
    return;

  if (side == 0) {
    if ((castling & 0x8) && !getBit(occupancies[2], 61) && !getBit(occupancies[2], 62) && !isSquareAttacked(61, 1) && !isSquareAttacked(62, 1))
      addMove(moveList, buildMove(60, 62, piece, 0, 0, 0, 0, 1));
    if ((castling & 0x4) && !getBit(occupancies[2], 57) && !getBit(occupancies[2], 58) && !getBit(occupancies[2], 59) && !isSquareAttacked(59, 1) && !isSquareAttacked(58, 1))
      addMove(moveList, buildMove(60, 58, piece, 0, 0, 0, 0, 1));
  } else {
    if ((castling & 0x2) && !getBit(occupancies[2], 5) && !getBit(occupancies[2], 6) && !isSquareAttacked(5, 0) && !isSquareAttacked(6, 0))
      addMove(moveList, buildMove(4, 6, piece, 0, 0, 0, 0, 1));
    if ((castling & 0x1) && !getBit(occupancies[2], 1) && !getBit(occupancies[2], 2) && !getBit(occupancies[2], 3) && !isSquareAttacked(3, 0) && !isSquareAttacked(2, 0))
      addMove(moveList, buildMove(4, 2, piece, 0, 0, 0, 0, 1));
  }
}

void generateKingQuiets(moves_t *moveList) {
  int piece = 10 + side;
  bb_t kings = pieces[piece];

  while (kings) {
    int start = lsb(kings);

    bb_t attacks = getKingAttacks(start) & ~occupancies[2];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(kings);
  }
}

void generateKingMoves(moves_t *moveList) {
  generateKingCaptures(moveList);
  generateKingCastles(moveList);
  generateKingQuiets(moveList);
}

void generateMoves(moves_t *moveList) {
  moveList->count = 0;

  generatePawnMoves(moveList);
  generateKnightMoves(moveList);
  generateBishopMoves(moveList);
  generateRookMoves(moveList);
  generateQueenMoves(moveList);
  generateKingMoves(moveList);
}

void printMoves(moves_t *moveList) {
  printf("move  p c d e t\n");

  for (int i = 0; i < moveList->count; i++) {
    move_t m = moveList->moves[i];
    printf("%s%s%c %c %d %d %d %d\n", idxToCord[moveStart(m)], idxToCord[moveEnd(m)], movePromo(m) ? pieceChars[movePromo(m)] : ' ', pieceChars[movePiece(m)], moveCapture(m),
           moveDouble(m), moveEP(m), moveCastle(m));
  }
}
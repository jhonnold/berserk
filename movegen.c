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
const bb_t filled = -1ULL;

inline void addMove(moves_t *moveList, move_t m) { moveList->moves[moveList->count++] = m; }

void generatePawnPromotions(moves_t *moveList, bb_t pawns, bb_t possibilities) {
  bb_t promotingPawns = pawns & promotionRanks[side];

  bb_t quietPromoters = shift(promotingPawns, pawnDirections[side]) & ~occupancies[2] & possibilities;
  bb_t capturingPromotersE = shift(promotingPawns, pawnDirections[side] - 1) & occupancies[xside] & possibilities;
  bb_t capturingPromotersW = shift(promotingPawns, pawnDirections[side] + 1) & occupancies[xside] & possibilities;

  while (quietPromoters) {
    int end = lsb(quietPromoters);
    int start = end - pawnDirections[side];

    addMove(moveList, buildMove(start, end, side, 8 + side, 0, 0, 0, 0));
    addMove(moveList, buildMove(start, end, side, 6 + side, 0, 0, 0, 0));
    addMove(moveList, buildMove(start, end, side, 4 + side, 0, 0, 0, 0));
    addMove(moveList, buildMove(start, end, side, 2 + side, 0, 0, 0, 0));

    popLsb(quietPromoters);
  }

  while (capturingPromotersE) {
    int end = lsb(capturingPromotersE);
    int start = end - (pawnDirections[side] - 1);

    addMove(moveList, buildMove(start, end, side, 8 + side, 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, side, 6 + side, 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, side, 4 + side, 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, side, 2 + side, 1, 0, 0, 0));

    popLsb(capturingPromotersE);
  }

  while (capturingPromotersW) {
    int end = lsb(capturingPromotersW);
    int start = end - (pawnDirections[side] + 1);

    addMove(moveList, buildMove(start, end, side, 8 + side, 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, side, 6 + side, 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, side, 4 + side, 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, side, 2 + side, 1, 0, 0, 0));

    popLsb(capturingPromotersW);
  }
}

void generatePawnCaptures(moves_t *moveList, bb_t pawns, bb_t possibilities) {
  bb_t nonPromotingPawns = pawns & ~promotionRanks[side];
  bb_t capturingE = shift(nonPromotingPawns, pawnDirections[side] - 1) & occupancies[xside] & possibilities;
  bb_t capturingW = shift(nonPromotingPawns, pawnDirections[side] + 1) & occupancies[xside] & possibilities;

  while (capturingE) {
    int end = lsb(capturingE);

    addMove(moveList, buildMove(end - (pawnDirections[side] - 1), end, side, 0, 1, 0, 0, 0));

    popLsb(capturingE);
  }

  while (capturingW) {
    int end = lsb(capturingW);
    addMove(moveList, buildMove(end - (pawnDirections[side] + 1), end, side, 0, 1, 0, 0, 0));
    popLsb(capturingW);
  }

  if (epSquare) {
    bb_t epPawns = getPawnAttacks(epSquare, xside) & nonPromotingPawns;

    while (epPawns) {
      int start = lsb(epPawns);
      addMove(moveList, buildMove(start, epSquare, side, 0, 1, 0, 1, 0));
      popLsb(epPawns);
    }
  }
}

void generatePawnQuiets(moves_t *moveList, bb_t pawns, bb_t possibilities) {
  bb_t empty = ~occupancies[2];
  bb_t nonPromotingPawns = pawns & ~promotionRanks[side];

  bb_t singlePush = shift(nonPromotingPawns, pawnDirections[side]) & empty;
  bb_t doublePush = shift(singlePush & thirdRanks[side], pawnDirections[side]) & empty;

  singlePush &= possibilities;
  doublePush &= possibilities;

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

void generatePawnMoves(moves_t *moveList, bb_t pawns, bb_t possibilities) {
  generatePawnPromotions(moveList, pawns, possibilities);
  generatePawnCaptures(moveList, pawns, possibilities);
  generatePawnQuiets(moveList, pawns, possibilities);
}

void generateKnightCaptures(moves_t *moveList, bb_t knights, bb_t possibilities) {
  int piece = 2 + side;

  while (knights) {
    int start = lsb(knights);

    bb_t attacks = getKnightAttacks(start) & occupancies[xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(knights);
  }
}

void generateKnightQuiets(moves_t *moveList, bb_t knights, bb_t possibilities) {
  int piece = 2 + side;

  while (knights) {
    int start = lsb(knights);

    bb_t attacks = getKnightAttacks(start) & ~occupancies[2] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(knights);
  }
}

void generateKnightMoves(moves_t *moveList, bb_t knights, bb_t possibilities) {
  generateKnightCaptures(moveList, knights, possibilities);
  generateKnightQuiets(moveList, knights, possibilities);
}

void generateBishopCaptures(moves_t *moveList, bb_t bishops, bb_t possibilities) {
  int piece = 4 + side;

  while (bishops) {
    int start = lsb(bishops);

    bb_t attacks = getBishopAttacks(start, occupancies[2]) & occupancies[xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(bishops);
  }
}

void generateBishopQuiets(moves_t *moveList, bb_t bishops, bb_t possibilities) {
  int piece = 4 + side;

  while (bishops) {
    int start = lsb(bishops);

    bb_t attacks = getBishopAttacks(start, occupancies[2]) & ~occupancies[2] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(bishops);
  }
}

void generateBishopMoves(moves_t *moveList, bb_t bishops, bb_t possibilities) {
  generateBishopCaptures(moveList, bishops, possibilities);
  generateBishopQuiets(moveList, bishops, possibilities);
}

void generateRookCaptures(moves_t *moveList, bb_t rooks, bb_t possibilities) {
  int piece = 6 + side;

  while (rooks) {
    int start = lsb(rooks);

    bb_t attacks = getRookAttacks(start, occupancies[2]) & occupancies[xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(rooks);
  }
}

void generateRookQuiets(moves_t *moveList, bb_t rooks, bb_t possibilities) {
  int piece = 6 + side;

  while (rooks) {
    int start = lsb(rooks);

    bb_t attacks = getRookAttacks(start, occupancies[2]) & ~occupancies[2] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(rooks);
  }
}

void generateRookMoves(moves_t *moveList, bb_t rooks, bb_t possibilities) {
  generateRookCaptures(moveList, rooks, possibilities);
  generateRookQuiets(moveList, rooks, possibilities);
}

void generateQueenCaptures(moves_t *moveList, bb_t queens, bb_t possibilities) {
  int piece = 8 + side;

  while (queens) {
    int start = lsb(queens);

    bb_t attacks = getQueenAttacks(start, occupancies[2]) & occupancies[xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(queens);
  }
}

void generateQueenQuiets(moves_t *moveList, bb_t queens, bb_t possibilities) {
  int piece = 8 + side;

  while (queens) {
    int start = lsb(queens);

    bb_t attacks = getQueenAttacks(start, occupancies[2]) & ~occupancies[2] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, piece, 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(queens);
  }
}

void generateQueenMoves(moves_t *moveList, bb_t queens, bb_t possibilities) {
  generateQueenCaptures(moveList, queens, possibilities);
  generateQueenQuiets(moveList, queens, possibilities);
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
    if ((castling & 0x8) && !(occupancies[2] & getInBetween(60, 63)))
      addMove(moveList, buildMove(60, 62, piece, 0, 0, 0, 0, 1));
    if ((castling & 0x4) && !(occupancies[2] & getInBetween(60, 56)))
      addMove(moveList, buildMove(60, 58, piece, 0, 0, 0, 0, 1));
  } else {
    if ((castling & 0x2) && !(occupancies[2] & getInBetween(4, 7)))
      addMove(moveList, buildMove(4, 6, piece, 0, 0, 0, 0, 1));
    if ((castling & 0x1) && !(occupancies[2] & getInBetween(4, 0)))
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
  int kingSq = lsb(pieces[10 + side]);

  if (bits(checkers) > 1) {
    generateKingMoves(moveList);
  } else if (checkers) {
    bb_t betweens = getInBetween(kingSq, lsb(checkers));

    bb_t nonPinned = ~pinned;
    generatePawnMoves(moveList, pieces[side] & nonPinned, betweens | checkers);
    generateKnightMoves(moveList, pieces[2 + side] & nonPinned, betweens | checkers);
    generateBishopMoves(moveList, pieces[4 + side] & nonPinned, betweens | checkers);
    generateRookMoves(moveList, pieces[6 + side] & nonPinned, betweens | checkers);
    generateQueenMoves(moveList, pieces[8 + side] & nonPinned, betweens | checkers);
    generateKingMoves(moveList);
  } else {
    bb_t nonPinned = ~pinned;
    generatePawnMoves(moveList, pieces[side] & nonPinned, -1ULL);
    generateKnightMoves(moveList, pieces[2 + side] & nonPinned, -1ULL);
    generateBishopMoves(moveList, pieces[4 + side] & nonPinned, -1ULL);
    generateRookMoves(moveList, pieces[6 + side] & nonPinned, -1ULL);
    generateQueenMoves(moveList, pieces[8 + side] & nonPinned, -1ULL);
    generateKingMoves(moveList);

    // TODO: Clean this up?
    bb_t pinnedPawns = pieces[side] & pinned;
    bb_t pinnedBishops = pieces[4 + side] & pinned;
    bb_t pinnedRooks = pieces[6 + side] & pinned;
    bb_t pinnedQueens = pieces[8 + side] & pinned;

    while (pinnedPawns) {
      int sq = lsb(pinnedPawns);
      generatePawnMoves(moveList, pinnedPawns & -pinnedPawns, getPinnedMoves(sq, kingSq));
      popLsb(pinnedPawns);
    }

    while (pinnedBishops) {
      int sq = lsb(pinnedBishops);
      generateBishopMoves(moveList, pinnedBishops & -pinnedBishops, getPinnedMoves(sq, kingSq));
      popLsb(pinnedBishops);
    }

    while (pinnedRooks) {
      int sq = lsb(pinnedRooks);
      generateRookMoves(moveList, pinnedRooks & -pinnedRooks, getPinnedMoves(sq, kingSq));
      popLsb(pinnedRooks);
    }

    while (pinnedQueens) {
      int sq = lsb(pinnedQueens);
      generateQueenMoves(moveList, pinnedQueens & -pinnedQueens, getPinnedMoves(sq, kingSq));
      popLsb(pinnedQueens);
    }
  }

  move_t *curr = moveList->moves;
  while (curr != moveList->moves + moveList->count) {
    if ((moveStart(*curr) == kingSq || moveEP(*curr)) && !isLegal(*curr))
      *curr = moveList->moves[--moveList->count];
    else
      ++curr;
  }
}

void printMoves(moves_t *moveList) {
  printf("move  p c d e t\n");

  for (int i = 0; i < moveList->count; i++) {
    move_t m = moveList->moves[i];
    printf("%s%s%c %c %d %d %d %d\n", idxToCord[moveStart(m)], idxToCord[moveEnd(m)], movePromo(m) ? pieceChars[movePromo(m)] : ' ', pieceChars[movePiece(m)], moveCapture(m),
           moveDouble(m), moveEP(m), moveCastle(m));
  }
}
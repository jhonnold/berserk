#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "movegen.h"
#include "types.h"

const char* promotionChars = "nbrq";
const int pawnDirections[] = {-8, 8};
const BitBoard middleFour = 281474976645120UL;
const BitBoard promotionRanks[] = {65280UL, 71776119061217280L};
const BitBoard homeRanks[] = {71776119061217280L, 65280UL};
const BitBoard thirdRanks[] = {280375465082880L, 16711680L};
const BitBoard filled = -1ULL;

inline void addMove(MoveList* moveList, Move move) { moveList->moves[moveList->count++] = move; }

void generatePawnPromotions(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard promotingPawns = pawns & promotionRanks[board->side];

  BitBoard quietPromoters =
      shift(promotingPawns, pawnDirections[board->side]) & ~board->occupancies[BOTH] & possibilities;
  BitBoard capturingPromotersE =
      shift(promotingPawns, pawnDirections[board->side] - 1) & board->occupancies[board->xside] & possibilities;
  BitBoard capturingPromotersW =
      shift(promotingPawns, pawnDirections[board->side] + 1) & board->occupancies[board->xside] & possibilities;

  while (quietPromoters) {
    int end = lsb(quietPromoters);
    int start = end - pawnDirections[board->side];

    addMove(moveList, buildMove(start, end, PAWN[board->side], QUEEN[board->side], 0, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], ROOK[board->side], 0, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], BISHOP[board->side], 0, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], KNIGHT[board->side], 0, 0, 0, 0));

    popLsb(quietPromoters);
  }

  while (capturingPromotersE) {
    int end = lsb(capturingPromotersE);
    int start = end - (pawnDirections[board->side] - 1);

    addMove(moveList, buildMove(start, end, PAWN[board->side], QUEEN[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], ROOK[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], BISHOP[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], KNIGHT[board->side], 1, 0, 0, 0));

    popLsb(capturingPromotersE);
  }

  while (capturingPromotersW) {
    int end = lsb(capturingPromotersW);
    int start = end - (pawnDirections[board->side] + 1);

    addMove(moveList, buildMove(start, end, PAWN[board->side], QUEEN[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], ROOK[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], BISHOP[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], KNIGHT[board->side], 1, 0, 0, 0));

    popLsb(capturingPromotersW);
  }
}

void generatePawnCaptures(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard nonPromotingPawns = pawns & ~promotionRanks[board->side];
  BitBoard capturingE =
      shift(nonPromotingPawns, pawnDirections[board->side] - 1) & board->occupancies[board->xside] & possibilities;
  BitBoard capturingW =
      shift(nonPromotingPawns, pawnDirections[board->side] + 1) & board->occupancies[board->xside] & possibilities;

  while (capturingE) {
    int end = lsb(capturingE);

    addMove(moveList, buildMove(end - (pawnDirections[board->side] - 1), end, PAWN[board->side], 0, 1, 0, 0, 0));

    popLsb(capturingE);
  }

  while (capturingW) {
    int end = lsb(capturingW);
    addMove(moveList, buildMove(end - (pawnDirections[board->side] + 1), end, PAWN[board->side], 0, 1, 0, 0, 0));
    popLsb(capturingW);
  }

  if (board->epSquare) {
    BitBoard epPawns = getPawnAttacks(board->epSquare, board->xside) & nonPromotingPawns;

    while (epPawns) {
      int start = lsb(epPawns);
      addMove(moveList, buildMove(start, board->epSquare, PAWN[board->side], 0, 1, 0, 1, 0));
      popLsb(epPawns);
    }
  }
}

void generatePawnQuiets(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard empty = ~board->occupancies[2];
  BitBoard nonPromotingPawns = pawns & ~promotionRanks[board->side];

  BitBoard singlePush = shift(nonPromotingPawns, pawnDirections[board->side]) & empty;
  BitBoard doublePush = shift(singlePush & thirdRanks[board->side], pawnDirections[board->side]) & empty;

  singlePush &= possibilities;
  doublePush &= possibilities;

  while (singlePush) {
    int end = lsb(singlePush);
    addMove(moveList, buildMove(end - pawnDirections[board->side], end, PAWN[board->side], 0, 0, 0, 0, 0));
    popLsb(singlePush);
  }

  while (doublePush) {
    int end = lsb(doublePush);
    addMove(moveList, buildMove(end - pawnDirections[board->side] - pawnDirections[board->side], end, PAWN[board->side],
                                0, 0, 1, 0, 0));
    popLsb(doublePush);
  }
}

void generatePawnMoves(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  generatePawnPromotions(moveList, pawns, possibilities, board);
  generatePawnCaptures(moveList, pawns, possibilities, board);
  generatePawnQuiets(moveList, pawns, possibilities, board);
}

void generateKnightCaptures(MoveList* moveList, BitBoard knights, BitBoard possibilities, Board* board) {
  while (knights) {
    int start = lsb(knights);

    BitBoard attacks = getKnightAttacks(start) & board->occupancies[board->xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, KNIGHT[board->side], 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(knights);
  }
}

void generateKnightQuiets(MoveList* moveList, BitBoard knights, BitBoard possibilities, Board* board) {
  while (knights) {
    int start = lsb(knights);

    BitBoard attacks = getKnightAttacks(start) & ~board->occupancies[BOTH] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, KNIGHT[board->side], 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(knights);
  }
}

void generateKnightMoves(MoveList* moveList, BitBoard knights, BitBoard possibilities, Board* board) {
  generateKnightCaptures(moveList, knights, possibilities, board);
  generateKnightQuiets(moveList, knights, possibilities, board);
}

void generateBishopCaptures(MoveList* moveList, BitBoard bishops, BitBoard possibilities, Board* board) {
  while (bishops) {
    int start = lsb(bishops);

    BitBoard attacks =
        getBishopAttacks(start, board->occupancies[BOTH]) & board->occupancies[board->xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, BISHOP[board->side], 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(bishops);
  }
}

void generateBishopQuiets(MoveList* moveList, BitBoard bishops, BitBoard possibilities, Board* board) {
  while (bishops) {
    int start = lsb(bishops);

    BitBoard attacks = getBishopAttacks(start, board->occupancies[BOTH]) & ~board->occupancies[BOTH] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, BISHOP[board->side], 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(bishops);
  }
}

void generateBishopMoves(MoveList* moveList, BitBoard bishops, BitBoard possibilities, Board* board) {
  generateBishopCaptures(moveList, bishops, possibilities, board);
  generateBishopQuiets(moveList, bishops, possibilities, board);
}

void generateRookCaptures(MoveList* moveList, BitBoard rooks, BitBoard possibilities, Board* board) {
  while (rooks) {
    int start = lsb(rooks);

    BitBoard attacks = getRookAttacks(start, board->occupancies[2]) & board->occupancies[board->xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, ROOK[board->side], 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(rooks);
  }
}

void generateRookQuiets(MoveList* moveList, BitBoard rooks, BitBoard possibilities, Board* board) {
  while (rooks) {
    int start = lsb(rooks);

    BitBoard attacks = getRookAttacks(start, board->occupancies[BOTH]) & ~board->occupancies[BOTH] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, ROOK[board->side], 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(rooks);
  }
}

void generateRookMoves(MoveList* moveList, BitBoard rooks, BitBoard possibilities, Board* board) {
  generateRookCaptures(moveList, rooks, possibilities, board);
  generateRookQuiets(moveList, rooks, possibilities, board);
}

void generateQueenCaptures(MoveList* moveList, BitBoard queens, BitBoard possibilities, Board* board) {
  while (queens) {
    int start = lsb(queens);

    BitBoard attacks =
        getQueenAttacks(start, board->occupancies[BOTH]) & board->occupancies[board->xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, QUEEN[board->side], 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(queens);
  }
}

void generateQueenQuiets(MoveList* moveList, BitBoard queens, BitBoard possibilities, Board* board) {
  while (queens) {
    int start = lsb(queens);

    BitBoard attacks = getQueenAttacks(start, board->occupancies[BOTH]) & ~board->occupancies[BOTH] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, QUEEN[board->side], 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(queens);
  }
}

void generateQueenMoves(MoveList* moveList, BitBoard queens, BitBoard possibilities, Board* board) {
  generateQueenCaptures(moveList, queens, possibilities, board);
  generateQueenQuiets(moveList, queens, possibilities, board);
}

void generateKingCaptures(MoveList* moveList, Board* board) {
  BitBoard kings = board->pieces[KING[board->side]];

  while (kings) {
    int start = lsb(kings);

    BitBoard attacks = getKingAttacks(start) & board->occupancies[board->xside];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, KING[board->side], 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(kings);
  }
}

void generateKingCastles(MoveList* moveList, Board* board) {
  if (inCheck(board))
    return;

  if (board->side == WHITE) {
    if ((board->castling & 0x8) && !(board->occupancies[BOTH] & getInBetween(60, 63)))
      addMove(moveList, buildMove(60, 62, KING[board->side], 0, 0, 0, 0, 1));
    if ((board->castling & 0x4) && !(board->occupancies[BOTH] & getInBetween(60, 56)))
      addMove(moveList, buildMove(60, 58, KING[board->side], 0, 0, 0, 0, 1));
  } else {
    if ((board->castling & 0x2) && !(board->occupancies[BOTH] & getInBetween(4, 7)))
      addMove(moveList, buildMove(4, 6, KING[board->side], 0, 0, 0, 0, 1));
    if ((board->castling & 0x1) && !(board->occupancies[BOTH] & getInBetween(4, 0)))
      addMove(moveList, buildMove(4, 2, KING[board->side], 0, 0, 0, 0, 1));
  }
}

void generateKingQuiets(MoveList* moveList, Board* board) {
  BitBoard kings = board->pieces[KING[board->side]];

  while (kings) {
    int start = lsb(kings);

    BitBoard attacks = getKingAttacks(start) & ~board->occupancies[BOTH];
    while (attacks) {
      int end = lsb(attacks);

      addMove(moveList, buildMove(start, end, KING[board->side], 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(kings);
  }
}

void generateKingMoves(MoveList* moveList, Board* board) {
  generateKingCaptures(moveList, board);
  generateKingCastles(moveList, board);
  generateKingQuiets(moveList, board);
}

void generateMoves(MoveList* moveList, Board* board) {
  moveList->count = 0;
  int kingSq = lsb(board->pieces[KING[board->side]]);

  if (bits(board->checkers) > 1) { // double check
    generateKingMoves(moveList, board);
  } else if (board->checkers) {
    BitBoard betweens = getInBetween(kingSq, lsb(board->checkers));

    BitBoard nonPinned = ~board->pinners;
    generatePawnMoves(moveList, board->pieces[PAWN[board->side]] & nonPinned, betweens | board->checkers, board);
    generateKnightMoves(moveList, board->pieces[KNIGHT[board->side]] & nonPinned, betweens | board->checkers, board);
    generateBishopMoves(moveList, board->pieces[BISHOP[board->side]] & nonPinned, betweens | board->checkers, board);
    generateRookMoves(moveList, board->pieces[ROOK[board->side]] & nonPinned, betweens | board->checkers, board);
    generateQueenMoves(moveList, board->pieces[QUEEN[board->side]] & nonPinned, betweens | board->checkers, board);
    generateKingMoves(moveList, board);
  } else {
    BitBoard nonPinned = ~board->pinners;
    generatePawnMoves(moveList, board->pieces[PAWN[board->side]] & nonPinned, -1ULL, board);
    generateKnightMoves(moveList, board->pieces[KNIGHT[board->side]] & nonPinned, -1ULL, board);
    generateBishopMoves(moveList, board->pieces[BISHOP[board->side]] & nonPinned, -1ULL, board);
    generateRookMoves(moveList, board->pieces[ROOK[board->side]] & nonPinned, -1ULL, board);
    generateQueenMoves(moveList, board->pieces[QUEEN[board->side]] & nonPinned, -1ULL, board);
    generateKingMoves(moveList, board);

    // TODO: Clean this up?
    // Could implement like stockfish and eliminate illegal pinned moves after the fact
    BitBoard pinnedPawns = board->pieces[PAWN[board->side]] & board->pinners;
    BitBoard pinnedBishops = board->pieces[BISHOP[board->side]] & board->pinners;
    BitBoard pinnedRooks = board->pieces[ROOK[board->side]] & board->pinners;
    BitBoard pinnedQueens = board->pieces[QUEEN[board->side]] & board->pinners;

    while (pinnedPawns) {
      int sq = lsb(pinnedPawns);
      generatePawnMoves(moveList, pinnedPawns & -pinnedPawns, getPinnedMoves(sq, kingSq), board);
      popLsb(pinnedPawns);
    }

    while (pinnedBishops) {
      int sq = lsb(pinnedBishops);
      generateBishopMoves(moveList, pinnedBishops & -pinnedBishops, getPinnedMoves(sq, kingSq), board);
      popLsb(pinnedBishops);
    }

    while (pinnedRooks) {
      int sq = lsb(pinnedRooks);
      generateRookMoves(moveList, pinnedRooks & -pinnedRooks, getPinnedMoves(sq, kingSq), board);
      popLsb(pinnedRooks);
    }

    while (pinnedQueens) {
      int sq = lsb(pinnedQueens);
      generateQueenMoves(moveList, pinnedQueens & -pinnedQueens, getPinnedMoves(sq, kingSq), board);
      popLsb(pinnedQueens);
    }
  }

  Move* curr = moveList->moves;
  while (curr != moveList->moves + moveList->count) {
    if ((moveStart(*curr) == kingSq || moveEP(*curr)) && !isLegal(*curr, board))
      *curr = moveList->moves[--moveList->count];
    else
      ++curr;
  }
}

void printMoves(MoveList* moveList) {
  printf("move  p c d e t\n");

  for (int i = 0; i < moveList->count; i++) {
    Move move = moveList->moves[i];
    printf("%s%s%c %c %d %d %d %d\n", idxToCord[moveStart(move)], idxToCord[moveEnd(move)],
           movePromo(move) ? pieceChars[movePromo(move)] : ' ', pieceChars[movePiece(move)], moveCapture(move),
           moveDouble(move), moveEP(move), moveCastle(move));
  }
}

Move parseMove(char* moveStr, Board* board) {
  MoveList moveList[1];
  generateMoves(moveList, board);

  int start = (moveStr[0] - 'a') + (8 - (moveStr[1] - '0')) * 8;
  int end = (moveStr[2] - 'a') + (8 - (moveStr[3] - '0')) * 8;

  for (int i = 0; i < moveList->count; i++) {
    Move match = moveList->moves[i];
    if (start != moveStart(match) || end != moveEnd(match))
      continue;

    int promotedPiece = movePromo(match);
    if (!promotedPiece)
      return match;

    if (promotionChars[promotedPiece >> 1] == moveStr[4])
      return match;
  }

  return 0;
}
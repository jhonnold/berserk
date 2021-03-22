#include <assert.h>
#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "see.h"
#include "transposition.h"
#include "types.h"
#include "util.h"

const int HASH = INT32_MAX;
const int GOOD_CAPTURE = INT32_MAX - INT16_MAX;
const int BAD_CAPTURE = -INT32_MAX + 2 * INT16_MAX;
const int KILLER1 = INT32_MAX - 2 * INT16_MAX;
const int KILLER2 = INT32_MAX - 2 * INT16_MAX - 1;
const int COUNTER = INT32_MAX - 2 * INT16_MAX - 10;

const int MVV_LVA[12][12] = {{105, 105, 205, 205, 305, 305, 405, 405, 505, 505, 605, 605},
                             {105, 105, 205, 205, 305, 305, 405, 405, 505, 505, 605, 605},
                             {104, 104, 204, 204, 304, 304, 404, 404, 504, 504, 604, 604},
                             {104, 104, 204, 204, 304, 304, 404, 404, 504, 504, 604, 604},
                             {103, 103, 203, 203, 303, 303, 403, 403, 503, 503, 603, 603},
                             {103, 103, 203, 203, 303, 303, 403, 403, 503, 503, 603, 603},
                             {102, 102, 202, 202, 302, 302, 402, 402, 502, 502, 602, 602},
                             {102, 102, 202, 202, 302, 302, 402, 402, 502, 502, 602, 602},
                             {101, 101, 201, 201, 301, 301, 401, 401, 501, 501, 601, 601},
                             {101, 101, 201, 201, 301, 301, 401, 401, 501, 501, 601, 601},
                             {100, 100, 200, 200, 300, 300, 400, 400, 500, 500, 600, 600},
                             {100, 100, 200, 200, 300, 300, 400, 400, 500, 500, 600, 600}};

const int PAWN_DIRECTIONS[] = {N, S};
const BitBoard PROMOTION_RANKS[] = {RANK_7, RANK_2};
const BitBoard HOME_RANKS[] = {RANK_2, RANK_7};
const BitBoard THIRD_RANKS[] = {RANK_3, RANK_6};
const BitBoard FILLED = -1ULL;

inline void addKiller(SearchData* data, Move move) {
  if (data->killers[data->ply][0] == move)
    data->killers[data->ply][1] = data->killers[data->ply][0];
  data->killers[data->ply][0] = move;
}

inline void addCounter(SearchData* data, Move move, Move parent) { data->counters[moveSE(parent)] = move; }

inline void addHistoryHeuristic(SearchData* data, Move move, int depth) {
  data->hh[data->board->side][moveSE(move)] += depth * depth;
}

inline void addBFHeuristic(SearchData* data, Move move, int depth) {
  data->bf[data->board->side][moveSE(move)] += depth * depth;
}

inline void addMove(MoveList* moveList, Move move) { moveList->moves[moveList->count++] = move; }

void generatePawnPromotions(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard promotingPawns = pawns & PROMOTION_RANKS[board->side];

  BitBoard quietPromoters =
      shift(promotingPawns, PAWN_DIRECTIONS[board->side]) & ~board->occupancies[BOTH] & possibilities;
  BitBoard capturingPromotersW =
      shift(promotingPawns, PAWN_DIRECTIONS[board->side] + W) & board->occupancies[board->xside] & possibilities;
  BitBoard capturingPromotersE =
      shift(promotingPawns, PAWN_DIRECTIONS[board->side] + E) & board->occupancies[board->xside] & possibilities;

  while (quietPromoters) {
    int end = lsb(quietPromoters);
    int start = end - PAWN_DIRECTIONS[board->side];

    addMove(moveList, buildMove(start, end, PAWN[board->side], QUEEN[board->side], 0, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], ROOK[board->side], 0, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], BISHOP[board->side], 0, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], KNIGHT[board->side], 0, 0, 0, 0));

    popLsb(quietPromoters);
  }

  while (capturingPromotersE) {
    int end = lsb(capturingPromotersE);
    int start = end - (PAWN_DIRECTIONS[board->side] + E);

    addMove(moveList, buildMove(start, end, PAWN[board->side], QUEEN[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], ROOK[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], BISHOP[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], KNIGHT[board->side], 1, 0, 0, 0));

    popLsb(capturingPromotersE);
  }

  while (capturingPromotersW) {
    int end = lsb(capturingPromotersW);
    int start = end - (PAWN_DIRECTIONS[board->side] + W);

    addMove(moveList, buildMove(start, end, PAWN[board->side], QUEEN[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], ROOK[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], BISHOP[board->side], 1, 0, 0, 0));
    addMove(moveList, buildMove(start, end, PAWN[board->side], KNIGHT[board->side], 1, 0, 0, 0));

    popLsb(capturingPromotersW);
  }
}

void generatePawnCaptures(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard nonPromotingPawns = pawns & ~PROMOTION_RANKS[board->side];
  BitBoard capturingW =
      shift(nonPromotingPawns, PAWN_DIRECTIONS[board->side] + W) & board->occupancies[board->xside] & possibilities;
  BitBoard capturingE =
      shift(nonPromotingPawns, PAWN_DIRECTIONS[board->side] + E) & board->occupancies[board->xside] & possibilities;

  while (capturingE) {
    int end = lsb(capturingE);

    addMove(moveList, buildMove(end - (PAWN_DIRECTIONS[board->side] + E), end, PAWN[board->side], 0, 1, 0, 0, 0));

    popLsb(capturingE);
  }

  while (capturingW) {
    int end = lsb(capturingW);
    addMove(moveList, buildMove(end - (PAWN_DIRECTIONS[board->side] + W), end, PAWN[board->side], 0, 1, 0, 0, 0));
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
  BitBoard empty = ~board->occupancies[BOTH];
  BitBoard nonPromotingPawns = pawns & ~PROMOTION_RANKS[board->side];

  BitBoard singlePush = shift(nonPromotingPawns, PAWN_DIRECTIONS[board->side]) & empty;
  BitBoard doublePush = shift(singlePush & THIRD_RANKS[board->side], PAWN_DIRECTIONS[board->side]) & empty;

  singlePush &= possibilities;
  doublePush &= possibilities;

  while (singlePush) {
    int end = lsb(singlePush);
    addMove(moveList, buildMove(end - PAWN_DIRECTIONS[board->side], end, PAWN[board->side], 0, 0, 0, 0, 0));
    popLsb(singlePush);
  }

  while (doublePush) {
    int end = lsb(doublePush);
    addMove(moveList, buildMove(end - PAWN_DIRECTIONS[board->side] - PAWN_DIRECTIONS[board->side], end,
                                PAWN[board->side], 0, 0, 1, 0, 0));
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

    BitBoard attacks =
        getRookAttacks(start, board->occupancies[BOTH]) & board->occupancies[board->xside] & possibilities;
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
  if (board->checkers)
    return;

  if (board->side == WHITE) {
    if ((board->castling & 0x8) && !(board->occupancies[BOTH] & getInBetween(E1, H1)))
      addMove(moveList, buildMove(E1, G1, KING[board->side], 0, 0, 0, 0, 1));
    if ((board->castling & 0x4) && !(board->occupancies[BOTH] & getInBetween(E1, A1)))
      addMove(moveList, buildMove(E1, C1, KING[board->side], 0, 0, 0, 0, 1));
  } else {
    if ((board->castling & 0x2) && !(board->occupancies[BOTH] & getInBetween(E8, H8)))
      addMove(moveList, buildMove(E8, G8, KING[board->side], 0, 0, 0, 0, 1));
    if ((board->castling & 0x1) && !(board->occupancies[BOTH] & getInBetween(E8, A8)))
      addMove(moveList, buildMove(E8, C8, KING[board->side], 0, 0, 0, 0, 1));
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

// captures and promotions
void generateQuiesceMoves(MoveList* moveList, SearchData* data) {
  moveList->count = 0;

  Board* board = data->board;
  int kingSq = lsb(board->pieces[KING[board->side]]);

  if (bits(board->checkers) > 1) {
    generateKingCaptures(moveList, board);
  } else if (board->checkers) {
    BitBoard nonPinned = ~board->pinners;
    generatePawnCaptures(moveList, board->pieces[PAWN[board->side]] & nonPinned, board->checkers, board);
    generatePawnPromotions(moveList, board->pieces[PAWN[board->side]] & nonPinned, board->checkers, board);
    generateKnightCaptures(moveList, board->pieces[KNIGHT[board->side]] & nonPinned, board->checkers, board);
    generateBishopCaptures(moveList, board->pieces[BISHOP[board->side]] & nonPinned, board->checkers, board);
    generateRookCaptures(moveList, board->pieces[ROOK[board->side]] & nonPinned, board->checkers, board);
    generateQueenCaptures(moveList, board->pieces[QUEEN[board->side]] & nonPinned, board->checkers, board);
    generateKingCaptures(moveList, board);
  } else {
    BitBoard nonPinned = ~board->pinners;
    generatePawnCaptures(moveList, board->pieces[PAWN[board->side]] & nonPinned, FILLED, board);
    generatePawnPromotions(moveList, board->pieces[PAWN[board->side]] & nonPinned, FILLED, board);
    generateKnightCaptures(moveList, board->pieces[KNIGHT[board->side]] & nonPinned, FILLED, board);
    generateBishopCaptures(moveList, board->pieces[BISHOP[board->side]] & nonPinned, FILLED, board);
    generateRookCaptures(moveList, board->pieces[ROOK[board->side]] & nonPinned, FILLED, board);
    generateQueenCaptures(moveList, board->pieces[QUEEN[board->side]] & nonPinned, FILLED, board);
    generateKingCaptures(moveList, board);

    // TODO: Clean this up?
    // Could implement like stockfish and eliminate illegal pinned moves after the fact
    BitBoard pinnedPawns = board->pieces[PAWN[board->side]] & board->pinners;
    BitBoard pinnedBishops = board->pieces[BISHOP[board->side]] & board->pinners;
    BitBoard pinnedRooks = board->pieces[ROOK[board->side]] & board->pinners;
    BitBoard pinnedQueens = board->pieces[QUEEN[board->side]] & board->pinners;

    while (pinnedPawns) {
      int sq = lsb(pinnedPawns);
      generatePawnCaptures(moveList, pinnedPawns & -pinnedPawns, getPinnedMoves(sq, kingSq), board);
      generatePawnPromotions(moveList, pinnedPawns & -pinnedPawns, getPinnedMoves(sq, kingSq), board);
      popLsb(pinnedPawns);
    }

    while (pinnedBishops) {
      int sq = lsb(pinnedBishops);
      generateBishopCaptures(moveList, pinnedBishops & -pinnedBishops, getPinnedMoves(sq, kingSq), board);
      popLsb(pinnedBishops);
    }

    while (pinnedRooks) {
      int sq = lsb(pinnedRooks);
      generateRookCaptures(moveList, pinnedRooks & -pinnedRooks, getPinnedMoves(sq, kingSq), board);
      popLsb(pinnedRooks);
    }

    while (pinnedQueens) {
      int sq = lsb(pinnedQueens);
      generateQueenCaptures(moveList, pinnedQueens & -pinnedQueens, getPinnedMoves(sq, kingSq), board);
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

  for (int i = 0; i < moveList->count; i++) {
    Move move = moveList->moves[i];

    if (movePromo(move)) {
      moveList->scores[i] = scoreMG(MATERIAL_VALUES[movePromo(move)]);
    } else if (moveEP(move)) {
      moveList->scores[i] = MVV_LVA[PAWN[board->side]][PAWN[board->xside]];
    } else {
      int mover = movePiece(move);
      int captured = board->squares[moveEnd(move)];

      assert(captured != NO_PIECE);
      assert(mover != NO_PIECE);

      int seeScore = 0;
      if ((PIECE_TYPE[captured] <= PIECE_TYPE[mover]) && (seeScore = see(board, move)) < 0) {
        moveList->scores[i] = seeScore;
      } else {
        moveList->scores[i] = MVV_LVA[mover][captured];
      }
    }
  }
}

void generateMoves(MoveList* moveList, SearchData* data) {
  moveList->count = 0;

  Board* board = data->board;
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
    generatePawnMoves(moveList, board->pieces[PAWN[board->side]] & nonPinned, FILLED, board);
    generateKnightMoves(moveList, board->pieces[KNIGHT[board->side]] & nonPinned, FILLED, board);
    generateBishopMoves(moveList, board->pieces[BISHOP[board->side]] & nonPinned, FILLED, board);
    generateRookMoves(moveList, board->pieces[ROOK[board->side]] & nonPinned, FILLED, board);
    generateQueenMoves(moveList, board->pieces[QUEEN[board->side]] & nonPinned, FILLED, board);
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

  // TODO: this seems inefficient
  TTValue ttValue = ttProbe(board->zobrist);
  Move hashMove = ttMove(ttValue);
  for (int i = 0; i < moveList->count; i++) {
    Move move = moveList->moves[i];

    if (move == hashMove) {
      moveList->scores[i] = HASH;
    } else if (moveEP(move)) {
      moveList->scores[i] = GOOD_CAPTURE + MVV_LVA[PAWN[board->side]][PAWN[board->xside]];
    } else if (moveCapture(move)) {
      int mover = movePiece(move);
      int captured = board->squares[moveEnd(move)];

      assert(captured != NO_PIECE);
      assert(mover != NO_PIECE);

      int seeScore = 0;
      if ((PIECE_TYPE[captured] <= PIECE_TYPE[mover]) && (seeScore = see(board, move)) < 0) {
        moveList->scores[i] = BAD_CAPTURE + seeScore;
      } else {
        moveList->scores[i] = GOOD_CAPTURE + MVV_LVA[mover][captured];
      }
    } else if (movePromo(move) >= 8) {
      moveList->scores[i] = GOOD_CAPTURE + scoreMG(QUEEN_VALUE);
    } else if (move == data->killers[data->ply][0]) {
      moveList->scores[i] = KILLER1;
    } else if (move == data->killers[data->ply][1]) {
      moveList->scores[i] = KILLER2;
    } else if (data->ply && move == data->counters[moveSE(data->moves[data->ply - 1])]) {
      moveList->scores[i] = COUNTER;
    } else {
      moveList->scores[i] = 100 * data->hh[board->side][moveSE(move)] / max(1, data->bf[board->side][moveSE(move)]);
    }
  }
}

void bubbleTopMove(MoveList* moveList, int from) {
  int max = moveList->scores[from];
  int idx = from;

  for (int i = from + 1; i < moveList->count; i++) {
    if (moveList->scores[i] > max) {
      idx = i;
      max = moveList->scores[i];
    }
  }

  // We don't have to swap
  if (idx != from) {
    Move temp = moveList->moves[from];
    moveList->moves[from] = moveList->moves[idx];
    moveList->moves[idx] = temp;

    temp = moveList->scores[from];
    moveList->scores[from] = moveList->scores[idx];
    moveList->scores[idx] = temp;
  }
}

void printMoves(MoveList* moveList) {
  printf("move  score p c d e t\n");

  for (int i = 0; i < moveList->count; i++) {
    bubbleTopMove(moveList, i);
    Move move = moveList->moves[i];
    printf("%s %5d %c %d %d %d %d\n", moveStr(move), moveList->scores[i], PIECE_TO_CHAR[movePiece(move)],
           moveCapture(move), moveDouble(move), moveEP(move), moveCastle(move));
  }
}
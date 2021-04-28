// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

// Currently Berserk does not have phased move generation
// rather moves are sorted into groups via wide gaps in scores
const int HASH_MOVE_SCORE = INT32_MAX;
const int GOOD_CAPTURE_SCORE = INT32_MAX - INT16_MAX;
const int BAD_CAPTURE_SCORE = -INT32_MAX + 2 * INT16_MAX;
const int KILLER1_SCORE = INT32_MAX - 2 * INT16_MAX;
const int KILLER2_SCORE = INT32_MAX - 2 * INT16_MAX - 1;
const int COUNTER_SCORE = INT32_MAX - 2 * INT16_MAX - 10;

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

inline void AddKillerMove(SearchData* data, Move move) {
  // Prevent duplicate
  if (data->killers[data->ply][0] == move)
    data->killers[data->ply][1] = data->killers[data->ply][0];

  data->killers[data->ply][0] = move;
}

inline void AddCounterMove(SearchData* data, Move move, Move parent) { data->counters[MoveStartEnd(parent)] = move; }

inline void AddHistoryHeuristic(SearchData* data, Move move, int sideToMove, int depth) {
  data->hh[sideToMove][MoveStartEnd(move)] += depth * depth;
}

inline void AddBFHeuristic(SearchData* data, Move move, int sideToMove, int depth) {
  data->bf[sideToMove][MoveStartEnd(move)] += depth * depth;
}

inline void AppendMove(MoveList* moveList, Move move) { moveList->moves[moveList->count++] = move; }

// Move generation is pretty similar across all piece types with captures and quiets.
// Both receieve a BitBoard of acceptable squares, and additional logic is applied within
// each method to determine quiet vs capture.
// Promotions and Castles have specific methods

void GeneratePawnPromotions(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard promotingPawns = pawns & PROMOTION_RANKS[board->side];

  BitBoard quietPromoters =
      Shift(promotingPawns, PAWN_DIRECTIONS[board->side]) & ~board->occupancies[BOTH] & possibilities;
  BitBoard capturingPromotersW =
      Shift(promotingPawns, PAWN_DIRECTIONS[board->side] + W) & board->occupancies[board->xside] & possibilities;
  BitBoard capturingPromotersE =
      Shift(promotingPawns, PAWN_DIRECTIONS[board->side] + E) & board->occupancies[board->xside] & possibilities;

  while (quietPromoters) {
    int end = lsb(quietPromoters);
    int start = end - PAWN_DIRECTIONS[board->side];

    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], QUEEN[board->side], 0, 0, 0, 0));
    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], ROOK[board->side], 0, 0, 0, 0));
    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], BISHOP[board->side], 0, 0, 0, 0));
    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], KNIGHT[board->side], 0, 0, 0, 0));

    popLsb(quietPromoters);
  }

  while (capturingPromotersE) {
    int end = lsb(capturingPromotersE);
    int start = end - (PAWN_DIRECTIONS[board->side] + E);

    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], QUEEN[board->side], 1, 0, 0, 0));
    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], ROOK[board->side], 1, 0, 0, 0));
    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], BISHOP[board->side], 1, 0, 0, 0));
    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], KNIGHT[board->side], 1, 0, 0, 0));

    popLsb(capturingPromotersE);
  }

  while (capturingPromotersW) {
    int end = lsb(capturingPromotersW);
    int start = end - (PAWN_DIRECTIONS[board->side] + W);

    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], QUEEN[board->side], 1, 0, 0, 0));
    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], ROOK[board->side], 1, 0, 0, 0));
    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], BISHOP[board->side], 1, 0, 0, 0));
    AppendMove(moveList, BuildMove(start, end, PAWN[board->side], KNIGHT[board->side], 1, 0, 0, 0));

    popLsb(capturingPromotersW);
  }
}

void GeneratePawnCaptures(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard nonPromotingPawns = pawns & ~PROMOTION_RANKS[board->side];
  BitBoard capturingW =
      Shift(nonPromotingPawns, PAWN_DIRECTIONS[board->side] + W) & board->occupancies[board->xside] & possibilities;
  BitBoard capturingE =
      Shift(nonPromotingPawns, PAWN_DIRECTIONS[board->side] + E) & board->occupancies[board->xside] & possibilities;

  while (capturingE) {
    int end = lsb(capturingE);

    AppendMove(moveList, BuildMove(end - (PAWN_DIRECTIONS[board->side] + E), end, PAWN[board->side], 0, 1, 0, 0, 0));

    popLsb(capturingE);
  }

  while (capturingW) {
    int end = lsb(capturingW);
    AppendMove(moveList, BuildMove(end - (PAWN_DIRECTIONS[board->side] + W), end, PAWN[board->side], 0, 1, 0, 0, 0));
    popLsb(capturingW);
  }

  if (board->epSquare) {
    BitBoard epPawns = GetPawnAttacks(board->epSquare, board->xside) & nonPromotingPawns;

    while (epPawns) {
      int start = lsb(epPawns);
      AppendMove(moveList, BuildMove(start, board->epSquare, PAWN[board->side], 0, 1, 0, 1, 0));
      popLsb(epPawns);
    }
  }
}

void GeneratePawnQuiets(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard empty = ~board->occupancies[BOTH];
  BitBoard nonPromotingPawns = pawns & ~PROMOTION_RANKS[board->side];

  BitBoard singlePush = Shift(nonPromotingPawns, PAWN_DIRECTIONS[board->side]) & empty;
  BitBoard doublePush = Shift(singlePush & THIRD_RANKS[board->side], PAWN_DIRECTIONS[board->side]) & empty;

  singlePush &= possibilities;
  doublePush &= possibilities;

  while (singlePush) {
    int end = lsb(singlePush);
    AppendMove(moveList, BuildMove(end - PAWN_DIRECTIONS[board->side], end, PAWN[board->side], 0, 0, 0, 0, 0));
    popLsb(singlePush);
  }

  while (doublePush) {
    int end = lsb(doublePush);
    AppendMove(moveList, BuildMove(end - PAWN_DIRECTIONS[board->side] - PAWN_DIRECTIONS[board->side], end,
                                   PAWN[board->side], 0, 0, 1, 0, 0));
    popLsb(doublePush);
  }
}

void GenerateAllPawnMoves(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  GeneratePawnPromotions(moveList, pawns, possibilities, board);
  GeneratePawnCaptures(moveList, pawns, possibilities, board);
  GeneratePawnQuiets(moveList, pawns, possibilities, board);
}

void GenerateKnightCaptures(MoveList* moveList, BitBoard knights, BitBoard possibilities, Board* board) {
  while (knights) {
    int start = lsb(knights);

    BitBoard attacks = GetKnightAttacks(start) & board->occupancies[board->xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      AppendMove(moveList, BuildMove(start, end, KNIGHT[board->side], 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(knights);
  }
}

void GenerateKnightQuiets(MoveList* moveList, BitBoard knights, BitBoard possibilities, Board* board) {
  while (knights) {
    int start = lsb(knights);

    BitBoard attacks = GetKnightAttacks(start) & ~board->occupancies[BOTH] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      AppendMove(moveList, BuildMove(start, end, KNIGHT[board->side], 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(knights);
  }
}

void GenerateAllKnightMoves(MoveList* moveList, BitBoard knights, BitBoard possibilities, Board* board) {
  GenerateKnightCaptures(moveList, knights, possibilities, board);
  GenerateKnightQuiets(moveList, knights, possibilities, board);
}

void generateBishopCaptures(MoveList* moveList, BitBoard bishops, BitBoard possibilities, Board* board) {
  while (bishops) {
    int start = lsb(bishops);

    BitBoard attacks =
        GetBishopAttacks(start, board->occupancies[BOTH]) & board->occupancies[board->xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      AppendMove(moveList, BuildMove(start, end, BISHOP[board->side], 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(bishops);
  }
}

void GenerateBishopQuiets(MoveList* moveList, BitBoard bishops, BitBoard possibilities, Board* board) {
  while (bishops) {
    int start = lsb(bishops);

    BitBoard attacks = GetBishopAttacks(start, board->occupancies[BOTH]) & ~board->occupancies[BOTH] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      AppendMove(moveList, BuildMove(start, end, BISHOP[board->side], 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(bishops);
  }
}

void GenerateAllBishopMoves(MoveList* moveList, BitBoard bishops, BitBoard possibilities, Board* board) {
  generateBishopCaptures(moveList, bishops, possibilities, board);
  GenerateBishopQuiets(moveList, bishops, possibilities, board);
}

void generateRookCaptures(MoveList* moveList, BitBoard rooks, BitBoard possibilities, Board* board) {
  while (rooks) {
    int start = lsb(rooks);

    BitBoard attacks =
        GetRookAttacks(start, board->occupancies[BOTH]) & board->occupancies[board->xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      AppendMove(moveList, BuildMove(start, end, ROOK[board->side], 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(rooks);
  }
}

void GenerateRookQuiets(MoveList* moveList, BitBoard rooks, BitBoard possibilities, Board* board) {
  while (rooks) {
    int start = lsb(rooks);

    BitBoard attacks = GetRookAttacks(start, board->occupancies[BOTH]) & ~board->occupancies[BOTH] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      AppendMove(moveList, BuildMove(start, end, ROOK[board->side], 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(rooks);
  }
}

void GnerateAllRookMoves(MoveList* moveList, BitBoard rooks, BitBoard possibilities, Board* board) {
  generateRookCaptures(moveList, rooks, possibilities, board);
  GenerateRookQuiets(moveList, rooks, possibilities, board);
}

void GenerateQueenCaptures(MoveList* moveList, BitBoard queens, BitBoard possibilities, Board* board) {
  while (queens) {
    int start = lsb(queens);

    BitBoard attacks =
        GetQueenAttacks(start, board->occupancies[BOTH]) & board->occupancies[board->xside] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      AppendMove(moveList, BuildMove(start, end, QUEEN[board->side], 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(queens);
  }
}

void GenerateQueenQuiets(MoveList* moveList, BitBoard queens, BitBoard possibilities, Board* board) {
  while (queens) {
    int start = lsb(queens);

    BitBoard attacks = GetQueenAttacks(start, board->occupancies[BOTH]) & ~board->occupancies[BOTH] & possibilities;
    while (attacks) {
      int end = lsb(attacks);

      AppendMove(moveList, BuildMove(start, end, QUEEN[board->side], 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(queens);
  }
}

void GenerateAllQueenMoves(MoveList* moveList, BitBoard queens, BitBoard possibilities, Board* board) {
  GenerateQueenCaptures(moveList, queens, possibilities, board);
  GenerateQueenQuiets(moveList, queens, possibilities, board);
}

void GenerateKingCaptures(MoveList* moveList, Board* board) {
  BitBoard kings = board->pieces[KING[board->side]];

  while (kings) {
    int start = lsb(kings);

    BitBoard attacks = GetKingAttacks(start) & board->occupancies[board->xside];
    while (attacks) {
      int end = lsb(attacks);

      AppendMove(moveList, BuildMove(start, end, KING[board->side], 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(kings);
  }
}

void GenerateCastles(MoveList* moveList, Board* board) {
  if (board->checkers) // can't castle in check
    return;

  // logic for each castle is hardcoded (only 4 cases)
  // validate it's still possible, and that nothing is in the way
  // square attack logic is applied later
  if (board->side == WHITE) {
    if ((board->castling & 0x8) && !(board->occupancies[BOTH] & GetInBetweenSquares(E1, H1)))
      AppendMove(moveList, BuildMove(E1, G1, KING[board->side], 0, 0, 0, 0, 1));
    if ((board->castling & 0x4) && !(board->occupancies[BOTH] & GetInBetweenSquares(E1, A1)))
      AppendMove(moveList, BuildMove(E1, C1, KING[board->side], 0, 0, 0, 0, 1));
  } else {
    if ((board->castling & 0x2) && !(board->occupancies[BOTH] & GetInBetweenSquares(E8, H8)))
      AppendMove(moveList, BuildMove(E8, G8, KING[board->side], 0, 0, 0, 0, 1));
    if ((board->castling & 0x1) && !(board->occupancies[BOTH] & GetInBetweenSquares(E8, A8)))
      AppendMove(moveList, BuildMove(E8, C8, KING[board->side], 0, 0, 0, 0, 1));
  }
}

void GenerateKingQuiets(MoveList* moveList, Board* board) {
  BitBoard kings = board->pieces[KING[board->side]];

  while (kings) {
    int start = lsb(kings);

    BitBoard attacks = GetKingAttacks(start) & ~board->occupancies[BOTH];
    while (attacks) {
      int end = lsb(attacks);

      AppendMove(moveList, BuildMove(start, end, KING[board->side], 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(kings);
  }
}

void GenerateAllKingMoves(MoveList* moveList, Board* board) {
  GenerateKingCaptures(moveList, board);
  GenerateCastles(moveList, board);
  GenerateKingQuiets(moveList, board);
}

// captures and promotions
void GenerateTacticalMoves(MoveList* moveList, Board* board) {
  moveList->count = 0;

  int kingSq = lsb(board->pieces[KING[board->side]]);

  if (bits(board->checkers) > 1) {
    // double check means only king moves are possible
    GenerateKingCaptures(moveList, board);
  } else if (board->checkers) {
    // while in check, only tactical moves to evade are captures
    // pinned pieces can NEVER be the piece to evade a check with a capture, so
    // those are filtered out
    BitBoard nonPinned = ~board->pinned;

    GeneratePawnCaptures(moveList, board->pieces[PAWN[board->side]] & nonPinned, board->checkers, board);
    GeneratePawnPromotions(moveList, board->pieces[PAWN[board->side]] & nonPinned, board->checkers, board);
    GenerateKnightCaptures(moveList, board->pieces[KNIGHT[board->side]] & nonPinned, board->checkers, board);
    generateBishopCaptures(moveList, board->pieces[BISHOP[board->side]] & nonPinned, board->checkers, board);
    generateRookCaptures(moveList, board->pieces[ROOK[board->side]] & nonPinned, board->checkers, board);
    GenerateQueenCaptures(moveList, board->pieces[QUEEN[board->side]] & nonPinned, board->checkers, board);
    GenerateKingCaptures(moveList, board);
  } else {
    // generate moves in two stages, non pinned piece moves, then pinned piece moves

    // all non-pinned captures
    BitBoard nonPinned = ~board->pinned;
    GeneratePawnCaptures(moveList, board->pieces[PAWN[board->side]] & nonPinned, FILLED, board);
    GeneratePawnPromotions(moveList, board->pieces[PAWN[board->side]] & nonPinned, FILLED, board);
    GenerateKnightCaptures(moveList, board->pieces[KNIGHT[board->side]] & nonPinned, FILLED, board);
    generateBishopCaptures(moveList, board->pieces[BISHOP[board->side]] & nonPinned, FILLED, board);
    generateRookCaptures(moveList, board->pieces[ROOK[board->side]] & nonPinned, FILLED, board);
    GenerateQueenCaptures(moveList, board->pieces[QUEEN[board->side]] & nonPinned, FILLED, board);
    GenerateKingCaptures(moveList, board);

    // get the pinned pieces and generate their moves
    // knights when pinned cannot move
    BitBoard pinnedPawns = board->pieces[PAWN[board->side]] & board->pinned;
    BitBoard pinnedBishops = board->pieces[BISHOP[board->side]] & board->pinned;
    BitBoard pinnedRooks = board->pieces[ROOK[board->side]] & board->pinned;
    BitBoard pinnedQueens = board->pieces[QUEEN[board->side]] & board->pinned;

    while (pinnedPawns) {
      int sq = lsb(pinnedPawns);
      GeneratePawnCaptures(moveList, pinnedPawns & -pinnedPawns, GetPinnedMovementSquares(sq, kingSq), board);
      GeneratePawnPromotions(moveList, pinnedPawns & -pinnedPawns, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedPawns);
    }

    while (pinnedBishops) {
      int sq = lsb(pinnedBishops);
      generateBishopCaptures(moveList, pinnedBishops & -pinnedBishops, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedBishops);
    }

    while (pinnedRooks) {
      int sq = lsb(pinnedRooks);
      generateRookCaptures(moveList, pinnedRooks & -pinnedRooks, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedRooks);
    }

    while (pinnedQueens) {
      int sq = lsb(pinnedQueens);
      GenerateQueenCaptures(moveList, pinnedQueens & -pinnedQueens, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedQueens);
    }
  }

  // this is the final legality check for moves - certain move types are specifically checked here
  // king moves, castles, and EP (some crazy pins)
  Move* curr = moveList->moves;
  while (curr != moveList->moves + moveList->count) {
    if ((MoveStart(*curr) == kingSq || MoveEP(*curr)) && !IsMoveLegal(*curr, board))
      *curr = moveList->moves[--moveList->count]; // overwrite this illegal move with the last move and try again
    else if (MovePromo(*curr) && MovePromo(*curr) < QUEEN_WHITE)
      *curr = moveList->moves[--moveList->count]; // filter out under promotions
    else
      ++curr;
  }

  // for tactical moves just SEE is utilized for ordering
  for (int i = 0; i < moveList->count; i++) {
    Move move = moveList->moves[i];

    int see = SEE(board, move);
    if (MoveEP(move))
      see += STATIC_MATERIAL_VALUE[PAWN_TYPE];
    
    moveList->scores[i] = see;
  }
}

void GenerateAllMoves(MoveList* moveList, Board* board, SearchData* data) {
  moveList->count = 0;

  int kingSq = lsb(board->pieces[KING[board->side]]);

  if (bits(board->checkers) > 1) {
    // double check, only king moves
    GenerateAllKingMoves(moveList, board);
  } else if (board->checkers) {
    // while in check, only non pinned pieces can move
    // they can move to squares that block the check, or capture
    // kings can fully evade

    BitBoard betweens = GetInBetweenSquares(kingSq, lsb(board->checkers));

    BitBoard nonPinned = ~board->pinned;
    GenerateAllPawnMoves(moveList, board->pieces[PAWN[board->side]] & nonPinned, betweens | board->checkers, board);
    GenerateAllKnightMoves(moveList, board->pieces[KNIGHT[board->side]] & nonPinned, betweens | board->checkers, board);
    GenerateAllBishopMoves(moveList, board->pieces[BISHOP[board->side]] & nonPinned, betweens | board->checkers, board);
    GnerateAllRookMoves(moveList, board->pieces[ROOK[board->side]] & nonPinned, betweens | board->checkers, board);
    GenerateAllQueenMoves(moveList, board->pieces[QUEEN[board->side]] & nonPinned, betweens | board->checkers, board);

    GenerateAllKingMoves(moveList, board);
  } else {
    // all non-pinned moves to anywhere on the board (FILLED)

    BitBoard nonPinned = ~board->pinned;
    GenerateAllPawnMoves(moveList, board->pieces[PAWN[board->side]] & nonPinned, FILLED, board);
    GenerateAllKnightMoves(moveList, board->pieces[KNIGHT[board->side]] & nonPinned, FILLED, board);
    GenerateAllBishopMoves(moveList, board->pieces[BISHOP[board->side]] & nonPinned, FILLED, board);
    GnerateAllRookMoves(moveList, board->pieces[ROOK[board->side]] & nonPinned, FILLED, board);
    GenerateAllQueenMoves(moveList, board->pieces[QUEEN[board->side]] & nonPinned, FILLED, board);
    GenerateAllKingMoves(moveList, board);

    // generate pinned piece moves, knights cannot move while pinned
    BitBoard pinnedPawns = board->pieces[PAWN[board->side]] & board->pinned;
    BitBoard pinnedBishops = board->pieces[BISHOP[board->side]] & board->pinned;
    BitBoard pinnedRooks = board->pieces[ROOK[board->side]] & board->pinned;
    BitBoard pinnedQueens = board->pieces[QUEEN[board->side]] & board->pinned;

    while (pinnedPawns) {
      int sq = lsb(pinnedPawns);
      GenerateAllPawnMoves(moveList, pinnedPawns & -pinnedPawns, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedPawns);
    }

    while (pinnedBishops) {
      int sq = lsb(pinnedBishops);
      GenerateAllBishopMoves(moveList, pinnedBishops & -pinnedBishops, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedBishops);
    }

    while (pinnedRooks) {
      int sq = lsb(pinnedRooks);
      GnerateAllRookMoves(moveList, pinnedRooks & -pinnedRooks, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedRooks);
    }

    while (pinnedQueens) {
      int sq = lsb(pinnedQueens);
      GenerateAllQueenMoves(moveList, pinnedQueens & -pinnedQueens, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedQueens);
    }
  }

  // this is the final legality check for moves - certain move types are specifically checked here
  // king moves, castles, and EP (some crazy pins)
  Move* curr = moveList->moves;
  while (curr != moveList->moves + moveList->count) {
    if ((MoveStart(*curr) == kingSq || MoveEP(*curr)) && !IsMoveLegal(*curr, board))
      *curr = moveList->moves[--moveList->count]; // overwrite this illegal move with the last move and try again
    else
      ++curr;
  }

  // Get the hash move for sorting
  TTValue ttValue = TTProbe(board->zobrist); // TODO: Don't I know this already from the search?
  Move hashMove = TTMove(ttValue);

  for (int i = 0; i < moveList->count; i++) {
    Move move = moveList->moves[i];

    // this is really ugly and should be made phased
    // we check the type of move being made and apply a score
    // HASH -> GOOD CAPTURES -> KILLERS -> COUNTER -> QUIETS -> BAD CAPTURES
    // large integers are used to spread these categories out to prevent conflict when sorting

    if (move == hashMove) {
      moveList->scores[i] = HASH_MOVE_SCORE;
    } else if (MoveEP(move)) {
      moveList->scores[i] = GOOD_CAPTURE_SCORE + MVV_LVA[PAWN[board->side]][PAWN[board->xside]];
    } else if (MoveCapture(move)) {
      int mover = MovePiece(move);
      int captured = board->squares[MoveEnd(move)];

      assert(captured != NO_PIECE);
      assert(mover != NO_PIECE);

      int seeScore = 0;
      if ((PIECE_TYPE[captured] <= PIECE_TYPE[mover]) && (seeScore = SEE(board, move)) < 0) {
        moveList->scores[i] = BAD_CAPTURE_SCORE + seeScore;
      } else {
        moveList->scores[i] = GOOD_CAPTURE_SCORE + MVV_LVA[mover][captured];
      }
    } else if (MovePromo(move) >= 8) {
      moveList->scores[i] = GOOD_CAPTURE_SCORE + STATIC_MATERIAL_VALUE[QUEEN_TYPE];
    } else if (move == data->killers[data->ply][0]) {
      moveList->scores[i] = KILLER1_SCORE;
    } else if (move == data->killers[data->ply][1]) {
      moveList->scores[i] = KILLER2_SCORE;
    } else if (data->ply && move == data->counters[MoveStartEnd(data->moves[data->ply - 1])]) {
      moveList->scores[i] = COUNTER_SCORE;
    } else {
      // if the move is totally quiet, we use our history heuristic.
      // hh is how many times this move caused a beta cutoff (depth scaled)
      // bf is how many times this move was searched and DIDNT cause a beta cutoff (depth scaled)
      // we scale hh by 100 for granularity sake
      moveList->scores[i] =
          100 * data->hh[board->side][MoveStartEnd(move)] / max(1, data->bf[board->side][MoveStartEnd(move)]);
    }
  }
}

// From a given index (to the end) find the best move
// and put it at the from index
// This is a single step of selection sort
void ChooseTopMove(MoveList* moveList, int from) {
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

void PrintMoves(MoveList* moveList) {
  printf("move  score p c d e t\n");

  for (int i = 0; i < moveList->count; i++) {
    ChooseTopMove(moveList, i);
    Move move = moveList->moves[i];
    printf("%s %5d %c %d %d %d %d\n", MoveToStr(move), moveList->scores[i], PIECE_TO_CHAR[MovePiece(move)],
           MoveCapture(move), MoveDoublePush(move), MoveEP(move), MoveCastle(move));
  }
}
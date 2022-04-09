// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2022 Jay Honnold

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

#include "movegen.h"

#include <assert.h>
#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "search.h"
#include "see.h"
#include "transposition.h"
#include "types.h"
#include "util.h"

inline void AppendMove(Move* arr, uint8_t* n, Move move) { arr[(*n)++] = move; }

// Move generation is pretty similar across all piece types with captures and quiets.
// Both receieve a BitBoard of acceptable squares, and additional logic is applied within
// each method to determine quiet vs capture.
// Promotions and Castles have specific methods

void GeneratePawnPromotions(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard promotingPawns = pawns & PromoRank(board->stm);

  BitBoard quietPromoters =
      (board->stm == WHITE ? ShiftN(promotingPawns) : ShiftS(promotingPawns)) & ~OccBB(BOTH) & possibilities;
  BitBoard capturingPromotersW =
      (board->stm == WHITE ? ShiftNW(promotingPawns) : ShiftSW(promotingPawns)) & OccBB(board->xstm) & possibilities;
  BitBoard capturingPromotersE =
      (board->stm == WHITE ? ShiftNE(promotingPawns) : ShiftSE(promotingPawns)) & OccBB(board->xstm) & possibilities;

  while (quietPromoters) {
    int to = lsb(quietPromoters);
    int from = to - PawnDir(board->stm);

    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(QUEEN, board->stm), 0, 0, 0, 0));
    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(ROOK, board->stm), 0, 0, 0, 0));
    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(BISHOP, board->stm), 0, 0, 0, 0));
    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(KNIGHT, board->stm), 0, 0, 0, 0));

    popLsb(quietPromoters);
  }

  while (capturingPromotersE) {
    int to = lsb(capturingPromotersE);
    int from = to - (PawnDir(board->stm) + E);

    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(QUEEN, board->stm), 1, 0, 0, 0));
    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(ROOK, board->stm), 1, 0, 0, 0));
    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(BISHOP, board->stm), 1, 0, 0, 0));
    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(KNIGHT, board->stm), 1, 0, 0, 0));

    popLsb(capturingPromotersE);
  }

  while (capturingPromotersW) {
    int to = lsb(capturingPromotersW);
    int from = to - (PawnDir(board->stm) + W);

    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(QUEEN, board->stm), 1, 0, 0, 0));
    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(ROOK, board->stm), 1, 0, 0, 0));
    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(BISHOP, board->stm), 1, 0, 0, 0));
    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(from, to, Piece(PAWN, board->stm), Piece(KNIGHT, board->stm), 1, 0, 0, 0));

    popLsb(capturingPromotersW);
  }
}

void GeneratePawnCaptures(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard nonPromotingPawns = pawns & ~PromoRank(board->stm);
  BitBoard capturingW = (board->stm == WHITE ? ShiftNW(nonPromotingPawns) : ShiftSW(nonPromotingPawns)) &
                        OccBB(board->xstm) & possibilities;
  BitBoard capturingE = (board->stm == WHITE ? ShiftNE(nonPromotingPawns) : ShiftSE(nonPromotingPawns)) &
                        OccBB(board->xstm) & possibilities;

  while (capturingE) {
    int to = lsb(capturingE);

    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(to - (PawnDir(board->stm) + E), to, Piece(PAWN, board->stm), 0, 1, 0, 0, 0));

    popLsb(capturingE);
  }

  while (capturingW) {
    int to = lsb(capturingW);
    AppendMove(moveList->tactical, &moveList->nTactical,
               BuildMove(to - (PawnDir(board->stm) + W), to, Piece(PAWN, board->stm), 0, 1, 0, 0, 0));
    popLsb(capturingW);
  }

  if (board->epSquare) {
    BitBoard epPawns = GetPawnAttacks(board->epSquare, board->xstm) & nonPromotingPawns;

    while (epPawns) {
      int from = lsb(epPawns);
      AppendMove(moveList->tactical, &moveList->nTactical,
                 BuildMove(from, board->epSquare, Piece(PAWN, board->stm), 0, 1, 0, 1, 0));
      popLsb(epPawns);
    }
  }
}

void GeneratePawnQuiets(MoveList* moveList, BitBoard pawns, BitBoard possibilities, Board* board) {
  BitBoard empty = ~OccBB(BOTH);
  BitBoard nonPromotingPawns = pawns & ~PromoRank(board->stm);

  BitBoard singlePush = (board->stm == WHITE ? ShiftN(nonPromotingPawns) : ShiftS(nonPromotingPawns)) & empty;
  BitBoard doublePush = (board->stm == WHITE ? ShiftN(singlePush & RANK_3) : ShiftS(singlePush & RANK_6)) & empty;

  singlePush &= possibilities;
  doublePush &= possibilities;

  while (singlePush) {
    int to = lsb(singlePush);
    AppendMove(moveList->quiet, &moveList->nQuiets,
               BuildMove(to - PawnDir(board->stm), to, Piece(PAWN, board->stm), 0, 0, 0, 0, 0));
    popLsb(singlePush);
  }

  while (doublePush) {
    int to = lsb(doublePush);
    AppendMove(moveList->quiet, &moveList->nQuiets,
               BuildMove(to - PawnDir(board->stm) - PawnDir(board->stm), to, Piece(PAWN, board->stm), 0, 0, 1, 0, 0));
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
    int from = lsb(knights);

    BitBoard attacks = GetKnightAttacks(from) & OccBB(board->xstm) & possibilities;
    while (attacks) {
      int to = lsb(attacks);

      AppendMove(moveList->tactical, &moveList->nTactical,
                 BuildMove(from, to, Piece(KNIGHT, board->stm), 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(knights);
  }
}

void GenerateKnightQuiets(MoveList* moveList, BitBoard knights, BitBoard possibilities, Board* board) {
  while (knights) {
    int from = lsb(knights);

    BitBoard attacks = GetKnightAttacks(from) & ~OccBB(BOTH) & possibilities;
    while (attacks) {
      int to = lsb(attacks);

      AppendMove(moveList->quiet, &moveList->nQuiets, BuildMove(from, to, Piece(KNIGHT, board->stm), 0, 0, 0, 0, 0));

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
    int from = lsb(bishops);

    BitBoard attacks = GetBishopAttacks(from, OccBB(BOTH)) & OccBB(board->xstm) & possibilities;
    while (attacks) {
      int to = lsb(attacks);

      AppendMove(moveList->tactical, &moveList->nTactical,
                 BuildMove(from, to, Piece(BISHOP, board->stm), 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(bishops);
  }
}

void GenerateBishopQuiets(MoveList* moveList, BitBoard bishops, BitBoard possibilities, Board* board) {
  while (bishops) {
    int from = lsb(bishops);

    BitBoard attacks = GetBishopAttacks(from, OccBB(BOTH)) & ~OccBB(BOTH) & possibilities;
    while (attacks) {
      int to = lsb(attacks);

      AppendMove(moveList->quiet, &moveList->nQuiets, BuildMove(from, to, Piece(BISHOP, board->stm), 0, 0, 0, 0, 0));

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
    int from = lsb(rooks);

    BitBoard attacks = GetRookAttacks(from, OccBB(BOTH)) & OccBB(board->xstm) & possibilities;
    while (attacks) {
      int to = lsb(attacks);

      AppendMove(moveList->tactical, &moveList->nTactical, BuildMove(from, to, Piece(ROOK, board->stm), 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(rooks);
  }
}

void GenerateRookQuiets(MoveList* moveList, BitBoard rooks, BitBoard possibilities, Board* board) {
  while (rooks) {
    int from = lsb(rooks);

    BitBoard attacks = GetRookAttacks(from, OccBB(BOTH)) & ~OccBB(BOTH) & possibilities;
    while (attacks) {
      int to = lsb(attacks);

      AppendMove(moveList->quiet, &moveList->nQuiets, BuildMove(from, to, Piece(ROOK, board->stm), 0, 0, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(rooks);
  }
}

void GenerateAllRookMoves(MoveList* moveList, BitBoard rooks, BitBoard possibilities, Board* board) {
  generateRookCaptures(moveList, rooks, possibilities, board);
  GenerateRookQuiets(moveList, rooks, possibilities, board);
}

void GenerateQueenCaptures(MoveList* moveList, BitBoard queens, BitBoard possibilities, Board* board) {
  while (queens) {
    int from = lsb(queens);

    BitBoard attacks = GetQueenAttacks(from, OccBB(BOTH)) & OccBB(board->xstm) & possibilities;
    while (attacks) {
      int to = lsb(attacks);

      AppendMove(moveList->tactical, &moveList->nTactical,
                 BuildMove(from, to, Piece(QUEEN, board->stm), 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(queens);
  }
}

void GenerateQueenQuiets(MoveList* moveList, BitBoard queens, BitBoard possibilities, Board* board) {
  while (queens) {
    int from = lsb(queens);

    BitBoard attacks = GetQueenAttacks(from, OccBB(BOTH)) & ~OccBB(BOTH) & possibilities;
    while (attacks) {
      int to = lsb(attacks);

      AppendMove(moveList->quiet, &moveList->nQuiets, BuildMove(from, to, Piece(QUEEN, board->stm), 0, 0, 0, 0, 0));

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
  BitBoard kings = PieceBB(KING, board->stm);

  while (kings) {
    int from = lsb(kings);

    BitBoard attacks = GetKingAttacks(from) & OccBB(board->xstm);
    while (attacks) {
      int to = lsb(attacks);

      AppendMove(moveList->tactical, &moveList->nTactical, BuildMove(from, to, Piece(KING, board->stm), 0, 1, 0, 0, 0));

      popLsb(attacks);
    }

    popLsb(kings);
  }
}

void GenerateCastles(MoveList* moveList, Board* board) {
  if (board->checkers)  // can't castle in check
    return;

  // logic for each castle is hardcoded (only 4 cases)
  // validate it's still possible, and that nothing is in the way
  // square attack logic is applied later
  if (board->stm == WHITE) {
    int kingSq = lsb(PieceBB(KING, WHITE));
    if (board->castling & 0x8 && !getBit(board->pinned, board->castleRooks[0])) {
      BitBoard between =
          GetInBetweenSquares(kingSq, G1) | GetInBetweenSquares(board->castleRooks[0], F1) | bit(G1) | bit(F1);
      if (!((OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ bit(board->castleRooks[0])) & between))
        AppendMove(moveList->quiet, &moveList->nQuiets, BuildMove(kingSq, G1, Piece(KING, board->stm), 0, 0, 0, 0, 1));
    }

    if (board->castling & 0x4 && !getBit(board->pinned, board->castleRooks[1])) {
      BitBoard between =
          GetInBetweenSquares(kingSq, C1) | GetInBetweenSquares(board->castleRooks[1], D1) | bit(C1) | bit(D1);
      if (!((OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ bit(board->castleRooks[1])) & between))
        AppendMove(moveList->quiet, &moveList->nQuiets, BuildMove(kingSq, C1, Piece(KING, board->stm), 0, 0, 0, 0, 1));
    }
  } else {
    int kingSq = lsb(PieceBB(KING, BLACK));
    if (board->castling & 0x2 && !getBit(board->pinned, board->castleRooks[2])) {
      BitBoard between =
          GetInBetweenSquares(kingSq, G8) | GetInBetweenSquares(board->castleRooks[2], F8) | bit(G8) | bit(F8);
      if (!((OccBB(BOTH) ^ PieceBB(KING, BLACK) ^ bit(board->castleRooks[2])) & between))
        AppendMove(moveList->quiet, &moveList->nQuiets, BuildMove(kingSq, G8, Piece(KING, board->stm), 0, 0, 0, 0, 1));
    }

    if (board->castling & 0x1 && !getBit(board->pinned, board->castleRooks[3])) {
      BitBoard between =
          GetInBetweenSquares(kingSq, C8) | GetInBetweenSquares(board->castleRooks[3], D8) | bit(C8) | bit(D8);
      if (!((OccBB(BOTH) ^ PieceBB(KING, BLACK) ^ bit(board->castleRooks[3])) & between))
        AppendMove(moveList->quiet, &moveList->nQuiets, BuildMove(kingSq, C8, Piece(KING, board->stm), 0, 0, 0, 0, 1));
    }
  }
}

void GenerateKingQuiets(MoveList* moveList, Board* board) {
  BitBoard kings = PieceBB(KING, board->stm);

  while (kings) {
    int from = lsb(kings);

    BitBoard attacks = GetKingAttacks(from) & ~OccBB(BOTH);
    while (attacks) {
      int to = lsb(attacks);

      AppendMove(moveList->quiet, &moveList->nQuiets, BuildMove(from, to, Piece(KING, board->stm), 0, 0, 0, 0, 0));

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

void GenerateAllMoves(MoveList* moveList, Board* board) {
  int kingSq = lsb(PieceBB(KING, board->stm));

  if (bits(board->checkers) > 1) {
    GenerateAllKingMoves(moveList, board);
  } else if (board->checkers) {
    BitBoard betweens = GetInBetweenSquares(kingSq, lsb(board->checkers));

    BitBoard nonPinned = ~board->pinned;
    GenerateAllPawnMoves(moveList, PieceBB(PAWN, board->stm) & nonPinned, betweens | board->checkers, board);
    GenerateAllKnightMoves(moveList, PieceBB(KNIGHT, board->stm) & nonPinned, betweens | board->checkers, board);
    GenerateAllBishopMoves(moveList, PieceBB(BISHOP, board->stm) & nonPinned, betweens | board->checkers, board);
    GenerateAllRookMoves(moveList, PieceBB(ROOK, board->stm) & nonPinned, betweens | board->checkers, board);
    GenerateAllQueenMoves(moveList, PieceBB(QUEEN, board->stm) & nonPinned, betweens | board->checkers, board);
    GenerateAllKingMoves(moveList, board);
  } else {
    BitBoard nonPinned = ~board->pinned;
    GenerateAllPawnMoves(moveList, PieceBB(PAWN, board->stm) & nonPinned, -1, board);
    GenerateAllKnightMoves(moveList, PieceBB(KNIGHT, board->stm) & nonPinned, -1, board);
    GenerateAllBishopMoves(moveList, PieceBB(BISHOP, board->stm) & nonPinned, -1, board);
    GenerateAllRookMoves(moveList, PieceBB(ROOK, board->stm) & nonPinned, -1, board);
    GenerateAllQueenMoves(moveList, PieceBB(QUEEN, board->stm) & nonPinned, -1, board);
    GenerateAllKingMoves(moveList, board);

    // TODO: Clean this up?
    // Could implement like stockfish and eliminate illegal pinned moves after the fact
    BitBoard pinnedPawns = PieceBB(PAWN, board->stm) & board->pinned;
    BitBoard pinnedBishops = PieceBB(BISHOP, board->stm) & board->pinned;
    BitBoard pinnedRooks = PieceBB(ROOK, board->stm) & board->pinned;
    BitBoard pinnedQueens = PieceBB(QUEEN, board->stm) & board->pinned;

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
      GenerateAllRookMoves(moveList, pinnedRooks & -pinnedRooks, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedRooks);
    }

    while (pinnedQueens) {
      int sq = lsb(pinnedQueens);
      GenerateAllQueenMoves(moveList, pinnedQueens & -pinnedQueens, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedQueens);
    }
  }

  Move* curr = moveList->tactical;
  while (curr != moveList->tactical + moveList->nTactical) {
    if ((From(*curr) == kingSq || IsEP(*curr)) && !IsMoveLegal(*curr, board))
      *curr =
          moveList->tactical[--moveList->nTactical];  // overwrite this illegal move with the last move and try again
    else
      ++curr;
  }

  curr = moveList->quiet;
  while (curr != moveList->quiet + moveList->nQuiets) {
    if (From(*curr) == kingSq && !IsMoveLegal(*curr, board))
      *curr = moveList->quiet[--moveList->nQuiets];  // overwrite this illegal move with the last move and try again
    else
      ++curr;
  }
}

// captures and promotions
void GenerateTacticalMoves(MoveList* moveList, Board* board) {
  int kingSq = lsb(PieceBB(KING, board->stm));

  if (bits(board->checkers) > 1) {
    // double check means only king moves are possible
    GenerateKingCaptures(moveList, board);
  } else if (board->checkers) {
    // while in check, only tactical moves to evade are captures
    // pinned pieces can NEVER be the piece to evade a check with a capture, so
    // those are filtered out
    BitBoard nonPinned = ~board->pinned;

    GeneratePawnCaptures(moveList, PieceBB(PAWN, board->stm) & nonPinned, board->checkers, board);
    GeneratePawnPromotions(moveList, PieceBB(PAWN, board->stm) & nonPinned,
                           board->checkers | GetInBetweenSquares(kingSq, lsb(board->checkers)), board);
    GenerateKnightCaptures(moveList, PieceBB(KNIGHT, board->stm) & nonPinned, board->checkers, board);
    generateBishopCaptures(moveList, PieceBB(BISHOP, board->stm) & nonPinned, board->checkers, board);
    generateRookCaptures(moveList, PieceBB(ROOK, board->stm) & nonPinned, board->checkers, board);
    GenerateQueenCaptures(moveList, PieceBB(QUEEN, board->stm) & nonPinned, board->checkers, board);
    GenerateKingCaptures(moveList, board);
  } else {
    // generate moves in two stages, non pinned piece moves, then pinned piece moves

    // all non-pinned captures
    BitBoard nonPinned = ~board->pinned;
    GeneratePawnCaptures(moveList, PieceBB(PAWN, board->stm) & nonPinned, -1, board);
    GeneratePawnPromotions(moveList, PieceBB(PAWN, board->stm) & nonPinned, -1, board);
    GenerateKnightCaptures(moveList, PieceBB(KNIGHT, board->stm) & nonPinned, -1, board);
    generateBishopCaptures(moveList, PieceBB(BISHOP, board->stm) & nonPinned, -1, board);
    generateRookCaptures(moveList, PieceBB(ROOK, board->stm) & nonPinned, -1, board);
    GenerateQueenCaptures(moveList, PieceBB(QUEEN, board->stm) & nonPinned, -1, board);
    GenerateKingCaptures(moveList, board);

    // get the pinned pieces and generate their moves
    // knights when pinned cannot move
    BitBoard pinnedPawns = PieceBB(PAWN, board->stm) & board->pinned;
    BitBoard pinnedBishops = PieceBB(BISHOP, board->stm) & board->pinned;
    BitBoard pinnedRooks = PieceBB(ROOK, board->stm) & board->pinned;
    BitBoard pinnedQueens = PieceBB(QUEEN, board->stm) & board->pinned;

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
  Move* curr = moveList->tactical;
  while (curr != moveList->tactical + moveList->nTactical) {
    if ((From(*curr) == kingSq || IsEP(*curr)) && !IsMoveLegal(*curr, board))
      *curr =
          moveList->tactical[--moveList->nTactical];  // overwrite this illegal move with the last move and try again
    else
      ++curr;
  }
}

void GenerateQuietMoves(MoveList* moveList, Board* board) {
  int kingSq = lsb(PieceBB(KING, board->stm));

  if (bits(board->checkers) > 1) {
    // double check, only king moves
    GenerateKingQuiets(moveList, board);
  } else if (board->checkers) {
    // while in check, only non pinned pieces can move
    // they can move to squares that block the check, or capture
    // kings can fully evade

    BitBoard betweens = GetInBetweenSquares(kingSq, lsb(board->checkers));

    BitBoard nonPinned = ~board->pinned;
    GeneratePawnQuiets(moveList, PieceBB(PAWN, board->stm) & nonPinned, betweens, board);
    GenerateKnightQuiets(moveList, PieceBB(KNIGHT, board->stm) & nonPinned, betweens, board);
    GenerateBishopQuiets(moveList, PieceBB(BISHOP, board->stm) & nonPinned, betweens, board);
    GenerateRookQuiets(moveList, PieceBB(ROOK, board->stm) & nonPinned, betweens, board);
    GenerateQueenQuiets(moveList, PieceBB(QUEEN, board->stm) & nonPinned, betweens, board);
    GenerateKingQuiets(moveList, board);
  } else {
    // all non-pinned moves to anywhere on the board (-1)

    BitBoard nonPinned = ~board->pinned;
    GeneratePawnQuiets(moveList, PieceBB(PAWN, board->stm) & nonPinned, -1, board);
    GenerateKnightQuiets(moveList, PieceBB(KNIGHT, board->stm) & nonPinned, -1, board);
    GenerateBishopQuiets(moveList, PieceBB(BISHOP, board->stm) & nonPinned, -1, board);
    GenerateRookQuiets(moveList, PieceBB(ROOK, board->stm) & nonPinned, -1, board);
    GenerateQueenQuiets(moveList, PieceBB(QUEEN, board->stm) & nonPinned, -1, board);
    GenerateKingQuiets(moveList, board);
    GenerateCastles(moveList, board);

    // generate pinned piece moves, knights cannot move while pinned
    BitBoard pinnedPawns = PieceBB(PAWN, board->stm) & board->pinned;
    BitBoard pinnedBishops = PieceBB(BISHOP, board->stm) & board->pinned;
    BitBoard pinnedRooks = PieceBB(ROOK, board->stm) & board->pinned;
    BitBoard pinnedQueens = PieceBB(QUEEN, board->stm) & board->pinned;

    while (pinnedPawns) {
      int sq = lsb(pinnedPawns);
      GeneratePawnQuiets(moveList, pinnedPawns & -pinnedPawns, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedPawns);
    }

    while (pinnedBishops) {
      int sq = lsb(pinnedBishops);
      GenerateBishopQuiets(moveList, pinnedBishops & -pinnedBishops, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedBishops);
    }

    while (pinnedRooks) {
      int sq = lsb(pinnedRooks);
      GenerateRookQuiets(moveList, pinnedRooks & -pinnedRooks, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedRooks);
    }

    while (pinnedQueens) {
      int sq = lsb(pinnedQueens);
      GenerateQueenQuiets(moveList, pinnedQueens & -pinnedQueens, GetPinnedMovementSquares(sq, kingSq), board);
      popLsb(pinnedQueens);
    }
  }

  // this is the final legality check for moves - certain move types are specifically checked here
  // king moves, castles, and EP (some crazy pins)
  Move* curr = moveList->quiet;
  while (curr != moveList->quiet + moveList->nQuiets) {
    if (From(*curr) == kingSq && !IsMoveLegal(*curr, board))
      *curr = moveList->quiet[--moveList->nQuiets];  // overwrite this illegal move with the last move and try again
    else
      ++curr;
  }
}
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

#ifndef MOVEGEN_H
#define MOVEGEN_H

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

#define PawnDir(c)   ((c) == WHITE ? N : S)
#define PromoRank(c) ((c) == WHITE ? RANK_7 : RANK_2)
#define DPRank(c)    ((c) == WHITE ? RANK_3 : RANK_6)

#define ALL -1ULL

#define NO_PROMO 0
#define QUIET    0b0000
#define CAPTURE  0b0001
#define EP       0b0101
#define DP       0b0010
#define CASTLE   0b1000

#define WHITE_KS 0x8
#define WHITE_QS 0x4
#define BLACK_KS 0x2
#define BLACK_QS 0x1

#define CanCastle(dir) (board->castling & (dir))

INLINE ScoredMove* AddMove(ScoredMove* moves, int from, int to, int moving, int promo, int flags) {
  *moves++ = (ScoredMove) {.move = BuildMove(from, to, moving, promo, flags), .score = 0};
  return moves;
}

INLINE ScoredMove* AddPromotions(ScoredMove* moves, int from, int to, int moving, int flags, const int stm) {
  moves = AddMove(moves, from, to, moving, Piece(QUEEN, stm), flags);
  moves = AddMove(moves, from, to, moving, Piece(ROOK, stm), flags);
  moves = AddMove(moves, from, to, moving, Piece(BISHOP, stm), flags);
  moves = AddMove(moves, from, to, moving, Piece(KNIGHT, stm), flags);
  return moves;
}

INLINE ScoredMove* AddPawnMoves(ScoredMove* moves, BitBoard opts, Board* board, const int stm, const int type) {
  const int xstm = stm ^ 1;

  if (type == QUIET) {
    // Quiet non-promotions
    BitBoard valid     = PieceBB(PAWN, stm) & ~PromoRank(stm);
    BitBoard targets   = ShiftPawnDir(valid, stm) & ~OccBB(BOTH);
    BitBoard dpTargets = ShiftPawnDir(targets & DPRank(stm), stm) & ~OccBB(BOTH);

    targets &= opts, dpTargets &= opts;

    while (targets) {
      int to = popAndGetLsb(&targets);
      moves  = AddMove(moves, to - PawnDir(stm), to, Piece(PAWN, stm), NO_PROMO, QUIET);
    }

    while (dpTargets) {
      int to = popAndGetLsb(&dpTargets);
      moves  = AddMove(moves, to - PawnDir(stm) - PawnDir(stm), to, Piece(PAWN, stm), NO_PROMO, DP);
    }
  } else {
    // Captures
    BitBoard valid    = PieceBB(PAWN, stm) & ~PromoRank(stm);
    BitBoard eTargets = ShiftPawnCapE(valid, stm) & OccBB(xstm) & opts;
    BitBoard wTargets = ShiftPawnCapW(valid, stm) & OccBB(xstm) & opts;

    while (eTargets) {
      int to = popAndGetLsb(&eTargets);
      moves  = AddMove(moves, to - (PawnDir(stm) + E), to, Piece(PAWN, stm), NO_PROMO, CAPTURE);
    }

    while (wTargets) {
      int to = popAndGetLsb(&wTargets);
      moves  = AddMove(moves, to - (PawnDir(stm) + W), to, Piece(PAWN, stm), NO_PROMO, CAPTURE);
    }

    if (board->epSquare) {
      BitBoard movers = GetPawnAttacks(board->epSquare, xstm) & valid;

      while (movers) {
        int from = popAndGetLsb(&movers);
        moves    = AddMove(moves, from, board->epSquare, Piece(PAWN, stm), NO_PROMO, EP);
      }
    }

    // Promotions (both capture and non-capture)
    valid             = PieceBB(PAWN, stm) & PromoRank(stm);
    BitBoard sTargets = ShiftPawnDir(valid, stm) & ~OccBB(BOTH) & opts;
    eTargets          = ShiftPawnCapE(valid, stm) & OccBB(xstm) & opts;
    wTargets          = ShiftPawnCapW(valid, stm) & OccBB(xstm) & opts;

    while (sTargets) {
      int to = popAndGetLsb(&sTargets);
      moves  = AddPromotions(moves, to - PawnDir(stm), to, Piece(PAWN, stm), QUIET, stm);
    }

    while (eTargets) {
      int to = popAndGetLsb(&eTargets);
      moves  = AddPromotions(moves, to - (PawnDir(stm) + E), to, Piece(PAWN, stm), CAPTURE, stm);
    }

    while (wTargets) {
      int to = popAndGetLsb(&wTargets);
      moves  = AddPromotions(moves, to - (PawnDir(stm) + W), to, Piece(PAWN, stm), CAPTURE, stm);
    }
  }

  return moves;
}

INLINE ScoredMove* AddPieceMoves(ScoredMove* moves,
                                 BitBoard opts,
                                 Board* board,
                                 const int stm,
                                 const int type,
                                 const int piece) {
  const int xstm = stm ^ 1;

  BitBoard movers = PieceBB(piece, stm);
  while (movers) {
    int from = popAndGetLsb(&movers);

    BitBoard valid   = GetPieceAttacks(from, OccBB(BOTH), piece) & opts;
    BitBoard targets = type == QUIET ? (valid & ~OccBB(BOTH)) : (valid & OccBB(xstm));

    while (targets) {
      int to = popAndGetLsb(&targets);
      moves  = AddMove(moves, from, to, Piece(piece, stm), NO_PROMO, type == QUIET ? QUIET : CAPTURE);
    }
  }

  return moves;
}

INLINE ScoredMove* AddCastles(ScoredMove* moves, Board* board, const int stm) {
  if (stm == WHITE) {
    int from = lsb(PieceBB(KING, WHITE));

    if (CanCastle(WHITE_KS)) {
      BitBoard between = BetweenSquares(from, G1) | BetweenSquares(board->cr[0], F1) | bit(G1) | bit(F1);
      if (!((OccBB(BOTH) ^ PieceBB(KING, stm) ^ bit(board->cr[0])) & between))
        moves = AddMove(moves, from, G1, Piece(KING, stm), NO_PROMO, CASTLE);
    }

    if (CanCastle(WHITE_QS)) {
      BitBoard between = BetweenSquares(from, C1) | BetweenSquares(board->cr[1], D1) | bit(C1) | bit(D1);
      if (!((OccBB(BOTH) ^ PieceBB(KING, stm) ^ bit(board->cr[1])) & between))
        moves = AddMove(moves, from, C1, Piece(KING, stm), NO_PROMO, CASTLE);
    }
  } else {
    int from = lsb(PieceBB(KING, BLACK));

    if (CanCastle(BLACK_KS)) {
      BitBoard between = BetweenSquares(from, G8) | BetweenSquares(board->cr[2], F8) | bit(G8) | bit(F8);
      if (!((OccBB(BOTH) ^ PieceBB(KING, stm) ^ bit(board->cr[2])) & between))
        moves = AddMove(moves, from, G8, Piece(KING, stm), NO_PROMO, CASTLE);
    }

    if (CanCastle(BLACK_QS)) {
      BitBoard between = BetweenSquares(from, C8) | BetweenSquares(board->cr[3], D8) | bit(C8) | bit(D8);
      if (!((OccBB(BOTH) ^ PieceBB(KING, stm) ^ bit(board->cr[3])) & between))
        moves = AddMove(moves, from, C8, Piece(KING, stm), NO_PROMO, CASTLE);
    }
  }

  return moves;
}

INLINE ScoredMove* AddPseudoLegalMoves(ScoredMove* moves, Board* board, const int type) {
  const int color = board->stm == WHITE ? WHITE : BLACK;

  if (bits(board->checkers) > 1) return AddPieceMoves(moves, ALL, board, color, type, KING);

  BitBoard pieceOpts = !board->checkers ? ALL :
                       type == QUIET    ? BetweenSquares(lsb(PieceBB(KING, color)), lsb(board->checkers)) :
                                          board->checkers;
  BitBoard pawnOpts  = !board->checkers ? ALL :
                                          BetweenSquares(lsb(PieceBB(KING, color)), lsb(board->checkers)) |
                                           ((type != QUIET) * board->checkers);

  moves = AddPawnMoves(moves, pawnOpts, board, color, type);
  moves = AddPieceMoves(moves, pieceOpts, board, color, type, KNIGHT);
  moves = AddPieceMoves(moves, pieceOpts, board, color, type, BISHOP);
  moves = AddPieceMoves(moves, pieceOpts, board, color, type, ROOK);
  moves = AddPieceMoves(moves, pieceOpts, board, color, type, QUEEN);
  moves = AddPieceMoves(moves, ALL, board, color, type, KING);
  if (type == QUIET && !board->checkers) moves = AddCastles(moves, board, color);

  return moves;
}

INLINE ScoredMove* AddLegalMoves(ScoredMove* moves, Board* board, const int type) {
  ScoredMove* curr = moves;
  BitBoard pinned  = board->pinned;
  int king         = lsb(PieceBB(KING, board->stm));

  moves = AddPseudoLegalMoves(moves, board, type);

  while (curr != moves) {
    if (((pinned && getBit(pinned, From(curr->move))) || From(curr->move) == king || IsEP(curr->move)) &&
        !IsLegal(curr->move, board))
      curr->move = (--moves)->move;
    else
      curr++;
  }

  return moves;
}

ScoredMove* AddTacticalMoves(ScoredMove* moves, Board* board);
ScoredMove* AddQuietMoves(ScoredMove* moves, Board* board);

#endif

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

#define QUIET 0
#define NOISY 1

#define WHITE_KS 0x8
#define WHITE_QS 0x4
#define BLACK_KS 0x2
#define BLACK_QS 0x1

#define CanCastle(dir) (board->castling & (dir))

INLINE void AppendMove(Move* arr, uint8_t* n, int from, int to, int flags) {
  arr[(*n)++] = BuildMove(from, to, flags);
}

INLINE void GeneratePawnPromotions(MoveList* list, BitBoard movers, BitBoard opts, Board* board, const int stm) {
  const int xstm = stm ^ 1;
  Move* arr      = list->tactical;
  uint8_t* n     = &list->nTactical;

  BitBoard valid   = movers & PromoRank(stm);
  BitBoard targets = ShiftPawnDir(valid, stm) & ~OccBB(BOTH) & opts;

  while (targets) {
    int to   = popAndGetLsb(&targets);
    int from = to - PawnDir(stm);

    AppendMove(arr, n, from, to, QUEEN_PROMO);
    AppendMove(arr, n, from, to, ROOK_PROMO);
    AppendMove(arr, n, from, to, BISHOP_PROMO);
    AppendMove(arr, n, from, to, KNIGHT_PROMO);
  }

  targets = ShiftPawnCapE(valid, stm) & OccBB(xstm) & opts;

  while (targets) {
    int to   = popAndGetLsb(&targets);
    int from = to - (PawnDir(stm) + E);

    AppendMove(arr, n, from, to, QUEEN_PROMO);
    AppendMove(arr, n, from, to, ROOK_PROMO);
    AppendMove(arr, n, from, to, BISHOP_PROMO);
    AppendMove(arr, n, from, to, KNIGHT_PROMO);
  }

  targets = ShiftPawnCapW(valid, stm) & OccBB(xstm) & opts;

  while (targets) {
    int to   = popAndGetLsb(&targets);
    int from = to - (PawnDir(stm) + W);

    AppendMove(arr, n, from, to, QUEEN_PROMO);
    AppendMove(arr, n, from, to, ROOK_PROMO);
    AppendMove(arr, n, from, to, BISHOP_PROMO);
    AppendMove(arr, n, from, to, KNIGHT_PROMO);
  }
}

INLINE void GeneratePawnCaptures(MoveList* list, BitBoard movers, BitBoard opts, Board* board, const int stm) {
  const int xstm = stm ^ 1;
  Move* arr      = list->tactical;
  uint8_t* n     = &list->nTactical;

  BitBoard valid   = movers & ~PromoRank(stm);
  BitBoard targets = ShiftPawnCapE(valid, stm) & OccBB(xstm) & opts;

  while (targets) {
    int to   = popAndGetLsb(&targets);
    int from = to - (PawnDir(stm) + E);

    AppendMove(arr, n, from, to, NORMAL);
  }

  targets = ShiftPawnCapW(valid, stm) & OccBB(xstm) & opts;

  while (targets) {
    int to   = popAndGetLsb(&targets);
    int from = to - (PawnDir(stm) + W);

    AppendMove(arr, n, from, to, NORMAL);
  }

  if (!board->epSquare) return;

  BitBoard pawns = GetPawnAttacks(board->epSquare, xstm) & valid;

  while (pawns) {
    int from = popAndGetLsb(&pawns);
    int to   = board->epSquare;

    AppendMove(arr, n, from, to, EP);
  }
}

INLINE void GeneratePawnQuiets(MoveList* list, BitBoard movers, BitBoard opts, Board* board, const int stm) {
  Move* arr  = list->quiet;
  uint8_t* n = &list->nQuiets;

  BitBoard valid     = movers & ~PromoRank(stm);
  BitBoard targets   = ShiftPawnDir(valid, stm) & ~OccBB(BOTH);
  BitBoard dpTargets = ShiftPawnDir(targets & DPRank(stm), stm) & ~OccBB(BOTH);

  targets &= opts;
  dpTargets &= opts;

  while (targets) {
    int to   = popAndGetLsb(&targets);
    int from = to - PawnDir(stm);

    AppendMove(arr, n, from, to, NORMAL);
  }

  while (dpTargets) {
    int to   = popAndGetLsb(&dpTargets);
    int from = to - PawnDir(stm) - PawnDir(stm);

    AppendMove(arr, n, from, to, NORMAL);
  }
}

INLINE void GeneratePieceMoves(MoveList* list,
                               BitBoard movers,
                               BitBoard opts,
                               Board* board,
                               const int stm,
                               const int piece,
                               const int type) {
  const int xstm = stm ^ 1;
  Move* arr      = type == QUIET ? list->quiet : list->tactical;
  uint8_t* n     = type == QUIET ? &list->nQuiets : &list->nTactical;

  while (movers) {
    int from = popAndGetLsb(&movers);

    BitBoard targets = (type == QUIET ? GetPieceAttacks(from, OccBB(BOTH), piece) & ~OccBB(BOTH) :
                                        GetPieceAttacks(from, OccBB(BOTH), piece) & OccBB(xstm)) &
                       opts;
    while (targets) {
      int to = popAndGetLsb(&targets);

      AppendMove(arr, n, from, to, NORMAL);
    }
  }
}

INLINE void GenerateCastles(MoveList* list, Board* board, const int stm) {
  Move* arr  = list->quiet;
  uint8_t* n = &list->nQuiets;

  if (stm == WHITE) {
    int from = lsb(PieceBB(KING, WHITE));

    if (CanCastle(WHITE_KS)) {
      BitBoard between = BetweenSquares(from, G1) | BetweenSquares(board->cr[0], F1) | bit(G1) | bit(F1);
      if (!((OccBB(BOTH) ^ PieceBB(KING, stm) ^ bit(board->cr[0])) & between))
        AppendMove(arr, n, from, G1, CASTLE);
    }

    if (CanCastle(WHITE_QS)) {
      BitBoard between = BetweenSquares(from, C1) | BetweenSquares(board->cr[1], D1) | bit(C1) | bit(D1);
      if (!((OccBB(BOTH) ^ PieceBB(KING, stm) ^ bit(board->cr[1])) & between))
        AppendMove(arr, n, from, C1, CASTLE);
    }
  } else {
    int from = lsb(PieceBB(KING, BLACK));

    if (CanCastle(BLACK_KS)) {
      BitBoard between = BetweenSquares(from, G8) | BetweenSquares(board->cr[2], F8) | bit(G8) | bit(F8);
      if (!((OccBB(BOTH) ^ PieceBB(KING, stm) ^ bit(board->cr[2])) & between))
        AppendMove(arr, n, from, G8, CASTLE);
    }

    if (CanCastle(BLACK_QS)) {
      BitBoard between = BetweenSquares(from, C8) | BetweenSquares(board->cr[3], D8) | bit(C8) | bit(D8);
      if (!((OccBB(BOTH) ^ PieceBB(KING, stm) ^ bit(board->cr[3])) & between))
        AppendMove(arr, n, from, C8, CASTLE);
    }
  }
}

INLINE void GenerateMoves(MoveList* list, Board* board, const int type, const int legal) {
  const int stm = board->stm;
  Move* arr     = type == QUIET ? list->quiet : list->tactical;
  uint8_t* n    = type == QUIET ? &list->nQuiets : &list->nTactical;

  int king = lsb(PieceBB(KING, stm));

  if (bits(board->checkers) > 1)
    GeneratePieceMoves(list, PieceBB(KING, stm), ALL, board, stm, KING, type);
  else if (board->checkers) {
    BitBoard opts = type == QUIET ? BetweenSquares(king, lsb(board->checkers)) : board->checkers;

    if (type == QUIET)
      GeneratePawnQuiets(list, PieceBB(PAWN, stm), opts, board, stm);
    else {
      GeneratePawnCaptures(list, PieceBB(PAWN, stm), opts, board, stm);
      GeneratePawnPromotions(list, PieceBB(PAWN, stm), opts | BetweenSquares(king, lsb(board->checkers)), board, stm);
    }

    GeneratePieceMoves(list, PieceBB(KNIGHT, stm), opts, board, stm, KNIGHT, type);
    GeneratePieceMoves(list, PieceBB(BISHOP, stm), opts, board, stm, BISHOP, type);
    GeneratePieceMoves(list, PieceBB(ROOK, stm), opts, board, stm, ROOK, type);
    GeneratePieceMoves(list, PieceBB(QUEEN, stm), opts, board, stm, QUEEN, type);
    GeneratePieceMoves(list, PieceBB(KING, stm), ALL, board, stm, KING, type);
  } else {
    if (type == QUIET)
      GeneratePawnQuiets(list, PieceBB(PAWN, stm), ALL, board, stm);
    else {
      GeneratePawnCaptures(list, PieceBB(PAWN, stm), ALL, board, stm);
      GeneratePawnPromotions(list, PieceBB(PAWN, stm), ALL, board, stm);
    }

    GeneratePieceMoves(list, PieceBB(KNIGHT, stm), ALL, board, stm, KNIGHT, type);
    GeneratePieceMoves(list, PieceBB(BISHOP, stm), ALL, board, stm, BISHOP, type);
    GeneratePieceMoves(list, PieceBB(ROOK, stm), ALL, board, stm, ROOK, type);
    GeneratePieceMoves(list, PieceBB(QUEEN, stm), ALL, board, stm, QUEEN, type);
    GeneratePieceMoves(list, PieceBB(KING, stm), ALL, board, stm, KING, type);
    if (type == QUIET) GenerateCastles(list, board, stm);
  }

  if (legal) {
    // this is the final legality check for moves - certain move types are specifically checked here
    // king moves, castles, and EP (some crazy pins)
    Move* curr = arr;
    while (curr != arr + *n) {
      if (((board->pinned && getBit(board->pinned, From(*curr))) || From(*curr) == king || IsEP(*curr)) &&
          !IsLegal(*curr, board))
        *curr = arr[--(*n)]; // overwrite this illegal move with the last move and try again
      else
        ++curr;
    }
  }
}

void GenerateQuietMoves(MoveList* moveList, Board* board);
void GenerateTacticalMoves(MoveList* moveList, Board* board);
void GenerateAllMoves(MoveList* moveList, Board* board);

#endif

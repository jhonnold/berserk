// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2023 Jay Honnold

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

extern const int CASTLE_MAP[4][3];

#define PawnDir(c)   ((c) == WHITE ? N : S)
#define PromoRank(c) ((c) == WHITE ? RANK_7 : RANK_2)
#define DPRank(c)    ((c) == WHITE ? RANK_3 : RANK_6)

#define ALL -1ULL

#define WHITE_KS 0x8
#define WHITE_QS 0x4
#define BLACK_KS 0x2
#define BLACK_QS 0x1

#define CanCastle(dir) (board->castling & (dir))

// Note: Any usage of & that matches 1 or 2 will match 3
//       thus making GT_LEGAL generate all.
enum {
  GT_QUIET   = 0b01,
  GT_CAPTURE = 0b10,
  GT_LEGAL   = 0b11,
};

INLINE ScoredMove* AddMove(ScoredMove* moves, int from, int to, int moving, int flags) {
  *moves++ = (ScoredMove) {.move = BuildMove(from, to, moving, flags), .score = 0};
  return moves;
}

INLINE ScoredMove* AddPromotions(ScoredMove* moves, int from, int to, int moving, const int baseFlag, const int type) {
  if (type & GT_CAPTURE)
    moves = AddMove(moves, from, to, moving, baseFlag | QUEEN_PROMO_FLAG);

  if (type & GT_QUIET) {
    moves = AddMove(moves, from, to, moving, baseFlag | ROOK_PROMO_FLAG);
    moves = AddMove(moves, from, to, moving, baseFlag | BISHOP_PROMO_FLAG);
    moves = AddMove(moves, from, to, moving, baseFlag | KNIGHT_PROMO_FLAG);
  }

  return moves;
}

INLINE ScoredMove* AddPawnMoves(ScoredMove* moves, BitBoard opts, Board* board, const int stm, const int type) {
  const int xstm = stm ^ 1;

  if (type & GT_QUIET) {
    // Quiet non-promotions
    BitBoard valid     = PieceBB(PAWN, stm) & ~PromoRank(stm);
    BitBoard targets   = ShiftPawnDir(valid, stm) & ~OccBB(BOTH);
    BitBoard dpTargets = ShiftPawnDir(targets & DPRank(stm), stm) & ~OccBB(BOTH);

    targets &= opts, dpTargets &= opts;

    while (targets) {
      int to = PopLSB(&targets);
      moves  = AddMove(moves, to - PawnDir(stm), to, Piece(PAWN, stm), QUIET_FLAG);
    }

    while (dpTargets) {
      int to = PopLSB(&dpTargets);
      moves  = AddMove(moves, to - PawnDir(stm) - PawnDir(stm), to, Piece(PAWN, stm), QUIET_FLAG);
    }
  }

  if (type & GT_CAPTURE) {
    // Captures non-promotions
    BitBoard valid    = PieceBB(PAWN, stm) & ~PromoRank(stm);
    BitBoard eTargets = ShiftPawnCapE(valid, stm) & OccBB(xstm) & opts;
    BitBoard wTargets = ShiftPawnCapW(valid, stm) & OccBB(xstm) & opts;

    while (eTargets) {
      int to = PopLSB(&eTargets);
      moves  = AddMove(moves, to - (PawnDir(stm) + E), to, Piece(PAWN, stm), CAPTURE_FLAG);
    }

    while (wTargets) {
      int to = PopLSB(&wTargets);
      moves  = AddMove(moves, to - (PawnDir(stm) + W), to, Piece(PAWN, stm), CAPTURE_FLAG);
    }

    if (board->epSquare) {
      BitBoard movers = GetPawnAttacks(board->epSquare, xstm) & valid;

      while (movers) {
        int from = PopLSB(&movers);
        moves    = AddMove(moves, from, board->epSquare, Piece(PAWN, stm), EP_FLAG);
      }
    }
  }

  // Promotions (both capture and non-capture)
  BitBoard valid    = PieceBB(PAWN, stm) & PromoRank(stm);
  BitBoard sTargets = ShiftPawnDir(valid, stm) & ~OccBB(BOTH) & opts;
  BitBoard eTargets = ShiftPawnCapE(valid, stm) & OccBB(xstm) & opts;
  BitBoard wTargets = ShiftPawnCapW(valid, stm) & OccBB(xstm) & opts;

  while (sTargets) {
    int to = PopLSB(&sTargets);
    moves  = AddPromotions(moves, to - PawnDir(stm), to, Piece(PAWN, stm), QUIET_FLAG, type);
  }

  while (eTargets) {
    int to = PopLSB(&eTargets);
    moves  = AddPromotions(moves, to - (PawnDir(stm) + E), to, Piece(PAWN, stm), CAPTURE_FLAG, type);
  }

  while (wTargets) {
    int to = PopLSB(&wTargets);
    moves  = AddPromotions(moves, to - (PawnDir(stm) + W), to, Piece(PAWN, stm), CAPTURE_FLAG, type);
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
    int from = PopLSB(&movers);

    BitBoard valid = GetPieceAttacks(from, OccBB(BOTH), piece) & opts;
    BitBoard targets = type == GT_QUIET ? valid & ~OccBB(BOTH) : //
                       type == GT_CAPTURE ? valid & OccBB(xstm) : //
                       valid & ~OccBB(stm);

    while (targets) {
      int to = PopLSB(&targets);
      int flags = type == GT_QUIET ? QUIET_FLAG : //
                  type == GT_CAPTURE ? CAPTURE_FLAG : //
                  GetBit(OccBB(xstm), to) ? CAPTURE_FLAG : QUIET_FLAG;

      moves = AddMove(moves, from, to, Piece(piece, stm), flags);
    }
  }

  return moves;
}

INLINE ScoredMove* AddCastles(ScoredMove* moves, Board* board, const int stm) {
  const int rookStart = stm == WHITE ? 0 : 2;
  const int rookEnd   = stm == WHITE ? 2 : 4;
  const int from      = LSB(PieceBB(KING, stm));

  for (int rk = rookStart; rk < rookEnd; rk++) {
    if (!CanCastle(CASTLE_MAP[rk][0]))
      continue;

    int rookFrom = board->cr[rk];
    if (GetBit(board->pinned, rookFrom))
      continue;

    int to = CASTLE_MAP[rk][1], rookTo = CASTLE_MAP[rk][2];

    BitBoard kingCrossing = BetweenSquares(from, to) | Bit(to);
    BitBoard rookCrossing = BetweenSquares(rookFrom, rookTo) | Bit(rookTo);
    BitBoard between      = kingCrossing | rookCrossing;

    if (!((OccBB(BOTH) ^ Bit(from) ^ Bit(rookFrom)) & between))
      if (!(kingCrossing & board->threatened))
        moves = AddMove(moves, from, to, Piece(KING, stm), CASTLE_FLAG);
  }

  return moves;
}

INLINE ScoredMove* AddQuietChecks(ScoredMove* moves, Board* board, const int stm) {
  const int xstm            = !stm;
  const int oppKingSq       = LSB(PieceBB(KING, xstm));
  const BitBoard bishopMask = GetBishopAttacks(oppKingSq, OccBB(BOTH));
  const BitBoard rookMask   = GetRookAttacks(oppKingSq, OccBB(BOTH));

  moves = AddPieceMoves(moves, bishopMask | rookMask, board, stm, GT_QUIET, QUEEN);
  moves = AddPieceMoves(moves, rookMask, board, stm, GT_QUIET, ROOK);
  moves = AddPieceMoves(moves, bishopMask, board, stm, GT_QUIET, BISHOP);
  moves = AddPieceMoves(moves, GetKnightAttacks(oppKingSq), board, stm, GT_QUIET, KNIGHT);

  return AddPawnMoves(moves, GetPawnAttacks(oppKingSq, xstm), board, stm, GT_QUIET);
}

INLINE ScoredMove* AddPseudoLegalMovesTo(ScoredMove* moves,
                                         Board* board,
                                         BitBoard opts,
                                         BitBoard kingOpts,
                                         const int type,
                                         const int color) {
  moves = AddPawnMoves(moves, opts, board, color, type);
  moves = AddPieceMoves(moves, opts, board, color, type, KNIGHT);
  moves = AddPieceMoves(moves, opts, board, color, type, BISHOP);
  moves = AddPieceMoves(moves, opts, board, color, type, ROOK);
  moves = AddPieceMoves(moves, opts, board, color, type, QUEEN);
  return AddPieceMoves(moves, kingOpts, board, color, type, KING);
}

INLINE ScoredMove* AddPseudoLegalMoves(ScoredMove* moves, Board* board, const int type, const int color) {
  if (BitCount(board->checkers) > 1)
    return AddPieceMoves(moves, ~board->threatened, board, color, type, KING);

  const int kingSq  = LSB(PieceBB(KING, color));
  const int checker = LSB(board->checkers);

  BitBoard opts     = !board->checkers ? ALL : BetweenSquares(kingSq, checker) | board->checkers;
  BitBoard kingOpts = ~board->threatened;

  moves = AddPseudoLegalMovesTo(moves, board, opts, kingOpts, type, color);
  if ((type & GT_QUIET) && !board->checkers)
    moves = AddCastles(moves, board, color);

  return moves;
}

INLINE ScoredMove* AddLegalMoves(ScoredMove* moves, Board* board, const int color) {
  ScoredMove* curr = moves;
  BitBoard pinned  = board->pinned;

  moves = AddPseudoLegalMoves(moves, board, GT_LEGAL, color);

  while (curr != moves) {
    if (((pinned && GetBit(pinned, From(curr->move))) || IsEP(curr->move)) && !IsLegal(curr->move, board))
      curr->move = (--moves)->move;
    else
      curr++;
  }

  return moves;
}

ScoredMove* AddNoisyMoves(ScoredMove* moves, Board* board);
ScoredMove* AddQuietMoves(ScoredMove* moves, Board* board);
ScoredMove* AddEvasionMoves(ScoredMove* moves, Board* board);
ScoredMove* AddQuietCheckMoves(ScoredMove* moves, Board* board);
ScoredMove* AddRecaptureMoves(ScoredMove* moves, Board* board, const int sq);
ScoredMove* AddPerftMoves(ScoredMove* moves, Board* board);

#endif

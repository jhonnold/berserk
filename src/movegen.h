// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2026 Jay Honnold

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
#include "move.h"
#include "types.h"
#include "util.h"

typedef struct {
  int score;
  Move move;
} ScoredMove;

extern const int CASTLE_MAP[4][3];

INLINE BitBoard PromoRank(Color c) {
  return c == WHITE ? RANK_7 : RANK_2;
}

INLINE BitBoard DPRank(Color c) {
  return c == WHITE ? RANK_3 : RANK_6;
}

#define ALL -1ULL

#define WHITE_KS 0x8
#define WHITE_QS 0x4
#define BLACK_KS 0x2
#define BLACK_QS 0x1

INLINE Bool CanCastle(Board* board, int dir) {
  return !!(board->castling & dir);
}

// Note: Any usage of & that matches 1 or 2 will match 3
//       thus making GT_LEGAL generate all.
enum {
  GT_QUIET   = 0b01,
  GT_CAPTURE = 0b10,
  GT_LEGAL   = 0b11,
};

INLINE ScoredMove* AddMove(ScoredMove* moves, Square from, Square to, Piece moving, int flags) {
  *moves++ = (ScoredMove) {.move = BuildMove(from, to, moving, flags), .score = 0};
  return moves;
}

INLINE ScoredMove* AddPromotions(ScoredMove* moves,
                                 Square from,
                                 Square to,
                                 Piece moving,
                                 const int baseFlag,
                                 const int type) {
  if (type & GT_CAPTURE)
    moves = AddMove(moves, from, to, moving, baseFlag | QUEEN_PROMO_FLAG);

  if (type & GT_QUIET) {
    moves = AddMove(moves, from, to, moving, baseFlag | ROOK_PROMO_FLAG);
    moves = AddMove(moves, from, to, moving, baseFlag | BISHOP_PROMO_FLAG);
    moves = AddMove(moves, from, to, moving, baseFlag | KNIGHT_PROMO_FLAG);
  }

  return moves;
}

INLINE ScoredMove* AddPawnMoves(ScoredMove* moves, BitBoard opts, Board* board, const Color stm, const int type) {
  const Color xstm = stm ^ 1;

  if (type & GT_QUIET) {
    // Quiet non-promotions
    BitBoard valid     = PieceBB(board, PAWN, stm) & ~PromoRank(stm);
    BitBoard targets   = ShiftPawnDir(valid, stm) & ~OccBB(board, BOTH);
    BitBoard dpTargets = ShiftPawnDir(targets & DPRank(stm), stm) & ~OccBB(board, BOTH);

    targets &= opts, dpTargets &= opts;

    while (targets) {
      Square to = PopLSB(&targets);
      moves     = AddMove(moves, to - PawnDir(stm), to, MakePiece(PAWN, stm), QUIET_FLAG);
    }

    while (dpTargets) {
      Square to = PopLSB(&dpTargets);
      moves     = AddMove(moves, to - PawnDir(stm) - PawnDir(stm), to, MakePiece(PAWN, stm), QUIET_FLAG);
    }
  }

  if (type & GT_CAPTURE) {
    // Captures non-promotions
    BitBoard valid    = PieceBB(board, PAWN, stm) & ~PromoRank(stm);
    BitBoard eTargets = ShiftPawnCapE(valid, stm) & OccBB(board, xstm) & opts;
    BitBoard wTargets = ShiftPawnCapW(valid, stm) & OccBB(board, xstm) & opts;

    while (eTargets) {
      Square to = PopLSB(&eTargets);
      moves     = AddMove(moves, to - (PawnDir(stm) + E), to, MakePiece(PAWN, stm), CAPTURE_FLAG);
    }

    while (wTargets) {
      Square to = PopLSB(&wTargets);
      moves     = AddMove(moves, to - (PawnDir(stm) + W), to, MakePiece(PAWN, stm), CAPTURE_FLAG);
    }

    if (board->epSquare) {
      BitBoard movers = GetPawnAttacks(board->epSquare, xstm) & valid;

      while (movers) {
        Square from = PopLSB(&movers);
        moves       = AddMove(moves, from, board->epSquare, MakePiece(PAWN, stm), EP_FLAG);
      }
    }
  }

  // Promotions (both capture and non-capture)
  BitBoard valid    = PieceBB(board, PAWN, stm) & PromoRank(stm);
  BitBoard sTargets = ShiftPawnDir(valid, stm) & ~OccBB(board, BOTH) & opts;
  BitBoard eTargets = ShiftPawnCapE(valid, stm) & OccBB(board, xstm) & opts;
  BitBoard wTargets = ShiftPawnCapW(valid, stm) & OccBB(board, xstm) & opts;

  while (sTargets) {
    Square to = PopLSB(&sTargets);
    moves     = AddPromotions(moves, to - PawnDir(stm), to, MakePiece(PAWN, stm), QUIET_FLAG, type);
  }

  while (eTargets) {
    Square to = PopLSB(&eTargets);
    moves     = AddPromotions(moves, to - (PawnDir(stm) + E), to, MakePiece(PAWN, stm), CAPTURE_FLAG, type);
  }

  while (wTargets) {
    Square to = PopLSB(&wTargets);
    moves     = AddPromotions(moves, to - (PawnDir(stm) + W), to, MakePiece(PAWN, stm), CAPTURE_FLAG, type);
  }

  return moves;
}

INLINE ScoredMove* AddPieceMoves(ScoredMove* moves,
                                 BitBoard opts,
                                 Board* board,
                                 const Color stm,
                                 const int type,
                                 const PieceType piece) {
  const Color xstm = stm ^ 1;

  BitBoard movers = PieceBB(board, piece, stm);
  while (movers) {
    Square from    = PopLSB(&movers);
    BitBoard valid = GetPieceAttacks(from, OccBB(board, BOTH), piece) & opts;

    if (type & GT_CAPTURE) {
      BitBoard targets = valid & OccBB(board, xstm);

      while (targets) {
        Square to = PopLSB(&targets);

        moves = AddMove(moves, from, to, MakePiece(piece, stm), CAPTURE_FLAG);
      }
    }

    if (type & GT_QUIET) {
      BitBoard targets = valid & ~OccBB(board, BOTH);

      while (targets) {
        Square to = PopLSB(&targets);

        moves = AddMove(moves, from, to, MakePiece(piece, stm), QUIET_FLAG);
      }
    }
  }

  return moves;
}

INLINE ScoredMove* AddCastles(ScoredMove* moves, Board* board, const Color stm) {
  const int rookStart = stm == WHITE ? 0 : 2;
  const int rookEnd   = stm == WHITE ? 2 : 4;
  const Square from   = LSB(PieceBB(board, KING, stm));

  for (int rk = rookStart; rk < rookEnd; rk++) {
    if (!CanCastle(board, CASTLE_MAP[rk][0]))
      continue;

    Square rookFrom = board->cr[rk];
    if (GetBit(board->pinned, rookFrom))
      continue;

    Square to = CASTLE_MAP[rk][1], rookTo = CASTLE_MAP[rk][2];

    BitBoard kingCrossing = BetweenSquares(from, to) | Bit(to);
    BitBoard rookCrossing = BetweenSquares(rookFrom, rookTo) | Bit(rookTo);
    BitBoard between      = kingCrossing | rookCrossing;

    if (!((OccBB(board, BOTH) ^ Bit(from) ^ Bit(rookFrom)) & between))
      if (!(kingCrossing & board->threatened))
        moves = AddMove(moves, from, to, MakePiece(KING, stm), CASTLE_FLAG);
  }

  return moves;
}

INLINE ScoredMove* AddQuietChecks(ScoredMove* moves, Board* board, const Color stm) {
  const Color xstm          = !stm;
  const Square oppKingSq    = LSB(PieceBB(board, KING, xstm));
  const BitBoard bishopMask = GetBishopAttacks(oppKingSq, OccBB(board, BOTH));
  const BitBoard rookMask   = GetRookAttacks(oppKingSq, OccBB(board, BOTH));

  moves = AddPieceMoves(moves, bishopMask | rookMask, board, stm, GT_QUIET, QUEEN);
  moves = AddPieceMoves(moves, rookMask, board, stm, GT_QUIET, ROOK);
  moves = AddPieceMoves(moves, bishopMask, board, stm, GT_QUIET, BISHOP);
  moves = AddPieceMoves(moves, GetKnightAttacks(oppKingSq), board, stm, GT_QUIET, KNIGHT);

  return AddPawnMoves(moves, GetPawnAttacks(oppKingSq, xstm), board, stm, GT_QUIET);
}

INLINE ScoredMove* AddPseudoLegalMoves(ScoredMove* moves, Board* board, const int type, const Color color) {
  if (BitCount(board->checkers) > 1)
    return AddPieceMoves(moves, ~board->threatened, board, color, type, KING);

  BitBoard opts =
    !board->checkers ? ALL : BetweenSquares(LSB(PieceBB(board, KING, color)), LSB(board->checkers)) | board->checkers;

  moves = AddPawnMoves(moves, opts, board, color, type);
  moves = AddPieceMoves(moves, opts, board, color, type, KNIGHT);
  moves = AddPieceMoves(moves, opts, board, color, type, BISHOP);
  moves = AddPieceMoves(moves, opts, board, color, type, ROOK);
  moves = AddPieceMoves(moves, opts, board, color, type, QUEEN);
  moves = AddPieceMoves(moves, ~board->threatened, board, color, type, KING);
  if ((type & GT_QUIET) && !board->checkers)
    moves = AddCastles(moves, board, color);

  return moves;
}

INLINE ScoredMove* AddLegalMoves(ScoredMove* moves, Board* board, const Color color) {
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

// Kept out of line: each one expands to a full generator per colour, and
// inlining them into NextMove multiplied its size several times over.
ScoredMove* AddNoisyMoves(ScoredMove* moves, Board* board);

ScoredMove* AddQuietMoves(ScoredMove* moves, Board* board);

INLINE ScoredMove* AddEvasionMoves(ScoredMove* moves, Board* board) {
  return board->stm == WHITE ? AddPseudoLegalMoves(moves, board, GT_LEGAL, WHITE) : //
                               AddPseudoLegalMoves(moves, board, GT_LEGAL, BLACK);
}

ScoredMove* AddQuietCheckMoves(ScoredMove* moves, Board* board);

// every legal move, in generation order
void RootMoves(SimpleMoveList* moves, Board* board);

INLINE ScoredMove* AddPerftMoves(ScoredMove* moves, Board* board) {
  return board->stm == WHITE ? AddLegalMoves(moves, board, WHITE) : //
                               AddLegalMoves(moves, board, BLACK);
}

#endif

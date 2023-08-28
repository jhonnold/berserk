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

// Note: Any usage of & that matches 1 or 2 will match 3
//       thus making GT_LEGAL generate all.
enum {
  GT_QUIET   = 0b01,
  GT_CAPTURE = 0b10,
  GT_LEGAL   = 0b11,
};

INLINE ScoredMove* AddMove(ScoredMove* moves, int from, int to, int moving, int promo, int flags) {
  *moves++ = (ScoredMove) {.move = BuildMove(from, to, moving, promo, flags), .score = 0};
  return moves;
}

INLINE ScoredMove*
AddPromotions(ScoredMove* moves, int from, int to, int moving, int flags, const int stm, const int type) {
  if (type & GT_CAPTURE)
    moves = AddMove(moves, from, to, moving, Piece(QUEEN, stm), flags);

  if (type & GT_QUIET) {
    moves = AddMove(moves, from, to, moving, Piece(ROOK, stm), flags);
    moves = AddMove(moves, from, to, moving, Piece(BISHOP, stm), flags);
    moves = AddMove(moves, from, to, moving, Piece(KNIGHT, stm), flags);
  }

  return moves;
}

INLINE ScoredMove* AddPawnMoves(ScoredMove* moves,
                                BitBoard movers,
                                BitBoard opts,
                                Board* board,
                                const int stm,
                                const int type) {
  const int xstm = stm ^ 1;

  if (type & GT_QUIET) {
    // Quiet non-promotions
    BitBoard valid     = movers & ~PromoRank(stm);
    BitBoard targets   = ShiftPawnDir(valid, stm) & ~OccBB(BOTH);
    BitBoard dpTargets = ShiftPawnDir(targets & DPRank(stm), stm) & ~OccBB(BOTH);

    targets &= opts, dpTargets &= opts;

    while (targets) {
      int to = PopLSB(&targets);
      moves  = AddMove(moves, to - PawnDir(stm), to, Piece(PAWN, stm), NO_PROMO, QUIET);
    }

    while (dpTargets) {
      int to = PopLSB(&dpTargets);
      moves  = AddMove(moves, to - PawnDir(stm) - PawnDir(stm), to, Piece(PAWN, stm), NO_PROMO, DP);
    }
  }

  if (type & GT_CAPTURE) {
    // Captures non-promotions
    BitBoard valid    = movers & ~PromoRank(stm);
    BitBoard eTargets = ShiftPawnCapE(valid, stm) & OccBB(xstm) & opts;
    BitBoard wTargets = ShiftPawnCapW(valid, stm) & OccBB(xstm) & opts;

    while (eTargets) {
      int to = PopLSB(&eTargets);
      moves  = AddMove(moves, to - (PawnDir(stm) + E), to, Piece(PAWN, stm), NO_PROMO, CAPTURE);
    }

    while (wTargets) {
      int to = PopLSB(&wTargets);
      moves  = AddMove(moves, to - (PawnDir(stm) + W), to, Piece(PAWN, stm), NO_PROMO, CAPTURE);
    }

    if (board->epSquare) {
      BitBoard epMovers = GetPawnAttacks(board->epSquare, xstm) & valid;

      while (epMovers) {
        int from = PopLSB(&epMovers);
        moves    = AddMove(moves, from, board->epSquare, Piece(PAWN, stm), NO_PROMO, EP);
      }
    }
  }

  // Promotions (both capture and non-capture)
  BitBoard valid    = movers & PromoRank(stm);
  BitBoard sTargets = ShiftPawnDir(valid, stm) & ~OccBB(BOTH) & opts;
  BitBoard eTargets = ShiftPawnCapE(valid, stm) & OccBB(xstm) & opts;
  BitBoard wTargets = ShiftPawnCapW(valid, stm) & OccBB(xstm) & opts;

  while (sTargets) {
    int to = PopLSB(&sTargets);
    moves  = AddPromotions(moves, to - PawnDir(stm), to, Piece(PAWN, stm), QUIET, stm, type);
  }

  while (eTargets) {
    int to = PopLSB(&eTargets);
    moves  = AddPromotions(moves, to - (PawnDir(stm) + E), to, Piece(PAWN, stm), CAPTURE, stm, type);
  }

  while (wTargets) {
    int to = PopLSB(&wTargets);
    moves  = AddPromotions(moves, to - (PawnDir(stm) + W), to, Piece(PAWN, stm), CAPTURE, stm, type);
  }

  return moves;
}

INLINE ScoredMove* AddPieceMoves(ScoredMove* moves,
                                 BitBoard movers,
                                 BitBoard opts,
                                 Board* board,
                                 const int stm,
                                 const int type,
                                 const int piece) {
  const int xstm = stm ^ 1;

  while (movers) {
    int from = PopLSB(&movers);

    BitBoard valid = GetPieceAttacks(from, OccBB(BOTH), piece) & opts;
    BitBoard targets = type == GT_QUIET ? valid & ~OccBB(BOTH) : //
                       type == GT_CAPTURE ? valid & OccBB(xstm) : //
                       valid & ~OccBB(stm);

    while (targets) {
      int to = PopLSB(&targets);
      int flags = type == GT_QUIET ? QUIET : //
                  type == GT_CAPTURE ? CAPTURE : //
                  GetBit(OccBB(xstm), to) ? CAPTURE : QUIET;

      moves = AddMove(moves, from, to, Piece(piece, stm), NO_PROMO, flags);
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

    BitBoard between = BetweenSquares(from, to) | BetweenSquares(rookFrom, rookTo) | Bit(to) | Bit(rookTo);
    if (!((OccBB(BOTH) ^ Bit(from) ^ Bit(rookFrom)) & between))
      moves = AddMove(moves, from, to, Piece(KING, stm), NO_PROMO, CASTLE);
  }

  return moves;
}

INLINE ScoredMove* AddQuietChecks(ScoredMove* moves, Board* board, const int stm) {
  const int xstm            = !stm;
  const int oppKingSq       = LSB(PieceBB(KING, xstm));
  const BitBoard bishopMask = GetBishopAttacks(oppKingSq, OccBB(BOTH));
  const BitBoard rookMask   = GetRookAttacks(oppKingSq, OccBB(BOTH));
  const BitBoard queenMask  = bishopMask | rookMask;

  const BitBoard checks[6] = {
    GetPawnAttacks(oppKingSq, xstm),
    GetKnightAttacks(oppKingSq),
    bishopMask,
    rookMask,
    queenMask,
    0,
  };

  moves = AddPieceMoves(moves, PieceBB(QUEEN, stm), queenMask, board, stm, GT_QUIET, QUEEN);
  moves = AddPieceMoves(moves, PieceBB(ROOK, stm), rookMask, board, stm, GT_QUIET, ROOK);
  moves = AddPieceMoves(moves, PieceBB(BISHOP, stm), bishopMask, board, stm, GT_QUIET, BISHOP);
  moves = AddPieceMoves(moves, PieceBB(KNIGHT, stm), checks[KNIGHT], board, stm, GT_QUIET, KNIGHT);
  moves = AddPawnMoves(moves, PieceBB(PAWN, stm), checks[PAWN], board, stm, GT_QUIET);

  const BitBoard discoveryBlockers = queenMask & OccBB(stm);
  BitBoard discoveryPieces = (GetBishopAttacks(oppKingSq, OccBB(BOTH) ^ discoveryBlockers) & PieceBB(BISHOP, stm)) |
                             (GetRookAttacks(oppKingSq, OccBB(BOTH) ^ discoveryBlockers) & PieceBB(ROOK, stm));

  while (discoveryPieces) {
    const int discoveryFrom = PopLSB(&discoveryPieces);
    const BitBoard between  = BetweenSquares(discoveryFrom, oppKingSq);
    const int from          = LSB(between & discoveryBlockers);
    const int pc            = PieceType(board->squares[from]);

    if (pc != PAWN) {
      const BitBoard opts = ~(between | OccBB(BOTH) | checks[pc]);
      moves               = AddPieceMoves(moves, Bit(from), opts, board, stm, GT_QUIET, pc);
    } else {
      const BitBoard opts = ~(between | OccBB(BOTH) | RANK_1 | RANK_8);
      moves               = AddPawnMoves(moves, Bit(from), opts, board, stm, GT_QUIET);
    }
  }

  return moves;
}

INLINE ScoredMove* AddPseudoLegalMoves(ScoredMove* moves, Board* board, const int type, const int color) {
  if (BitCount(board->checkers) > 1)
    return AddPieceMoves(moves, PieceBB(KING, color), ALL, board, color, type, KING);

  BitBoard opts =
    !board->checkers ? ALL : BetweenSquares(LSB(PieceBB(KING, color)), LSB(board->checkers)) | board->checkers;

  moves = AddPawnMoves(moves, PieceBB(PAWN, color), opts, board, color, type);
  moves = AddPieceMoves(moves, PieceBB(KNIGHT, color), opts, board, color, type, KNIGHT);
  moves = AddPieceMoves(moves, PieceBB(BISHOP, color), opts, board, color, type, BISHOP);
  moves = AddPieceMoves(moves, PieceBB(ROOK, color), opts, board, color, type, ROOK);
  moves = AddPieceMoves(moves, PieceBB(QUEEN, color), opts, board, color, type, QUEEN);
  moves = AddPieceMoves(moves, PieceBB(KING, color), ALL, board, color, type, KING);
  if ((type & GT_QUIET) && !board->checkers)
    moves = AddCastles(moves, board, color);

  return moves;
}

INLINE ScoredMove* AddLegalMoves(ScoredMove* moves, Board* board, const int color) {
  ScoredMove* curr = moves;
  BitBoard pinned  = board->pinned;
  int king         = LSB(PieceBB(KING, board->stm));

  moves = AddPseudoLegalMoves(moves, board, GT_LEGAL, color);

  while (curr != moves) {
    if (((pinned && GetBit(pinned, From(curr->move))) || From(curr->move) == king || IsEP(curr->move)) &&
        !IsLegal(curr->move, board))
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
ScoredMove* AddPerftMoves(ScoredMove* moves, Board* board);

#endif

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

#include "tb.h"

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "move.h"
#include "movegen.h"
#include "pyrrhic/tbprobe.h"

#define ByteSwap(bb) __builtin_bswap64((bb))

// Pyrrhic's eight board arguments: both colours, then each piece type, in a1 = 0 order
#define TB_PIECES(board)                                                     \
  ByteSwap(OccBB(board, WHITE)), ByteSwap(OccBB(board, BLACK)),              \
    ByteSwap(PieceBB(board, KING, WHITE) | PieceBB(board, KING, BLACK)),     \
    ByteSwap(PieceBB(board, QUEEN, WHITE) | PieceBB(board, QUEEN, BLACK)),   \
    ByteSwap(PieceBB(board, ROOK, WHITE) | PieceBB(board, ROOK, BLACK)),     \
    ByteSwap(PieceBB(board, BISHOP, WHITE) | PieceBB(board, BISHOP, BLACK)), \
    ByteSwap(PieceBB(board, KNIGHT, WHITE) | PieceBB(board, KNIGHT, BLACK)), \
    ByteSwap(PieceBB(board, PAWN, WHITE) | PieceBB(board, PAWN, BLACK))

INLINE Move TBMoveFromResult(unsigned res, Board* board) {
  unsigned from  = TB_GET_FROM(res) ^ 56;
  unsigned to    = TB_GET_TO(res) ^ 56;
  unsigned ep    = TB_GET_EP(res);
  unsigned promo = TB_GET_PROMOTES(res);
  Piece piece    = board->squares[from];
  Bool capture   = board->squares[to] != NO_PIECE;

  int flags = QUIET_FLAG;
  if (ep)
    flags = EP_FLAG;
  else if (capture)
    flags = CAPTURE_FLAG;

  if (promo) {
    switch (KING - promo) {
      case KNIGHT: flags |= KNIGHT_PROMO_FLAG; break;
      case BISHOP: flags |= BISHOP_PROMO_FLAG; break;
      case ROOK: flags |= ROOK_PROMO_FLAG; break;
      case QUEEN: flags |= QUEEN_PROMO_FLAG; break;
    }
  }

  return BuildMove(from, to, piece, flags);
}

static unsigned TBRootProbe(Board* board, unsigned* results);

void TBRootMoves(SimpleMoveList* moves, Board* board) {
  moves->count = 0;

  unsigned results[MAX_MOVES];
  unsigned result = TBRootProbe(board, results);

  if (result == TB_RESULT_FAILED || result == TB_RESULT_CHECKMATE || result == TB_RESULT_STALEMATE)
    return;

  const unsigned wdl = TB_GET_WDL(result);

  for (int i = 0; i < MAX_MOVES && results[i] != TB_RESULT_FAILED; i++)
    if (wdl == TB_GET_WDL(results[i]))
      moves->moves[moves->count++] = TBMoveFromResult(results[i], board);
}

static unsigned TBRootProbe(Board* board, unsigned* results) {
  if (board->castling || BitCount(OccBB(board, BOTH)) > TB_LARGEST)
    return TB_RESULT_FAILED;

  return tb_probe_root(TB_PIECES(board),
                       board->fmr,
                       board->epSquare ? (board->epSquare ^ 56) : 0,
                       board->stm == WHITE ? 1 : 0,
                       results);
}

unsigned TBProbe(Board* board) {
  if (board->castling || board->fmr || BitCount(OccBB(board, BOTH)) > TB_LARGEST)
    return TB_RESULT_FAILED;

  return tb_probe_wdl(TB_PIECES(board), board->epSquare ? (board->epSquare ^ 56) : 0, board->stm == WHITE ? 1 : 0);
}
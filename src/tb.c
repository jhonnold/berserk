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

#include "tb.h"

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "move.h"
#include "movegen.h"
#include "pyrrhic/tbprobe.h"

#define ByteSwap(bb) __builtin_bswap64((bb))

Move TBRootProbe(Board* board) {
  if (board->castling || BitCount(OccBB(BOTH)) > TB_LARGEST)
    return NULL_MOVE;

  unsigned results[MAX_MOVES]; // used for native support
  unsigned res = tb_probe_root(ByteSwap(OccBB(WHITE)),
                               ByteSwap(OccBB(BLACK)),
                               ByteSwap(PieceBB(KING, WHITE) | PieceBB(KING, BLACK)),
                               ByteSwap(PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK)),
                               ByteSwap(PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK)),
                               ByteSwap(PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK)),
                               ByteSwap(PieceBB(KNIGHT, WHITE) | PieceBB(KNIGHT, BLACK)),
                               ByteSwap(PieceBB(PAWN, WHITE) | PieceBB(PAWN, BLACK)),
                               board->fmr,
                               board->epSquare ? (board->epSquare ^ 56) : 0,
                               board->stm == WHITE ? 1 : 0,
                               results);

  if (res == TB_RESULT_FAILED || res == TB_RESULT_STALEMATE || res == TB_RESULT_CHECKMATE)
    return NULL_MOVE;

  unsigned from  = TB_GET_FROM(res) ^ 56;
  unsigned to    = TB_GET_TO(res) ^ 56;
  unsigned ep    = TB_GET_EP(res);
  unsigned promo = TB_GET_PROMOTES(res);
  int piece      = board->squares[from];
  int capture    = board->squares[to] != NO_PIECE;
  int promoPiece = promo ? Piece(KING - promo, board->stm) : 0;

  int flags = QUIET;
  if (ep)
    flags = EP;
  else if (capture)
    flags = CAPTURE;
  else if (PieceType(piece) == PAWN && (from ^ to) == 16)
    flags = DP;

  return BuildMove(from, to, piece, promoPiece, flags);
}

unsigned TBProbe(Board* board) {
  if (board->castling || board->fmr || BitCount(OccBB(BOTH)) > TB_LARGEST)
    return TB_RESULT_FAILED;

  return tb_probe_wdl(ByteSwap(OccBB(WHITE)),
                      ByteSwap(OccBB(BLACK)),
                      ByteSwap(PieceBB(KING, WHITE) | PieceBB(KING, BLACK)),
                      ByteSwap(PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK)),
                      ByteSwap(PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK)),
                      ByteSwap(PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK)),
                      ByteSwap(PieceBB(KNIGHT, WHITE) | PieceBB(KNIGHT, BLACK)),
                      ByteSwap(PieceBB(PAWN, WHITE) | PieceBB(PAWN, BLACK)),
                      board->epSquare ? (board->epSquare ^ 56) : 0,
                      board->stm == WHITE ? 1 : 0);
}
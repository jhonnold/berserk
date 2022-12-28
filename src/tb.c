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

#include "tb.h"

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "move.h"
#include "movegen.h"
#include "pyrrhic/tbprobe.h"

#define vf(bb) __builtin_bswap64((bb))

const int TB_PROMO_MAP[5] = {
  NORMAL,
  QUEEN_PROMO,
  ROOK_PROMO,
  BISHOP_PROMO,
  ROOK_PROMO,
};

Move TBRootProbe(Board* board) {
  if (board->castling || bits(OccBB(BOTH)) > TB_LARGEST) return NULL_MOVE;

  unsigned results[MAX_MOVES]; // used for native support
  unsigned res = tb_probe_root(vf(OccBB(WHITE)),
                               vf(OccBB(BLACK)),
                               vf(PieceBB(KING, WHITE) | PieceBB(KING, BLACK)),
                               vf(PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK)),
                               vf(PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK)),
                               vf(PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK)),
                               vf(PieceBB(KNIGHT, WHITE) | PieceBB(KNIGHT, BLACK)),
                               vf(PieceBB(PAWN, WHITE) | PieceBB(PAWN, BLACK)),
                               board->halfMove,
                               board->epSquare ? (board->epSquare ^ 56) : 0,
                               board->stm == WHITE ? 1 : 0,
                               results);

  if (res == TB_RESULT_FAILED || res == TB_RESULT_STALEMATE || res == TB_RESULT_CHECKMATE) return NULL_MOVE;

  unsigned from  = TB_GET_FROM(res) ^ 56;
  unsigned to    = TB_GET_TO(res) ^ 56;
  unsigned ep    = TB_GET_EP(res);
  unsigned promo = TB_GET_PROMOTES(res);

  if (ep) return BuildMove(from, to, EP);
  if (promo) return BuildMove(from, to, TB_PROMO_MAP[promo]);

  return BuildMove(from, to, board->squares[to] != NO_PIECE ? NORMAL_CAPTURE : NORMAL);
}

unsigned TBProbe(Board* board) {
  if (board->castling || board->halfMove || bits(OccBB(BOTH)) > TB_LARGEST) return TB_RESULT_FAILED;

  return tb_probe_wdl(vf(OccBB(WHITE)),
                      vf(OccBB(BLACK)),
                      vf(PieceBB(KING, WHITE) | PieceBB(KING, BLACK)),
                      vf(PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK)),
                      vf(PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK)),
                      vf(PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK)),
                      vf(PieceBB(KNIGHT, WHITE) | PieceBB(KNIGHT, BLACK)),
                      vf(PieceBB(PAWN, WHITE) | PieceBB(PAWN, BLACK)),
                      board->epSquare ? (board->epSquare ^ 56) : 0,
                      board->stm == WHITE ? 1 : 0);
}
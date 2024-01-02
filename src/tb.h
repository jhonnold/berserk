// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

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

#ifndef TB_H
#define TB_H

#include "move.h"
#include "movegen.h"
#include "pyrrhic/tbprobe.h"
#include "types.h"
#include "util.h"

INLINE Move TBMoveFromResult(unsigned res, Board* board) {
  unsigned from  = TB_GET_FROM(res) ^ 56;
  unsigned to    = TB_GET_TO(res) ^ 56;
  unsigned ep    = TB_GET_EP(res);
  unsigned promo = TB_GET_PROMOTES(res);
  int piece      = board->squares[from];
  int capture    = board->squares[to] != NO_PIECE;

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

void TBRootMoves(SimpleMoveList* moves, Board* board);
unsigned TBRootProbe(Board* board, unsigned* results);
unsigned TBProbe(Board* board);

#endif
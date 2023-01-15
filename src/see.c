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

#include "see.h"

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "move.h"
#include "movegen.h"
#include "types.h"
#include "util.h"

const int SEE_VALUE[7] = {100, 422, 422, 642, 1015, 30000, 0};

// Static exchange evaluation using The Swap Algorithm -
// https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
inline int SEE(Board* board, Move move, int threshold) {
  if (IsCas(move) || IsEP(move) || Promo(move))
    return 1;

  int from = From(move);
  int to   = To(move);

  int v = SEE_VALUE[PieceType(board->squares[to])] - threshold;
  if (v < 0)
    return 0;

  v -= SEE_VALUE[PieceType(Moving(move))];
  if (v >= 0)
    return 1;

  BitBoard occ       = (OccBB(BOTH) ^ bit(from)) | bit(to);
  BitBoard attackers = AttacksToSquare(board, to, occ);

  BitBoard diag     = PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK) | PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK);
  BitBoard straight = PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK) | PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK);

  int stm = board->xstm;
  while (1) {
    attackers &= occ;

    BitBoard mine = attackers & OccBB(stm);
    if (!mine)
      break;

    int piece = PAWN;
    for (piece = PAWN; piece < KING; piece++)
      if (mine & PieceBB(piece, stm))
        break;

    stm ^= 1;

    if ((v = -v - 1 - SEE_VALUE[piece]) >= 0) {
      if (piece == KING && (attackers & OccBB(stm)))
        stm ^= 1;

      break;
    }

    occ ^= bit(lsb(mine & PieceBB(piece, stm ^ 1)));

    if (piece == PAWN || piece == BISHOP || piece == QUEEN)
      attackers |= GetBishopAttacks(to, occ) & diag;
    if (piece == ROOK || piece == QUEEN)
      attackers |= GetRookAttacks(to, occ) & straight;
  }

  return stm != board->stm;
}

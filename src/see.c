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
  if (IsCas(move) || IsEP(move) || IsPromo(move))
    return 1;

  int from = From(move);
  int to   = To(move);

  int v = SEE_VALUE[PieceType(board->squares[to])] - threshold;
  if (v < 0)
    return 0;

  v = SEE_VALUE[PieceType(Moving(move))] - v;
  if (v <= 0)
    return 1;

  int stm            = board->stm;
  BitBoard occ       = OccBB(BOTH) ^ Bit(from) ^ Bit(to);
  BitBoard attackers = AttacksToSquare(board, to, occ);
  BitBoard mine, leastAttacker;

  const BitBoard diag = PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK) | PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK);
  const BitBoard straight = PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK) | PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK);

  int result = 1;

  while (1) {
    stm ^= 1;
    attackers &= occ;

    if (!(mine = (attackers & OccBB(stm))))
      break;

    result ^= 1;

    if ((leastAttacker = mine & PieceBB(PAWN, stm))) {
      if ((v = SEE_VALUE[PAWN] - v) < result)
        break;

      occ ^= (leastAttacker & -leastAttacker);
      attackers |= GetBishopAttacks(to, occ) & diag;
    } else if ((leastAttacker = mine & PieceBB(KNIGHT, stm))) {
      if ((v = SEE_VALUE[KNIGHT] - v) < result)
        break;

      occ ^= (leastAttacker & -leastAttacker);
    } else if ((leastAttacker = mine & PieceBB(BISHOP, stm))) {
      if ((v = SEE_VALUE[BISHOP] - v) < result)
        break;

      occ ^= (leastAttacker & -leastAttacker);
      attackers |= GetBishopAttacks(to, occ) & diag;
    } else if ((leastAttacker = mine & PieceBB(ROOK, stm))) {
      if ((v = SEE_VALUE[ROOK] - v) < result)
        break;

      occ ^= (leastAttacker & -leastAttacker);
      attackers |= GetRookAttacks(to, occ) & straight;
    } else if ((leastAttacker = mine & PieceBB(QUEEN, stm))) {
      if ((v = SEE_VALUE[QUEEN] - v) < result)
        break;

      occ ^= (leastAttacker & -leastAttacker);
      attackers |= (GetBishopAttacks(to, occ) & diag) | (GetRookAttacks(to, occ) & straight);
    } else
      return (attackers & ~OccBB(stm)) ? result ^ 1 : result;
  }

  return result;
}

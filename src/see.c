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

#include "see.h"

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "move.h"
#include "movegen.h"
#include "tune.h"
#include "types.h"

#ifdef SPSA
int SEE_VALUE[7];

// SEE_VALUE is built from tunables, so it is refreshed whenever one is set
void InitSeeValues(void) {
  const int values[7] = {SEE_PAWN, SEE_KNIGHT, SEE_BISHOP, SEE_ROOK, SEE_QUEEN, 30000, 0};

  for (int i = 0; i < 7; i++)
    SEE_VALUE[i] = values[i];
}
#else
const int SEE_VALUE[7] = {SEE_PAWN, SEE_KNIGHT, SEE_BISHOP, SEE_ROOK, SEE_QUEEN, 30000, 0};
#endif

// Static exchange evaluation using The Swap Algorithm -
// https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
Bool SEE(Board* board, Move move, Score threshold) {
  if (IsCas(move) || IsEP(move) || IsPromo(move))
    return 1;

  Square from = From(move);
  Square to   = To(move);

  int v = SEE_VALUE[TypeOf(board->squares[to])] - threshold;
  if (v < 0)
    return 0;

  v = SEE_VALUE[TypeOf(Moving(move))] - v;
  if (v <= 0)
    return 1;

  Color stm          = board->stm;
  BitBoard occ       = OccBB(board, BOTH) ^ Bit(from) ^ Bit(to);
  BitBoard attackers = AttacksToSquare(board, to, occ);
  BitBoard mine, leastAttacker;

  const BitBoard diag = PieceBB(board, BISHOP, WHITE) | PieceBB(board, BISHOP, BLACK) | PieceBB(board, QUEEN, WHITE) |
                        PieceBB(board, QUEEN, BLACK);
  const BitBoard straight = PieceBB(board, ROOK, WHITE) | PieceBB(board, ROOK, BLACK) | PieceBB(board, QUEEN, WHITE) |
                            PieceBB(board, QUEEN, BLACK);

  int result = 1;

  while (1) {
    stm ^= 1;
    attackers &= occ;

    if (!(mine = (attackers & OccBB(board, stm))))
      break;

    result ^= 1;

    if ((leastAttacker = mine & PieceBB(board, PAWN, stm))) {
      if ((v = SEE_VALUE[PAWN] - v) < result)
        break;

      occ ^= (leastAttacker & -leastAttacker);
      attackers |= GetBishopAttacks(to, occ) & diag;
    } else if ((leastAttacker = mine & PieceBB(board, KNIGHT, stm))) {
      if ((v = SEE_VALUE[KNIGHT] - v) < result)
        break;

      occ ^= (leastAttacker & -leastAttacker);
    } else if ((leastAttacker = mine & PieceBB(board, BISHOP, stm))) {
      if ((v = SEE_VALUE[BISHOP] - v) < result)
        break;

      occ ^= (leastAttacker & -leastAttacker);
      attackers |= GetBishopAttacks(to, occ) & diag;
    } else if ((leastAttacker = mine & PieceBB(board, ROOK, stm))) {
      if ((v = SEE_VALUE[ROOK] - v) < result)
        break;

      occ ^= (leastAttacker & -leastAttacker);
      attackers |= GetRookAttacks(to, occ) & straight;
    } else if ((leastAttacker = mine & PieceBB(board, QUEEN, stm))) {
      if ((v = SEE_VALUE[QUEEN] - v) < result)
        break;

      occ ^= (leastAttacker & -leastAttacker);
      attackers |= (GetBishopAttacks(to, occ) & diag) | (GetRookAttacks(to, occ) & straight);
    } else
      return (attackers & ~OccBB(board, stm)) ? result ^ 1 : result;
  }

  return result;
}

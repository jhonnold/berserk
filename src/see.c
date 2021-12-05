// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

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

const int STATIC_MATERIAL_VALUE[7] = {100, 565, 565, 705, 1000, 30000, 0};

// Static exchange evaluation using The Swap Algorithm - https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
inline int SEE(Board* board, Move move) {
  if (IsCas(move) || (!IsCap(move) && PieceType(Moving(move)) == KING)) return 0;

  BitBoard occupied = OccBB(BOTH);
  int stm = board->stm;

  int gain[32];
  int captureCount = 1;

  int from = From(move);
  int to = To(move);

  BitBoard attackers = AttacksToSquare(board, to, OccBB(BOTH));
  int attackedPieceVal =
      IsEP(move) ? STATIC_MATERIAL_VALUE[PAWN] : STATIC_MATERIAL_VALUE[PieceType(board->squares[To(move)])];
  popBit(occupied, from);
  if (IsEP(move)) popBit(occupied, to - PawnDir(stm));

  stm ^= 1;
  gain[0] = attackedPieceVal;

  int piece = Moving(move);
  attackedPieceVal = STATIC_MATERIAL_VALUE[PieceType(piece)];

  // Recalculate attacks if xray now open
  if (PieceType(piece) == PAWN || PieceType(piece) == BISHOP || PieceType(piece) == QUEEN)
    attackers |= GetBishopAttacks(to, occupied) &
                 (PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK) | PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK));

  // Recalculate attacks if xray now open
  if (PieceType(piece) == ROOK || PieceType(piece) == QUEEN)
    attackers |= GetRookAttacks(to, occupied) &
                 (PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK) | PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK));

  BitBoard attackee = 0;
  attackers &= occupied;

  while (attackers) {
    for (piece = Piece(PAWN, stm); piece <= Piece(KING, stm); piece += 2)
      if ((attackee = board->pieces[piece] & attackers)) break;

    if (piece > BLACK_KING) break;

    occupied ^= (attackee & -attackee);

    // Recalculate attacks if xray now open
    if (PieceType(piece) == PAWN || PieceType(piece) == BISHOP || PieceType(piece) == QUEEN)
      attackers |= GetBishopAttacks(to, occupied) &
                   (PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK) | PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK));

    // Recalculate attacks if xray now open
    if (PieceType(piece) == ROOK || PieceType(piece) == QUEEN)
      attackers |= GetRookAttacks(to, occupied) &
                   (PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK) | PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK));

    gain[captureCount] = -gain[captureCount - 1] + attackedPieceVal;
    attackedPieceVal = STATIC_MATERIAL_VALUE[PieceType(piece)];

    // Stand pat if the capture is not good
    if (gain[captureCount++] - attackedPieceVal > 0) break;

    stm ^= 1;
    attackers &= occupied;
  }

  while (--captureCount) gain[captureCount - 1] = -max(-gain[captureCount - 1], gain[captureCount]);

  return gain[0];
}

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
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "types.h"
#include "util.h"

// Static exchange evaluation using The Swap Algorithm - https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
inline int SEE(Board* board, Move move) {
  if (MoveCastle(move) || (!MoveCapture(move) && PIECE_TYPE[MovePiece(move)] == KING_TYPE))
    return 0;

  BitBoard occupied = board->occupancies[BOTH];
  int side = board->side;

  int gain[32];
  int captureCount = 1;

  int start = MoveStart(move);
  int end = MoveEnd(move);

  BitBoard attackers = AttacksToSquare(board, end, board->occupancies[BOTH]);
  int attackedPieceVal = MoveEP(move) ? STATIC_MATERIAL_VALUE[PAWN_TYPE]
                                      : STATIC_MATERIAL_VALUE[PIECE_TYPE[board->squares[MoveEnd(move)]]];
  popBit(occupied, start);
  if (MoveEP(move))
    popBit(occupied, end - PAWN_DIRECTIONS[side]);

  side ^= 1;
  gain[0] = attackedPieceVal;

  int piece = MovePiece(move);
  attackedPieceVal = STATIC_MATERIAL_VALUE[PIECE_TYPE[piece]];

  // Recalculate attacks if xray now open
  if (PIECE_TYPE[piece] == PAWN_TYPE || PIECE_TYPE[piece] == BISHOP_TYPE || PIECE_TYPE[piece] == QUEEN_TYPE)
    attackers |= GetBishopAttacks(end, occupied) & (board->pieces[BISHOP[WHITE]] | board->pieces[BISHOP[BLACK]] |
                                                    board->pieces[QUEEN[WHITE]] | board->pieces[QUEEN[BLACK]]);

  // Recalculate attacks if xray now open
  if (PIECE_TYPE[piece] == ROOK_TYPE || PIECE_TYPE[piece] == QUEEN_TYPE)
    attackers |= GetRookAttacks(end, occupied) & (board->pieces[ROOK[WHITE]] | board->pieces[ROOK[BLACK]] |
                                                  board->pieces[QUEEN[WHITE]] | board->pieces[QUEEN[BLACK]]);

  BitBoard attackee = 0;
  attackers &= occupied;

  while (attackers) {
    for (piece = PAWN[side]; piece <= KING[side]; piece += 2)
      if ((attackee = board->pieces[piece] & attackers))
        break;

    if (piece > KING[BLACK])
      break;

    occupied ^= (attackee & -attackee);

    // Recalculate attacks if xray now open
    if (PIECE_TYPE[piece] == PAWN_TYPE || PIECE_TYPE[piece] == BISHOP_TYPE || PIECE_TYPE[piece] == QUEEN_TYPE)
      attackers |= GetBishopAttacks(end, occupied) & (board->pieces[BISHOP[WHITE]] | board->pieces[BISHOP[BLACK]] |
                                                      board->pieces[QUEEN[WHITE]] | board->pieces[QUEEN[BLACK]]);

    // Recalculate attacks if xray now open
    if (PIECE_TYPE[piece] == ROOK_TYPE || PIECE_TYPE[piece] == QUEEN_TYPE)
      attackers |= GetRookAttacks(end, occupied) & (board->pieces[ROOK[WHITE]] | board->pieces[ROOK[BLACK]] |
                                                    board->pieces[QUEEN[WHITE]] | board->pieces[QUEEN[BLACK]]);

    gain[captureCount] = -gain[captureCount - 1] + attackedPieceVal;
    attackedPieceVal = STATIC_MATERIAL_VALUE[PIECE_TYPE[piece]];

    // Stand pat if the capture is not good
    if (gain[captureCount++] - attackedPieceVal > 0)
      break;

    side ^= 1;
    attackers &= occupied;
  }

  while (--captureCount)
    gain[captureCount - 1] = -max(-gain[captureCount - 1], gain[captureCount]);

  return gain[0];
}

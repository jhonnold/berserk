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

#include "eval.h"

#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "nn.h"
#include "util.h"
#include "search.h"

const int PHASE_VALUES[6] = {0, 3, 3, 5, 10, 0};
const int MAX_PHASE = 64;

// "Threats" logic to be utilized in search
// idea originating in Koivisto
BitBoard Threats(Board* board, int stm) {
  int xstm = stm ^ 1;

  BitBoard threatened = 0;
  BitBoard opponentPieces = OccBB(xstm) & ~PieceBB(PAWN, xstm);

  BitBoard pawnAttacks = stm == WHITE ? ShiftNW(PieceBB(PAWN, WHITE)) | ShiftNE(PieceBB(PAWN, WHITE))
                                      : ShiftSW(PieceBB(PAWN, BLACK)) | ShiftSE(PieceBB(PAWN, BLACK));
  threatened |= pawnAttacks & opponentPieces;

  // remove minors
  opponentPieces &= ~(PieceBB(KNIGHT, xstm) | PieceBB(BISHOP, xstm));

  BitBoard knights = PieceBB(KNIGHT, stm);
  while (knights) threatened |= opponentPieces & GetKnightAttacks(popAndGetLsb(&knights));

  BitBoard bishops = PieceBB(BISHOP, stm);
  while (bishops) threatened |= opponentPieces & GetBishopAttacks(popAndGetLsb(&bishops), OccBB(BOTH));

  // remove rooks
  opponentPieces &= ~PieceBB(ROOK, xstm);

  BitBoard rooks = PieceBB(ROOK, stm);
  while (rooks) threatened |= opponentPieces & GetRookAttacks(popAndGetLsb(&rooks), OccBB(BOTH));

  return threatened;
}

// Main evalution method
Score Evaluate(Board* board) {
  if (IsMaterialDraw(board)) return 0;

  int score = OutputLayer(board->accumulators[board->stm][board->ply], board->accumulators[board->xstm][board->ply]);

  int scalar = 128 + board->phase;
  int scaled = scalar * score / 128;
  return min(TB_WIN_BOUND - 1, max(-TB_WIN_BOUND + 1, scaled));
}
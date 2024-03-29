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

#include "history.h"

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "move.h"
#include "util.h"

void UpdateHistories(SearchStack* ss,
                     ThreadData* thread,
                     Move bestMove,
                     int depth,
                     Move quiets[],
                     int nQ,
                     Move captures[],
                     int nC) {
  Board* board = &thread->board;
  int stm      = board->stm;

  int16_t inc = HistoryBonus(depth);

  if (!IsCap(bestMove)) {
    if (PromoPT(bestMove) != QUEEN) {
      AddKillerMove(ss, bestMove);

      if ((ss - 1)->move)
        AddCounterMove(thread, bestMove, (ss - 1)->move);
    }

    // Only increase the best move history when it
    // wasn't trivial. This idea was first thought of
    // by Alayan in Ethereal
    if (nQ > 1 || depth > 4) {
      AddHistoryHeuristic(&HH(stm, bestMove, board->threatened), inc);
      UpdateCH(ss, bestMove, inc);
    }
  } else {
    int piece    = Moving(bestMove);
    int to       = To(bestMove);
    int defended = !GetBit(board->threatened, to);
    int captured = IsEP(bestMove) ? PAWN : PieceType(board->squares[to]);

    AddHistoryHeuristic(&TH(piece, to, defended, captured), inc);
  }

  // Update quiets
  if (!IsCap(bestMove)) {
    for (int i = 0; i < nQ; i++) {
      Move m = quiets[i];
      if (m == bestMove)
        continue;

      AddHistoryHeuristic(&HH(stm, m, board->threatened), -inc);
      UpdateCH(ss, m, -inc);
    }
  }

  // Update captures
  for (int i = 0; i < nC; i++) {
    Move m = captures[i];
    if (m == bestMove)
      continue;

    int piece    = Moving(m);
    int to       = To(m);
    int defended = !GetBit(board->threatened, to);
    int captured = IsEP(m) ? PAWN : PieceType(board->squares[to]);

    AddHistoryHeuristic(&TH(piece, to, defended, captured), -inc);
  }
}

void UpdatePawnCorrection(int raw, int real, Board* board, ThreadData* thread) {
  const int16_t correction = Min(30000, Max(-30000, (real - raw) * PAWN_CORRECTION_GRAIN));
  const int idx            = (board->pawnZobrist & PAWN_CORRECTION_MASK);

  thread->pawnCorrection[idx] = (thread->pawnCorrection[idx] * 255 + correction) / 256;
}

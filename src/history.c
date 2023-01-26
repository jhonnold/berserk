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

#include "history.h"

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "move.h"
#include "util.h"

INLINE void AddKillerMove(SearchStack* ss, Move move) {
  if (ss->killers[0] != move) {
    ss->killers[1] = ss->killers[0];
    ss->killers[0] = move;
  }
}

INLINE void AddCounterMove(ThreadData* thread, Move move, Move parent) {
  thread->counters[Moving(parent)][To(parent)] = move;
}

INLINE void AddHistoryHeuristic(int16_t* entry, int16_t inc) {
  *entry += inc - *entry * abs(inc) / 16384;
}

INLINE void UpdateCH(SearchStack* ss, Move move, int16_t bonus) {
  if ((ss - 1)->move)
    AddHistoryHeuristic(&(*(ss - 1)->ch)[Moving(move)][To(move)], bonus);
  if ((ss - 2)->move)
    AddHistoryHeuristic(&(*(ss - 2)->ch)[Moving(move)][To(move)], bonus);
  if ((ss - 4)->move)
    AddHistoryHeuristic(&(*(ss - 4)->ch)[Moving(move)][To(move)], bonus);
}

void UpdateHistories(Board* board,
                     SearchStack* ss,
                     ThreadData* thread,
                     Move bestMove,
                     int depth,
                     int stm,
                     Move quiets[],
                     int nQ,
                     Move captures[],
                     int nC,
                     BitBoard threats) {
  int16_t inc = Min(1896, 4 * depth * depth + 120 * depth - 120);

  if (!IsCap(bestMove)) {
    AddKillerMove(ss, bestMove);
    AddHistoryHeuristic(&HH(stm, bestMove, threats), inc);
    UpdateCH(ss, bestMove, inc);

    if ((ss - 1)->move)
      AddCounterMove(thread, bestMove, (ss - 1)->move);

  } else {
    int piece    = PieceType(Moving(bestMove));
    int to       = To(bestMove);
    int captured = IsEP(bestMove) ? PAWN : PieceType(board->squares[to]);

    AddHistoryHeuristic(&TH(piece, to, captured), inc);
  }

  // Update quiets
  if (!IsCap(bestMove)) {
    for (int i = 0; i < nQ; i++) {
      Move m = quiets[i];
      if (m == bestMove)
        continue;

      AddHistoryHeuristic(&HH(stm, m, threats), -inc);
      UpdateCH(ss, m, -inc);
    }
  }

  // Update captures
  for (int i = 0; i < nC; i++) {
    Move m = captures[i];
    if (m == bestMove)
      continue;

    int piece    = PieceType(Moving(m));
    int to       = To(m);
    int captured = IsEP(m) ? PAWN : PieceType(board->squares[to]);

    AddHistoryHeuristic(&TH(piece, to, captured), -inc);
  }
}

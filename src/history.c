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

#include <stdlib.h>

#include "history.h"
#include "move.h"
#include "util.h"

void AddKillerMove(SearchData* data, Move move) {
  if (data->killers[data->ply][0] == move)
    data->killers[data->ply][1] = data->killers[data->ply][0];

  data->killers[data->ply][0] = move;
}

void AddCounterMove(SearchData* data, Move move, Move parent) { data->counters[MoveStartEnd(parent)] = move; }

void AddHistoryHeuristic(SearchData* data, Move move, int stm, int inc) {
  int* e = &data->hh[stm][MoveStartEnd(move)];

  *e += 64 * inc - *e * abs(inc) / 1024;
}

void UpdateHistories(SearchData* data, Move bestMove, int depth, int stm, MoveList* quiets) {
  int inc = min(depth * depth, 576);
  
  if (!Tactical(bestMove)) {
    AddKillerMove(data, bestMove);
    AddCounterMove(data, bestMove, data->moves[data->ply - 1]);
    AddHistoryHeuristic(data, bestMove, stm, inc);
  }

  for (int i = 0; i < quiets->count; i++)
    if (quiets->moves[i] != bestMove)
      AddHistoryHeuristic(data, quiets->moves[i], stm, -inc);
}
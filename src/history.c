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

#include "board.h"
#include "history.h"
#include "move.h"
#include "util.h"

void AddKillerMove(SearchData* data, Move move) {
  if (data->killers[data->ply][0] != move)
    data->killers[data->ply][1] = data->killers[data->ply][0];

  data->killers[data->ply][0] = move;
}

void AddCounterMove(SearchData* data, Move move, Move parent) { data->counters[MoveStartEnd(parent)] = move; }

void AddHistoryHeuristic(int* entry, int inc) { *entry += 64 * inc - *entry * abs(inc) / 1024; }

void UpdateHistories(SearchData* data, Move bestMove, int depth, int stm, Move quiets[], int nQ) {
  int inc = min(depth * depth, 576);

  Move parent = data->ply > 0 ? data->moves[data->ply - 1] : NULL_MOVE;
  Move grandParent = data->ply > 1 ? data->moves[data->ply - 2] : NULL_MOVE;

  if (!Tactical(bestMove)) {
    AddKillerMove(data, bestMove);
    AddHistoryHeuristic(&data->hh[stm][MoveStartEnd(bestMove)], inc);

    if (parent) {
      AddCounterMove(data, bestMove, parent);
      AddHistoryHeuristic(
          &data->ch[PIECE_TYPE[MovePiece(parent)]][MoveEnd(parent)][PIECE_TYPE[MovePiece(bestMove)]][MoveEnd(bestMove)],
          inc);
    }

    if (grandParent)
      AddHistoryHeuristic(&data->fh[PIECE_TYPE[MovePiece(grandParent)]][MoveEnd(grandParent)]
                                   [PIECE_TYPE[MovePiece(bestMove)]][MoveEnd(bestMove)],
                          inc);
  }

  for (int i = 0; i < nQ; i++) {
    Move m = quiets[i];
    if (m != bestMove) {
      AddHistoryHeuristic(&data->hh[stm][MoveStartEnd(m)], -inc);
      if (parent)
        AddHistoryHeuristic(
            &data->ch[PIECE_TYPE[MovePiece(parent)]][MoveEnd(parent)][PIECE_TYPE[MovePiece(m)]][MoveEnd(m)], -inc);
      if (grandParent)
        AddHistoryHeuristic(
            &data->fh[PIECE_TYPE[MovePiece(grandParent)]][MoveEnd(grandParent)][PIECE_TYPE[MovePiece(m)]][MoveEnd(m)],
            -inc);
    }
  }
}

int GetHistory(SearchData* data, Move move, int stm) {
  if (Tactical(move))
    return 0; // TODO: Capture history

  int history = data->hh[stm][MoveStartEnd(move)];

  Move parent = data->ply > 0 ? data->moves[data->ply - 1] : NULL_MOVE;
  if (parent)
    history += data->ch[PIECE_TYPE[MovePiece(parent)]][MoveEnd(parent)][PIECE_TYPE[MovePiece(move)]][MoveEnd(move)];

  Move grandParent = data->ply > 1 ? data->moves[data->ply - 2] : NULL_MOVE;
  if (grandParent)
    history +=
        data->fh[PIECE_TYPE[MovePiece(grandParent)]][MoveEnd(grandParent)][PIECE_TYPE[MovePiece(move)]][MoveEnd(move)];

  return history;
}
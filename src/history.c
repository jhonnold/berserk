// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2022 Jay Honnold

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

void AddKillerMove(SearchStack* ss, Move move) {
  if (ss->killers[0] != move) {
    ss->killers[1] = ss->killers[0];
    ss->killers[0] = move;
  }
}

void AddCounterMove(SearchData* data, Move move, Move parent) {
  data->counters[FromTo(parent)] = move;
}

void AddHistoryHeuristic(int* entry, int inc) {
  *entry += inc - *entry * abs(inc) / 65536;
}

void UpdateHistories(Board* board,
                     SearchStack* ss,
                     SearchData* data,
                     Move bestMove,
                     int depth,
                     int stm,
                     Move quiets[],
                     int nQ,
                     Move tacticals[],
                     int nT,
                     BitBoard threats) {
  int inc = min(7584, 16 * depth * depth + 480 * depth - 480);

  Move parent      = (ss - 1)->move;
  Move grandParent = (ss - 2)->move;

  if (!IsTactical(bestMove)) {
    AddKillerMove(ss, bestMove);
    AddHistoryHeuristic(&HH(stm, bestMove, threats), inc);

    if (parent) {
      AddCounterMove(data, bestMove, parent);
      AddHistoryHeuristic(&CH(parent, bestMove), inc);
    }

    if (grandParent) AddHistoryHeuristic(&CH(grandParent, bestMove), inc);
  } else {
    int piece    = PieceType(Moving(bestMove));
    int to       = To(bestMove);
    int captured = IsEP(bestMove) ? PAWN : PieceType(board->squares[to]);

    AddHistoryHeuristic(&TH(piece, to, captured), inc);
  }

  // Update quiets
  if (!IsTactical(bestMove)) {
    for (int i = 0; i < nQ; i++) {
      Move m = quiets[i];
      if (m != bestMove) {
        AddHistoryHeuristic(&HH(stm, m, threats), -inc);
        if (parent) AddHistoryHeuristic(&CH(parent, m), -inc);
        if (grandParent) AddHistoryHeuristic(&CH(grandParent, m), -inc);
      }
    }
  }

  // Update tacticals
  for (int i = 0; i < nT; i++) {
    Move m = tacticals[i];

    if (m != bestMove) {
      int piece    = PieceType(Moving(m));
      int to       = To(m);
      int captured = IsEP(m) ? PAWN : PieceType(board->squares[to]);

      AddHistoryHeuristic(&TH(piece, to, captured), -inc);
    }
  }
}

int GetQuietHistory(SearchStack* ss, SearchData* data, Move move, int stm, BitBoard threats) {
  if (IsTactical(move)) return 0;

  int history = HH(stm, move, threats);

  Move parent = (ss - 1)->move;
  if (parent) history += CH(parent, move);

  Move grandParent = (ss - 2)->move;
  if (grandParent) history += CH(grandParent, move);

  return history;
}

int GetTacticalHistory(SearchData* data, Board* board, Move m) {
  int piece    = PieceType(Moving(m));
  int to       = To(m);
  int captured = IsEP(m) ? PAWN : PieceType(board->squares[to]);

  return TH(piece, to, captured);
}
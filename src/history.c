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

void AddKillerMove(SearchData* data, Move move) {
  if (data->killers[data->ply][0] != move) data->killers[data->ply][1] = data->killers[data->ply][0];

  data->killers[data->ply][0] = move;
}

void AddCounterMove(SearchData* data, Move move, Move parent) { data->counters[FromTo(parent)] = move; }

void AddHistoryHeuristic(int* entry, int inc) { *entry += 64 * inc - *entry * abs(inc) / 1024; }

void UpdateHistories(Board* board, SearchData* data, Move bestMove, int depth, int stm, Move quiets[], int nQ,
                     Move tacticals[], int nT, BitBoard threats) {
  int inc = min(depth * depth, 576);

  Move parent = data->moves[data->ply - 1];
  Move grandParent = data->moves[data->ply - 2];

  if (!IsTactical(bestMove)) {
    AddKillerMove(data, bestMove);
    AddHistoryHeuristic(&HH(stm, bestMove, threats), inc);

    if (parent) {
      AddCounterMove(data, bestMove, parent);
      AddHistoryHeuristic(&CH(parent, bestMove), inc);
    }

    if (grandParent) AddHistoryHeuristic(&CH(grandParent, bestMove), inc);
  } else {
    int piece = PieceType(Moving(bestMove));
    int to = To(bestMove);
    int captured = (IsEP(bestMove) || Promo(bestMove)) ? PAWN : PieceType(board->squares[to]);

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
      int piece = PieceType(Moving(m));
      int to = To(m);
      int captured = (IsEP(m) || Promo(m)) ? PAWN : PieceType(board->squares[to]);

      AddHistoryHeuristic(&TH(piece, to, captured), -inc);
    }
  }
}

int GetQuietHistory(SearchData* data, Move move, int stm, BitBoard threats) {
  if (IsTactical(move)) return 0;

  int history = HH(stm, move, threats);

  Move parent = data->moves[data->ply - 1];
  if (parent) history += CH(parent, move);

  Move grandParent = data->moves[data->ply - 2];
  if (grandParent) history += CH(grandParent, move);

  return history;
}

int GetCounterHistory(SearchData* data, Move move) {
  if (IsTactical(move)) return 0;

  Move parent = data->moves[data->ply - 1];
  return CH(parent, move);
}

int GetTacticalHistory(SearchData* data, Board* board, Move m) {
  int piece = PieceType(Moving(m));
  int to = To(m);
  int captured = (IsEP(m) || Promo(m)) ? PAWN : PieceType(board->squares[to]);

  return TH(piece, to, captured);
}
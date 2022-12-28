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

void AddCounterMove(Board* board, ThreadData* thread, Move move, Move parent) {
  int to     = To(parent);
  int moving = Promo(parent) ? Piece(PAWN, board->xstm) : board->squares[to];

  thread->counters[moving][to] = move;
}

void AddHistoryHeuristic(int* entry, int inc) {
  *entry += inc - *entry * abs(inc) / 65536;
}

void UpdateCH(Board* board, SearchStack* ss, Move move, int bonus) {
  for (int i = 1; i < 3; i++) {
    if ((ss - i)->move) AddHistoryHeuristic(&(*(ss - i)->ch)[board->squares[From(move)]][To(move)], bonus);
  }
}

void UpdateHistories(Board* board,
                     SearchStack* ss,
                     ThreadData* thread,
                     Move bestMove,
                     int depth,
                     int stm,
                     Move quiets[],
                     int nQ,
                     Move tacticals[],
                     int nT,
                     BitBoard threats) {
  int inc = min(7584, 16 * depth * depth + 480 * depth - 480);

  if (!IsTactical(bestMove)) {
    AddKillerMove(ss, bestMove);
    AddHistoryHeuristic(&HH(stm, bestMove, threats), inc);
    UpdateCH(board, ss, bestMove, inc);

    if ((ss - 1)->move) AddCounterMove(board, thread, bestMove, (ss - 1)->move);

  } else {
    int piece    = PieceType(board->squares[From(bestMove)]);
    int to       = To(bestMove);
    int captured = IsEP(bestMove) ? PAWN : PieceType(board->squares[to]);

    AddHistoryHeuristic(&TH(piece, to, captured), inc);
  }

  // Update quiets
  if (!IsTactical(bestMove)) {
    for (int i = 0; i < nQ; i++) {
      Move m = quiets[i];
      if (m == bestMove) continue;

      AddHistoryHeuristic(&HH(stm, m, threats), -inc);
      UpdateCH(board, ss, m, -inc);
    }
  }

  // Update tacticals
  for (int i = 0; i < nT; i++) {
    Move m = tacticals[i];
    if (m == bestMove) continue;

    int piece    = PieceType(board->squares[From(m)]);
    int to       = To(m);
    int captured = IsEP(m) ? PAWN : PieceType(board->squares[to]);

    AddHistoryHeuristic(&TH(piece, to, captured), -inc);
  }
}

int GetQuietHistory(Board* board, SearchStack* ss, ThreadData* thread, Move move, int stm, BitBoard threats) {
  if (IsTactical(move)) return 0;

  int history = HH(stm, move, threats);
  history += (*(ss - 1)->ch)[board->squares[From(move)]][To(move)];
  history += (*(ss - 2)->ch)[board->squares[From(move)]][To(move)];

  return history;
}

int GetTacticalHistory(ThreadData* thread, Board* board, Move m) {
  int piece    = PieceType(board->squares[From(m)]);
  int to       = To(m);
  int captured = IsEP(m) ? PAWN : PieceType(board->squares[to]);

  return TH(piece, to, captured);
}
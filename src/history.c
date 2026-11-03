// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2026 Jay Honnold

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
#include <string.h>

#include "board.h"
#include "move.h"
#include "util.h"

CorrectionHistory Correction;

void UpdateHistories(SearchStack* ss,
                     ThreadData* thread,
                     Move bestMove,
                     Depth depth,
                     Move quiets[],
                     int nQ,
                     Move captures[],
                     int nC) {
  Board* board = &thread->board;
  Color stm    = board->stm;

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
    if (nQ > 1 || depth > HIST_UPDATE_DEPTH) {
      AddHistoryHeuristic(&HH(thread, stm, bestMove, board->threatened), inc);
      UpdateCH(ss, bestMove, inc);
    }
  } else {
    Piece piece        = Moving(bestMove);
    Square to          = To(bestMove);
    int defended       = !GetBit(board->threatened, to);
    PieceType captured = IsEP(bestMove) ? PAWN : TypeOf(board->squares[to]);

    AddHistoryHeuristic(&TH(thread, piece, to, defended, captured), inc);
  }

  // Update quiets
  if (!IsCap(bestMove)) {
    const int malus = Min(0, -inc + HIST_MALUS_SCALE * (nQ - 1));

    for (int i = 0; i < nQ; i++) {
      Move m = quiets[i];
      if (m == bestMove)
        continue;

      AddHistoryHeuristic(&HH(thread, stm, m, board->threatened), malus);
      UpdateCH(ss, m, malus);
    }
  }

  // Update captures
  for (int i = 0; i < nC; i++) {
    Move m = captures[i];
    if (m == bestMove)
      continue;

    Piece piece        = Moving(m);
    Square to          = To(m);
    int defended       = !GetBit(board->threatened, to);
    PieceType captured = IsEP(m) ? PAWN : TypeOf(board->squares[to]);

    AddHistoryHeuristic(&TH(thread, piece, to, defended, captured), -inc);
  }
}

void UpdateCorrection(Score raw, Score real, Depth depth, Board* board) {
  const int16_t correction = Min(CORR_MAX, Max(-CORR_MAX, CORR_UPDATE * (real - raw) * depth));
  int16_t* pawnCorrection  = &Correction.pawn[board->pawnZobrist & CORRECTION_MASK];
  AddHistoryHeuristic(pawnCorrection, correction);

  int16_t* whiteCorrection = &Correction.nonPawn[WHITE][board->nonPawnZobrist[WHITE] & CORRECTION_MASK];
  AddHistoryHeuristic(whiteCorrection, correction);
  int16_t* blackCorrection = &Correction.nonPawn[BLACK][board->nonPawnZobrist[BLACK] & CORRECTION_MASK];
  AddHistoryHeuristic(blackCorrection, correction);
}

void UpdateContCorrection(Score raw, Score real, Depth depth, SearchStack* ss) {
  if (!(ss - 1)->move)
    return;

  const int16_t correction = Min(CONT_CORR_MAX, Max(-CONT_CORR_MAX, CONT_CORR_UPDATE * (real - raw) * depth));
  const Piece moving       = Moving((ss - 1)->move);
  const Square to          = To((ss - 1)->move);

  if ((ss - 2)->move)
    AddHistoryHeuristic(&(*(ss - 2)->cont)[moving][to], correction);

  if ((ss - 3)->move)
    AddHistoryHeuristic(&(*(ss - 3)->cont)[moving][to], correction);
}

// Zero this thread's slice of the shared tables
void CorrectionClearPart(int idx, int count) {
  const uint64_t begin = idx * CORRECTION_SIZE / count;
  const uint64_t end   = (idx + 1) * CORRECTION_SIZE / count;
  const uint64_t bytes = (end - begin) * sizeof(int16_t);

  memset(Correction.pawn + begin, 0, bytes);
  memset(Correction.nonPawn[WHITE] + begin, 0, bytes);
  memset(Correction.nonPawn[BLACK] + begin, 0, bytes);
}

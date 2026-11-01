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
#include <string.h>

#include "board.h"
#include "move.h"
#include "util.h"

CorrectionHistory Correction;

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
    if (nQ > 1 || depth > 5) {
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
    const int malus = Min(0, -inc + 36 * (nQ - 1));

    for (int i = 0; i < nQ; i++) {
      Move m = quiets[i];
      if (m == bestMove)
        continue;

      AddHistoryHeuristic(&HH(stm, m, board->threatened), malus);
      UpdateCH(ss, m, malus);
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

void UpdateCorrection(int raw, int real, int depth, Board* board) {
  const int16_t correction = Min(4096, Max(-4096, 3 * (real - raw) * depth));
  int16_t* pawnCorrection  = &Correction.pawn[board->pawnZobrist & Correction.mask];
  AddHistoryHeuristic(pawnCorrection, correction);

  int16_t* whiteCorrection =
    &Correction.nonPawn[WHITE][board->nonPawnZobrist[WHITE] & Correction.mask];
  AddHistoryHeuristic(whiteCorrection, correction);
  int16_t* blackCorrection =
    &Correction.nonPawn[BLACK][board->nonPawnZobrist[BLACK] & Correction.mask];
  AddHistoryHeuristic(blackCorrection, correction);
}

void UpdateContCorrection(int raw, int real, int depth, SearchStack* ss) {
  if ((ss - 1)->move && (ss - 2)->move) {
    const int16_t correction = Min(4096, Max(-4096, 4 * (real - raw) * depth));
    int16_t* contCorrection  = &(*(ss - 2)->cont)[Moving((ss - 1)->move)][To((ss - 1)->move)];
    AddHistoryHeuristic(contCorrection, correction);
  }

  if ((ss - 1)->move && (ss - 3)->move) {
    const int16_t correction = Min(4096, Max(-4096, 4 * (real - raw) * depth));
    int16_t* contCorrection  = &(*(ss - 3)->cont)[Moving((ss - 1)->move)][To((ss - 1)->move)];
    AddHistoryHeuristic(contCorrection, correction);
  }
}

void CorrectionFree() {
  if (Correction.pawn) {
    AlignedFree(Correction.pawn);
    AlignedFree(Correction.nonPawn[WHITE]);
    AlignedFree(Correction.nonPawn[BLACK]);
  }

  Correction.pawn = Correction.nonPawn[WHITE] = Correction.nonPawn[BLACK] = NULL;
  Correction.size = Correction.mask = 0;
}

// Size the shared tables to CORRECTION_SIZE entries per thread (rounded to a power of 2)
void CorrectionResize(int threads) {
  uint64_t size = CORRECTION_SIZE;
  while (size < (uint64_t) CORRECTION_SIZE * threads)
    size <<= 1;

  if (size == Correction.size)
    return;

  CorrectionFree();

  Correction.size           = size;
  Correction.mask           = size - 1;
  Correction.pawn           = AlignedMalloc(size * sizeof(int16_t), 64);
  Correction.nonPawn[WHITE] = AlignedMalloc(size * sizeof(int16_t), 64);
  Correction.nonPawn[BLACK] = AlignedMalloc(size * sizeof(int16_t), 64);

  CorrectionClearPart(0, 1);
}

// Zero this thread's slice of the shared tables
void CorrectionClearPart(int idx, int count) {
  const uint64_t begin = idx * Correction.size / count;
  const uint64_t end   = (idx + 1) * Correction.size / count;
  const uint64_t bytes = (end - begin) * sizeof(int16_t);

  memset(Correction.pawn + begin, 0, bytes);
  memset(Correction.nonPawn[WHITE] + begin, 0, bytes);
  memset(Correction.nonPawn[BLACK] + begin, 0, bytes);
}

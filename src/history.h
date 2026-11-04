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

#ifndef HISTORY_H
#define HISTORY_H

#include "bits.h"
#include "board.h"
#include "move.h"
#include "search.h"
#include "thread.h"
#include "tune.h"
#include "types.h"
#include "util.h"

// correction history entries, shared across all threads
#define CORRECTION_SIZE 131072

// pawn and non-pawn correction histories, shared by all threads
typedef struct {
  int16_t pawn[CORRECTION_SIZE] ALIGN;
  int16_t nonPawn[2][CORRECTION_SIZE] ALIGN;
} CorrectionHistory;

#define CORRECTION_MASK (CORRECTION_SIZE - 1)

#define HH(t, stm, m, threats) ((t)->hh[stm][!GetBit(threats, From(m))][!GetBit(threats, To(m))][FromTo(m)])
#define TH(t, p, e, d, c)      ((t)->caph[p][e][d][c])

INLINE int GetQuietHistory(SearchStack* ss, ThreadData* thread, Move move) {
  return (int) HH(thread, thread->board.stm, move, thread->board.threatened) + //
         (int) (*(ss - 1)->ch)[Moving(move)][To(move)] +                       //
         (int) (*(ss - 2)->ch)[Moving(move)][To(move)] +                       //
         (int) (*(ss - 4)->ch)[Moving(move)][To(move)];
}

INLINE int GetCaptureHistory(ThreadData* thread, Move move) {
  Board* board = &thread->board;

  return TH(thread,
            Moving(move),
            To(move),
            !GetBit(board->threatened, To(move)),
            IsEP(move) ? PAWN : TypeOf(board->squares[To(move)]));
}

INLINE int GetHistory(SearchStack* ss, ThreadData* thread, Move move) {
  return IsCap(move) ? GetCaptureHistory(thread, move) : GetQuietHistory(ss, thread, move);
}

INLINE void AddKillerMove(SearchStack* ss, Move move) {
  if (ss->killers[0] != move) {
    ss->killers[1] = ss->killers[0];
    ss->killers[0] = move;
  }
}

INLINE void AddCounterMove(ThreadData* thread, Move move, Move parent) {
  thread->counters[Moving(parent)][To(parent)] = move;
}

INLINE int16_t HistoryBonus(Depth depth) {
  return Min(HIST_BONUS_MAX, HIST_BONUS_QUAD * depth * depth + HIST_BONUS_LINEAR * depth + HIST_BONUS_BASE);
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
  if ((ss - 6)->move)
    AddHistoryHeuristic(&(*(ss - 6)->ch)[Moving(move)][To(move)], bonus);
}

extern CorrectionHistory Correction;

INLINE Score GetCorrectionScore(Board* board, SearchStack* ss) {
  const int pawn  = Correction.pawn[board->pawnZobrist & CORRECTION_MASK];
  const int white = Correction.nonPawn[WHITE][board->nonPawnZobrist[WHITE] & CORRECTION_MASK];
  const int black = Correction.nonPawn[BLACK][board->nonPawnZobrist[BLACK] & CORRECTION_MASK];
  const int cont1 = (*(ss - 3)->cont)[Moving((ss - 1)->move)][To((ss - 1)->move)];
  const int cont2 = (*(ss - 2)->cont)[Moving((ss - 1)->move)][To((ss - 1)->move)];

  return (CORR_PAWN * pawn + CORR_NON_PAWN * (white + black) + CORR_CONT1 * cont1 + CORR_CONT2 * cont2) / 8192;
}

void UpdateHistories(SearchStack* ss,
                     ThreadData* thread,
                     Move bestMove,
                     Depth depth,
                     Move quiets[],
                     int nQ,
                     Move captures[],
                     int nC);

void UpdateCorrection(Score raw, Score real, Depth depth, Board* board);
void UpdateContCorrection(Score raw, Score real, Depth depth, SearchStack* ss);
void CorrectionClearPart(int idx, int count);

#endif

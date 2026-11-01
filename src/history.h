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

#ifndef HISTORY_H
#define HISTORY_H

#include "bits.h"
#include "board.h"
#include "move.h"
#include "types.h"
#include "util.h"

#define HH(stm, m, threats) (thread->hh[stm][!GetBit(threats, From(m))][!GetBit(threats, To(m))][FromTo(m)])
#define TH(p, e, d, c)      (thread->caph[p][e][d][c])

INLINE int GetQuietHistory(SearchStack* ss, ThreadData* thread, Move move) {
  return (int) HH(thread->board.stm, move, thread->board.threatened) + //
         (int) (*(ss - 1)->ch)[Moving(move)][To(move)] +               //
         (int) (*(ss - 2)->ch)[Moving(move)][To(move)] +               //
         (int) (*(ss - 4)->ch)[Moving(move)][To(move)];
}

INLINE int GetCaptureHistory(ThreadData* thread, Move move) {
  Board* board = &thread->board;

  return TH(Moving(move),
            To(move),
            !GetBit(board->threatened, To(move)),
            IsEP(move) ? PAWN : PieceType(board->squares[To(move)]));
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

INLINE int16_t HistoryBonus(int depth) {
  return Min(1708, 4 * depth * depth + 191 * depth - 118);
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

INLINE int GetCorrectionScore(Board* board, SearchStack* ss) {
  const int pawn  = Correction.pawn[board->pawnZobrist & Correction.mask];
  const int white = Correction.nonPawn[WHITE][board->nonPawnZobrist[WHITE] & Correction.mask];
  const int black = Correction.nonPawn[BLACK][board->nonPawnZobrist[BLACK] & Correction.mask];
  const int cont1 = (*(ss - 3)->cont)[Moving((ss - 1)->move)][To((ss - 1)->move)];
  const int cont2 = (*(ss - 2)->cont)[Moving((ss - 1)->move)][To((ss - 1)->move)];

  return (31 * pawn + 26 * (white + black) + 17 * cont1 + 46 * cont2) / 8192;
}

void UpdateHistories(SearchStack* ss,
                     ThreadData* thread,
                     Move bestMove,
                     int depth,
                     Move quiets[],
                     int nQ,
                     Move captures[],
                     int nC);

void UpdateCorrection(int raw, int real, int depth, Board* board);
void UpdateContCorrection(int raw, int real, int depth, SearchStack* ss);
void CorrectionResize(int threads);
void CorrectionClearPart(int idx, int count);
void CorrectionFree();

#endif

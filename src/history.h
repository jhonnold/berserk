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

void UpdateHistories(SearchStack* ss,
                     ThreadData* thread,
                     Move bestMove,
                     int depth,
                     Move quiets[],
                     int nQ,
                     Move captures[],
                     int nC);

#endif
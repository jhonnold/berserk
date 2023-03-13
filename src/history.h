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

#define HH(stm, m, thr, cov) \
  (thread->hh[stm][!GetBit(thr, From(m))][!GetBit(thr, To(m))][!GetBit(cov, To(m))][FromTo(m)])
#define TH(p, e, c) (thread->caph[p][e][c])

INLINE int GetQuietHistory(SearchStack* ss,
                           ThreadData* thread,
                           Move move,
                           int stm,
                           BitBoard threats,
                           BitBoard covered) {
  return (int) HH(stm, move, threats, covered) +         //
         (int) (*(ss - 1)->ch)[Moving(move)][To(move)] + //
         (int) (*(ss - 2)->ch)[Moving(move)][To(move)] + //
         (int) (*(ss - 4)->ch)[Moving(move)][To(move)];
}

INLINE int GetCaptureHistory(ThreadData* thread, Board* board, Move move) {
  return TH(Moving(move), To(move), IsEP(move) ? PAWN : PieceType(board->squares[To(move)]));
}

void UpdateHistories(Board* board,
                     SearchStack* ss,
                     ThreadData* thread,
                     Move bestMove,
                     int depth,
                     int stm,
                     Move quiets[],
                     int nQ,
                     Move captures[],
                     int nC,
                     BitBoard threats,
                     BitBoard covered);

#endif
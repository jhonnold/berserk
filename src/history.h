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

#ifndef HISTORY_H
#define HISTORY_H

#include "bits.h"
#include "types.h"

#define HH(stm, m, threats) (thread->hh[stm][!getBit(threats, From(m))][!getBit(threats, To(m))][FromTo(m)])
#define TH(p, e, c)         (thread->th[p][e][c])

void AddKillerMove(SearchStack* ss, Move move);
void AddCounterMove(ThreadData* thread, Move move, Move parent);
void AddHistoryHeuristic(int16_t* entry, int16_t inc);
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
                     BitBoard threats);
int16_t GetQuietHistory(SearchStack* ss, ThreadData* thread, Move move, int stm, BitBoard threats);
int16_t GetTacticalHistory(ThreadData* thread, Board* board, Move move);

#endif
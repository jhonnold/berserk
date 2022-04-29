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

#define HH(stm, m, threats) (data->hh[stm][threats ? lsb(threats) : 64][FromTo(m)])
#define CH(p, m) (data->ch[PieceType(Moving(p))][To(p)][PieceType(Moving(m))][To(m)])
#define FH(g, m) (data->fh[PieceType(Moving(g))][To(g)][PieceType(Moving(m))][To(m)])
#define TH(p, e, c) (data->th[p][e][c])

void AddKillerMove(SearchData* data, Move move);
void AddCounterMove(SearchData* data, Move move, Move parent);
void AddHistoryHeuristic(int* entry, int inc);
void UpdateHistories(Board* board, SearchData* data, Move bestMove, int depth, int stm, Move quiets[], int nQ,
                     Move tacticals[], int nT, BitBoard threats);
int GetQuietHistory(SearchData* data, Move move, int stm, BitBoard threats);
int GetCounterHistory(SearchData* data, Move move);
int GetTacticalHistory(SearchData* data, Board* board, Move move);
#include "history.c"
#endif

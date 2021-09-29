// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

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

#include "types.h"

void AddKillerMove(SearchData* data, Move move);
void AddCounterMove(SearchData* data, Move move, Move parent);
void AddHistoryHeuristic(int* entry, int inc);
void UpdateHistories(Board* board, SearchData* data, Move bestMove, int depth, int stm, Move quiets[], int nQ,
                     Move tacticals[], int nT);
int GetQuietHistory(SearchData* data, Move move, int stm);
int GetCounterHistory(SearchData* data, Move move);
int GetTacticalHistory(SearchData* data, Board* board, Move move);

#endif
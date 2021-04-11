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

#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

extern const int CHECKMATE;
extern const int MATE_BOUND;

void InitPruningAndReductionTables();
void ClearSearchData(SearchData* data);

void* Search(void* arg);
void* IterativeDeepening(void* arg);
int Negamax(int alpha, int beta, int depth, ThreadData* thread, PV* pv);
int Quiesce(int alpha, int beta, ThreadData* thread, PV* pv);

void PrintInfo(PV* pv, int score, int depth, ThreadData* thread);
void PrintPV(PV* pv);

#endif

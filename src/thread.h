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

#ifndef THREAD_H
#define THREAD_H

#include "types.h"

ThreadData* CreatePool(int count);
void InitPool(Board* board, SearchParams* params, ThreadData* threads, SearchResults* results);
void ResetThreadPool(ThreadData* threads);
uint64_t NodesSearched(ThreadData* threads);
uint64_t TBHits(ThreadData* threads);
int Seldepth(ThreadData* threads);

#endif
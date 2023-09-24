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

#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

// search specific score evals
#define UNKNOWN \
  32257 // this must be higher than CHECKMATE (some conditional logic relies on
        // this)
#define CHECKMATE    32200
#define MATE_BOUND   (32200 - MAX_SEARCH_PLY)
#define TB_WIN_SCORE MATE_BOUND
#define TB_WIN_BOUND (TB_WIN_SCORE - MAX_SEARCH_PLY)

// static evaluation pruning
#define SEE_PRUNE_CAPTURE_CUTOFF 104
#define SEE_PRUNE_CUTOFF         17

// delta pruning in QS
#define DELTA_CUTOFF 55

// base window value
#define WINDOW 10

void InitPruningAndReductionTables();

void StartSearch(Board* board, uint8_t ponder);
void MainSearch();
void Search(ThreadData* thread);
int Negamax(int alpha, int beta, int depth, int isPV, int cutnode, ThreadData* thread, PV* pv, SearchStack* ss);
int Quiesce(int alpha, int beta, int depth, int isPV, ThreadData* thread, SearchStack* ss);

void PrintUCI(ThreadData* thread, int alpha, int beta, Board* board);
void PrintPV(PV* pv, Board* board);

void SortRootMoves(ThreadData* thread, int offset);
int ValidRootMove(ThreadData* thread, Move move);

void SearchClearThread(ThreadData* thread);
void SearchClear();

#endif

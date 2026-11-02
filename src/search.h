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

#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

typedef int16_t PieceTo[12][64];

typedef struct {
  int ply, staticEval, de;
  int reduction;
  PieceTo* ch;
  PieceTo* cont;
  Move move, skip;
  Move killers[2];
} SearchStack;

typedef struct {
  long start;
  int alloc;
  int max;

  uint64_t nodes;
  uint64_t softNodes;
  int hitrate;

  int timeset;
  int depth;
  int mate;
  int multiPV;
  int infinite;
  int searchMoves;
  SimpleMoveList searchable;
} SearchParams;

// search specific score evals
#define UNKNOWN \
  32257 // this must be higher than CHECKMATE (some conditional logic relies on
        // this)
#define CHECKMATE    32200
#define MATE_BOUND   (32200 - MAX_SEARCH_PLY)
#define TB_WIN_SCORE MATE_BOUND
#define TB_WIN_BOUND (TB_WIN_SCORE - MAX_SEARCH_PLY)

extern SearchParams Limits;

void InitPruningAndReductionTables();

void StartSearch(Board* board, uint8_t ponder);
void MainSearch();
void Search(ThreadData* thread);



void SearchClearThread(ThreadData* thread);
void SearchClear();

#endif

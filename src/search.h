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

// search specific score evals
#define UNKNOWN \
  32257 // this must be higher than CHECKMATE (some conditional logic relies on
        // this)
#define CHECKMATE    32200
#define MATE_BOUND   (32200 - MAX_SEARCH_PLY)
#define TB_WIN_SCORE MATE_BOUND
#define TB_WIN_BOUND (TB_WIN_SCORE - MAX_SEARCH_PLY)

void InitPruningAndReductionTables();

void StartSearch(Board* board, uint8_t ponder);
void MainSearch();
void Search(ThreadData* thread);
int Negamax(int alpha, int beta, int depth, int cutnode, ThreadData* thread, PV* pv, SearchStack* ss);
int Quiesce(int alpha, int beta, int depth, ThreadData* thread, SearchStack* ss);

void PrintUCI(ThreadData* thread, int alpha, int beta, Board* board);
void PrintPV(PV* pv, Board* board);

void SortRootMoves(ThreadData* thread, int offset);
int ValidRootMove(ThreadData* thread, Move move);

void SearchClearThread(ThreadData* thread);
void SearchClear();

extern float a0;
extern float a1;
extern float a2;
extern float a3;
extern float a4;
extern float a5;
extern float a6;
extern float a7;

// ID
extern int b0;
extern int b1;

// Node pruning
// Prior reduction corrections
extern int c0;
extern int c1;
extern int c2;

// RFP
extern int d0;
extern int d1;
extern int d2;
extern int d3;
extern int d4;

// Razoring
extern int e0;
extern int e1;

// NMP
extern int f0;
extern int f1;
extern int f2;
extern int f3;
extern int f4;

// Probcut
extern int g0;
extern int g1;

// Move pruning
extern int h0;
extern int h1;
extern int h2;
extern int h3;
extern int h4;
extern int h5;

// Extensions
extern int i0;
extern int i1;
extern int i2;
extern int i3;
extern int i4;
extern int i5;

extern int j3;
extern int j4;
extern int j2;

// QSearch
extern int k0;

#endif

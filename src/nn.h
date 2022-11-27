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

#include "board.h"
#include "types.h"
#include "util.h"

int Predict(Board* board);
int OutputLayer(Accumulator stm, Accumulator xstm);
void ResetRefreshTable(AccumulatorKingState* refreshTable[2]);
void RefreshAccumulator(Accumulator accumulator, Board* board, const int perspective);
void ResetAccumulator(Accumulator output, Board* board, const int perspective);

void ApplyUpdates(Board* board, Move move, int captured, const int view);

void LoadDefaultNN();
int LoadNetwork(char* path);
